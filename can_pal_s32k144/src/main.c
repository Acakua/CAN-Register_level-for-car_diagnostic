#include "sdk_project_config.h"
#include <interrupt_manager.h>
#include <stdint.h>
#include <stdbool.h>

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

void BoardInit(void)
{
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);
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
        can_message_t message = {
            .cs = 0U,
            .id = TX_MSG_ID,
            .data[0] = ledRequested,
            .length = 1U
        };

        CAN_Send(&can_pal1_instance, TX_MAILBOX, &message);
        PINS_DRV_TogglePins(GPIO_PORT, (1 << LED0));
        ledRequested ^= 1U;

        for (volatile uint32_t delay = 0; delay < 1000000; delay++);
    }

    return exit_code;
}
