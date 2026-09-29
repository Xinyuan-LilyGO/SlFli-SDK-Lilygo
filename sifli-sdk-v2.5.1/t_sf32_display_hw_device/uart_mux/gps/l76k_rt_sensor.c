/* Optional RT-Thread Sensor adapter for the L76K GPS driver. */
#include "l76k_rt_sensor.h"

#include "xl9555.h"
#include <sensor.h>

#define DBG_TAG "l76k.sensor"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifndef L76K_RT_UART_NAME
#define L76K_RT_UART_NAME "uart2"
#endif

#ifndef L76K_RT_BAUDRATE
#define L76K_RT_BAUDRATE 115200
#endif

static struct rt_sensor_device g_gps_sensor;
static struct rt_mutex g_init_lock;
static rt_bool_t g_initialized;
static rt_bool_t g_powered;

static rt_err_t l76k_sensor_ensure_ready(void)
{
    rt_err_t result = RT_EOK;

    rt_mutex_take(&g_init_lock, RT_WAITING_FOREVER);
    if (!g_initialized)
    {
        result = xl9555_init();
        if (result == RT_EOK)
            result = l76k_init(L76K_RT_UART_NAME, L76K_RT_BAUDRATE);
        if (result == RT_EOK)
        {
            g_initialized = RT_TRUE;
            g_powered = RT_TRUE;
        }
    }
    else if (!g_powered)
    {
        result = l76k_set_mode(L76K_MODE_NORMAL);
        if (result == 0)
            g_powered = RT_TRUE;
    }
    rt_mutex_release(&g_init_lock);
    return (result == 0) ? RT_EOK : -RT_ERROR;
}

static rt_size_t l76k_sensor_fetch(struct rt_sensor_device *sensor,
                                   void *buffer, rt_size_t length)
{
    struct rt_sensor_data *data = buffer;
    GPSInfo info;

    if (sensor == RT_NULL || data == RT_NULL || length == 0)
        return 0;
    if (sensor->config.mode != RT_SENSOR_MODE_POLLING)
        return 0;
    if (l76k_sensor_ensure_ready() != RT_EOK)
        return 0;
    if (l76k_get_gps_info(&info) != 0)
        return 0;

    data->type = RT_SENSOR_CLASS_GPS;
    data->data.gps.lati = info.latitude;
    data->data.gps.longi = info.longitude;
    data->data.gps.alti = info.altitude;
    data->timestamp = rt_sensor_get_ts();
    return 1;
}

static rt_err_t l76k_sensor_control(struct rt_sensor_device *sensor,
                                    int command, void *args)
{
    rt_uint32_t value;
    int result;

    if (sensor == RT_NULL)
        return -RT_EINVAL;

    switch (command)
    {
    case RT_SENSOR_CTRL_GET_ID:
        if (args == RT_NULL)
            return -RT_EINVAL;
        *(rt_uint8_t *)args = 0x76;
        return RT_EOK;
    case RT_SENSOR_CTRL_SET_MODE:
        value = (rt_uint32_t)(rt_ubase_t)args;
        return (value == RT_SENSOR_MODE_POLLING) ? RT_EOK : -RT_ENOSYS;
    case RT_SENSOR_CTRL_SET_POWER:
        value = (rt_uint32_t)(rt_ubase_t)args;
        if (value == RT_SENSOR_POWER_DOWN)
        {
            if (!g_initialized || !g_powered)
                return RT_EOK;
            result = l76k_set_mode(L76K_MODE_SLEEP);
            if (result == 0)
                g_powered = RT_FALSE;
            return (result == 0) ? RT_EOK : -RT_ERROR;
        }
        if (value == RT_SENSOR_POWER_NORMAL ||
            value == RT_SENSOR_POWER_LOW || value == RT_SENSOR_POWER_HIGH)
            return l76k_sensor_ensure_ready();
        return -RT_EINVAL;
    case L76K_RT_SENSOR_CTRL_GET_INFO:
        if (args == RT_NULL)
            return -RT_EINVAL;
        if (l76k_sensor_ensure_ready() != RT_EOK)
            return -RT_ERROR;
        return (l76k_get_gps_info((GPSInfo *)args) == 0)
                   ? RT_EOK
                   : -RT_ERROR;
    case RT_SENSOR_CTRL_SET_RANGE:
    case RT_SENSOR_CTRL_SET_ODR:
    case RT_SENSOR_CTRL_SELF_TEST:
        return -RT_ENOSYS;
    default:
        return -RT_EINVAL;
    }
}

static const struct rt_sensor_ops g_l76k_sensor_ops =
{
    .fetch_data = l76k_sensor_fetch,
    .control = l76k_sensor_control,
};

int rt_hw_l76k_sensor_register(void)
{
    if (rt_device_find("gps_" L76K_RT_SENSOR_NAME) != RT_NULL)
        return RT_EOK;

    rt_mutex_init(&g_init_lock, "gps_init", RT_IPC_FLAG_FIFO);
    rt_memset(&g_gps_sensor, 0, sizeof(g_gps_sensor));
    g_gps_sensor.info.type = RT_SENSOR_CLASS_GPS;
    g_gps_sensor.info.vendor = RT_SENSOR_VENDOR_UNKNOWN;
    g_gps_sensor.info.model = "l76k";
    g_gps_sensor.info.unit = RT_SENSOR_UNIT_DEG;
    g_gps_sensor.info.intf_type = RT_SENSOR_INTF_UART;
    g_gps_sensor.info.range_min = -180;
    g_gps_sensor.info.range_max = 180;
    g_gps_sensor.info.period_min = 1000;
    g_gps_sensor.config.intf.dev_name = L76K_RT_UART_NAME;
    g_gps_sensor.config.intf.type = RT_SENSOR_INTF_UART;
    g_gps_sensor.config.irq_pin.pin = RT_PIN_NONE;
    g_gps_sensor.config.mode = RT_SENSOR_MODE_POLLING;
    g_gps_sensor.config.power = RT_SENSOR_POWER_NONE;
    g_gps_sensor.ops = &g_l76k_sensor_ops;

    return rt_hw_sensor_register(&g_gps_sensor, L76K_RT_SENSOR_NAME,
                                 RT_DEVICE_FLAG_RDONLY, RT_NULL);
}
INIT_COMPONENT_EXPORT(rt_hw_l76k_sensor_register);
