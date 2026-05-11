#ifndef __AW21009_H__
#define __AW21009_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>
#include "rtthread.h"
#include <rtdevice.h>


// AW21009 默认I2C地址（AD0=GND, AD1=GND）
// #define AW21009_I2C_ADDR         0x20
#define AW21009_I2C_ADDR         0x25
#define AW21009_CHIP_ID          0x12

// 寄存器地址定义
#define REG_GCR                  0x20

// LED1-LED6的12位亮度寄存器（标准模式）
#define REG_BR00L                0x21  // LED1低8位
#define REG_BR00H                0x22  // LED1高4位
#define REG_BR01L                0x23  // LED2低8位
#define REG_BR01H                0x24  // LED2高4位
#define REG_BR02L                0x25  // LED3低8位
#define REG_BR02H                0x26  // LED3高4位
#define REG_BR03L                0x27  // LED4低8位
#define REG_BR03H                0x28  // LED4高4位
#define REG_BR04L                0x29  // LED5低8位
#define REG_BR04H                0x2A  // LED5高4位
#define REG_BR05L                0x2B  // LED6低8位
#define REG_BR05H                0x2C  // LED6高4位

#define REG_UPDATE               0x45

// SL寄存器（电流缩放）
#define REG_SL00                 0x46  // LED1
#define REG_SL01                 0x47  // LED2
#define REG_SL02                 0x48  // LED3
#define REG_SL03                 0x49  // LED4
#define REG_SL04                 0x4A  // LED5
#define REG_SL05                 0x4B  // LED6

#define REG_GCCR                 0x58
#define REG_GCR2                 0x61

#define REG_RESET                0x70

// 全局电流设置（0-255对应0-40mA）
#define GLOBAL_CURRENT           0xFF  // 最大电流

enum {
    LED_BRIGHT_0 = 0,      // 0% 亮度
    LED_BRIGHT_10 = 410,   // 10% 亮度
    LED_BRIGHT_20 = 819,   // 20% 亮度
    LED_BRIGHT_30 = 1229,  // 30% 亮度
    LED_BRIGHT_40 = 1638,  // 40% 亮度
    LED_BRIGHT_50 = 2048,  // 50% 亮度
    LED_BRIGHT_60 = 2457,  // 60% 亮度
    LED_BRIGHT_70 = 2867,  // 70% 亮度
    LED_BRIGHT_80 = 3276,  // 80% 亮度
    LED_BRIGHT_90 = 3686,  // 90% 亮度
    LED_BRIGHT_100 = 4095, // 100% 亮度
};


int aw21009_app_init(void);
rt_err_t aw21009_set_all_brightness(rt_uint16_t brightness);
rt_err_t aw21009_set_led_brightness(rt_uint8_t led, rt_uint16_t brightness);

#ifdef __cplusplus
}
#endif

#endif