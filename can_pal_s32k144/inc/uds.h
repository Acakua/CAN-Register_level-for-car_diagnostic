/*
 * @brief  Header file for the Unified Diagnostic Services (UDS) handler.
 * This module focuses on handling Service 0x19: ReadDTCInformation.
 */
#ifndef INC_UDS_H_
#define INC_UDS_H_

#include <stdint.h>
#include "FlexCan.h"

/* --- UDS Service IDs --- */
#define SID_READ_DTC_INFORMATION 0x19

/* --- Service 0x19 Sub-functions --- */
#define SF_REPORT_NUMBER_OF_DTC_BY_STATUS_MASK          (0x01)
#define SF_REPORT_DTC_BY_STATUS_MASK                    (0x02)
#define SF_REPORT_DTC_SNAPSHOT_RECORD_BY_DTC_NUMBER     (0x04)
#define SF_REPORT_SUPPORTED_DTC                         (0x0A)

/* --- UDS Negative Response Codes (NRCs) --- */
#define NRC_SUB_FUNCTION_NOT_SUPPORTED           (0x12)
#define NRC_INCORRECT_MESSAGE_LENGTH_OR_FORMAT   (0x13)
#define NRC_REQUEST_OUT_OF_RANGE                 (0x31)

/* --- DTC Format Identifier (for response messages) --- */
#define DTC_FORMAT_ID_ISO14229_1 (0x01)

/* --- ISO 15765-2 (ISO-TP) Protocol Control Information (PCI) Types --- */
#define ISO_TP_PCI_TYPE_FIRST_FRAME       (0x10)
#define ISO_TP_PCI_TYPE_CONSECUTIVE_FRAME (0x20)

/**
 * @brief Main handler for UDS Service 0x19 (ReadDTCInformation).
 * This function acts as a dispatcher, routing the request to the correct
 * sub-function handler based on the request message.
 * @param requestMsg The incoming CAN message request from the FlexCAN driver.
 */
void UDS_ReadDTCInformation(const CAN_Message_t *requestMsg);

#endif /* INC_UDS_H_ */
