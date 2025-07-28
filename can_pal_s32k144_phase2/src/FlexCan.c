#include "sdk_project_config.h"
#include <FlexCan.h>
#include <stdint.h>
void FLEXCAN0_init(void) {
    uint32_t i;

    PCC->PCCn[PCC_FlexCAN0_INDEX] |= PCC_PCCn_CGC_MASK;
    CAN0->MCR |= CAN_MCR_MDIS_MASK;
    CAN0->CTRL1 &= ~CAN_CTRL1_CLKSRC_MASK;
    CAN0->MCR &= ~CAN_MCR_MDIS_MASK;
    while (!(CAN0->MCR & CAN_MCR_FRZACK_MASK)) {}

    CAN0->CTRL1 = 0x00DB0006;

    for (i = 0; i < 128; i++) CAN0->RAMn[i] = 0;
    for (i = 0; i < 16; i++) CAN0->RXIMR[i] = 0xFFFFFFFF;
    CAN0->RXMGMASK = 0x7FF << 18;

    CAN0->RAMn[4*RX_MB_INDEX + 1] = RX_MSG_ID << 18;
    CAN0->RAMn[4*RX_MB_INDEX] = 0x04000000 | (8 << 16);  // Ready to receive, DLC=8

    CAN0->MCR = 0x0000001F;
    while (CAN0->MCR & CAN_MCR_FRZACK_MASK) {}
    while (CAN0->MCR & CAN_MCR_NOTRDY_MASK) {}
}

void FLEXCAN0_transmit_msg(const CAN_Message_t *msg) {
    // Clear interrupt flag for TX mailbox
    CAN0->IFLAG1 = 1 << TX_MB_INDEX;

    CAN0->RAMn[MSG_BUF_SIZE * TX_MB_INDEX + 1] = (msg->canID << 18);
    CAN0->RAMn[MSG_BUF_SIZE * TX_MB_INDEX] = 0x0C400000 | ((msg->dlc & 0xF) << 16);

    uint32_t dataWord0 = ((uint32_t)msg->data[0] << 24) |
                         ((uint32_t)msg->data[1] << 16) |
                         ((uint32_t)msg->data[2] << 8)  |
                         ((uint32_t)msg->data[3]);

    uint32_t dataWord1 = ((uint32_t)msg->data[4] << 24) |
                         ((uint32_t)msg->data[5] << 16) |
                         ((uint32_t)msg->data[6] << 8)  |
                         ((uint32_t)msg->data[7]);

    CAN0->RAMn[4 * TX_MB_INDEX + 2] = dataWord0;
    CAN0->RAMn[4 * TX_MB_INDEX + 3] = dataWord1;
}

int FLEXCAN0_receive_msg(CAN_Message_t *msg) {
    if (CAN0->IFLAG1 & (1 << RX_MB_INDEX)) {
        // Clear interrupt flag for RX mailbox
        CAN0->IFLAG1 = (1 << RX_MB_INDEX);

        uint32_t word0 = CAN0->RAMn[4 * RX_MB_INDEX];
        uint32_t word1 = CAN0->RAMn[4 * RX_MB_INDEX + 1];

        msg->canID = (word1 >> 18) & 0x7FF;
        msg->dlc = (word0 >> 16) & 0xF;

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

        // Ready to receive next message, reset RX mailbox
        CAN0->RAMn[4 * RX_MB_INDEX] = 0x04000000 | ((msg->dlc & 0xF) << 16);

        return 1;  // Có message mới
    }
    return 0;  // Không có message mới
}
