#include "sdk_project_config.h"
#include <interrupt_manager.h>
#include <FlexCan.h>
#include <stdint.h>
#include "adc_pal.h"
#include "adc_pal_cfg.h"
#include <uds.h>
#include "adc.h"

volatile int exit_code = 0;

void BoardInit(void)
{
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);

    myADC_Init();
}

int main(void)
{
    BoardInit();
    FLEXCAN0_init();

    CAN_Message_t msg_rx;
    while (1)
    {

        if (FLEXCAN0_receive_msg(&msg_rx)) {
            UDS_DispatchService(msg_rx);
        }
    }
    return exit_code;
}