#ifndef SENDDATA_H
#define SENDDATA_H

#include <stdint.h>

/* ================================
 * Function Prototypes
 * ================================
 */

/**
 * @brief Initialize CAN sender module with periodic timer.
 *
 * This function configures LPIT (Low Power Interrupt Timer) channel 0
 * to trigger every 1 second. Each time the timer expires, the interrupt
 * handler (LPIT0_Ch0_IRQHandler) will be executed to read sensor data
 * and send a CAN message.
 */
void CAN_SENDER_Init(void);

/**
 * @brief LPIT Channel 0 Interrupt Handler.
 *
 * This ISR is automatically called every time LPIT channel 0 expires.
 * It performs the following tasks:
 * - Reads ADC values (temperature, light level)
 * - Packages them along with fan/headlight states
 * - Sends the data via CAN bus
 */
extern void LPIT0_Ch0_IRQHandler(void);

#endif /* SENDDATA_H */
