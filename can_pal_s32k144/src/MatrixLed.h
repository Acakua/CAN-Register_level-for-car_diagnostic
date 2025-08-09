#ifndef MATRIX_H
#define MATRIX_H

#include <stdint.h>

void Matrix_Init(void);
void Matrix_SetPixel(uint8_t x, uint8_t y, uint8_t on);
void Matrix_Update(void);

#endif
