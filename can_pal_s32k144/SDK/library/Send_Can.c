#include <Send_Can.h>

#define CAN0_BASE    0x40024000
#define CAN0_MCR     (*(volatile uint32_t*)(CAN0_BASE + 0x000))
#define CAN0_RAMn(i) (*(volatile uint32_t*)(CAN0_BASE + 0x080 + (i) * 4))
#define CAN0_IFLAG1  (*(volatile uint32_t*)(CAN0_BASE + 0x030))

/**
 * @brief Send a CAN frame without blocking CPU.
 *
 * Checks if the transmit buffer is available,
 * then prepares and sends the CAN frame if possible.
 *
 * @param id   Standard 11-bit CAN identifier.
 * @param data Pointer to data bytes to send.
 * @param len  Number of data bytes (max 8).
 * @return int Returns 1 if the frame is sent (buffer ready), 0 if the transmit buffer is busy.
 */
int Send_Can(uint16_t id, uint8_t *data, uint8_t len)
{
    if (len > 8) len = 8;

    // Check if transmit buffer is free
    if ((CAN0_IFLAG1 & 0x1) == 0) {
        // Buffer busy, cannot send now
        return 0;
    }

    uint32_t std_id = (id & 0x7FF) << 18;

    // Clear previous data
    CAN0_RAMn(0) = 0;
    CAN0_RAMn(1) = std_id;
    CAN0_RAMn(2) = 0;
    CAN0_RAMn(3) = 0;

    // Pack data bytes into registers
    for (uint8_t i = 0; i < len; i++) {
        if (i < 4)
            CAN0_RAMn(3) |= ((uint32_t)data[i]) << (8 * (3 - i));
        else
            CAN0_RAMn(2) |= ((uint32_t)data[i]) << (8 * (7 - i));
    }

    // Set transmit command and data length
    CAN0_RAMn(0) = 0x0C400000 | ((uint32_t)len << 16);

    // Clear transmit flag by writing 1
    CAN0_IFLAG1 = 0x1;

    return 1; // Frame sent (buffer was ready)
}
