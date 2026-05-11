/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-xx-xx     YourName     first version
 */

#include "aw86224.h"
#include "xl9555.h"
#include <rtdevice.h>
#include <rtthread.h>

// #define XL9555_WIFI_3V3_PIN 2    /* Example: P0_0 controls AW86224 enable */
// #define XL9555_AW86224_RST_PIN 3 /* Example: P0_0 controls AW86224 enable */

#define DBG_ENABLE
#define DBG_SECTION_NAME "aw86224"
#define DBG_LEVEL DBG_LOG
#include <rtdbg.h>

/* Internal structure */
struct aw86224_device
{
    struct rt_i2c_bus_device *i2c_bus;
    rt_uint8_t init_flag;
    rt_uint32_t f0_calibrated;
};
static struct aw86224_device g_aw86224_dev = {0};

/* Forward declarations */
static rt_err_t aw86224_write_reg(rt_uint8_t reg, rt_uint8_t data);
static rt_err_t aw86224_read_reg(rt_uint8_t reg, rt_uint8_t *data);
static rt_err_t aw86224_write_bits(rt_uint8_t reg, rt_uint8_t mask,
                                   rt_uint8_t data);
static rt_err_t aw86224_wait_standby(rt_uint32_t timeout_ms);

rt_uint8_t aw862xx_ram_data[] = {
    // Default 170HZ waveform
    0x55, 0x08, 0x11, 0x09, 0x76, 0x09, 0x77, 0x0a, 0x80, 0x0a, 0x81, 0x0b,
    0x2e, 0x0b, 0x2f, 0x0b, 0x61, 0x00, 0x0e, 0x1d, 0x2b, 0x38, 0x45, 0x50,
    0x5b, 0x63, 0x6b, 0x71, 0x75, 0x77, 0x77, 0x76, 0x73, 0x6e, 0x68, 0x5f,
    0x56, 0x4b, 0x3f, 0x32, 0x24, 0x16, 0x07, 0xfa, 0xeb, 0xdd, 0xcf, 0xc2,
    0xb6, 0xab, 0xa1, 0x99, 0x92, 0x8d, 0x8a, 0x89, 0x89, 0x8b, 0x8f, 0x95,
    0x9c, 0xa5, 0xaf, 0xba, 0xc7, 0xd4, 0xe2, 0xf1, 0x00, 0x0d, 0x1c, 0x2a,
    0x37, 0x44, 0x50, 0x5a, 0x63, 0x6a, 0x70, 0x74, 0x77, 0x77, 0x76, 0x73,
    0x6e, 0x68, 0x60, 0x57, 0x4c, 0x40, 0x33, 0x25, 0x17, 0x08, 0xfb, 0xec,
    0xde, 0xd0, 0xc3, 0xb6, 0xab, 0xa2, 0x99, 0x93, 0x8e, 0x8a, 0x89, 0x89,
    0x8b, 0x8f, 0x94, 0x9c, 0xa4, 0xae, 0xba, 0xc6, 0xd3, 0xe1, 0xf0, 0xff,
    0x0c, 0x1b, 0x29, 0x37, 0x43, 0x4f, 0x59, 0x62, 0x6a, 0x70, 0x74, 0x77,
    0x77, 0x76, 0x73, 0x6f, 0x69, 0x61, 0x57, 0x4c, 0x41, 0x34, 0x26, 0x18,
    0x09, 0xfb, 0xed, 0xde, 0xd1, 0xc3, 0xb7, 0xac, 0xa2, 0x9a, 0x93, 0x8e,
    0x8a, 0x89, 0x89, 0x8b, 0x8e, 0x94, 0x9b, 0xa4, 0xae, 0xb9, 0xc5, 0xd3,
    0xe1, 0xef, 0x00, 0xf3, 0xe5, 0xd8, 0xcb, 0xbf, 0xb4, 0xaa, 0xa2, 0x9a,
    0x95, 0x91, 0x8e, 0x8e, 0x8e, 0x91, 0x95, 0x9b, 0xa3, 0xab, 0xb5, 0xc0,
    0xcd, 0xd9, 0xe7, 0xf4, 0x01, 0x0f, 0x1d, 0x2a, 0x37, 0x43, 0x4e, 0x57,
    0x60, 0x67, 0x6c, 0x70, 0x72, 0x72, 0x71, 0x6e, 0x6a, 0x64, 0x5c, 0x53,
    0x49, 0x3e, 0x32, 0x25, 0x17, 0x0a, 0xfd, 0xef, 0xe1, 0xd4, 0xc7, 0xbc,
    0xb1, 0xa8, 0x9f, 0x99, 0x93, 0x90, 0x8e, 0x8e, 0x8f, 0x92, 0x97, 0x9d,
    0xa5, 0xae, 0xb8, 0xc4, 0xd0, 0xdd, 0xea, 0xf8, 0x05, 0x13, 0x21, 0x2e,
    0x3a, 0x46, 0x50, 0x5a, 0x62, 0x68, 0x6d, 0x71, 0x72, 0x72, 0x71, 0x6d,
    0x68, 0x62, 0x5a, 0x51, 0x46, 0x3b, 0x2e, 0x21, 0x14, 0x00, 0xf9, 0xf2,
    0xeb, 0xe4, 0xde, 0xd8, 0xd3, 0xcf, 0xcb, 0xc8, 0xc6, 0xc5, 0xc5, 0xc5,
    0xc7, 0xc9, 0xcc, 0xd1, 0xd5, 0xdb, 0xe1, 0xe7, 0xee, 0xf5, 0xfd, 0x03,
    0x0a, 0x11, 0x18, 0x1f, 0x25, 0x2a, 0x2f, 0x33, 0x37, 0x39, 0x3b, 0x3b,
    0x3b, 0x3a, 0x38, 0x35, 0x32, 0x2d, 0x28, 0x23, 0x1c, 0x16, 0x0f, 0x07,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x07, 0x16, 0x24, 0x32, 0x3f, 0x4b, 0x56, 0x5f, 0x67, 0x6e,
    0x73, 0x76, 0x77, 0x77, 0x75, 0x71, 0x6b, 0x64, 0x5b, 0x51, 0x45, 0x39,
    0x2b, 0x1d, 0x0f, 0x00, 0xf2, 0xe4, 0xd6, 0xc8, 0xbc, 0xb0, 0xa6, 0x9d,
    0x95, 0x90, 0x8b, 0x89, 0x89, 0x8a, 0x8d, 0x92, 0x98, 0xa0, 0xaa, 0xb5,
    0xc1, 0xce, 0xdb, 0xea, 0xf8, 0x06, 0x15, 0x23, 0x31, 0x3e, 0x4a, 0x55,
    0x5f, 0x67, 0x6e, 0x73, 0x76, 0x77, 0x77, 0x75, 0x71, 0x6b, 0x64, 0x5b,
    0x51, 0x46, 0x39, 0x2c, 0x1e, 0x10, 0x01, 0xf3, 0xe5, 0xd7, 0xc9, 0xbc,
    0xb1, 0xa6, 0x9d, 0x96, 0x90, 0x8c, 0x89, 0x89, 0x8a, 0x8d, 0x91, 0x98,
    0xa0, 0xa9, 0xb4, 0xc0, 0xcd, 0xda, 0xe9, 0xf7, 0x05, 0x14, 0x22, 0x30,
    0x3d, 0x49, 0x54, 0x5e, 0x66, 0x6d, 0x72, 0x76, 0x77, 0x77, 0x75, 0x71,
    0x6c, 0x65, 0x5c, 0x52, 0x47, 0x3a, 0x2d, 0x1f, 0x11, 0x00, 0x0f, 0x1e,
    0x2c, 0x3a, 0x47, 0x53, 0x5e, 0x67, 0x6f, 0x75, 0x79, 0x7c, 0x7c, 0x7b,
    0x78, 0x74, 0x6d, 0x65, 0x5b, 0x50, 0x44, 0x37, 0x29, 0x1a, 0x0b, 0xfd,
    0xee, 0xdf, 0xd0, 0xc3, 0xb6, 0xaa, 0xa0, 0x97, 0x90, 0x8a, 0x86, 0x84,
    0x84, 0x85, 0x89, 0x8e, 0x95, 0x9d, 0xa7, 0xb3, 0xbf, 0xcd, 0xdb, 0xea,
    0xf9, 0x07, 0x16, 0x25, 0x33, 0x41, 0x4d, 0x59, 0x63, 0x6b, 0x72, 0x77,
    0x7b, 0x7c, 0x7c, 0x7a, 0x76, 0x70, 0x69, 0x60, 0x56, 0x4a, 0x3d, 0x30,
    0x21, 0x13, 0x00, 0xfa, 0xf4, 0xee, 0xe9, 0xe4, 0xdf, 0xdb, 0xd7, 0xd4,
    0xd1, 0xd0, 0xcf, 0xcf, 0xcf, 0xd0, 0xd2, 0xd5, 0xd9, 0xdd, 0xe1, 0xe6,
    0xeb, 0xf1, 0xf7, 0xfd, 0x02, 0x08, 0x0e, 0x14, 0x1a, 0x1f, 0x23, 0x27,
    0x2b, 0x2d, 0x30, 0x31, 0x31, 0x31, 0x30, 0x2f, 0x2c, 0x29, 0x26, 0x21,
    0x1d, 0x17, 0x12, 0x0c, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x52, 0x63, 0x6d, 0x73, 0x78, 0x7a, 0x7c, 0x7d,
    0x7d, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7d, 0x7c, 0x7b, 0x79, 0x76,
    0x71, 0x68, 0x5b, 0x47, 0x26, 0xf2, 0xc8, 0xae, 0x9d, 0x93, 0x8c, 0x88,
    0x86, 0x84, 0x83, 0x83, 0x82, 0x82, 0x82, 0x82, 0x83, 0x83, 0x84, 0x85,
    0x88, 0x8b, 0x91, 0x9a, 0xa9, 0xc1, 0xe6, 0x07, 0x17, 0x26, 0x34, 0x42,
    0x4f, 0x5a, 0x64, 0x6d, 0x74, 0x79, 0x7d, 0x7e, 0x7e, 0x7c, 0x78, 0x72,
    0x6a, 0x61, 0x56, 0x4a, 0x3d, 0x2f, 0x20, 0x11, 0x00, 0x0d, 0x1a, 0x27,
    0x33, 0x3f, 0x49, 0x53, 0x5b, 0x62, 0x67, 0x6b, 0x6d, 0x6d, 0x6c, 0x6a,
    0x65, 0x5f, 0x58, 0x4f, 0x46, 0x3b, 0x2f, 0x22, 0x15, 0x08, 0xf9, 0xe9,
    0xda, 0xcc, 0xbe, 0xb1, 0xa6, 0x9c, 0x93, 0x8c, 0x87, 0x83, 0x82, 0x82,
    0x84, 0x88, 0x8e, 0x96, 0x9f, 0xaa, 0xb6, 0xc3, 0xd1, 0xe0, 0xef, 0x40,
    0x53, 0x5e, 0x66, 0x6b, 0x6e, 0x6f, 0x71, 0x71, 0x72, 0x72, 0x72, 0x72,
    0x72, 0x72, 0x72, 0x71, 0x70, 0x6f, 0x6d, 0x6a, 0x64, 0x5c, 0x4f, 0x39,
    0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x11, 0x19, 0x21, 0x28, 0x2f, 0x35, 0x3a, 0x3e, 0x41, 0x44, 0x45,
    0x45, 0x45, 0x43, 0x40, 0x3c, 0x37, 0x32, 0x2c, 0x25, 0x1d, 0x15, 0x0d,
    0x04, 0xfc, 0xf4, 0xec, 0xe4, 0xdc, 0xd5, 0xce, 0xc9, 0xc4, 0xc0, 0xbd,
    0xbb, 0xbb, 0xbb, 0xbc, 0xbe, 0xc2, 0xc6, 0xcb, 0xd1, 0xd8, 0xdf, 0xe7,
    0xef, 0xf7,
};
/**
 * @brief Enable/disable AW86224 power
 */
static void aw86224_power_enable(rt_bool_t enable)
{
    if (enable)
    {
        xl9555_pin_mode(XL9555_WIFI_EN_PIN, XL9555_PIN_OUTPUT);
        xl9555_digital_write(XL9555_WIFI_EN_PIN, 1);
        rt_thread_mdelay(100);
    }
    else
    {
        xl9555_pin_mode(XL9555_WIFI_EN_PIN, XL9555_PIN_OUTPUT);
        xl9555_digital_write(XL9555_WIFI_EN_PIN, 0);
        rt_thread_mdelay(200);
    }
}

/**
 * @brief Hardware reset AW86224
 */
static void aw86224_hw_reset(void)
{
    xl9555_pin_mode(XL9555_AW86224_RST_PIN, XL9555_PIN_OUTPUT);
    xl9555_digital_write(XL9555_AW86224_RST_PIN, 0);
    rt_thread_mdelay(100);
    xl9555_digital_write(XL9555_AW86224_RST_PIN, 1);
    rt_thread_mdelay(200);
}

/**
 * @brief Write single byte to AW86224 register
 */
static rt_err_t aw86224_write_reg(rt_uint8_t reg, rt_uint8_t data)
{
    struct rt_i2c_msg msgs[2];
    rt_uint8_t buf[2];

    if (g_aw86224_dev.i2c_bus == RT_NULL)
        return -RT_ERROR;

    buf[0] = reg;
    buf[1] = data;

    msgs[0].addr = AW86224_I2C_ADDR;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = buf;
    msgs[0].len = 2;

    if (rt_i2c_transfer(g_aw86224_dev.i2c_bus, msgs, 1) == 1)
        return RT_EOK;
    else
        return -RT_ERROR;
}

/**
 * @brief Read single byte from AW86224 register
 */
static rt_err_t aw86224_read_reg(rt_uint8_t reg, rt_uint8_t *data)
{
    struct rt_i2c_msg msgs[2];

    if (g_aw86224_dev.i2c_bus == RT_NULL)
        return -RT_ERROR;

    msgs[0].addr = AW86224_I2C_ADDR;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = &reg;
    msgs[0].len = 1;

    msgs[1].addr = AW86224_I2C_ADDR;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = data;
    msgs[1].len = 1;

    if (rt_i2c_transfer(g_aw86224_dev.i2c_bus, msgs, 2) == 2)
        return RT_EOK;
    else
        return -RT_ERROR;
}

/**
 * @brief Write multiple bytes to AW86224 registers
 */
static rt_err_t aw86224_write_regs(rt_uint8_t reg, rt_uint8_t *data,
                                   rt_uint16_t len)
{
    struct rt_i2c_msg msgs[2];
    rt_uint8_t *buf;

    if (g_aw86224_dev.i2c_bus == RT_NULL)
        return -RT_ERROR;

    buf = rt_malloc(len + 1);
    if (buf == RT_NULL)
        return -RT_ENOMEM;

    buf[0] = reg;
    rt_memcpy(buf + 1, data, len);

    msgs[0].addr = AW86224_I2C_ADDR;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = buf;
    msgs[0].len = len + 1;

    rt_err_t ret = (rt_i2c_transfer(g_aw86224_dev.i2c_bus, msgs, 1) == 1)
                       ? RT_EOK
                       : -RT_ERROR;
    rt_free(buf);
    return ret;
}

/**
 * @brief Write bits to register with mask
 */
static rt_err_t aw86224_write_bits(rt_uint8_t reg, rt_uint8_t mask,
                                   rt_uint8_t data)
{
    rt_uint8_t reg_val;
    rt_err_t ret;

    ret = aw86224_read_reg(reg, &reg_val);
    if (ret != RT_EOK)
        return ret;

    reg_val &= mask;
    reg_val |= (data & (~mask));

    return aw86224_write_reg(reg, reg_val);
}

/**
 * @brief Software reset the device
 */
static rt_err_t aw86224_soft_reset(void)
{
    return aw86224_write_reg(AW86224_REG_SRST, 0xAA);
}

/**
 * @brief Wait for device to enter standby mode
 */
static rt_err_t aw86224_wait_standby(rt_uint32_t timeout_ms)
{
    rt_uint8_t state;
    rt_uint32_t elapsed = 0;
    rt_err_t ret;

    while (elapsed < timeout_ms)
    {
        ret = aw86224_read_reg(AW86224_REG_GLBRD5, &state);
        if (ret != RT_EOK)
            return ret;

        if ((state & 0x0F) == AW86224_STATE_STANDBY)
            return RT_EOK;

        rt_thread_mdelay(2);
        elapsed += 2;
    }

    LOG_W("Wait standby timeout, force standby");
    /* Force to standby */
    aw86224_write_bits(AW86224_REG_SYSCRTL2, ~(1 << 6), (1 << 6));
    aw86224_write_bits(AW86224_REG_SYSCRTL2, ~(1 << 6), (0 << 6));

    return -RT_ETIMEOUT;
}

/**
 * @brief Stop current playback
 */
rt_err_t aw86224_stop_playback(void)
{
    rt_err_t ret;

    ret = aw86224_write_bits(AW86224_REG_PLAYCFG4, ~(1 << 1), (1 << 1));
    if (ret != RT_EOK)
        return ret;

    return aw86224_wait_standby(200);
}

/**
 * @brief Check chip ID
 */
static rt_bool_t aw86224_check_chipid(void)
{
    rt_uint8_t chipid;
    rt_err_t ret;

    ret = aw86224_read_reg(AW86224_REG_CHIPID, &chipid);
    if (ret != RT_EOK)
    {
        LOG_E("Read chip ID failed");
        return RT_FALSE;
    }

    LOG_D("Chip ID: 0x%02X", chipid);

    /* AW86224: bit6=0, bit0=0 */
    if ((chipid & AW86224_CHIPID_H_MASK) == 0 &&
        (chipid & AW86224_CHIPID_L_MASK) == 0)
    {
        return RT_TRUE;
    }

    LOG_E("Invalid chip ID: 0x%02X, expected bit6=0, bit0=0", chipid);
    return RT_FALSE;
}

/**
 * @brief Initialize RAM with waveform data
 */
static rt_err_t aw86224_ram_init(rt_uint8_t *wave_data, rt_uint16_t len)
{
    rt_err_t ret;

    if (wave_data == RT_NULL || len == 0)
        return -RT_EINVAL;

    /* Enable digital module clock */
    ret = aw86224_write_bits(AW86224_REG_SYSCRTL1, ~(1 << 3), (1 << 3));
    if (ret != RT_EOK)
        return ret;

    /* Set base address - 2KB FIFO, 1KB RAM (base_addr = 0x0800) */
    ret = aw86224_write_reg(AW86224_REG_RTPCFG1, 0x08);
    if (ret != RT_EOK)
        goto exit;
    ret = aw86224_write_reg(AW86224_REG_RTPCFG2, 0x00);
    if (ret != RT_EOK)
        goto exit;

    /* Write waveform data to RAM */
    ret = aw86224_write_regs(AW86224_REG_RAMDATA, wave_data, len);
    if (ret != RT_EOK)
        goto exit;

    LOG_I("RAM initialized, %d bytes written", len);

exit:
    /* Disable digital module clock */
    aw86224_write_bits(AW86224_REG_SYSCRTL1, ~(1 << 3), (0 << 3));
    return ret;
}

/**
 * @brief Initialize chip configuration
 */
static rt_err_t aw86224_chip_init(void)
{
    rt_err_t ret;

    /* Set waveform sample rate to 12KHz */
    ret = aw86224_write_bits(AW86224_REG_SYSCRTL2, ~(3 << 0),
                             (AW86224_SRATE_12K << 0));
    if (ret != RT_EOK)
        return ret;

    /* Enable DC protection */
    ret = aw86224_write_bits(AW86224_REG_PWMCFG3, ~(1 << 7), (1 << 7));
    if (ret != RT_EOK)
        return ret;

    /* Enable gain bypass (allows gain change during playback) */
    ret = aw86224_write_bits(AW86224_REG_SYSCRTL7, ~(1 << 6), (1 << 6));
    if (ret != RT_EOK)
        return ret;

    /* Set gain to default 0x80 (100%) */
    ret = aw86224_write_reg(AW86224_REG_PLAYCFG2, 0x80);
    if (ret != RT_EOK)
        return ret;

    return RT_EOK;
}

/**
 * @brief Initialize AW86224 device
 * @return RT_EOK on success, negative error code on failure
 */
rt_err_t aw86224_init()
{
    rt_err_t ret;
    /* Power on */
    aw86224_power_enable(RT_TRUE);

    if (g_aw86224_dev.init_flag)
    {
        LOG_W("AW86224 already initialized");
        return RT_EOK;
    }

    /* Get I2C bus device */
    g_aw86224_dev.i2c_bus =
        (struct rt_i2c_bus_device *)rt_device_find(AW86224_I2C_BUS_NAME);
    if (g_aw86224_dev.i2c_bus == RT_NULL)
    {
        LOG_E("I2C bus %s not found", AW86224_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    if (rt_device_open((rt_device_t)g_aw86224_dev.i2c_bus,
                       RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        LOG_E("open %s device failed", AW86224_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    struct rt_i2c_configuration configuration = {
        .mode = 0,
        .addr = 0,
        .timeout = 500,   // ms
        .max_hz = 400000, // 400 kHz
    };
    rt_i2c_configure(g_aw86224_dev.i2c_bus, &configuration);

    /* Hardware reset */
    aw86224_hw_reset();

    /* Check chip ID */
    if (!aw86224_check_chipid())
    {
        LOG_E("Chip ID check failed");
        aw86224_power_enable(RT_FALSE);
        return -RT_ERROR;
    }

    /* Software reset */
    ret = aw86224_soft_reset();
    if (ret != RT_EOK)
    {
        LOG_E("Software reset failed");
        aw86224_power_enable(RT_FALSE);
        return ret;
    }
    rt_thread_mdelay(1);

    /* Initialize chip configuration */
    ret = aw86224_chip_init();
    if (ret != RT_EOK)
    {
        LOG_E("Chip init failed");
        aw86224_power_enable(RT_FALSE);
        return ret;
    }

    aw86224_set_gain(0x80);

    g_aw86224_dev.init_flag = 1;
    LOG_I("AW86224 initialized successfully");

    return RT_EOK;
}

/**
 * @brief Deinitialize AW86224 device
 */
rt_err_t aw86224_deinit(void)
{
    if (!g_aw86224_dev.init_flag)
        return RT_EOK;

    aw86224_power_enable(RT_FALSE);
    g_aw86224_dev.init_flag = 0;
    g_aw86224_dev.i2c_bus = RT_NULL;

    LOG_I("AW86224 deinitialized");
    return RT_EOK;
}

/**
 * @brief Load waveform data to RAM
 *
 * @param wave_data Waveform data array
 * @param len Length of waveform data
 * @return RT_EOK on success, negative error code on failure
 */
rt_err_t aw86224_load_waveform(rt_uint8_t *wave_data, rt_uint16_t len)
{
    if (!g_aw86224_dev.init_flag)
    {
        LOG_E("AW86224 not initialized");
        return -RT_ERROR;
    }

    return aw86224_ram_init(wave_data, len);
}

/**
 * @brief Set playback gain
 *
 * @param gain Gain value (0-255, 128 = 100%)
 * @return RT_EOK on success, negative error code on failure
 */
rt_err_t aw86224_set_gain(rt_uint8_t gain)
{
    if (!g_aw86224_dev.init_flag)
        return -RT_ERROR;

    return aw86224_write_reg(AW86224_REG_PLAYCFG2, gain);
}

/**
 * @brief Play waveform in RAM mode
 *
 * @param wave_id Waveform ID (1-127)
 * @param loop Loop count (0-14: play N+1 times, 15: infinite loop)
 * @param auto_brake Enable auto brake after playback
 * @return RT_EOK on success, negative error code on failure
 */
rt_err_t aw86224_play_ram(rt_uint8_t wave_id, rt_uint8_t loop,
                          rt_bool_t auto_brake)
{
    rt_err_t ret;

    if (!g_aw86224_dev.init_flag)
        return -RT_ERROR;

    if (wave_id == 0 || wave_id > 127)
        return -RT_EINVAL;

    /* Stop current playback */
    aw86224_stop_playback();

    /* Configure sequence */
    ret = aw86224_write_reg(AW86224_REG_WAVCFG1, wave_id);
    if (ret != RT_EOK)
        return ret;

    ret = aw86224_write_reg(AW86224_REG_WAVCFG2, 0x00);
    if (ret != RT_EOK)
        return ret;

    /* Set loop count */
    ret = aw86224_write_bits(AW86224_REG_WAVCFG9, ~(0x0F << 4), (loop << 4));
    if (ret != RT_EOK)
        return ret;

    /* Set play mode to RAM */
    ret = aw86224_write_bits(AW86224_REG_PLAYCFG3, ~(3 << 0),
                             (AW86224_PLAY_MODE_RAM << 0));
    if (ret != RT_EOK)
        return ret;

    /* Configure auto brake */
    ret = aw86224_write_bits(AW86224_REG_PLAYCFG3, ~(1 << 2),
                             (auto_brake ? (1 << 2) : 0));
    if (ret != RT_EOK)
        return ret;

    /* Start playback */
    ret = aw86224_write_bits(AW86224_REG_PLAYCFG4, ~(1 << 0), (1 << 0));
    if (ret != RT_EOK)
        return ret;

    LOG_D("RAM playback started: ID=%d, loop=%d", wave_id, loop);
    return RT_EOK;
}

/**
 * @brief Play waveform in RTP mode (Real-Time Playback)
 *
 * @param data RTP data buffer
 * @param len Data length
 * @param auto_brake Enable auto brake after playback
 * @return RT_EOK on success, negative error code on failure
 */
rt_err_t aw86224_play_rtp(rt_uint8_t *data, rt_uint16_t len,
                          rt_bool_t auto_brake)
{
    rt_err_t ret;

    if (!g_aw86224_dev.init_flag)
        return -RT_ERROR;

    if (data == RT_NULL || len == 0)
        return -RT_EINVAL;

    /* Stop current playback */
    aw86224_stop_playback();

    /* Set play mode to RTP */
    ret = aw86224_write_bits(AW86224_REG_PLAYCFG3, ~(3 << 0),
                             (AW86224_PLAY_MODE_RTP << 0));
    if (ret != RT_EOK)
        return ret;

    /* Configure auto brake */
    ret = aw86224_write_bits(AW86224_REG_PLAYCFG3, ~(1 << 2),
                             (auto_brake ? (1 << 2) : 0));
    if (ret != RT_EOK)
        return ret;

    /* Start RTP mode */
    ret = aw86224_write_bits(AW86224_REG_PLAYCFG4, ~(1 << 0), (1 << 0));
    if (ret != RT_EOK)
        return ret;

    rt_thread_mdelay(1);

    /* Write RTP data */
    ret = aw86224_write_regs(AW86224_REG_RTPDATA, data, len);
    if (ret != RT_EOK)
        return ret;

    LOG_D("RTP playback started, %d bytes", len);
    return RT_EOK;
}

/**
 * @brief Play CONT mode (continuous vibration)
 *
 * @param f0_target Target F0 frequency in Hz*10 (e.g., 1700 for 170Hz)
 * @param duration_ms Duration in milliseconds
 * @param strength Strength (0-127)
 * @return RT_EOK on success, negative error code on failure
 */
rt_err_t aw86224_play_cont(rt_uint32_t f0_target, rt_uint32_t duration_ms,
                           rt_uint8_t strength)
{
    rt_err_t ret;
    rt_uint32_t f0_pre;
    rt_uint8_t drv_width, drv2_lvl;

    if (!g_aw86224_dev.init_flag)
        return -RT_ERROR;

    if (strength > 127)
        strength = 127;

    f0_pre = f0_target;

    /* Stop current playback */
    aw86224_stop_playback();

    /* Set play mode to CONT */
    ret = aw86224_write_bits(AW86224_REG_PLAYCFG3, ~(3 << 0),
                             (AW86224_PLAY_MODE_CONT << 0));
    if (ret != RT_EOK)
        return ret;

    /* Disable F0 detection for simple CONT play */
    ret = aw86224_write_bits(AW86224_REG_CONTCFG1, ~(1 << 3), (0 << 3));
    if (ret != RT_EOK)
        return ret;

    /* Disable auto brake */
    ret = aw86224_write_bits(AW86224_REG_PLAYCFG3, ~(1 << 2), 0);
    if (ret != RT_EOK)
        return ret;

    /* Set drive level */
    ret = aw86224_write_reg(AW86224_REG_CONTCFG6, 0x80 | (strength & 0x7F));
    if (ret != RT_EOK)
        return ret;

    /* Calculate and set drive width */
    drv_width = (240000 / f0_pre) - 8 - 8 - 15;
    if (drv_width > 0xFF)
        drv_width = 0xFF;
    ret = aw86224_write_reg(AW86224_REG_CONTCFG3, drv_width);
    if (ret != RT_EOK)
        return ret;

    /* Set drive times */
    ret = aw86224_write_reg(AW86224_REG_CONTCFG8, 0x04);
    if (ret != RT_EOK)
        return ret;

    drv2_lvl = strength;
    ret = aw86224_write_reg(AW86224_REG_CONTCFG7, drv2_lvl);
    if (ret != RT_EOK)
        return ret;

    ret = aw86224_write_reg(AW86224_REG_CONTCFG9, 0x06);
    if (ret != RT_EOK)
        return ret;

    /* Start playback */
    ret = aw86224_write_bits(AW86224_REG_PLAYCFG4, ~(1 << 0), (1 << 0));
    if (ret != RT_EOK)
        return ret;

    LOG_D("CONT playback started");

    /* Wait for duration then stop */
    if (duration_ms > 0)
    {
        rt_thread_mdelay(duration_ms);
        aw86224_stop_playback();
    }

    return RT_EOK;
}

/**
 * @brief Detect LRA F0 frequency
 *
 * @param f0 Output F0 frequency in Hz*10
 * @return RT_EOK on success, negative error code on failure
 */
rt_err_t aw86224_f0_detect(rt_uint32_t *f0)
{
    rt_err_t ret;
    rt_uint8_t reg_val;
    rt_uint32_t f0_raw;

    if (!g_aw86224_dev.init_flag)
        return -RT_ERROR;

    if (f0 == RT_NULL)
        return -RT_EINVAL;

    /* Stop current playback */
    aw86224_stop_playback();

    /* Reset TRIM_LRA */
    aw86224_write_reg(AW86224_REG_TRIMCFG3, 0x00);

    /* Enter CONT mode with F0 detection */
    ret = aw86224_write_bits(AW86224_REG_PLAYCFG3, ~(3 << 0),
                             (AW86224_PLAY_MODE_CONT << 0));
    if (ret != RT_EOK)
        return ret;

    /* Enable F0 detection and tracking */
    ret = aw86224_write_bits(AW86224_REG_CONTCFG1, ~(1 << 3), (1 << 3));
    if (ret != RT_EOK)
        return ret;

    ret = aw86224_write_bits(AW86224_REG_CONTCFG6, ~(1 << 7), (1 << 7));
    if (ret != RT_EOK)
        return ret;

    /* Enable auto brake */
    ret = aw86224_write_bits(AW86224_REG_PLAYCFG3, ~(1 << 2), (1 << 2));
    if (ret != RT_EOK)
        return ret;

    /* Configure drive parameters */
    aw86224_write_reg(AW86224_REG_CONTCFG6, 0xFF);
    aw86224_write_reg(AW86224_REG_CONTCFG7, 0x7F);
    aw86224_write_reg(AW86224_REG_CONTCFG8, 0x04);
    aw86224_write_reg(AW86224_REG_CONTCFG9, 0x06);
    aw86224_write_reg(AW86224_REG_CONTCFG11, 0x0F);

    /* Start playback */
    ret = aw86224_write_bits(AW86224_REG_PLAYCFG4, ~(1 << 0), (1 << 0));
    if (ret != RT_EOK)
        return ret;

    /* Wait for F0 detection to complete */
    rt_thread_mdelay(300);

    /* Read F0 value */
    aw86224_read_reg(AW86224_REG_CONTRD14, &reg_val);
    f0_raw = reg_val << 8;
    aw86224_read_reg(AW86224_REG_CONTRD15, &reg_val);
    f0_raw |= reg_val;

    if (f0_raw > 0)
        *f0 = 3840000 / f0_raw;
    else
        *f0 = 0;

    LOG_I("F0 detected: %d.%d Hz", *f0 / 10, *f0 % 10);

    /* Disable F0 detection */
    aw86224_write_bits(AW86224_REG_CONTCFG1, ~(1 << 3), (0 << 3));
    aw86224_write_bits(AW86224_REG_PLAYCFG3, ~(1 << 2), (0 << 2));
    aw86224_stop_playback();

    return RT_EOK;
}

/**
 * @brief Calibrate LRA F0 and save to chip
 *
 * @param f0_pre Expected F0 frequency in Hz*10
 * @param f0_measured Measured F0 frequency in Hz*10
 * @return RT_EOK on success, negative error code on failure
 */
rt_err_t aw86224_f0_calibrate(rt_uint32_t f0_pre, rt_uint32_t f0_measured)
{
    rt_int32_t f0_cali_step;
    rt_uint8_t f0_cali_lra;
    rt_uint32_t f0_min, f0_max;

    if (!g_aw86224_dev.init_flag)
        return -RT_ERROR;

    /* Check if measured F0 is within ±7% of expected */
    f0_min = f0_pre * 93 / 100;
    f0_max = f0_pre * 107 / 100;

    if (f0_measured < f0_min || f0_measured > f0_max)
    {
        LOG_W("F0 out of calibration range: %d.%d Hz", f0_measured / 10,
              f0_measured % 10);
        return -RT_EINVAL;
    }

    /* Calculate calibration step */
    f0_cali_step = 100000 * ((rt_int32_t)f0_measured - (rt_int32_t)f0_pre) /
                   ((rt_int32_t)f0_pre * 24);

    if (f0_cali_step >= 0)
    {
        if (f0_cali_step >= 5)
            f0_cali_step = 32 + (f0_cali_step / 10 + 1);
        else
            f0_cali_step = 32 + f0_cali_step / 10;
    }
    else
    {
        if (f0_cali_step <= -5)
            f0_cali_step = 32 + (f0_cali_step / 10 - 1);
        else
            f0_cali_step = 32 + f0_cali_step / 10;
    }

    if (f0_cali_step > 31)
        f0_cali_lra = (rt_uint8_t)(f0_cali_step - 32);
    else
        f0_cali_lra = (rt_uint8_t)(f0_cali_step + 32);

    LOG_I("F0 calibration value: 0x%02X", f0_cali_lra & 0x3F);

    /* Write calibration value to chip */
    aw86224_write_reg(AW86224_REG_TRIMCFG3, f0_cali_lra & 0x3F);

    g_aw86224_dev.f0_calibrated = f0_measured;

    return RT_EOK;
}

/**
 * @brief Measure LRA resistance
 *
 * @param resistance Output resistance in mΩ
 * @return RT_EOK on success, negative error code on failure
 */
rt_err_t aw86224_measure_resistance(rt_uint32_t *resistance)
{
    rt_err_t ret;
    rt_uint8_t d2s_gain_pre, d2s_gain, reg_val;
    rt_uint32_t rl_raw;

    if (!g_aw86224_dev.init_flag)
        return -RT_ERROR;

    if (resistance == RT_NULL)
        return -RT_EINVAL;

    /* Save current D2S_GAIN */
    aw86224_read_reg(AW86224_REG_SYSCRTL7, &d2s_gain_pre);
    d2s_gain_pre &= 0x07;

    /* Set appropriate D2S_GAIN */
    d2s_gain = 10; /* For RL 31-60Ω, use D2S_GAIN=10 */
    ret = aw86224_write_bits(AW86224_REG_SYSCRTL7, ~(0x07), d2s_gain);
    if (ret != RT_EOK)
        return ret;

    /* Enable RAM clock */
    ret = aw86224_write_bits(AW86224_REG_SYSCRTL1, ~(1 << 3), (1 << 3));
    if (ret != RT_EOK)
        goto restore;

    /* Enable RL measurement mode */
    ret = aw86224_write_bits(AW86224_REG_DETCFG1, ~(1 << 4), (1 << 4));
    if (ret != RT_EOK)
        goto restore;

    /* Start measurement */
    ret = aw86224_write_bits(AW86224_REG_DETCFG2, ~(1 << 0), (1 << 0));
    if (ret != RT_EOK)
        goto restore;

    rt_thread_mdelay(3);

    /* Read result */
    aw86224_read_reg(AW86224_REG_DET_RL, &reg_val);
    rl_raw = reg_val << 2;
    aw86224_read_reg(AW86224_REG_DET_LO, &reg_val);
    rl_raw |= (reg_val & 0x03);

    /* Calculate resistance: RL = (rl_raw * 678) / (1024 * d2s_gain) Ω */
    *resistance = (rl_raw * 678) / (1024 * d2s_gain);

    LOG_I("LRA resistance: %d mΩ", *resistance);

restore:
    /* Restore settings */
    aw86224_write_bits(AW86224_REG_DETCFG1, ~(1 << 4), 0);
    aw86224_write_bits(AW86224_REG_SYSCRTL1, ~(1 << 3), 0);
    aw86224_write_bits(AW86224_REG_SYSCRTL7, ~(0x07), d2s_gain_pre & 0x07);

    return ret;
}

/**
 * @brief Measure battery voltage
 *
 * @param voltage Output voltage in mV
 * @return RT_EOK on success, negative error code on failure
 */
rt_err_t aw86224_measure_vbat(rt_uint32_t *voltage)
{
    rt_err_t ret;
    rt_uint8_t reg_val;
    rt_uint32_t vbat_raw;

    if (!g_aw86224_dev.init_flag)
        return -RT_ERROR;

    if (voltage == RT_NULL)
        return -RT_EINVAL;

    /* Enable RAM clock */
    ret = aw86224_write_bits(AW86224_REG_SYSCRTL1, ~(1 << 3), (1 << 3));
    if (ret != RT_EOK)
        return ret;

    /* Start VBAT measurement */
    ret = aw86224_write_bits(AW86224_REG_DETCFG2, ~(1 << 1), (1 << 1));
    if (ret != RT_EOK)
        goto exit;

    rt_thread_mdelay(3);

    /* Read result */
    aw86224_read_reg(AW86224_REG_DET_VBAT, &reg_val);
    vbat_raw = reg_val << 2;
    aw86224_read_reg(AW86224_REG_DET_LO, &reg_val);
    vbat_raw |= ((reg_val >> 4) & 0x03);

    /* Calculate voltage: VDD = (vbat_raw * 6.1) / 1024 V */
    *voltage = (vbat_raw * 6100) / 1024;

    LOG_I("VBAT: %d mV", *voltage);

exit:
    aw86224_write_bits(AW86224_REG_SYSCRTL1, ~(1 << 3), 0);
    return ret;
}

/**
 * @brief Get device state
 *
 * @return Current state (0=STANDBY, 6=CONT, 7=RAM, 8=RTP, 9=TRIG, 11=BRAKE)
 */
rt_uint8_t aw86224_get_state(void)
{
    rt_uint8_t state = 0;

    if (!g_aw86224_dev.init_flag)
        return 0;

    aw86224_read_reg(AW86224_REG_GLBRD5, &state);
    return (state & 0x0F);
}

/**
 * @brief Check if device is playing
 *
 * @return RT_TRUE if playing, RT_FALSE otherwise
 */
rt_bool_t aw86224_is_playing(void)
{
    if (!g_aw86224_dev.init_flag)
        return -RT_ERROR;

    rt_uint8_t state = aw86224_get_state();
    return (state != AW86224_STATE_STANDBY);
}

/**
 * @brief Wait for playback to complete
 *
 * @param timeout_ms Timeout in milliseconds
 * @return RT_EOK on success, -RT_ETIMEOUT on timeout
 */
rt_err_t aw86224_wait_complete(rt_uint32_t timeout_ms)
{
    if (!g_aw86224_dev.init_flag)
        return -RT_ERROR;

    return aw86224_wait_standby(timeout_ms);
}

/**
 * @brief Play RAM waveform once (short vibration)
 *
 * @param wave_id Waveform ID (1-127)
 * @param auto_brake Enable auto brake after playback
 * @return RT_EOK on success, negative error code on failure
 */
rt_err_t aw86224_play_ram_short(rt_uint8_t wave_id, rt_bool_t auto_brake)
{
    if (!g_aw86224_dev.init_flag)
        return -RT_ERROR;
    return aw86224_play_ram(wave_id, 0, auto_brake);
}

/**
 * @brief Play RAM waveform continuously (long vibration)
 *
 * @param wave_id Waveform ID (1-127)
 * @param loop Loop count: 0 for infinite loop, 1-14 for specified times
 * @param auto_brake Enable auto brake after playback
 * @return RT_EOK on success, negative error code on failure
 */
rt_err_t aw86224_play_ram_long(rt_uint8_t wave_id, rt_uint8_t loop,
                               rt_bool_t auto_brake)
{
    if (!g_aw86224_dev.init_flag)
        return -RT_ERROR;

    rt_uint8_t actual_loop = loop;

    if (loop == 0)
        actual_loop = 0x0F; /* Infinite loop per software design guide */
    else if (loop > 14)
        actual_loop = 14;

    return aw86224_play_ram(wave_id, actual_loop, auto_brake);
}
/* Export commands to MSH */
#ifdef RT_USING_FINSH
    #include <finsh.h>

static void aw86224_test(rt_int32_t argc, char **argv)
{
    rt_err_t ret;
    rt_uint32_t f0, res, vbat;

    if (argc < 2)
    {
        rt_kprintf("Usage:\n");
        rt_kprintf("  aw86224_test init <i2c_bus>  - Initialize AW86224\n");
        rt_kprintf("  aw86224_test deinit          - Deinitialize AW86224\n");
        rt_kprintf("  aw86224_test ram <id> <loop> - Play RAM waveform\n");
        rt_kprintf("  aw86224_test cont <ms>       - Play CONT mode\n");
        rt_kprintf("  aw86224_test stop            - Stop playback\n");
        rt_kprintf("  aw86224_test f0              - Detect F0\n");
        rt_kprintf("  aw86224_test res             - Measure resistance\n");
        rt_kprintf("  aw86224_test vbat            - Measure VBAT\n");
        rt_kprintf("  aw86224_test state           - Get state\n");
        return;
    }

    if (rt_strcmp(argv[1], "init") == 0)
    {
        if (argc < 3)
        {
            rt_kprintf("Please specify I2C bus name\n");
            return;
        }
        ret = aw86224_init(argv[2]);
        if (ret == RT_EOK)
            rt_kprintf("AW86224 initialized\n");
        else
            rt_kprintf("AW86224 init failed: %d\n", ret);
    }
    else if (rt_strcmp(argv[1], "deinit") == 0)
    {
        aw86224_deinit();
        rt_kprintf("AW86224 deinitialized\n");
    }
    else if (rt_strcmp(argv[1], "ram") == 0)
    {
        rt_uint8_t id = 1, loop = 0;
        if (argc > 2)
            id = atoi(argv[2]);
        if (argc > 3)
            loop = atoi(argv[3]);
        aw86224_play_ram(id, loop, RT_TRUE);
        rt_kprintf("RAM playback: ID=%d, loop=%d\n", id, loop);
    }
    else if (rt_strcmp(argv[1], "cont") == 0)
    {
        rt_uint32_t ms = 1000;
        if (argc > 2)
            ms = atoi(argv[2]);
        aw86224_play_cont(1700, ms, 100);
        rt_kprintf("CONT playback: %d ms\n", ms);
    }
    else if (rt_strcmp(argv[1], "stop") == 0)
    {
        aw86224_stop_playback();
        rt_kprintf("Playback stopped\n");
    }
    else if (rt_strcmp(argv[1], "f0") == 0)
    {
        ret = aw86224_f0_detect(&f0);
        if (ret == RT_EOK)
            rt_kprintf("F0: %d.%d Hz\n", f0 / 10, f0 % 10);
    }
    else if (rt_strcmp(argv[1], "res") == 0)
    {
        ret = aw86224_measure_resistance(&res);
        if (ret == RT_EOK)
            rt_kprintf("Resistance: %d mΩ\n", res);
    }
    else if (rt_strcmp(argv[1], "vbat") == 0)
    {
        ret = aw86224_measure_vbat(&vbat);
        if (ret == RT_EOK)
            rt_kprintf("VBAT: %d mV\n", vbat);
    }
    else if (rt_strcmp(argv[1], "state") == 0)
    {
        rt_kprintf("State: 0x%02X\n", aw86224_get_state());
    }
    else
    {
        rt_kprintf("Unknown command\n");
    }
}
MSH_CMD_EXPORT(aw86224_test, AW86224 test command);
#endif /* RT_USING_FINSH */