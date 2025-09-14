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
#include <stdbool.h>

/* Thresholds */
#define LIGHT_LOW_THRESHOLD     30
#define LIGHT_HIGH_THRESHOLD    60

volatile uint16_t temp_threshold_low = 60;
volatile uint16_t temp_threshold_medium = 70;
volatile uint16_t temp_threshold_high = 90;

volatile static bool overheat_flag = false;

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

volatile uint16_t temperature = 0;
volatile uint16_t light_level = 0;

volatile uint16_t temp_raw = 0;
volatile uint16_t light_raw = 0;

volatile uint8_t fan_state = 0;
volatile uint8_t light_state = 0;

volatile uint8_t day = 1;

void LoadConfigurationFromNVM(void) {
    NVM_Read(DID_TEMP_THRESHOLD_LOW_OFFSET, (uint8_t*)&temp_threshold_low, 2);
    NVM_Read(DID_TEMP_THRESHOLD_MEDIUM_OFFSET, (uint8_t*)&temp_threshold_medium, 2);
    NVM_Read(DID_TEMP_THRESHOLD_HIGH_OFFSET, (uint8_t*)&temp_threshold_high, 2);

    // If NVM is empty (value 0xFFFF), use default value
    if (temp_threshold_low == 0xFFFF) temp_threshold_low = 60;
    if (temp_threshold_medium == 0xFFFF) temp_threshold_medium = 70;
    if (temp_threshold_high == 0xFFFF) temp_threshold_high = 90;
}

void BoardInit(void)
{
	CLOCK_SYS_Init(g_clockManConfigsArr, CLOCK_MANAGER_CONFIG_CNT,
			g_clockManCallbacksArr, CLOCK_MANAGER_CALLBACK_CNT);
	CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT);
	PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);

    myADC_Init();
    FLEXCAN0_init();
    Motor_Init();
	MatrixLed_Init();
	CAN_SENDER_Init();   /* Timer interrupt handles periodic CAN sending */

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
	LoadConfigurationFromNVM();
	day = 1;
}


int main(void)
{
    CAN_Message_t msg_rx;

    BoardInit();


    while (1)
    {
		if (FLEXCAN0_receive_msg(&msg_rx, RX_MSG_ID_UDS)) {
			UDS_DispatchService(msg_rx);
		}
		/* Read ADC channels */
    	temp_raw = myADC_Read(13); /* Temperature sensor */
    	light_raw = myADC_Read(12); /* Light sensor */

    	float voltage = temp_raw * (3.3f / 4095.0f);


    	temperature = (uint16_t)(voltage * 100);
    	light_level = (uint16_t)((light_raw * 100)/4095);

    	/* ----------- Motor Control ----------- */
		if (temperature > temp_threshold_high) {
			if (overheat_flag == false) {
				DTC_Snapshot_t snapshot;
				snapshot.temperature = (uint8_t) temperature;
				snapshot.day = day;
				snapshot.month = 8;
				snapshot.year = 2025;
				day++;

				uint8_t status_mask = DTC_STATUS_TEST_FAILED
						| DTC_STATUS_PENDING_DTC;
				DTC_Set(DTC_ENGINE_OVERHEAT, status_mask, &snapshot);
			}
			Motor_SetDirection(MOTOR_DIR_FORWARD);
			Motor_SetSpeed(1000);
			fan_state = 1;
		} else {
			overheat_flag = false;
			if (temperature > temp_threshold_medium) {
				Motor_SetDirection(MOTOR_DIR_FORWARD);
				Motor_SetSpeed(600);
				fan_state = 1;
			} else {
				Motor_SetDirection(MOTOR_DIR_BRAKE);
				Motor_SetSpeed(0);
				fan_state = 0;
			}
		}


		/* ----------- Matrix LED Control ----------- */
		static uint8_t last_mode = 0xFF;   // 0: full_on, 1: half_on, 2: all_off
		uint8_t mode_now;

		if (light_level < LIGHT_LOW_THRESHOLD)
			mode_now = 2;
		else if (light_level > LIGHT_HIGH_THRESHOLD)
			mode_now = 0;
		else
			mode_now = 1;

		if (mode_now != last_mode) {
			if (mode_now == 0) {
				MatrixLed_Load(full_on_pattern);
				light_state = 1;
			} else if (mode_now == 1) {
				MatrixLed_Load(half_on_pattern);
				light_state = 1;
			} else {
				MatrixLed_Load(all_off_pattern);
				light_state = 0;
			}
			last_mode = mode_now;
		}
    }

    return 0;
}
