// ultrasonic.c
#include "ultrasonic.h"
#include "timer.h"
#include "Delay.h"    // ?? Delay_us?Delay_ms

// ?????????,???? ECHO ?????
u16 msHcCount = 0;

/**
  * @brief  HC-SR04 ????????
  *         TRIG: ???? 50MHz
  *         ECHO: ????
  */
void Ultrasonic_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // ?? GPIO ??
    RCC_APB2PeriphClockCmd(ULTRASONIC_GPIO_CLK, ENABLE);

    // ?? TRIG ???????,50MHz
    GPIO_InitStructure.GPIO_Pin   = ULTRASONIC_TRIG_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ULTRASONIC_GPIO_PORT, &GPIO_InitStructure);

    // ?? ECHO ???????
    GPIO_InitStructure.GPIO_Pin   = ULTRASONIC_ECHO_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ULTRASONIC_GPIO_PORT, &GPIO_InitStructure);

    // ??????,???????
    msHcCount = 0;
    TIM4_Int_Init(1000 - 1, 72 - 1);
}

/**
  * @brief  ?????,???? ECHO ?????
  */
static void OpenTimerForHc(void)
{
    TIM_SetCounter(TIM4, 0);
    msHcCount = 0;
    TIM_Cmd(TIM4, ENABLE);
}

/**
  * @brief  ?????
  */
static void CloseTimerForHc(void)
{
    TIM_Cmd(TIM4, DISABLE);
}

/**
  * @brief  ???????(??????)
  * @retval ????? (µs)
  */
u32 GetEchoTimer(void)
{
    u32 t = msHcCount * 1000 + TIM_GetCounter(TIM4);
    TIM4->CNT = 0;
    Delay_ms(50);  // ?????
    return t;
}

/**
  * @brief  ????????(??:cm)
  *         ????????,??????,???? -1.0f
  * @retval ??? (cm) ?????? -1.0f
  */
float UltrasonicGetLength(void)
{
    u32 t;
    float sum = 0.0f;

    for (int i = 0; i < 5; i++)
    {
        // ? 20µs ????
        TRIG_Send = 1;
        Delay_us(20);
        TRIG_Send = 0;

        // ?? ECHO ????,??? 60ms
        u32 timeout = 60000;
        while (ECHO_Reci == 0 && timeout--) {
            Delay_us(1);
        }
        if (timeout == 0) {
            return -1.0f;  // ?????????
        }

        OpenTimerForHc();

        // ?? ECHO ????,??? 60ms
        timeout = 60000;
        while (ECHO_Reci == 1 && timeout--) {
            Delay_us(1);
        }
        CloseTimerForHc();
        if (timeout == 0) {
            return -1.0f;  // ?????????
        }

        // ????????????????? cm
        t = GetEchoTimer();
        sum += ((float)t) / 58.0f;
    }

    return (sum / 5.0f);
}
