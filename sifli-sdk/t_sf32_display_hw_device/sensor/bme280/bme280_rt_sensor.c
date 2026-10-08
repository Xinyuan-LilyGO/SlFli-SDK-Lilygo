/*
 * Optional RT-Thread Sensor adapter for the BME280 driver.
 */
#include "bme280_rt_sensor.h"

#include "bme280.h"
#include <sensor.h>

#define DBG_TAG "bme280.sensor"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static struct rt_sensor_device g_temp_sensor;
static struct rt_sensor_device g_humi_sensor;
static struct rt_sensor_device g_baro_sensor;
static struct rt_mutex g_init_lock;
static rt_bool_t g_initialized;
static rt_bool_t g_powered;
static rt_uint8_t g_active_sensors;

static rt_int32_t scaled_float(float value, float scale)
{
    float scaled = value * scale;

    return (rt_int32_t)(scaled + ((scaled >= 0.0f) ? 0.5f : -0.5f));
}

static rt_err_t bme280_sensor_ensure_ready(void)
{
    rt_err_t result = RT_EOK;

    rt_mutex_take(&g_init_lock, RT_WAITING_FOREVER);
    if (!g_initialized)
    {
        result = bme280_init();
        if (result == RT_EOK)
        {
            g_initialized = RT_TRUE;
            g_powered = RT_TRUE;
        }
    }

    if (result == RT_EOK && !g_powered)
    {
        result = bme280_config(bme280_get_device(), BME280_MODE_NORMAL,
                               BME280_SAMPLING_X2, BME280_SAMPLING_X2,
                               BME280_SAMPLING_X1, BME280_FILTER_X16,
                               BME280_STANDBY_MS_10);
        if (result == RT_EOK)
            g_powered = RT_TRUE;
    }
    rt_mutex_release(&g_init_lock);
    return result;
}

static rt_size_t bme280_sensor_fetch(struct rt_sensor_device *sensor,
                                     void *buffer, rt_size_t length)
{
    struct rt_sensor_data *data = buffer;
    float value;
    rt_err_t result;

    if (sensor == RT_NULL || data == RT_NULL || length == 0)
        return 0;
    if (sensor->config.mode != RT_SENSOR_MODE_POLLING)
        return 0;

    result = bme280_sensor_ensure_ready();
    if (result != RT_EOK)
        return 0;

    switch (sensor->info.type)
    {
    case RT_SENSOR_CLASS_TEMP:
        result = bme280_read_temperature(bme280_get_device(), &value);
        if (result == RT_EOK)
            data->data.temp = scaled_float(value, 10.0f);
        break;
    case RT_SENSOR_CLASS_HUMI:
        result = bme280_read_humidity(bme280_get_device(), &value);
        if (result == RT_EOK)
            data->data.humi = scaled_float(value, 10.0f);
        break;
    case RT_SENSOR_CLASS_BARO:
        result = bme280_read_pressure(bme280_get_device(), &value);
        if (result == RT_EOK)
            data->data.baro = scaled_float(value, 1.0f);
        break;
    default:
        return 0;
    }

    if (result != RT_EOK)
        return 0;

    data->type = sensor->info.type;
    data->timestamp = rt_sensor_get_ts();
    return 1;
}

static rt_err_t bme280_sensor_control(struct rt_sensor_device *sensor,
                                      int command, void *args)
{
    rt_uint8_t value;
    rt_bool_t power_down;
    rt_err_t result;

    if (sensor == RT_NULL)
        return -RT_EINVAL;

    switch (command)
    {
    case RT_SENSOR_CTRL_GET_ID:
        if (args == RT_NULL)
            return -RT_EINVAL;
        *(rt_uint8_t *)args = 0x60;
        return RT_EOK;

    case RT_SENSOR_CTRL_SET_MODE:
        value = (rt_uint8_t)(rt_ubase_t)args;
        return (value == RT_SENSOR_MODE_POLLING) ? RT_EOK : -RT_ENOSYS;

    case RT_SENSOR_CTRL_SET_POWER:
        value = (rt_uint8_t)(rt_ubase_t)args;
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
            if (!power_down)
                return RT_EOK;

            result = bme280_config(bme280_get_device(), BME280_MODE_SLEEP,
                                   BME280_SAMPLING_X2, BME280_SAMPLING_X2,
                                   BME280_SAMPLING_X1, BME280_FILTER_X16,
                                   BME280_STANDBY_MS_10);
            if (result == RT_EOK)
                g_powered = RT_FALSE;
            return result;
        }
        if (value == RT_SENSOR_POWER_NORMAL ||
            value == RT_SENSOR_POWER_LOW || value == RT_SENSOR_POWER_HIGH)
        {
            result = bme280_sensor_ensure_ready();
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
    case RT_SENSOR_CTRL_SET_ODR:
    case RT_SENSOR_CTRL_SELF_TEST:
        return -RT_ENOSYS;

    default:
        return -RT_EINVAL;
    }
}

static const struct rt_sensor_ops g_bme280_sensor_ops =
{
    .fetch_data = bme280_sensor_fetch,
    .control = bme280_sensor_control,
};

static void bme280_sensor_setup(struct rt_sensor_device *sensor,
                                rt_uint8_t type, rt_uint8_t unit,
                                rt_int32_t minimum, rt_int32_t maximum)
{
    rt_memset(sensor, 0, sizeof(*sensor));
    sensor->info.type = type;
    sensor->info.vendor = RT_SENSOR_VENDOR_BOSCH;
    sensor->info.model = "bme280";
    sensor->info.unit = unit;
    sensor->info.intf_type = RT_SENSOR_INTF_I2C;
    sensor->info.range_min = minimum;
    sensor->info.range_max = maximum;
    sensor->info.period_min = 10;
    sensor->config.intf.dev_name = BME280_I2C_BUS_NAME;
    sensor->config.intf.type = RT_SENSOR_INTF_I2C;
    sensor->config.irq_pin.pin = RT_PIN_NONE;
    sensor->config.mode = RT_SENSOR_MODE_POLLING;
    sensor->config.power = RT_SENSOR_POWER_NONE;
    sensor->ops = &g_bme280_sensor_ops;
}

int rt_hw_bme280_sensor_register(void)
{
    rt_err_t result;

    if (rt_device_find("temp_" BME280_RT_SENSOR_NAME) != RT_NULL)
        return RT_EOK;

    rt_mutex_init(&g_init_lock, "bme_init", RT_IPC_FLAG_FIFO);
    bme280_sensor_setup(&g_temp_sensor, RT_SENSOR_CLASS_TEMP,
                        RT_SENSOR_UNIT_DCELSIUS, -400, 850);
    bme280_sensor_setup(&g_humi_sensor, RT_SENSOR_CLASS_HUMI,
                        RT_SENSOR_UNIT_PERMILLAGE, 0, 1000);
    bme280_sensor_setup(&g_baro_sensor, RT_SENSOR_CLASS_BARO,
                        RT_SENSOR_UNIT_PA, 30000, 110000);

    result = rt_hw_sensor_register(&g_temp_sensor, BME280_RT_SENSOR_NAME,
                                   RT_DEVICE_FLAG_RDONLY, RT_NULL);
    if (result != RT_EOK)
        return result;
    result = rt_hw_sensor_register(&g_humi_sensor, BME280_RT_SENSOR_NAME,
                                   RT_DEVICE_FLAG_RDONLY, RT_NULL);
    if (result != RT_EOK)
        return result;
    return rt_hw_sensor_register(&g_baro_sensor, BME280_RT_SENSOR_NAME,
                                 RT_DEVICE_FLAG_RDONLY, RT_NULL);
}
INIT_COMPONENT_EXPORT(rt_hw_bme280_sensor_register);
