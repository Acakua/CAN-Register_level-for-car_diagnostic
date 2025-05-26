#include "sdk_project_config.h"
#include <interrupt_manager.h>
#include <stdint.h>
#include <stdbool.h>
#include "adc_pal.h"
#include "adc_pal_cfg.h"
#include "FlexCan.h"

#define TX_MAILBOX  (1UL)
#define TX_MSG_ID   (0x7E8UL)
#define RX_MAILBOX  (0UL)
#define RX_MSG_ID   (0x7DFUL)

void ADCInit(void)
{

    PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;


    PCC->PCCn[PCC_ADC0_INDEX] = PCC_PCCn_PCS(1) | PCC_PCCn_CGC_MASK;


    PORTC->PCR[14] = (PORTC->PCR[14] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(0);


    ADC0->SC3 = ADC_SC3_CAL_MASK;
    while (ADC0->SC3 & ADC_SC3_CAL_MASK) { }

    ADC0->SC1[0] = ADC_SC1_ADCH(31);


    ADC0->CFG1 =
        (0 << 0) |  // ADICLK = 0
        (1 << 2) |  // MODE = 12-bit
        (0 << 4) |  // ADLSMP = short
        (0 << 5);   // ADIV = 1

    ADC0->SC2 = 0;
}



uint16_t ReadADCValue(void)
{
    ADC0->SC1[0] = ADC_SC1_ADCH(12);
    while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0);
    //return 0x1234;
    return (uint16_t)(ADC0->R[0]);
}


void BoardInit(void)
{
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);
    ADCInit();
}

volatile int exit_code = 0;
can_message_t rxMsg;

void CheckCANRequest(void)
{
    status_t status = CAN_Receive(&can_pal1_instance, RX_MAILBOX, &rxMsg);

    if (status == STATUS_SUCCESS)
    {
        while (CAN_GetTransferStatus(&can_pal1_instance, RX_MAILBOX) == STATUS_BUSY);

        if (rxMsg.id != RX_MSG_ID) return;
        uint8_t len  = rxMsg.data[0];
        uint8_t sid  = rxMsg.data[1];
        uint8_t didh = rxMsg.data[2];
        uint8_t didl = rxMsg.data[3];

        // phan hoi khi loi
        can_message_t negRsp = {
            .cs     = 0U,
            .id     = TX_MSG_ID,
            .length = 4U,
            .data   = { 0x03, 0x7F, sid, 0x00 }
        };

        // NRC = 0x13: Incorrect message length
        if (len != 3)
        {
            negRsp.data[3] = 0x13;
            CAN_Send(&can_pal1_instance, TX_MAILBOX, &negRsp);
            return;
        }

        // NRC = 0x11: Service not supported
        if (sid != 0x22)
        {
            negRsp.data[3] = 0x11;
            CAN_Send(&can_pal1_instance, TX_MAILBOX, &negRsp);
            return;
        }

        // NRC = 0x31: Request out of range
        if (didh != 0xF1 || didl != 0x90)
        {
            negRsp.data[3] = 0x31;
            CAN_Send(&can_pal1_instance, TX_MAILBOX, &negRsp);
            return;
        }


        if ((rxMsg.id == RX_MSG_ID) &&
            (rxMsg.length == 4) &&
            (rxMsg.data[0] == 0x03) &&
            (rxMsg.data[1] == 0x22) &&
            (rxMsg.data[2] == 0xF1) &&
            (rxMsg.data[3] == 0x90))
        {
            uint16_t adcVal = ReadADCValue();

            can_message_t txMsg = {
                .cs     = 0U,
                .id     = TX_MSG_ID,
                .length = 6U,
                .data   = {
                    0x05,       // Length
                    0x62,       // Positive Response SID
                    0xF1,       // DID high byte
                    0x90,       // DID low byte
                    (uint8_t)(adcVal & 0xFF),         // ADC Low Byte
                    (uint8_t)((adcVal >> 8) & 0xFF)   // ADC High Byte
                }
            };

            CAN_Send(&can_pal1_instance, TX_MAILBOX, &txMsg);
        }
    }
}


int main(void)
{
    BoardInit();
    CAN_Init(&can_pal1_instance, &can_pal1_Config0);

    can_buff_config_t txBuffCfg = {
        .enableFD = false,
        .enableBRS = false,
        .fdPadding = 0U,
        .idType = CAN_MSG_ID_STD,
        .isRemote = false
    };

    CAN_ConfigTxBuff(&can_pal1_instance, TX_MAILBOX, &txBuffCfg);

    can_buff_config_t rxBuffCfg = {
        .enableFD   = false,
        .enableBRS  = false,
        .fdPadding  = 0U,
        .idType     = CAN_MSG_ID_STD,
        .isRemote   = false
    };
    CAN_ConfigRxBuff(&can_pal1_instance, RX_MAILBOX, &rxBuffCfg, RX_MSG_ID);

    while(1)
    {
        CheckCANRequest();

    }

    return exit_code;
}
