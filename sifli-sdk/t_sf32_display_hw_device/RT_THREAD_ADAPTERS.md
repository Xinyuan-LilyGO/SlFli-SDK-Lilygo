# RT-Thread Device Adapters

这些 Adapter 是可选层。原始驱动 API 保持可用，Adapter 只在对应 Kconfig
开关启用时参与编译。设备注册阶段不访问硬件，首次 `rt_device_open()` 或
`rt_device_read()` 时才执行原始驱动初始化。

## 配置与设备名

| 驱动 | Kconfig | 注册设备 |
| --- | --- | --- |
| BME280 | `BME280_USING_RT_SENSOR` | `temp_bme280`, `humi_bme280`, `baro_bme280` |
| BHI260AP | `BHI260AP_USING_RT_SENSOR` | `acce_bhi260`, `gyro_bhi260`, `step_bhi260` |
| L76K | `L76K_USING_RT_SENSOR` | `gps_l76k` |
| SGM41562B | `SGM41562B_USING_RT_DEVICE` | `sgm41562b` |
| AW86224 | `AW86224_USING_RT_DEVICE` | `aw86224` |
| TCA8418 | `TCA8418_USING_RT_DEVICE` | `tca8418` |
| XL9555 | `XL9555_USING_RT_DEVICE` | `xl9555` |

设备名可以在各驱动的 Kconfig 中修改。Sensor Adapter 会自动选择
`RT_USING_SENSOR`。

## Sensor 读取

```c
#include <sensor.h>

struct rt_sensor_data sample;
rt_device_t dev = rt_device_find("temp_bme280");

rt_device_open(dev, RT_DEVICE_FLAG_RDONLY);
if (rt_device_read(dev, 0, &sample, 1) == 1)
    rt_kprintf("temperature=%d.%d C\n",
               sample.data.temp / 10, sample.data.temp % 10);
```

单位遵循 RT-Thread Sensor 约定：

- BME280：温度 0.1 摄氏度、湿度 0.1 %RH、压力 Pa。
- BHI260AP：加速度 mg、角速度 mdps、步数 1。
- L76K：标准 GPS sample 包含经度、纬度和海拔。

L76K 的速度、航向、卫星数、HDOP 和 UTC 时间可以通过扩展命令读取：

```c
#include "l76k_rt_sensor.h"

GPSInfo info;
rt_device_control(dev, L76K_RT_SENSOR_CTRL_GET_INFO, &info);
```

## SGM41562B

```c
#include "sgm41562b_rt_device.h"

rt_device_t dev = rt_device_find("sgm41562b");
struct sgm41562b_rt_status status;
rt_bool_t enable = RT_TRUE;

rt_device_open(dev, RT_DEVICE_OFLAG_RDWR);
rt_device_read(dev, 0, &status, 1);
rt_device_control(dev, SGM41562B_RT_CTRL_ENABLE_CHARGING, &enable);
```

电压、电流、HIZ、复位和 shipping mode 命令定义在
`sgm41562b_rt_device.h`。

## AW86224

```c
#include "aw86224_rt_device.h"

rt_device_t dev = rt_device_find("aw86224");
struct aw86224_rt_ram_effect effect = {1, 0, RT_TRUE};

rt_device_open(dev, RT_DEVICE_OFLAG_RDWR);
rt_device_control(dev, AW86224_RT_CTRL_PLAY_RAM, &effect);
rt_device_control(dev, AW86224_RT_CTRL_STOP, RT_NULL);
```

`rt_device_write()` 将缓冲区作为 RTP 波形播放。RAM、连续振动、增益、F0、
电池电压和线圈电阻使用头文件中的 control 命令。

## TCA8418

```c
#include "tca8418_rt_device.h"

rt_device_t dev = rt_device_find("tca8418");
key_board_event_msg_t events[4];

rt_device_open(dev, RT_DEVICE_FLAG_RDONLY);
rt_size_t count = rt_device_read(dev, 0, events, 4);
```

默认读取永久等待第一个按键事件，然后非阻塞地取出剩余事件。使用
`TCA8418_RT_CTRL_SET_READ_TIMEOUT` 设置 tick 超时，使用 LOCK/UNLOCK 控制键盘。

## XL9555

```c
#include "xl9555_rt_device.h"

rt_device_t dev = rt_device_find("xl9555");
struct xl9555_rt_pin_mode mode = {6, XL9555_PIN_INPUT};
rt_uint8_t value;

rt_device_open(dev, RT_DEVICE_OFLAG_RDWR);
rt_device_control(dev, XL9555_RT_CTRL_SET_PIN_MODE, &mode);
rt_device_read(dev, 6, &value, 1); /* position is XL9555 pin 0-15 */
```

`rt_device_write()` 同样使用 position 指定 pin。中断回调仍使用原驱动定义的
`xl9555_irq_callback_t`，通过 `XL9555_RT_CTRL_ATTACH_IRQ` 绑定。
