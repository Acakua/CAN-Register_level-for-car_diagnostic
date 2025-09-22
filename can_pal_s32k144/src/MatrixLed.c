#include "MatrixLed.h"

#include "sdk_project_config.h"
#include "device_registers.h"
#include "lpspi_master_driver.h"

/* =========================
 * Hardware Pinout Configuration
 * - PTB0 : LPSPI0_PCS0 (ALT3) -> LOAD/CS (HW)
 * - PTB1 : LPSPI0_SOUT (ALT3) -> DIN MAX7219
 * - PTB2 : LPSPI0_SCK  (ALT3) -> CLK
 * ========================= */

#define MATRIXLED_LPSPI_INST   0u       /* LPSPI instance number */
#define MATRIXLED_SPI_BAUD     500000u  /* SPI baud rate in Hz */

/**
 * @brief Internal frame buffer for the 8x8 LED matrix.
 * @details Each element of the array represents a column (X-coordinate).
 * Each bit within an element corresponds to a row's LED (Y-coordinate).
 * s_frameBuffer[0] is column 0.
 * Bit 0 of s_frameBuffer[0] is the pixel at (0, 0).
 */
static uint8_t s_frameBuffer[8];

/* External LPSPI master configuration structure. */
extern const lpspi_master_config_t lpspi_0_MasterConfig0;

/* State structure for the LPSPI driver. */
static lpspi_state_t s_lpspiState;

/**
 * @brief MAX7219 Internal Register Addresses.
 */
enum {
    REG_NOOP        = 0x00,
    REG_DIGIT0      = 0x01,
    REG_DIGIT1      = 0x02,
    REG_DIGIT2      = 0x03,
    REG_DIGIT3      = 0x04,
    REG_DIGIT4      = 0x05,
    REG_DIGIT5      = 0x06,
    REG_DIGIT6      = 0x07,
    REG_DIGIT7      = 0x08,
    REG_DECODE_MODE = 0x09,
    REG_INTENSITY   = 0x0A,
    REG_SCAN_LIMIT  = 0x0B,
    REG_SHUTDOWN    = 0x0C,
    REG_DISPLAYTEST = 0x0F
};

/**
 * @brief Initializes the LPSPI peripheral for communication with the MAX7219.
 * @details Processing Logic:
 * Calls the platform-specific LPSPI driver initialization function
 * with the predefined instance number and configuration structures.
 */
static void spi_init(void)
{
    (void)LPSPI_DRV_MasterInit(MATRIXLED_LPSPI_INST, &s_lpspiState, &lpspi_0_MasterConfig0);
}

/**
 * @brief Sends a 16-bit command (register address + data) to the MAX7219.
 * @details Processing Logic:
 * 1. Creates a 2-byte buffer. The first byte is the register address,
 * and the second byte is the data to be written.
 * 2. Initiates a blocking SPI transfer to send these two bytes.
 * 3. A dummy receive buffer is provided as required by the driver API.
 * @param registerAddress The address of the MAX7219 register to write to.
 * @param dataPayload The 8-bit data to write to the specified register.
 */
static void max7219_write(uint8_t registerAddress, uint8_t dataPayload)
{
    uint8_t transmitBuffer[2] = { registerAddress, dataPayload };
    uint8_t receiveBuffer[2]; // Dummy buffer for received data
    (void)LPSPI_DRV_MasterTransferBlocking(MATRIXLED_LPSPI_INST,
                                           transmitBuffer, receiveBuffer,
                                           sizeof(transmitBuffer),
                                           5u /* timeout in ms */);
}

/**
 * @brief Initializes the MAX7219 driver and the underlying SPI communication.
 * @details Processing Logic:
 * 1. Initializes the SPI peripheral by calling `spi_init()`.
 * 2. Configures MAX7219 registers for matrix operation:
 * - Decode Mode: 0x00 (No BCD decoding, direct bit mapping).
 * - Scan Limit:  0x07 (Display all 8 columns/digits).
 * - Intensity:   0x08 (Medium brightness).
 * - Display Test:0x00 (Disable display test mode).
 * 3. Clears the matrix display and the internal buffer.
 * 4. Takes the device out of shutdown mode to enable the display.
 */
void MatrixLed_Init(void)
{
    spi_init();

    max7219_write(REG_DECODE_MODE, 0x00);
    max7219_write(REG_SCAN_LIMIT,  0x07);
    max7219_write(REG_INTENSITY,   0x08);
    max7219_write(REG_DISPLAYTEST, 0x00);

    MatrixLed_Clear();
    max7219_write(REG_SHUTDOWN,    0x01); // 0x01 = Normal Operation
}

/**
 * @brief Sets the brightness of the entire LED matrix.
 * @details Processing Logic:
 * 1. Clamps the input `brightnessLevel` to the valid range of 0x00 to 0x0F.
 * 2. Writes the clamped value to the Intensity Register (0x0A).
 */
void MatrixLed_SetBrightness(uint8_t brightnessLevel)
{
    if (brightnessLevel > 0x0F) {
        brightnessLevel = 0x0F;
    }
    max7219_write(REG_INTENSITY, brightnessLevel);
}

/**
 * @brief Sets the number of columns (digits) to be scanned and displayed.
 * @details Processing Logic:
 * 1. Clamps the input `scanLimit` to the valid range of 0 to 7.
 * 2. Writes the clamped value to the Scan-Limit Register (0x0B).
 */
void MatrixLed_SetScanLimit(uint8_t scanLimit)
{
    if (scanLimit > 7u) {
        scanLimit = 7u;
    }
    max7219_write(REG_SCAN_LIMIT, scanLimit);
}

/**
 * @brief Puts the MAX7219 into shutdown mode or brings it back to normal operation.
 * @details Processing Logic:
 * 1. The MAX7219 shutdown register (0x0C) uses 0x00 for shutdown and 0x01 for normal mode.
 * 2. A ternary operator maps the boolean input `enterShutdown` to the correct register value.
 * - `true`  -> 0x00 (Enter shutdown)
 * - `false` -> 0x01 (Normal operation)
 * 3. Writes the resulting value to the Shutdown Register.
 */
void MatrixLed_Shutdown(bool enterShutdown)
{
    max7219_write(REG_SHUTDOWN, enterShutdown ? 0x00u : 0x01u);
}

/**
 * @brief Enables or disables the display test mode of the MAX7219.
 * @details Processing Logic:
 * 1. The display test register (0x0F) uses 0x01 for test mode and 0x00 for normal mode.
 * 2. A ternary operator maps the boolean input `enableTestMode` to the correct register value.
 * 3. Writes the resulting value to the Display-Test Register.
 */
void MatrixLed_DisplayTest(bool enableTestMode)
{
    max7219_write(REG_DISPLAYTEST, enableTestMode ? 0x01u : 0x00u);
}

/**
 * @brief Clears the internal display buffer and turns off all LEDs on the matrix.
 * @details Processing Logic:
 * 1. Iterates through all 8 columns (from 0 to 7).
 * 2. In each iteration, it sets the corresponding byte in the `s_frameBuffer` to 0x00.
 * 3. It then sends a command to the MAX7219 to write 0x00 to the current digit register
 * (REG_DIGIT0 + loop index), effectively clearing one column on the physical display.
 */
void MatrixLed_Clear(void)
{
    for (uint8_t columnIndex = 0u; columnIndex < 8u; columnIndex++) {
        s_frameBuffer[columnIndex] = 0x00u;
        max7219_write((uint8_t)(REG_DIGIT0 + columnIndex), 0x00u);
    }
}

/**
 * @brief Loads a full 8x8 image from a buffer into the matrix display.
 * @details Processing Logic:
 * 1. Iterates through all 8 columns (from 0 to 7).
 * 2. In each iteration, it copies one byte from the user-provided `frameBuffer`
 * to the corresponding location in the internal `s_frameBuffer`.
 * 3. It then immediately sends this byte to the appropriate digit register
 * (REG_DIGIT0 + loop index) on the MAX7219 to update the physical display.
 */
void MatrixLed_Load(const uint8_t frameBuffer[8])
{
    for (uint8_t columnIndex = 0u; columnIndex < 8u; columnIndex++) {
        s_frameBuffer[columnIndex] = frameBuffer[columnIndex];
        max7219_write((uint8_t)(REG_DIGIT0 + columnIndex), s_frameBuffer[columnIndex]);
    }
}

/**
 * @brief Turns a single pixel on or off at a specified coordinate.
 * @details Processing Logic:
 * 1. Performs a boundary check to ensure x and y coordinates are within the 0-7 range.
 * If they are out of bounds, the function exits immediately.
 * 2. Based on the `isOn` parameter, it performs a bitwise operation on the
 * internal frame buffer for the specified column `x`.
 * - To turn a pixel ON (`isOn` = true): It uses a bitwise OR with a mask
 * (1 shifted left by `y` bits) to set the `y`-th bit to 1.
 * `s_frameBuffer[x] |= (1u << y);`
 * - To turn a pixel OFF (`isOn` = false): It uses a bitwise AND with an
 * inverted mask to clear the `y`-th bit to 0.
 * `s_frameBuffer[x] &= ~(1u << y);`
 * 3. After modifying the buffer, it writes the entire updated byte for column `x`
 * to the corresponding digit register on the MAX7219.
 */
void MatrixLed_DrawPixel(uint8_t x, uint8_t y, bool isOn)
{
    if (x > 7u || y > 7u) {
        return; // Exit if coordinates are out of bounds
    }

    if (isOn) {
        s_frameBuffer[x] |= (uint8_t)(1u << y);
    } else {
        s_frameBuffer[x] &= (uint8_t)~(1u << y);
    }

    max7219_write((uint8_t)(REG_DIGIT0 + x), s_frameBuffer[x]);
}
