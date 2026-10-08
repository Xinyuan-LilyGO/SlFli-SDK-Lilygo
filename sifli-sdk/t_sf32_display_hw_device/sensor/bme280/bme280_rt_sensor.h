/*
 * Optional RT-Thread Sensor adapter for the BME280 driver.
 */
#ifndef __BME280_RT_SENSOR_H__
#define __BME280_RT_SENSOR_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BME280_RT_SENSOR_NAME
#define BME280_RT_SENSOR_NAME "bme280"
#endif

/* Registers temp_bme280, humi_bme280 and baro_bme280. */
int rt_hw_bme280_sensor_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __BME280_RT_SENSOR_H__ */
