// === Delay.h ===
#ifndef __DELAY_H
#define __DELAY_H

#include <stdint.h>

// 延时模块初始化，需在 SystemCoreClockUpdate() 后调用
void Delay_Init(void);

// 微秒级延时
void Delay_us(uint32_t us);

// 毫秒级延时
void Delay_ms(uint32_t ms);

// 秒级延时
void Delay_s(uint32_t s);

#endif // __DELAY_H


// === Delay.c ===
#include "stm32f10x.h"
#include "system_stm32f10x.h"
#include "Delay.h"

static uint32_t fac_us;  // 微秒延时时钟倍数
static uint32_t fac_ms;  // 毫秒延时时钟倍数

/**
  * @brief  延时模块初始化
  * @note   必须在 SystemCoreClockUpdate() 之后调用
  */
void Delay_Init(void)
{
    // 配置 SysTick 时钟源为 HCLK
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
    // 计算延时倍数
    fac_us = SystemCoreClock / 1000000;
    fac_ms = fac_us * 1000;
}

/**
  * @brief  微秒延时
  * @param  us: 需要延时的微秒数
  */
void Delay_us(uint32_t us)
{
    uint32_t cnt = us * fac_us;
    // 大于 24 位最大值分段延时
    while (cnt > 0x00FFFFFF)
    {
        SysTick->LOAD = 0x00FFFFFF;
        SysTick->VAL  = 0;
        SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
        while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
        cnt -= 0x00FFFFFF;
    }
    SysTick->LOAD = cnt;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
    SysTick->CTRL = 0;
    SysTick->VAL  = 0;
}

/**
  * @brief  毫秒延时
  * @param  ms: 需要延时的毫秒数
  */
void Delay_ms(uint32_t ms)
{
    uint32_t cnt = ms * fac_ms;
    while (cnt > 0x00FFFFFF)
    {
        SysTick->LOAD = 0x00FFFFFF;
        SysTick->VAL  = 0;
        SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
        while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
        cnt -= 0x00FFFFFF;
    }
    SysTick->LOAD = cnt;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
    SysTick->CTRL = 0;
    SysTick->VAL  = 0;
}

/**
  * @brief  秒级延时
  * @param  s: 需要延时的秒数
  */
void Delay_s(uint32_t s)
{
    while (s--)
    {
        Delay_ms(1000);
    }
}
