#ifndef UDS_H_
#define UDS_H_


#include <stdint.h>
#include <stdbool.h>
#include "FlexCan.h"

// ===== UDS Service IDs =====
#define UDS_SERVICE_ECU_RESET         0x11
#define UDS_SERVICE_READ_DID         0x22
#define UDS_SERVICE_WRITE_DID        0x2E

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
#define DID_ENGINE_TEMP      0xF190
#define DID_ENGINE_LIGHT     0xF191
#define DID_THRESHOLD        0xF192

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
bool writeToNVM(uint16_t did, uint16_t value);
uint16_t ReadADCValue(void);
void ECU_Reset(void);

#endif
