#include "nvm.h"
#include "uds.h"
#include "FlexCan.h"
#include "sdk_project_config.h"
#include <interrupt_manager.h>
#include "flash_driver.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "adc.h"

status_t status;

flash_ssd_config_t flashSSDConfig;

volatile int exit_code = 0;

void BoardInit(void)
{
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);

    myADC_Init();

	/* Initialize the Flash driver for NVM operations. */
	status = FLASH_DRV_Init(&Flash_InitConfig0, &flashSSDConfig);
	DEV_ASSERT(status == STATUS_SUCCESS);

	/* Partition FlexNVM for EEPROM emulation if it hasn't been done yet. */
	if ((FEATURE_FLS_HAS_FLEX_NVM == 1u) && (FEATURE_FLS_HAS_FLEX_RAM == 1u)) {
		if (flashSSDConfig.EEESize == 0u) {
			status = FLASH_DRV_DEFlashPartition(&flashSSDConfig, 0x02u, 0x08u,
					0x0u, false, true);
			DEV_ASSERT(status == STATUS_SUCCESS);

			/* Re-initialize driver after partitioning to update its configuration. */
			status = FLASH_DRV_Init(&Flash_InitConfig0, &flashSSDConfig);
			DEV_ASSERT(status == STATUS_SUCCESS);
		}
	}
	/* Enable EEPROM emulation functionality. */
	status = FLASH_DRV_SetFlexRamFunction(&flashSSDConfig, EEE_ENABLE, 0x00u,
	NULL);
	DEV_ASSERT(status == STATUS_SUCCESS);
}

int main(void)
{
    BoardInit();
    FLEXCAN0_init();

    CAN_Message_t msg_rx;
    while (1)
    {
        if (FLEXCAN0_receive_msg(&msg_rx, RX_MSG_ID_UDS)) {
            UDS_DispatchService(msg_rx);
        }
    }
    return exit_code;
}