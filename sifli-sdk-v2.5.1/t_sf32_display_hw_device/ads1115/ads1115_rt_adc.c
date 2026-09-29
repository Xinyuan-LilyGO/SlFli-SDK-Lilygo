/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ads1115_rt_adc.h"

#include "ads1115.h"
#include <drivers/adc.h>
#include <rtdevice.h>

#define DBG_TAG "ads1115.adc"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define MICROVOLTS_PER_0P1_MV 100

#ifndef ADS1115_ADC_DEFAULT_PGA
#define ADS1115_ADC_DEFAULT_PGA ADS1115_PGA_4_096V
#endif

#ifndef ADS1115_ADC_DEFAULT_RATE
#define ADS1115_ADC_DEFAULT_RATE ADS1115_RATE_128_SPS
#endif

static struct rt_adc_device g_ads1115_adc;
static rt_bool_t g_ads1115_adc_registered;

static rt_err_t ads1115_adc_core_init(void)
{
    rt_bool_t apply_defaults;
    ads1115_device_t *core;
    rt_err_t result;

    apply_defaults = ads1115_get_device() == RT_NULL;
    result = ads1115_init();
    if (result != RT_EOK)
    {
        LOG_E("initialize core failed: %d", result);
        return result;
    }

    core = ads1115_get_device();
    if (core == RT_NULL)
        return -RT_ERROR;

    if (apply_defaults)
    {
        core->pga = (ads1115_pga_t)ADS1115_ADC_DEFAULT_PGA;
        core->rate = (ads1115_rate_t)ADS1115_ADC_DEFAULT_RATE;
    }

    return RT_EOK;
}

static rt_err_t ads1115_adc_init(struct rt_adc_device *device)
{
    if (device == RT_NULL)
        return -RT_EINVAL;

    return ads1115_adc_core_init();
}

static rt_err_t ads1115_adc_enabled(struct rt_adc_device *device,
                                    rt_uint32_t channel,
                                    rt_bool_t enabled)
{
    if (device == RT_NULL || channel >= ADS1115_RT_ADC_CHANNEL_COUNT)
        return -RT_EINVAL;

    if (enabled)
        return ads1115_adc_core_init();

    return RT_EOK;
}

static rt_err_t ads1115_adc_convert(struct rt_adc_device *device,
                                    rt_uint32_t channel,
                                    rt_uint32_t *value)
{
    ads1115_device_t *core;
    rt_int16_t raw = 0;
    rt_int32_t microvolts = 0;
    rt_err_t result;

    if (value == RT_NULL)
        return -RT_EINVAL;

    *value = 0;
    if (device == RT_NULL || channel >= ADS1115_RT_ADC_CHANNEL_COUNT)
        return -RT_EINVAL;

    result = ads1115_adc_core_init();
    if (result != RT_EOK)
        return result;

    core = ads1115_get_device();
    result = ads1115_read_single_ended(core, (rt_uint8_t)channel,
                                       core->pga, core->rate,
                                       &raw, &microvolts);
    if (result != RT_EOK)
        return result;
    if (microvolts < 0)
        return -RT_EIO;

    /* Match the SiFli ADC convention: one unit equals 0.1 mV. */
    *value = (rt_uint32_t)((microvolts + 50) /
                           MICROVOLTS_PER_0P1_MV);
    return RT_EOK;
}

static rt_err_t ads1115_adc_control(struct rt_adc_device *device,
                                    rt_uint32_t cmd, void *arg)
{
    rt_adc_cmd_read_arg_t *read_arg;

    if (device == RT_NULL || cmd != RT_ADC_CMD_READ || arg == RT_NULL)
        return -RT_EINVAL;

    read_arg = (rt_adc_cmd_read_arg_t *)arg;
    return ads1115_adc_convert(device, read_arg->channel,
                               &read_arg->value);
}

static const struct rt_adc_ops g_ads1115_adc_ops =
{
    .enabled = ads1115_adc_enabled,
    .convert = ads1115_adc_convert,
    .init = ads1115_adc_init,
    .control = ads1115_adc_control,
};

int rt_hw_ads1115_adc_register(void)
{
    rt_err_t result;

    if (g_ads1115_adc_registered)
        return RT_EOK;
    if (rt_strlen(ADS1115_ADC_DEVICE_NAME) >= RT_NAME_MAX)
    {
        LOG_E("device name '%s' is too long", ADS1115_ADC_DEVICE_NAME);
        return -RT_EINVAL;
    }
    if (rt_device_find(ADS1115_ADC_DEVICE_NAME) != RT_NULL)
    {
        LOG_E("device name '%s' is already in use",
              ADS1115_ADC_DEVICE_NAME);
        return -RT_EBUSY;
    }

    rt_memset(&g_ads1115_adc, 0, sizeof(g_ads1115_adc));
    result = rt_hw_adc_register(&g_ads1115_adc,
                                ADS1115_ADC_DEVICE_NAME,
                                &g_ads1115_adc_ops,
                                RT_NULL);
    if (result != RT_EOK)
    {
        LOG_E("register '%s' failed: %d", ADS1115_ADC_DEVICE_NAME,
              result);
        return result;
    }

    g_ads1115_adc_registered = RT_TRUE;
    LOG_I("registered RT-Thread ADC device '%s' (channels 0-3)",
          ADS1115_ADC_DEVICE_NAME);
    return RT_EOK;
}
INIT_COMPONENT_EXPORT(rt_hw_ads1115_adc_register);
