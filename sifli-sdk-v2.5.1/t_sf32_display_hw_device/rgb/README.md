# WS2812 RGB driver

该驱动使用 PWM3 CH1 DMA 产生 WS2812 的 800 kHz 单总线时序。T-Display-SF32
板级 pinmux 已将 PA36 配置为 GPTIM2_CH1，对应 Joypad 原理图中的 `RGB_D1`。

## 配置

在工程 `proj.conf` 中启用：

```ini
CONFIG_PKG_USING_WS2812=y
CONFIG_WS2812_LED_COUNT=4
```

驱动会自动选择 PWM3 和 PWM3 CH1 DMA。默认设备名为 `pwm3`。

## 使用

```c
#include "ws2812.h"

if (ws2812_init() == RT_EOK)
{
    ws2812_set_brightness(128);

    ws2812_set_pixel(0, 255, 0, 0);
    ws2812_set_pixel(1, 0, 255, 0);
    ws2812_set_pixel(2, 0, 0, 255);
    ws2812_set_pixel(3, 255, 255, 255);
    ws2812_show();
}
```

`ws2812_set_pixel()` 和 `ws2812_fill()` 只更新内存中的像素缓冲，调用
`ws2812_show()` 后才会发送。`ws2812_clear()` 会立即关闭全部 LED。

WS2812 线上使用 GRB 字节顺序，但 API 参数始终是 RGB。`ws2812_show()` 会等待
DMA 使用完波形缓冲，因此不能在中断服务函数中调用。

PA36/PWM3 CH1 也可能被红外发送功能占用。WS2812 驱动与红外 PWM 不能同时使用
该通道。
