#ifndef UDS_H_
#define UDS_H_


#include <stdint.h>
#include <stdbool.h>
#include "FlexCan.h"
#include "dtc.h"

// ===== UDS Service IDs =====
#define UDS_SERVICE_ECU_RESET                0x11
#define UDS_SERVICE_READ_DID                 0x22
#define UDS_SERVICE_WRITE_DID                0x2E


// ===== NRC (Negative Response Codes) =====
#define NRC_SERVICE_NOT_SUPPORTED        0x11
#define NRC_SUBFUNC_NOT_SUPPORTED        0x12
#define NRC_INCORRECT_LENGTH             0x13
#define NRC_CONDITIONS_NOT_CORRECT       0x22
#define NRC_SECURITY_ACCESS_DENIED       0x33
#define NRC_REQUEST_OUT_OF_RANGE         0x31
#define NRC_GENERAL_PROGRAMMING_FAILURE  0x72
#define NRC_RESPONSE_TOO_LONG            0x14

// ===== DIDs =====
#define DID_ENGINE_TEMP             0xF190
#define DID_ENGINE_LIGHT            0xF191
#define DID_VEHICLE_ID              0x2015
#define DID_TEMP_THRESHOLD_LOW      0xF192
#define DID_TEMP_THRESHOLD_MEDIUM   0xF193
#define DID_TEMP_THRESHOLD_HIGH     0xF194

/* --- Service 0x19 Sub-functions --- */
#define SF_REPORT_NUMBER_OF_DTC_BY_STATUS_MASK          (0x01)
#define SF_REPORT_DTC_BY_STATUS_MASK                    (0x02)
#define SF_REPORT_DTC_SNAPSHOT_RECORD_BY_DTC_NUMBER     (0x04)
#define SF_REPORT_SUPPORTED_DTC                         (0x0A)

/* --- DTC Format Identifier (for response messages) --- */
#define DTC_FORMAT_ID_ISO14229_1 (0x01)

/* --- ISO 15765-2 (ISO-TP) Protocol Control Information (PCI) Types --- */
#define ISO_TP_PCI_TYPE_FIRST_FRAME       (0x10)
#define ISO_TP_PCI_TYPE_CONSECUTIVE_FRAME (0x20)


// ===== Security Levels =====
#define SECURITY_LEVEL_NONE     0
#define SECURITY_LEVEL_ENGINE   1


// ===== Global Variables =====
extern uint8_t currentSecurityLevel;
extern uint16_t engineTemp;

// ===== Function Prototypes =====
void UDS_DispatchService(const CAN_Message_t msg_rx);

// Service handlers (optional to expose)
void handleECUReset(const CAN_Message_t msg_rx);
void handleReadDataByIdentifier(const CAN_Message_t msg_rx);
void handleWriteDataByIdentifier(const CAN_Message_t msg_rx);
void UDS_SendResponse(void);
void UDS_SendMultiFrameISO_TP(const uint8_t *data, uint16_t length);



// External dependencies (must be implemented elsewhere)
bool isResetConditionOk(void);
bool isSecurityAccessGranted(uint16_t did);
bool isConditionOk(uint16_t did);
void ECU_Reset(void);

#endif
