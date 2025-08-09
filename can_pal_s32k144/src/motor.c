#include "sdk_project_config.h"
#include "motor.h"

void Motor_Init(void)
{
    PCC->PCCn[PCC_PORTB_INDEX] |= PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_PORTA_INDEX] |= PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_FTM1_INDEX] |= PCC_PCCn_CGC_MASK;

    PORTB->PCR[0] = PORT_PCR_MUX(1);
    PORTB->PCR[1] = PORT_PCR_MUX(1);
    PTB->PDDR |= (1<<0) | (1<<1);

    PORTA->PCR[12] = PORT_PCR_MUX(2);

    FTM1->MODE |= FTM_MODE_WPDIS_MASK;
    FTM1->SC = 0;
    FTM1->CNTIN = 0;
    FTM1->MOD = 1000 - 1; // 1000 steps

    FTM1->CONTROLS[0].CnSC = FTM_CnSC_MSB_MASK | FTM_CnSC_ELSB_MASK;
    FTM1->CONTROLS[0].CnV = 0;

    FTM1->SC = FTM_SC_CLKS(1) | FTM_SC_PS(0);
}

void Motor_Forward(void)
{
    PTB->PSOR = (1<<0);
    PTB->PCOR = (1<<1);
}

void Motor_Reverse(void)
{
    PTB->PSOR = (1<<1);
    PTB->PCOR = (1<<0);
}

void Motor_Stop(void)
{
    PTB->PCOR = (1<<0) | (1<<1);
}

void Motor_SetSpeed(uint16_t duty)
{
    if (duty > 1000) duty = 1000;
    FTM1->CONTROLS[0].CnV = duty;
}
