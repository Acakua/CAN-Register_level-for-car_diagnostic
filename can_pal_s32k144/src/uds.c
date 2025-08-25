#include "uds.h"
#include "dtc.h"
#include <string.h>
#include "FlexCan.h"
#include <stdbool.h>

/**
 * @brief Defines the type of response flow for the current UDS transaction.
 */
typedef enum {
    UDS_FLOW_NONE = 0, /* No response should be sent */
    UDS_FLOW_POS,      /* Positive response */
    UDS_FLOW_NEG       /* Negative response */
} UDS_FlowType;

/**
 * @brief Holds the complete context for the currently processing UDS transaction.
 * All necessary data for sending a response is stored here.
 */
typedef struct {
    UDS_FlowType   flow;          /* POS / NEG / NONE */
    uint8_t        sid;           /* The Service ID of the original request */
    uint8_t        nrc;           /* Negative Response Code if flow = NEG */
    const uint8_t* payload;       /* Pointer to the positive response payload */
    uint16_t       payload_len;   /* Length of the positive response payload */
} UDS_Context;

/* Global context for the UDS service handler */
static UDS_Context udsCtx;

/**
 * @brief Simple blocking delay function.
 * @note This is a crude delay. For production, a hardware timer is recommended.
 * @param ms Milliseconds to delay.
 */
static void delay_ms(volatile uint32_t ms) {
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 5000; j++) {
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

	for (uint8_t i = 0; i < DTC_GetCount(); ++i) {
		DTC_Record_t record;
		if (DTC_GetRecord(i, &record)) {
			if ((requested_mask == 0xFF)
					|| ((record.status_mask & requested_mask) == requested_mask)) {
				count++;
			}
		}
	}

	payload[0] = SF_REPORT_NUMBER_OF_DTC_BY_STATUS_MASK;
	payload[1] = 0xFF;
	payload[2] = DTC_FORMAT_ID_ISO14229_1;
	payload[3] = (uint8_t) (count >> 8);
	payload[4] = (uint8_t) (count & 0xFF);

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
			if ((requested_mask == 0xFF)
					|| ((record.status_mask & requested_mask) == requested_mask)) {
				/* For each matching DTC, append its 3-byte code and 1-byte status to the payload. */
				payload[payload_len++] = (uint8_t) (record.dtc_code >> 16);
				payload[payload_len++] = (uint8_t) (record.dtc_code >> 8);
				payload[payload_len++] = (uint8_t) (record.dtc_code);
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

	if(requested_record_number != 0x01 && requested_record_number != 0xFF) {
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
			payload[payload_len++] = (uint8_t) (record.snapshot.year >> 8);   /* Year, High Byte (Big-Endian) */
			payload[payload_len++] = (uint8_t) (record.snapshot.year & 0xFF); /* Year, Low Byte */

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

/* --- Main Service Handler --- */

/**
 * @brief Send a UDS response according to the current udsCtx (POS/NEG/NONE).
 *
 * Chooses negative response (single frame 0x7F) or positive response.
 * Positive path selects Single Frame for ≤7 bytes (RspSID + payload) or
 * falls back to multi-frame ISO-TP for longer responses.
 *
 * Parameters: none (uses global udsCtx)
 *
 * Processing logic:
 * 1) If udsCtx.flow == NEG:
 *    - Build 4-byte SF: [03][7F][OriginalSID][NRC], send.
 * 2) If udsCtx.flow == POS:
 *    - Compute total (1 + payload_len). If ≤7: pack into one SF.
 *    - Else: assemble (RspSID + payload) and call ISO-TP sender.
 * 3) If udsCtx.flow == NONE: do nothing.
 *
 * Context:
 * - udsCtx.sid: original request SID to form response or NRC frame.
 * - udsCtx.payload / payload_len: POS response data (excludes RspSID).
 * - udsCtx.nrc: negative response code when needed.
 */
void UDS_SendResponse(void) {
    if (udsCtx.flow == UDS_FLOW_NEG) {
        /* Send Negative Response */
        CAN_Message_t msg;
        msg.canID = TX_MSG_ID_UDS;
        msg.dlc   = 4;
        msg.data[0] = 0x03;       /* PCI: Single Frame, length 3 */
        msg.data[1] = 0x7F;       /* NRC Service ID */
        msg.data[2] = udsCtx.sid; /* Original SID from context */
        msg.data[3] = udsCtx.nrc; /* NRC from context */
        FLEXCAN0_transmit_msg(&msg);

    } else if (udsCtx.flow == UDS_FLOW_POS) {
        /* Send Positive Response */
        uint8_t response_sid = udsCtx.sid + 0x40;
        uint16_t total_uds_length = 1 + udsCtx.payload_len; /* ResponseSID + payload */

        if (total_uds_length <= 7) {
            /* Fits in a single frame */
            CAN_Message_t msg;
            msg.canID = TX_MSG_ID_UDS;
            msg.dlc   = 1 + total_uds_length;
            msg.data[0] = (uint8_t)total_uds_length;
            msg.data[1] = response_sid;

            if (udsCtx.payload != NULL && udsCtx.payload_len > 0) {
                memcpy(&msg.data[2], udsCtx.payload, udsCtx.payload_len);
            }
            FLEXCAN0_transmit_msg(&msg);
        } else {
            /* Requires multi-frame (ISO-TP) */
            static uint8_t full_uds_payload[4095];
            full_uds_payload[0] = response_sid;
            if (udsCtx.payload != NULL && udsCtx.payload_len > 0) {
                memcpy(&full_uds_payload[1], udsCtx.payload, udsCtx.payload_len);
            }
            UDS_SendMultiFrameISO_TP(full_uds_payload, total_uds_length);
        }
    }
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
    UDS_SendResponse();
}

/**
 * @brief Dispatch a UDS request to the service-specific handler and send a response.
 *
 * Extracts the Service ID (SID) from msg_rx.data[1], resets the UDS context for
 * this transaction, routes to the corresponding handler, and finally calls
 * UDS_SendResponse() to emit either a positive or a negative response.
 *
 * @param msg_rx Received CAN message: includes payload in msg_rx.data[] and DLC.
 *
 * Processing logic:
 * 1) Read SID = msg_rx.data[1].
 * 2) Reset udsCtx for this transaction:
 *    - udsCtx.flow = UDS_FLOW_NONE
 *    - udsCtx.sid  = SID
 *    - udsCtx.nrc  = 0
 * 3) Switch on SID:
 *    - UDS_SERVICE_READ_DTC_INFORMATION → handleReadDTCInformation(&msg_rx)
 *    - UDS_SERVICE_CLEAR_DTC            → handleClearDiagnosticInformation(msg_rx)
 *    - default                          → NEG: udsCtx.nrc = NRC_SERVICE_NOT_SUPPORTED
 * 4) Call UDS_SendResponse() once to transmit POS/NEG response based on udsCtx.
 *
 * Context:
 * - Uses the global udsCtx to carry response state (flow/nrc/payload).
 * - Handlers are responsible for validating request format/length and
 *   setting udsCtx.flow/nrc/payload accordingly.
 *
 */
void UDS_DispatchService(const CAN_Message_t msg_rx) {
	uint8_t sid = msg_rx.data[1];

	/* reset context */
	udsCtx.flow = UDS_FLOW_NONE;
	udsCtx.sid = sid;
	udsCtx.nrc = 0;

	switch (sid) {
	case UDS_SERVICE_READ_DTC_INFORMATION:
		handleReadDTCInformation(&msg_rx);
		break;
    case UDS_SERVICE_CLEAR_DTC:
        handleClearDiagnosticInformation(&msg_rx);
        break;
	default:
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_SERVICE_NOT_SUPPORTED;
		break;
	}
	UDS_SendResponse();
}
