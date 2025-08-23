#include "nvm.h"
#include "uds.h"
#include "FlexCan.h"
#include "sdk_project_config.h"
#include <interrupt_manager.h>
#include "flash_driver.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <SendData.h>
#include "adc.h"
#include "motor.h"
#include "MatrixLed.h"

/* Thresholds */
#define TEMP_LOW_THRESHOLD      1500
#define TEMP_HIGH_THRESHOLD     2500

#define LIGHT_LOW_THRESHOLD     1200
#define LIGHT_HIGH_THRESHOLD    2800

/* Matrix LED patterns */
uint8_t full_on_pattern[8] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

uint8_t half_on_pattern[8] = {
    0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF
};

uint8_t all_off_pattern[8] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};


status_t status;

flash_ssd_config_t flashSSDConfig;


void BoardInit(void)
{
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);

    myADC_Init();
    FLEXCAN0_init();
    Motor_Init();
	MatrixLed_Init();
	myADC_Init();
	//CAN_SENDER_Init();   /* Timer interrupt handles periodic CAN sending */

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
    uint16_t temperature = 0;
    uint16_t light_level = 0;
    CAN_Message_t msg_rx;

    /* Initialize peripherals */
    BoardInit();

    while (1)
    {
        /* Read ADC channels */
        temperature = myADC_Read(13);  /* Temperature sensor */
        light_level = myADC_Read(12);  /* Light sensor */

        /* ----------- Motor Control ----------- */
        if (temperature < TEMP_LOW_THRESHOLD)
        {
            Motor_SetDirection(MOTOR_FORWARD);
            Motor_SetSpeed(700);
        }
        else if (temperature > TEMP_HIGH_THRESHOLD)
        {
            Motor_SetDirection(MOTOR_REVERSE);
            Motor_SetSpeed(700);
        }
        else
        {
            Motor_SetDirection(MOTOR_STOP);
            Motor_SetSpeed(0);
        }

        /* ----------- Matrix LED Control ----------- */
        if (light_level < LIGHT_LOW_THRESHOLD)
        {
            MatrixLed_DisplayMatrix(full_on_pattern);  /* All LEDs ON */
        }
        else if (light_level > LIGHT_HIGH_THRESHOLD)
        {
            MatrixLed_DisplayMatrix(all_off_pattern);  /* All LEDs OFF */
        }
        else
        {
            MatrixLed_DisplayMatrix(half_on_pattern);  /* Half LEDs ON */
        }


        if (FLEXCAN0_receive_msg(&msg_rx, RX_MSG_ID_UDS)) {
        	UDS_DispatchService(msg_rx);
        }
    }

    return 0;
}
