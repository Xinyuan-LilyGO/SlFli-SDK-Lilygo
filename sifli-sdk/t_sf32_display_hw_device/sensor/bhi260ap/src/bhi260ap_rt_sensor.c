/*
 * Optional RT-Thread Sensor adapter for the BHI260AP driver.
 */
#include "bhi260ap_rt_sensor.h"

#include "bhi260ap.h"
#include <sensor.h>

#define DBG_TAG "bhi260.sensor"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifndef BHI260AP_RT_SENSOR_ODR
#define BHI260AP_RT_SENSOR_ODR 100
#endif

static struct rt_sensor_device g_acce_sensor;
static struct rt_sensor_device g_gyro_sensor;
static struct rt_sensor_device g_step_sensor;
static struct rt_mutex g_init_lock;
static rt_bool_t g_initialized;
static rt_bool_t g_powered;
static rt_uint8_t g_active_sensors;
static rt_uint16_t g_acce_odr = BHI260AP_RT_SENSOR_ODR;
static rt_uint16_t g_gyro_odr = BHI260AP_RT_SENSOR_ODR;
static rt_uint16_t g_step_odr = BHI260AP_RT_SENSOR_ODR;

static rt_int32_t scaled_float(float value, float scale)
{
    float scaled = value * scale;

    return (rt_int32_t)(scaled + ((scaled >= 0.0f) ? 0.5f : -0.5f));
}

static rt_uint8_t bhi260ap_sensor_id(rt_uint8_t sensor_type)
{
    switch (sensor_type)
    {
    case RT_SENSOR_CLASS_ACCE:
        return BHY2_SENSOR_ID_ACC;
    case RT_SENSOR_CLASS_GYRO:
        return BHY2_SENSOR_ID_GYRO;
    case RT_SENSOR_CLASS_STEP:
        return BHY2_SENSOR_ID_STC;
    default:
        return 0;
    }
}

static rt_uint16_t *bhi260ap_sensor_odr(rt_uint8_t sensor_type)
{
    switch (sensor_type)
    {
    case RT_SENSOR_CLASS_ACCE:
        return &g_acce_odr;
    case RT_SENSOR_CLASS_GYRO:
        return &g_gyro_odr;
    case RT_SENSOR_CLASS_STEP:
        return &g_step_odr;
    default:
        return RT_NULL;
    }
}

static rt_err_t bhi260ap_configure_enabled_sensors(void)
{
    if (!bhi260ap_configure(BHY2_SENSOR_ID_ACC, (rt_uint8_t)g_acce_odr, 0))
        return -RT_ERROR;
    if (!bhi260ap_configure(BHY2_SENSOR_ID_GYRO, (rt_uint8_t)g_gyro_odr, 0))
        return -RT_ERROR;
    if (!bhi260ap_configure(BHY2_SENSOR_ID_STC, (rt_uint8_t)g_step_odr, 0))
        return -RT_ERROR;
    return RT_EOK;
}

static rt_err_t bhi260ap_sensor_ensure_ready(void)
{
    rt_err_t result = RT_EOK;

    rt_mutex_take(&g_init_lock, RT_WAITING_FOREVER);
    if (!g_initialized)
    {
        result = rt_bhi260ap_init();
        if (result == RT_EOK)
        {
            result = bhi260ap_configure_enabled_sensors();
            if (result == RT_EOK)
            {
                bhi260ap_thread_resume();
                g_initialized = RT_TRUE;
                g_powered = RT_TRUE;
            }
        }
    }
    else if (!g_powered)
    {
        result = bhi260ap_configure_enabled_sensors();
        if (result == RT_EOK)
        {
            bhi260ap_thread_resume();
            g_powered = RT_TRUE;
        }
    }
    rt_mutex_release(&g_init_lock);
    return result;
}

static rt_size_t bhi260ap_sensor_fetch(struct rt_sensor_device *sensor,
                                       void *buffer, rt_size_t length)
{
    struct rt_sensor_data *data = buffer;

    if (sensor == RT_NULL || data == RT_NULL || length == 0)
        return 0;
    if (sensor->config.mode != RT_SENSOR_MODE_POLLING)
        return 0;
    if (bhi260ap_sensor_ensure_ready() != RT_EOK)
        return 0;

    switch (sensor->info.type)
    {
    case RT_SENSOR_CLASS_ACCE:
    {
        struct bhy2_data_xyz_float value = bhi260ap_get_acc_sensor_data();
        data->data.acce.x = scaled_float(value.x, 1000.0f);
        data->data.acce.y = scaled_float(value.y, 1000.0f);
        data->data.acce.z = scaled_float(value.z, 1000.0f);
        break;
    }
    case RT_SENSOR_CLASS_GYRO:
    {
        struct bhy2_data_xyz_float value = bhi260ap_get_gyro_sensor_data();
        data->data.gyro.x = scaled_float(value.x, 1000.0f);
        data->data.gyro.y = scaled_float(value.y, 1000.0f);
        data->data.gyro.z = scaled_float(value.z, 1000.0f);
        break;
    }
    case RT_SENSOR_CLASS_STEP:
        data->data.step = bhi260ap_get_step_counter_sensor_data();
        break;
    default:
        return 0;
    }

    data->type = sensor->info.type;
    data->timestamp = rt_sensor_get_ts();
    return 1;
}

static rt_err_t bhi260ap_sensor_control(struct rt_sensor_device *sensor,
                                        int command, void *args)
{
    rt_uint16_t *odr;
    rt_uint32_t value;
    rt_uint8_t sensor_id;
    rt_bool_t power_down;
    rt_err_t result;

    if (sensor == RT_NULL)
        return -RT_EINVAL;
    sensor_id = bhi260ap_sensor_id(sensor->info.type);

    switch (command)
    {
    case RT_SENSOR_CTRL_GET_ID:
        if (args == RT_NULL)
            return -RT_EINVAL;
        *(rt_uint8_t *)args = sensor_id;
        return RT_EOK;

    case RT_SENSOR_CTRL_SET_MODE:
        value = (rt_uint32_t)(rt_ubase_t)args;
        return (value == RT_SENSOR_MODE_POLLING) ? RT_EOK : -RT_ENOSYS;

    case RT_SENSOR_CTRL_SET_ODR:
        value = (rt_uint32_t)(rt_ubase_t)args;
        if (value == 0 || value > 255)
            return -RT_EINVAL;
        odr = bhi260ap_sensor_odr(sensor->info.type);
        if (odr == RT_NULL)
            return -RT_EINVAL;
        *odr = (rt_uint16_t)value;
        if (!g_initialized || !g_powered)
            return RT_EOK;
        return bhi260ap_configure(sensor_id, (rt_uint8_t)value, 0)
                   ? RT_EOK
                   : -RT_ERROR;

    case RT_SENSOR_CTRL_SET_POWER:
        value = (rt_uint32_t)(rt_ubase_t)args;
        if (value == RT_SENSOR_POWER_DOWN)
        {
            if (!g_initialized)
                return RT_EOK;

            power_down = RT_FALSE;
            rt_mutex_take(&g_init_lock, RT_WAITING_FOREVER);
            if (sensor->config.power == RT_SENSOR_POWER_NORMAL &&
                g_active_sensors > 0)
            {
                g_active_sensors--;
                power_down = (g_active_sensors == 0);
            }
            rt_mutex_release(&g_init_lock);
            if (power_down && g_powered)
            {
                bhi260ap_configure(sensor_id, 0, 0);
                bhi260ap_thread_pause();
                g_powered = RT_FALSE;
            }
            return RT_EOK;
        }
        if (value == RT_SENSOR_POWER_NORMAL ||
            value == RT_SENSOR_POWER_LOW || value == RT_SENSOR_POWER_HIGH)
        {
            result = bhi260ap_sensor_ensure_ready();
            if (result == RT_EOK &&
                sensor->config.power != RT_SENSOR_POWER_NORMAL)
            {
                rt_mutex_take(&g_init_lock, RT_WAITING_FOREVER);
                g_active_sensors++;
                rt_mutex_release(&g_init_lock);
            }
            return result;
        }
        return -RT_EINVAL;

    case RT_SENSOR_CTRL_SET_RANGE:
    case RT_SENSOR_CTRL_SELF_TEST:
        return -RT_ENOSYS;

    default:
        return -RT_EINVAL;
    }
}

static const struct rt_sensor_ops g_bhi260ap_sensor_ops =
{
    .fetch_data = bhi260ap_sensor_fetch,
    .control = bhi260ap_sensor_control,
};

static void bhi260ap_sensor_setup(struct rt_sensor_device *sensor,
                                  rt_uint8_t type, rt_uint8_t unit,
                                  rt_int32_t minimum, rt_int32_t maximum)
{
    rt_memset(sensor, 0, sizeof(*sensor));
    sensor->info.type = type;
    sensor->info.vendor = RT_SENSOR_VENDOR_BOSCH;
    sensor->info.model = "bhi260ap";
    sensor->info.unit = unit;
    sensor->info.intf_type = RT_SENSOR_INTF_I2C;
    sensor->info.range_min = minimum;
    sensor->info.range_max = maximum;
    sensor->info.period_min = 4;
    sensor->config.intf.dev_name = BHI260AP_I2C_BUS_NAME;
    sensor->config.intf.type = RT_SENSOR_INTF_I2C;
    sensor->config.irq_pin.pin = RT_PIN_NONE;
    sensor->config.mode = RT_SENSOR_MODE_POLLING;
    sensor->config.power = RT_SENSOR_POWER_NONE;
    sensor->ops = &g_bhi260ap_sensor_ops;
}

int rt_hw_bhi260ap_sensor_register(void)
{
    rt_err_t result;

    if (rt_device_find("acce_" BHI260AP_RT_SENSOR_NAME) != RT_NULL)
        return RT_EOK;

    rt_mutex_init(&g_init_lock, "bhi_init", RT_IPC_FLAG_FIFO);
    bhi260ap_sensor_setup(&g_acce_sensor, RT_SENSOR_CLASS_ACCE,
                          RT_SENSOR_UNIT_MG, -16000, 16000);
    bhi260ap_sensor_setup(&g_gyro_sensor, RT_SENSOR_CLASS_GYRO,
                          RT_SENSOR_UNIT_MDPS, -2000000, 2000000);
    bhi260ap_sensor_setup(&g_step_sensor, RT_SENSOR_CLASS_STEP,
                          RT_SENSOR_UNIT_ONE, 0, 0x7FFFFFFF);

    result = rt_hw_sensor_register(&g_acce_sensor, BHI260AP_RT_SENSOR_NAME,
                                   RT_DEVICE_FLAG_RDONLY, RT_NULL);
    if (result != RT_EOK)
        return result;
    result = rt_hw_sensor_register(&g_gyro_sensor, BHI260AP_RT_SENSOR_NAME,
                                   RT_DEVICE_FLAG_RDONLY, RT_NULL);
    if (result != RT_EOK)
        return result;
    return rt_hw_sensor_register(&g_step_sensor, BHI260AP_RT_SENSOR_NAME,
                                 RT_DEVICE_FLAG_RDONLY, RT_NULL);
}
INIT_COMPONENT_EXPORT(rt_hw_bhi260ap_sensor_register);
