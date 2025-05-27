#ifndef FLEXCAN_H_
#define FLEXCAN_H_

#include <stdint.h>
typedef struct {
    uint32_t canID;
    uint8_t dlc;
    uint8_t data[8];
} CAN_Message_t;

/*
 * Khoi tao module FlexCAN0 cho S32K144
 * Cau hinh toc do CAN 500Kbps, 1 bo dem nhan tin (MB4) voi ID chuan (ID phu thuoc NODE_A)
 */
void FLEXCAN0_init(void);

/*
 * Ham truyen tin qua FlexCAN0
 * Tham so:
 *   buffer: mang byte du lieu can gui (toi da 4 byte trong truong hop nay)
 * Yeu cau:
 *   Bo dem tin nhan phai o trang thai INACTIVE truoc khi goi ham
 */
void FLEXCAN0_transmit_msg(const CAN_Message_t *msg);


#endif /* FLEXCAN_H_ */
