#include "sdk_project_config.h"
#include "motor.h"
#include "MatrixLed.h"
#include "lpit.h"

int main(void)
{
    Motor_Init();
    Matrix_Init();
    LPIT0_init();

    for (int i=0;i<8;i++) {
        Matrix_SetPixel(i,i,1);
        Matrix_SetPixel(7-i,i,1);
    }

    while(1)
    {
        Motor_Forward();
        Motor_SetSpeed(700);
        for (volatile int i=0; i<800000; i++);
        Motor_Stop();
        for (volatile int i=0; i<800000; i++);
    }
}
