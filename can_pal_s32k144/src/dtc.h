#ifndef DTC_H_
#define DTC_H_

#include <stdint.h>
#include <stdbool.h>
#include "nvm.h"

/* --- DTC Definitions --- */

/* Defines a custom DTC code for a specific fault condition. */
#define DTC_ENGINE_OVERHEAT (0x905010U) /* Custom DTC for Engine Temperature Too High */

/* DTC Status Bit Masks as defined by the ISO 14229-1 standard. */
#define DTC_STATUS_TEST_FAILED                              (0x01)
#define DTC_STATUS_TEST_FAILED_THIS_OPERATION_CYCLE         (0x02)
#define DTC_STATUS_PENDING_DTC                              (0x04)
#define DTC_STATUS_CONFIRMED_DTC                            (0x08)
#define DTC_STATUS_TEST_NOT_COMPLETED_SINCE_LAST_CLEAR      (0x10)
#define DTC_STATUS_TEST_FAILED_SINCE_LAST_CLEAR             (0x20)
#define DTC_STATUS_TEST_NOT_COMPLETED_THIS_OPERATION_CYCLE  (0x40)
#define DTC_STATUS_WARNING_INDICATOR_REQUESTED              (0x80)

/**
 * @brief Structure for the DTC snapshot data.
 * The "packed" attribute is crucial to prevent the compiler from adding
 * padding bytes, ensuring the struct's size in memory matches the sum of its members.
 * This is essential for correct NVM storage and retrieval.
 */
typedef struct __attribute__((packed))
{
    uint8_t  temperature; /* Temperature in Celsius at the time of fault */
    uint8_t  day;         /* Day of the month (1-31) */
    uint8_t  month;       /* Month of the year (1-12) */
    uint16_t year;        /* Year (e.g., 2025) */
} DTC_Snapshot_t;

/**
 * @brief Structure for a full DTC record as it is stored in NVM.
 * Also packed to ensure a predictable memory layout.
 */
typedef struct __attribute__((packed))
{
    uint32_t       dtc_code;      /* The 3-byte DTC code, stored in a 4-byte integer. */
    uint8_t        status_mask;   /* The 1-byte status mask, composed of the bits above. */
    DTC_Snapshot_t snapshot;      /* The snapshot data captured when the fault occurred. */
} DTC_Record_t;


/**
 * @brief Sets a new DTC or updates an existing one in NVM.
 * @param dtc_code The DTC to set.
 * @param status The new status mask for the DTC.
 * @param snapshot Pointer to the snapshot data to store with the DTC.
 * @return true if the operation was successful, false otherwise.
 */
bool DTC_Set(uint32_t dtc_code, uint8_t status, const DTC_Snapshot_t* snapshot);

/**
 * @brief Retrieves a full DTC record from NVM by its storage index.
 * @param index The slot index in NVM (from 0 to DTC_COUNT-1).
 * @param record Pointer to a DTC_Record_t struct where the data will be stored.
 * @return true if a valid DTC was read, false if the slot is empty or an error occurred.
 */
bool DTC_GetRecord(uint8_t index, DTC_Record_t* record);

/**
 * @brief Gets the maximum number of DTCs that can be stored.
 * @return The total number of available DTC slots.
 */
uint8_t DTC_GetCount(void);

/**
 * @brief Finds the NVM storage index of a given DTC.
 * @param dtc_code The DTC code to search for. Can also be 0xFFFFFFFF to find an empty slot.
 * @return The index (0 to DTC_COUNT-1) if found, or -1 if not found.
 */
int8_t DTC_Find(uint32_t dtc_code);

#endif /* DTC_H_ */
