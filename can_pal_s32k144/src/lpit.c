#include "sdk_project_config.h"
#include "MatrixLed.h"

void LPIT0_init(void)
{
    PCC->PCCn[PCC_LPIT_INDEX] = PCC_PCCn_PCS(6) | PCC_PCCn_CGC_MASK;

    LPIT0->MCR = LPIT_MCR_M_CEN_MASK;

    LPIT0->TMR[0].TVAL = 40000;

    LPIT0->TMR[0].TCTRL = LPIT_TMR_TCTRL_T_EN_MASK;

    LPIT0->MIER = LPIT_MIER_TIE0_MASK;
	#define S32_NVIC_ISER ((volatile uint32_t*)0xE000E100u)
    S32_NVIC_ISER[LPIT0_Ch0_IRQn / 32] = (1UL << (LPIT0_Ch0_IRQn % 32));
}

void LPIT0_Ch0_IRQHandler(void)
{
    LPIT0->MSR = LPIT_MSR_TIF0_MASK;

    Matrix_Update();
}
