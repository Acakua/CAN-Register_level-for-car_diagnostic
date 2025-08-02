/*
 * dtc.c
 *
 * Created on: Aug 2, 2025
 * Author: Your Name
 *
 * @brief  Implementation of the Diagnostic Trouble Code (DTC) management module.
 */
#include "dtc.h"
#include "nvm.h"
#include <string.h> /* For memset and memcpy */

/*
 * A static variable to keep track of the next slot to overwrite when the DTC memory is full.
 * This implements a simple FIFO (First-In, First-Out) or circular buffer strategy.
 * 'static' ensures it retains its value between function calls and is private to this file.
 */
static uint8_t next_dtc_overwrite_index = 0;

/*
 * @brief See dtc.h for function documentation.
 */
void DTC_Init(void) {
	/* NVM_Init() should be called before this. This function can be used later
	   to check DTC data integrity on startup if needed. */
}

/*
 * @brief See dtc.h for function documentation.
 */
uint8_t DTC_GetCount(void) {
	return DTC_COUNT;
}

/*
 * @brief See dtc.h for function documentation.
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

/*
 * @brief See dtc.h for function documentation.
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

/*
 * @brief See dtc.h for function documentation.
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
