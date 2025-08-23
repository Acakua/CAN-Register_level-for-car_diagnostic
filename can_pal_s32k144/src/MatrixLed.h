#ifndef MATRIXLED_H_
#define MATRIXLED_H_

#include "S32K144.h"
#include <stdint.h>

/* ================================
 * GPIO Pin Configuration for Matrix LED (MAX7219)
 * ================================
 */

#define MATRIXLED_DIN_PIN        1
#define MATRIXLED_CS_PIN         0
#define MATRIXLED_CLK_PIN        2

/* ================================
 * MAX7219 Register Addresses
 * ================================
 */

/** No operation (used for daisy-chained devices) */
#define MATRIXLED_REG_NOOP       0x00
#define MATRIXLED_REG_DIGIT0     0x01
#define MATRIXLED_REG_DIGIT1     0x02
#define MATRIXLED_REG_DIGIT2     0x03
#define MATRIXLED_REG_DIGIT3     0x04
#define MATRIXLED_REG_DIGIT4     0x05
#define MATRIXLED_REG_DIGIT5     0x06
#define MATRIXLED_REG_DIGIT6     0x07
#define MATRIXLED_REG_DIGIT7     0x08
/** Decode mode register (set BCD decode on/off) */
#define MATRIXLED_REG_DECODEMODE 0x09
/** Intensity register (set display brightness) */
#define MATRIXLED_REG_INTENSITY  0x0A
/** Scan limit register (set number of digits/rows displayed) */
#define MATRIXLED_REG_SCANLIMIT  0x0B
/** Shutdown register (normal operation / shutdown mode) */
#define MATRIXLED_REG_SHUTDOWN   0x0C
/** Display test register (turn on all LEDs for testing) */
#define MATRIXLED_REG_DISPLAYTEST 0x0F

/* ================================
 * Function Prototypes
 * ================================
 */

/**
 * @brief Initialize the MAX7219 LED driver.
 *
 * This function configures GPIO pins and sends initialization commands
 * to the MAX7219 (decode mode, intensity, scan limit, shutdown).
 */
void MatrixLed_Init(void);

/**
 * @brief Send a command to MAX7219.
 *
 * @param address Register address (e.g., MATRIXLED_REG_DIGIT0).
 * @param data    Data to be written into the register.
 */
void MatrixLed_Send(uint8_t address, uint8_t data);

/**
 * @brief Clear the LED matrix (turn off all LEDs).
 */
void MatrixLed_Clear(void);

/**
 * @brief Display a matrix pattern.
 *
 * @param data Pointer to an array of 8 bytes, where each byte
 *             represents a row of the LED matrix (8x8).
 */
void MatrixLed_DisplayMatrix(uint8_t *data);

#endif /* MATRIXLED_H_ */
