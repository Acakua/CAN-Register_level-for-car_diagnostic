#include "uds.h"

uint8_t currentSecurityLevel = SECURITY_LEVEL_ENGINE;
uint16_t engineTemp = 0x1234;

CAN_Message_t msg_tx;


// === UDS Service Dispatcher ===
void UDS_DispatchService(const CAN_Message_t msg_rx)
{
    uint8_t sid = msg_rx.data[1];

    msg_tx.canID = 0x768;
    msg_tx.dlc   = 4;
    msg_tx.data[0] = 0x03;
    msg_tx.data[1] = 0x7F;
    msg_tx.data[2] = sid;
    msg_tx.data[3] = 0x00;

    if (sid == UDS_SERVICE_ECU_RESET) {
        handleECUReset(msg_rx);
    } else if (sid == UDS_SERVICE_WRITE_DID) {
        handleWriteDataByIdentifier(msg_rx);
    } else if (sid == UDS_SERVICE_READ_DID) {
        handleReadDataByIdentifier(msg_rx);
    } else {
        msg_tx.data[3] = NRC_SERVICE_NOT_SUPPORTED;
        FLEXCAN0_transmit_msg(&msg_tx);
    }
}

void handleECUReset(const CAN_Message_t msg_rx)
{
    if (msg_rx.dlc < 3) {
        msg_tx.data[3] = NRC_INCORRECT_LENGTH;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }

    uint8_t subFunc = msg_rx.data[2];

    if ((subFunc & 0x7F) != 0x01) {
        msg_tx.data[3] = NRC_SUBFUNC_NOT_SUPPORTED;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }

    if (!isResetConditionOk()) {
        msg_tx.data[3] = NRC_CONDITIONS_NOT_CORRECT;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }

    if (currentSecurityLevel < SECURITY_LEVEL_ENGINE) {
        msg_tx.data[3] = NRC_SECURITY_ACCESS_DENIED;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }

    if (!(subFunc & 0x80)) {
        CAN_Message_t rsp = {
            .canID = msg_tx.canID,
            .dlc = 3,
            .data[0] = 0x02,
            .data[1] = 0x51,
            .data[2] = subFunc
        };
        FLEXCAN0_transmit_msg(&rsp);
    }

    for (volatile int i = 0; i < 1000000; ++i);
    ECU_Reset();
}

void handleWriteDataByIdentifier(const CAN_Message_t msg_rx)
{
    if (msg_rx.dlc < 5) {  // 1 length, 1 SID, 2 DID, min 1 data
        msg_tx.data[3] = NRC_INCORRECT_LENGTH;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }

    uint16_t did = (msg_rx.data[2] << 8) | msg_rx.data[3];

    if (did != DID_THRESHOLD) {
        msg_tx.data[3] = NRC_REQUEST_OUT_OF_RANGE;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }

    if (msg_rx.dlc >= 7) {  // 1 length, 1 SID, 2 DID, max 2 data
        msg_tx.data[3] = NRC_INCORRECT_LENGTH;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }

    if (!isSecurityAccessGranted(did)) {
        msg_tx.data[3] = NRC_SECURITY_ACCESS_DENIED;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }

    if (!isConditionOk(did)) {
        msg_tx.data[3] = NRC_CONDITIONS_NOT_CORRECT;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }

    uint16_t newVal = (msg_rx.data[4] << 8) | msg_rx.data[5]; // Big-endian

    if (newVal >= 4096) {
        msg_tx.data[3] = NRC_REQUEST_OUT_OF_RANGE;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }

    engineTemp = newVal;
    bool writeSuccess = writeToNVM(did, newVal);

    if (!writeSuccess) {
        msg_tx.data[3] = NRC_GENERAL_PROGRAMMING_FAILURE;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }


    // Positive Response
    CAN_Message_t rsp = {
        .canID = msg_tx.canID,
        .dlc = 4,
        .data = {
            0x03, 0x6E,
            msg_rx.data[2], msg_rx.data[3]
        }
    };
    FLEXCAN0_transmit_msg(&rsp);
}




void handleReadDataByIdentifier(const CAN_Message_t msg_rx)
{
    if (msg_rx.dlc < 4 || ((msg_rx.dlc) % 2) != 0) {  // 1 for length, 1 for SID, 2 for DID
        msg_tx.data[3] = NRC_INCORRECT_LENGTH;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }


    /*if (msg_rx.dlc > 8 ) {  // 1 for length, 1 for SID, 2 for each DID
        msg_tx.data[3] = NRC_INCORRECT_LENGTH;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }*/

    uint8_t validCount = 0;
    uint8_t responseIdx = 2;

    msg_tx.data[1] = 0x62;

    for (uint8_t i = 2; i < msg_rx.dlc; i += 2) {
        uint16_t did = (msg_rx.data[i] << 8) | msg_rx.data[i + 1];

        if (did == DID_ENGINE_TEMP || did == DID_THRESHOLD) {
            if (responseIdx + 4 > 8) {
                msg_tx.data[3] = NRC_RESPONSE_TOO_LONG;
                FLEXCAN0_transmit_msg(&msg_tx);
                return;
            }


            if (!isSecurityAccessGranted(did)) {
                msg_tx.data[3] = NRC_SECURITY_ACCESS_DENIED;
                FLEXCAN0_transmit_msg(&msg_tx);
                return;
            }

            if (!isConditionOk(did)) {
                msg_tx.data[3] = NRC_CONDITIONS_NOT_CORRECT;
                FLEXCAN0_transmit_msg(&msg_tx);
                return;
            }

            uint16_t value = (did == DID_ENGINE_TEMP) ? ReadADCValue() : engineTemp;

            msg_tx.data[responseIdx++] = msg_rx.data[i];        // did high byte
            msg_tx.data[responseIdx++] = msg_rx.data[i + 1];    // did low byte
            msg_tx.data[responseIdx++] = value >> 8;            // value high byte
            msg_tx.data[responseIdx++] = value & 0xFF;          // value low byte

            validCount++;
        }
    }

    if (validCount == 0) {
        msg_tx.data[3] = NRC_REQUEST_OUT_OF_RANGE;
        FLEXCAN0_transmit_msg(&msg_tx);
        return;
    }

    msg_tx.dlc = responseIdx;
    msg_tx.data[0] = responseIdx - 1;
    FLEXCAN0_transmit_msg(&msg_tx);
}



bool isResetConditionOk() {
    return true;
}


#define SCB_AIRCR                  (*(volatile uint32_t*)0xE000ED0C)
#define SCB_AIRCR_VECTKEY_MASK    (0x5FA << 16)
#define SCB_AIRCR_SYSRESETREQ     (1 << 2)

void ECU_Reset(void)
{
    SCB_AIRCR = SCB_AIRCR_VECTKEY_MASK | SCB_AIRCR_SYSRESETREQ;
    while (1) {}
}


bool isSecurityAccessGranted(uint16_t did) {
    return true;
}

bool isConditionOk(uint16_t did) {
    return true;
}


bool writeToNVM(uint16_t did, uint16_t value)
{
    if (did == DID_THRESHOLD) {
        engineTemp = value;
        return true;
    }
    return false;
}
