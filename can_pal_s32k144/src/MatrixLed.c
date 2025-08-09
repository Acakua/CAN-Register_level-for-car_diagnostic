#include "sdk_project_config.h"
#include "MatrixLed.h"

static uint8_t led_buffer[8];
static uint8_t current_row = 0;

void Matrix_Init(void)
{
    PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK;

    for (int i=0; i<8; i++) {
        PORTC->PCR[i] = PORT_PCR_MUX(1);
        PTC->PDDR |= (1<<i);

        PORTD->PCR[i] = PORT_PCR_MUX(1);
        PTD->PDDR |= (1<<i);
    }
}

void Matrix_SetPixel(uint8_t x, uint8_t y, uint8_t on)
{
    if (x >= 8 || y >= 8) return;
    if (on)
        led_buffer[y] |= (1 << x);
    else
        led_buffer[y] &= ~(1 << x);
}

void Matrix_Update(void)
{
    PTD->PSOR = 0xFF;

    uint8_t data = led_buffer[current_row];
    for (int col=0; col<8; col++) {
        if (data & (1<<col))
            PTC->PSOR = (1<<col);  // HIGH
        else
            PTC->PCOR = (1<<col);  // LOW
    }

    PTD->PCOR = (1<<current_row);

    current_row++;
    if (current_row >= 8) current_row = 0;
}
