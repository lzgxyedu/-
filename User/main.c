/*==============================================================================
  文件：main.c (修复OLED闪烁和单位丢失、切换不刷新等问题)
==============================================================================*/
#include "stm32f10x.h"
#include "system_stm32f10x.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "Delay.h"
#include "OLED.h"
#include "LED.h"
#include "CountSensor.h"
#include "LDR.h"
#include "dht11.h"
#include "ultrasonic.h"
#include "PWM.h"
#include "LightControl.h"
#include "Config.h"
#include "KeyEXTI.h"

// wrapper 声明
uint32_t CountSensor_GetSpeed(uint16_t c, uint32_t dt_ms, uint32_t pd_cm);
uint8_t  DHT11_Read(uint8_t *t, uint8_t *h);
uint8_t  LDR_GetPercent(void);
void     LED1_Toggle(void);
float    Ultrasonic_GetDistance(void);

// 模式定义
#define MODE_AUTO    0
#define MODE_MANUAL  1
#define MODE_CONFIG  2

// 刷新间隔
#define SAMPLE_INTERVAL_MS  100   // 传感器采样周期
#define DISPLAY_UPDATE_MS   200   // 显示更新周期
#define IDLE_LOOP_MS        20    // 主循环空闲时间

// 车速计算参数
#define WHEEL_CIRCUMFERENCE_CM  20.0f  // 车轮周长
#define PULSES_PER_ROTATION     20     // 码盘一圈脉冲数
#define ZERO_SPEED_TIMEOUT_MS   200    // 200ms无脉冲认为停止

static char     buf[20];
static uint32_t spd;
static uint8_t  lp, tp, hp;
static float    ds;

// 车速计算相关变量
static uint16_t last_pulse_count = 0;
static uint32_t last_calc_time = 0;

// 显示缓冲区
typedef struct {
    uint32_t speed;
    uint8_t  light;
    float    distance;
    uint8_t  temperature;
    uint8_t  humidity;
    uint8_t  mode;
    uint8_t  valid;
} DisplayBuffer_t;

static DisplayBuffer_t display_buffer = {0};

// --- OLED防闪烁行缓冲（修正单位丢失核心） ---
static char line_buffers[5][17] = {0};  // 每行最多16个字符 + '\0'

// 清空行缓冲
void Clear_LineBuffers(void) {
    for (int i = 0; i < 5; i++) {
        memset(line_buffers[i], 0, sizeof(line_buffers[i]));
    }
}

// 安全写入字符串到OLED（含单位修正）
void Safe_OLED_ShowString(uint8_t line, uint8_t column, char* str, uint8_t max_len)
{
    if (line == 0 || line > 4 || column == 0 || column > 16) return;

    uint8_t start_pos = column - 1;
    uint8_t str_len = strlen(str);
    if (str_len > max_len) str_len = max_len;

    uint8_t need_update = 0;
    for (uint8_t i = 0; i < str_len; i++) {
        if (start_pos + i >= 16) break;
        if (line_buffers[line][start_pos + i] != str[i]) {
            need_update = 1;
            break;
        }
    }
    for (uint8_t i = str_len; i < max_len && start_pos + i < 16; i++) {
        if (line_buffers[line][start_pos + i] != ' ' && line_buffers[line][start_pos + i] != '\0') {
            need_update = 1;
            break;
        }
    }
    if (need_update) {
        for (uint8_t i = 0; i < max_len && start_pos + i < 16; i++) {
            if (i < str_len) {
                line_buffers[line][start_pos + i] = str[i];
            } else {
                line_buffers[line][start_pos + i] = ' ';
            }
        }
        OLED_ShowString(line, column, str);
        for (uint8_t i = str_len; i < max_len; i++) {
            OLED_ShowChar(line, column + i, ' ');
        }
    }
}

// OLED主标签重绘，并重置缓冲
void Redraw_OLED_Labels(void)
{
    OLED_ShowString(1, 1, "Spd:");
    OLED_ShowString(2, 1, "Lux:");
    OLED_ShowString(3, 1, "Dst:");
    OLED_ShowString(4, 1, "T:");
    OLED_ShowString(4, 8, "H:");
    Clear_LineBuffers();
    display_buffer.valid = 0;
}

// 车速计算
uint32_t Calculate_Real_Speed(void)
{
    uint32_t current_time = GetTick();
    uint16_t current_pulses = CountSensor_Get();

    uint32_t time_delta = current_time - last_calc_time;
    uint16_t pulse_delta = 0;

    if (current_pulses >= last_pulse_count) {
        pulse_delta = current_pulses - last_pulse_count;
    } else {
        pulse_delta = (10000 - last_pulse_count) + current_pulses;
    }
    uint32_t speed = 0;

    if (pulse_delta > 0 && time_delta > 0) {
        float rotations = (float)pulse_delta / PULSES_PER_ROTATION;
        float distance_cm = rotations * WHEEL_CIRCUMFERENCE_CM;
        float calculated_speed = (distance_cm * 1000.0f) / time_delta;
        if (calculated_speed > 200.0f) calculated_speed = 200.0f;
        if (calculated_speed < 0.0f) calculated_speed = 0.0f;
        speed = (uint32_t)calculated_speed;
    } else {
        if (time_delta > ZERO_SPEED_TIMEOUT_MS) {
            speed = 0;
        } else {
            return spd;
        }
    }
    last_pulse_count = current_pulses;
    last_calc_time = current_time;
    return speed;
}

// 读取所有传感器
void Read_AllSensors(void)
{
    lp  = LDR_GetPercent();
    DHT11_Read(&tp, &hp);
    ds  = Ultrasonic_GetDistance();
}

// OLED主数据刷新（修正切换后不刷新bug/单位丢失）
void Update_Display(void)
{
    static uint8_t last_mode = 0xFF;
    uint8_t mode = LightControl_GetMode();

    // 模式切换时，必须全部重画并刷新所有数据
    if (mode != last_mode && mode != MODE_CONFIG) {
        OLED_Clear();
        Delay_ms(20);
        Redraw_OLED_Labels();
        last_mode = mode;
        display_buffer.valid = 0;

        // 强制刷新全部内容（含单位！）
        sprintf(buf, "%3lu", (unsigned long)spd);
        Safe_OLED_ShowString(1, 5, buf, 4);
        Safe_OLED_ShowString(1, 9, "cm/s", 4);

        sprintf(buf, "%3d", lp);
        Safe_OLED_ShowString(2, 5, buf, 3);
        Safe_OLED_ShowString(2, 9, "%", 1);

        if (ds >= 0) {
            sprintf(buf, "%4.1f", ds);
        } else {
            sprintf(buf, "----");
        }
        Safe_OLED_ShowString(3, 5, buf, 5);
        Safe_OLED_ShowString(3, 10, "cm", 2);

        sprintf(buf, "%2d", tp);
        Safe_OLED_ShowString(4, 3, buf, 2);
        Safe_OLED_ShowString(4, 6, "C", 1);

        sprintf(buf, "%2d", hp);
        Safe_OLED_ShowString(4, 10, buf, 2);
        Safe_OLED_ShowString(4, 13, "%", 1);

        switch (mode) {
            case MODE_AUTO:   Safe_OLED_ShowString(1, 15, "A", 1); break;
            case MODE_MANUAL: Safe_OLED_ShowString(1, 15, "M", 1); break;
            default:          Safe_OLED_ShowString(1, 15, " ", 1); break;
        }
        // 更新缓冲区内容
        display_buffer.speed = spd;
        display_buffer.light = lp;
        display_buffer.distance = ds;
        display_buffer.temperature = tp;
        display_buffer.humidity = hp;
        display_buffer.mode = mode;
        display_buffer.valid = 1;
        return;
    }

    // 非配置模式时自动刷新主界面
    if (mode != MODE_CONFIG) {
        uint8_t need_update = 0;
        if (!display_buffer.valid ||
            display_buffer.speed != spd ||
            display_buffer.light != lp ||
            display_buffer.distance != ds ||
            display_buffer.temperature != tp ||
            display_buffer.humidity != hp ||
            display_buffer.mode != mode) {
            need_update = 1;
        }
        if (need_update) {
            sprintf(buf, "%3lu", (unsigned long)spd);
            Safe_OLED_ShowString(1, 5, buf, 4);
            Safe_OLED_ShowString(1, 9, "cm/s", 4);

            sprintf(buf, "%3d", lp);
            Safe_OLED_ShowString(2, 5, buf, 3);
            Safe_OLED_ShowString(2, 9, "%", 1);

            if (ds >= 0) {
                sprintf(buf, "%4.1f", ds);
            } else {
                sprintf(buf, "----");
            }
            Safe_OLED_ShowString(3, 5, buf, 5);
            Safe_OLED_ShowString(3, 10, "cm", 2);

            sprintf(buf, "%2d", tp);
            Safe_OLED_ShowString(4, 3, buf, 2);
            Safe_OLED_ShowString(4, 6, "C", 1);

            sprintf(buf, "%2d", hp);
            Safe_OLED_ShowString(4, 10, buf, 2);
            Safe_OLED_ShowString(4, 13, "%", 1);

            switch (mode) {
                case MODE_AUTO:   Safe_OLED_ShowString(1, 15, "A", 1); break;
                case MODE_MANUAL: Safe_OLED_ShowString(1, 15, "M", 1); break;
                default:          Safe_OLED_ShowString(1, 15, " ", 1); break;
            }
            display_buffer.speed = spd;
            display_buffer.light = lp;
            display_buffer.distance = ds;
            display_buffer.temperature = tp;
            display_buffer.humidity = hp;
            display_buffer.mode = mode;
            display_buffer.valid = 1;
        }
    }
}

// 系统状态LED
void Update_SystemStatus(void)
{
    static uint32_t last_led = 0;
    uint32_t now = GetTick();
    uint16_t interval;
    switch (LightControl_GetMode()) {
        case MODE_AUTO:   interval = 1000; break;
        case MODE_MANUAL: interval = 200;  break;
        case MODE_CONFIG: interval = 500;  break;
        default:          interval = 300;  break;
    }
    if (now - last_led >= interval) {
        LED1_Toggle();
        last_led = now;
    }
}

int main(void)
{
    uint32_t last_sample = 0;
    uint32_t last_display_update = 0;

    SystemInit();
    SystemCoreClockUpdate();
    Delay_Init();

    LED_Init();
    LED1_ON();
    LED2_OFF();

    OLED_Init();
    OLED_Clear();
    OLED_ShowString(2, 3, "System Ready");
    Delay_ms(1500);
    OLED_Clear();
    Delay_ms(50);

    display_buffer.valid = 0;
    Redraw_OLED_Labels();

    PWM_Init();
    Config_Init();
    KeyEXTI_Init();
    CountSensor_Init();
    CountSensor_Reset();
    LDR_Init();
    DHT11_Init();
    Ultrasonic_Init();
    LightControl_Init();

    last_pulse_count = CountSensor_Get();
    last_calc_time = GetTick();
    spd = 0;

    while (1) {
        uint32_t now = GetTick();
        spd = Calculate_Real_Speed();
        if (now - last_sample >= SAMPLE_INTERVAL_MS) {
            Read_AllSensors();
            last_sample = now;
        }
        if (now - last_display_update >= DISPLAY_UPDATE_MS) {
            Update_Display();
            last_display_update = now;
        }
        LightControl_Update(lp, spd, ds, tp, hp);
        Update_SystemStatus();
        Delay_ms(IDLE_LOOP_MS);
    }
}

// wrapper实现
uint32_t CountSensor_GetSpeed(uint16_t c, uint32_t dt_ms, uint32_t pd_cm) {
    if (dt_ms == 0) return 0;
    return (uint32_t)c * pd_cm * 1000UL / dt_ms;
}
uint8_t DHT11_Read(uint8_t *t, uint8_t *h) { return DHT11_Read_Data(t, h); }
uint8_t LDR_GetPercent(void) { return LDR_Percent(); }
void    LED1_Toggle(void) { LED1_Turn(); }
float   Ultrasonic_GetDistance(void) { return UltrasonicGetLength(); }
