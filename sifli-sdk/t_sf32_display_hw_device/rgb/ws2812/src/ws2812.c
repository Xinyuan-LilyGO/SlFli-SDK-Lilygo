/*
 * WS2812 RGB LED driver for RT-Thread.
 *
 * PWM3 channel 1 is GPTIM2_CH1 on SF32LB52X. GPTIM2 runs at 24 MHz, so
 * compare values 7 and 16 produce approximately 333 ns and 708 ns high
 * pulses in a 1.25 us WS2812 bit period.
 */
#include "ws2812.h"

#include <rtdevice.h>
#include <drivers/rt_drv_pwm.h>

#define LOG_TAG "ws2812"
#include <ulog.h>

#ifndef WS2812_PWM_DEVICE_NAME
#define WS2812_PWM_DEVICE_NAME "pwm3"
#endif

#ifndef WS2812_LED_COUNT
#define WS2812_LED_COUNT 4
#endif

#define WS2812_PWM_CHANNEL       1U
#define WS2812_PWM_PERIOD_NS     1250U
#define WS2812_T0H_COMPARE       7U
#define WS2812_T1H_COMPARE       16U
#define WS2812_RESET_SLOTS       240U
#define WS2812_BITS_PER_PIXEL    24U
#define WS2812_DATA_SLOTS        (WS2812_LED_COUNT * WS2812_BITS_PER_PIXEL)
#define WS2812_FRAME_SLOTS       (WS2812_RESET_SLOTS + WS2812_DATA_SLOTS + \
                                  WS2812_RESET_SLOTS)

static struct rt_device_pwm *ws2812_pwm;
static struct rt_mutex ws2812_lock;
static rt_bool_t ws2812_initialized;
static rt_uint8_t ws2812_brightness = 255U;
static ws2812_color_t ws2812_pixels[WS2812_LED_COUNT];
static rt_uint16_t ws2812_waveform[WS2812_FRAME_SLOTS];

static rt_uint8_t ws2812_scale_component(rt_uint8_t component)
{
    return (rt_uint8_t)(((rt_uint16_t)component * ws2812_brightness + 127U) /
                        255U);
}

static void ws2812_encode_byte(rt_size_t *position, rt_uint8_t value)
{
    rt_uint8_t mask;

    for (mask = 0x80U; mask != 0U; mask >>= 1)
    {
        ws2812_waveform[*position] =
            (value & mask) ? WS2812_T1H_COMPARE : WS2812_T0H_COMPARE;
        (*position)++;
    }
}

static void ws2812_build_waveform(void)
{
    rt_size_t index;
    rt_size_t position = WS2812_RESET_SLOTS;

    rt_memset(ws2812_waveform, 0, sizeof(ws2812_waveform));

    for (index = 0; index < WS2812_LED_COUNT; index++)
    {
        /* WS2812 wire order is GRB, while the public API uses RGB. */
        ws2812_encode_byte(&position,
                           ws2812_scale_component(ws2812_pixels[index].green));
        ws2812_encode_byte(&position,
                           ws2812_scale_component(ws2812_pixels[index].red));
        ws2812_encode_byte(&position,
                           ws2812_scale_component(ws2812_pixels[index].blue));
    }
}

rt_err_t ws2812_init(void)
{
    rt_err_t result;

    if (ws2812_initialized)
        return RT_EOK;

    ws2812_pwm =
        (struct rt_device_pwm *)rt_device_find(WS2812_PWM_DEVICE_NAME);
    if (ws2812_pwm == RT_NULL)
    {
        LOG_E("PWM device %s not found", WS2812_PWM_DEVICE_NAME);
        return -RT_ENOSYS;
    }

    result = rt_mutex_init(&ws2812_lock, "ws2812", RT_IPC_FLAG_PRIO);
    if (result != RT_EOK)
    {
        ws2812_pwm = RT_NULL;
        return result;
    }

    rt_memset(ws2812_pixels, 0, sizeof(ws2812_pixels));
    ws2812_brightness = 255U;
    ws2812_initialized = RT_TRUE;

    result = ws2812_show();
    if (result != RT_EOK)
    {
        ws2812_initialized = RT_FALSE;
        rt_mutex_detach(&ws2812_lock);
        ws2812_pwm = RT_NULL;
    }
    return result;
}

rt_err_t ws2812_deinit(void)
{
    rt_err_t result;

    if (!ws2812_initialized)
        return RT_EOK;

    result = rt_pwm_disable(ws2812_pwm, WS2812_PWM_CHANNEL);
    ws2812_initialized = RT_FALSE;
    ws2812_pwm = RT_NULL;
    rt_mutex_detach(&ws2812_lock);
    return result;
}

rt_size_t ws2812_get_pixel_count(void)
{
    return WS2812_LED_COUNT;
}

rt_uint8_t ws2812_get_brightness(void)
{
    return ws2812_brightness;
}

rt_err_t ws2812_set_brightness(rt_uint8_t brightness)
{
    if (!ws2812_initialized)
        return -RT_ENOSYS;

    rt_mutex_take(&ws2812_lock, RT_WAITING_FOREVER);
    ws2812_brightness = brightness;
    rt_mutex_release(&ws2812_lock);
    return RT_EOK;
}

rt_err_t ws2812_set_pixel(rt_size_t index, rt_uint8_t red,
                          rt_uint8_t green, rt_uint8_t blue)
{
    if (!ws2812_initialized)
        return -RT_ENOSYS;
    if (index >= WS2812_LED_COUNT)
        return -RT_EINVAL;

    rt_mutex_take(&ws2812_lock, RT_WAITING_FOREVER);
    ws2812_pixels[index].red = red;
    ws2812_pixels[index].green = green;
    ws2812_pixels[index].blue = blue;
    rt_mutex_release(&ws2812_lock);
    return RT_EOK;
}

rt_err_t ws2812_set_pixel_color(rt_size_t index, ws2812_color_t color)
{
    return ws2812_set_pixel(index, color.red, color.green, color.blue);
}

rt_err_t ws2812_fill(rt_uint8_t red, rt_uint8_t green, rt_uint8_t blue)
{
    rt_size_t index;

    if (!ws2812_initialized)
        return -RT_ENOSYS;

    rt_mutex_take(&ws2812_lock, RT_WAITING_FOREVER);
    for (index = 0; index < WS2812_LED_COUNT; index++)
    {
        ws2812_pixels[index].red = red;
        ws2812_pixels[index].green = green;
        ws2812_pixels[index].blue = blue;
    }
    rt_mutex_release(&ws2812_lock);
    return RT_EOK;
}

rt_err_t ws2812_show(void)
{
    struct rt_pwm_configuration configuration;
    rt_uint32_t transfer_us;
    rt_uint32_t wait_ms;
    rt_err_t result;

    if (!ws2812_initialized)
        return -RT_ENOSYS;

    rt_mutex_take(&ws2812_lock, RT_WAITING_FOREVER);
    ws2812_build_waveform();

    rt_memset(&configuration, 0, sizeof(configuration));
    configuration.channel = WS2812_PWM_CHANNEL;
    configuration.period = WS2812_PWM_PERIOD_NS;
    configuration.pulse = 0U;
    configuration.dma_data = ws2812_waveform;
    configuration.data_len = (rt_uint16_t)WS2812_FRAME_SLOTS;

    result = rt_device_control((rt_device_t)ws2812_pwm, PWM_CMD_SET,
                               &configuration);
    if (result == RT_EOK)
    {
        result = rt_device_control((rt_device_t)ws2812_pwm, PWM_CMD_ENABLE,
                                   &configuration);
    }

    if (result == RT_EOK)
    {
        /* Keep the DMA source stable until the complete frame is transmitted. */
        transfer_us = (WS2812_FRAME_SLOTS * WS2812_PWM_PERIOD_NS + 999U) /
                      1000U;
        wait_ms = (transfer_us + 999U) / 1000U;
        rt_thread_mdelay(wait_ms == 0U ? 1U : wait_ms);
    }

    rt_mutex_release(&ws2812_lock);
    return result;
}

rt_err_t ws2812_clear(void)
{
    rt_err_t result = ws2812_fill(0U, 0U, 0U);

    if (result != RT_EOK)
        return result;
    return ws2812_show();
}
