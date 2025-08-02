/*
 * @brief  Implementation of the NVM abstraction layer for S32K144.
 */
#include "nvm.h"
#include <string.h>

/* The Flash driver configuration structure. It's initialized by the S32K Config Tool. */
extern flash_ssd_config_t flashSSDConfig;


/*
 * @brief See nvm.h for function documentation.
 */
NVM_Status NVM_Read(uint32_t offset, uint8_t *data, uint8_t len)
{
    /* Validate input parameters to prevent memory errors. */
    if (data == NULL || (offset + len) > NVM_SIZE)
    {
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

/*
 * @brief See nvm.h for function documentation.
 */
NVM_Status NVM_Write(uint32_t offset, const uint8_t *data, uint8_t len)
{
    /* Validate input parameters. */
    if (data == NULL || (offset + len) > NVM_SIZE)
    {
        return NVM_INVALID_PARAM;
    }

    /*
     * Use the S32K SDK's Flash Driver API to write to the emulated EEPROM.
     * This high-level function conveniently handles the required erase-before-write
     * cycles of the underlying flash memory internally.
     */
    status_t flash_status = FLASH_DRV_EEEWrite(&flashSSDConfig, NVM_START_ADDRESS + offset, len, data);

    /* Convert the SDK's status code to our module's status code. */
    if (flash_status == STATUS_SUCCESS)
    {
        return NVM_OK;
    }
    else
    {
        return NVM_ERROR;
    }
}

/*
 * @brief See nvm.h for function documentation.
 */
NVM_Status NVM_Erase(uint32_t offset, uint32_t len)
{
    /* Validate parameters. */
    if ((offset + len) > NVM_SIZE)
    {
        return NVM_INVALID_PARAM;
    }

    /* Create a temporary buffer filled with 0xFF, which is the erased state of flash memory. */
    uint8_t erase_buffer[32];
    memset(erase_buffer, 0xFF, sizeof(erase_buffer));

    uint32_t remaining_len = len;
    uint32_t current_offset = offset;
    status_t flash_status = STATUS_SUCCESS;

    /* Erase in chunks to avoid using a large buffer on the stack. */
    while (remaining_len > 0)
    {
        uint32_t chunk_size = (remaining_len > sizeof(erase_buffer)) ? sizeof(erase_buffer) : remaining_len;

        /* "Erasing" is performed by writing 0xFF to the desired area. */
        flash_status = FLASH_DRV_EEEWrite(&flashSSDConfig, NVM_START_ADDRESS + current_offset, chunk_size, erase_buffer);

        if (flash_status != STATUS_SUCCESS)
        {
            return NVM_ERROR;
        }

        current_offset += chunk_size;
        remaining_len -= chunk_size;
    }

    return NVM_OK;
}
