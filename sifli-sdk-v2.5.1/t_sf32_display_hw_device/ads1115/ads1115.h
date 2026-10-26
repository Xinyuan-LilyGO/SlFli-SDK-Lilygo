/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ADS1115_H__
#define __ADS1115_H__

#include <rtdevice.h>
#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADS1115_ADDR_GND              0x48U
#define ADS1115_ADDR_VDD              0x49U
#define ADS1115_ADDR_SDA              0x4AU
#define ADS1115_ADDR_SCL              0x4BU

#define ADS1115_REG_CONVERSION        0x00U
#define ADS1115_REG_CONFIG            0x01U
#define ADS1115_REG_LO_THRESH         0x02U
#define ADS1115_REG_HI_THRESH         0x03U

#define ADS1115_CONFIG_OS             0x8000U
#define ADS1115_CONFIG_MODE_SINGLE    0x0100U
#define ADS1115_CONFIG_COMP_MODE      0x0010U
#define ADS1115_CONFIG_COMP_POL       0x0008U
#define ADS1115_CONFIG_COMP_LAT       0x0004U
#define ADS1115_CONFIG_COMP_QUE_MASK  0x0003U
#define ADS1115_CONFIG_COMP_DISABLE   0x0003U

#ifndef ADS1115_I2C_BUS_NAME
#define ADS1115_I2C_BUS_NAME          "i2c1"
#endif

#ifndef ADS1115_I2C_ADDR
#define ADS1115_I2C_ADDR              ADS1115_ADDR_GND
#endif

typedef enum
{
    ADS1115_MUX_AIN0_AIN1 = 0,
    ADS1115_MUX_AIN0_AIN3,
    ADS1115_MUX_AIN1_AIN3,
    ADS1115_MUX_AIN2_AIN3,
    ADS1115_MUX_AIN0_GND,
    ADS1115_MUX_AIN1_GND,
    ADS1115_MUX_AIN2_GND,
    ADS1115_MUX_AIN3_GND
} ads1115_mux_t;

typedef enum
{
    ADS1115_PGA_6_144V = 0,
    ADS1115_PGA_4_096V,
    ADS1115_PGA_2_048V,
    ADS1115_PGA_1_024V,
    ADS1115_PGA_0_512V,
    ADS1115_PGA_0_256V
} ads1115_pga_t;

typedef enum
{
    ADS1115_RATE_8_SPS = 0,
    ADS1115_RATE_16_SPS,
    ADS1115_RATE_32_SPS,
    ADS1115_RATE_64_SPS,
    ADS1115_RATE_128_SPS,
    ADS1115_RATE_250_SPS,
    ADS1115_RATE_475_SPS,
    ADS1115_RATE_860_SPS
} ads1115_rate_t;

typedef enum
{
    ADS1115_COMP_TRADITIONAL = 0,
    ADS1115_COMP_WINDOW
} ads1115_comp_mode_t;

typedef enum
{
    ADS1115_COMP_ACTIVE_LOW = 0,
    ADS1115_COMP_ACTIVE_HIGH
} ads1115_comp_polarity_t;

typedef enum
{
    ADS1115_COMP_ASSERT_1 = 0,
    ADS1115_COMP_ASSERT_2,
    ADS1115_COMP_ASSERT_4,
    ADS1115_COMP_DISABLED
} ads1115_comp_queue_t;

typedef struct
{
    struct rt_i2c_bus_device *i2c_bus;
    struct rt_mutex lock;
    rt_uint8_t address;
    ads1115_pga_t pga;
    ads1115_rate_t rate;
    rt_bool_t initialized;
} ads1115_device_t;

/* Initialize the singleton device using menuconfig values. */
rt_err_t ads1115_init(void);
rt_err_t ads1115_init_device(const char *i2c_bus_name, rt_uint8_t address);
void ads1115_deinit(void);
ads1115_device_t *ads1115_get_device(void);

rt_err_t ads1115_read_register(ads1115_device_t *dev, rt_uint8_t reg,
                               rt_uint16_t *value);
rt_err_t ads1115_write_register(ads1115_device_t *dev, rt_uint8_t reg,
                                rt_uint16_t value);

rt_err_t ads1115_read_single_shot(ads1115_device_t *dev, ads1115_mux_t mux,
                                  ads1115_pga_t pga, ads1115_rate_t rate,
                                  rt_int16_t *raw, rt_int32_t *microvolts);
rt_err_t ads1115_read_single_ended(ads1115_device_t *dev, rt_uint8_t channel,
                                   ads1115_pga_t pga, ads1115_rate_t rate,
                                   rt_int16_t *raw, rt_int32_t *microvolts);
rt_err_t ads1115_read_differential(ads1115_device_t *dev,
                                   ads1115_mux_t differential_mux,
                                   ads1115_pga_t pga, ads1115_rate_t rate,
                                   rt_int16_t *raw, rt_int32_t *microvolts);

rt_err_t ads1115_start_continuous(ads1115_device_t *dev, ads1115_mux_t mux,
                                  ads1115_pga_t pga, ads1115_rate_t rate);
rt_err_t ads1115_stop_continuous(ads1115_device_t *dev);
rt_err_t ads1115_read_conversion(ads1115_device_t *dev, rt_int16_t *raw,
                                 rt_int32_t *microvolts);
rt_err_t ads1115_is_conversion_ready(ads1115_device_t *dev, rt_bool_t *ready);

rt_err_t ads1115_configure_comparator(ads1115_device_t *dev,
                                      ads1115_comp_mode_t mode,
                                      ads1115_comp_polarity_t polarity,
                                      rt_bool_t latch,
                                      ads1115_comp_queue_t queue,
                                      rt_int16_t low_threshold,
                                      rt_int16_t high_threshold);
rt_err_t ads1115_enable_conversion_ready_pin(ads1115_device_t *dev,
                                             rt_bool_t active_high);

rt_int32_t ads1115_raw_to_microvolts(rt_int16_t raw, ads1115_pga_t pga);

#ifdef __cplusplus
}
#endif

#endif /* __ADS1115_H__ */
