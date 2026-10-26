/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-05-29     juzhango     first version
 */

#include <rtthread.h>
#include <sensor_icm20948.h>

int icm20948_example(void)
{
    struct rt_sensor_config cfg;
    cfg.intf.dev_name = "i2c1";
    rt_hw_icm20948_init("icm20948", &cfg);
    return RT_EOK;
}
INIT_APP_EXPORT(icm20948_example);