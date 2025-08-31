#ifndef MATRIXLED_H_
#define MATRIXLED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the MAX7219 driver and the underlying SPI communication.
 * @details This function sets up the SPI peripheral and configures the MAX7219
 * with default parameters: no-decode mode, full scan limit (8 digits),
 * medium brightness, and normal operation mode (not shutdown or display test).
 * It also clears the display buffer.
 */
void MatrixLed_Init(void);

/**
 * @brief Sets the brightness of the entire LED matrix.
 * @param brightnessLevel The desired brightness level. Valid values are 0 (dimmest) to 15 (brightest).
 * Values outside this range will be clamped.
 */
void MatrixLed_SetBrightness(uint8_t brightnessLevel);

/**
 * @brief Sets the number of columns (digits) to be scanned and displayed.
 * @details For an 8x8 matrix, this should typically be set to 7 (for 8 digits, 0-7).
 * Setting a lower limit can be used to disable parts of the display and reduce power.
 * @param scanLimit The number of digits to display, from 0 (1 digit) to 7 (8 digits).
 * Values greater than 7 will be clamped to 7.
 */
void MatrixLed_SetScanLimit(uint8_t scanLimit);

/**
 * @brief Puts the MAX7219 into shutdown mode or brings it back to normal operation.
 * @details In shutdown mode, the display is turned off and power consumption is minimized.
 * The display buffer content is retained and will be shown again upon exiting shutdown.
 * @param enterShutdown Set to 'true' to enter shutdown mode, 'false' for normal operation.
 */
void MatrixLed_Shutdown(bool enterShutdown);

/**
 * @brief Enables or disables the display test mode of the MAX7219.
 * @details In display test mode, all LEDs are turned on at maximum brightness, overriding
 * the current brightness setting and display buffer content.
 * @param enableTestMode Set to 'true' to enable display test, 'false' for normal operation.
 */
void MatrixLed_DisplayTest(bool enableTestMode);

/**
 * @brief Clears the internal display buffer and turns off all LEDs on the matrix.
 * @details This function writes zeros to all 8 rows of the internal buffer and then
 * transmits these zeros to all 8 digit registers of the MAX7219.
 */
void MatrixLed_Clear(void);

/**
 * @brief Loads a full 8x8 image from a buffer into the matrix display.
 * @details The buffer represents the entire 8x8 matrix, where each byte corresponds
 * to a column (x-coordinate).
 * @param frameBuffer A pointer to an 8-byte array. `frameBuffer[x]` holds the 8-bit pattern
 * for column `x`, where bit `y` corresponds to the LED at (x, y).
 */
void MatrixLed_Load(const uint8_t frameBuffer[8]);

/**
 * @brief Turns a single pixel on or off at a specified coordinate.
 * @details This function modifies the internal display buffer and then sends the
 * updated column data to the MAX7219.
 * @param x The horizontal coordinate (column) of the pixel (0-7).
 * @param y The vertical coordinate (row) of the pixel (0-7).
 * @param isOn Set to 'true' to turn the pixel on, 'false' to turn it off.
 */
void MatrixLed_DrawPixel(uint8_t x, uint8_t y, bool isOn);

#ifdef __cplusplus
}
#endif

#endif /* MATRIXLED_H_ */
