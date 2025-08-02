#include "uds.h"
#include "adc.h"

// ==== Global Variables ====
uint8_t currentSecurityLevel = SECURITY_LEVEL_ENGINE;
uint16_t engineTemp = 0x1234;

typedef enum {
	UDS_FLOW_NONE = 0, UDS_FLOW_POS, UDS_FLOW_NEG
} UDS_FlowType;

typedef struct {
	UDS_FlowType flow;   // POS / NEG / NONE
	uint8_t sid;         // Service ID
	uint8_t nrc;         // Negative Response Code if flow = NEG
	CAN_Message_t msg;   // Buffer cho response
} UDS_Context;

static UDS_Context udsCtx;

void UDS_DispatchService(const CAN_Message_t msg_rx) {
	uint8_t sid = msg_rx.data[1];

	// reset context
	udsCtx.flow = UDS_FLOW_NONE;
	udsCtx.sid = sid;
	udsCtx.nrc = 0;

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

	UDS_SendResponse();
}

void UDS_SendResponse(void) {
	CAN_Message_t rsp;
	rsp.canID = 0x768;

	if (udsCtx.flow == UDS_FLOW_POS) {
		rsp = udsCtx.msg;
	} else if (udsCtx.flow == UDS_FLOW_NEG) {
		rsp.dlc = 4;
		rsp.data[0] = 0x03;
		rsp.data[1] = 0x7F;
		rsp.data[2] = udsCtx.sid;
		rsp.data[3] = udsCtx.nrc;
	} else {
		return; // NONE -> no responses
	}

	FLEXCAN0_transmit_msg(&rsp);

	if (udsCtx.flow == UDS_FLOW_POS && udsCtx.sid == UDS_SERVICE_ECU_RESET){
		ECU_Reset();
	}
}

void handleECUReset(const CAN_Message_t msg_rx) {
	if (msg_rx.dlc < 3) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_INCORRECT_LENGTH;
		return;
	}

	uint8_t subFunc = msg_rx.data[2];

	if ((subFunc & 0x7F) != 0x01) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_SUBFUNC_NOT_SUPPORTED;
		return;
	}

	if (!isResetConditionOk()) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_CONDITIONS_NOT_CORRECT;
		return;
	}

	if (currentSecurityLevel < SECURITY_LEVEL_ENGINE) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_SECURITY_ACCESS_DENIED;
		return;
	}

	// Positive response
	if (!(subFunc & 0x80)) {
	    udsCtx.flow = UDS_FLOW_POS;
	    udsCtx.msg.canID = 0x768;
	    udsCtx.msg.dlc   = 3;
	    udsCtx.msg.data[0] = 0x02;
	    udsCtx.msg.data[1] = 0x51;
	    udsCtx.msg.data[2] = subFunc;
	} else {
	    udsCtx.flow = UDS_FLOW_NONE;
	}

}

void handleWriteDataByIdentifier(const CAN_Message_t msg_rx) {
	if (msg_rx.dlc < 5) {    // 1 length, 1 sid, 2 did, min 1 data
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_INCORRECT_LENGTH;
		return;
	}

	uint16_t did = (msg_rx.data[2] << 8) | msg_rx.data[3];

	if (did != DID_THRESHOLD) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_REQUEST_OUT_OF_RANGE;
		return;
	}
	if (msg_rx.dlc >= 7) {    // // 1 length, 1 sid, 2 did, max 2 data
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_INCORRECT_LENGTH;
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

	uint16_t newVal = (msg_rx.data[4] << 8) | msg_rx.data[5];

	if (newVal >= 4096) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_REQUEST_OUT_OF_RANGE;
		return;
	}

	bool writeSuccess = writeToNVM(did, newVal);
	if (!writeSuccess) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_GENERAL_PROGRAMMING_FAILURE;
		return;
	}

	// Positive response
	udsCtx.flow = UDS_FLOW_POS;
	udsCtx.msg.canID = 0x768;
	udsCtx.msg.dlc = 4;
	udsCtx.msg.data[0] = 0x03;
	udsCtx.msg.data[1] = 0x6E;
	udsCtx.msg.data[2] = msg_rx.data[2];
	udsCtx.msg.data[3] = msg_rx.data[3];
}

// ==== Read Data By Identifier Handler ====
void handleReadDataByIdentifier(const CAN_Message_t msg_rx) {
	if (msg_rx.dlc < 4 || ((msg_rx.dlc) % 2) != 0) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_INCORRECT_LENGTH;
		return;
	}

	uint8_t validCount = 0;
	uint8_t responseIdx = 2;

	udsCtx.msg.canID = 0x768;
	udsCtx.msg.data[1] = 0x62;

	for (uint8_t i = 2; i < msg_rx.dlc; i += 2) {
		uint16_t did = (msg_rx.data[i] << 8) | msg_rx.data[i + 1];

		if (did == DID_ENGINE_TEMP || did == DID_THRESHOLD
				|| did == DID_ENGINE_LIGHT) {
			if (responseIdx + 4 > 8) {
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

			uint16_t value;
			if (did == DID_ENGINE_TEMP) {
				value = myADC_Read(13);
			} else if (did == DID_ENGINE_LIGHT) {
				value = myADC_Read(12);
			} else {
				value = engineTemp;
			}

			udsCtx.msg.data[responseIdx++] = msg_rx.data[i];
			udsCtx.msg.data[responseIdx++] = msg_rx.data[i + 1];
			udsCtx.msg.data[responseIdx++] = value >> 8;
			udsCtx.msg.data[responseIdx++] = value & 0xFF;

			validCount++;
		}
	}

	if (validCount == 0) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_REQUEST_OUT_OF_RANGE;
		return;
	}

	// Positive response
	udsCtx.flow = UDS_FLOW_POS;
	udsCtx.msg.dlc = responseIdx;
	udsCtx.msg.data[0] = responseIdx - 1;
}

bool isResetConditionOk() {
	return true;
}

bool isSecurityAccessGranted(uint16_t did) {
	return true;
}

bool isConditionOk(uint16_t did) {
	return true;
}

bool writeToNVM(uint16_t did, uint16_t value) {
	if (did == DID_THRESHOLD) {
		engineTemp = value;
		return true;
	}
	return false;
}

// ==== ECU Reset ====
#define SCB_AIRCR                  (*(volatile uint32_t*)0xE000ED0C)
#define SCB_AIRCR_VECTKEY_MASK    (0x5FA << 16)
#define SCB_AIRCR_SYSRESETREQ     (1 << 2)

void ECU_Reset(void) {
	SCB_AIRCR = SCB_AIRCR_VECTKEY_MASK | SCB_AIRCR_SYSRESETREQ;
	while (1) {
	}
}