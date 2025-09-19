// LDR.c
#include "LDR.h"

// ???? + ADC ???
void LDR_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(LDR_GPIO_CLK, ENABLE);

    GPIO_InitStructure.GPIO_Pin  = LDR_GPIO_PIN;    // PA7
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;   // ????
    GPIO_Init(LDR_GPIO_PORT, &GPIO_InitStructure);

    ADCx_Init();
}

// ?? ADC ??
static uint16_t LDR_ADC_Read(void)
{
    return ADC_GetValue(ADC_CHANNEL, ADC_SampleTime_55Cycles5);
}

// ???????
uint16_t LDR_Average_Data(void)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < LDR_READ_TIMES; i++)
    {
        sum += LDR_ADC_Read();
        Delay_ms(5);
    }
    return (uint16_t)(sum / LDR_READ_TIMES);
}

// ? ADC ????????(Lux)
uint16_t LDR_LuxData(void)
{
    float voltage = LDR_Average_Data() * (3.3f / 4096.0f);
    float R = voltage / (3.3f - voltage) * 10000.0f;
    uint16_t lux = (uint16_t)(40000.0f * powf(R, -0.6021f));
    return (lux > 999 ? 999 : lux);
}

// ? 0~999 Lux ????? 0~100%,????????
uint8_t LDR_Percent(void)
{
    uint16_t lux = LDR_LuxData();      // 0~999
    return (uint8_t)((lux * 100UL + 499UL) / 999UL); // ??????? 0~100
}
