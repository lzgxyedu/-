// LDR.h
#ifndef __LDR_H
#define __LDR_H

#include "stm32f10x.h"
#include "adcx.h"
#include "Delay.h"
#include <math.h>

#define LDR_READ_TIMES   10    // ADC ????

// —— ??????? PA7 / ADC_Channel_7 ——
#define LDR_GPIO_CLK     RCC_APB2Periph_GPIOA
#define LDR_GPIO_PORT    GPIOA
#define LDR_GPIO_PIN     GPIO_Pin_7
#define ADC_CHANNEL      ADC_Channel_7
// —— END ——

void LDR_Init(void);
uint16_t LDR_Average_Data(void);
uint16_t LDR_LuxData(void);
/**
  * @brief  ?????????(0~100),????,????
  * @retval ???(0~100)
  */
uint8_t  LDR_Percent(void);

#endif
