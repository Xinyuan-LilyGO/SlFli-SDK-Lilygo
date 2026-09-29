/* Optional RT-Thread Device adapter for AW86224. */
#include "aw86224_rt_device.h"

#include "aw86224.h"
#include "xl9555.h"

static struct rt_device g_haptic_device;
static struct rt_mutex g_init_lock;

static rt_err_t aw86224_device_ensure_ready(void)
{
    rt_err_t result;

    rt_mutex_take(&g_init_lock, RT_WAITING_FOREVER);
    result = xl9555_init();
    if (result == RT_EOK)
        result = aw86224_init();
    rt_mutex_release(&g_init_lock);
    return result;
}

static rt_err_t aw86224_device_init(rt_device_t device)
{
    (void)device;
    return aw86224_device_ensure_ready();
}

static rt_err_t aw86224_device_open(rt_device_t device, rt_uint16_t flags)
{
    (void)device;
    (void)flags;
    return aw86224_device_ensure_ready();
}

static rt_err_t aw86224_device_close(rt_device_t device)
{
    (void)device;
    return aw86224_stop_playback();
}

static rt_size_t aw86224_device_read(rt_device_t device, rt_off_t position,
                                     void *buffer, rt_size_t size)
{
    struct aw86224_rt_status *status = buffer;

    (void)device;
    (void)position;
    if (status == RT_NULL || size == 0)
        return 0;
    if (aw86224_device_ensure_ready() != RT_EOK)
        return 0;

    status->state = aw86224_get_state();
    status->gain = aw86224_get_gain();
    status->playing = aw86224_is_playing() ? 1U : 0U;
    return 1;
}

static rt_size_t aw86224_device_write(rt_device_t device, rt_off_t position,
                                      const void *buffer, rt_size_t size)
{
    (void)device;
    (void)position;
    if (buffer == RT_NULL || size == 0 || size > 0xFFFFU)
        return 0;
    if (aw86224_device_ensure_ready() != RT_EOK)
        return 0;

    return (aw86224_play_rtp((rt_uint8_t *)buffer, (rt_uint16_t)size,
                             RT_TRUE) == RT_EOK)
               ? size
               : 0;
}

static rt_err_t aw86224_device_control(rt_device_t device, int command,
                                       void *args)
{
    struct aw86224_rt_ram_effect *ram_effect;
    struct aw86224_rt_cont_effect *cont_effect;
    struct aw86224_rt_calibration *calibration;

    (void)device;
    if (command == RT_DEVICE_CTRL_SUSPEND)
        return aw86224_stop_playback();
    if (aw86224_device_ensure_ready() != RT_EOK)
        return -RT_ERROR;
    if (command == RT_DEVICE_CTRL_RESUME)
        return RT_EOK;

    switch (command)
    {
    case AW86224_RT_CTRL_STOP:
        return aw86224_stop_playback();
    case AW86224_RT_CTRL_SET_GAIN:
        return (args != RT_NULL)
                   ? aw86224_set_gain(*(rt_uint8_t *)args)
                   : -RT_EINVAL;
    case AW86224_RT_CTRL_PLAY_RAM:
        if (args == RT_NULL)
            return -RT_EINVAL;
        ram_effect = args;
        return aw86224_play_ram(ram_effect->wave_id, ram_effect->loop,
                                ram_effect->auto_brake);
    case AW86224_RT_CTRL_PLAY_CONT:
        if (args == RT_NULL)
            return -RT_EINVAL;
        cont_effect = args;
        return aw86224_play_cont(cont_effect->frequency_x10_hz,
                                 cont_effect->duration_ms,
                                 cont_effect->strength);
    case AW86224_RT_CTRL_GET_STATE:
        if (args == RT_NULL)
            return -RT_EINVAL;
        *(rt_uint8_t *)args = aw86224_get_state();
        return RT_EOK;
    case AW86224_RT_CTRL_GET_VBAT:
        return (args != RT_NULL) ? aw86224_measure_vbat(args) : -RT_EINVAL;
    case AW86224_RT_CTRL_DETECT_F0:
        return (args != RT_NULL) ? aw86224_f0_detect(args) : -RT_EINVAL;
    case AW86224_RT_CTRL_CALIBRATE_F0:
        if (args == RT_NULL)
            return -RT_EINVAL;
        calibration = args;
        return aw86224_f0_calibrate(calibration->expected_x10_hz,
                                    calibration->measured_x10_hz);
    case AW86224_RT_CTRL_GET_RESISTANCE:
        return (args != RT_NULL)
                   ? aw86224_measure_resistance(args)
                   : -RT_EINVAL;
    default:
        return -RT_EINVAL;
    }
}

#ifdef RT_USING_DEVICE_OPS
static const struct rt_device_ops g_aw86224_ops =
{
    .init = aw86224_device_init,
    .open = aw86224_device_open,
    .close = aw86224_device_close,
    .read = aw86224_device_read,
    .write = aw86224_device_write,
    .control = aw86224_device_control,
};
#endif

int rt_hw_aw86224_device_register(void)
{
    if (rt_device_find(AW86224_RT_DEVICE_NAME) != RT_NULL)
        return RT_EOK;

    rt_mutex_init(&g_init_lock, "aw_init", RT_IPC_FLAG_FIFO);
    rt_memset(&g_haptic_device, 0, sizeof(g_haptic_device));
    g_haptic_device.type = RT_Device_Class_Char;
#ifdef RT_USING_DEVICE_OPS
    g_haptic_device.ops = &g_aw86224_ops;
#else
    g_haptic_device.init = aw86224_device_init;
    g_haptic_device.open = aw86224_device_open;
    g_haptic_device.close = aw86224_device_close;
    g_haptic_device.read = aw86224_device_read;
    g_haptic_device.write = aw86224_device_write;
    g_haptic_device.control = aw86224_device_control;
#endif
    return rt_device_register(&g_haptic_device, AW86224_RT_DEVICE_NAME,
                              RT_DEVICE_FLAG_RDWR);
}
INIT_COMPONENT_EXPORT(rt_hw_aw86224_device_register);
