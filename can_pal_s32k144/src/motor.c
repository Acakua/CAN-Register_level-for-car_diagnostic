#include "motor.h"

/**
 * @brief Initialize motor GPIOs and PWM (FTM2).
 *
 * This function sets up:
 * - Clock gating for PORTD, PORTE, and FTM2
 * - GPIO configuration for motor direction pins
 * - PWM configuration using FTM2 channel for speed control
 */
void Motor_Init(void) {
    /* Enable clock for PORTD and PORTE */
    PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_PORTE_INDEX] |= PCC_PCCn_CGC_MASK;

    /* Configure motor direction pins as GPIO (ALT = 1) */
    PORTE->PCR[MOTOR_PIN1_GPIO_PIN] = PORT_PCR_MUX(1);
    PORTD->PCR[MOTOR_PIN2_GPIO_PIN] = PORT_PCR_MUX(1);

    /* Set direction pins as output */
    MOTOR_PIN1_GPIO_PORT->PDDR |= (1 << MOTOR_PIN1_GPIO_PIN);
    MOTOR_PIN2_GPIO_PORT->PDDR |= (1 << MOTOR_PIN2_GPIO_PIN);

    /* Initialize motor direction pins to LOW (stop state) */
    MOTOR_PIN1_GPIO_PORT->PCOR |= (1 << MOTOR_PIN1_GPIO_PIN);
    MOTOR_PIN2_GPIO_PORT->PCOR |= (1 << MOTOR_PIN2_GPIO_PIN);

    /* Enable clock for FTM2 (PWM generator) */
    PCC->PCCn[PCC_FTM2_INDEX] |= PCC_PCCn_CGC_MASK;

    /* Configure PWM pin alternative function (ALT = 2 for FTM2_CH0) */
    MOTOR_PWM_PORT->PCR[MOTOR_PWM_PIN] = PORT_PCR_MUX(2);

    /* Configure FTM2:
     * - Clock source = system clock
     * - Prescaler = 64 (PS = 6)
     */
    MOTOR_PWM_FTM_PERIPH->SC = FTM_SC_CLKS(1) | FTM_SC_PS(6);

    /* Set PWM period (MOD register = 25000 ticks) */
    MOTOR_PWM_FTM_PERIPH->MOD = 25000;

    /* Configure PWM mode:
     * - Edge-aligned PWM
     * - High-true pulses
     */
    MOTOR_PWM_FTM_PERIPH->CONTROLS[MOTOR_PWM_CHANNEL_IDX].CnSC =
        FTM_CnSC_MSB(1) | FTM_CnSC_ELSB(1);

    /* Initialize duty cycle = 0% (motor off) */
    MOTOR_PWM_FTM_PERIPH->CONTROLS[MOTOR_PWM_CHANNEL_IDX].CnV = 0;
}

/**
 * @brief Set motor speed by adjusting PWM duty cycle.
 *
 * @param speed Duty cycle value (0 to MOD register value).
 *              If speed > MOD, it will be capped to MOD.
 */
void Motor_SetSpeed(uint16_t speed) {
    /* Limit speed to maximum allowed (MOD value) */
    if (speed > MOTOR_PWM_FTM_PERIPH->MOD) {
        speed = MOTOR_PWM_FTM_PERIPH->MOD;
    }

    /* Update duty cycle (CnV register) */
    MOTOR_PWM_FTM_PERIPH->CONTROLS[MOTOR_PWM_CHANNEL_IDX].CnV = speed;
}

/**
 * @brief Set motor rotation direction.
 *
 * @param direction One of:
 *        - MOTOR_FORWARD : Rotate forward
 *        - MOTOR_REVERSE : Rotate backward
 *        - MOTOR_STOP    : Stop rotation
 */
void Motor_SetDirection(uint8_t direction) {
    switch (direction) {
        case MOTOR_FORWARD:
            /* Forward: IN1 = 0, IN2 = 1 */
            MOTOR_PIN1_GPIO_PORT->PCOR |= (1 << MOTOR_PIN1_GPIO_PIN);
            MOTOR_PIN2_GPIO_PORT->PSOR |= (1 << MOTOR_PIN2_GPIO_PIN);
            break;

        case MOTOR_REVERSE:
            /* Reverse: IN1 = 1, IN2 = 0 */
            MOTOR_PIN1_GPIO_PORT->PSOR |= (1 << MOTOR_PIN1_GPIO_PIN);
            MOTOR_PIN2_GPIO_PORT->PCOR |= (1 << MOTOR_PIN2_GPIO_PIN);
            break;

        case MOTOR_STOP:
        default:
            /* Stop: IN1 = 0, IN2 = 0 */
            MOTOR_PIN1_GPIO_PORT->PCOR |= (1 << MOTOR_PIN1_GPIO_PIN);
            MOTOR_PIN2_GPIO_PORT->PCOR |= (1 << MOTOR_PIN2_GPIO_PIN);
            break;
    }
}
