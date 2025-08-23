#include "MatrixLed.h"

/* ================================
 * Local Utility Functions
 * ================================
 */

/**
 * @brief Simple blocking delay (software loop).
 *
 * @param ms Delay time in milliseconds.
 *
 * Note: This uses a busy-wait loop with NOP instructions.
 *       Not accurate for real-time use, but sufficient for small delays.
 */
static void delay_ms(uint32_t ms) {
    for(uint32_t i=0; i < ms * 4000; i++) {
        __asm("nop");  /* No operation (consume 1 CPU cycle) */
    }
}

/**
 * @brief Send one byte of data using LPSPI0.
 *
 * @param data Byte to transmit.
 *
 * This function waits until the transmit FIFO is empty,
 * writes the data, and then waits for the receive flag
 * to clear the SPI status.
 */
static void SPI_SendByte(uint8_t data) {
    /* Wait until transmit FIFO is ready */
    while((LPSPI0->SR & LPSPI_SR_TDF_MASK) == 0);

    /* Write data to transmit register */
    LPSPI0->TDR = data;

    /* Wait until data has been shifted out and received */
    while((LPSPI0->SR & LPSPI_SR_RDF_MASK) == 0);

    /* Read receive register to clear RDF flag */
    (void)LPSPI0->RDR;
}

/* ================================
 * Public API Implementations
 * ================================
 */

/**
 * @brief Send a command to the MAX7219 via SPI.
 *
 * @param address Register address (0x01–0x08 for digits, or control registers).
 * @param data    Data to write into the register.
 */
void MatrixLed_Send(uint8_t address, uint8_t data) {
    /* Pull CS low to start transmission */
    PTB->PCOR = (1 << MATRIXLED_CS_PIN);

    /* Send address byte followed by data byte */
    SPI_SendByte(address);
    SPI_SendByte(data);

    /* Pull CS high to latch the data into MAX7219 */
    PTB->PSOR = (1 << MATRIXLED_CS_PIN);
}

/**
 * @brief Initialize MAX7219 and SPI peripheral.
 *
 * This function:
 * - Enables clocks for PORTB and LPSPI0
 * - Configures SPI pins (CLK, MOSI, CS)
 * - Configures LPSPI0 as master
 * - Initializes MAX7219 registers:
 *      - Scan limit (8 digits/rows)
 *      - Decode mode (no BCD decoding)
 *      - Shutdown mode disabled (normal operation)
 *      - Display test disabled
 *      - Intensity set to max
 * - Clears the LED matrix
 */
void MatrixLed_Init(void) {
    /* Enable clock for PORTB and LPSPI0 */
    PCC->PCCn[PCC_PORTB_INDEX] |= PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_LPSPI0_INDEX] = PCC_PCCn_PCS(0b001) | PCC_PCCn_CGC_MASK;

    /* Configure pin multiplexing:
     * - CLK pin -> ALT3 (LPSPI0_SCK)
     * - DIN pin -> ALT3 (LPSPI0_SOUT)
     * - CS pin  -> ALT1 (GPIO)
     */
    PORTB->PCR[MATRIXLED_CLK_PIN] = PORT_PCR_MUX(3);
    PORTB->PCR[MATRIXLED_DIN_PIN] = PORT_PCR_MUX(3);
    PORTB->PCR[MATRIXLED_CS_PIN]  = PORT_PCR_MUX(1);

    /* Configure CS as GPIO output, set high (inactive) */
    PTB->PDDR |= (1 << MATRIXLED_CS_PIN);
    PTB->PSOR = (1 << MATRIXLED_CS_PIN);

    /* Configure LPSPI0 as master */
    LPSPI0->CR = 0;  /* Disable before config */
    LPSPI0->CFGR1 = LPSPI_CFGR1_MASTER_MASK;
    LPSPI0->TCR = LPSPI_TCR_FRAMESZ(7)    /* 8-bit frame */
                | LPSPI_TCR_CPOL(0)       /* Clock polarity = 0 */
                | LPSPI_TCR_CPHA(0);      /* Clock phase = 0 */
    LPSPI0->CCR = LPSPI_CCR_SCKDIV(10);   /* Set SCK clock divider */
    LPSPI0->CR = LPSPI_CR_MEN_MASK;       /* Enable SPI module */

    /* Initialize MAX7219 registers */
    MatrixLed_Send(MATRIXLED_REG_SCANLIMIT, 0x07);   /* Display 8 digits/rows */
    MatrixLed_Send(MATRIXLED_REG_DECODEMODE, 0x00);  /* No BCD decode */
    MatrixLed_Send(MATRIXLED_REG_SHUTDOWN, 0x01);    /* Normal operation */
    MatrixLed_Send(MATRIXLED_REG_DISPLAYTEST, 0x00); /* Disable test mode */
    MatrixLed_Send(MATRIXLED_REG_INTENSITY, 0x0F);   /* Max brightness */

    /* Clear all LEDs */
    MatrixLed_Clear();
}

/**
 * @brief Clear the LED matrix (all LEDs off).
 */
void MatrixLed_Clear(void) {
    for(uint8_t i = 0; i < 8; i++) {
        MatrixLed_Send(i + 1, 0x00);
    }
}

/**
 * @brief Display an 8x8 matrix pattern.
 *
 * @param data Pointer to an array of 8 bytes,
 *             each byte represents one row of the LED matrix.
 *             - Bit 0 → Column 1
 *             - Bit 7 → Column 8
 */
void MatrixLed_DisplayMatrix(uint8_t *data) {
    for(uint8_t i = 0; i < 8; i++) {
        MatrixLed_Send(i + 1, data[i]);
    }
}
