#include "uds.h"
#include "adc.h"
#include "nvm.h"

// ==== Global Variables ====
// Current security level
uint8_t currentSecurityLevel = SECURITY_LEVEL_ENGINE;

// Threshold engine temperature used for ReadDID response
uint16_t engineTemp = 0x1234;

/**
 * @brief Data type representing the UDS response state.
 *
 * Enumerates the possible response flows in the UDS transaction:
 * - UDS_FLOW_NONE: No response will be sent.
 * - UDS_FLOW_POS:  Positive Response.
 * - UDS_FLOW_NEG:  Negative Response.
 */
typedef enum {
    UDS_FLOW_NONE = 0,   // No response to be sent
    UDS_FLOW_POS,        // Positive Response
    UDS_FLOW_NEG         // Negative Response
} UDS_FlowType;

/**
 * @brief Structure holding the current UDS processing state.
 *
 * Members:
 * - flow: POS/NEG/NONE for current transaction.
 * - sid:  Service ID being processed.
 * - nrc:  Negative Response Code if flow = NEG.
 * - msg:  Prepared CAN message for POS response.
 */
typedef struct {
    UDS_FlowType flow;   // POS / NEG / NONE
    uint8_t sid;         // Service ID of the current request
    uint8_t nrc;         // Negative Response Code (if flow = NEG)
    CAN_Message_t msg;   // Buffer containing the response packet
} UDS_Context;

// Global UDS context: persistent across service calls
static UDS_Context udsCtx;

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
 * 2) Reset udsCtx: flow=NONE, sid=SID, nrc=0.
 * 3) Switch on SID:
 *    - 0x11 (ECU Reset)        → handleECUReset()
 *    - 0x2E (Write DID)        → handleWriteDataByIdentifier()
 *    - 0x22 (Read DID)         → handleReadDataByIdentifier()
 *    - default                 → flow=NEG, nrc=NRC_SERVICE_NOT_SUPPORTED
 * 4) Call UDS_SendResponse().
 *
 * Context:
 * - Uses global udsCtx to store the response state.
 * - Handlers are responsible for validating format and setting udsCtx.
 */
void UDS_DispatchService(const CAN_Message_t msg_rx) {
    uint8_t sid = msg_rx.data[1];

    // Step 1: Reset context for new request
    udsCtx.flow = UDS_FLOW_NONE;
    udsCtx.sid = sid;
    udsCtx.nrc = 0;

    // Step 2: Dispatch to service-specific handler
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
        udsCtx.nrc = NRC_SERVICE_NOT_SUPPORTED;
        break;
    }

    // Step 3: Send final response
    UDS_SendResponse();
}

/**
 * @brief Transmits a UDS response according to udsCtx state.
 *
 * If udsCtx.flow == POS → send udsCtx.msg as-is.
 * If udsCtx.flow == NEG → construct and send a Negative Response [0x7F, SID, NRC].
 * If udsCtx.flow == NONE → send nothing.
 *
 * Special case: If POS response for ECU Reset is sent, trigger ECU reset after TX.
 */
void UDS_SendResponse(void) {
    CAN_Message_t rsp;
    rsp.canID = 0x768;

    if (udsCtx.flow == UDS_FLOW_POS) {
        // Send prepared POS response
        rsp = udsCtx.msg;
    } else if (udsCtx.flow == UDS_FLOW_NEG) {
        // Build standard NRC frame
        rsp.dlc = 4;
        rsp.data[0] = 0x03;           // Length byte
        rsp.data[1] = 0x7F;           // NRC identifier
        rsp.data[2] = udsCtx.sid;     // Original SID
        rsp.data[3] = udsCtx.nrc;     // Negative Response Code
    } else {
        return; // NONE → nothing to send
    }

    // Send via CAN
    FLEXCAN0_transmit_msg(&rsp);

    // Special case: Trigger ECU Reset after positive 0x11 response
    if (udsCtx.flow == UDS_FLOW_POS && udsCtx.sid == UDS_SERVICE_ECU_RESET) {
        ECU_Reset();
    }
}

/**
 * @brief Handles UDS ECU Reset (SID = 0x11).
 *
 * Validates request format, supported sub-function, conditions, and security.
 * If sub-function bit 7 is clear → send positive response first.
 * If bit 7 is set → perform reset immediately without response.
 *
 * @param msg_rx Incoming CAN message: [len, 0x11, sub-function]
 *
 * Steps:
 * 1) Verify length byte matches DLC - 1.
 * 2) Require DLC ≥ 3 bytes (len, SID, sub-function).
 * 3) Check sub-function = 0x01 (hard reset) only.
 * 4) Validate reset conditions and security access.
 * 5) If sub-function bit7=0 → prepare POS response [0x02 , 0x51, sub-function].
 *    If bit7=1 → trigger immediate reset without response.
 */
void handleECUReset(const CAN_Message_t msg_rx) {
    // Step 1: Length byte check
    if (msg_rx.data[0] != (msg_rx.dlc - 1)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    // Step 2: DLC check
    if (msg_rx.dlc < 3) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    uint8_t subFunc = msg_rx.data[2];

    // Step 3: Sub-function support check
    if ((subFunc & 0x7F) != 0x01) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_SUBFUNC_NOT_SUPPORTED;
        return;
    }

    // Step 4: Conditions check
    if (!isResetConditionOk()) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_CONDITIONS_NOT_CORRECT;
        return;
    }

    // Step 5: Security check
    if (currentSecurityLevel < SECURITY_LEVEL_ENGINE) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_SECURITY_ACCESS_DENIED;
        return;
    }

    // Step 6: Response or immediate reset
    if (!(subFunc & 0x80)) {
        udsCtx.flow = UDS_FLOW_POS;
        udsCtx.msg.canID = 0x768;
        udsCtx.msg.dlc   = 3;
        udsCtx.msg.data[0] = 0x02;       // Length
        udsCtx.msg.data[1] = 0x51;       // POS SID (0x11 + 0x40)
        udsCtx.msg.data[2] = subFunc;
    } else {
        udsCtx.flow = UDS_FLOW_NONE;
        ECU_Reset();
    }
}

/**
 * @brief Handles WriteDataByIdentifier (SID = 0x2E).
 *
 * Writes new value to a supported DID after validating:
 * - Length byte & DLC.
 * - Supported DID.
 * - Data size limit.
 * - Security & condition checks.
 * - Value range.
 * - NVM write success.
 *
 * @param msg_rx Incoming CAN message:
 *               [len, 0x2E, DID_H, DID_L, data_H, data_L]
 *
 * Steps:
 * 1) Length check (len == DLC - 1).
 * 2) Require DLC ≥ 5.  (1 byte len, 1 byte SID, 2 bytes DID, at least 1 byte data)
 * 3) DID support check (only DID_THRESHOLD allowed).
 * 4) Data length check (max 2 bytes data for DID_THRESHOLD).
 * 5) Security access validation.
 * 6) Condition validation.
 * 7) Value range check (< 4096).
 * 8) Write to NVM.
 * 9) Prepare POS response [0x03, 0x6E, DID high, DID Low].
 */
void handleWriteDataByIdentifier(const CAN_Message_t msg_rx) {
    // Step 1: Length check
    if (msg_rx.data[0] != (msg_rx.dlc - 1)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    // Step 2: Minimum length
    if (msg_rx.dlc < 5) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    uint16_t did = (msg_rx.data[2] << 8) | msg_rx.data[3];

    // Step 3: Supported DID check
    if (did != DID_THRESHOLD) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_REQUEST_OUT_OF_RANGE;
        return;
    }

    // Step 4: Max length
    if (msg_rx.dlc >= 7) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    // Step 5: Security check
    if (!isSecurityAccessGranted(did)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_SECURITY_ACCESS_DENIED;
        return;
    }

    // Step 6: Condition check
    if (!isConditionOk(did)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_CONDITIONS_NOT_CORRECT;
        return;
    }

    uint16_t newVal = (msg_rx.data[4] << 8) | msg_rx.data[5];

    // Step 7: Value range check
    if (newVal >= 4096) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_REQUEST_OUT_OF_RANGE;
        return;
    }

    // Step 8: NVM write
    bool writeSuccess = writeToNVM(did, newVal);

    if (!writeSuccess) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_GENERAL_PROGRAMMING_FAILURE;
        return;
    } else engineTemp = newVal;

    // Step 9: Positive response
    udsCtx.flow = UDS_FLOW_POS;
    udsCtx.msg.canID = 0x768;
    udsCtx.msg.dlc = 4;
    udsCtx.msg.data[0] = 0x03;
    udsCtx.msg.data[1] = 0x6E;   // POS SID (0x2E + 0x40)
    udsCtx.msg.data[2] = msg_rx.data[2];
    udsCtx.msg.data[3] = msg_rx.data[3];
}

/**
 * @brief Handle "Read Data By Identifier" (UDS SID = 0x22) requests.
 *
 * Parses one or more DIDs from the incoming CAN frame, validates them,
 * and constructs a positive response containing the DID(s) and their
 * corresponding values. If any validation fails, sets a negative response
 * code in udsCtx and returns immediately.
 *
 * @param msg_rx Received CAN message:
 *               - msg_rx.data[0] : Length byte (excluding itself)
 *               - msg_rx.data[1] : SID = 0x22 (ReadDataByIdentifier)
 *               - msg_rx.data[2+] : Sequence of DID_H, DID_L pairs
 *               - msg_rx.dlc     : Number of bytes in the frame
 *
 * Processing logic:
 * 1) Check data[0] matches DLC - 1.
 * 2) Ensure DLC ≥ 4 and that parameter bytes form complete DID pairs.
 * 3) Initialize response:
 *    - Set udsCtx.msg.canID = 0x768 (response CAN ID)
 *    - Set udsCtx.msg.data[1] = 0x62 (positive response SID = 0x22 + 0x40)
 * 4) For each DID in the request:
 *    - Check if DID is supported (DID_ENGINE_TEMP, DID_THRESHOLD, DID_ENGINE_LIGHT).
 *    - If adding 4 bytes (DID_H, DID_L, val_H, val_L) would exceed CAN payload (8 bytes), respond NEG: NRC_RESPONSE_TOO_LONG.
 *    - Check security access via isSecurityAccessGranted().
 *    - Check environmental/operational conditions via isConditionOk().
 *    - Read the DID value:
 *         • DID_ENGINE_TEMP → myADC_Read(13)
 *         • DID_ENGINE_LIGHT → myADC_Read(12)
 *         • DID_THRESHOLD → use engineTemp variable
 *    - Append DID_H, DID_L, val_H, val_L to udsCtx.msg.data[]
 *    - Increment validCount
 * 5) If no valid DID found (validCount == 0), respond NEG: NRC_REQUEST_OUT_OF_RANGE.
 * 6) On success, set udsCtx.flow = UDS_FLOW_POS, udsCtx.msg.dlc = responseIdx,
 *    and udsCtx.msg.data[0] = response length (excluding length byte itself).
 *
 * Context:
 * - Uses global udsCtx to store response data and flow state.
 * - Relies on global constants for DID IDs and NRC codes.
 * - This function does not send the response directly; UDS_SendResponse() handles transmission.
 */
void handleReadDataByIdentifier(const CAN_Message_t msg_rx) {
    // Step 1: Check that length byte matches DLC - 1
    if (msg_rx.data[0] != (msg_rx.dlc - 1)) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    // Step 2: Check minimum length (≥ 4) and even parameter count (DID pairs)
    if (msg_rx.dlc < 4 || ((msg_rx.dlc) % 2) != 0) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_INCORRECT_LENGTH;
        return;
    }

    uint8_t validCount = 0;       // Number of valid DIDs processed
    uint8_t responseIdx = 2;      // Start index for DID/value pairs in response

    // Step 3: Initialize positive response frame header
    udsCtx.msg.canID = 0x768;     // Response CAN ID
    udsCtx.msg.data[1] = 0x62;    // Positive SID (0x22 + 0x40)

    // Step 4: Iterate over requested DIDs
    for (uint8_t i = 2; i < msg_rx.dlc; i += 2) {
        uint16_t did = (msg_rx.data[i] << 8) | msg_rx.data[i + 1];

        // Check if DID is supported
        if (did == DID_ENGINE_TEMP || did == DID_THRESHOLD || did == DID_ENGINE_LIGHT) {
            // Check if adding this DID would exceed CAN payload limit
            if (responseIdx + 4 > 8) {
                udsCtx.flow = UDS_FLOW_NEG;
                udsCtx.nrc = NRC_RESPONSE_TOO_LONG;
                return;
            }

            // Security access check
            if (!isSecurityAccessGranted(did)) {
                udsCtx.flow = UDS_FLOW_NEG;
                udsCtx.nrc = NRC_SECURITY_ACCESS_DENIED;
                return;
            }

            // Condition check
            if (!isConditionOk(did)) {
                udsCtx.flow = UDS_FLOW_NEG;
                udsCtx.nrc = NRC_CONDITIONS_NOT_CORRECT;
                return;
            }

            // Read DID value based on DID type
            uint16_t value;
            if (did == DID_ENGINE_TEMP) {
                value = myADC_Read(13);
            } else if (did == DID_ENGINE_LIGHT) {
                value = myADC_Read(12);
            } else {
                value = engineTemp;
            }

            // Append DID and value to response
            udsCtx.msg.data[responseIdx++] = msg_rx.data[i];     // DID high byte
            udsCtx.msg.data[responseIdx++] = msg_rx.data[i + 1]; // DID low byte
            udsCtx.msg.data[responseIdx++] = value >> 8;         // Value high byte
            udsCtx.msg.data[responseIdx++] = value & 0xFF;       // Value low byte

            validCount++;
        }
    }

    // Step 5: No valid DID found → NEG response
    if (validCount == 0) {
        udsCtx.flow = UDS_FLOW_NEG;
        udsCtx.nrc = NRC_REQUEST_OUT_OF_RANGE;
        return;
    }

    // Step 6: Prepare positive response
    udsCtx.flow = UDS_FLOW_POS;
    udsCtx.msg.dlc = responseIdx;
    udsCtx.msg.data[0] = responseIdx - 1; // Length excluding length byte itself
}





/**
 * @brief Checks if ECU reset conditions are met.
 *
 * This would check hardware or software states
 * to decide if an ECU reset is safe to perform at the moment.
 *
 * @return true if conditions are OK, false otherwise.
 */
bool isResetConditionOk(void) {
    // Example: Always OK in this demo
    return true;
}

/**
 * @brief Checks if the given DID has security access granted.
 *
 * This would verify that the security
 * access level is high enough for the requested DID.
 *
 * @param did The DID being accessed.
 * @return true if access is granted, false otherwise.
 */
bool isSecurityAccessGranted(uint16_t did) {
    // Example: Allow all in this demo
    return true;
}

/**
 * @brief Checks if the given DID can be accessed under current conditions.
 *
 * Could verify ignition state, ECU mode, or other environmental factors.
 *
 * @param did The DID being accessed.
 * @return true if conditions allow access, false otherwise.
 */
bool isConditionOk(uint16_t did) {
    // Example: Always OK in this demo
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
        case DID_ENGINE_TEMP:
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

/**
 * @brief Perform a system reset of the ECU via Cortex-M System Control Block.
 *
 * Writes the VECTKEY (0x5FA) and SYSRESETREQ bit into the SCB Application
 * Interrupt and Reset Control Register (AIRCR) to request a system-wide reset.
 * After triggering, the function enters an infinite loop until the reset
 * takes effect.
 *
 * @note This reset mechanism is defined by ARM Cortex-M architecture.
 *       It is equivalent to a hardware reset — all peripherals and CPU state
 *       are reinitialized as if power-cycled.
 *
 * Processing logic:
 * 1) Prepare the AIRCR write:
 *    - Bits [31:16] = VECTKEY = 0x5FA (required unlock key for write access).
 *    - Bit 2 (SYSRESETREQ) = 1 to request a system reset.
 * 2) Write value to SCB_AIRCR register at address 0xE000ED0C.
 * 3) Enter infinite loop to wait for the reset to occur.
 *
 */

#define SCB_AIRCR                  (*(volatile uint32_t*)0xE000ED0C)
#define SCB_AIRCR_VECTKEY_MASK    (0x5FA << 16)
#define SCB_AIRCR_SYSRESETREQ     (1 << 2)

void ECU_Reset(void) {
    // Step 1 & 2: Write unlock key + reset request
    SCB_AIRCR = SCB_AIRCR_VECTKEY_MASK | SCB_AIRCR_SYSRESETREQ;
    while (1) {
        // Step 3: Wait indefinitely for reset to happen
    }
}

