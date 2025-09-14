#include "uds.h"
#include "adc.h"
#include <string.h>
#include "nvm.h"

/** Current security level (used for access checks in UDS services) */
uint8_t currentSecurityLevel = SECURITY_LEVEL_ENGINE;

/** Threshold engine temperature (example DID value) */
uint16_t engineTemp           = 0x1234;

extern volatile uint16_t temperature;
extern volatile uint16_t light_level;

extern uint16_t temp_threshold_low;
extern uint16_t temp_threshold_medium;
extern uint16_t temp_threshold_high;

/**
 * @brief UDS response flow state
 *
 * - UDS_FLOW_NONE: No response will be sent.
 * - UDS_FLOW_POS : Positive Response (0x40 | SID).
 * - UDS_FLOW_NEG : Negative Response (0x7F, SID, NRC).
 */
typedef enum {
    UDS_FLOW_NONE = 0,
    UDS_FLOW_POS,
    UDS_FLOW_NEG
} UDS_FlowType;

/**
 * @brief Holds the current UDS request/response state
 *
 * Members:
 * - flow       : POS / NEG / NONE.
 * - sid        : Service ID of the request.
 * - nrc        : Negative Response Code (if flow = NEG).
 * - payload    : Pointer to data of POS response.
 * - payload_len: Length of payload.
 */
typedef struct {
    UDS_FlowType   flow;
    uint8_t        sid;
    uint8_t        nrc;
    const uint8_t* payload;
    uint16_t       payload_len;
} UDS_Context;

/** Global context used across service handlers */
static UDS_Context udsCtx;

/**
 * @brief Simple busy-wait delay (optional)
 *
 * @param ms Milliseconds to wait
 */
static void delay_ms(volatile uint32_t ms) {
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 5000; j++) { /* Adjust factor to match clock speed */
            __asm__("nop");
        }
    }
}

/**
 * @brief Transmit a multi-frame UDS payload using a simplified ISO-TP flow.
 *
 * Builds and transmits First Frame (FF) followed by Consecutive Frames (CF)
 * with a rolling 4-bit sequence number, assuming a permissive Flow Control.
 *
 * @param data   Pointer to full UDS payload (RspSID + payload bytes).
 * @param length Total payload length in bytes.
 *
 * Processing logic:
 * 1) Send FF (8-byte CAN frame): PCI=FirstFrame + length(12-bit), first 6 data bytes.
 * 2) Wait a small fixed delay (assumes tester sent FC/CTS; no parsing here).
 * 3) Stream CFs, 7 data bytes per frame; pad tail bytes with 0xAA if short.
 * 4) Sequence number wraps 0..15 per ISO-TP.
 *
 * Effects/Assumptions:
 * - Uses TX_MSG_ID_UDS and FLEXCAN0_transmit_msg() to send frames.
 * - No FC parsing, block-size, or true STmin handling (simplified model).
 */
static void UDS_SendMultiFrameISO_TP(const uint8_t *data, uint16_t length) {
	CAN_Message_t msg;
	msg.canID = TX_MSG_ID_UDS;
	msg.dlc = 8; /* Both First Frames and Consecutive Frames always use 8-byte DLC. */

	uint16_t bytes_sent = 0;
	uint8_t sequence_number = 1;

	/* --- Step 1: Send the First Frame (FF) --- */
    /* The FF contains control information and the first 6 bytes of data. */
    /* PCI: Type + Upper 4 bits of length */
	msg.data[0] = ISO_TP_PCI_TYPE_FIRST_FRAME | (uint8_t) (length >> 8);
    /* Lower 8 bits of length */
	msg.data[1] = (uint8_t) (length & 0xFF);
	memcpy(&msg.data[2], &data[bytes_sent], 6);
	FLEXCAN0_transmit_msg(&msg);
	bytes_sent += 6;

	/* --- Step 2: Wait for Flow Control (FC) frame from the tester --- */
    /* A full implementation would parse the FC frame. This simplified version
       just waits a fixed amount of time, assuming a "ClearToSend" response. */
	delay_ms(10);

	/* --- Step 3: Send all Consecutive Frames (CF) --- */
    /* CFs contain the remaining data, 7 bytes at a time. */
	while (bytes_sent < length) {
        /* The CF PCI byte contains the type and a 4-bit rolling sequence number. */
		msg.data[0] = ISO_TP_PCI_TYPE_CONSECUTIVE_FRAME | sequence_number;

        /* Calculate how many bytes to copy into this frame. */
		uint16_t remaining_bytes = length - bytes_sent;
		uint8_t bytes_to_copy;
		if (remaining_bytes > 7) {
			bytes_to_copy = 7;
		} else {
			bytes_to_copy = (uint8_t) remaining_bytes;
		}

		memcpy(&msg.data[1], &data[bytes_sent], bytes_to_copy);

		/* If this is the last frame and it's not full, pad the remaining bytes. */
		if (bytes_to_copy < 7) {
			memset(&msg.data[1 + bytes_to_copy], 0xAA, 7 - bytes_to_copy);
		}

		FLEXCAN0_transmit_msg(&msg);

		bytes_sent += bytes_to_copy;
		sequence_number = (sequence_number + 1) % 16; /* Sequence number wraps around from 15 to 0. */
		delay_ms(5); /* Wait for STmin (Separation Time Minimum) before sending the next frame. */
	}
}

/**
 * @brief Clear DTC(s) from NVM based on the GroupOfDTC parameter.
 *
 * @param groupOfDTC 24-bit DTC group identifier (0xFFFFFF = all DTCs).
 * @return true if all targeted DTC(s) were successfully erased, false otherwise.
 */
static bool clearDTCFromNVM(uint32_t groupOfDTC) {
    uint8_t erased[DTC_SLOT_SIZE];
    memset(erased, 0xFF, DTC_SLOT_SIZE); // 0xFF = "empty" state

    // === Case 1: Clear ALL stored DTCs ===
    if (groupOfDTC == 0xFFFFFF) {
        bool cleared = true;
        for (uint8_t i = 0; i < DTC_GetCount(); i++) {
            uint32_t offset = DTC_REGION_OFFSET + (i * DTC_SLOT_SIZE);
            if (NVM_Erase(offset, DTC_SLOT_SIZE) != NVM_OK) {
                cleared = false; // Mark failure but continue erasing others
            }
        }
        return cleared;
    }

    // === Case 2: Clear a specific single DTC ===
    int8_t index = DTC_Find(groupOfDTC);
    if (index != -1) {
        uint32_t offset = DTC_REGION_OFFSET + (index * DTC_SLOT_SIZE);

        // Only erase this DTC slot without touching others
        if (NVM_Erase(offset, DTC_SLOT_SIZE) == NVM_OK) {
            return true; // Success
        } else {
            return false; // Failed to erase this slot
        }
    }

    // === Case 3: DTC not found ===
    // Per UDS ISO 14229, if the requested DTC is not present,
    // it is still considered a successful clear operation.
    return true;
}

/**
 * @brief Checks if a requested GroupOfDTC is supported by the ECU.
 */
static bool isGroupOfDTCSupported(uint32_t groupOfDTC) {
    // 0xFFFFFF = clear all DTCs
    if (groupOfDTC == 0xFFFFFF) return true;

    switch (groupOfDTC) {
        case DTC_ENGINE_OVERHEAT:
            return true;
        // TODO: Add more supported groups here
        default:
            return false;
    }
}

/**
 * @brief Verifies conditions before allowing DTC clearing.
 * @return true if clearing is allowed, false otherwise.
 */
static bool isConditionOkForClear(void) {
    // Example: Could check if ignition is ON, battery voltage OK, etc.
    return true;
}

/**
 * @brief Checks if ECU reset conditions are OK.
 *
 * Could check hardware/software state, ignition, etc.
 * In demo: always returns true.
 */
static bool isResetConditionOk(void) { return true; }

/**
 * @brief Checks if the given DID has security access granted.
 *
 * In real implementation, depends on currentSecurityLevel.
 * In demo: always returns true.
 */
static bool isSecurityAccessGranted(uint16_t did) {
    (void)did;
    return true;
}

/**
 * @brief Checks if the given DID can be accessed under current conditions.
 *
 * In real ECU, could depend on ignition state, operating mode, etc.
 * In demo: always returns true.
 */
static bool isConditionOk(uint16_t did) {
    (void)did;
    return true;
}

/**
 * @brief Handle 0x19 sub-function 0x01: reportNumberOfDTCByStatusMask.
 *
 * Expects format: [Len=3][SID=0x19][Sub=0x01][StatusMask].
 * Counts DTCs whose status mask matches requested mask (or 0xFF for all).
 *
 * @param requestMsg Received CAN frame: data[], dlc.
 *
 * Processing logic:
 * 1) Validate data[0] == 3; else NEG + NRC_INCORRECT_LENGTH.
 * 2) Validate dlc == 4; else NEG + NRC_INCORRECT_LENGTH.
 * 3) Read requested status mask at data[3].
 * 4) Iterate all DTC slots via DTC_GetCount()/DTC_GetRecord():
 *    - If (mask == 0xFF) or ((record.status_mask & mask) == mask), ++count.
 * 4) Build payload:
 *    [Sub=0x01][0xFF][DTC_FORMAT_ID_ISO14229_1][Count_H][Count_L].
 * 5) Set udsCtx POS with payload.
 *
 * Context:
 * - udsCtx.sid: original request SID to form response or NRC frame.
 * - udsCtx.payload / payload_len: POS response data (excludes RspSID).
 * - udsCtx.nrc: negative response code when needed.
 */
static void sf_reportNumberOfDTCByStatusMask(const CAN_Message_t *requestMsg) {
    /* Expected request format: [Length=3] [SID=19] [SubFunc=01] [StatusMask] */
    if (requestMsg->data[0] != 3) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }
    if (requestMsg->dlc < 4 || requestMsg->dlc != (requestMsg->data[0] + 1)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }
    static uint8_t payload[5];
    uint8_t requested_mask = requestMsg->data[3];
    uint16_t count = 0;

    for (uint8_t i = 0; i < DTC_GetCount(); ++i)
    {
        DTC_Record_t record;
        if (DTC_GetRecord(i, &record))
        {
            if ((requested_mask == 0xFF) || ((record.status_mask & requested_mask) == requested_mask))
            {
                count++;
            }
        }
    }

    payload[0] = SF_REPORT_NUMBER_OF_DTC_BY_STATUS_MASK;
    payload[1] = 0xFF;
    payload[2] = DTC_FORMAT_ID_ISO14229_1;
    payload[3] = (uint8_t)(count >> 8);
    payload[4] = (uint8_t)(count & 0xFF);

    udsCtx.flow = UDS_FLOW_POS;
    udsCtx.payload = payload;
    udsCtx.payload_len = sizeof(payload);
}

/**
 * @brief Handle 0x19 sub-function 0x02: reportDTCByStatusMask.
 *
 * Expects format: [Len=3][SID=0x19][Sub=0x02][StatusMask].
 * Returns a list of (DTC[3], Status) tuples filtered by status mask.
 *
 * @param requestMsg Received CAN frame: data[], dlc.
 *
 * Processing logic:
 * 1) Validate data[0] == 3; else NEG + NRC_INCORRECT_LENGTH.
 * 2) Validate dlc == 4; else NEG + NRC_INCORRECT_LENGTH.
 * 3) Append [0x02][0xFF] header to payload.
 * 4) For each valid DTC (per mask), append: DTC_H, DTC_M, DTC_L, status.
 * 5) Set udsCtx POS with assembled payload (variable length).
 */
static void sf_reportDTCByStatusMask(const CAN_Message_t *requestMsg) {
    /* Expected request format: [Length=3] [SID=19] [SubFunc=02] [StatusMask] */
    if (requestMsg->data[0] != 3) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    if (requestMsg->dlc < 4 || requestMsg->dlc != (requestMsg->data[0] + 1)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    /* Use a static payload buffer to avoid putting a large array on the stack. */
    static uint8_t payload[1 + (DTC_COUNT * 4)];
    uint16_t payload_len = 0;
    uint8_t requested_mask = requestMsg->data[3];
    payload[payload_len++] = SF_REPORT_DTC_BY_STATUS_MASK;
    payload[payload_len++] = 0xFF;

    for (uint8_t i = 0; i < DTC_GetCount(); ++i) {
        DTC_Record_t record;
        if (DTC_GetRecord(i, &record)) {
            if ((requested_mask == 0xFF) || ((record.status_mask & requested_mask) == requested_mask)) {
                /* For each matching DTC, append its 3-byte code and 1-byte status to the payload. */
                payload[payload_len++] = (uint8_t)(record.dtc_code >> 16);
                payload[payload_len++] = (uint8_t)(record.dtc_code >> 8);
                payload[payload_len++] = (uint8_t)(record.dtc_code);
                payload[payload_len++] = record.status_mask;
            }
        }
    }

    udsCtx.flow = UDS_FLOW_POS;
    udsCtx.payload = payload;
    udsCtx.payload_len = payload_len;
}

/**
 * @brief Handle 0x19 sub-function 0x04: reportDTCSnapshotRecordByDTCNumber.
 *
 * Expects: [Len=6][SID=0x19][Sub=0x04][DTC_H][DTC_M][DTC_L][RecNum].
 * Finds the DTC and returns snapshot (record number fixed to 0x01 here).
 *
 * @param requestMsg Received CAN frame: data[], dlc.
 *
 * Processing logic:
 * 1) Validate data[0] == 6; else NEG + NRC_INCORRECT_LENGTH.
 * 2) Validate dlc == 7; else NEG + NRC_INCORRECT_LENGTH.
 * 3) Validate requested_record_number == 0x01 || requested_record_number == 0xFF; else NEG + NRC_REQUEST_OUT_OF_RANGE.
 * 4) Rebuild requested DTC (3 bytes). Find index via DTC_Find().
 * 5) If found and DTC_GetRecord() OK:
 *    - Build payload:
 *      [0x04][DTC_H][DTC_M][DTC_L][status][recNo=0x01]
 *      [temperature][day][month][year_H][year_L].
 *    - Set udsCtx POS.
 * 6) Else NEG + NRC_REQUEST_OUT_OF_RANGE.
 */
static void sf_reportDTCSnapshotByDTCNumber(const CAN_Message_t *requestMsg) {
    /* Expected request format: [Length=6] [SID=19] [SubFunc=04] [DTC H] [DTC M] [DTC L] [RecNum] */
    if (requestMsg->data[0] != 6) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    if (requestMsg->dlc != 7 || requestMsg->dlc != requestMsg->data[0] + 1) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    uint8_t requested_record_number = requestMsg->data[6];

    if (requested_record_number != 0x01 && requested_record_number != 0xFF) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_REQUEST_OUT_OF_RANGE;
        return;
    }

    uint32_t requested_dtc = (requestMsg->data[3] << 16) | (requestMsg->data[4] << 8) | requestMsg->data[5];
    /* uint8_t record_number = requestMsg->data[6]; // We only support 1 record, so this is unused. */
    int8_t index = DTC_Find(requested_dtc);

    if (index != -1) {
        DTC_Record_t record;
        if (DTC_GetRecord(index, &record)) {
            /* The full response payload for this sub-function is 11 bytes. */
            static uint8_t payload[11];
            uint16_t payload_len = 0;

            /* Build the payload byte-by-byte to ensure correct structure. */
            payload[payload_len++] = SF_REPORT_DTC_SNAPSHOT_RECORD_BY_DTC_NUMBER;
            payload[payload_len++] = (uint8_t)(record.dtc_code >> 16);
            payload[payload_len++] = (uint8_t)(record.dtc_code >> 8);
            payload[payload_len++] = (uint8_t)(record.dtc_code);
            payload[payload_len++] = record.status_mask;
            payload[payload_len++] = 0x01; /* Snapshot Record Number is always 1 in this implementation. */
            payload[payload_len++] = record.snapshot.temperature;
            payload[payload_len++] = record.snapshot.day;
            payload[payload_len++] = record.snapshot.month;
            payload[payload_len++] = (uint8_t)(record.snapshot.year >> 8);   /* Year, High Byte (Big-Endian) */
            payload[payload_len++] = (uint8_t)(record.snapshot.year & 0xFF); /* Year, Low Byte */

            udsCtx.flow = UDS_FLOW_POS;
            udsCtx.payload = payload;
            udsCtx.payload_len = payload_len;
            return;
        }
    }
    udsCtx.flow = UDS_FLOW_NEG;
    udsCtx.nrc = NRC_REQUEST_OUT_OF_RANGE;
}

/**
 * @brief Handle 0x19 sub-function 0x0A: reportSupportedDTC.
 *
 * Expects: [Len=2][SID=0x19][Sub=0x0A].
 * Returns all supported DTCs with their current status byte.
 *
 * @param requestMsg Received CAN frame: data[], dlc.
 *
 * Processing logic:
 * 1) Validate data[0] == 2; else NEG + NRC_INCORRECT_LENGTH.
 * 2) Payload starts with [0x0A][0xFF].
 * 3) For each stored DTC (via DTC_GetRecord()), append DTC_H,M,L,status.
 * 4) Set udsCtx POS with payload.
 */
static void sf_reportSupportedDTC(const CAN_Message_t *requestMsg) {
    /* Expected request format: [Length=2] [SID=19] [SubFunc=0A] */
    if (requestMsg->data[0] != 2) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    static uint8_t payload[1 + (DTC_COUNT * 4)];
    uint16_t payload_len = 0;

    payload[payload_len++] = SF_REPORT_SUPPORTED_DTC;
    payload[payload_len++] = 0xFF;
    for (uint8_t i = 0; i < DTC_GetCount(); ++i) {
        DTC_Record_t record;
        if (DTC_GetRecord(i, &record)) {
            payload[payload_len++] = (uint8_t)(record.dtc_code >> 16);
            payload[payload_len++] = (uint8_t)(record.dtc_code >> 8);
            payload[payload_len++] = (uint8_t)(record.dtc_code);
            payload[payload_len++] = record.status_mask;
        }
    }
    udsCtx.flow = UDS_FLOW_POS;
    udsCtx.payload = payload;
    udsCtx.payload_len = payload_len;
}

// =====================================================
// ==== UDS Dispatcher ====
// =====================================================

/**
 * @brief Dispatches an incoming UDS request to the correct service handler.
 *
 * Extracts the Service ID (SID) from msg_rx.data[1], resets the UDS context,
 * routes to the corresponding service handler, and then transmits the response.
 *
 * @param msg_rx Received CAN message containing:
 *               - msg_rx.data[]: [len, SID, ...data...]
 *               - msg_rx.dlc:    number of bytes in this CAN frame.
 *
 * Processing logic:
 * 1) Extract SID = msg_rx.data[1].
 * 2) Reset udsCtx: flow=NONE, sid=SID, nrc=0, payload=NULL.
 * 3) Switch on SID:
 *    - 0x11 (ECU Reset)        -> handleECUReset()
 *    - 0x2E (Write DID)        -> handleWriteDataByIdentifier()
 *    - 0x22 (Read DID)         -> handleReadDataByIdentifier()
 *    - default                 -> flow=NEG, nrc=NRC_SERVICE_NOT_SUPPORTED
 * 4) Call UDS_SendResponse() to transmit reply.
 *
 * Context:
 * - Uses global udsCtx to store response state.
 * - Handlers set udsCtx.flow, payload, and payload_len appropriately.
 */
void UDS_DispatchService(const CAN_Message_t msg_rx) {
    uint8_t sid = msg_rx.data[1];

    // Reset context
    udsCtx.flow        = UDS_FLOW_NONE;
    udsCtx.sid         = sid;
    udsCtx.nrc         = 0;
    udsCtx.payload     = NULL;
    udsCtx.payload_len = 0;

	switch (sid) {
	case UDS_SERVICE_READ_DTC_INFORMATION:
		handleReadDTCInformation(&msg_rx);
		break;
	case UDS_SERVICE_CLEAR_DTC:
		handleClearDiagnosticInformation(&msg_rx);
		break;
	case UDS_SERVICE_ECU_RESET:
		handleECUReset(msg_rx);
		break;
	case UDS_SERVICE_WRITE_DID:
		handleWriteDataByIdentifier(msg_rx);
		break;
	case UDS_SERVICE_READ_DID:
		handleReadDataByIdentifier(msg_rx);
		break;
	default:
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_SERVICE_NOT_SUPPORTED;
		break;
	}

    UDS_SendResponse();
}

// =====================================================
// ==== UDS Response Sender ====
// =====================================================

/**
 * @brief Transmits a UDS response frame based on udsCtx state.
 *
 * Behavior:
 * - If udsCtx.flow == NEG:
 *    -> Build and send a Negative Response frame:
 *       [0x03, 0x7F, SID, NRC].
 * - If udsCtx.flow == POS:
 *    -> Build a Positive Response:
 *       - If total length <= 7: send as Single Frame.
 *       - If >7: delegate to ISO-TP multi-frame handler.
 * - If udsCtx.flow == NONE:
 *    -> Do nothing.
 *
 * Payload format:
 * - POS response = [length, rspSID (0x40|SID), payload...].
 * - NEG response = [length=3, 0x7F, SID, NRC].
 *
 * Context:
 * - Uses udsCtx.payload and udsCtx.payload_len to assemble response.
 * - CAN Tx via FLEXCAN0_transmit_msg().
 */
void UDS_SendResponse(void) {
    if (udsCtx.flow == UDS_FLOW_NEG) {
        /* ---- Negative Response ---- */
        CAN_Message_t msg;
        msg.canID   = TX_MSG_ID_UDS;
        msg.dlc     = 4;
        msg.data[0] = 0x03;        /* length=3 */
        msg.data[1] = 0x7F;        /* NRC header */
        msg.data[2] = udsCtx.sid;  /* original SID */
        msg.data[3] = udsCtx.nrc;  /* NRC */
        FLEXCAN0_transmit_msg(&msg);

    } else if (udsCtx.flow == UDS_FLOW_POS) {
        /* ---- Positive Response ---- */
        uint8_t  response_sid     = udsCtx.sid + 0x40;
        uint16_t total_uds_length = 1 + udsCtx.payload_len;

        if (total_uds_length <= 7) {
            /* Single Frame response */
            CAN_Message_t msg;
            msg.canID   = TX_MSG_ID_UDS;
            msg.dlc     = 1 + total_uds_length;
            msg.data[0] = (uint8_t)total_uds_length;
            msg.data[1] = response_sid;
            if (udsCtx.payload && udsCtx.payload_len) {
                memcpy(&msg.data[2], udsCtx.payload, udsCtx.payload_len);
            }
            FLEXCAN0_transmit_msg(&msg);
        } else {
            /* Multi-frame response via ISO-TP */
            static uint8_t full_uds_payload[4095];
            full_uds_payload[0] = response_sid;
            if (udsCtx.payload && udsCtx.payload_len) {
                memcpy(&full_uds_payload[1], udsCtx.payload, udsCtx.payload_len);
            }
            UDS_SendMultiFrameISO_TP(full_uds_payload, total_uds_length);
        }
    }

    /* Special case: Trigger ECU Reset after positive 0x11 response */
	if (udsCtx.flow == UDS_FLOW_POS && udsCtx.sid == UDS_SERVICE_ECU_RESET) {
		ECU_Reset();
	}

}

// =====================================================
// ==== ECU Reset Handler (SID 0x11) ====
// =====================================================

/**
 * @brief Handles ECU Reset service (SID = 0x11).
 *
 * Validates format and sub-function, checks reset conditions and security,
 * then prepares positive or negative response. If sub-function bit7 is set,
 * ECU reset may be triggered without response.
 *
 * @param msg_rx Incoming CAN message:
 *               - Format: [len, 0x11, sub-function].
 *               - sub-function 0x01 = Hard Reset (only supported).
 *
 * Processing logic:
 * 1) Verify length byte matches DLC - 1.
 * 2) Check DLC >= 3 (SID + subfunc).
 * 3) Verify sub-function = 0x01 (bit7 ignored).
 * 4) Check conditions (isResetConditionOk()).
 * 5) Check security access level.
 * 6) If bit7 clear:
 *    -> Prepare POS response [rspSID=0x51, sub-function].
 * 7) If bit7 set:
 *    -> flow=NONE, optional immediate ECU_Reset().
 *
 * Context:
 * - Updates udsCtx.flow and payload.
 * - ECU reset performed separately if required.
 */
void handleECUReset(const CAN_Message_t msg_rx) {
	 if (msg_rx.data[0] != (msg_rx.dlc - 1)) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_INCORRECT_LENGTH;
		return;
	}

    if (msg_rx.dlc < 3) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc  = NRC_INCORRECT_LENGTH;
        return;
    }

    uint8_t subFunc = msg_rx.data[2];

    if ((subFunc & 0x7F) != 0x01) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc  = NRC_SUBFUNC_NOT_SUPPORTED;
        return;
    }

    if (!isResetConditionOk()) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc  = NRC_CONDITIONS_NOT_CORRECT;
        return;
    }

    if (currentSecurityLevel < SECURITY_LEVEL_ENGINE) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc  = NRC_SECURITY_ACCESS_DENIED;
        return;
    }

    // Positive response (bit7=0)
    if (!(subFunc & 0x80)) {
        static uint8_t payload[1];
        payload[0] = subFunc;

        udsCtx.flow        = UDS_FLOW_POS;
        udsCtx.payload     = payload;
        udsCtx.payload_len = 1;
    } else {
        // No response, optional immediate ECU_Reset()
        udsCtx.flow = UDS_FLOW_NONE;
        ECU_Reset();
    }

}

// =====================================================
// ==== WriteDataByIdentifier Handler (SID 0x2E) ====
// =====================================================

/**
 * @brief Handles WriteDataByIdentifier (SID = 0x2E).
 *
 * Writes a new value to a supported DID (Data Identifier), after
 * validating message length, DID support, security, and value range.
 *
 * @param msg_rx Incoming CAN message:
 *               Format: [len, 0x2E, DID_H, DID_L, data...].
 *
 * Processing logic:
 * 1) Verify length byte matches DLC - 1.
 * 2) Check minimum length (>=5 bytes).
 * 3) Extract DID = data[2..3], verify supported (only DID_THRESHOLD).
 * 4) Reject if message too long (>=7).
 * 5) Check security (isSecurityAccessGranted()).
 * 6) Check conditions (isConditionOk()).
 * 7) Parse new value (2 bytes).
 * 8) Range check (value < 4096).
 * 9) Write to NVM (writeToNVM()).
 *    - On failure -> NRC_GENERAL_PROGRAMMING_FAILURE.
 * 10) On success -> POS response [rspSID, DID_H, DID_L].
 *
 * Context:
 * - Updates engineTemp variable if write successful.
 * - Uses udsCtx.payload to return DID echo.
 */
void handleWriteDataByIdentifier(const CAN_Message_t msg_rx) {
	 if (msg_rx.data[0] != (msg_rx.dlc - 1)) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_INCORRECT_LENGTH;
		return;
	}

    if (msg_rx.dlc < 5) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc  = NRC_INCORRECT_LENGTH;
        return;
    }

    uint16_t did = (msg_rx.data[2] << 8) | msg_rx.data[3];
    const uint8_t *data_payload = &msg_rx.data[4];
    uint16_t data_len = msg_rx.dlc - 3; /* dlc = 1(len) + 1(sid) + 2(did) + n(data) -> len = dlc - 4 */

    uint32_t nvm_offset;
    uint8_t expected_len;

    switch (did) {
		case DID_VEHICLE_ID:
			nvm_offset = DID_VEHICLE_ID_OFFSET;
			expected_len = 4;
			break;
		case DID_TEMP_THRESHOLD_LOW:
			nvm_offset = DID_TEMP_THRESHOLD_LOW_OFFSET;
			expected_len = 2;
			break;
		case DID_TEMP_THRESHOLD_MEDIUM:
			nvm_offset = DID_TEMP_THRESHOLD_MEDIUM_OFFSET;
			expected_len = 2;
			break;
		case DID_TEMP_THRESHOLD_HIGH:
			nvm_offset = DID_TEMP_THRESHOLD_HIGH_OFFSET;
			expected_len = 2;
			break;
		default:
			udsCtx.flow = UDS_FLOW_NEG;
			udsCtx.nrc = NRC_REQUEST_OUT_OF_RANGE;
			return;
	}

    if (data_len != (expected_len + 1)) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_INCORRECT_LENGTH;
		return;
	}

    if (!isSecurityAccessGranted(did)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc  = NRC_SECURITY_ACCESS_DENIED;
        return;
    }

    if (!isConditionOk(did)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc  = NRC_CONDITIONS_NOT_CORRECT;
        return;
    }

    /* Prepare data to write */
    uint8_t buffer[8];
    if (expected_len == 4) {
    	uint32_t temp_value_32 = ((uint32_t)data_payload[0] << 24) |
                              ((uint32_t)data_payload[1] << 16) |
                              ((uint32_t)data_payload[2] << 8)  |
                              ((uint32_t)data_payload[3]);
    	memcpy(buffer, &temp_value_32, 4);
    } else if (expected_len == 2) {
    	uint16_t temp_value_16 = ((uint16_t)data_payload[0] << 8) |
    	                       ((uint16_t)data_payload[1]);
    	memcpy(buffer, &temp_value_16, 2);
    }
	if (NVM_Write(nvm_offset, buffer, expected_len) == NVM_OK) {
		uint16_t new_value = (buffer[1] << 8) | buffer[0];
		/* Update threshold variable after write to NVM */
		if (did == DID_TEMP_THRESHOLD_LOW)
			temp_threshold_low = new_value;
		if (did == DID_TEMP_THRESHOLD_MEDIUM)
			temp_threshold_medium = new_value;
		if (did == DID_TEMP_THRESHOLD_HIGH)
			temp_threshold_high = new_value;

		/* Positive response */
		static uint8_t payload[2];
		payload[0] = msg_rx.data[2];
		payload[1] = msg_rx.data[3];
		udsCtx.flow = UDS_FLOW_POS;
		udsCtx.payload = payload;
		udsCtx.payload_len = 2;
	} else {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_GENERAL_PROGRAMMING_FAILURE;
	}
}

/**
 * @brief Handles ReadDataByIdentifier (SID = 0x22).
 *
 * Reads one or more DIDs and builds a response with DID+value pairs.
 *
 * @param msg_rx Incoming CAN message:
 *               Format: [len, 0x22, DID1_H, DID1_L, DID2_H, DID2_L, ...].
 *
 * Processing logic:
 * 1) Verify length byte matches DLC - 1.
 * 2) Verify minimum DLC >= 4 and even number of bytes after SID.
 * 3) For each DID:
 *    - Check if DID is supported (ENGINE_TEMP, THRESHOLD, ENGINE_LIGHT).
 *    - Ensure response payload does not exceed buffer.
 *    - Validate security (isSecurityAccessGranted()).
 *    - Validate conditions (isConditionOk()).
 *    - Read value:
 *       * DID_ENGINE_TEMP   -> myADC_Read(13).
 *       * DID_ENGINE_LIGHT  -> myADC_Read(12).
 *       * DID_THRESHOLD     -> engineTemp variable.
 *    - Append DID_H, DID_L, val_H, val_L to response payload.
 * 4) If no valid DID found -> NRC_REQUEST_OUT_OF_RANGE.
 * 5) Otherwise -> POS response [rspSID, DID+val...].
 *
 * Context:
 * - Uses static response_payload[] buffer.
 * - Updates udsCtx.flow and payload.
 */
void handleReadDataByIdentifier(const CAN_Message_t msg_rx) {
	 if (msg_rx.data[0] != (msg_rx.dlc - 1)) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_INCORRECT_LENGTH;
		return;
	}

    if (msg_rx.dlc < 4 || (msg_rx.dlc % 2) != 0) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc  = NRC_INCORRECT_LENGTH;
        return;
    }

    static uint8_t response_payload[16];
    uint16_t payload_len = 0;

    for (uint8_t i = 2; i < msg_rx.dlc; i += 2) {
		uint16_t did = (msg_rx.data[i] << 8) | msg_rx.data[i + 1];

		if (payload_len + 4 > sizeof(response_payload)) {
			udsCtx.flow = UDS_FLOW_NEG;
			udsCtx.nrc = NRC_RESPONSE_TOO_LONG;
			return;
		}
		if (!isSecurityAccessGranted(did)) {
			udsCtx.flow = UDS_FLOW_NEG;
			udsCtx.nrc = NRC_SECURITY_ACCESS_DENIED;
			return;
		}
		if (!isConditionOk(did)) {
			udsCtx.flow = UDS_FLOW_NEG;
			udsCtx.nrc = NRC_CONDITIONS_NOT_CORRECT;
			return;
		}

		response_payload[payload_len++] = (uint8_t) (did >> 8);
		response_payload[payload_len++] = (uint8_t) did;

		switch (did) {
			case DID_ENGINE_TEMP: {
				response_payload[payload_len++] = (uint8_t) (temperature >> 8);
				response_payload[payload_len++] = (uint8_t) temperature;
				break;
			}
			case DID_ENGINE_LIGHT: {
				response_payload[payload_len++] = (uint8_t) (light_level >> 8);
				response_payload[payload_len++] = (uint8_t) light_level;
				break;
			}
			case DID_VEHICLE_ID: {
				uint32_t vid_val;
				NVM_Read(DID_VEHICLE_ID_OFFSET, (uint8_t*) &vid_val, 4);
				response_payload[payload_len++] = (uint8_t) (vid_val >> 24);
				response_payload[payload_len++] = (uint8_t) (vid_val >> 16);
				response_payload[payload_len++] = (uint8_t) (vid_val >> 8);
				response_payload[payload_len++] = (uint8_t) vid_val;
				break;
			}
			case DID_TEMP_THRESHOLD_LOW: {
				response_payload[payload_len++] = (uint8_t) (temp_threshold_low >> 8);
				response_payload[payload_len++] = (uint8_t) temp_threshold_low;
				break;
			}
			case DID_TEMP_THRESHOLD_MEDIUM: {
				response_payload[payload_len++] = (uint8_t) (temp_threshold_medium >> 8);
				response_payload[payload_len++] = (uint8_t) temp_threshold_medium;
				break;
			}
			case DID_TEMP_THRESHOLD_HIGH: {
				response_payload[payload_len++] = (uint8_t) (temp_threshold_high >> 8);
				response_payload[payload_len++] = (uint8_t) temp_threshold_high;
				break;
			}
			default:
				udsCtx.flow = UDS_FLOW_NEG;
				udsCtx.nrc = NRC_REQUEST_OUT_OF_RANGE;
				return;
		}
    }

    if (payload_len == 0) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc  = NRC_REQUEST_OUT_OF_RANGE;
        return;
    }

    udsCtx.flow        = UDS_FLOW_POS;
    udsCtx.payload     = response_payload;
    udsCtx.payload_len = payload_len;
}

/**
 * @brief UDS Service 0x19 main dispatcher (ReadDTCInformation).
 *
 * Validates request length/format, extracts sub-function, dispatches to
 * a specific handler, and finally sends the response via UDS_SendResponse().
 *
 * @param requestMsg Pointer to the received CAN message (data[], dlc).
 *
 * Processing logic:
 * 1) Reset udsCtx; store SID=SID_READ_DTC_INFORMATION.
 * 2) Validate: (dlc >= 3) AND (dlc == data[0] + 1); else NEG + NRC_FORMAT.
 * 3) sub_function = data[2]; switch to sub-handlers:
 *    - 0x01, 0x02, 0x04, 0x0A supported; default → NEG + NRC_SUB_FUNCTION_NOT_SUPPORTED.
 * 4) Call UDS_SendResponse() once after handler returns.
 *
 * Context:
 * - Uses udsCtx.flow/nrc/payload to form final CAN response (SF or ISO-TP).
 */
void handleReadDTCInformation(const CAN_Message_t *requestMsg) {
    /* Reset context, but crucially, set the SID for this transaction */
    udsCtx.flow = UDS_FLOW_NONE;
    udsCtx.sid = UDS_SERVICE_READ_DTC_INFORMATION;
    udsCtx.nrc = 0;
    udsCtx.payload = NULL;
    udsCtx.payload_len = 0;

    /* Perform initial validation on the incoming request frame.
       The DLC must match the length specified in the first data byte. */
    if (requestMsg->dlc < 3 || requestMsg->dlc != (requestMsg->data[0] + 1)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
    }

    /* In our custom format, SID is at data[1] and Sub-function is at data[2]. */
    uint8_t sub_function = requestMsg->data[2];

    /* Dispatch to the appropriate handler based on the sub-function. */
    switch (sub_function) {
        case SF_REPORT_NUMBER_OF_DTC_BY_STATUS_MASK:
            sf_reportNumberOfDTCByStatusMask(requestMsg);
            break;
        case SF_REPORT_DTC_BY_STATUS_MASK:
            sf_reportDTCByStatusMask(requestMsg);
            break;
        case SF_REPORT_DTC_SNAPSHOT_RECORD_BY_DTC_NUMBER:
            sf_reportDTCSnapshotByDTCNumber(requestMsg);
            break;
        case SF_REPORT_SUPPORTED_DTC:
            sf_reportSupportedDTC(requestMsg);
            break;
        default:
            /* If the sub-function is not supported, send a negative response. */
            udsCtx.flow = UDS_FLOW_NEG;
            udsCtx.nrc = NRC_SUBFUNC_NOT_SUPPORTED;
            break;
    }
}

void handleClearDiagnosticInformation(const CAN_Message_t *requestMsg) {
    udsCtx.sid = UDS_SERVICE_CLEAR_DTC;

    if (requestMsg->dlc != 5 || requestMsg->data[0] != 0x04) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    uint32_t groupOfDTC =
        ((uint32_t)requestMsg->data[2] << 16) |
        ((uint32_t)requestMsg->data[3] << 8) |
        (uint32_t)requestMsg->data[4];

    if (!isGroupOfDTCSupported(groupOfDTC)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_REQUEST_OUT_OF_RANGE;
        return;
    }

    if (!isConditionOkForClear()) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_CONDITIONS_NOT_CORRECT;
        return;
    }

    if (!clearDTCFromNVM(groupOfDTC)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_GENERAL_PROGRAMMING_FAILURE;
        return;
    }

    /* Positive: no payload for 0x14 */
    udsCtx.flow = UDS_FLOW_POS;
    udsCtx.payload = NULL;
    udsCtx.payload_len = 0;
}


// =====================================================
// ==== ECU Reset implementation ====
// =====================================================

#define SCB_AIRCR               (*(volatile uint32_t*)0xE000ED0C)
#define SCB_AIRCR_VECTKEY_MASK  (0x5FAu << 16)
#define SCB_AIRCR_SYSRESETREQ   (1u << 2)

/**
 * @brief Perform ECU reset using Cortex-M System Control Block (SCB).
 *
 * Writes to SCB->AIRCR register with SYSRESETREQ set. Causes system-wide reset,
 * equivalent to a hardware reset. CPU and peripherals are reinitialized.
 *
 * Processing logic:
 * 1) Write AIRCR with:
 *    - VECTKEY = 0x5FA (bits [31:16]).
 *    - SYSRESETREQ bit = 1.
 * 2) Enter infinite loop waiting for reset.
 *
 * @note This function does not return. System restarts automatically.
 */
void ECU_Reset(void) {
    SCB_AIRCR = SCB_AIRCR_VECTKEY_MASK | SCB_AIRCR_SYSRESETREQ;
    while (1) {

    }
}
