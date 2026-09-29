# ICM20948

## 1、介绍

RT-Thread传感器驱动框架相关文档参考：[https://www.rt-thread.org/document/site/development-guide/sensor/sensor_driver/](https://www.rt-thread.org/document/site/development-guide/sensor/sensor_driver/)

### 1.1 ICM20948传感器简介

ICM20948是由InvenSense（现为TDK的一部分）生产的一款高性能惯性测量单元（IMU），它集成了三轴陀螺仪、三轴加速度计和三轴磁力计，以及一个数字运动处理器（DMP）。这款传感器专为需要高精度姿态检测和运动跟踪的应用而设计，如智能手机、无人机、机器人技术、可穿戴设备等。

### 1.2 功能介绍

- 通讯接口
  - [x] I2C
  - [ ] SPI

- 支持的功能
  - [x] 加速度计
  - [x] 陀螺仪
  - [x] 计步
  - [x] 温度
  
  **适配情况**


| 功能     | 加速度计 | 陀螺仪 | 计步  | 温度  |
| ------ | ---- | --- | --- | --- |
| **工作模式**   |      |     |     |     |
| 轮询     | √    | √   |     |     |
| 中断     |      |     |     |     |
| FIFO   |      |     |     |     |
| **电源模式**   |      |     |     |     |
| 掉电     |      |     |     |     |
| 低功耗    |      |     |     |     |
| 普通     |      |     |     |     |
| 高性能    |      |     |     |     |
| **数据输出速率** |      |     |     |     |
| 测量范围   | √    | √   |     |     |


  
  ~~**注意：当开启计步后，加速计数据输出速率被强制到26Hz**~~

### 1.3 目录结构


| 名称      | 说明      |
| ------- | ------- |
| drivers | 底层寄存器操作 |
| example | 例程      |


### 1.4 许可证

遵循 Apache license v2.0 许可，详见 `LICENSE` 文件。

### 1.5 依赖

- RT-Thread 4.0+
- I2C驱动
- RT-Thread传感器驱动框架

## 2、如何开启

 需要在 RT-Thread 的包管理器中选择它，具体路径如下：

```
RT-Thread online packages
    peripheral libraries and drivers  --->
    	 [*] sensors drivers  --->
         	 [*] icm20948 sensor driver package, support: accelerometer, gyroscope.  --->
```

```
--- icm20948 sensor driver package, support: accelerometer, gyroscope.
[*]   Enable icm20948 sample
      I2C device address type (I2C address low. if SA0 pad connect to GND, select me)  --->
      Version (latest)  --->
```

然后让 RT-Thread 的包管理器自动更新，或者使用 `pkgs --update` 命令更新包到 BSP 中。

**注意：你需要明确你所用的icm20948的SA0的连接方式，如果接到地，需要选择`I2C device address type`为`I2C address low`，否则选择`I2C address high`**

## 3、如何使用 


```c
#include <rtthread.h>
#include <sensor_icm20948.h>

int icm20948_example(void)
{
    struct rt_sensor_config cfg;
    cfg.intf.dev_name = "i2c1";
    rt_hw_icm20948_init("icm20948", &cfg);
    return RT_EOK;
}
INIT_APP_EXPORT(icm20948_example);
```

**示例**

```bash
[I/I2C] I2C bus [i2c1] registered

 \ | /
- RT -     Thread Operating System
 / | \     4.1.0 build Jun  3 2025 16:46:01
 2006 - 2022 Copyright by RT-Thread team
[I/sensor] rt_sensor[acce_icm20948] init success
[I/sensor] rt_sensor[gyro_icm20948] init success
[I/sensor.icm20948] icm20948_init 213: ICM20948 ID: 0xEA
[I/sensor.icm20948] rt_hw_icm20948_init 312: sensor init success

msh >sensor probe acce_icm
[I/sensor.cmd] device id: 0xea!
msh >sensor read 5
[I/sensor.cmd] num:  0, x:  -49, y:   22, z:  998 mg, timestamp:749787
[I/sensor.cmd] num:  1, x:  -50, y:   21, z:  998 mg, timestamp:749897
[I/sensor.cmd] num:  2, x:  -50, y:   21, z:  999 mg, timestamp:750007
[I/sensor.cmd] num:  3, x:  -50, y:   23, z:  999 mg, timestamp:750117
[I/sensor.cmd] num:  4, x:  -50, y:   23, z:  999 mg, timestamp:750227
msh >sensor probe gyro_icm
[I/sensor.cmd] device id: 0xea!
msh >sensor read 5
[I/sensor.cmd] num:  0, x:       0, y:       0, z:       0 dps, timestamp:775434
[I/sensor.cmd] num:  1, x:       1, y:       0, z:       0 dps, timestamp:775545
[I/sensor.cmd] num:  2, x:       0, y:       0, z:       0 dps, timestamp:775656
[I/sensor.cmd] num:  3, x:       0, y:       0, z:       0 dps, timestamp:775767
[I/sensor.cmd] num:  4, x:       0, y:       0, z:       0 dps, timestamp:775878


```

## 4、注意事项



## 5、联系方式 & 感谢

* 维护：juzhango
* 主页：[https://github.com/juzhango](https://github.com/juzhango)