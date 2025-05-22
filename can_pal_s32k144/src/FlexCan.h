#ifndef FLEXCAN_H_
#define FLEXCAN_H_

#include <stdint.h>

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
void FLEXCAN0_transmit_msg(uint8_t buffer[]);

#endif /* FLEXCAN_H_ */
