/*
 * @brief  Header file for the Non-Volatile Memory (NVM) abstraction layer.
 * This module provides a simple interface for reading from and writing to
 * the S32K144's emulated EEPROM over FlexNVM.
 */
#ifndef INC_NVM_H_
#define INC_NVM_H_

#include <stdint.h>
#include <stddef.h>
#include "flash_driver.h" /* Include S32K SDK header for Flash/EEPROM operations */

/* --- Configuration --- */
#define NVM_START_ADDRESS   (0x14000000U) /* Standard start address for FlexNVM on S32K144 */
#define NVM_SIZE            (4096U)       /* EEPROM size = 4 Kbytes, as per your spec */
#define DID_COUNT           (2U)          /* Number of Data Identifiers supported */
#define DTC_COUNT           (5U)          /* Number of Diagnostic Trouble Codes that can be stored */
#define DID_MAX_SIZE        (8U)          /* Max size in bytes for a single DID record */
#define DTC_SLOT_SIZE       (32U)         /* Reserved size in bytes for a single DTC record in NVM */

/* --- NVM Memory Layout --- */
/* Defines the memory map for different data regions within the NVM. */
#define DID_REGION_OFFSET   (0U)
#define DID_REGION_SIZE     (DID_COUNT * DID_MAX_SIZE)

#define DTC_REGION_OFFSET   (DID_REGION_OFFSET + DID_REGION_SIZE)
#define DTC_REGION_SIZE     (DTC_COUNT * DTC_SLOT_SIZE)

/* Defines the storage offset for each DID within the NVM's DID region. */
/* Each slot is spaced by DID_MAX_SIZE to prevent data collision. */
#define DID_ENGINE_TEMP_NVM_OFFSET    (DID_REGION_OFFSET + (0 * DID_MAX_SIZE))
#define DID_ENGINE_LIGHT_NVM_OFFSET   (DID_REGION_OFFSET + (1 * DID_MAX_SIZE))
#define DID_THRESHOLD_NVM_OFFSET      (DID_REGION_OFFSET + (2 * DID_MAX_SIZE))


/**
 * @brief Represents the status of NVM operations.
 */
typedef enum
{
    NVM_OK = 0,
    NVM_ERROR = -1,
    NVM_INVALID_PARAM = -2
} NVM_Status;

/**
 * @brief Reads data from the S32K144's emulated EEPROM.
 * @param offset The starting offset from NVM_START_ADDRESS.
 * @param data Pointer to the buffer to store read data.
 * @param len The number of bytes to read.
 * @return NVM_Status indicating the result of the operation.
 */
NVM_Status NVM_Read(uint32_t offset, uint8_t *data, uint8_t len);

/**
 * @brief Writes data to the S32K144's emulated EEPROM using the flash driver.
 * @param offset The starting offset from NVM_START_ADDRESS.
 * @param data Pointer to the data to be written.
 * @param len The number of bytes to write.
 * @return NVM_Status indicating the result of the operation.
 */
NVM_Status NVM_Write(uint32_t offset, const uint8_t *data, uint8_t len);

/**
 * @brief "Erases" a region of the emulated EEPROM by writing 0xFF.
 * @param offset The starting offset from NVM_START_ADDRESS.
 * @param len The number of bytes to erase.
 * @return NVM_Status indicating the result of the operation.
 */
NVM_Status NVM_Erase(uint32_t offset, uint32_t len);

#endif /* INC_NVM_H_ */
