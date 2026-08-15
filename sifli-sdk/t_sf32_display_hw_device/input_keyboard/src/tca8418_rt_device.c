/* Optional RT-Thread Device adapter for TCA8418. */
#include "tca8418_rt_device.h"

#include "xl9555.h"

struct tca8418_adapter
{
    struct rt_device parent;
    struct rt_mutex init_lock;
    rt_int32_t read_timeout;
};

static struct tca8418_adapter g_keyboard;

static rt_err_t tca8418_device_ensure_ready(void)
{
    rt_err_t result = RT_EOK;

    rt_mutex_take(&g_keyboard.init_lock, RT_WAITING_FOREVER);
    result = xl9555_init();
    if (result == RT_EOK)
        result = key_board_tca8418_init();
    rt_mutex_release(&g_keyboard.init_lock);
    return result;
}

static rt_err_t tca8418_device_init(rt_device_t device)
{
    (void)device;
    return tca8418_device_ensure_ready();
}

static rt_err_t tca8418_device_open(rt_device_t device, rt_uint16_t flags)
{
    (void)device;
    (void)flags;
    return tca8418_device_ensure_ready();
}

static rt_err_t tca8418_device_close(rt_device_t device)
{
    (void)device;
    return RT_EOK;
}

static rt_size_t tca8418_device_read(rt_device_t device, rt_off_t position,
                                     void *buffer, rt_size_t size)
{
    key_board_event_msg_t *events = buffer;
    rt_mq_t queue;
    rt_size_t count = 0;

    (void)device;
    (void)position;
    if (events == RT_NULL || size == 0)
        return 0;
    if (tca8418_device_ensure_ready() != RT_EOK)
        return 0;

    queue = key_board_get_mq();
    if (queue == RT_NULL)
        return 0;
    if (rt_mq_recv(queue, &events[0], sizeof(events[0]),
                   g_keyboard.read_timeout) != RT_EOK)
        return 0;
    count = 1;

    while (count < size &&
           rt_mq_recv(queue, &events[count], sizeof(events[count]),
                      RT_WAITING_NO) == RT_EOK)
    {
        count++;
    }
    return count;
}

static rt_err_t tca8418_device_control(rt_device_t device, int command,
                                       void *args)
{
    key_board_event_msg_t event;
    rt_mq_t queue;

    (void)device;
    switch (command)
    {
    case TCA8418_RT_CTRL_SET_READ_TIMEOUT:
        if (args == RT_NULL)
            return -RT_EINVAL;
        g_keyboard.read_timeout = *(rt_int32_t *)args;
        return RT_EOK;
    case TCA8418_RT_CTRL_LOCK:
        if (tca8418_device_ensure_ready() != RT_EOK)
            return -RT_ERROR;
        return TCA8418_LockKeypad();
    case TCA8418_RT_CTRL_UNLOCK:
        if (tca8418_device_ensure_ready() != RT_EOK)
            return -RT_ERROR;
        return TCA8418_UnlockKeypad();
    case TCA8418_RT_CTRL_FLUSH:
        if (tca8418_device_ensure_ready() != RT_EOK)
            return -RT_ERROR;
        queue = key_board_get_mq();
        while (rt_mq_recv(queue, &event, sizeof(event), RT_WAITING_NO) ==
               RT_EOK)
        {
        }
        return RT_EOK;
    case RT_DEVICE_CTRL_RESUME:
        return tca8418_device_ensure_ready();
    case RT_DEVICE_CTRL_SUSPEND:
        return RT_EOK;
    default:
        return -RT_EINVAL;
    }
}

#ifdef RT_USING_DEVICE_OPS
static const struct rt_device_ops g_tca8418_ops =
{
    .init = tca8418_device_init,
    .open = tca8418_device_open,
    .close = tca8418_device_close,
    .read = tca8418_device_read,
    .write = RT_NULL,
    .control = tca8418_device_control,
};
#endif

int rt_hw_tca8418_device_register(void)
{
    if (rt_device_find(TCA8418_RT_DEVICE_NAME) != RT_NULL)
        return RT_EOK;

    rt_memset(&g_keyboard, 0, sizeof(g_keyboard));
    rt_mutex_init(&g_keyboard.init_lock, "key_init", RT_IPC_FLAG_FIFO);
    g_keyboard.read_timeout = RT_WAITING_FOREVER;
    g_keyboard.parent.type = RT_Device_Class_Char;
#ifdef RT_USING_DEVICE_OPS
    g_keyboard.parent.ops = &g_tca8418_ops;
#else
    g_keyboard.parent.init = tca8418_device_init;
    g_keyboard.parent.open = tca8418_device_open;
    g_keyboard.parent.close = tca8418_device_close;
    g_keyboard.parent.read = tca8418_device_read;
    g_keyboard.parent.control = tca8418_device_control;
#endif
    return rt_device_register(&g_keyboard.parent, TCA8418_RT_DEVICE_NAME,
                              RT_DEVICE_FLAG_RDONLY);
}
INIT_COMPONENT_EXPORT(rt_hw_tca8418_device_register);
