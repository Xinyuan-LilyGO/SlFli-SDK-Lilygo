#ifndef __BME280_H__
#define __BME280_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <rtdevice.h>
#include <rtthread.h>
#include <stdint.h>
#include <string.h>

/* I2C addresses */
#define BME280_ADDRESS 0x77
#define BME280_ADDRESS_ALTERNATE 0x76

/* Registers */
#define BME280_REGISTER_DIG_T1 0x88
#define BME280_REGISTER_DIG_T2 0x8A
#define BME280_REGISTER_DIG_T3 0x8C
#define BME280_REGISTER_DIG_P1 0x8E
#define BME280_REGISTER_DIG_P2 0x90
#define BME280_REGISTER_DIG_P3 0x92
#define BME280_REGISTER_DIG_P4 0x94
#define BME280_REGISTER_DIG_P5 0x96
#define BME280_REGISTER_DIG_P6 0x98
#define BME280_REGISTER_DIG_P7 0x9A
#define BME280_REGISTER_DIG_P8 0x9C
#define BME280_REGISTER_DIG_P9 0x9E
#define BME280_REGISTER_DIG_H1 0xA1
#define BME280_REGISTER_DIG_H2 0xE1
#define BME280_REGISTER_DIG_H3 0xE3
#define BME280_REGISTER_DIG_H4 0xE4
#define BME280_REGISTER_DIG_H5 0xE5
#define BME280_REGISTER_DIG_H6 0xE7
#define BME280_REGISTER_CHIPID 0xD0
#define BME280_REGISTER_SOFTRESET 0xE0
#define BME280_REGISTER_CONTROLHUM 0xF2
#define BME280_REGISTER_STATUS 0xF3
#define BME280_REGISTER_CONTROL 0xF4
#define BME280_REGISTER_CONFIG 0xF5
#define BME280_REGISTER_PRESSUREDATA 0xF7
#define BME280_REGISTER_TEMPDATA 0xFA
#define BME280_REGISTER_HUMIDDATA 0xFD

/* Soft reset value */
#define BME280_SOFT_RESET 0xB6

    /* Sensor mode definitions */
    typedef enum
    {
        BME280_MODE_SLEEP = 0x00,
        BME280_MODE_FORCED = 0x01,
        BME280_MODE_NORMAL = 0x03
    } bme280_mode_t;

    /* Oversampling definitions */
    typedef enum
    {
        BME280_SAMPLING_NONE = 0x00,
        BME280_SAMPLING_X1 = 0x01,
        BME280_SAMPLING_X2 = 0x02,
        BME280_SAMPLING_X4 = 0x03,
        BME280_SAMPLING_X8 = 0x04,
        BME280_SAMPLING_X16 = 0x05
    } bme280_sampling_t;

    /* Filter definitions */
    typedef enum
    {
        BME280_FILTER_OFF = 0x00,
        BME280_FILTER_X2 = 0x01,
        BME280_FILTER_X4 = 0x02,
        BME280_FILTER_X8 = 0x03,
        BME280_FILTER_X16 = 0x04
    } bme280_filter_t;

    /* Standby time definitions */
    typedef enum
    {
        BME280_STANDBY_MS_0_5 = 0x00,
        BME280_STANDBY_MS_10 = 0x06,
        BME280_STANDBY_MS_20 = 0x07,
        BME280_STANDBY_MS_62_5 = 0x01,
        BME280_STANDBY_MS_125 = 0x02,
        BME280_STANDBY_MS_250 = 0x03,
        BME280_STANDBY_MS_500 = 0x04,
        BME280_STANDBY_MS_1000 = 0x05
    } bme280_standby_t;

    /* Calibration data structure */
    typedef struct
    {
        uint16_t dig_T1;
        int16_t dig_T2;
        int16_t dig_T3;
        uint16_t dig_P1;
        int16_t dig_P2;
        int16_t dig_P3;
        int16_t dig_P4;
        int16_t dig_P5;
        int16_t dig_P6;
        int16_t dig_P7;
        int16_t dig_P8;
        int16_t dig_P9;
        uint8_t dig_H1;
        int16_t dig_H2;
        uint8_t dig_H3;
        int16_t dig_H4;
        int16_t dig_H5;
        int8_t dig_H6;
    } bme280_calib_data_t;

    /* Device structure */
    typedef struct
    {
        struct rt_i2c_bus_device *i2c;
        uint8_t addr;
        bme280_calib_data_t calib;
        int32_t t_fine;
        uint8_t meas_reg;   /* stored ctrl_meas register value */
        uint8_t hum_reg;    /* stored ctrl_hum register value */
        uint8_t config_reg; /* stored config register value */
        struct rt_mutex lock;
    } bme280_device_t;

    int bme280_init(void);
    void bme280_deinit(bme280_device_t *dev);
    bme280_device_t *bme280_get_device(void);

    int bme280_config(bme280_device_t *dev, bme280_mode_t mode,
                      bme280_sampling_t temp_sampling,
                      bme280_sampling_t press_sampling,
                      bme280_sampling_t hum_sampling, bme280_filter_t filter,
                      bme280_standby_t standby);
    int bme280_take_forced_measurement(bme280_device_t *dev);
    int bme280_read_temperature(bme280_device_t *dev, float *temp);
    int bme280_read_pressure(bme280_device_t *dev, float *pressure);
    int bme280_read_humidity(bme280_device_t *dev, float *humidity);
    int bme280_read_all(bme280_device_t *dev, float *temp, float *press,
                        float *hum);

#ifdef __cplusplus
}
#endif

#endif
