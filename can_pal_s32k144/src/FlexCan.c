#include "S32K144.h"
#include "FlexCan.h"
#define MSG_BUF_SIZE  4  // Số từ trong 1 bộ đệm tin nhắn

void FLEXCAN0_init(void) {
    uint32_t i;

    PCC->PCCn[PCC_FlexCAN0_INDEX] |= PCC_PCCn_CGC_MASK;  // Bật clock FlexCAN0

    CAN0->MCR |= CAN_MCR_MDIS_MASK;       // Vô hiệu hóa module trước cấu hình
    CAN0->CTRL1 &= ~CAN_CTRL1_CLKSRC_MASK; // Chọn clock 8MHz

    CAN0->MCR &= ~CAN_MCR_MDIS_MASK;      // Kích hoạt module, vào chế độ freeze
    while (!(CAN0->MCR & CAN_MCR_FRZACK_MASK)) {}

    CAN0->CTRL1 = 0x00DB0006;  // Cấu hình tốc độ CAN 500kbps

    for (i = 0; i < 128; i++) CAN0->RAMn[i] = 0;  // Xóa bộ đệm tin nhắn

    for (i = 0; i < 16; i++) CAN0->RXIMR[i] = 0xFFFFFFFF;  // Nhận tất cả ID
    CAN0->RXMGMASK = 0x1FFFFFFF;

#ifdef NODE_A
    CAN0->RAMn[4*MSG_BUF_SIZE + 1] = 0x14440000; // ID ví dụ
#else
    CAN0->RAMn[4*MSG_BUF_SIZE + 1] = 0x15540000;
#endif
    CAN0->RAMn[4*MSG_BUF_SIZE] = 0x04000000; // RX inactive

    CAN0->MCR = 0x0000001F;  // Thoát chế độ freeze
    while (CAN0->MCR & CAN_MCR_FRZACK_MASK) {}
    while (CAN0->MCR & CAN_MCR_NOTRDY_MASK) {}
}

void FLEXCAN0_transmit_msg(const CAN_Message_t *msg) {
    CAN0->IFLAG1 = 0x00000001;  // Xóa cờ cũ bộ đệm 0

    CAN0->RAMn[MSG_BUF_SIZE * 0 + 1] = (msg->canID << 18) & 0x1FFC0000; // ID chuẩn 11-bit

    CAN0->RAMn[MSG_BUF_SIZE * 0] = 0x0C000000 | ((msg->dlc & 0xF) << 16);  // Kích hoạt TX frame, DLC

    // Gộp 4 byte đầu tiên
    uint32_t dataWord0 = 0;
    dataWord0 |= msg->data[3];
    dataWord0 |= ((uint32_t)msg->data[2] << 8);
    dataWord0 |= ((uint32_t)msg->data[1] << 16);
    dataWord0 |= ((uint32_t)msg->data[0] << 24);
    CAN0->RAMn[2] = dataWord0;

    // Gộp 4 byte tiếp theo (byte 4 đến byte 7)
    uint32_t dataWord1 = 0;
    dataWord1 |= msg->data[7];
    dataWord1 |= ((uint32_t)msg->data[6] << 8);
    dataWord1 |= ((uint32_t)msg->data[5] << 16);
    dataWord1 |= ((uint32_t)msg->data[4] << 24);
    CAN0->RAMn[3] = dataWord1;
}


