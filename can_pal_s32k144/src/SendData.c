#include <SendData.h>
#include "sdk_project_config.h"
#include "FlexCan.h"
#include "lpit_driver.h"
#include "adc.h"
#include "interrupt_manager.h"

/* ================================
 * Global / Static Variables
 * ================================
 */

/** Fan ON/OFF state (toggled every CAN message) */
static uint8_t fan_state = 0;
/** Headlight ON/OFF state (toggled every CAN message) */
static uint8_t headlight_state = 0;

/** Latest temperature sensor reading (ADC channel 13) */
static uint16_t temperature = 0;
/** Latest light sensor reading (ADC channel 12) */
static uint16_t light_level = 0;

/* External LPIT configuration (defined elsewhere) */
extern const lpit_user_config_t lpit1_InitConfig;
extern lpit_user_channel_config_t lpit1_ChnConfig0;

/* ================================
 * Function Implementations
 * ================================
 */

/**
 * @brief Initialize CAN message sender with periodic timer.
 *
 * This function:
 * - Initializes LPIT (Low Power Interrupt Timer)
 * - Configures Channel 0 to trigger every 1 second
 * - Installs and enables interrupt handler
 * - Starts LPIT timer channel
 *
 * Result: Every 1 second, LPIT0_Ch0_IRQHandler() will be called
 *         to read sensor data and send a CAN message.
 */
void CAN_SENDER_Init(void)
{
    status_t status;

    /* Initialize LPIT module */
    LPIT_DRV_Init(INST_LPIT_CONFIG_1, &lpit1_InitConfig);

    /* Configure LPIT channel 0 */
    status = LPIT_DRV_InitChannel(INST_LPIT_CONFIG_1, 0, &lpit1_ChnConfig0);
    DEV_ASSERT(status == STATUS_SUCCESS);

    /* Set period to 1 second (1,000,000 microseconds) */
    LPIT_DRV_SetTimerPeriodByUs(INST_LPIT_CONFIG_1, 0, 1000000U);

    /* Install and enable interrupt for LPIT channel 0 */
    INT_SYS_InstallHandler(LPIT0_Ch0_IRQn, LPIT0_Ch0_IRQHandler, NULL);
    INT_SYS_EnableIRQ(LPIT0_Ch0_IRQn);

    /* Start LPIT channel 0 timer */
    LPIT_DRV_StartTimerChannels(INST_LPIT_CONFIG_1, (1U << 0));
}

/**
 * @brief LPIT0 Channel 0 Interrupt Handler.
 *
 * This interrupt is triggered every 1 second. The handler:
 * - Clears interrupt flag
 * - Reads temperature (ADC channel 13) and light level (ADC channel 12)
 * - Prepares and transmits a CAN message with the following format:
 *      CAN ID = 0x200
 *      DLC    = 6 bytes
 *      Data[0] = Fan state (0/1)
 *      Data[1] = Headlight state (0/1)
 *      Data[2] = Temperature high byte
 *      Data[3] = Temperature low byte
 *      Data[4] = Light level high byte
 *      Data[5] = Light level low byte
 * - Toggles fan_state and headlight_state for next transmission
 */
void LPIT0_Ch0_IRQHandler(void)
{
    /* Clear interrupt flag */
    LPIT_DRV_ClearInterruptFlagTimerChannels(INST_LPIT_CONFIG_1, 0);

    /* Read sensor data from ADC channels */
    temperature = myADC_Read(13);
    light_level = myADC_Read(12);

    /* Prepare CAN message */
    CAN_Message_t tx_msg;
    tx_msg.canID = 0x200;       /* CAN identifier */
    tx_msg.dlc = 6;             /* Data length code = 6 bytes */
    tx_msg.data[0] = fan_state;
    tx_msg.data[1] = headlight_state;
    tx_msg.data[2] = (uint8_t)(temperature >> 8); /* High byte */
    tx_msg.data[3] = (uint8_t)temperature;        /* Low byte */
    tx_msg.data[4] = (uint8_t)(light_level >> 8); /* High byte */
    tx_msg.data[5] = (uint8_t)light_level;        /* Low byte */

    /* Transmit message via CAN */
    FLEXCAN0_transmit_msg(&tx_msg);

    /* Toggle fan and headlight states */
    fan_state = !fan_state;
    headlight_state = !headlight_state;
}
