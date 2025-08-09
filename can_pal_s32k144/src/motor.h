#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

void Motor_Init(void);
void Motor_Forward(void);
void Motor_Reverse(void);
void Motor_Stop(void);
void Motor_SetSpeed(uint16_t duty); // duty 0..1000

#endif
