#include "sdk_project_config.h"
#include <interrupt_manager.h>
#include <FlexCan.h>
#include <stdint.h>
#include "adc_pal.h"
#include "adc_pal_cfg.h"
#include <uds.h>


volatile int exit_code = 0;

// ==== ADC Init & Read ====
void ADCInit(void)
{
    PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_ADC0_INDEX] = PCC_PCCn_PCS(1) | PCC_PCCn_CGC_MASK;
    PORTC->PCR[14] = (PORTC->PCR[14] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(0);
    ADC0->SC3 = ADC_SC3_CAL_MASK;
    while (ADC0->SC3 & ADC_SC3_CAL_MASK) {}
    ADC0->SC1[0] = ADC_SC1_ADCH(31);
    ADC0->CFG1 = (0 << 0) | (1 << 2) | (0 << 4) | (0 << 5);
    ADC0->SC2 = 0;
}

uint16_t ReadADCValue(void)
{
    ADC0->SC1[0] = ADC_SC1_ADCH(12);
    while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0);
    return (uint16_t)(ADC0->R[0]);
}

// ==== Init Functions ====
void BoardInit(void)
{
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);
    ADCInit();
}


// ==== Main ====
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
