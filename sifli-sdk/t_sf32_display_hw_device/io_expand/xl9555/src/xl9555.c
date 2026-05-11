/*
 * XL9555 GPIO Expander Driver for RT-Thread
 * 假设使用的 I2C 设备名为 "i2c2"，可根据实际情况修改
 */
#include "xl9555.h"
#include "ulog.h"

static struct xl9555_device xl9555_dev;

/* 内部函数：向指定寄存器写入 16 位数据 */
static rt_err_t xl9555_write_reg(struct xl9555_device *dev, rt_uint8_t reg,
                                 rt_uint16_t data)
{
    rt_uint8_t send_buf[3];
    struct rt_i2c_msg msgs;

    send_buf[0] = reg;                /* 命令字节：寄存器地址 */
    send_buf[1] = data & 0xFF;        /* Port 0 (低8位) */
    send_buf[2] = (data >> 8) & 0xFF; /* Port 1 (高8位) */

    msgs.addr = dev->dev_addr;
    msgs.flags = RT_I2C_WR;
    msgs.buf = send_buf;
    msgs.len = 3;

    if (rt_i2c_transfer(dev->i2c_bus, &msgs, 1) == 1)
        return RT_EOK;
    else
        return -RT_ERROR;
}

/* 内部函数：从指定寄存器读取 16 位数据 */
static rt_err_t xl9555_read_reg(struct xl9555_device *dev, rt_uint8_t reg,
                                rt_uint16_t *data)
{
    struct rt_i2c_msg msgs[2];
    rt_uint8_t recv_buf[2];

    /* 步骤1: 发送寄存器地址 */
    msgs[0].addr = dev->dev_addr;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = &reg;
    msgs[0].len = 1;

    /* 步骤2: 读取数据 */
    msgs[1].addr = dev->dev_addr;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = recv_buf;
    msgs[1].len = 2;

    if (rt_i2c_transfer(dev->i2c_bus, msgs, 2) == 2)
    {
        *data = (recv_buf[1] << 8) | recv_buf[0];
        return RT_EOK;
    }
    else
    {
        return -RT_ERROR;
    }
}

/* -------------------- 用户 API 接口 -------------------- */
rt_err_t xl9555_init()
{
    rt_uint16_t config_val;
    xl9555_dev.dev_addr = XL9555_I2C_ADDR;
    xl9555_dev.i2c_bus = rt_i2c_bus_device_find(XL9555_I2C_BUS_NAME);
    if (xl9555_dev.i2c_bus == RT_NULL)
    {
        LOG_E("find %s device failed", XL9555_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    if (rt_device_open((rt_device_t)xl9555_dev.i2c_bus, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        LOG_E("open %s device failed", XL9555_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    struct rt_i2c_configuration configuration = {
        .mode = 0,
        .addr = 0,
        .timeout = 500,   // Waiting for timeout period (ms)
        .max_hz = 400000, // I2C rate (hz)
    };

    rt_i2c_configure(xl9555_dev.i2c_bus, &configuration);

    /* 初始化缓存：默认所有引脚为输入模式，上电后默认值即为全1 */
    xl9555_dev.config_cache = 0xFFFF;
    xl9555_dev.output_cache = 0xFFFF;

    xl9555_write_reg(&xl9555_dev, XL9555_CONFIG_0, xl9555_dev.config_cache);
    xl9555_write_reg(&xl9555_dev, XL9555_OUTPUT_PORT_0,
                     xl9555_dev.output_cache);
    xl9555_write_reg(&xl9555_dev, XL9555_CONFIG_1, xl9555_dev.config_cache);
    xl9555_write_reg(&xl9555_dev, XL9555_OUTPUT_PORT_1,
                     xl9555_dev.output_cache);

    return RT_EOK;
}

/* 设置某个引脚方向 (pin: 0-15, mode: 0=输出, 1=输入) */
void xl9555_pin_mode(rt_uint8_t pin, rt_uint8_t mode)
{
    rt_uint16_t mask = (1 << pin);

    if (mode == 0) /* 输出 */
    {
        xl9555_dev.config_cache &= ~mask;
    }
    else /* 输入 */
    {
        xl9555_dev.config_cache |= mask;
    }

    xl9555_write_reg(&xl9555_dev, XL9555_CONFIG_0, xl9555_dev.config_cache);
}

/* 写入数字值 (pin: 0-15, val: 0/1) */
void xl9555_digital_write(rt_uint8_t pin, rt_uint8_t val)
{
    rt_uint16_t mask = (1 << pin);

    if (val)
        xl9555_dev.output_cache |= mask;
    else
        xl9555_dev.output_cache &= ~mask;

    xl9555_write_reg(&xl9555_dev, XL9555_OUTPUT_PORT_0,
                     xl9555_dev.output_cache);
}

/* 读取数字值 (pin: 0-15) */
rt_uint8_t xl9555_digital_read(rt_uint8_t pin)
{
    rt_uint16_t val;
    rt_uint16_t mask = (1 << pin);

    if (xl9555_read_reg(&xl9555_dev, XL9555_INPUT_PORT_0, &val) == RT_EOK)
    {
        return (val & mask) ? 1 : 0;
    }
    return 0;
}






/* 测试示例：在 msh shell 中调用 */
static void xl9555_test(void)
{
    /* 假设设备挂载在 i2c2 总线上，A0/A1/A2 接地，地址 0x20 */
    xl9555_init();

    /* 配置 P0_0 为输出，P0_1 为输入 */
    xl9555_pin_mode(0, 0); // 输出
    xl9555_pin_mode(1, 1); // 输入

    /* 写 P0_0 高电平 */
    xl9555_digital_write(0, 1);

    /* 读 P0_1 状态 */
    rt_uint8_t state = xl9555_digital_read(1);
    rt_kprintf("P0_1 state: %d\n", state);
}
MSH_CMD_EXPORT(xl9555_test, xl9555 gpio expander test);