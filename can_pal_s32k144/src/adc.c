#include "sdk_project_config.h"
#include "adc.h"

/**
 * @brief Initialize ADC0 peripheral and configure input pins for analog mode.
 *
 * Configures clock gating for PORTC and ADC0, sets PORTC pins 14 & 15 to analog
 * function, performs ADC calibration, and sets up basic ADC configuration for
 * single-ended conversions.
 *
 * Processing logic:
 * 1) Enable clock for PORTC and ADC0 modules via PCC.
 * 2) Configure PORTC14 and PORTC15:
 *    - Clear MUX bits
 *    - Set MUX = 0 -> analog input function
 * 3) Start ADC calibration by setting ADC0->SC3[CAL] bit and wait until it clears.
 * 4) Disable ADC channel by setting ADC0->SC1[0] = ADCH(31) (module idle).
 * 5) Configure ADC0->CFG1:
 *    - ADICLK = 0 (Bus clock)
 *    - MODE   = 1 (12-bit mode)
 *    - ADLSMP = 0 (Short sample time)
 *    - ADIV   = 0 (Divide ratio = 1)
 * 6) Configure ADC0->SC2 = 0 (default: software trigger, VrefH/VrefL as ref).
 *
 * Context:
 * - Must be called once before using myADC_Read().
 * - Pins PORTC14 and PORTC15 become analog-only after configuration.
 */
void myADC_Init(void)
{
    // Step 1: Enable clock for PORTC and ADC0
    PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_ADC0_INDEX] = PCC_PCCn_PCS(1) | PCC_PCCn_CGC_MASK;

    // Step 2: Configure PORTC14 and PORTC15 for analog input
    PORTC->PCR[14] = (PORTC->PCR[14] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(0);
    PORTC->PCR[15] = (PORTC->PCR[15] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(0);

    // Step 3: Start ADC calibration
    ADC0->SC3 = ADC_SC3_CAL_MASK;
    while (ADC0->SC3 & ADC_SC3_CAL_MASK) {}

    // Step 4: Disable ADC channel (idle mode)
    ADC0->SC1[0] = ADC_SC1_ADCH(31);

    // Step 5: Configure ADC conversion parameters
    ADC0->CFG1 = (0 << 0) |  // ADICLK = 0 (Bus clock)
                 (1 << 2) |  // MODE = 1 (12-bit)
                 (0 << 4) |  // ADLSMP = 0 (Short sample)
                 (0 << 5);   // ADIV = 0 (Divide by 1)

    // Step 6: Default SC2 configuration (software trigger, VrefH/VrefL)
    ADC0->SC2 = 0;
}

/**
 * @brief Read a single ADC conversion result from the specified channel.
 *
 * Starts a conversion on the given ADC0 channel and blocks until the result
 * is ready, then returns the raw 12-bit conversion value.
 *
 * @param channel ADC channel number (0-31 depending on MCU pin mapping).
 * @return uint16_t Raw ADC conversion result (0-4095 for 12-bit mode).
 *
 * Processing logic:
 * 1) Write channel number to ADC0->SC1[0] to start conversion.
 * 2) Wait for conversion complete flag (COCO) in SC1[0] to be set.
 * 3) Read ADC0->R[0] register to get conversion result.
 *
 * Context:
 * - myADC_Init() must be called first to configure the ADC.
 * - This function blocks until conversion finishes.
 */
uint16_t myADC_Read(uint8_t channel)
{
    // Step 1: Trigger conversion on the specified channel
    ADC0->SC1[0] = ADC_SC1_ADCH(channel);

    // Step 2: Wait for conversion complete
    while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0);

    // Step 3: Return ADC result
    return (uint16_t)(ADC0->R[0]);
}
