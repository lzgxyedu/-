/*==============================================================================
  文件：Config.c
  功能：智能车灯控制系统参数配置界面实现（修复OLED显示）
==============================================================================*/
#include "stm32f10x.h"
#include "stm32f10x_flash.h"
#include <stdio.h>
#include <string.h>
#include "Delay.h"
#include "OLED.h"
#include "Config.h"
#include "KeyEXTI.h"

// 首次进入标志
uint8_t first_entry_flag = 1;

// 灯光控制模式定义
#define LIGHT_MODE_AUTO     0
#define LIGHT_MODE_MANUAL   1
#define LIGHT_MODE_CONFIG   2

// Flash配置
#define FLASH_CONFIG_PAGE_ADDR   0x0800F800
#define FLASH_CONFIG_SIZE        1024

// 配置模式相关变量
static uint8_t inConfigMode = 0;
static uint8_t currentParamIndex = 0;
static uint8_t blinkState = 0;
static uint32_t lastBlinkTime = 0;
static uint32_t configStartTime = 0;
static uint8_t displayOffset = 0;
static uint8_t display_initialized = 0;

// 临时参数数组
static uint8_t tempParamValues[PARAM_COUNT];

// 参数配置定义
static ParamConfig_t params[PARAM_COUNT] = {
    {"LUX", 'L', 60, 60, "Light threshold"},
    {"SPD", 'S', 40, 40, "Speed threshold"},
    {"DIS", 'D', 50, 50, "Distance threshold"},
    {"HUM", 'H', 85, 85, "Humidity threshold"}
};

// 外部函数声明
extern void Redraw_OLED_Labels(void);

/**
  * @brief  配置模块初始化
  */
void Config_Init(void)
{
    Config_LoadParams();
    
    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        tempParamValues[i] = params[i].value;
    }
}

/**
  * @brief  进入配置模式
  */
void Config_EnterConfigMode(void)
{
    inConfigMode = 1;
    currentParamIndex = 0;
    configStartTime = 0;
    blinkState = 0;
    lastBlinkTime = 0;
    displayOffset = 0;
    display_initialized = 0;
    
    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        tempParamValues[i] = params[i].value;
    }
    
    OLED_Clear();
    first_entry_flag = 1;
}

/**
  * @brief  退出配置模式
  */
void Config_ExitConfigMode(void)
{
    inConfigMode = 0;
    OLED_Clear();
    // 退出配置模式后重新绘制主界面标签
    Redraw_OLED_Labels();
}

/**
  * @brief  获取临时参数值
  */
uint8_t Config_GetTempParamValue(ConfigParam_t param)
{
    if (param < PARAM_COUNT) {
        return tempParamValues[param];
    }
    return 0;
}

/**
  * @brief  将临时参数设置为正式参数
  */
void Config_CommitTempParams(void)
{
    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        params[i].value = tempParamValues[i];
    }
}

/**
  * @brief  处理配置模式按键操作 - 使用KeyEXTI
  */
void Config_HandleKeys(void)
{
    // 获取短按事件
    uint8_t key = KeyEXTI_GetKey();
    
    if (key != KEY_NONE) {
        // 重置超时计时器
        configStartTime = 0;
        
        switch (key) {
            case KEY1_PRES:  // 当前参数 +1
                tempParamValues[currentParamIndex]++;
                if (tempParamValues[currentParamIndex] > 100) {
                    tempParamValues[currentParamIndex] = 0;
                }
                break;
                
            case KEY2_PRES:  // 当前参数 -1
                if (tempParamValues[currentParamIndex] > 0) {
                    tempParamValues[currentParamIndex]--;
                } else {
                    tempParamValues[currentParamIndex] = 100;
                }
                break;
                
            case KEY3_PRES:  // 下一参数
                currentParamIndex = (currentParamIndex + 1) % PARAM_COUNT;
                first_entry_flag = 1;
                break;
                
            default:
                break;
        }
    }
    
    // 处理长按连续调整 - 使用KeyEXTI的新接口
    
    // KEY1长按连续增加
    if (KeyEXTI_GetKeyState(1)) {  // 按键处于按下状态
        uint16_t press_time = KeyEXTI_GetPressTime(1);
        
        if (press_time > 300) {  // 300ms后开始连续
            // 使用连续触发事件
            if (KeyEXTI_GetRepeat(1)) {
                tempParamValues[currentParamIndex]++;
                if (tempParamValues[currentParamIndex] > 100) {
                    tempParamValues[currentParamIndex] = 0;
                }
                configStartTime = 0;  // 重置超时
            }
        }
    }
    
    // KEY2长按连续减少
    if (KeyEXTI_GetKeyState(2)) {
        uint16_t press_time = KeyEXTI_GetPressTime(2);
        
        if (press_time > 300) {
            if (KeyEXTI_GetRepeat(2)) {
                if (tempParamValues[currentParamIndex] > 0) {
                    tempParamValues[currentParamIndex]--;
                } else {
                    tempParamValues[currentParamIndex] = 100;
                }
                configStartTime = 0;
            }
        }
    }
    
    // KEY3长按连续切换参数
    if (KeyEXTI_GetKeyState(3)) {
        uint16_t press_time = KeyEXTI_GetPressTime(3);
        
        if (press_time > 500) {  // 500ms后开始连续切换
            static uint32_t last_switch_time = 0;
            uint32_t current_time = GetTick();
            
            if (current_time - last_switch_time >= 200) {  // 每200ms切换一次
                currentParamIndex = (currentParamIndex + 1) % PARAM_COUNT;
                first_entry_flag = 1;
                last_switch_time = current_time;
                configStartTime = 0;
            }
        }
    }
}

/**
  * @brief  更新配置界面显示
  */
void Config_UpdateDisplay(void)
{
    static uint8_t first_entry = 1;
    static uint8_t last_values[PARAM_COUNT] = {0};
    static uint8_t last_param_index = 0xFF;
    static uint8_t last_blink_state = 0xFF;
    
    if (first_entry_flag) {
        first_entry = 1;
        first_entry_flag = 0;
    }
    
    if (!display_initialized) {
        OLED_Clear();
        display_initialized = 1;
        first_entry = 1;
    }
    
    if (first_entry) {
        first_entry = 0;
        OLED_Clear();
        
        uint8_t displayLines = (PARAM_COUNT < 5) ? PARAM_COUNT : 4;
        
        for (uint8_t i = 0; i < displayLines; i++) {
            uint8_t paramIdx = i + displayOffset;
            if (paramIdx >= PARAM_COUNT) break;
            
            char unit[10] = "";
            switch (paramIdx) {
                case PARAM_LUX:  strcpy(unit, "lx"); break;
                case PARAM_SPD:  strcpy(unit, "cm/s"); break;
                case PARAM_DIS:  strcpy(unit, "cm"); break;
                case PARAM_HUM:  strcpy(unit, "%"); break;
                default: break;
            }
            
            char buf[20];
            sprintf(buf, "%s = %3d %s", params[paramIdx].name, 
                    tempParamValues[paramIdx], unit);
            OLED_ShowString(i + 1, 3, buf);
        }
        
        uint8_t row = currentParamIndex - displayOffset + 1;
        if (row >= 1 && row <= 4) {
            OLED_ShowString(row, 1, ">");
        }
        
        OLED_ShowChar(4, 16, params[currentParamIndex].symbol);
        
        for (uint8_t i = 0; i < PARAM_COUNT; i++) {
            last_values[i] = tempParamValues[i];
        }
        last_param_index = currentParamIndex;
        last_blink_state = blinkState;
        return;
    }
    
    // 闪烁处理
    uint32_t currentTime = GetTick();
    if (currentTime - lastBlinkTime >= 200) {
        blinkState = !blinkState;
        lastBlinkTime = currentTime;
    }
    
    // 参数索引变化时更新
    if (last_param_index != currentParamIndex) {
        uint8_t old_row = last_param_index - displayOffset + 1;
        if (old_row >= 1 && old_row <= 4) {
            OLED_ShowString(old_row, 1, " ");
        }
        
        if (currentParamIndex < displayOffset || currentParamIndex >= displayOffset + 4) {
            displayOffset = (currentParamIndex / 4) * 4;
            first_entry = 1;
            Config_UpdateDisplay();
            return;
        }
        
        uint8_t row = currentParamIndex - displayOffset + 1;
        if (row >= 1 && row <= 4) {
            if (blinkState) {
                OLED_ShowString(row, 1, ">");
            } else {
                OLED_ShowString(row, 1, " ");
            }
        }
        
        OLED_ShowChar(4, 16, params[currentParamIndex].symbol);
        last_param_index = currentParamIndex;
    }
    
    // 数值变化时更新
    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        if (last_values[i] != tempParamValues[i]) {
            if (i >= displayOffset && i < displayOffset + 4) {
                uint8_t row = i - displayOffset + 1;
                char buf[4];
                sprintf(buf, "%3d", tempParamValues[i]);
                char unit_buf[20];
                char unit[10] = "";
                switch (i) {
                    case PARAM_LUX:  strcpy(unit, "lx"); break;
                    case PARAM_SPD:  strcpy(unit, "cm/s"); break;
                    case PARAM_DIS:  strcpy(unit, "cm"); break;
                    case PARAM_HUM:  strcpy(unit, "%"); break;
                    default: break;
                }
                sprintf(unit_buf, "%3d %s", tempParamValues[i], unit);
                OLED_ShowString(row, 7, "      ");
                OLED_ShowString(row, 7, unit_buf);
            }
            last_values[i] = tempParamValues[i];
        }
    }
    
    // 闪烁状态变化时更新
    if (last_blink_state != blinkState) {
        uint8_t row = currentParamIndex - displayOffset + 1;
        if (row >= 1 && row <= 4 && (currentParamIndex >= displayOffset && currentParamIndex < displayOffset + 4)) {
            if (blinkState) {
                OLED_ShowString(row, 1, ">");
            } else {
                OLED_ShowString(row, 1, " ");
            }
        }
        last_blink_state = blinkState;
    }
}

/**
  * @brief  检查是否处于配置模式
  */
uint8_t Config_IsInConfigMode(void)
{
    return inConfigMode;
}

/**
  * @brief  获取参数值
  */
uint8_t Config_GetParamValue(ConfigParam_t param)
{
    if (param < PARAM_COUNT) {
        return params[param].value;
    }
    if (param == 4) return 10;  // 兼容旧TMP参数
    return 0;
}

/**
  * @brief  保存参数到Flash
  */
uint8_t Config_SaveParams(void)
{
    Config_CommitTempParams();
    
    uint32_t addr = FLASH_CONFIG_PAGE_ADDR;
    uint32_t data;
    uint8_t success = 1;
    
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    
    if (FLASH_ErasePage(FLASH_CONFIG_PAGE_ADDR) != FLASH_COMPLETE) {
        success = 0;
        goto end;
    }
    
    data = 0x504D4150;  // "PMAP"
    if (FLASH_ProgramWord(addr, data) != FLASH_COMPLETE) {
        success = 0;
        goto end;
    }
    addr += 4;
    
    data = 0x00000001;  // 版本1
    if (FLASH_ProgramWord(addr, data) != FLASH_COMPLETE) {
        success = 0;
        goto end;
    }
    addr += 4;
    
    data = PARAM_COUNT;
    if (FLASH_ProgramWord(addr, data) != FLASH_COMPLETE) {
        success = 0;
        goto end;
    }
    addr += 4;
    
    data = 0;
    for (uint8_t j = 0; j < PARAM_COUNT; j++) {
        data |= ((uint32_t)params[j].value << (j * 8));
    }
    
    if (FLASH_ProgramWord(addr, data) != FLASH_COMPLETE) {
        success = 0;
        goto end;
    }
    
end:
    FLASH_Lock();
    return success;
}

/**
  * @brief  从Flash加载参数
  */
void Config_LoadParams(void)
{
    uint32_t addr = FLASH_CONFIG_PAGE_ADDR;
    uint32_t data;
    
    data = *(volatile uint32_t*)addr;
    if (data != 0x504D4150) {
        Config_ResetToDefaults();
        return;
    }
    addr += 4;
    
    data = *(volatile uint32_t*)addr;
    if (data != 0x00000001) {
        Config_ResetToDefaults();
        return;
    }
    addr += 4;
    
    data = *(volatile uint32_t*)addr;
    if (data != PARAM_COUNT && data != 5) {
        Config_ResetToDefaults();
        return;
    }
    addr += 4;
    
    data = *(volatile uint32_t*)addr;
    
    for (uint8_t j = 0; j < PARAM_COUNT; j++) {
        uint8_t value = (data >> (j * 8)) & 0xFF;
        if (value <= 100) {
            params[j].value = value;
        } else {
            params[j].value = params[j].default_value;
        }
    }
    
    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        tempParamValues[i] = params[i].value;
    }
}

/**
  * @brief  重置参数到默认值
  */
void Config_ResetToDefaults(void)
{
    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        params[i].value = params[i].default_value;
        tempParamValues[i] = params[i].default_value;
    }
}

/**
  * @brief  处理配置模式超时
  */
void Config_ProcessTimeout(void)
{
    static uint32_t last_call_time = 0;
    uint32_t current_time = GetTick();
    
    if (configStartTime == 0) {
        last_call_time = current_time;
    }

    if (current_time - last_call_time >= 500) {
        last_call_time = current_time;
        
        if (inConfigMode) {
             configStartTime++;
        }

        if (configStartTime >= 40) {  // 20秒超时
            Config_CommitTempParams();
            if (Config_SaveParams()) {
                OLED_Clear();
                OLED_ShowString(3, 1, "AUTO SAVE OK");
                Delay_ms(1000);
            } else {
                OLED_Clear();
                OLED_ShowString(3, 1, "AUTO SAVE FAIL");
                Delay_ms(1000);
            }
            Config_ExitConfigMode();
            // 这里会自动调用Redraw_OLED_Labels()
        }
    }
}
