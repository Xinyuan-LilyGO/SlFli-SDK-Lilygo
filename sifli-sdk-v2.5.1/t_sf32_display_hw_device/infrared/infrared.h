#ifndef __INFRARED_H__
#define __INFRARED_H__

#include "rtdevice.h"
#include "rtthread.h"

#ifdef __cplusplus__
extern "C"
{
#endif

    rt_err_t infrared_init(void);
    rt_err_t infrared_pwm_set(uint32_t period, uint32_t pulse);
    rt_err_t infrared_pwm_enable(void);
    rt_err_t infrared_pwm_disable(void);

#ifdef __cplusplus__
}
#endif

#endif
