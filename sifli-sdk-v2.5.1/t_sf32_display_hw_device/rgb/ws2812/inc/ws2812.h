/*
 * WS2812 RGB LED driver for RT-Thread.
 *
 * The implementation uses PWM3 channel 1 DMA on the T-Display-SF32.
 */
#ifndef __WS2812_H__
#define __WS2812_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    rt_uint8_t red;
    rt_uint8_t green;
    rt_uint8_t blue;
} ws2812_color_t;

rt_err_t ws2812_init(void);
rt_err_t ws2812_deinit(void);

rt_size_t ws2812_get_pixel_count(void);
rt_uint8_t ws2812_get_brightness(void);
rt_err_t ws2812_set_brightness(rt_uint8_t brightness);

rt_err_t ws2812_set_pixel(rt_size_t index, rt_uint8_t red,
                          rt_uint8_t green, rt_uint8_t blue);
rt_err_t ws2812_set_pixel_color(rt_size_t index, ws2812_color_t color);
rt_err_t ws2812_fill(rt_uint8_t red, rt_uint8_t green, rt_uint8_t blue);

/* Transfer the buffered pixel colors to the LED chain. */
rt_err_t ws2812_show(void);

/* Clear the pixel buffer and immediately update the LED chain. */
rt_err_t ws2812_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* __WS2812_H__ */
