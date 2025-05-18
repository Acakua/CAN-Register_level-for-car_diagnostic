#include "sdk_project_config.h"    // dung Config Tools sinh
#include "adc_driver.h"
#include "flexcan_driver.h"
#include <stdint.h>
#include <stdbool.h>

/* --- Định nghĩa cố định --- */
#define ADC_INSTANCE       0U      // ADC0
#define ADC_GROUP          0U      // groupConfig đã tạo
#define CAN_TX_BUFFER      1U      // buffer 1 trên CAN1
#define CAN_TX_MSG_ID      0x123U  // ID frame

static void BoardInit(void)
{
    /* 1. Clock + PCC (ADC0, CAN1) */
    CLOCK_DRV_Init(&clockMan1_InitConfig0);

    /* 2. PinMux (ADC0_SE12, CAN1_TX, CAN1_RX) */
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);
}

static void ADCInit(void)
{
    /* Init ADC module và group */
    ADC_DRV_Init(ADC_INSTANCE, &adc0_cfg_0);
    ADC_DRV_InitGroup(ADC_INSTANCE, ADC_GROUP, &adc0_groupConfig0);
}

static void CANInit(void)
{
    /* Init FlexCAN1 với cấu hình từ Config Tools */
    CAN_Init(&can_pal1_instance, &can_pal1_Config0);
}

int main(void)
{
    uint16_t adcRaw;
    can_message_t msg = {
        .id     = CAN_TX_MSG_ID,
        .length = 2,
        .cs     = 0
    };

    /* --- Khởi tạo --- */
    BoardInit();
    ADCInit();
    CANInit();

    while (1)
    {
        /* --- Đọc ADC --- */
        ADC_DRV_StartGroupConversion(ADC_INSTANCE, ADC_GROUP);
        while (!ADC_DRV_GetConvCompleteFlag(ADC_INSTANCE, ADC_GROUP));
        ADC_DRV_ClearConvCompleteFlag(ADC_INSTANCE, ADC_GROUP);
        ADC_DRV_GetGroupConversionResult(ADC_INSTANCE, ADC_GROUP, &adcRaw);

        /* --- Gửi CAN --- */
        msg.data[0] = (uint8_t)(adcRaw >> 8);
        msg.data[1] = (uint8_t)(adcRaw & 0xFF);
        CAN_Send(&can_pal1_instance, CAN_TX_BUFFER, &msg);

        /* --- Delay đơn giản --- */
        for (volatile uint32_t d = 0; d < 1000000; d++) {
            __asm("NOP");
        }
    }

    return 0;
}
