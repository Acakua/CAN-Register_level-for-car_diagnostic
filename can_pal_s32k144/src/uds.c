/*
 * @brief  Implementation of the UDS Service 0x14 (ClearDiagnosticInformation)
 *         with improved logic and detailed English comments.
 */

#include "uds.h"
#include "dtc.h"
#include <string.h>
#include "FlexCan.h"
#include <stdbool.h>

/**
 * @brief Defines the type of response flow for the current UDS transaction.
 */
typedef enum {
    UDS_FLOW_NONE = 0, /* No response to be sent */
    UDS_FLOW_POS,      /* Positive response */
    UDS_FLOW_NEG       /* Negative response */
} UDS_FlowType;

/**
 * @brief Holds all necessary information for responding to the current UDS request.
 */
typedef struct {
    UDS_FlowType   flow;          /* POS / NEG / NONE */
    uint8_t        sid;           /* Requested Service ID */
    uint8_t        nrc;           /* Negative Response Code if NEG */
    const uint8_t* payload;       /* Pointer to POS response payload (if any) */
    uint16_t       payload_len;   /* Length of POS response payload */
} UDS_Context;

/* Global context for UDS */
static UDS_Context udsCtx;

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
 * @brief UDS service dispatcher.
 *        Calls the appropriate service handler based on SID.
 */
void UDS_DispatchService(const CAN_Message_t msg_rx) {
    uint8_t sid = msg_rx.data[1];

    /* Reset UDS context for new request */
    udsCtx.flow = UDS_FLOW_NONE;
    udsCtx.sid = sid;
    udsCtx.nrc = 0;

    switch (sid) {
        case UDS_SERVICE_READ_DTC_INFORMATION:
            handleReadDTCInformation(&msg_rx);
            break;

        case UDS_SERVICE_CLEAR_DTC:
            handleClearDiagnosticInformation(msg_rx);
            break;

        default:
            udsCtx.flow = UDS_FLOW_NEG;
            udsCtx.nrc = NRC_SERVICE_NOT_SUPPORTED;
            break;
    }

    /* Send response after processing */
    UDS_SendResponse();
}

/**
 * @brief Sends either a Positive or Negative UDS response based on the context.
 */
void UDS_SendResponse(void) {
    if (udsCtx.flow == UDS_FLOW_NEG) {
        /* === Send Negative Response Frame === */
        CAN_Message_t msg;
        msg.canID = TX_MSG_ID_UDS;
        msg.dlc   = 4;
        msg.data[0] = 0x03;       /* PCI: Single Frame, length 3 */
        msg.data[1] = 0x7F;       /* NRC header */
        msg.data[2] = udsCtx.sid; /* Original SID */
        msg.data[3] = udsCtx.nrc; /* NRC code */
        FLEXCAN0_transmit_msg(&msg);

    } else if (udsCtx.flow == UDS_FLOW_POS) {
        /* === Send Positive Response === */
        uint8_t response_sid = udsCtx.sid + 0x40;
        uint16_t total_len = 1 + udsCtx.payload_len; // SID + payload

        if (total_len <= 7) {
            /* Fits into Single Frame */
            CAN_Message_t msg;
            msg.canID   = TX_MSG_ID_UDS;
            msg.dlc     = 1 + total_len;
            msg.data[0] = (uint8_t)total_len;
            msg.data[1] = response_sid;

            if (udsCtx.payload && udsCtx.payload_len > 0) {
                memcpy(&msg.data[2], udsCtx.payload, udsCtx.payload_len);
            }
            FLEXCAN0_transmit_msg(&msg);

        } else {
            /* Requires Multi-Frame (ISO-TP) transmission */
            static uint8_t full_payload[4095];
            full_payload[0] = response_sid;

            if (udsCtx.payload && udsCtx.payload_len > 0) {
                memcpy(&full_payload[1], udsCtx.payload, udsCtx.payload_len);
            }
            UDS_SendMultiFrameISO_TP(full_payload, total_len);
        }
    }
}

/**
 * @brief Handles UDS Service 0x14: ClearDiagnosticInformation.
 *
 * Format: [len] [SID] [DTC-high-byte] [DTC-mid-byte] [DTC-low-byte]
 */
void handleClearDiagnosticInformation(const CAN_Message_t msg_rx) {
    /* Check request length: 1 len + 1 SID + 3 bytes groupOfDTC */
    if (msg_rx.dlc != 5 || msg_rx.data[0] != 0x04) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    /* Extract GroupOfDTC from request */
    uint32_t groupOfDTC =
        (msg_rx.data[2] << 16) |
        (msg_rx.data[3] << 8)  |
         msg_rx.data[4];

    /* Validate that the requested GroupOfDTC is supported */
    if (!isGroupOfDTCSupported(groupOfDTC)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_REQUEST_OUT_OF_RANGE;
        return;
    }

    /* Check operational conditions for clearing */
    if (!isConditionOkForClear()) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_CONDITIONS_NOT_CORRECT;
        return;
    }

    /* Try to clear DTC(s) from NVM */
    if (!clearDTCFromNVM(groupOfDTC)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_GENERAL_PROGRAMMING_FAILURE;
        return;
    }

    /* Positive Response: No payload required for service 0x14 */
    udsCtx.flow = UDS_FLOW_POS;
    udsCtx.payload = NULL;
    udsCtx.payload_len = 0;
}
