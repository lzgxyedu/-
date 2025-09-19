#include "dht11.h"

// DHT11??????
static void DHT11_Mode(uint8_t mode)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    GPIO_InitStructure.GPIO_Pin = DHT11_GPIO_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    
    if(mode == 1) // ????
    {
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    }
    else // ????
    {
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    }
    
    GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStructure);
}

// ??DHT11
static void DHT11_Reset(void)
{
    DHT11_Mode(1); // ?????
    GPIO_ResetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN); // ??
    Delay_ms(20);  // ????18ms
    GPIO_SetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN);   // ??
    Delay_us(30);  // ??20-40us
}

// ??DHT11??
static uint8_t DHT11_Check(void)
{
    uint8_t retry = 0;
    
    DHT11_Mode(0); // ?????
    
    // ??DHT11??(????80us??)
    while(GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) && retry < 100)
    {
        retry++;
        Delay_us(1);
    }
    
    if(retry >= 100) return 1; // ????
    
    retry = 0;
    
    // ??DHT11??(????80us??)
    while(!GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) && retry < 100)
    {
        retry++;
        Delay_us(1);
    }
    
    if(retry >= 100) return 1; // ????
    
    return 0; // ??
}

// ?DHT11?????
static uint8_t DHT11_Read_Bit(void)
{
    uint8_t retry = 0;
    
    // ???????
    while(GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) && retry < 100)
    {
        retry++;
        Delay_us(1);
    }
    
    retry = 0;
    
    // ???????
    while(!GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) && retry < 100)
    {
        retry++;
        Delay_us(1);
    }
    
    // ??40us?????
    Delay_us(40);
    
    // ??????,????1,???0
    if(GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN))
        return 1;
    else
        return 0;
}

// ?DHT11??????
static uint8_t DHT11_Read_Byte(void)
{
    uint8_t i, data = 0;
    
    for(i = 0; i < 8; i++)
    {
        data <<= 1;
        data |= DHT11_Read_Bit();
    }
    
    return data;
}

// ???DHT11
uint8_t DHT11_Init(void)
{
    // ??GPIO??
    RCC_APB2PeriphClockCmd(DHT11_GPIO_CLK, ENABLE);
    
    // ??GPIO??
    DHT11_Mode(1); // ???????
    GPIO_SetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN); // ????????
    
    // ???????
    Delay_ms(1000);
    
    // ?????????
    DHT11_Reset();
    return DHT11_Check();
}

// ?DHT11???????
uint8_t DHT11_Read_Data(uint8_t *temperature, uint8_t *humidity)
{
    uint8_t buf[5];
    uint8_t i;
    
    // ?????????
    DHT11_Reset();
    if(DHT11_Check() == 0)
    {
        // ??40?(5??)??
        for(i = 0; i < 5; i++)
        {
            buf[i] = DHT11_Read_Byte();
        }
        
        // ????
        if((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4])
        {
            *humidity = buf[0];
            *temperature = buf[2];
            return 0; // ??
        }
    }
    
    return 1; // ??
}
