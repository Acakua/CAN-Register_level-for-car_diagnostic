/*
 * @brief  Implementation of the UDS Service 0x19 handler and ISO-TP sender.
 */
#include "uds.h"
#include "dtc.h"
#include <string.h>
#include "FlexCan.h"

/**
 * @brief Defines the type of response flow for the current UDS transaction.
 */
typedef enum {
    UDS_FLOW_NONE = 0, /* No response should be sent */
    UDS_FLOW_POS,      /* A positive response should be sent */
    UDS_FLOW_NEG       /* A negative response should be sent */
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

/* --- START OF ISO-TP AND RESPONSE SENDING LOGIC --- */

/**
 * @brief Simple blocking delay function.
 * @note This is a crude delay. For production, a hardware timer is recommended.
 * @param ms Milliseconds to delay.
 */
static void delay_ms(volatile uint32_t ms) {
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 5000; j++) { /* This loop count should be calibrated to your MCU's clock speed. */
            __asm__("nop");
        }
    }
}

/**
 * @brief Sends a multi-frame response using a simplified ISO-TP implementation.
 * @param data Pointer to the full UDS payload to be sent.
 * @param length The total length of the UDS payload.
 */
static void UDS_SendMultiFrameISO_TP(const uint8_t *data, uint16_t length) {
	CAN_Message_t msg;
	msg.canID = TX_MSG_ID_UDS;
	msg.dlc = 8; /* Both First Frames and Consecutive Frames always use 8-byte DLC. */

	uint16_t bytes_sent = 0;
	uint8_t sequence_number = 1;

	/* --- Step 1: Send the First Frame (FF) --- */
    /* The FF contains control information and the first 6 bytes of data. */
	msg.data[0] = ISO_TP_PCI_TYPE_FIRST_FRAME | (uint8_t) (length >> 8); /* PCI: Type + Upper 4 bits of length */
	msg.data[1] = (uint8_t) (length & 0xFF);                           /* Lower 8 bits of length */
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

/*
 * @brief Handles sub-function 0x01: reportNumberOfDTCByStatusMask.
 */
static void sf_reportNumberOfDTCByStatusMask(const CAN_Message_t *requestMsg) {
    /* Expected request format: [Length=3] [SID=19] [SubFunc=01] [StatusMask] */
	if (requestMsg->data[0] != 3) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_INCORRECT_LENGTH;
		return;
	}
	static uint8_t payload[4];
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
	payload[1] = DTC_FORMAT_ID_ISO14229_1;
	payload[2] = (uint8_t) (count >> 8);
	payload[3] = (uint8_t) (count & 0xFF);

	udsCtx.flow = UDS_FLOW_POS;
	udsCtx.payload = payload;
	udsCtx.payload_len = sizeof(payload);
}


/*
 * @brief Handles sub-function 0x02: reportDTCByStatusMask.
 */
static void sf_reportDTCByStatusMask(const CAN_Message_t *requestMsg) {
	if (requestMsg->data[0] != 3) {
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_INCORRECT_LENGTH;
		return;
	}
    /* Use a static payload buffer to avoid putting a large array on the stack. */
    static uint8_t payload[1 + (DTC_COUNT * 4)];
    uint16_t payload_len = 0;
    uint8_t requested_mask = requestMsg->data[3];
    payload[payload_len++] = SF_REPORT_DTC_BY_STATUS_MASK;

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


/*
 * @brief Handles sub-function 0x04: reportDTCSnapshotRecordByDTCNumber.
 */
static void sf_reportDTCSnapshotByDTCNumber(const CAN_Message_t *requestMsg) {
    /* Expected request format: [Length=6] [SID=19] [SubFunc=04] [DTC H] [DTC M] [DTC L] [RecNum] */
	if (requestMsg->data[0] != 6) {
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

/*
 * @brief Handles sub-function 0x0A: reportSupportedDTC.
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

/*
 * @brief See uds.h for function documentation.
 */
void handleReadDTCInformation(const CAN_Message_t *requestMsg) {
	/* Reset context, but crucially, set the SID for this transaction */
	udsCtx.flow = UDS_FLOW_NONE;
	udsCtx.sid = UDS_SERVICE_READ_DTC_INFORMATION; // Store SID in context
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
	default:
		udsCtx.flow = UDS_FLOW_NEG;
		udsCtx.nrc = NRC_SERVICE_NOT_SUPPORTED;
		break;
	}
	UDS_SendResponse();
}

/*
 * @brief See uds.h for function documentation.
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
        uint16_t total_uds_length = 1 + udsCtx.payload_len; /* RspSID + payload */

        if (total_uds_length <= 7) {
            /* Fits in a single frame */
            CAN_Message_t msg;
            msg.canID   = TX_MSG_ID_UDS;
            msg.dlc     = 1 + total_uds_length;
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
