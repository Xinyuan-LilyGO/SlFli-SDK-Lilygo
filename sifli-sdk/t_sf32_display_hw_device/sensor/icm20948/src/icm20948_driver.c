#include "icm20948_driver.h"
#include "icm20948_reg.h"
#include "log.h"
#include "rthw.h"
#include <stdio.h>
#include <string.h>

#ifdef PKG_USING_ICM20948
static struct rt_i2c_bus_device *i2c_bus = NULL;
static struct icm20948_t icm20948_dev;

static int icm20948_i2c_read(void *handle, uint8_t reg_addr, uint8_t *reg_data,
                             uint16_t length)
{
    rt_size_t ret = 0;
    ret = rt_i2c_mem_read(handle, ICM20948_I2C_ADD_L, reg_addr, 1, reg_data,
                          length);
    if (ret != length)
    {
        LOG_E("read reg data failed");
        return -1;
    }
    return RT_EOK;
}

static int icm20948_i2c_write(void *handle, uint8_t reg_addr, uint8_t *reg_data,
                              uint16_t length)
{
    rt_size_t ret = 0;
    uint8_t *buf = rt_malloc(length);
    if (!buf)
    {
        return -RT_EOK;
    }
    memcpy(&buf[0], reg_data, length);
    ret =
        rt_i2c_mem_write(handle, ICM20948_I2C_ADD_L, reg_addr, 1, buf, length);
    rt_free(buf);
    if (ret != length)
    {
        LOG_E("write reg addr and data failed");
    }
    return RT_EOK;
}

static int icm20948_init(void)
{
    int ret;
    uint8_t chip_id = 0x00;
    rt_uint32_t ts = 0;
    uint8_t rst = 1;

    i2c_bus = rt_i2c_bus_device_find(ICM20948_I2C_BUS_NAME);
    if (i2c_bus == RT_NULL)
    {
        LOG_E("find %s device failed", ICM20948_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    if (rt_device_open((rt_device_t)i2c_bus, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        LOG_E("open %s device failed", ICM20948_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    struct rt_i2c_configuration configuration = {
        .mode = 0,
        .addr = 0,
        .timeout = 500,   // Waiting for timeout period (ms)
        .max_hz = 800000, // I2C rate (hz)
    };

    rt_i2c_configure(i2c_bus, &configuration);

    icm20948_dev.dev_ctx.handle = i2c_bus;
    icm20948_dev.dev_ctx.read_reg = icm20948_i2c_read;
    icm20948_dev.dev_ctx.write_reg = icm20948_i2c_write;
    if (icm20948_device_id_get(&icm20948_dev.dev_ctx, &chip_id) == RT_EOK)
    {
        if (chip_id != 0XEA)
        {
            LOG_E("This device(chip_id=0x%X) is not LSM6DSM", chip_id);
        }
    }
    LOG_I("ICM20948 chip_id: 0x%X", chip_id);
    ts = rt_tick_get_millisecond();

    do
    {
        icm20948_reset_get(&icm20948_dev.dev_ctx, &rst);
        if ((rt_tick_get_millisecond() - ts) > 1000)
        {
            LOG_E("Reset ICM20948 failed!");
            break;
        }
    } while (rst);
    icm20948_user_bank_set(&icm20948_dev.dev_ctx, ICM20948_SEL_USER_BANK_0);
    icm20948_sleep_set(&icm20948_dev.dev_ctx, ICM20948_RUN_MODE);
    rt_thread_mdelay(10);

    icm20948_user_bank_set(&icm20948_dev.dev_ctx, ICM20948_SEL_USER_BANK_2);
    icm20948_odr_align_set(&icm20948_dev.dev_ctx, 1);

    icm20948_gyro_smplrt_div_set(&icm20948_dev.dev_ctx, 0x7);
    icm20948_gyro_dlpfcfg_set(&icm20948_dev.dev_ctx, ICM20948_GYRO_DLPFCFG_6);
    icm20948_gyro_full_scale_set(&icm20948_dev.dev_ctx,
                                 ICM20948_GYRO_FS_SEL_500dps);
    icm20948_gyro_fchoice_set(&icm20948_dev.dev_ctx, ICM20948_ENABLE_GYRO_DLPF);

    icm20948_accel_smplrt_div_2_set(&icm20948_dev.dev_ctx, 0x7);
    icm20948_accel_dlpfcfg_set(&icm20948_dev.dev_ctx, ICM20948_ACCEL_DLPFCFG_6);
    icm20948_accel_full_scale_set(&icm20948_dev.dev_ctx,
                                  ICM20948_ACCEL_FS_SEL_2g);
    icm20948_accel_fchoice_set(&icm20948_dev.dev_ctx,
                               ICM20948_ENABLE_ACCEL_DLPF);

    icm20948_user_bank_set(&icm20948_dev.dev_ctx, ICM20948_SEL_USER_BANK_0);
    rt_thread_mdelay(100);

    return RT_EOK;
}

rt_err_t rt_icm20948_init(void)
{
    if (icm20948_init() != RT_EOK)
    {
        LOG_E("icm20948 init failed");
        return -RT_ERROR;
    }
    LOG_I("sensor icm20948 init success");
    if(ak09916_init() != RT_EOK)
    {
        LOG_E("ak09916 init failed");
        return -RT_ERROR;
    }
    LOG_I("sensor ak09916 init success");
    return RT_EOK;
}

int32_t icm20948_get_raw_acc(ImuAccel *acc)
{
    uint16_t reg_data[3] = {0};
    if (icm20948_acceleration_raw_get(&icm20948_dev.dev_ctx, reg_data))
    {
        return -RT_EOK;
    }
    acc->AccX = reg_data[0];
    acc->AccY = reg_data[1];
    acc->AccZ = reg_data[2];

    return RT_EOK;
}

int32_t icm20948_get_raw_gyro(ImuGyro *gyro)
{
    uint16_t reg_data[3] = {0};
    if (icm20948_angular_rate_raw_get(&icm20948_dev.dev_ctx, reg_data))
    {
        return -RT_EOK;
    }
    gyro->GyroX = reg_data[0];
    gyro->GyroY = reg_data[1];
    gyro->GyroZ = reg_data[2];
    return RT_EOK;
}

int32_t icm20948_get_raw_temp(int16_t *temp)
{
    if (icm20948_temperature_rate_raw_get(&icm20948_dev.dev_ctx, temp))
    {
        return -RT_EOK;
    }
    return RT_EOK;
}

int32_t icm20948_get_temp_data(float *temp)
{
    int16_t tempData;
    icm20948_get_raw_temp(&tempData);
    *temp = (float)(tempData * 1.0 - ICM20948_ROOM_TEMP_OFFSET) / 333.87 + 21.0f;
    return RT_EOK;
}

extern int32_t icm20948_real_data(ImuReal *accReal, ImuReal *gyroReal)
{
    ImuAccel accData;
    ImuGyro gyroData;
    icm20948_user_bank_set(&icm20948_dev.dev_ctx, ICM20948_SEL_USER_BANK_0);

    icm20948_get_raw_acc(&accData);
    icm20948_get_raw_gyro(&gyroData);

    rt_thread_mdelay(5);

    accReal->x = (float)(accData.AccX / 4096.0);
    accReal->y = (float)(accData.AccY / 4096.0);
    accReal->z = (float)(accData.AccZ / 4096.0);
    gyroReal->x = (float)(gyroData.GyroX / 16.4);
    gyroReal->y = (float)(gyroData.GyroY / 16.4);
    gyroReal->z = (float)(gyroData.GyroZ / 16.4);

    return RT_EOK;
}

/*********AK09916 magnetometer sensors*********/

int ak09916_init(void)
{
    int ret;
    icm20948_enable_i2c_master(&icm20948_dev.dev_ctx);
    ak09916_reset(&icm20948_dev.dev_ctx);
    rt_thread_mdelay(200);
    icm20948_reset_set(&icm20948_dev.dev_ctx, ICM20948_RESET);
    rt_thread_mdelay(50);
    icm20948_sleep_set(&icm20948_dev.dev_ctx, ICM20948_SLEEP);

    icm20948_user_bank_set(&icm20948_dev.dev_ctx, ICM20948_SEL_USER_BANK_2);
    icm20948_odr_align_set(&icm20948_dev.dev_ctx, 1);

    icm20948_enable_i2c_master(&icm20948_dev.dev_ctx);
    rt_thread_mdelay(100);
    uint16_t id = ak09916_id_get(&icm20948_dev.dev_ctx);
    if (id != AK09916_ID_1 && id != AK09916_ID_2)
    {
        return -RT_ERROR;
    }
    LOG_I("AK09916 chip_id: 0x%X", id);

    LOG_I("AK09916 init success");

    ak09916_set_mode(&icm20948_dev.dev_ctx, AK09916_CONT_MODE_20HZ);
    return RT_EOK;
}

int32_t ak09916_get_raw_mag(ImuMag *mag)
{
    uint16_t reg_data[3] = {0};
    if (ak09916_magnetometer_raw_get(&icm20948_dev.dev_ctx, reg_data))
    {
        return -RT_EOK;
    }
    mag->MagX = reg_data[0];
    mag->MagY = reg_data[1];
    mag->MagZ = reg_data[2];
    return RT_EOK;
}

extern void ak09916_real_data(ImuReal *magReal)
{
    ImuMag magData;
    icm20948_user_bank_set(&icm20948_dev.dev_ctx, ICM20948_SEL_USER_BANK_0);

    ak09916_get_raw_mag(&magData);

    magReal->x = (float)(magData.MagX * 0.15);
    magReal->y = (float)(magData.MagY * 0.15);
    magReal->z = (float)(magData.MagZ * 0.15);
}

/******I2C bus find address***************/
void rt_i2c_find_address(void)
{
    uint8_t reg_data = 0x00;
    for (uint8_t addr = 0x07; addr <= 0x7F; addr++)
    {
        struct rt_i2c_msg msgs[1];
        msgs[0].addr = addr;
        msgs[0].flags = RT_I2C_WR;
        msgs[0].buf = &reg_data;
        msgs[0].len = 1;

        if (rt_i2c_transfer(i2c_bus, msgs, 1) == 1)
        {
            LOG_I("Found device at address: 0x%02X", addr);
        }
    }
}

#endif