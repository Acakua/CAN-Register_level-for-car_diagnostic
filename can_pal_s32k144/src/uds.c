#include "uds.h"
#include "adc.h"
#include <string.h>
#include "nvm.h"

// ==== Global Variables ====

/** Current security level (used for access checks in UDS services) */
uint8_t  currentSecurityLevel = SECURITY_LEVEL_ENGINE;

/** Threshold engine temperature (example DID value) */
uint16_t engineTemp           = 0x1234;



// ==== UDS Context Types ====

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


// ==== Utility Functions ====

/**
 * @brief Simple busy-wait delay (optional)
 *
 * @param ms Milliseconds to wait
 */
static void delay_ms(volatile uint32_t ms) {
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 5000; j++) { 
            __asm__("nop");
        }
    }
}


// ==== UDS Dispatcher ====

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
        udsCtx.nrc  = NRC_SERVICE_NOT_SUPPORTED;
        break;
    }

    UDS_SendResponse();
}


// ==== UDS Response Sender ====

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
        // ---- Negative Response ----
        CAN_Message_t msg;
        msg.canID   = TX_MSG_ID_UDS;
        msg.dlc     = 4;
        msg.data[0] = 0x03;        // length=3
        msg.data[1] = 0x7F;        // NRC header
        msg.data[2] = udsCtx.sid;  // original SID
        msg.data[3] = udsCtx.nrc;  // NRC
        FLEXCAN0_transmit_msg(&msg);

    } else if (udsCtx.flow == UDS_FLOW_POS) {
        // ---- Positive Response ----
        uint8_t  response_sid     = udsCtx.sid + 0x40;
        uint16_t total_uds_length = 1 + udsCtx.payload_len;

        if (total_uds_length <= 7) {
            // Single Frame response
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
            // Multi-frame response via ISO-TP
            static uint8_t full_uds_payload[4095];
            full_uds_payload[0] = response_sid;
            if (udsCtx.payload && udsCtx.payload_len) {
                memcpy(&full_uds_payload[1], udsCtx.payload, udsCtx.payload_len);
            }
            UDS_SendMultiFrameISO_TP(full_uds_payload, total_uds_length);
        }
    }

    // Special case: Trigger ECU Reset after positive 0x11 response
	if (udsCtx.flow == UDS_FLOW_POS && udsCtx.sid == UDS_SERVICE_ECU_RESET) {
		ECU_Reset();
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
void UDS_SendMultiFrameISO_TP(const uint8_t *data, uint16_t length) {
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


// ==== WriteDataByIdentifier Handler (SID 0x2E) ====

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

    if (did != DID_THRESHOLD) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc  = NRC_REQUEST_OUT_OF_RANGE;
        return;
    }

    if (msg_rx.dlc >= 7) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc  = NRC_INCORRECT_LENGTH;
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

    uint16_t newVal = (msg_rx.data[4] << 8) | msg_rx.data[5];
    if (newVal >= 4096) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc  = NRC_REQUEST_OUT_OF_RANGE;
        return;
    }

    if (!writeToNVM(did, newVal)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc  = NRC_GENERAL_PROGRAMMING_FAILURE;
        return;
    }

    // Build POS payload
    static uint8_t payload[2];
    payload[0] = msg_rx.data[2];
    payload[1] = msg_rx.data[3];

    udsCtx.flow        = UDS_FLOW_POS;
    udsCtx.payload     = payload;
    udsCtx.payload_len = 2;
}

// ==== ReadDataByIdentifier Handler (SID 0x22) ====

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

        if (did == DID_ENGINE_TEMP || did == DID_THRESHOLD || did == DID_ENGINE_LIGHT) {
            if (payload_len + 4 > sizeof(response_payload)) {
                udsCtx.flow = UDS_FLOW_NEG;
                udsCtx.nrc  = NRC_RESPONSE_TOO_LONG;
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

            uint16_t value;
            if (did == DID_ENGINE_TEMP) {
                value = myADC_Read(13);
            } else if (did == DID_ENGINE_LIGHT) {
                value = myADC_Read(12);
            } else {
                value = engineTemp;
            }

            response_payload[payload_len++] = (uint8_t)(did >> 8);
            response_payload[payload_len++] = (uint8_t)(did & 0xFF);
            response_payload[payload_len++] = (uint8_t)(value >> 8);
            response_payload[payload_len++] = (uint8_t)(value & 0xFF);
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


// ==== Helpers & Stubs ====

/**
 * @brief Checks if ECU reset conditions are OK.
 *
 * Could check hardware/software state, ignition, etc.
 * In demo: always returns true.
 */
bool isResetConditionOk(void) { return true; }

/**
 * @brief Checks if the given DID has security access granted.
 *
 */
bool isSecurityAccessGranted(uint16_t did) {
    (void)did;
    return true;
}

/**
 * @brief Checks if the given DID can be accessed under current conditions.
 *
 */
bool isConditionOk(uint16_t did) {
    (void)did;
    return true;
}

/**
 * @brief Writes a value to NVM for a specific DID.
 *
 * This function uses the memory layout defined in nvm.h to determine
 * the correct address to write to. It supports writing various DID types and can be
 * easily extended.
 *
 * @param did   The Data Identifier to be written.
 * @param value The value to be stored.
 * @return true on successful write, false on failure or if the DID is not supported.
 */
bool writeToNVM(uint16_t did, uint16_t value) {
    uint32_t offset;

    // 1. Determine the correct NVM offset based on the DID.
    switch (did) {
        case DID_THRESHOLD:
            offset = DID_ENGINE_TEMP_NVM_OFFSET;
            break;
        // DID others

        default:
            // This DID is not supported for writing.
            return false;
    }

    // 2. Prepare the 16-bit value as a 2-byte array for NVM writing.
    uint8_t data_to_write[2];
    data_to_write[0] = (uint8_t)(value >> 8);   // High Byte
    data_to_write[1] = (uint8_t)(value & 0xFF); // Low Byte

    // 3. Call the NVM driver to perform the write operation.
    if (NVM_Write(offset, data_to_write, 2) == NVM_OK) {
        return true; // Write successful!
    } else {
        return false; // NVM write failed.
    }
}


// ==== ECU Reset implementation ====

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

