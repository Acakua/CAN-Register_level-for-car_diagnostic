#include "sdk_project_config.h"
#include <interrupt_manager.h>
#include <stdint.h>
#include <stdbool.h>
#include "adc_driver.h"
#include "pins_driver.h"
#include "adc_pal.h"
#include "adc_pal_cfg.h"
#include "adc_hw_access.h"


#define EVB

#ifdef EVB
    #define LED_PORT        PORTD
    #define GPIO_PORT       PTD
    #define PCC_INDEX       PCC_PORTD_INDEX
    #define LED0            15U
    #define LED1            16U
#else
    #define LED_PORT        PORTC
    #define GPIO_PORT       PTC
    #define PCC_INDEX       PCC_PORTC_INDEX
    #define LED0            0U
    #define LED1            1U
#endif

#define MASTER

#if defined(MASTER)
    #define TX_MAILBOX  (1UL)
    #define TX_MSG_ID   (1UL)
    #define RX_MAILBOX  (0UL)
    #define RX_MSG_ID   (2UL)
#elif defined(SLAVE)
    #define TX_MAILBOX  (0UL)
    #define TX_MSG_ID   (2UL)
    #define RX_MAILBOX  (1UL)
    #define RX_MSG_ID   (1UL)
#endif

typedef enum {
    LED0_CHANGE_REQUESTED = 0x00U,
    LED1_CHANGE_REQUESTED = 0x01U
} can_commands_list;

uint8_t ledRequested = LED0_CHANGE_REQUESTED;


void ADCInit(void)
{
	ADC_Init(&adc_pal_1_instance, &adc_pal_1_config);

}


uint16_t ReadADCValue(void)
{
    status_t status;
    uint16_t adcResult = 0;


    ADC_StartConversion(&adc_pal_1_instance);


    while (ADC_GetStatusFlags(&adc_pal_1_instance) != ADC_STATUS_CONV_COMPLETE) {}


    status = ADC_GetConvValueRAW(&adc_pal_1_instance, 0U, &adcResult);
    if (status == STATUS_SUCCESS)
    {
        return adcResult;
    }
    return 0;
}



void BoardInit(void)
{
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);
    ADCInit();
}

void GPIOInit(void)
{
    PINS_DRV_SetPinsDirection(GPIO_PORT, (1 << LED1) | (1 << LED0));
    PINS_DRV_ClearPins(GPIO_PORT, (1 << LED1) | (1 << LED0));
}

volatile int exit_code = 0;

int main(void)
{
    BoardInit();
    GPIOInit();
    CAN_Init(&can_pal1_instance, &can_pal1_Config0);

    can_buff_config_t txBuffCfg = {
        .enableFD = true,
        .enableBRS = true,
        .fdPadding = 0U,
        .idType = CAN_MSG_ID_STD,
        .isRemote = false
    };

    CAN_ConfigTxBuff(&can_pal1_instance, TX_MAILBOX, &txBuffCfg);

    while(1)
    {

        uint16_t adcVal = ReadADCValue();


        float voltage = (adcVal * 3.3f) / 4095.0f;


        float temperatureC = voltage * 100.0f;

        uint16_t tempToSend = (uint16_t)(temperatureC * 10);


        can_message_t message = {
            .cs = 0U,
            .id = TX_MSG_ID,
            .length = 2U
        };


        message.data[0] = (uint8_t)(tempToSend & 0xFF);
        message.data[1] = (uint8_t)((tempToSend >> 8) & 0xFF);

        CAN_Send(&can_pal1_instance, TX_MAILBOX, &message);


        PINS_DRV_TogglePins(GPIO_PORT, (1 << LED0));


        for (volatile uint32_t delay = 0; delay < 1000000; delay++);
    }

    return exit_code;
}
