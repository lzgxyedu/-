/*==============================================================================
  文件：CountSensor.c (实时响应版 - 彻底解决速度延迟问题)
==============================================================================*/
#include "stm32f10x.h"
#include "CountSensor.h"

static volatile uint16_t CountSensor_Count = 0;
static volatile uint32_t Last_Pulse_Time = 0;  // 记录最后一次脉冲时间

/**
  * @brief  计数传感器初始化 (PA5)
  */
void CountSensor_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource5);

    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_InitStructure.EXTI_Line    = EXTI_Line5;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel                   = EXTI9_5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_Init(&NVIC_InitStructure);

    CountSensor_Count = 0;
    Last_Pulse_Time = 0;
}

/**
  * @brief  获取当前累计脉冲数
  */
uint16_t CountSensor_Get(void)
{
    return CountSensor_Count;
}

/**
  * @brief  获取最后一次脉冲的时间戳
  */
uint32_t CountSensor_GetLastPulseTime(void)
{
    return Last_Pulse_Time;
}

/**
  * @brief  复位计数（清零）
  */
void CountSensor_Reset(void)
{
    CountSensor_Count = 0;
    Last_Pulse_Time = 0;
}

/**
  * @brief  EXTI9_5 外部中断服务函数 - 简化版
  */
void EXTI9_5_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line5) == SET)
    {
        // 简单消抖：检测引脚状态
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5) == 0) {
            CountSensor_Count++;
            if (CountSensor_Count > 9999) {
                CountSensor_Count = 0;
            }
            // 记录脉冲时间戳（使用SysTick）
            Last_Pulse_Time = SysTick->VAL;
        }
        
        EXTI_ClearITPendingBit(EXTI_Line5);
    }
}
