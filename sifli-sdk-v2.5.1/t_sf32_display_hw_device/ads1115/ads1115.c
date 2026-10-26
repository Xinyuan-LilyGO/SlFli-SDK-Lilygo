/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ads1115.h"

#define DBG_TAG "ads1115"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define ADS1115_CONFIG_MUX_SHIFT      12U
#define ADS1115_CONFIG_PGA_SHIFT      9U
#define ADS1115_CONFIG_RATE_SHIFT     5U
#define ADS1115_SINGLE_TIMEOUT_MS     200U

static ads1115_device_t g_ads1115;

static rt_bool_t ads1115_address_valid(rt_uint8_t address)
{
    return (address >= ADS1115_ADDR_GND && address <= ADS1115_ADDR_SCL);
}

static rt_bool_t ads1115_config_valid(ads1115_mux_t mux, ads1115_pga_t pga,
                                     ads1115_rate_t rate)
{
    return (mux <= ADS1115_MUX_AIN3_GND &&
            pga <= ADS1115_PGA_0_256V &&
            rate <= ADS1115_RATE_860_SPS);
}

static rt_uint16_t ads1115_build_config(ads1115_mux_t mux,
                                       ads1115_pga_t pga,
                                       ads1115_rate_t rate,
                                       rt_bool_t single_shot)
{
    rt_uint16_t config;

    config = (rt_uint16_t)(((rt_uint16_t)mux << ADS1115_CONFIG_MUX_SHIFT) |
                           ((rt_uint16_t)pga << ADS1115_CONFIG_PGA_SHIFT) |
                           ((rt_uint16_t)rate << ADS1115_CONFIG_RATE_SHIFT) |
                           ADS1115_CONFIG_COMP_DISABLE);
    if (single_shot)
    {
        config |= ADS1115_CONFIG_OS | ADS1115_CONFIG_MODE_SINGLE;
    }

    return config;
}

static rt_err_t ads1115_read_register_unlocked(ads1115_device_t *dev,
                                                rt_uint8_t reg,
                                                rt_uint16_t *value)
{
    struct rt_i2c_msg messages[2];
    rt_uint8_t data[2];

    messages[0].addr = dev->address;
    messages[0].flags = RT_I2C_WR;
    messages[0].buf = &reg;
    messages[0].len = 1;

    messages[1].addr = dev->address;
    messages[1].flags = RT_I2C_RD;
    messages[1].buf = data;
    messages[1].len = sizeof(data);

    if (rt_i2c_transfer(dev->i2c_bus, messages, 2) != 2)
    {
        LOG_E("read register 0x%02x failed", reg);
        return -RT_EIO;
    }

    *value = (rt_uint16_t)(((rt_uint16_t)data[0] << 8) | data[1]);
    return RT_EOK;
}

static rt_err_t ads1115_write_register_unlocked(ads1115_device_t *dev,
                                                 rt_uint8_t reg,
                                                 rt_uint16_t value)
{
    struct rt_i2c_msg message;
    rt_uint8_t data[3];

    data[0] = reg;
    data[1] = (rt_uint8_t)(value >> 8);
    data[2] = (rt_uint8_t)value;

    message.addr = dev->address;
    message.flags = RT_I2C_WR;
    message.buf = data;
    message.len = sizeof(data);

    if (rt_i2c_transfer(dev->i2c_bus, &message, 1) != 1)
    {
        LOG_E("write register 0x%02x failed", reg);
        return -RT_EIO;
    }

    return RT_EOK;
}

static rt_err_t ads1115_check_device(ads1115_device_t *dev)
{
    if (dev == RT_NULL || !dev->initialized || dev->i2c_bus == RT_NULL)
    {
        return -RT_EINVAL;
    }

    return RT_EOK;
}

rt_err_t ads1115_init(void)
{
    if (g_ads1115.initialized)
    {
        return RT_EOK;
    }

    return ads1115_init_device(ADS1115_I2C_BUS_NAME,
                               (rt_uint8_t)ADS1115_I2C_ADDR);
}

rt_err_t ads1115_init_device(const char *i2c_bus_name, rt_uint8_t address)
{
    rt_err_t result;
    rt_uint16_t config;

    if (i2c_bus_name == RT_NULL || !ads1115_address_valid(address))
    {
        return -RT_EINVAL;
    }
    if (g_ads1115.initialized)
    {
        return -RT_EBUSY;
    }

    rt_memset(&g_ads1115, 0, sizeof(g_ads1115));
    g_ads1115.i2c_bus = rt_i2c_bus_device_find(i2c_bus_name);
    if (g_ads1115.i2c_bus == RT_NULL)
    {
        LOG_E("I2C bus %s not found", i2c_bus_name);
        return -RT_ENOSYS;
    }

    result = rt_device_open((rt_device_t)g_ads1115.i2c_bus,
                            RT_DEVICE_OFLAG_RDWR);
    if (result != RT_EOK)
    {
        LOG_E("open I2C bus %s failed: %d", i2c_bus_name, result);
        g_ads1115.i2c_bus = RT_NULL;
        return result;
    }

    result = rt_mutex_init(&g_ads1115.lock, "ads1115", RT_IPC_FLAG_PRIO);
    if (result != RT_EOK)
    {
        rt_device_close((rt_device_t)g_ads1115.i2c_bus);
        g_ads1115.i2c_bus = RT_NULL;
        return result;
    }

    g_ads1115.address = address;
    g_ads1115.pga = ADS1115_PGA_2_048V;
    g_ads1115.rate = ADS1115_RATE_128_SPS;
    g_ads1115.initialized = RT_TRUE;

    result = ads1115_read_register(&g_ads1115, ADS1115_REG_CONFIG, &config);
    if (result != RT_EOK)
    {
        ads1115_deinit();
        return result;
    }

    LOG_I("ready on %s at 0x%02x, config 0x%04x", i2c_bus_name,
          (unsigned int)address, (unsigned int)config);
    return RT_EOK;
}

void ads1115_deinit(void)
{
    if (!g_ads1115.initialized)
    {
        return;
    }

    (void)ads1115_stop_continuous(&g_ads1115);
    rt_mutex_detach(&g_ads1115.lock);
    rt_device_close((rt_device_t)g_ads1115.i2c_bus);
    rt_memset(&g_ads1115, 0, sizeof(g_ads1115));
}

ads1115_device_t *ads1115_get_device(void)
{
    return g_ads1115.initialized ? &g_ads1115 : RT_NULL;
}

rt_err_t ads1115_read_register(ads1115_device_t *dev, rt_uint8_t reg,
                               rt_uint16_t *value)
{
    rt_err_t result;

    if (ads1115_check_device(dev) != RT_EOK || value == RT_NULL ||
        reg > ADS1115_REG_HI_THRESH)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);
    result = ads1115_read_register_unlocked(dev, reg, value);
    rt_mutex_release(&dev->lock);
    return result;
}

rt_err_t ads1115_write_register(ads1115_device_t *dev, rt_uint8_t reg,
                                rt_uint16_t value)
{
    rt_err_t result;

    if (ads1115_check_device(dev) != RT_EOK ||
        reg < ADS1115_REG_CONFIG || reg > ADS1115_REG_HI_THRESH)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);
    result = ads1115_write_register_unlocked(dev, reg, value);
    rt_mutex_release(&dev->lock);
    return result;
}

rt_err_t ads1115_read_single_shot(ads1115_device_t *dev, ads1115_mux_t mux,
                                  ads1115_pga_t pga, ads1115_rate_t rate,
                                  rt_int16_t *raw, rt_int32_t *microvolts)
{
    rt_err_t result;
    rt_uint16_t value;
    rt_tick_t deadline;

    if (ads1115_check_device(dev) != RT_EOK || raw == RT_NULL ||
        !ads1115_config_valid(mux, pga, rate))
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);

    value = ads1115_build_config(mux, pga, rate, RT_TRUE);
    result = ads1115_write_register_unlocked(dev, ADS1115_REG_CONFIG, value);
    if (result != RT_EOK)
    {
        goto exit;
    }

    deadline = rt_tick_get() + rt_tick_from_millisecond(ADS1115_SINGLE_TIMEOUT_MS);
    do
    {
        result = ads1115_read_register_unlocked(dev, ADS1115_REG_CONFIG,
                                                &value);
        if (result != RT_EOK)
        {
            goto exit;
        }
        if ((value & ADS1115_CONFIG_OS) != 0U)
        {
            break;
        }
        rt_thread_mdelay(1);
    } while ((rt_int32_t)(deadline - rt_tick_get()) > 0);

    if ((value & ADS1115_CONFIG_OS) == 0U)
    {
        result = -RT_ETIMEOUT;
        goto exit;
    }

    result = ads1115_read_register_unlocked(dev, ADS1115_REG_CONVERSION,
                                            &value);
    if (result == RT_EOK)
    {
        *raw = (rt_int16_t)value;
        if (microvolts != RT_NULL)
        {
            *microvolts = ads1115_raw_to_microvolts(*raw, pga);
        }
        dev->pga = pga;
        dev->rate = rate;
    }

exit:
    rt_mutex_release(&dev->lock);
    return result;
}

rt_err_t ads1115_read_single_ended(ads1115_device_t *dev, rt_uint8_t channel,
                                   ads1115_pga_t pga, ads1115_rate_t rate,
                                   rt_int16_t *raw, rt_int32_t *microvolts)
{
    if (channel > 3U)
    {
        return -RT_EINVAL;
    }

    return ads1115_read_single_shot(dev,
                                    (ads1115_mux_t)(ADS1115_MUX_AIN0_GND +
                                                    channel),
                                    pga, rate, raw, microvolts);
}

rt_err_t ads1115_read_differential(ads1115_device_t *dev,
                                   ads1115_mux_t differential_mux,
                                   ads1115_pga_t pga, ads1115_rate_t rate,
                                   rt_int16_t *raw, rt_int32_t *microvolts)
{
    if (differential_mux > ADS1115_MUX_AIN2_AIN3)
    {
        return -RT_EINVAL;
    }

    return ads1115_read_single_shot(dev, differential_mux, pga, rate, raw,
                                    microvolts);
}

rt_err_t ads1115_start_continuous(ads1115_device_t *dev, ads1115_mux_t mux,
                                  ads1115_pga_t pga, ads1115_rate_t rate)
{
    rt_err_t result;
    rt_uint16_t config;

    if (ads1115_check_device(dev) != RT_EOK ||
        !ads1115_config_valid(mux, pga, rate))
    {
        return -RT_EINVAL;
    }

    config = ads1115_build_config(mux, pga, rate, RT_FALSE);
    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);
    result = ads1115_write_register_unlocked(dev, ADS1115_REG_CONFIG, config);
    if (result == RT_EOK)
    {
        dev->pga = pga;
        dev->rate = rate;
    }
    rt_mutex_release(&dev->lock);
    return result;
}

rt_err_t ads1115_stop_continuous(ads1115_device_t *dev)
{
    rt_err_t result;
    rt_uint16_t config;

    if (ads1115_check_device(dev) != RT_EOK)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);
    result = ads1115_read_register_unlocked(dev, ADS1115_REG_CONFIG, &config);
    if (result == RT_EOK)
    {
        config |= ADS1115_CONFIG_MODE_SINGLE;
        result = ads1115_write_register_unlocked(dev, ADS1115_REG_CONFIG,
                                                 config);
    }
    rt_mutex_release(&dev->lock);
    return result;
}

rt_err_t ads1115_read_conversion(ads1115_device_t *dev, rt_int16_t *raw,
                                 rt_int32_t *microvolts)
{
    rt_err_t result;
    rt_uint16_t value;

    if (ads1115_check_device(dev) != RT_EOK || raw == RT_NULL)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);
    result = ads1115_read_register_unlocked(dev, ADS1115_REG_CONVERSION,
                                            &value);
    if (result == RT_EOK)
    {
        *raw = (rt_int16_t)value;
        if (microvolts != RT_NULL)
        {
            *microvolts = ads1115_raw_to_microvolts(*raw, dev->pga);
        }
    }
    rt_mutex_release(&dev->lock);
    return result;
}

rt_err_t ads1115_is_conversion_ready(ads1115_device_t *dev, rt_bool_t *ready)
{
    rt_err_t result;
    rt_uint16_t config;

    if (ready == RT_NULL)
    {
        return -RT_EINVAL;
    }

    result = ads1115_read_register(dev, ADS1115_REG_CONFIG, &config);
    if (result == RT_EOK)
    {
        *ready = ((config & ADS1115_CONFIG_OS) != 0U) ? RT_TRUE : RT_FALSE;
    }
    return result;
}

rt_err_t ads1115_configure_comparator(ads1115_device_t *dev,
                                      ads1115_comp_mode_t mode,
                                      ads1115_comp_polarity_t polarity,
                                      rt_bool_t latch,
                                      ads1115_comp_queue_t queue,
                                      rt_int16_t low_threshold,
                                      rt_int16_t high_threshold)
{
    rt_err_t result;
    rt_uint16_t config;

    if (ads1115_check_device(dev) != RT_EOK || mode > ADS1115_COMP_WINDOW ||
        polarity > ADS1115_COMP_ACTIVE_HIGH ||
        queue > ADS1115_COMP_DISABLED || low_threshold >= high_threshold)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);
    result = ads1115_write_register_unlocked(dev, ADS1115_REG_LO_THRESH,
                                             (rt_uint16_t)low_threshold);
    if (result != RT_EOK)
    {
        goto exit;
    }
    result = ads1115_write_register_unlocked(dev, ADS1115_REG_HI_THRESH,
                                             (rt_uint16_t)high_threshold);
    if (result != RT_EOK)
    {
        goto exit;
    }
    result = ads1115_read_register_unlocked(dev, ADS1115_REG_CONFIG, &config);
    if (result != RT_EOK)
    {
        goto exit;
    }

    config &= (rt_uint16_t)~(ADS1115_CONFIG_COMP_MODE |
                             ADS1115_CONFIG_COMP_POL |
                             ADS1115_CONFIG_COMP_LAT |
                             ADS1115_CONFIG_COMP_QUE_MASK);
    if (mode == ADS1115_COMP_WINDOW)
    {
        config |= ADS1115_CONFIG_COMP_MODE;
    }
    if (polarity == ADS1115_COMP_ACTIVE_HIGH)
    {
        config |= ADS1115_CONFIG_COMP_POL;
    }
    if (latch)
    {
        config |= ADS1115_CONFIG_COMP_LAT;
    }
    config |= (rt_uint16_t)queue;
    result = ads1115_write_register_unlocked(dev, ADS1115_REG_CONFIG, config);

exit:
    rt_mutex_release(&dev->lock);
    return result;
}

rt_err_t ads1115_enable_conversion_ready_pin(ads1115_device_t *dev,
                                             rt_bool_t active_high)
{
    rt_err_t result;
    rt_uint16_t config;

    if (ads1115_check_device(dev) != RT_EOK)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);
    result = ads1115_write_register_unlocked(dev, ADS1115_REG_LO_THRESH,
                                             0x0000U);
    if (result != RT_EOK)
    {
        goto exit;
    }
    result = ads1115_write_register_unlocked(dev, ADS1115_REG_HI_THRESH,
                                             0x8000U);
    if (result != RT_EOK)
    {
        goto exit;
    }
    result = ads1115_read_register_unlocked(dev, ADS1115_REG_CONFIG, &config);
    if (result != RT_EOK)
    {
        goto exit;
    }

    config &= (rt_uint16_t)~(ADS1115_CONFIG_COMP_MODE |
                             ADS1115_CONFIG_COMP_POL |
                             ADS1115_CONFIG_COMP_LAT |
                             ADS1115_CONFIG_COMP_QUE_MASK);
    if (active_high)
    {
        config |= ADS1115_CONFIG_COMP_POL;
    }
    result = ads1115_write_register_unlocked(dev, ADS1115_REG_CONFIG, config);

exit:
    rt_mutex_release(&dev->lock);
    return result;
}

rt_int32_t ads1115_raw_to_microvolts(rt_int16_t raw, ads1115_pga_t pga)
{
    static const rt_int32_t full_scale_uv[] =
    {
        6144000, 4096000, 2048000, 1024000, 512000, 256000
    };

    if (pga > ADS1115_PGA_0_256V)
    {
        return 0;
    }

    return (rt_int32_t)(((rt_int64_t)raw * full_scale_uv[pga]) / 32768);
}
