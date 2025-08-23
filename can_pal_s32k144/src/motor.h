#ifndef MOTOR_H
#define MOTOR_H

#include "S32K144.h"

/* ================================
 * Motor GPIO Pin Configuration
 * ================================
 */

#define MOTOR_PIN1_GPIO_PORT  PTE
#define MOTOR_PIN1_GPIO_PIN   0

#define MOTOR_PIN2_GPIO_PORT  PTD
#define MOTOR_PIN2_GPIO_PIN   17

#define MOTOR_PWM_PORT        PORTD
#define MOTOR_PWM_PIN         10
#define MOTOR_PWM_FTM_PERIPH  FTM2
#define MOTOR_PWM_CHANNEL_IDX 0

/* ================================
 * Motor Direction Control Macros
 * ================================
 */

/** Stop the motor */
#define MOTOR_STOP    0
/** Run the motor in forward direction */
#define MOTOR_FORWARD 1
/** Run the motor in reverse direction */
#define MOTOR_REVERSE 2

/* ================================
 * Function Prototypes
 * ================================
 */

/**
 * @brief Initialize motor control peripherals.
 *
 * This function configures GPIOs and the FlexTimer module (FTM)
 * to prepare the motor for PWM speed control and direction control.
 */
void Motor_Init(void);

/**
 * @brief Set motor speed.
 *
 * @param speed PWM duty cycle value (0 - 100% mapped to timer range).
 *              A higher value means higher motor speed.
 */
void Motor_SetSpeed(uint16_t speed);

/**
 * @brief Set motor direction.
 *
 * @param direction Use one of the predefined macros:
 *                  - MOTOR_STOP
 *                  - MOTOR_FORWARD
 *                  - MOTOR_REVERSE
 */
void Motor_SetDirection(uint8_t direction);

#endif /* MOTOR_H */
