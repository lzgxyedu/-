/*==============================================================================
  文件：LightControl.c (修复OLED显示问题)
==============================================================================*/
#include "stm32f10x.h"
#include "PWM.h"
#include "KeyEXTI.h"
#include "Delay.h"
#include "Config.h"
#include "OLED.h"

// 控制模式定义
#define LIGHT_MODE_AUTO     0
#define LIGHT_MODE_MANUAL   1
#define LIGHT_MODE_CONFIG   2

// 光照阈值
#define LIGHT_THRESHOLD     60

// 灯光等级 (占空比)
#define LOW_BEAM_LEVEL1     40
#define LOW_BEAM_LEVEL2     60
#define LOW_BEAM_LEVEL3     80

#define HIGH_BEAM_LEVEL1    50
#define HIGH_BEAM_LEVEL2    75
#define HIGH_BEAM_LEVEL3    100

// 隧道检测
#define LIGHT_DROP_THRESHOLD 30
#define TUNNEL_FLAG_TIME     3

// 车速 (cm/s)
#define SPEED_LOW           10
#define SPEED_HIGH          40

// 距离 (cm)
#define DISTANCE_CLOSE      20
#define DISTANCE_FAR        50

// 环境值
#define LIGHT_DARK_THRESHOLD   20
#define LIGHT_DIM_THRESHOLD    40
#define HUMIDITY_THRESHOLD     55
#define TEMPERATURE_THRESHOLD  30

// 灯光控制内部状态变量
static uint8_t  lightMode = LIGHT_MODE_AUTO;
static uint16_t prevLowBeamDuty = 0;
static uint16_t prevHighBeamDuty = 0;
static uint8_t  fogLightState = 0;

// 手动模式下的灯光状态
static uint8_t  manualHighBeamState = 0;
static uint8_t  manualLowBeamState = 0;
static uint8_t  manualFogLightState = 0;

// 光照检测变量
static uint8_t prevLight = 0;
static uint8_t tunnelFlag = 0;
static uint16_t tunnelTimer = 0;

// 函数声明
void Redraw_OLED_Labels(void);

/**
  * @brief  灯光控制系统初始化
  */
void LightControl_Init(void)
{
    PWM_SetCompare1(0);
    PWM_SetCompare2(0);  // 远光灯
    PWM_SetCompare3(0);  // 近光灯
    PWM_SetCompare4(0);  // 雾灯
    
    lightMode = LIGHT_MODE_AUTO;
    prevLowBeamDuty = 0;
    prevHighBeamDuty = 0;
    fogLightState = 0;
    
    manualHighBeamState = 0;
    manualLowBeamState = 0;
    manualFogLightState = 0;
    
    prevLight = 0;
    tunnelFlag = 0;
    tunnelTimer = 0;
}

/**
  * @brief  检测光照突变（隧道检测）
  */
static void DetectLightChange(uint8_t light)
{
    if (prevLight > 0 && prevLight - light >= LIGHT_DROP_THRESHOLD) {
        tunnelFlag = 1;
        tunnelTimer = TUNNEL_FLAG_TIME * 2;
    }

    if (tunnelFlag && tunnelTimer > 0) {
        tunnelTimer--;
        if (tunnelTimer == 0) {
            tunnelFlag = 0;
        }
    }
    
    prevLight = light;
}

/**
  * @brief  指数平滑过渡PWM的占空比
  */
static uint16_t ExponentialSmoothPWM(uint8_t channel, uint16_t target, uint16_t current, uint8_t smoothFactor)
{
    if (target == current) return current;
    
    uint16_t diff = (target > current) ? (target - current) : (current - target);
    if (diff <= 1) {
        current = target;
    } else {
        int32_t delta = ((int32_t)target - (int32_t)current) * smoothFactor / 100;
        if (delta == 0) {
            delta = (target > current) ? 1 : -1;
        }
        current += delta;
    }
    
    switch (channel) {
        case 2: PWM_SetCompare2(current); break;  // 远光灯
        case 3: PWM_SetCompare3(current); break;  // 近光灯
        case 4: PWM_SetCompare4(current); break;  // 雾灯
    }
    
    return current;
}

/**
  * @brief  计算近光灯亮度
  */
static uint16_t CalculateLowBeam(uint8_t light, uint32_t speed, float distance)
{
    uint16_t duty = 0;
    uint8_t lightThreshold = Config_GetParamValue(PARAM_LUX);
    
    if (light > lightThreshold) return 0;
    
    if (light < LIGHT_DARK_THRESHOLD) duty = LOW_BEAM_LEVEL3;
    else if (light < LIGHT_DIM_THRESHOLD) duty = LOW_BEAM_LEVEL2;
    else duty = LOW_BEAM_LEVEL1;

    uint8_t speedThreshold = Config_GetParamValue(PARAM_SPD);
    if (speed > speedThreshold) {
        if (duty < LOW_BEAM_LEVEL3) duty += 20;
    } else if (speed > SPEED_LOW) {
        if (duty == LOW_BEAM_LEVEL1) duty = LOW_BEAM_LEVEL2;
    }

    if (distance < DISTANCE_CLOSE) duty = (uint16_t)(duty * 0.6f);
    if (tunnelFlag) duty = LOW_BEAM_LEVEL3;
    
    return duty;
}

/**
  * @brief  计算远光灯亮度
  */
static uint16_t CalculateHighBeam(uint8_t light, uint32_t speed, float distance)
{
    uint8_t lightThreshold = Config_GetParamValue(PARAM_LUX);
    uint8_t speedThreshold = Config_GetParamValue(PARAM_SPD);
    uint8_t distanceThreshold = Config_GetParamValue(PARAM_DIS);
    
    if (light > lightThreshold) return 0;
    if (distance < distanceThreshold || distance <= DISTANCE_CLOSE) return 0;
    if (speed < SPEED_LOW) return 0;
    
    uint16_t duty = 0;
    
    if (speed >= speedThreshold) {
        duty = HIGH_BEAM_LEVEL3;
    } else if (speed >= (speedThreshold + SPEED_LOW) / 2) {
        duty = HIGH_BEAM_LEVEL2;
    } else {
        duty = HIGH_BEAM_LEVEL1;
    }
    
    if (light >= LIGHT_DARK_THRESHOLD && light <= LIGHT_DIM_THRESHOLD) {
        duty = (duty > HIGH_BEAM_LEVEL2) ? HIGH_BEAM_LEVEL2 : duty;
    } else if (light < LIGHT_DARK_THRESHOLD) {
        duty = (duty < HIGH_BEAM_LEVEL2) ? HIGH_BEAM_LEVEL2 : duty;
    }
    
    if (distance < (distanceThreshold + DISTANCE_FAR) / 2) {
        duty = (uint16_t)(duty * 0.85f); 
    }
    
    return duty;
}

/**
  * @brief  检查雾灯开启条件
  */
static uint8_t CheckFogLight(uint8_t temp, uint8_t humidity)
{
    uint8_t humidityThreshold = Config_GetParamValue(PARAM_HUM);
    
    if (humidity > humidityThreshold) {
        return 1;
    }
    return 0;
}

/**
  * @brief  处理按键输入
  */
static void HandleKeyInput(void)
{
    uint8_t key = KeyEXTI_GetKey();
    
    if (lightMode == LIGHT_MODE_CONFIG) {
        return;
    }
    
    if (key != KEY_NONE) {
        switch (key) {
            case KEY1_PRES:
                if (lightMode == LIGHT_MODE_MANUAL) {
                    manualHighBeamState = !manualHighBeamState;
                    PWM_SetCompare2(manualHighBeamState ? HIGH_BEAM_LEVEL3 : 0);
                    prevHighBeamDuty = manualHighBeamState ? HIGH_BEAM_LEVEL3 : 0;
                }
                break;

            case KEY2_PRES:
                if (lightMode == LIGHT_MODE_MANUAL) {
                    manualLowBeamState = !manualLowBeamState;
                    PWM_SetCompare3(manualLowBeamState ? LOW_BEAM_LEVEL3 : 0);
                    prevLowBeamDuty = manualLowBeamState ? LOW_BEAM_LEVEL3 : 0;
                }
                break;

            case KEY3_PRES:
                if (lightMode == LIGHT_MODE_MANUAL) {
                    manualFogLightState = !manualFogLightState;
                    PWM_SetCompare4(manualFogLightState ? 100 : 0);
                    fogLightState = manualFogLightState;
                }
                break;

            case KEY4_PRES:
                if (lightMode == LIGHT_MODE_AUTO) {
                    lightMode = LIGHT_MODE_MANUAL;
                    OLED_Clear();
                    OLED_ShowString(2, 4, "MANUAL MODE");
                    Delay_ms(800);
                    OLED_Clear();
                    Redraw_OLED_Labels();  // 重新绘制标签
                    
                    manualHighBeamState = 0;
                    manualLowBeamState = 0;
                    manualFogLightState = 0;
                    PWM_SetCompare2(0);
                    PWM_SetCompare3(0);
                    PWM_SetCompare4(0);
                    prevLowBeamDuty = 0;
                    prevHighBeamDuty = 0;
                    fogLightState = 0;
                    
                } else if (lightMode == LIGHT_MODE_MANUAL) {
                    lightMode = LIGHT_MODE_AUTO;
                    OLED_Clear();
                    OLED_ShowString(2, 4, "AUTO MODE");
                    Delay_ms(800);
                    OLED_Clear();
                    Redraw_OLED_Labels();  // 重新绘制标签
                    
                    manualHighBeamState = 0;
                    manualLowBeamState = 0;
                    manualFogLightState = 0;
                    PWM_SetCompare2(0);
                    PWM_SetCompare3(0);
                    PWM_SetCompare4(0);
                    prevLowBeamDuty = 0;
                    prevHighBeamDuty = 0;
                    fogLightState = 0;
                }
                break;

            default:
                break;
        }
    }
}

/**
  * @brief  更新灯光控制逻辑
  */
void LightControl_Update(uint8_t light, uint32_t speed, float distance, uint8_t temp, uint8_t humidity)
{
    // 处理长按事件
    if (KeyEXTI_GetLongPress(1) && lightMode == LIGHT_MODE_AUTO) {
        lightMode = LIGHT_MODE_CONFIG;
        Config_EnterConfigMode();
        OLED_Clear();
        OLED_ShowString(2, 4, "CONFIG MODE");
        Delay_ms(500);
        return;
    }
    
    if (KeyEXTI_GetLongPress(4) && lightMode == LIGHT_MODE_CONFIG) {
        Config_CommitTempParams();
        uint8_t save_result = Config_SaveParams();
        
        OLED_Clear();
        if (save_result) {
            OLED_ShowString(2, 5, "SAVE OK");
        } else {
            OLED_ShowString(2, 4, "SAVE FAIL");
        }
        Delay_ms(1000);
        
        Config_ExitConfigMode();
        lightMode = LIGHT_MODE_AUTO;
        
        prevLowBeamDuty = 0;
        prevHighBeamDuty = 0;
        fogLightState = 0;
        manualHighBeamState = 0;
        manualLowBeamState = 0;
        manualFogLightState = 0;
        PWM_SetCompare2(0);
        PWM_SetCompare3(0);
        PWM_SetCompare4(0);
        
        OLED_Clear();
        OLED_ShowString(2, 4, "AUTO MODE");
        Delay_ms(500);
        OLED_Clear();
        Redraw_OLED_Labels();  // 重新绘制标签
        return;
    }
    
    if (lightMode == LIGHT_MODE_CONFIG) {
        Config_HandleKeys();
        Config_UpdateDisplay();
        Config_ProcessTimeout();
        return;
    }

    HandleKeyInput();
    
    DetectLightChange(light);

    if (lightMode == LIGHT_MODE_AUTO) {
        uint16_t lowBeamTarget = CalculateLowBeam(light, speed, distance);
        prevLowBeamDuty = ExponentialSmoothPWM(3, lowBeamTarget, prevLowBeamDuty, 20);

        uint16_t highBeamTarget = CalculateHighBeam(light, speed, distance);
        prevHighBeamDuty = ExponentialSmoothPWM(2, highBeamTarget, prevHighBeamDuty, 15);

        uint8_t fogTarget = CheckFogLight(temp, humidity);
        if (fogTarget != fogLightState) {
            fogLightState = fogTarget;
            PWM_SetCompare4(fogLightState ? 100 : 0);
        }
    }
}

/**
  * @brief  获取当前控制模式
  */
uint8_t LightControl_GetMode(void)
{
    return lightMode;
}

/**
  * @brief  设置控制模式
  */
void LightControl_SetMode(uint8_t mode)
{
    if (mode <= LIGHT_MODE_CONFIG) {
        lightMode = mode;
        
        prevLowBeamDuty = 0;
        prevHighBeamDuty = 0;
        fogLightState = 0;
        manualHighBeamState = 0;
        manualLowBeamState = 0;
        manualFogLightState = 0;
        PWM_SetCompare2(0);
        PWM_SetCompare3(0);
        PWM_SetCompare4(0);
        
        if (mode != LIGHT_MODE_CONFIG) {
            OLED_Clear();
            Redraw_OLED_Labels();  // 重新绘制标签
        }
    }
}
