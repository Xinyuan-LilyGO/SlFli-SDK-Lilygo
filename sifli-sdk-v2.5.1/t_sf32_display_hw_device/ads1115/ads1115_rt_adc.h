/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ADS1115_RT_ADC_H__
#define __ADS1115_RT_ADC_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ADS1115_ADC_DEVICE_NAME
#define ADS1115_ADC_DEVICE_NAME "ads1115"
#endif

#define ADS1115_RT_ADC_CHANNEL_COUNT 4U

/*
 * Register AIN0-AIN3 as RT-Thread ADC channels 0-3.
 * rt_adc_read() returns voltage in 0.1 mV units on this SiFli port.
 */
int rt_hw_ads1115_adc_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADS1115_RT_ADC_H__ */
