/*
 * BME280 driver for RT-Thread (C version)
 * Based on Adafruit BME280 library and Bosch Sensortec BME280 datasheet.
 * I2C address: 0x76 (SDO low) or 0x77 (SDO high)
 */
#include "bme280.h"
#include "ulog.h"

static bme280_device_t bme_dev;
static rt_bool_t bme280_initialized;

/* Static functions */
static int bme280_write_reg(bme280_device_t *dev, uint8_t reg, uint8_t val)
{
    struct rt_i2c_msg msg;
    uint8_t buf[2] = {reg, val};

    msg.addr = dev->addr;
    msg.flags = RT_I2C_WR;
    msg.buf = buf;
    msg.len = 2;

#ifdef RT_USING_PM
    rt_pm_request(PM_SLEEP_MODE_IDLE);
    if (rt_i2c_transfer(dev->i2c, &msg, 1) == 1)
    {
        rt_pm_release(PM_SLEEP_MODE_IDLE);
        return RT_EOK;
    }
    else
    {
        rt_pm_release(PM_SLEEP_MODE_IDLE);
        return -RT_ERROR;
    }
#else
    if (rt_i2c_transfer(dev->i2c, &msg, 1) == 1)
        return RT_EOK;
    else
        return -RT_ERROR;
#endif
}

static int bme280_read_regs(bme280_device_t *dev, uint8_t reg, uint8_t *buf,
                            uint16_t len)
{
    struct rt_i2c_msg msgs[2];
    uint8_t reg_buf = reg;

    msgs[0].addr = dev->addr;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = &reg_buf;
    msgs[0].len = 1;

    msgs[1].addr = dev->addr;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = buf;
    msgs[1].len = len;

#ifdef RT_USING_PM
    rt_pm_request(PM_SLEEP_MODE_IDLE);
    if (rt_i2c_transfer(dev->i2c, msgs, 2) == 2)
    {
        rt_pm_release(PM_SLEEP_MODE_IDLE);
        return RT_EOK;
    }
    else
    {
        rt_pm_release(PM_SLEEP_MODE_IDLE);
        return -RT_ERROR;
    }
#else
    if (rt_i2c_transfer(dev->i2c, msgs, 2) == 2)
        return RT_EOK;
    else
        return -RT_ERROR;
#endif
}

static uint8_t bme280_read8(bme280_device_t *dev, uint8_t reg)
{
    uint8_t value = 0;
    bme280_read_regs(dev, reg, &value, 1);
    return value;
}

static uint16_t bme280_read16_le(bme280_device_t *dev, uint8_t reg)
{
    uint8_t buf[2];
    if (bme280_read_regs(dev, reg, buf, 2) != RT_EOK)
        return 0;
    return (uint16_t)buf[1] << 8 | buf[0];
}

static int16_t bme280_read_s16_le(bme280_device_t *dev, uint8_t reg)
{
    return (int16_t)bme280_read16_le(dev, reg);
}

static uint16_t bme280_read16_be(bme280_device_t *dev, uint8_t reg)
{
    uint8_t buf[2];
    if (bme280_read_regs(dev, reg, buf, 2) != RT_EOK)
        return 0;
    return ((uint16_t)buf[0] << 8) | buf[1];
}

static uint32_t bme280_read24(bme280_device_t *dev, uint8_t reg)
{
    uint8_t buf[3];
    if (bme280_read_regs(dev, reg, buf, 3) != RT_EOK)
        return 0;
    return ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
}

static int bme280_read_coefficients(bme280_device_t *dev)
{
    bme280_calib_data_t *c = &dev->calib;

    c->dig_T1 = bme280_read16_le(dev, BME280_REGISTER_DIG_T1);
    c->dig_T2 = bme280_read_s16_le(dev, BME280_REGISTER_DIG_T2);
    c->dig_T3 = bme280_read_s16_le(dev, BME280_REGISTER_DIG_T3);

    c->dig_P1 = bme280_read16_le(dev, BME280_REGISTER_DIG_P1);
    c->dig_P2 = bme280_read_s16_le(dev, BME280_REGISTER_DIG_P2);
    c->dig_P3 = bme280_read_s16_le(dev, BME280_REGISTER_DIG_P3);
    c->dig_P4 = bme280_read_s16_le(dev, BME280_REGISTER_DIG_P4);
    c->dig_P5 = bme280_read_s16_le(dev, BME280_REGISTER_DIG_P5);
    c->dig_P6 = bme280_read_s16_le(dev, BME280_REGISTER_DIG_P6);
    c->dig_P7 = bme280_read_s16_le(dev, BME280_REGISTER_DIG_P7);
    c->dig_P8 = bme280_read_s16_le(dev, BME280_REGISTER_DIG_P8);
    c->dig_P9 = bme280_read_s16_le(dev, BME280_REGISTER_DIG_P9);

    c->dig_H1 = bme280_read8(dev, BME280_REGISTER_DIG_H1);
    c->dig_H2 = bme280_read_s16_le(dev, BME280_REGISTER_DIG_H2);
    c->dig_H3 = bme280_read8(dev, BME280_REGISTER_DIG_H3);
    c->dig_H4 = ((int16_t)bme280_read8(dev, BME280_REGISTER_DIG_H4) << 4) |
                (bme280_read8(dev, BME280_REGISTER_DIG_H4 + 1) & 0x0F);
    c->dig_H5 = ((int16_t)bme280_read8(dev, BME280_REGISTER_DIG_H5 + 1) << 4) |
                (bme280_read8(dev, BME280_REGISTER_DIG_H5) >> 4);
    c->dig_H6 = (int8_t)bme280_read8(dev, BME280_REGISTER_DIG_H6);

    return RT_EOK;
}

/* Wait until calibration copy is done */
static void bme280_wait_for_calibration(bme280_device_t *dev)
{
    while (bme280_read8(dev, BME280_REGISTER_STATUS) & 0x01)
    {
        rt_thread_mdelay(1);
    }
}

/*!
 * @brief Initialize BME280 sensor
 * @return pointer to device structure, or NULL on failure
 */
int bme280_init(void)
{
    struct rt_i2c_bus_device *i2c;
    uint8_t chip_id;

    if (bme280_initialized)
        return RT_EOK;

    i2c = rt_i2c_bus_device_find(BME280_I2C_BUS_NAME);
    if (!i2c)
    {
        rt_kprintf("BME280: I2C bus %s not found\n", BME280_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    if (rt_device_open((rt_device_t)i2c, RT_DEVICE_FLAG_RDWR) != RT_EOK)
    {
        rt_kprintf("BME280: I2C bus %s open failed\n", BME280_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    memset(&bme_dev, 0, sizeof(bme280_device_t));
    bme_dev.i2c = i2c;
    bme_dev.addr = BME280_ADDRESS_ALTERNATE;
    rt_mutex_init(&bme_dev.lock, "bme280_lock", RT_IPC_FLAG_FIFO);

    /* Check chip ID */
    rt_mutex_take(&bme_dev.lock, RT_WAITING_FOREVER);
    chip_id = bme280_read8(&bme_dev, BME280_REGISTER_CHIPID);
    rt_mutex_release(&bme_dev.lock);
    if (chip_id != 0x60)
    {
        rt_kprintf("BME280: wrong chip ID 0x%02X (expected 0x60)\n", chip_id);
        return -RT_ERROR;
    }

    /* Soft reset */
    bme280_write_reg(&bme_dev, BME280_REGISTER_SOFTRESET, BME280_SOFT_RESET);
    rt_thread_mdelay(10);

    /* Wait for calibration data to be ready */
    bme280_wait_for_calibration(&bme_dev);

    /* Read calibration coefficients */
    rt_mutex_take(&bme_dev.lock, RT_WAITING_FOREVER);
    if (bme280_read_coefficients(&bme_dev) != RT_EOK)
    {
        rt_mutex_release(&bme_dev.lock);
        return -RT_ERROR;
    }
    rt_mutex_release(&bme_dev.lock);

    /* Default configuration: normal mode, x16 oversampling, filter off */
    bme280_config(&bme_dev, BME280_MODE_NORMAL, BME280_SAMPLING_X2,
                  BME280_SAMPLING_X2, BME280_SAMPLING_X1, BME280_FILTER_X16,
                  BME280_STANDBY_MS_10);
    bme280_initialized = RT_TRUE;
    return RT_EOK;
}

bme280_device_t *bme280_get_device(void)
{
    return &bme_dev;
}

/*!
 * @brief Configure BME280 sensor
 * @param dev device structure
 * @param mode sensor mode (SLEEP, FORCED, NORMAL)
 * @param temp_sampling temperature oversampling
 * @param press_sampling pressure oversampling
 * @param hum_sampling humidity oversampling
 * @param filter IIR filter coefficient
 * @param standby standby time (normal mode only)
 * @return RT_EOK on success
 */
int bme280_config(bme280_device_t *dev, bme280_mode_t mode,
                  bme280_sampling_t temp_sampling,
                  bme280_sampling_t press_sampling,
                  bme280_sampling_t hum_sampling, bme280_filter_t filter,
                  bme280_standby_t standby)
{
    uint8_t ctrl_hum, ctrl_meas, config;

    if (!dev)
        return -RT_ERROR;

    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);

    /* Set humidity control */
    ctrl_hum = hum_sampling & 0x07;
    bme280_write_reg(dev, BME280_REGISTER_CONTROLHUM, ctrl_hum);
    dev->hum_reg = ctrl_hum;

    /* Set config register (standby and filter) */
    config = ((standby & 0x07) << 5) | ((filter & 0x07) << 2);
    bme280_write_reg(dev, BME280_REGISTER_CONFIG, config);
    dev->config_reg = config;

    /* Set ctrl_meas register (oversampling and mode) */
    ctrl_meas = ((temp_sampling & 0x07) << 5) | ((press_sampling & 0x07) << 2) |
                (mode & 0x03);
    bme280_write_reg(dev, BME280_REGISTER_CONTROL, ctrl_meas);
    dev->meas_reg = ctrl_meas;

    rt_mutex_release(&dev->lock);
    return RT_EOK;
}

/*!
 * @brief Trigger a forced measurement (only in forced mode)
 * @param dev device structure
 * @return RT_EOK if measurement completed, else -RT_ERROR on timeout
 */
int bme280_take_forced_measurement(bme280_device_t *dev)
{
    uint8_t mode;

    if (!dev)
        return -RT_ERROR;

    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);
    mode = dev->meas_reg & 0x03;
    rt_mutex_release(&dev->lock);

    if (mode != BME280_MODE_FORCED)
        return RT_EOK; /* not in forced mode, nothing to do */

    /* Write the same ctrl_meas register again to trigger forced mode */
    bme280_write_reg(dev, BME280_REGISTER_CONTROL, dev->meas_reg);

    /* Wait for measurement to complete */
    uint32_t timeout = rt_tick_get() + rt_tick_from_millisecond(2000);
    while ((bme280_read8(dev, BME280_REGISTER_STATUS) & 0x08) &&
           rt_tick_get() < timeout)
    {
        rt_thread_mdelay(1);
    }

    if (rt_tick_get() >= timeout)
        return -RT_ETIMEOUT;

    return RT_EOK;
}

/*!
 * @brief Read temperature (in degrees Celsius)
 * @param dev device structure
 * @param temp pointer to store temperature (float)
 * @return RT_EOK on success
 */
int bme280_read_temperature(bme280_device_t *dev, float *temp)
{
    int32_t adc_T, var1, var2;
    bme280_calib_data_t *c = &dev->calib;

    if (!dev || !temp)
        return -RT_ERROR;

    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);
    adc_T = bme280_read24(dev, BME280_REGISTER_TEMPDATA);
    rt_mutex_release(&dev->lock);

    adc_T >>= 4; /* 20-bit data */

    // log_d("raw temperature: %d", adc_T);

    // var1 = (int32_t)((adc_T / 8) - ((int32_t)c->dig_T1 * 2));
    // var1 = (var1 * (int32_t)c->dig_T2) / 2048;
    // var2 = (int32_t)((adc_T / 16) - (int32_t)c->dig_T1);
    // var2 = (((var2 * var2) / 4096) * (int32_t)c->dig_T3) / 16384;

    // dev->t_fine = var1 + var2;
    // int32_t T = (dev->t_fine * 5 + 128) / 256;
    // *temp = (float)T / 100.0f;

    var1 = ((double)adc_T / 16384.0 - (double)c->dig_T1 / 1024.0) *
           (double)c->dig_T2;
    var2 = ((double)adc_T / 131072.0 - (double)c->dig_T1 / 8192.0) *
           ((double)adc_T / 131072.0 - (double)c->dig_T1 / 8192.0) *
           (double)c->dig_T3;
    dev->t_fine = (int32_t)(var1 + var2);
    *temp = (float)(dev->t_fine / 5120.0);
    // log_d("temperature: %lf %d", *temp, dev->t_fine);
    return RT_EOK;
}

/*!
 * @brief Read pressure (in Pascal)
 * @param dev device structure
 * @param pressure pointer to store pressure (float, Pa)
 * @return RT_EOK on success
 */
int bme280_read_pressure(bme280_device_t *dev, float *pressure)
{
    int64_t var1, var2, var3, var4;
    bme280_calib_data_t *c = &dev->calib;
    int32_t adc_P;
    float P;

    if (!dev || !pressure)
        return -RT_ERROR;

    /* First read temperature to update t_fine */
    float temp_unused;
    bme280_read_temperature(dev, &temp_unused);

    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);
    adc_P = bme280_read24(dev, BME280_REGISTER_PRESSUREDATA);
    rt_mutex_release(&dev->lock);

    adc_P >>= 4; /* 20-bit data */

    var1 = ((int64_t)dev->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)c->dig_P6;
    var2 = var2 + ((var1 * (int64_t)c->dig_P5) * 131072);
    var2 = var2 + (((int64_t)c->dig_P4) * 34359738368);
    var1 = ((var1 * var1 * (int64_t)c->dig_P3) / 256) +
           ((var1 * (int64_t)c->dig_P2) * 4096);
    var3 = ((int64_t)1) * 140737488355328;
    var1 = (var3 + var1) * ((int64_t)c->dig_P1) / 8589934592;

    if (var1 == 0)
    {
        *pressure = 0;
        return -RT_ERROR;
    }

    var4 = 1048576 - adc_P;
    var4 = (((var4 * 2147483648) - var2) * 3125) / var1;
    var1 = (((int64_t)c->dig_P9) * (var4 / 8192) * (var4 / 8192)) / 33554432;
    var2 = (((int64_t)c->dig_P8) * var4) / 524288;
    var4 = ((var4 + var1 + var2) / 256) + (((int64_t)c->dig_P7) * 16);
    P = (float)var4 / 256.0f;
    *pressure = P;
    return RT_EOK;
}

/*!
 * @brief Read relative humidity (in percent)
 * @param dev device structure
 * @param humidity pointer to store humidity (%RH)
 * @return RT_EOK on success
 */
int bme280_read_humidity(bme280_device_t *dev, float *humidity)
{
    int32_t var1, var2, var3, var4, var5;
    bme280_calib_data_t *c = &dev->calib;
    int32_t adc_H;
    uint32_t H;

    if (!dev || !humidity)
        return -RT_ERROR;

    /* First read temperature to update t_fine */
    float temp_unused;
    bme280_read_temperature(dev, &temp_unused);

    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);
    /* Humidity raw data at 0xFD-0xFE is MSB-first (big-endian) */
    adc_H = bme280_read16_be(dev, BME280_REGISTER_HUMIDDATA);
    rt_mutex_release(&dev->lock);

    /* Bosch datasheet compensation formula (integer version) */
    var1 = dev->t_fine - 76800;
    var2 = (adc_H * 16384);
    var3 = ((int32_t)c->dig_H4 * 1048576);
    var4 = ((int32_t)c->dig_H5 * var1);
    var5 = (((var2 - var3) - var4) + 16384) / 32768;

    var2 = (var1 * (int32_t)c->dig_H6) / 1024;
    var3 = (var1 * (int32_t)c->dig_H3) / 2048;
    var4 = ((var2 * (var3 + 32768)) / 1024) + 2097152;
    /* Use 64-bit to avoid int32_t overflow in var4 * dig_H2 */
    var2 = (int32_t)(((int64_t)var4 * (int32_t)c->dig_H2 + 8192) / 16384);

    /* Use 64-bit to avoid int32_t overflow in var5 * var2 */
    var3 = (int32_t)((int64_t)var5 * var2);
    var4 = ((var3 / 32768) * (var3 / 32768)) / 128;
    var5 = var3 - ((var4 * (int32_t)c->dig_H1) / 16);

    if (var5 < 0)
        var5 = 0;
    if (var5 > 419430400)
        var5 = 419430400;

    H = (uint32_t)(var5 / 4096);
    *humidity = (float)H / 1024.0f;
    return RT_EOK;
}

/*!
 * @brief Read temperature, pressure, and humidity in one call
 * @param dev device structure
 * @param temp pointer to temperature (°C) (can be NULL)
 * @param press pointer to pressure (Pa) (can be NULL)
 * @param hum pointer to humidity (%RH) (can be NULL)
 * @return RT_EOK on success
 */
int bme280_read_all(bme280_device_t *dev, float *temp, float *press, float *hum)
{
    int ret;

    if (!dev)
        return -RT_ERROR;

    ret = bme280_read_temperature(dev, temp);
    if (ret != RT_EOK)
        return ret;

    if (press)
    {
        ret = bme280_read_pressure(dev, press);
        if (ret != RT_EOK)
            return ret;
    }

    if (hum)
    {
        ret = bme280_read_humidity(dev, hum);
        if (ret != RT_EOK)
            return ret;
    }

    return RT_EOK;
}

/*!
 * @brief Deinitialize BME280 device
 * @param dev device structure
 */
void bme280_deinit(bme280_device_t *dev)
{
    if (!dev)
        return;

    /* Put sensor in sleep mode to save power */
    bme280_write_reg(dev, BME280_REGISTER_CONTROL, BME280_MODE_SLEEP);

    rt_mutex_detach(&dev->lock);
    memset(dev, 0, sizeof(bme280_device_t));
    bme280_initialized = RT_FALSE;
}
