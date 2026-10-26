/* Optional RT-Thread Sensor adapter for the L76K GPS driver. */
#ifndef __L76K_RT_SENSOR_H__
#define __L76K_RT_SENSOR_H__

#include "l76k.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef L76K_RT_SENSOR_NAME
#define L76K_RT_SENSOR_NAME "l76k"
#endif

enum l76k_rt_sensor_command
{
    /* args: GPSInfo *. The standard GPS sample only contains lat/lon/alt. */
    L76K_RT_SENSOR_CTRL_GET_INFO = 0x100,
};

/* Registers gps_l76k. */
int rt_hw_l76k_sensor_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __L76K_RT_SENSOR_H__ */
