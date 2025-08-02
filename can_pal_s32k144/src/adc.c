#include "sdk_project_config.h"
#include "adc.h"


void myADC_Init(void)
{
    PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_ADC0_INDEX] = PCC_PCCn_PCS(1) | PCC_PCCn_CGC_MASK;

    PORTC->PCR[14] = (PORTC->PCR[14] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(0);

    PORTC->PCR[15] = (PORTC->PCR[15] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(0);

    ADC0->SC3 = ADC_SC3_CAL_MASK;
    while (ADC0->SC3 & ADC_SC3_CAL_MASK) {}

    ADC0->SC1[0] = ADC_SC1_ADCH(31);
    ADC0->CFG1 = (0 << 0) | (1 << 2) | (0 << 4) | (0 << 5);
    ADC0->SC2 = 0;
}

uint16_t myADC_Read(uint8_t channel)
{
    ADC0->SC1[0] = ADC_SC1_ADCH(channel);
    while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0);
    return (uint16_t)(ADC0->R[0]);
}
