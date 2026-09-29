/* Optional RT-Thread Device adapter for XL9555. */
#include "xl9555_rt_device.h"

static struct rt_device g_xl9555_device;
static struct rt_mutex g_init_lock;

static rt_err_t xl9555_device_ensure_ready(void)
{
    rt_err_t result;

    rt_mutex_take(&g_init_lock, RT_WAITING_FOREVER);
    result = xl9555_init();
    rt_mutex_release(&g_init_lock);
    return result;
}

static rt_err_t xl9555_device_init(rt_device_t device)
{
    (void)device;
    return xl9555_device_ensure_ready();
}

static rt_err_t xl9555_device_open(rt_device_t device, rt_uint16_t flags)
{
    (void)device;
    (void)flags;
    return xl9555_device_ensure_ready();
}

static rt_err_t xl9555_device_close(rt_device_t device)
{
    (void)device;
    return RT_EOK;
}

static rt_size_t xl9555_device_read(rt_device_t device, rt_off_t position,
                                    void *buffer, rt_size_t size)
{
    rt_uint8_t *value = buffer;

    (void)device;
    if (value == RT_NULL || size == 0 || position < 0 || position >= 16)
        return 0;
    if (xl9555_device_ensure_ready() != RT_EOK)
        return 0;

    *value = xl9555_digital_read((rt_uint8_t)position);
    return 1;
}

static rt_size_t xl9555_device_write(rt_device_t device, rt_off_t position,
                                     const void *buffer, rt_size_t size)
{
    const rt_uint8_t *value = buffer;

    (void)device;
    if (value == RT_NULL || size == 0 || position < 0 || position >= 16)
        return 0;
    if (xl9555_device_ensure_ready() != RT_EOK)
        return 0;

    xl9555_digital_write((rt_uint8_t)position, *value ? 1U : 0U);
    return 1;
}

static rt_err_t xl9555_device_control(rt_device_t device, int command,
                                      void *args)
{
    struct xl9555_rt_pin_mode *pin_mode;
    struct xl9555_rt_irq_config *irq_config;
    rt_uint16_t state;
    rt_uint8_t pin;

    (void)device;
    if (command == RT_DEVICE_CTRL_SUSPEND)
        return RT_EOK;
    if (xl9555_device_ensure_ready() != RT_EOK)
        return -RT_ERROR;
    if (command == RT_DEVICE_CTRL_RESUME)
        return RT_EOK;

    switch (command)
    {
    case XL9555_RT_CTRL_SET_PIN_MODE:
        if (args == RT_NULL)
            return -RT_EINVAL;
        pin_mode = args;
        if (pin_mode->pin >= 16 || pin_mode->mode > XL9555_PIN_INPUT)
            return -RT_EINVAL;
        xl9555_pin_mode(pin_mode->pin, pin_mode->mode);
        return RT_EOK;
    case XL9555_RT_CTRL_READ_ALL:
        if (args == RT_NULL)
            return -RT_EINVAL;
        state = 0;
        for (pin = 0; pin < 16; pin++)
        {
            if (xl9555_digital_read(pin))
                state |= (rt_uint16_t)(1U << pin);
        }
        *(rt_uint16_t *)args = state;
        return RT_EOK;
    case XL9555_RT_CTRL_ATTACH_IRQ:
        if (args == RT_NULL)
            return -RT_EINVAL;
        irq_config = args;
        return xl9555_irq_attach(irq_config->callback,
                                 irq_config->user_data);
    case XL9555_RT_CTRL_ENABLE_IRQ:
        return (args != RT_NULL)
                   ? xl9555_irq_enable(*(rt_bool_t *)args)
                   : -RT_EINVAL;
    default:
        return -RT_EINVAL;
    }
}

#ifdef RT_USING_DEVICE_OPS
static const struct rt_device_ops g_xl9555_ops =
{
    .init = xl9555_device_init,
    .open = xl9555_device_open,
    .close = xl9555_device_close,
    .read = xl9555_device_read,
    .write = xl9555_device_write,
    .control = xl9555_device_control,
};
#endif

int rt_hw_xl9555_device_register(void)
{
    if (rt_device_find(XL9555_RT_DEVICE_NAME) != RT_NULL)
        return RT_EOK;

    rt_mutex_init(&g_init_lock, "xl_init", RT_IPC_FLAG_FIFO);
    rt_memset(&g_xl9555_device, 0, sizeof(g_xl9555_device));
    g_xl9555_device.type = RT_Device_Class_Miscellaneous;
#ifdef RT_USING_DEVICE_OPS
    g_xl9555_device.ops = &g_xl9555_ops;
#else
    g_xl9555_device.init = xl9555_device_init;
    g_xl9555_device.open = xl9555_device_open;
    g_xl9555_device.close = xl9555_device_close;
    g_xl9555_device.read = xl9555_device_read;
    g_xl9555_device.write = xl9555_device_write;
    g_xl9555_device.control = xl9555_device_control;
#endif
    return rt_device_register(&g_xl9555_device, XL9555_RT_DEVICE_NAME,
                              RT_DEVICE_FLAG_RDWR);
}
INIT_COMPONENT_EXPORT(rt_hw_xl9555_device_register);
