/*
 * @brief  Implementation of the UDS Service 0x19 handler and ISO-TP sender.
 */
#include "uds.h"
#include "dtc.h"
#include <string.h>
#include "FlexCan.h"

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
	msg.canID = TX_MSG_ID;
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

/**
 * @brief Sends a single-frame response using the custom format.
 * @param response_sid The service ID for the response (e.g., 0x59).
 * @param payload Pointer to the data payload.
 * @param payload_length Length of the data payload.
 */
static void sendUDSResponse(uint8_t response_sid, const uint8_t *payload, uint16_t payload_length) {
	CAN_Message_t msg_to_transmit;
	msg_to_transmit.canID = TX_MSG_ID;

	/* This is a custom, non-standard single-frame format. */
    /* Byte 0: Length of the following data (SID + payload). */
	uint8_t length_byte = payload_length + 1;
	msg_to_transmit.dlc = length_byte + 1;

	msg_to_transmit.data[0] = length_byte;
	msg_to_transmit.data[1] = response_sid;

	if (payload != NULL && payload_length > 0) {
		memcpy(&msg_to_transmit.data[2], payload, payload_length);
	}

	FLEXCAN0_transmit_msg(&msg_to_transmit);
}

/**
 * @brief Sends a Negative Response Code (NRC).
 * NRCs are always short and sent as a single frame.
 */
static void sendNRC(uint8_t sid, uint8_t nrc) {
	uint8_t nrc_payload[2];
	nrc_payload[0] = sid; /* Echo the original Service ID. */
	nrc_payload[1] = nrc; /* The Negative Response Code. */
	sendUDSResponse(0x7F, nrc_payload, sizeof(nrc_payload)); /* The SID for an NRC is always 0x7F. */
}

/**
 * @brief Constructs and sends a Positive Response.
 * This function is the main dispatcher for sending responses. It automatically
 * chooses between the custom single-frame format and the multi-frame ISO-TP
 * format based on the total message length.
 */
static void sendPositiveResponse(uint8_t sid, const uint8_t *payload, uint16_t length) {
    uint8_t response_sid = sid + 0x40; /* Positive Response SID is original SID + 0x40. */

    /* Calculate the total size the CAN frame would need for the custom single-frame format. */
    /* Size = 1 (length byte) + 1 (SID) + data length */
    uint16_t total_can_frame_size = 1 + 1 + length;

    if (total_can_frame_size <= 8) {
        /* The message is short enough for a single frame. Use the custom format. */
        sendUDSResponse(response_sid, payload, length);
    } else {
        /* The message is too long and must be sent using ISO-TP multi-frame. */
        static uint8_t full_uds_payload[4095]; /* A large static buffer for the full UDS message. */
        full_uds_payload[0] = response_sid;
        if (payload != NULL && length > 0) {
            memcpy(&full_uds_payload[1], payload, length);
        }
        UDS_SendMultiFrameISO_TP(full_uds_payload, length + 1);
    }
}

/* --- Sub-function Handlers --- */

/*
 * @brief Handles sub-function 0x01: reportNumberOfDTCByStatusMask.
 */
static void sf_reportNumberOfDTCByStatusMask(const CAN_Message_t *requestMsg) {
    /* Expected request format: [Length=3] [SID=19] [SubFunc=01] [StatusMask] */
    if (requestMsg->data[0] != 3) {
        sendNRC(SID_READ_DTC_INFORMATION, NRC_INCORRECT_MESSAGE_LENGTH_OR_FORMAT);
        return;
    }
    uint8_t requested_mask = requestMsg->data[3];
    uint16_t count = 0;
    /* Iterate through all DTC slots to find matches. */
    for (uint8_t i = 0; i < DTC_GetCount(); ++i) {
        DTC_Record_t record;
        /* A DTC matches if all bits in the requested mask are also set in the stored mask. */
        if (DTC_GetRecord(i, &record) && ((record.status_mask & requested_mask) == requested_mask)) {
            count++;
        }
    }
    /* Build and send the positive response. */
    uint8_t payload[4];
    payload[0] = SF_REPORT_NUMBER_OF_DTC_BY_STATUS_MASK;
    payload[1] = DTC_FORMAT_ID_ISO14229_1;
    payload[2] = (uint8_t)(count >> 8);   /* Count, High Byte */
    payload[3] = (uint8_t)(count & 0xFF); /* Count, Low Byte */
    sendPositiveResponse(SID_READ_DTC_INFORMATION, payload, sizeof(payload));
}


/*
 * @brief Handles sub-function 0x02: reportDTCByStatusMask.
 */
static void sf_reportDTCByStatusMask(const CAN_Message_t *requestMsg) {
    if (requestMsg->data[0] != 3) {
        sendNRC(SID_READ_DTC_INFORMATION, NRC_INCORRECT_MESSAGE_LENGTH_OR_FORMAT);
        return;
    }
    uint8_t requested_mask = requestMsg->data[3];
    /* Use a static payload buffer to avoid putting a large array on the stack. */
    static uint8_t payload[1 + (DTC_COUNT * 4)];
    uint16_t payload_len = 0;
    payload[payload_len++] = SF_REPORT_DTC_BY_STATUS_MASK;

    for (uint8_t i = 0; i < DTC_GetCount(); ++i) {
        DTC_Record_t record;
        if (DTC_GetRecord(i, &record) && ((record.status_mask & requested_mask) == requested_mask)) {
            /* For each matching DTC, append its 3-byte code and 1-byte status to the payload. */
            payload[payload_len++] = (uint8_t)(record.dtc_code >> 16);
            payload[payload_len++] = (uint8_t)(record.dtc_code >> 8);
            payload[payload_len++] = (uint8_t)(record.dtc_code);
            payload[payload_len++] = record.status_mask;
        }
    }
    /* Send the response. This may be a single or multi-frame message. */
    sendPositiveResponse(SID_READ_DTC_INFORMATION, payload, payload_len);
}


/*
 * @brief Handles sub-function 0x04: reportDTCSnapshotRecordByDTCNumber.
 */
static void sf_reportDTCSnapshotByDTCNumber(const CAN_Message_t *requestMsg) {
    /* Expected request format: [Length=6] [SID=19] [SubFunc=04] [DTC H] [DTC M] [DTC L] [RecNum] */
    if (requestMsg->data[0] != 6) {
        sendNRC(SID_READ_DTC_INFORMATION, NRC_INCORRECT_MESSAGE_LENGTH_OR_FORMAT);
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

            /* Send the 11-byte payload. This will be sent as a multi-frame ISO-TP message. */
            sendPositiveResponse(SID_READ_DTC_INFORMATION, payload, payload_len);
            return;
        }
    }
    sendNRC(SID_READ_DTC_INFORMATION, NRC_REQUEST_OUT_OF_RANGE);
}

/*
 * @brief Handles sub-function 0x0A: reportSupportedDTC.
 */
static void sf_reportSupportedDTC(const CAN_Message_t *requestMsg) {
    if (requestMsg->data[0] != 2) {
        sendNRC(SID_READ_DTC_INFORMATION, NRC_INCORRECT_MESSAGE_LENGTH_OR_FORMAT);
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
    sendPositiveResponse(SID_READ_DTC_INFORMATION, payload, payload_len);
}

/* --- Main Service Handler --- */

/*
 * @brief See uds.h for function documentation.
 */
void UDS_ReadDTCInformation(const CAN_Message_t *requestMsg) {
    /* Perform initial validation on the incoming request frame.
       The DLC must match the length specified in the first data byte. */
    if (requestMsg->dlc < 3 || requestMsg->dlc != (requestMsg->data[0] + 1)) {
        sendNRC(SID_READ_DTC_INFORMATION, NRC_INCORRECT_MESSAGE_LENGTH_OR_FORMAT);
        return;
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
        sendNRC(SID_READ_DTC_INFORMATION, NRC_SUB_FUNCTION_NOT_SUPPORTED);
        break;
    }
}
