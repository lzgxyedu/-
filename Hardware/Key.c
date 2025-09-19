/*==============================================================================
  文件：Key.c
  功能：按键初始化及消抖读取（PB5–PB8）
==============================================================================*/
#include "stm32f10x.h"
#include "Delay.h"
#include "Key.h"
#include "KeyEXTI.h"

// 按键去抖动时间调整为500us
#define KEY_DEBOUNCE_TIME  0.5  // 500us消抖时间

// 按键状态变量
static uint8_t key_last_state[4] = {1, 1, 1, 1}; // 按键的上一次状态
static uint8_t key_release_flag[4] = {0, 0, 0, 0}; // 按键释放标志
static uint32_t key_release_time[4] = {0, 0, 0, 0}; // 按键释放时间

void Key_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

uint8_t Key_GetNum(void)
{
    // 读取当前按键状态
    uint8_t key1_state = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_5);
    uint8_t key2_state = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_6);
    uint8_t key3_state = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7);
    uint8_t key4_state = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8);
    
    uint8_t current_state[4] = {key1_state, key2_state, key3_state, key4_state};
    uint8_t key = KEY_NONE;
    uint32_t current_time = GetTick();
    
    // 遍历4个按键
    for (uint8_t i = 0; i < 4; i++) {
        // 按键按下（从高电平变为低电平）
        if (current_state[i] == 0 && key_last_state[i] == 1) {
            key_release_flag[i] = 0; // 清除释放标志
        }
        
        // 按键释放（从低电平变为高电平）
        if (current_state[i] == 1 && key_last_state[i] == 0) {
            key_release_flag[i] = 1; // 设置释放标志
            key_release_time[i] = current_time; // 记录释放时间
        }
        
        // 检测有效按键释放（防止抖动）
        if (key_release_flag[i] && (current_time - key_release_time[i] >= KEY_DEBOUNCE_TIME)) {
            key = i + 1; // KEY1_PRES=1, KEY2_PRES=2, ...
            key_release_flag[i] = 0; // 清除释放标志，防止重复触发
        }
        
        // 更新按键状态
        key_last_state[i] = current_state[i];
    }
    
    return key;
}

// 获取按键当前状态（不经过消抖处理）
uint8_t Key_GetState(uint8_t key_num)
{
    if (key_num < 1 || key_num > 4) return 0;
    
    uint8_t state = 0;
    switch(key_num) {
        case 1: state = !GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_5); break; // KEY1
        case 2: state = !GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_6); break; // KEY2
        case 3: state = !GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7); break; // KEY3
        case 4: state = !GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8); break; // KEY4
    }
    
    return state;
}
