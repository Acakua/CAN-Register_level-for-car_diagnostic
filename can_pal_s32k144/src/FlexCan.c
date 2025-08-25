#include "sdk_project_config.h"
#include <FlexCan.h>
#include <stdint.h>

/**
 * @brief Initialize FlexCAN0 module for CAN communication.
 *
 * Configures FlexCAN0 to use the oscillator clock, sets bit timing,
 * initializes message buffers, sets receive mask and message ID filters,
 * and prepares RX mailbox to receive UDS messages.
 *
 * Processing logic:
 * 1) Enable clock for FlexCAN0 via PCC.
 * 2) Enter freeze mode (MDIS=1, CLKSRC=0) to allow configuration.
 * 3) Wait for FRZACK bit to assert (confirm freeze mode).
 * 4) Configure CAN_CTRL1 with bit timing parameters:
 *    - This example uses 0x00DB0006 (bit segments, prescaler)
 *      for a specific baud rate (e.g., 500 kbps).
 * 5) Clear all 128 message buffer RAM entries.
 * 6) Set RX individual mask registers (RXIMR) to 0xFFFFFFFF (accept all bits).
 * 7) Set global RX mask (RXMGMASK) to filter by standard ID bits [28:18].
 * 8) Configure RX mailbox (RX_MB_INDEX) with UDS message ID and DLC=8.
 * 9) Exit freeze mode, wait until module is ready.
 *
 * Context:
 * - Must be called once before transmit/receive functions.
 * - RX_MB_INDEX and TX_MB_INDEX are defined in FlexCAN.h.
 */
void FLEXCAN0_init(void) {
    uint32_t i;

    // Step 1: Enable PCC clock for FlexCAN0
    PCC->PCCn[PCC_FlexCAN0_INDEX] |= PCC_PCCn_CGC_MASK;

    // Step 2: Enter freeze mode (disable module for config)
    CAN0->MCR |= CAN_MCR_MDIS_MASK;
    CAN0->CTRL1 &= ~CAN_CTRL1_CLKSRC_MASK; // Select oscillator clock
    CAN0->MCR &= ~CAN_MCR_MDIS_MASK;

    // Step 3: Wait until freeze acknowledge
    while (!(CAN0->MCR & CAN_MCR_FRZACK_MASK)) {}

    // Step 4: Configure CAN bit timing
    CAN0->CTRL1 = 0x00DB0006; // Prescaler, PropSeg, PSEG1, PSEG2 values

    // Step 5: Clear all message buffer RAM
    for (i = 0; i < 128; i++) CAN0->RAMn[i] = 0;

    // Step 6: Accept all IDs initially in RXIMR
    for (i = 0; i < 16; i++) CAN0->RXIMR[i] = 0xFFFFFFFF;

    // Step 7: Set global RX mask for standard IDs (11-bit)
    CAN0->RXMGMASK = 0x7FF << 18;

    // Step 8: Configure RX mailbox with expected UDS ID and DLC=8
    CAN0->RAMn[4*RX_MB_INDEX + 1] = RX_MSG_ID_UDS << 18;
    CAN0->RAMn[4*RX_MB_INDEX] = 0x04000000 | (8 << 16);  // Empty, ready to receive

    // Step 9: Exit freeze mode
    CAN0->MCR = 0x0000001F; // Enable module, max mailbox=16
    while (CAN0->MCR & CAN_MCR_FRZACK_MASK) {}   // Wait until not frozen
    while (CAN0->MCR & CAN_MCR_NOTRDY_MASK) {}   // Wait until ready
}

/**
 * @brief Transmit a CAN frame via FlexCAN0.
 *
 * Loads the TX mailbox with CAN ID, DLC, and payload, then triggers
 * transmission and waits for completion.
 *
 * @param msg Pointer to CAN_Message_t containing:
 *            - canID: Standard ID (11-bit)
 *            - dlc:   Data length code (0-8)
 *            - data[]: Payload bytes
 *
 * Processing logic:
 * 1) Prepare TX mailbox in INACTIVE state (code=1000b).
 * 2) Write CAN ID to ID word (shifted to bits [28:18]).
 * 3) Pack first 4 data bytes into dataWord0, last 4 into dataWord1.
 * 4) Write packed data into mailbox RAM.
 * 5) Clear TX IFLAG to reset status.
 * 6) Activate TX mailbox with DATA frame code (1100b) and DLC.
 * 7) Wait until IFLAG is set (TX complete).
 * 8) Clear IFLAG.
 *
 * Context:
 * - TX_MB_INDEX must be configured during init.
 */
void FLEXCAN0_transmit_msg(const CAN_Message_t *msg) {
    // Step 1: Set TX mailbox to INACTIVE
    CAN0->RAMn[MSG_BUF_SIZE * TX_MB_INDEX] = 0x08000000;

    // Step 2: Set CAN ID (standard)
    CAN0->RAMn[MSG_BUF_SIZE * TX_MB_INDEX + 1] = (msg->canID << 18);

    // Step 3: Pack data into two 32-bit words
    uint32_t dataWord0 = ((uint32_t)msg->data[0] << 24) |
                         ((uint32_t)msg->data[1] << 16) |
                         ((uint32_t)msg->data[2] << 8)  |
                         ((uint32_t)msg->data[3]);
    uint32_t dataWord1 = ((uint32_t)msg->data[4] << 24) |
                         ((uint32_t)msg->data[5] << 16) |
                         ((uint32_t)msg->data[6] << 8)  |
                         ((uint32_t)msg->data[7]);

    // Step 4: Write data words into mailbox RAM
    CAN0->RAMn[4 * TX_MB_INDEX + 2] = dataWord0;
    CAN0->RAMn[4 * TX_MB_INDEX + 3] = dataWord1;

    // Step 5: Clear TX IFLAG
    CAN0->IFLAG1 = (1 << TX_MB_INDEX);

    // Step 6: Activate TX mailbox (send data frame)
    CAN0->RAMn[MSG_BUF_SIZE * TX_MB_INDEX] =
        0x0C400000 | ((msg->dlc & 0xF) << 16);

    // Step 7: Wait for TX completion
    while (!(CAN0->IFLAG1 & (1 << TX_MB_INDEX))) {}

    // Step 8: Clear TX completion flag
    CAN0->IFLAG1 = (1 << TX_MB_INDEX);
}

/**
 * @brief Receive a CAN frame from FlexCAN0 RX mailbox.
 *
 * Checks if a new frame is available in the RX mailbox, validates the ID,
 * and copies the payload into the provided message structure.
 *
 * @param msg Pointer to CAN_Message_t to store received frame.
 * @param expectedID Expected CAN ID to accept.
 * @return int 1 if a valid frame with matching ID is received, 0 otherwise.
 *
 * Processing logic:
 * 1) Check RX IFLAG for RX_MB_INDEX.
 * 2) Clear RX IFLAG to acknowledge reception.
 * 3) Read word0 (control/status) and word1 (ID).
 * 4) Extract rxID and dlc from words.
 * 5) If rxID != expectedID:
 *    - Reset mailbox with DLC=8 and return 0.
 * 6) Otherwise, read dataWord0 and dataWord1 from mailbox.
 * 7) Extract 8 bytes into msg->data[].
 * 8) Reset RX mailbox with current DLC.
 * 9) Return 1 (success).
 *
 * Context:
 * - Non-blocking: returns 0 if no message or ID mismatch.
 */
int FLEXCAN0_receive_msg(CAN_Message_t *msg, uint32_t expectedID) {
    // Step 1: Check RX mailbox flag
    if (CAN0->IFLAG1 & (1 << RX_MB_INDEX)) {
        // Step 2: Clear flag
        CAN0->IFLAG1 = (1 << RX_MB_INDEX);

        // Step 3: Read control/status and ID
        uint32_t word0 = CAN0->RAMn[4 * RX_MB_INDEX];
        uint32_t word1 = CAN0->RAMn[4 * RX_MB_INDEX + 1];

        // Step 4: Extract ID and DLC
        uint32_t rxID = (word1 >> 18) & 0x7FF;
        uint8_t dlc  = (word0 >> 16) & 0xF;

        // Step 5: Validate ID
        if (rxID != expectedID) {
            CAN0->RAMn[4 * RX_MB_INDEX] = 0x04000000 | (8 << 16);
            return 0;
        }

        // Step 6: Store received ID & DLC
        msg->canID = rxID;
        msg->dlc = dlc;

        // Step 7: Read and unpack data
        uint32_t dataWord0 = CAN0->RAMn[4 * RX_MB_INDEX + 2];
        uint32_t dataWord1 = CAN0->RAMn[4 * RX_MB_INDEX + 3];

        msg->data[0] = (dataWord0 >> 24) & 0xFF;
        msg->data[1] = (dataWord0 >> 16) & 0xFF;
        msg->data[2] = (dataWord0 >> 8)  & 0xFF;
        msg->data[3] = (dataWord0 >> 0)  & 0xFF;
        msg->data[4] = (dataWord1 >> 24) & 0xFF;
        msg->data[5] = (dataWord1 >> 16) & 0xFF;
        msg->data[6] = (dataWord1 >> 8)  & 0xFF;
        msg->data[7] = (dataWord1 >> 0)  & 0xFF;

        // Step 8: Reset RX mailbox to ready state
        CAN0->RAMn[4 * RX_MB_INDEX] =
            0x04000000 | ((msg->dlc & 0xF) << 16);

        return 1; // Step 9: Success
    }
    return 0; // No message
}