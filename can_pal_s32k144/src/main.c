#include "sdk_project_config.h"
#include <interrupt_manager.h>
#include <FlexCan.h>
#include <stdint.h>
#include "adc_pal.h"
#include "adc_pal_cfg.h"


#define INCORRECT_LENGTH        (0X13UL)
#define SERVICE_NOT_SUPPORTED   (0X11UL)
#define REQUEST_OUT_OF_RANGE    (0X31UL)

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

 void CheckCANRequest(void)
 {
	 CAN_Message_t msg_rx;
     int status = FLEXCAN0_receive_msg(&msg_rx);

     if (status == 1)
     {
    	 CAN_Message_t msg_tx = {
    			 .canID = TX_MSG_ID,
				 .dlc = 4,
				 .data[0] = 0x03,
				 .data[1] = 0x7F,
				 .data[2] = msg_rx.data[1],
				 .data[3] = 0x00
    	 };
         // NRC = 0x13: Incorrect message length
         if (msg_rx.data[0] != 3 || msg_rx.dlc < 4)
         {
             msg_tx.data[3] = INCORRECT_LENGTH;
             FLEXCAN0_transmit_msg(&msg_tx);
             return;
         }

         // NRC = 0x11: Service not supported
         if (msg_rx.data[1] != 0x22)
         {
        	 msg_tx.data[3] = SERVICE_NOT_SUPPORTED;
        	 FLEXCAN0_transmit_msg(&msg_tx);
             return;
         }

         // NRC = 0x31: Request out of range
         if (msg_rx.data[2] != 0xF1 || msg_rx.data[3] != 0x90)
         {
        	 msg_tx.data[3] = REQUEST_OUT_OF_RANGE;
        	 FLEXCAN0_transmit_msg(&msg_tx);
             return;
         }


         if ((msg_rx.data[0] == 0x03) &&
             (msg_rx.data[1] == 0x22) &&
             (msg_rx.data[2] == 0xF1) &&
             (msg_rx.data[3] == 0x90))
         {
             uint16_t adcVal = ReadADCValue();
             CAN_Message_t msg_tx2 = {
                 			 .canID = TX_MSG_ID,
             				 .dlc = 6,
             				 .data[0] = 0x05,
             				 .data[1] = 0x62,
             				 .data[2] = 0xF1,
             				 .data[3] = 0x90,
							 .data[4] = (uint8_t)(adcVal & 0xFF),
							 .data[5] = (uint8_t)((adcVal >> 8) & 0xFF)
                 	 };


             FLEXCAN0_transmit_msg(&msg_tx2);
         }
     }
 }


 int main(void)
 {
     BoardInit();
     FLEXCAN0_init();

     while(1)
     {
         CheckCANRequest();

     }

     return exit_code;
 }

