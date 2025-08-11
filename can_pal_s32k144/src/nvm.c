/*
 * @brief  Implementation of the NVM abstraction layer for S32K144.
 */
#include "nvm.h"
#include <string.h>

/* The Flash driver configuration structure. It's initialized by the S32K Config Tool. */
extern flash_ssd_config_t flashSSDConfig;


/**
 * @brief Read bytes from EEPROM-emulated FlexNVM region.
 *
 * Performs bounds checking, then memcpys from memory-mapped EEE address.
 *
 * @param offset Byte offset within NVM region [0..NVM_SIZE).
 * @param data   Destination buffer (must not be NULL).
 * @param len    Number of bytes to read.
 * @return NVM_OK on success; NVM_INVALID_PARAM on bad args; NVM_ERROR on driver error.
 *
 * Processing logic:
 * 1) Validate data != NULL and (offset + len) ≤ NVM_SIZE.
 * 2) Compute base pointer: (NVM_START_ADDRESS + offset).
 * 3) memcpy() len bytes into caller buffer.
 */
NVM_Status NVM_Read(uint32_t offset, uint8_t *data, uint8_t len) {
    /* Validate input parameters to prevent memory errors. */
    if (data == NULL || (offset + len) > NVM_SIZE) {
        return NVM_INVALID_PARAM;
    }

    /*
     * The FlexNVM memory used for EEPROM emulation is memory-mapped.
     * This allows us to read it directly via a pointer, which is highly efficient.
     */
    const uint8_t* eeprom_address = (const uint8_t*)(NVM_START_ADDRESS + offset);

    /* Copy the data directly from the memory-mapped address into the destination buffer. */
    memcpy(data, eeprom_address, len);

    return NVM_OK;
}

/**
 * @brief Write bytes to EEPROM-emulated FlexNVM region.
 *
 * Uses FLASH_DRV_EEEWrite() which handles erase-before-write internally.
 *
 * @param offset Byte offset within NVM region [0..NVM_SIZE).
 * @param data   Source buffer (must not be NULL).
 * @param len    Number of bytes to write.
 * @return NVM_OK on success; NVM_INVALID_PARAM on bad args; NVM_ERROR on driver error.
 *
 * Processing logic:
 * 1) Validate data != NULL and (offset + len) ≤ NVM_SIZE.
 * 2) Call FLASH_DRV_EEEWrite(flashSSDConfig, start + offset, len, data).
 * 3) Map status_t → NVM_Status.
 */
NVM_Status NVM_Write(uint32_t offset, const uint8_t *data, uint8_t len) {
    /* Validate input parameters. */
    if (data == NULL || (offset + len) > NVM_SIZE) {
        return NVM_INVALID_PARAM;
    }

    /*
     * Use the S32K SDK's Flash Driver API to write to the emulated EEPROM.
     * This high-level function conveniently handles the required erase-before-write
     * cycles of the underlying flash memory internally.
     */
    status_t flash_status = FLASH_DRV_EEEWrite(&flashSSDConfig, NVM_START_ADDRESS + offset, len, data);

    /* Convert the SDK's status code to our module's status code. */
    if (flash_status == STATUS_SUCCESS) {
        return NVM_OK;
    } else {
        return NVM_ERROR;
    }
}

/**
 * @brief Erase a range in the EEE region by writing 0xFF blocks.
 *
 * Performs chunked writes of 0xFF to avoid large stack buffers.
 *
 * @param offset Start offset within NVM region.
 * @param len    Length in bytes to erase.
 * @return NVM_OK on success; NVM_INVALID_PARAM on bad args; NVM_ERROR on driver error.
 *
 * Processing logic:
 * 1) Validate (offset + len) ≤ NVM_SIZE.
 * 2) Fill a small local buffer with 0xFF (erased state of flash).
 * 3) Loop while remaining_len > 0:
 *    - chunk_size = min(remaining_len, sizeof(local_buf)).
 *    - FLASH_DRV_EEEWrite(start + current_offset, chunk_size, 0xFF...).
 *    - Advance offset/remaining.
 * 4) Return NVM_OK if all chunks succeed; else NVM_ERROR.
 */
NVM_Status NVM_Erase(uint32_t offset, uint32_t len) {
    /* Validate parameters. */
    if ((offset + len) > NVM_SIZE) {
        return NVM_INVALID_PARAM;
    }

    /* Create a temporary buffer filled with 0xFF, which is the erased state of flash memory. */
    uint8_t erase_buffer[32];
    memset(erase_buffer, 0xFF, sizeof(erase_buffer));

    uint32_t remaining_len = len;
    uint32_t current_offset = offset;
    status_t flash_status = STATUS_SUCCESS;

    /* Erase in chunks to avoid using a large buffer on the stack. */
    while (remaining_len > 0){
        uint32_t chunk_size = (remaining_len > sizeof(erase_buffer)) ? sizeof(erase_buffer) : remaining_len;

        /* "Erasing" is performed by writing 0xFF to the desired area. */
        flash_status = FLASH_DRV_EEEWrite(&flashSSDConfig, NVM_START_ADDRESS + current_offset, chunk_size, erase_buffer);

        if (flash_status != STATUS_SUCCESS) {
            return NVM_ERROR;
        }

        current_offset += chunk_size;
        remaining_len -= chunk_size;
    }

    return NVM_OK;
}
