#ifndef ADC_H
#define ADC_H

#include <stdint.h>

void myADC_Init(void);
uint16_t myADC_Read(uint8_t channel);

#endif
