/* Optional RT-Thread Device adapter for SGM41562B. */
#include "sgm41562b_rt_device.h"

#include "sgm41562b.h"

#define DBG_TAG "sgm41562b.dev"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static struct rt_device g_charger_device;
static struct rt_mutex g_init_lock;
static sgm41562b_handle_t g_charger;

static rt_err_t sgm41562b_device_ensure_ready(void)
{
    rt_mutex_take(&g_init_lock, RT_WAITING_FOREVER);
    if (g_charger == RT_NULL)
    {
        g_charger = sgm41562b_get_handle();
        if (g_charger == RT_NULL)
            g_charger = sgm41562b_init(SGM41562B_I2C_BUS_NAME,
                                       SGM41562B_IRQ_PIN);
    }
    rt_mutex_release(&g_init_lock);
    return (g_charger != RT_NULL) ? RT_EOK : -RT_ERROR;
}

static rt_err_t sgm41562b_device_init(rt_device_t device)
{
    (void)device;
    return sgm41562b_device_ensure_ready();
}

static rt_err_t sgm41562b_device_open(rt_device_t device, rt_uint16_t flags)
{
    (void)device;
    (void)flags;
    return sgm41562b_device_ensure_ready();
}

static rt_err_t sgm41562b_device_close(rt_device_t device)
{
    (void)device;
    return RT_EOK;
}

static rt_size_t sgm41562b_device_read(rt_device_t device, rt_off_t position,
                                       void *buffer, rt_size_t size)
{
    struct sgm41562b_rt_status *status = buffer;

    (void)device;
    (void)position;
    if (status == RT_NULL || size == 0)
        return 0;
    if (sgm41562b_device_ensure_ready() != RT_EOK)
        return 0;
    if (sgm41562b_get_system_status(g_charger, &status->system_status) !=
        RT_EOK)
        return 0;
    if (sgm41562b_get_fault_status(g_charger, &status->fault_status) != RT_EOK)
        return 0;

    status->charge_status =
        (status->system_status & SGM41562B_CHG_STAT_MASK) >>
        SGM41562B_CHG_STAT_SHIFT;
    status->power_good =
        (status->system_status & SGM41562B_PG_STAT) ? 1U : 0U;
    return 1;
}

static rt_err_t sgm41562b_device_control(rt_device_t device, int command,
                                         void *args)
{
    (void)device;

    if (command == RT_DEVICE_CTRL_SUSPEND)
        return RT_EOK;
    if (sgm41562b_device_ensure_ready() != RT_EOK)
        return -RT_ERROR;
    if (command == RT_DEVICE_CTRL_RESUME)
        return RT_EOK;

    switch (command)
    {
    case SGM41562B_RT_CTRL_GET_DEVICE_ID:
        return (args != RT_NULL)
                   ? sgm41562b_get_device_id(g_charger, args)
                   : -RT_EINVAL;
    case SGM41562B_RT_CTRL_GET_STATUS:
        return (args != RT_NULL)
                   ? sgm41562b_get_system_status(g_charger, args)
                   : -RT_EINVAL;
    case SGM41562B_RT_CTRL_GET_FAULT:
        return (args != RT_NULL)
                   ? sgm41562b_get_fault_status(g_charger, args)
                   : -RT_EINVAL;
    case SGM41562B_RT_CTRL_ENABLE_CHARGING:
        return (args != RT_NULL)
                   ? sgm41562b_enable_charging(g_charger,
                                               *(rt_bool_t *)args)
                   : -RT_EINVAL;
    case SGM41562B_RT_CTRL_SET_HIZ:
        return (args != RT_NULL)
                   ? sgm41562b_set_hiz_mode(g_charger, *(rt_bool_t *)args)
                   : -RT_EINVAL;
    case SGM41562B_RT_CTRL_SET_INPUT_VOLTAGE:
        return (args != RT_NULL)
                   ? sgm41562b_set_input_voltage_limit(
                         g_charger, *(rt_uint16_t *)args)
                   : -RT_EINVAL;
    case SGM41562B_RT_CTRL_SET_INPUT_CURRENT:
        return (args != RT_NULL)
                   ? sgm41562b_set_input_current_limit(
                         g_charger, *(rt_uint16_t *)args)
                   : -RT_EINVAL;
    case SGM41562B_RT_CTRL_SET_CHARGE_VOLTAGE:
        return (args != RT_NULL)
                   ? sgm41562b_set_charge_voltage(g_charger,
                                                  *(rt_uint16_t *)args)
                   : -RT_EINVAL;
    case SGM41562B_RT_CTRL_SET_CHARGE_CURRENT:
        return (args != RT_NULL)
                   ? sgm41562b_set_charge_current(g_charger,
                                                  *(rt_uint16_t *)args)
                   : -RT_EINVAL;
    case SGM41562B_RT_CTRL_WATCHDOG_RESET:
        return sgm41562b_watchdog_reset(g_charger);
    case SGM41562B_RT_CTRL_SOFTWARE_RESET:
        return sgm41562b_software_reset(g_charger);
    case SGM41562B_RT_CTRL_ENTER_SHIPPING:
        return sgm41562b_enter_shipping_mode(g_charger);
    default:
        return -RT_EINVAL;
    }
}

#ifdef RT_USING_DEVICE_OPS
static const struct rt_device_ops g_sgm41562b_ops =
{
    .init = sgm41562b_device_init,
    .open = sgm41562b_device_open,
    .close = sgm41562b_device_close,
    .read = sgm41562b_device_read,
    .write = RT_NULL,
    .control = sgm41562b_device_control,
};
#endif

int rt_hw_sgm41562b_device_register(void)
{
    if (rt_device_find(SGM41562B_RT_DEVICE_NAME) != RT_NULL)
        return RT_EOK;

    rt_mutex_init(&g_init_lock, "sgm_init", RT_IPC_FLAG_FIFO);
    rt_memset(&g_charger_device, 0, sizeof(g_charger_device));
    g_charger_device.type = RT_Device_Class_Miscellaneous;
#ifdef RT_USING_DEVICE_OPS
    g_charger_device.ops = &g_sgm41562b_ops;
#else
    g_charger_device.init = sgm41562b_device_init;
    g_charger_device.open = sgm41562b_device_open;
    g_charger_device.close = sgm41562b_device_close;
    g_charger_device.read = sgm41562b_device_read;
    g_charger_device.control = sgm41562b_device_control;
#endif
    return rt_device_register(&g_charger_device,
                              SGM41562B_RT_DEVICE_NAME,
                              RT_DEVICE_FLAG_RDWR);
}
INIT_COMPONENT_EXPORT(rt_hw_sgm41562b_device_register);
