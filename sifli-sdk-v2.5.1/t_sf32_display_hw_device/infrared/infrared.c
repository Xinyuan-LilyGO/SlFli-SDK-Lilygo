#include "infrared.h"
#include "drv_io.h"
#include "bf0_hal.h"
#include "board.h"

#define DBG_TAG "pwm"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define IR_CARRIER_PERIOD_NS   (26316)   // 38kHz -> 26.316us = 26316ns
#define IR_CARRIER_PULSE_NS    (13158)   // 50% 占空比

struct rt_device_pwm *pwm_device = RT_NULL;

int8_t infrared_init(void)
{
    HAL_PIN_Set(PAD_PA36, GPTIM2_CH1, PIN_NOPULL, 1);

    pwm_device = (struct rt_device_pwm *)rt_device_find(INFRARED_PWM_NAME);
    if (pwm_device == RT_NULL)
    {
        return -RT_ERROR;
    }
    return RT_EOK;
}

void infrared_pwm_set(uint32_t period, uint32_t pulse)
{
    rt_pwm_set(pwm_device, INFRARED_PWM_CHANNEL, period, pulse);
}

void infrared_pwm_enable(void)
{
    static uint8_t configured = 0;
    if (!configured) {
        rt_pwm_set(pwm_device, INFRARED_PWM_CHANNEL,
                   IR_CARRIER_PERIOD_NS, IR_CARRIER_PULSE_NS);
        configured = 1;
    }
    rt_pwm_enable(pwm_device, INFRARED_PWM_CHANNEL);
}

void infrared_pwm_disable(void)
{
    rt_pwm_disable(pwm_device, INFRARED_PWM_CHANNEL);
}