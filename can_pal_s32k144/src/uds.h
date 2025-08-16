#ifndef UDS_H_
#define UDS_H_

#include <stdint.h>
#include <stdbool.h>
#include "FlexCan.h"
#include "dtc.h"

// ===== UDS Service IDs =====
#define UDS_SERVICE_ECU_RESET         0x11
#define UDS_SERVICE_READ_DID         0x22
#define UDS_SERVICE_WRITE_DID        0x2E
#define UDS_SERVICE_CLEAR_DTC        0x14   // <== NEW: Service 0x14

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

// ===== Security Levels =====
#define SECURITY_LEVEL_NONE     0
#define SECURITY_LEVEL_ENGINE   1

// ===== Global Variables =====
extern uint8_t currentSecurityLevel;
extern uint16_t engineTemp;

// ===== Function Prototypes =====
void UDS_DispatchService(const CAN_Message_t msg_rx);
void UDS_SendResponse(void);

// Service handlers
void handleECUReset(const CAN_Message_t msg_rx);
void handleReadDataByIdentifier(const CAN_Message_t msg_rx);
void handleWriteDataByIdentifier(const CAN_Message_t msg_rx);
void handleClearDiagnosticInformation(const CAN_Message_t msg_rx); // <== NEW

// External dependencies
bool isResetConditionOk(void);
bool isSecurityAccessGranted(uint16_t did);
bool isConditionOk(uint16_t did);
bool writeToNVM(uint16_t did, uint16_t value);
bool clearDTCFromNVM(uint32_t groupOfDTC)
bool isGroupOfDTCSupported(uint32_t groupOfDTC);
bool isConditionOkForClear(void);
bool clearDTCFromNVM(uint32_t groupOfDTC);
uint16_t ReadADCValue(void);
void ECU_Reset(void);

// Internal DTC operations (optional expose)
bool clearDTCFromNVM(uint32_t dtc);  // <== NEW helper for service 0x14
void handleClearDiagnosticInformation(const CAN_Message_t msg_rx);

#endif /* UDS_H_ */
