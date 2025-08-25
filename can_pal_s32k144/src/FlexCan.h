#ifndef FLEXCAN_H
#define FLEXCAN_H

#define RX_MB_INDEX  0UL
#define TX_MB_INDEX  1UL

#define RX_MSG_ID_UDS    0x769
#define TX_MSG_ID_UDS    0x768

#define MSG_BUF_SIZE 4

typedef struct {
    uint32_t canID;
    uint8_t  dlc;
    uint8_t  data[8];
} CAN_Message_t;

void FLEXCAN0_init(void);
void FLEXCAN0_transmit_msg(const CAN_Message_t *msg);
int  FLEXCAN0_receive_msg(CAN_Message_t *msg, uint32_t expectedID);

#endif /* FLEXCAN_H */
