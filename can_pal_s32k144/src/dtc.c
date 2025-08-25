#include "dtc.h"
#include "nvm.h"
#include <string.h> /* For memset and memcpy */

/*
 * A static variable to keep track of the next slot to overwrite when the DTC memory is full.
 * This implements a simple FIFO (First-In, First-Out) or circular buffer strategy.
 * 'static' ensures it retains its value between function calls and is private to this file.
 */
static uint8_t next_dtc_overwrite_index = 0;

/**
 * @brief Get the total number of DTC slots available.
 *
 * @return Number of slots available in NVM for DTC records.
 */
uint8_t DTC_GetCount(void) {
	return DTC_COUNT;
}

/**
 * @brief Find the index of a DTC code in storage (3-byte UDS DTC match).
 *
 * Compares only the lower 24 bits (UDS 3-byte code). Reads each slot's
 * first 4 bytes and reconstructs a 32-bit value to avoid unaligned access.
 *
 * @param dtc_code 32-bit value; only bits 23:0 are used for comparison.
 * @return Index [0..DTC_COUNT-1] if found, else -1.
 * 
 * Processing logic:
 * 1) Define mask 0x00FFFFFF to compare only the 3 UDS bytes.
 * 2) For i = 0..DTC_COUNT-1:
 *    2.1) Compute slot offset = DTC_REGION_OFFSET + i * DTC_SLOT_SIZE.
 *    2.2) NVM_Read(offset, 4 bytes → temp_buffer); if fail, continue next slot.
 *    2.3) Rebuild stored_dtc_code from temp_buffer[0..3] (avoid unaligned access).
 *    2.4) If (stored_dtc_code & mask) == (dtc_code & mask) → return i.
 * 3) If none matched, return -1 (not found).
 */
int8_t DTC_Find(uint32_t dtc_code) {
    /* UDS DTCs are 3 bytes long. This mask is used to ignore the most significant byte. */
    const uint32_t DTC_MASK = 0x00FFFFFF;

    for (uint8_t i = 0; i < DTC_COUNT; ++i) {
        uint8_t temp_buffer[4]; /* A safe buffer to read the DTC code into. */
        uint32_t stored_dtc_code;
        uint32_t offset = DTC_REGION_OFFSET + (i * DTC_SLOT_SIZE);

        /* Read only the first 4 bytes of the slot, which contain the DTC code. */
        if (NVM_Read(offset, temp_buffer, 4) == NVM_OK) {
            /* Reconstruct the 32-bit integer from the buffer. This avoids potential
               unaligned memory access faults that can occur when casting pointers. */
            stored_dtc_code = ((uint32_t)temp_buffer[3] << 24) |
                              ((uint32_t)temp_buffer[2] << 16) |
                              ((uint32_t)temp_buffer[1] << 8)  |
                              ((uint32_t)temp_buffer[0]);

            /* Compare only the 3 relevant bytes of the DTC code. */
            if ((stored_dtc_code & DTC_MASK) == (dtc_code & DTC_MASK)) {
                return i; /* Found it, return the index. */
            }
        }
    }
    return -1; /* Not found. */
}

/**
 * @brief Set or update a DTC record with status and optional snapshot.
 *
 * If the DTC already exists → update. Else use first empty (erased=0xFFFFFFFF)
 * slot. If full, overwrite the oldest slot in a circular manner.
 *
 * @param dtc_code  UDS DTC (lower 24 bits significant).
 * @param status    Status byte to store (UDS status mask).
 * @param snapshot  Optional pointer to snapshot data; if NULL, snapshot is zeroed.
 * @return true on successful write to NVM, false otherwise.
 * 
 *  * Processing logic:
 * 1) Build DTC_Record_t new_record:
 *    - new_record.dtc_code   = dtc_code
 *    - new_record.status_mask= status
 *    - new_record.snapshot   = *snapshot if not NULL, else zeroed.
 * 2) Locate target index:
 *    2.1) Try DTC_Find(dtc_code) → update in-place if found.
 *    2.2) If not found, try DTC_Find(0xFFFFFFFF) → first erased/empty slot.
 *    2.3) If still not found, pick index = next_dtc_overwrite_index (FIFO)
 *         and advance next_dtc_overwrite_index with wrap-around.
 * 3) If index is valid:
 *    - Compute offset = DTC_REGION_OFFSET + index * DTC_SLOT_SIZE.
 *    - NVM_Write(offset, &new_record, sizeof(DTC_Record_t)).
 *    - If write OK → return true; else return false.
 * 4) If no valid index or write failed → return false.
 */
bool DTC_Set(uint32_t dtc_code, uint8_t status, const DTC_Snapshot_t *snapshot) {
	DTC_Record_t new_record;
	new_record.dtc_code = dtc_code;
	new_record.status_mask = status;

	if (snapshot != NULL) {
		memcpy(&new_record.snapshot, snapshot, sizeof(DTC_Snapshot_t));
	} else {
		/* Clear snapshot data if a null pointer is provided. */
		memset(&new_record.snapshot, 0, sizeof(DTC_Snapshot_t));
	}

	/* Logic Priority 1: Check if the DTC already exists to update it. */
	int8_t index = DTC_Find(dtc_code);
	if (index == -1) {
		/* Logic Priority 2: If it's a new DTC, find an empty slot.
		   0xFFFFFFFF is used to signify an erased/empty slot. */
		index = DTC_Find(0xFFFFFFFF);
	}

	/* Logic Priority 3: If no empty slots are found, overwrite the oldest DTC. */
	if (index == -1) {
		index = next_dtc_overwrite_index;
		/* Move the pointer to the next oldest slot for the next time memory is full. */
		next_dtc_overwrite_index++;
		if (next_dtc_overwrite_index >= DTC_COUNT) {
			next_dtc_overwrite_index = 0; /* Wrap around. */
		}
	}

	/* If a valid index was found (either existing, empty, or to-be-overwritten)... */
	if (index != -1) {
		uint32_t offset = DTC_REGION_OFFSET + (index * DTC_SLOT_SIZE);
		/* Write the prepared record to the determined slot in NVM. */
		if (NVM_Write(offset, (const uint8_t*) &new_record, sizeof(DTC_Record_t)) == NVM_OK) {
			return true;
		}
	}

	/* This is reached if no valid index was found or if the NVM write operation failed. */
	return false;
}

/**
 * @brief Read a DTC record by index into user buffer.
 *
 * Filters out erased (all 0xFF) or cleared (all zeros) records after read.
 *
 * @param index  Zero-based slot index in [0..DTC_COUNT-1].
 * @param record Output pointer to receive the DTC record.
 * @return true if a valid active DTC is returned, false otherwise.
 * 
 * Processing logic:
 * 1) Validate arguments: record != NULL and index < DTC_COUNT; else return false.
 * 2) Compute offset = DTC_REGION_OFFSET + index * DTC_SLOT_SIZE.
 * 3) NVM_Read(offset, record, sizeof(DTC_Record_t)); if read fails → return false.
 * 4) Validate content:
 *    - If record->dtc_code == 0xFFFFFFFF (erased) or == 0x00000000 (cleared)
 *      → return false.
 *    - Otherwise → return true (slot contains a valid, active DTC).
 */
bool DTC_GetRecord(uint8_t index, DTC_Record_t *record) {
	if (record == NULL || index >= DTC_COUNT) {
		return false;
	}

	uint32_t offset = DTC_REGION_OFFSET + (index * DTC_SLOT_SIZE);

	if (NVM_Read(offset, (uint8_t*) record, sizeof(DTC_Record_t)) == NVM_OK) {
		/* After reading, check if the slot contains an active DTC.
		   An erased slot will read as all 0xFFs. A cleared slot might be all 0s. */
		if (record->dtc_code != 0xFFFFFFFF && record->dtc_code != 0) {
			return true; /* The slot contains a valid, active DTC. */
		}
	}
	return false; /* The slot is empty or a read error occurred. */
}
