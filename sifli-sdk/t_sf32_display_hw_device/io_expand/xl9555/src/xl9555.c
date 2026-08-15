/*
 * XL9555 GPIO Expander Driver for RT-Thread
 * 假设使用的 I2C 设备名为 "i2c2"，可根据实际情况修改
 */
#include "xl9555.h"
#include "ulog.h"
#ifdef RT_USING_PM
    #include <drivers/pm.h>
#endif
static struct xl9555_device xl9555_dev;

#ifdef RT_USING_PM
static rt_err_t xl9555_restore(void);
#endif

#if defined(XL9555_IRQ_PIN) && (XL9555_IRQ_PIN >= 0)
static rt_sem_t xl9555_irq_sem;
static rt_thread_t xl9555_irq_thread;
static rt_bool_t xl9555_irq_running;
static rt_bool_t xl9555_irq_enabled;
static rt_bool_t xl9555_irq_attached;
static rt_uint16_t xl9555_input_cache;
static xl9555_irq_callback_t xl9555_irq_callback;
static void *xl9555_irq_user_data;

static rt_err_t xl9555_irq_init(void);
static void xl9555_irq_deinit(void);
#endif

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

#if defined(XL9555_IRQ_PIN) && (XL9555_IRQ_PIN >= 0)

static void xl9555_irq_rearm(void)
{
    if (!xl9555_irq_running || !xl9555_irq_enabled)
        return;

    rt_pin_irq_enable(XL9555_IRQ_PIN, PIN_IRQ_ENABLE);

    /* INT is active low. Rescan if it was asserted while the IRQ was off. */
    if (rt_pin_read(XL9555_IRQ_PIN) == PIN_LOW)
    {
        rt_pin_irq_enable(XL9555_IRQ_PIN, PIN_IRQ_DISABLE);
        rt_sem_release(xl9555_irq_sem);
    }
}

static void xl9555_irq_isr(void *args)
{
    (void)args;

    if (!xl9555_irq_running || !xl9555_irq_enabled)
        return;

    rt_pin_irq_enable(XL9555_IRQ_PIN, PIN_IRQ_DISABLE);
    rt_sem_release(xl9555_irq_sem);
}

static void xl9555_irq_thread_entry(void *parameter)
{
    rt_uint16_t input_state;
    rt_uint16_t changed_mask;

    (void)parameter;

    while (xl9555_irq_running)
    {
        if (rt_sem_take(xl9555_irq_sem, RT_WAITING_FOREVER) != RT_EOK)
            continue;
        if (!xl9555_irq_running)
            break;
        if (!xl9555_irq_enabled)
            continue;

        if (xl9555_read_reg(&xl9555_dev, XL9555_INPUT_PORT_0,
                            &input_state) == RT_EOK)
        {
            changed_mask = (input_state ^ xl9555_input_cache) &
                           xl9555_dev.config_cache;
            xl9555_input_cache = input_state;

            if (changed_mask != 0 && xl9555_irq_callback != RT_NULL)
            {
                xl9555_irq_callback(input_state, changed_mask,
                                    xl9555_irq_user_data);
            }
        }
        else
        {
            LOG_E("read input registers after interrupt failed");
            rt_thread_mdelay(10);
        }

        xl9555_irq_rearm();
    }
}

static void xl9555_irq_deinit(void)
{
    xl9555_irq_enabled = RT_FALSE;

    if (xl9555_irq_attached)
    {
        rt_pin_irq_enable(XL9555_IRQ_PIN, PIN_IRQ_DISABLE);
        rt_pin_detach_irq(XL9555_IRQ_PIN);
        xl9555_irq_attached = RT_FALSE;
    }

    xl9555_irq_running = RT_FALSE;
    if (xl9555_irq_thread != RT_NULL)
    {
        if (xl9555_irq_sem != RT_NULL)
            rt_sem_release(xl9555_irq_sem);
        rt_thread_delete(xl9555_irq_thread);
        xl9555_irq_thread = RT_NULL;
    }

    if (xl9555_irq_sem != RT_NULL)
    {
        rt_sem_delete(xl9555_irq_sem);
        xl9555_irq_sem = RT_NULL;
    }

    xl9555_irq_callback = RT_NULL;
    xl9555_irq_user_data = RT_NULL;
}

static rt_err_t xl9555_irq_init(void)
{
    rt_err_t result;

    xl9555_irq_sem = rt_sem_create("xl_irq", 0, RT_IPC_FLAG_FIFO);
    if (xl9555_irq_sem == RT_NULL)
        return -RT_ENOMEM;

    xl9555_irq_thread =
        rt_thread_create("xl9555", xl9555_irq_thread_entry, RT_NULL, 1024,
                         RT_THREAD_PRIORITY_MAX / 2, 10);
    if (xl9555_irq_thread == RT_NULL)
    {
        xl9555_irq_deinit();
        return -RT_ENOMEM;
    }

    result = xl9555_read_reg(&xl9555_dev, XL9555_INPUT_PORT_0,
                             &xl9555_input_cache);
    if (result != RT_EOK)
    {
        xl9555_irq_deinit();
        return result;
    }

    rt_pin_mode(XL9555_IRQ_PIN, PIN_MODE_INPUT_PULLUP);
    result = rt_pin_attach_irq(XL9555_IRQ_PIN, PIN_IRQ_MODE_FALLING,
                               xl9555_irq_isr, RT_NULL);
    if (result != RT_EOK)
    {
        xl9555_irq_deinit();
        return result;
    }
    xl9555_irq_attached = RT_TRUE;

    xl9555_irq_running = RT_TRUE;
    result = rt_thread_startup(xl9555_irq_thread);
    if (result != RT_EOK)
    {
        xl9555_irq_deinit();
        return result;
    }

    xl9555_irq_enabled = RT_TRUE;
    xl9555_irq_rearm();
    return RT_EOK;
}

#endif

#ifdef RT_USING_PM

static int xl9555_pm_suspend(const struct rt_device *device, uint8_t mode)
{
    // xl9555 由系统电源域供电时寄存器状态保留，无需额外操作
    // 如果由可关闭的电源域供电，则在此关闭
    return 0;
}

static void xl9555_pm_resume(const struct rt_device *device, uint8_t mode)
{
    // 深度睡眠后 I2C 可能需要重新打开和配置
    if (xl9555_dev.i2c_bus == RT_NULL)
    {
        xl9555_dev.i2c_bus = rt_i2c_bus_device_find(XL9555_I2C_BUS_NAME);
    }
    if (xl9555_dev.i2c_bus != RT_NULL)
    {
        rt_device_open((rt_device_t)xl9555_dev.i2c_bus, RT_DEVICE_OFLAG_RDWR);

        struct rt_i2c_configuration configuration = {
            .mode = 0,
            .addr = 0,
            .timeout = 500,
            .max_hz = 400000,
        };
        rt_i2c_configure(xl9555_dev.i2c_bus, &configuration);

        // 恢复所有寄存器（cache 中保存的是断电前的正确状态）
        xl9555_restore();
    }
}

static const struct rt_device_pm_ops xl9555_pm_op = {
    .suspend = xl9555_pm_suspend,
    .resume = xl9555_pm_resume,
};

#endif

/* -------------------- 用户 API 接口 -------------------- */
rt_err_t xl9555_init()
{
    if (xl9555_dev.i2c_bus != RT_NULL)
        return RT_EOK;

    xl9555_dev.dev_addr = XL9555_I2C_ADDR;
    xl9555_dev.i2c_bus = rt_i2c_bus_device_find(XL9555_I2C_BUS_NAME);
    if (xl9555_dev.i2c_bus == RT_NULL)
    {
        LOG_E("find %s device failed", XL9555_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    if (rt_device_open((rt_device_t)xl9555_dev.i2c_bus, RT_DEVICE_OFLAG_RDWR) !=
        RT_EOK)
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
    xl9555_dev.config_cache = 0x0000;
    xl9555_dev.output_cache = 0x0000;

    xl9555_write_reg(&xl9555_dev, XL9555_CONFIG_0, xl9555_dev.config_cache);
    xl9555_write_reg(&xl9555_dev, XL9555_OUTPUT_PORT_0,
                     xl9555_dev.output_cache);
    xl9555_write_reg(&xl9555_dev, XL9555_CONFIG_1, xl9555_dev.config_cache);
    xl9555_write_reg(&xl9555_dev, XL9555_OUTPUT_PORT_1,
                     xl9555_dev.output_cache);
#ifdef RT_USING_PM
    rt_pm_device_register(NULL, &xl9555_pm_op);
#endif

#if defined(XL9555_IRQ_PIN) && (XL9555_IRQ_PIN >= 0)
    rt_err_t result = xl9555_irq_init();
    if (result != RT_EOK)
    {
        LOG_E("initialize IRQ pin %d failed: %d", XL9555_IRQ_PIN, result);
        rt_device_close((rt_device_t)xl9555_dev.i2c_bus);
        xl9555_dev.i2c_bus = RT_NULL;
        return result;
    }
#endif
    return RT_EOK;
}

rt_err_t xl9555_deinit(void)
{
#if defined(XL9555_IRQ_PIN) && (XL9555_IRQ_PIN >= 0)
    xl9555_irq_deinit();
#endif

    if (xl9555_dev.i2c_bus != RT_NULL)
    {
        rt_device_close((rt_device_t)xl9555_dev.i2c_bus);
        xl9555_dev.i2c_bus = RT_NULL;
    }
    return RT_EOK;
}

#ifdef RT_USING_PM
static rt_err_t xl9555_restore(void)
{
    xl9555_write_reg(&xl9555_dev, XL9555_CONFIG_0, xl9555_dev.config_cache);
    xl9555_write_reg(&xl9555_dev, XL9555_OUTPUT_PORT_0,
                     xl9555_dev.output_cache);
    return RT_EOK;
}
#endif

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
#ifdef RT_USING_PM
    rt_pm_request(PM_SLEEP_MODE_IDLE);
#endif
    xl9555_write_reg(&xl9555_dev, XL9555_OUTPUT_PORT_0,
                     xl9555_dev.output_cache);
#ifdef RT_USING_PM
    rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
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

rt_err_t xl9555_irq_attach(xl9555_irq_callback_t callback, void *user_data)
{
#if defined(XL9555_IRQ_PIN) && (XL9555_IRQ_PIN >= 0)
    xl9555_irq_user_data = user_data;
    xl9555_irq_callback = callback;
    return RT_EOK;
#else
    return -RT_ENOSYS;
#endif
}

rt_err_t xl9555_irq_enable(rt_bool_t enabled)
{
#if defined(XL9555_IRQ_PIN) && (XL9555_IRQ_PIN >= 0)
    rt_err_t result;

    if (!xl9555_irq_attached || !xl9555_irq_running)
        return -RT_ERROR;

    if ((enabled && xl9555_irq_enabled) ||
        (!enabled && !xl9555_irq_enabled))
        return RT_EOK;

    if (!enabled)
    {
        xl9555_irq_enabled = RT_FALSE;
        return rt_pin_irq_enable(XL9555_IRQ_PIN, PIN_IRQ_DISABLE);
    }

    result = xl9555_read_reg(&xl9555_dev, XL9555_INPUT_PORT_0,
                             &xl9555_input_cache);
    if (result != RT_EOK)
        return result;

    xl9555_irq_enabled = RT_TRUE;
    xl9555_irq_rearm();
    return RT_EOK;
#else
    return -RT_ENOSYS;
#endif
}

rt_uint8_t xl9555_all_digital_wirte(rt_bool_t val)
{
    if (val)
    {
        xl9555_dev.config_cache = 0xFFFF;
        xl9555_dev.output_cache = 0xFFFF;
    }
    else
    {
        xl9555_dev.config_cache = 0x0000;
        xl9555_dev.output_cache = 0x0000;
    }

    xl9555_write_reg(&xl9555_dev, XL9555_CONFIG_0, xl9555_dev.config_cache);
    xl9555_write_reg(&xl9555_dev, XL9555_OUTPUT_PORT_0,
                     xl9555_dev.output_cache);
    xl9555_write_reg(&xl9555_dev, XL9555_CONFIG_1, xl9555_dev.config_cache);
    xl9555_write_reg(&xl9555_dev, XL9555_OUTPUT_PORT_1,
                     xl9555_dev.output_cache);
    return RT_EOK;
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
