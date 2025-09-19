/*==============================================================================
  文件：KeyEXTI.c
  功能：优化的基于定时器中断的按键处理模块（支持短按、长按、连续按）
  优化：减少消抖时间，提高响应速度，增加边沿检测
==============================================================================*/
#include "stm32f10x.h"
#include "KeyEXTI.h"
#include "Config.h"
#include "LightControl.h"
#include "OLED.h"
#include "Delay.h"

// 优化后的消抖和长按参数
#define DEBOUNCE_TIME_MS    8     // 8ms消抖时间（减少延迟）
#define LONG_PRESS_TIME_MS  1200  // 1.2秒长按判定（减少等待时间）
#define REPEAT_START_MS     200   // 200ms后开始连续触发（更快响应）
#define REPEAT_INTERVAL_MS  80    // 连续触发间隔（更快速度）

// 按键引脚和端口定义
#define KEY_GPIO GPIOB
#define KEY1_PIN GPIO_Pin_5
#define KEY2_PIN GPIO_Pin_6
#define KEY3_PIN GPIO_Pin_7
#define KEY4_PIN GPIO_Pin_8

// 按键编号定义（与原Key.h保持一致）
#define KEY_NONE    0
#define KEY1_PRES   1
#define KEY2_PRES   2
#define KEY3_PRES   3
#define KEY4_PRES   4

// 按键状态结构体
typedef struct {
    uint8_t stable_state;        // 消抖后稳定状态：1未按，0按下
    uint8_t last_raw_state;      // 上次原始采样状态
    uint8_t raw_state;           // 当前原始状态
    uint8_t debounce_counter;    // 消抖计数器（减少位数提高效率）
    uint8_t pressed_flag;        // 按键按下标志
    uint16_t press_time_counter; // 按键按下持续时间计数（ms）
    uint8_t short_press_flag;    // 短按标志
    uint8_t long_press_flag;     // 长按标志
    uint8_t long_press_handled;  // 长按是否已处理标志
    uint8_t repeat_flag;         // 连续触发标志
    uint8_t repeat_counter;      // 连续触发计数器（减少位数）
    uint8_t edge_detected;       // 边沿检测标志（新增）
} KeyState_t;

// 4个按键的状态
static volatile KeyState_t keys[4] = {
    {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},  // KEY1
    {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},  // KEY2
    {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},  // KEY3
    {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}   // KEY4
};

static volatile uint32_t sys_tick_counter = 0;    // 系统时钟计数器（ms）

// 按键引脚数组
static const uint16_t key_pins[4] = {KEY1_PIN, KEY2_PIN, KEY3_PIN, KEY4_PIN};

/**
  * @brief  按键和定时器初始化
  */
void KeyEXTI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 使能GPIO时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    // 配置按键引脚为上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // 初始化定时器
    KeyEXTI_TimerInit();
}

/**
  * @brief  定时器3初始化（1ms中断）
  */
void KeyEXTI_TimerInit(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    // 使能TIM3时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    
    // 配置定时器
    TIM_TimeBaseStructure.TIM_Period = 999;     // 1ms中断周期 (72MHz/72/1000)
    TIM_TimeBaseStructure.TIM_Prescaler = 71;   // 预分频器
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
    
    // 配置中断 - 提高优先级以确保及时响应
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  // 最高优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    // 使能更新中断
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
    
    // 启动定时器
    TIM_Cmd(TIM3, ENABLE);
}

/**
  * @brief  处理单个按键的状态机（优化版）
  * @param  key_idx: 按键索引(0-3)
  */
static void KeyEXTI_ProcessKey(uint8_t key_idx)
{
    volatile KeyState_t* key = &keys[key_idx];
    key->raw_state = GPIO_ReadInputDataBit(KEY_GPIO, key_pins[key_idx]);  // 读取当前电平
    
    // 边沿检测（立即响应按键变化）
    if (key->raw_state != key->last_raw_state) {
        key->edge_detected = 1;
        key->debounce_counter = 0;  // 重置消抖计数器
    }
    
    // 消抖检测 - 使用更灵敏的逻辑
    if (key->raw_state == key->last_raw_state) {
        if (key->debounce_counter < DEBOUNCE_TIME_MS) {
            key->debounce_counter++;
            
            // 达到消抖时间，更新稳定状态
            if (key->debounce_counter == DEBOUNCE_TIME_MS) {
                if (key->stable_state != key->raw_state) {
                    key->stable_state = key->raw_state;
                    
                    if (key->raw_state == 0) {
                        // 按键按下事件
                        key->pressed_flag = 1;
                        key->press_time_counter = 0;
                        key->long_press_flag = 0;
                        key->long_press_handled = 0;
                        key->repeat_flag = 0;
                        key->repeat_counter = 0;
                    } else {
                        // 按键释放事件
                        if (key->pressed_flag && !key->long_press_flag) {
                            // 短按事件
                            key->short_press_flag = 1;
                        }
                        key->pressed_flag = 0;
                        key->press_time_counter = 0;
                        key->repeat_flag = 0;
                        key->repeat_counter = 0;
                    }
                }
            }
        }
    }
    
    // 快速响应模式：如果检测到边沿且消抖时间较短，提前触发
    if (key->edge_detected && key->debounce_counter >= (DEBOUNCE_TIME_MS / 2)) {
        if (key->raw_state == key->last_raw_state && key->stable_state != key->raw_state) {
            key->stable_state = key->raw_state;
            key->debounce_counter = DEBOUNCE_TIME_MS;  // 直接设为消抖完成
            
            if (key->raw_state == 0) {
                // 按键按下事件（快速响应）
                key->pressed_flag = 1;
                key->press_time_counter = 0;
                key->long_press_flag = 0;
                key->long_press_handled = 0;
                key->repeat_flag = 0;
                key->repeat_counter = 0;
            } else {
                // 按键释放事件（快速响应）
                if (key->pressed_flag && !key->long_press_flag) {
                    key->short_press_flag = 1;
                }
                key->pressed_flag = 0;
                key->press_time_counter = 0;
                key->repeat_flag = 0;
                key->repeat_counter = 0;
            }
        }
        key->edge_detected = 0;
    }
    
    key->last_raw_state = key->raw_state;
    
    // 处理按键按下时的计时
    if (key->pressed_flag) {
        key->press_time_counter++;
        
        // 长按检测
        if (!key->long_press_flag && key->press_time_counter >= LONG_PRESS_TIME_MS) {
            key->long_press_flag = 1;
            key->long_press_handled = 0;
        }
        
        // 连续触发检测（用于配置模式）
        if (key->press_time_counter >= REPEAT_START_MS) {
            key->repeat_counter++;
            if (key->repeat_counter >= REPEAT_INTERVAL_MS) {
                key->repeat_counter = 0;
                key->repeat_flag = 1;
            }
        }
    }
}

/**
  * @brief  获取按键事件（类似原Key_GetNum()）
  * @retval 按键编号 (1-4) 或 KEY_NONE
  */
uint8_t KeyEXTI_GetKey(void)
{
    for (uint8_t i = 0; i < 4; i++) {
        if (keys[i].short_press_flag) {
            keys[i].short_press_flag = 0;
            return i + 1;
        }
    }
    return KEY_NONE;
}

/**
  * @brief  获取按键当前状态（是否按下）
  * @param  key_num: 按键编号(1-4)
  * @retval 1-按下, 0-未按下
  */
uint8_t KeyEXTI_GetKeyState(uint8_t key_num)
{
    if (key_num < 1 || key_num > 4) return 0;
    return keys[key_num - 1].pressed_flag;
}

/**
  * @brief  获取按键长按标志
  * @param  key_num: 按键编号(1-4)
  * @retval 1-长按, 0-非长按
  */
uint8_t KeyEXTI_GetLongPress(uint8_t key_num)
{
    if (key_num < 1 || key_num > 4) return 0;
    
    volatile KeyState_t* key = &keys[key_num - 1];
    if (key->long_press_flag && !key->long_press_handled) {
        key->long_press_handled = 1;
        return 1;
    }
    return 0;
}

/**
  * @brief  获取按键连续触发标志（用于配置模式）
  * @param  key_num: 按键编号(1-4)
  * @retval 1-需要触发, 0-不需要
  */
uint8_t KeyEXTI_GetRepeat(uint8_t key_num)
{
    if (key_num < 1 || key_num > 4) return 0;
    
    volatile KeyState_t* key = &keys[key_num - 1];
    if (key->repeat_flag) {
        key->repeat_flag = 0;
        return 1;
    }
    return 0;
}

/**
  * @brief  获取按键按下时间（ms）
  * @param  key_num: 按键编号(1-4)
  * @retval 按下时间
  */
uint16_t KeyEXTI_GetPressTime(uint8_t key_num)
{
    if (key_num < 1 || key_num > 4) return 0;
    return keys[key_num - 1].press_time_counter;
}

/**
  * @brief  重置按键状态
  * @param  key_num: 按键编号(1-4)
  */
void KeyEXTI_ResetKey(uint8_t key_num)
{
    if (key_num < 1 || key_num > 4) return;
    
    volatile KeyState_t* key = &keys[key_num - 1];
    key->short_press_flag = 0;
    key->long_press_flag = 0;
    key->long_press_handled = 0;
    key->repeat_flag = 0;
    key->edge_detected = 0;
}

/**
  * @brief  获取系统时钟计数值（ms）
  */
uint32_t GetTick(void)
{
    return sys_tick_counter;
}

/**
  * @brief  兼容旧接口 - KEY1长按
  */
uint8_t KeyEXTI_GetKey1LongPressFlag(void)
{
    return KeyEXTI_GetLongPress(1);
}

/**
  * @brief  兼容旧接口 - KEY4长按
  */
uint8_t KeyEXTI_GetKey4LongPressFlag(void)
{
    return KeyEXTI_GetLongPress(4);
}

/**
  * @brief  兼容旧接口 - 重置KEY1长按
  */
void KeyEXTI_ResetKey1LongPressFlag(void)
{
    KeyEXTI_ResetKey(1);
}

/**
  * @brief  兼容旧接口 - 重置KEY4长按
  */
void KeyEXTI_ResetKey4LongPressFlag(void)
{
    KeyEXTI_ResetKey(4);
}

/**
  * @brief  TIM3中断服务函数，1ms调用一次（优化版）
  */
void TIM3_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
        
        // 系统时钟计数
        sys_tick_counter++;
        
        // 处理所有按键 - 优化为内联处理提高效率
        for (uint8_t i = 0; i < 4; i++) {
            KeyEXTI_ProcessKey(i);
        }
    }
}
