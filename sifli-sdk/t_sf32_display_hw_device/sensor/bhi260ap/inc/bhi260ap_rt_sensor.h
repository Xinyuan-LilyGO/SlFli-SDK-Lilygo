/*
 * Optional RT-Thread Sensor adapter for the BHI260AP driver.
 */
#ifndef __BHI260AP_RT_SENSOR_H__
#define __BHI260AP_RT_SENSOR_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BHI260AP_RT_SENSOR_NAME
#define BHI260AP_RT_SENSOR_NAME "bhi260"
#endif

/* Registers acce_bhi260, gyro_bhi260 and step_bhi260. */
int rt_hw_bhi260ap_sensor_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __BHI260AP_RT_SENSOR_H__ */
