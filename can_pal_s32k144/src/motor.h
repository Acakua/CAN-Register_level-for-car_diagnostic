#ifndef MOTOR_H_
#define MOTOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "sdk_project_config.h"   /* pulls in ftm_pwm_driver.h, pins_driver.h, etc. */

#define MOTOR_FTM_INSTANCE            INST_FLEXTIMER_PWM_1
#define MOTOR_FTM_CHANNEL             (flexTimer_pwm_1_IndependentChannelsConfig[0].hwChannelId)

/* Direction control pins for the H-bridge */
#define MOTOR_IN1_GPIO                PTE
#define MOTOR_IN1_PIN                 (0u)

#define MOTOR_IN2_GPIO                PTD
#define MOTOR_IN2_PIN                 (17u)

/* Optional Standby/Enable pin for the H-bridge */
#define MOTOR_HAS_STBY                (0)    /* Set to 1 if a STBY pin is used */
#define MOTOR_STBY_GPIO               PTC
#define MOTOR_STBY_PIN                (3u)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Defines the possible operational states for the H-bridge motor driver.
 */
typedef enum
{
    MOTOR_DIR_COAST = 0,    /**< IN1=0, IN2=0: Motor is disconnected, spins freely (high-impedance). */
    MOTOR_DIR_FORWARD,      /**< IN1=1, IN2=0: Motor rotates in the forward direction. */
    MOTOR_DIR_REVERSE,      /**< IN1=0, IN2=1: Motor rotates in the reverse direction. */
    MOTOR_DIR_BRAKE         /**< IN1=1, IN2=1: Motor terminals are shorted, causing active braking. */
} motor_dir_t;

/**
 * @brief Initializes the motor driver.
 * @details This function configures the FTM peripheral for PWM generation and sets
 * the direction control GPIO pins to a safe initial state (COAST). The initial
 * motor speed is set to 0.
 */
void Motor_Init(void);

/**
 * @brief Sets the rotational direction of the motor.
 * @details This function immediately changes the state of the direction control GPIO pins.
 * It does not affect the currently set PWM duty cycle (speed).
 * @param direction The desired motor direction (COAST, FORWARD, REVERSE, or BRAKE).
 */
void Motor_SetDirection(motor_dir_t direction);

/**
 * @brief Sets the motor speed as a permille value (parts per thousand).
 * @details This provides fine-grained control over the motor's speed. The function
 * calculates the required PWM duty cycle in timer ticks to be independent of the PWM
 * frequency and alignment mode.
 * @param permille The desired speed from 0 (stopped) to 1000 (full speed). Values
 * above 1000 will be saturated to 1000.
 */
void Motor_SetSpeed(uint16_t speed);

/**
 * @brief Stops the motor safely.
 * @details This function sets the PWM duty cycle to 0 and puts the H-bridge
 * into the COAST state, allowing the motor to spin down freely.
 */
void Motor_Stop(void);

/**
 * @brief De-initializes the motor driver.
 * @details Puts the motor into a safe state (stopped, coasting) and, if applicable,
 * puts the H-bridge driver IC into standby mode to conserve power.
 */
void Motor_Deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_H_ */
