#include "sdk_project_config.h"
#include <interrupt_manager.h>
#include <stdint.h>
#include <stdbool.h>
#include <FlexCan.h>
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

int main(void) {
    BoardInit();
    GPIOInit();

    FLEXCAN0_init();  // Khoi tao FlexCAN0

    uint8_t buffer[4] = {0};
    uint8_t ledRequested = 0;  // Trang thai led gui: 0 hoac 1

    while (1) {
        // Chuẩn bi du lieu gui
        for (int i = 0; i < 4; i++) {
            buffer[i] = ledRequested;  // Gan 4 byte giong nhau, co the sua theo y muon
        }

        FLEXCAN0_transmit_msg(buffer);  // Gui du lieu CAN

        PINS_DRV_TogglePins(GPIO_PORT, (1 << LED0));  // Dao trang thai LED0

        ledRequested ^= 1;  // Doi trang thai 0 -> 1 hoac 1 -> 0

        for (volatile uint32_t delay = 0; delay < 1000000; delay++);  // Delay don gian
    }

    return 0;
}
