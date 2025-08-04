/*
 * @brief  Header file for the Unified Diagnostic Services (UDS) handler.
 * This module focuses on handling Service 0x19: ReadDTCInformation.
 */
#ifndef INC_UDS_H_
#define INC_UDS_H_

#include <stdint.h>
#include <stdbool.h>
#include "FlexCan.h"

// ===== UDS Service IDs =====
#define UDS_SERVICE_ECU_RESET                0x11
#define UDS_SERVICE_READ_DID                 0x22
#define UDS_SERVICE_WRITE_DID                0x2E
#define UDS_SERVICE_READ_DTC_INFORMATION     0x19

/* --- Service 0x19 Sub-functions --- */
#define SF_REPORT_NUMBER_OF_DTC_BY_STATUS_MASK          (0x01)
#define SF_REPORT_DTC_BY_STATUS_MASK                    (0x02)
#define SF_REPORT_DTC_SNAPSHOT_RECORD_BY_DTC_NUMBER     (0x04)
#define SF_REPORT_SUPPORTED_DTC                         (0x0A)

// ===== NRC (Negative Response Codes) =====
#define NRC_SERVICE_NOT_SUPPORTED        0x11
#define NRC_SUBFUNC_NOT_SUPPORTED        0x12
#define NRC_INCORRECT_LENGTH             0x13
#define NRC_CONDITIONS_NOT_CORRECT       0x22
#define NRC_SECURITY_ACCESS_DENIED       0x33
#define NRC_REQUEST_OUT_OF_RANGE         0x31
#define NRC_GENERAL_PROGRAMMING_FAILURE  0x72
#define NRC_RESPONSE_TOO_LONG            0x14

/* --- DTC Format Identifier (for response messages) --- */
#define DTC_FORMAT_ID_ISO14229_1 (0x01)

/* --- ISO 15765-2 (ISO-TP) Protocol Control Information (PCI) Types --- */
#define ISO_TP_PCI_TYPE_FIRST_FRAME       (0x10)
#define ISO_TP_PCI_TYPE_CONSECUTIVE_FRAME (0x20)

// ===== DIDs =====
#define DID_ENGINE_TEMP      0xF190
#define DID_ENGINE_LIGHT     0xF191
#define DID_THRESHOLD        0xF192

// ===== Security Levels =====
#define SECURITY_LEVEL_NONE     0
#define SECURITY_LEVEL_ENGINE   1


/**
 * @brief Main handler for UDS Service 0x19 (ReadDTCInformation).
 * This function acts as a dispatcher, routing the request to the correct
 * sub-function handler based on the request message.
 * @param requestMsg The incoming CAN message request from the FlexCAN driver.
 */
void handleReadDTCInformation(const CAN_Message_t *requestMsg);

/**
 * @brief Sends the final UDS response based on the global udsCtx.
 * This single function handles positive (single/multi-frame) and negative responses.
 * It takes no parameters, as all required information is in the context.
 */
void UDS_SendResponse(void);

void UDS_DispatchService(const CAN_Message_t msg_rx);

#endif /* INC_UDS_H_ */
