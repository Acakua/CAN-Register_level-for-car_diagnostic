#include "S32K144.h"  // Thu vien dinh nghia cac thanh ghi

#define MSG_BUF_SIZE  4  // So tu trong 1 bo dem tin nhan (2 tu header + 2 tu du lieu)

void FLEXCAN0_init(void) {
    uint32_t i = 0;

    // Bat clock cho module FlexCAN0
    PCC->PCCn[PCC_FlexCAN0_INDEX] |= PCC_PCCn_CGC_MASK;

    // Vo hieu hoa module truoc khi cau hinh
    CAN0->MCR |= CAN_MCR_MDIS_MASK;

    // Chon nguon clock 8 MHz tu oscillator
    CAN0->CTRL1 &= ~CAN_CTRL1_CLKSRC_MASK;

    // Kich hoat module tro lai (vao che do freeze de cau hinh)
    CAN0->MCR &= ~CAN_MCR_MDIS_MASK;
    while (!((CAN0->MCR & CAN_MCR_FRZACK_MASK) >> CAN_MCR_FRZACK_SHIFT)) {}

    // Cau hinh toc do CAN 500 kHz (tham so CAN0->CTRL1)
    CAN0->CTRL1 = 0x00DB0006;

    // Xoa toan bo bo dem tin nhan (128 words tuong ung 32 buffer * 4 words)
    for (i = 0; i < 128; i++) {
        CAN0->RAMn[i] = 0;
    }

    // Cau hinh bo loc, cho phep nhan tat ca ID
    for (i = 0; i < 16; i++) {
        CAN0->RXIMR[i] = 0xFFFFFFFF;
    }

    CAN0->RXMGMASK = 0x1FFFFFFF;

    // Cau hinh bo dem 4 nhan tin nhan voi ID chuan, chua kich hoat (CODE=4)
#ifdef NODE_A
    CAN0->RAMn[4*MSG_BUF_SIZE + 1] = 0x14440000; // ID = 0x111 (vi du)
#else
    CAN0->RAMn[4*MSG_BUF_SIZE + 1] = 0x15540000; // ID = 0x555 (vi du)
#endif
    CAN0->RAMn[4*MSG_BUF_SIZE + 0] = 0x04000000; // CODE=4 (RX inactive)

    // Kich hoat module CAN, thoat freeze mode
    CAN0->MCR = 0x0000001F;
    while ((CAN0->MCR & CAN_MCR_FRZACK_MASK) >> CAN_MCR_FRZACK_SHIFT) {}
    while ((CAN0->MCR & CAN_MCR_NOTRDY_MASK) >> CAN_MCR_NOTRDY_SHIFT) {}
}

void FLEXCAN0_transmit_msg(uint8_t buffer[]) {
    // Xoa co cu cua bo dem 0
    CAN0->IFLAG1 = 0x00000001;

#ifdef NODE_A
    // Cau hinh ID chuan cho bo dem 0
    CAN0->RAMn[0*MSG_BUF_SIZE + 1] = 0x15540000; // ID 0x555
#else
    CAN0->RAMn[0*MSG_BUF_SIZE + 1] = 0x14440000; // ID 0x511
#endif

    // Cau hinh bo dem 0 kich hoat truyen voi DLC=8 bytes, CODE=0xC (TX frame)
    CAN0->RAMn[0*MSG_BUF_SIZE + 0] = 0x0C400000 | (8 << CAN_WMBn_CS_DLC_SHIFT);

    // Gan du lieu gui vao RAM bo dem (gop 4 bytes dau thanh 1 word)
    uint32_t dataWord = 0;
    dataWord |= buffer[0];
    dataWord |= ((uint32_t)buffer[1] << 8);
    dataWord |= ((uint32_t)buffer[2] << 16);
    dataWord |= ((uint32_t)buffer[3] << 24);

    CAN0->RAMn[2] = dataWord;  // Ghi du lieu vao word 2 cua MB0 (doan du lieu)
}
