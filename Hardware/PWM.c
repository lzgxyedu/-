/*==============================================================================
  文件：PWM.c
  功能：PWM 初始化及占空比设置
==============================================================================*/
#include "stm32f10x.h"
#include "PWM.h"

void PWM_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TIM_TimeBaseInitTypeDef tb;
    tb.TIM_Period        = 100-1;
    tb.TIM_Prescaler     = 720-1;
    tb.TIM_ClockDivision = TIM_CKD_DIV1;
    tb.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &tb);

    TIM_OCInitTypeDef oc;
    TIM_OCStructInit(&oc);
    oc.TIM_OCMode      = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_OCPolarity  = TIM_OCPolarity_High;
    oc.TIM_Pulse       = 0;
    TIM_OC1Init(TIM2,&oc);
    TIM_OC2Init(TIM2,&oc);
    TIM_OC3Init(TIM2,&oc);
    TIM_OC4Init(TIM2,&oc);

    TIM_Cmd(TIM2, ENABLE);
}

void PWM_SetCompare1(uint16_t C){ TIM_SetCompare1(TIM2,C); }
void PWM_SetCompare2(uint16_t C){ TIM_SetCompare2(TIM2,C); }
void PWM_SetCompare3(uint16_t C){ TIM_SetCompare3(TIM2,C); }
void PWM_SetCompare4(uint16_t C){ TIM_SetCompare4(TIM2,C); }
