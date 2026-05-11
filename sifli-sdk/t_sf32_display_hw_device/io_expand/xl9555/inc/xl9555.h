#ifndef __XL9555_H__
#define __XL9555_H__

#include <rtthread.h>
#include <rtdevice.h>

#define XL9555_I2C_ADDR 0x22
#define XL9555_PIN_OUTPUT 0
#define XL9555_PIN_INPUT 1

/* 寄存器地址定义 */
#define XL9555_INPUT_PORT_0     0x00    /* 输入寄存器 0 (P0-P7) */
#define XL9555_INPUT_PORT_1     0x01    /* 输入寄存器 1 (P8-P15) */
#define XL9555_OUTPUT_PORT_0    0x02    /* 输出寄存器 0 */
#define XL9555_OUTPUT_PORT_1    0x03    /* 输出寄存器 1 */
#define XL9555_POLARITY_0       0x04    /* 极性反转寄存器 0 */
#define XL9555_POLARITY_1       0x05    /* 极性反转寄存器 1 */
#define XL9555_CONFIG_0         0x06    /* 配置寄存器 0 (0:输出, 1:输入) */
#define XL9555_CONFIG_1         0x07    /* 配置寄存器 1 */

#ifdef BSP_USING_BOARD_T_DISPLAY_SF32
#define XL9555_GPS_EN_PIN           0
#define XL9555_KEY_LED_EN_PIN       1
#define XL9555_WIFI_EN_PIN          2
#define XL9555_AW86224_RST_PIN      3 /* Example: P0_0 controls AW86224 enable */
#define XL9555_GPS_RST_PIN          4
#define XL9555_WIFI_RST_PIN         5
#define XL9555_GPS_ESP32C6_SEL_PIN  6 // 0: GPS, 1: ESP32C6
#define XL9555_KEY_RST_PIN          7 
#endif

/* 设备结构体，用于存储当前状态和I2C句柄 */
struct xl9555_device
{
    struct rt_i2c_bus_device *i2c_bus;  /* I2C 总线设备句柄 */
    rt_uint8_t dev_addr;                /* 设备地址 (7位地址) */
    rt_uint16_t output_cache;           /* 输出寄存器缓存 */
    rt_uint16_t config_cache;           /* 配置寄存器缓存 */
};

rt_err_t xl9555_init();
void xl9555_pin_mode(rt_uint8_t pin, rt_uint8_t mode);
void xl9555_digital_write(rt_uint8_t pin, rt_uint8_t val);
rt_uint8_t xl9555_digital_read(rt_uint8_t pin);

#endif /* __XL9555_H__ */