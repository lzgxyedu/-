/*==============================================================================
  文件：Config.h
  功能：智能车灯控制系统参数配置界面（适用于STM32F103C8T6）
==============================================================================*/
#ifndef __CONFIG_H
#define __CONFIG_H
#include <stdio.h>  // 添加这一行以使用sprintf

#include <stdint.h>

// 首次进入标志
extern uint8_t first_entry_flag;

// 参数类型定义 - 仅保留四个需要的参数
typedef enum {
    PARAM_LUX = 0,    // 光照阈值
    PARAM_SPD,        // 车速阈值
    PARAM_DIS,        // 距离阈值
    PARAM_HUM,        // 湿度阈值
    PARAM_COUNT       // 参数总数 - 现在是4
} ConfigParam_t;

// 参数结构体
typedef struct {
    char name[4];     // 参数名称（3个字符+结束符）
    char symbol;      // OLED右下角显示字母
    uint8_t value;    // 参数值（0-100）
    uint8_t default_value; // 默认值
    char description[20];  // 参数描述
} ParamConfig_t;

// 函数声明
void Config_Init(void);
void Config_EnterConfigMode(void);
void Config_ExitConfigMode(void);
void Config_HandleKeys(void);
void Config_UpdateDisplay(void);
uint8_t Config_IsInConfigMode(void);
uint8_t Config_GetParamValue(ConfigParam_t param);
uint8_t Config_GetTempParamValue(ConfigParam_t param);
void Config_CommitTempParams(void);
uint8_t Config_SaveParams(void);
void Config_LoadParams(void);
void Config_ResetToDefaults(void);
void Config_ProcessTimeout(void);

#endif // __CONFIG_H
