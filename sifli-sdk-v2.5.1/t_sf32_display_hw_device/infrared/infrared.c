#include "infrared.h"
#include "drv_io.h"
#include "bf0_hal.h"
#include "board.h"
#include <stdlib.h>

#define DBG_TAG "pwm"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define IR_CARRIER_PERIOD_NS   (26316)   // 38kHz -> 26.316us = 26316ns
#define IR_CARRIER_PULSE_NS    (13158)   // 50% 占空比

static struct rt_device_pwm *pwm_device = RT_NULL;
static rt_bool_t infrared_initialized;

rt_err_t infrared_init(void)
{
    rt_err_t result;

    if (infrared_initialized)
    {
        return RT_EOK;
    }

    if (HAL_PIN_Set(PAD_PA36, GPTIM2_CH1, PIN_NOPULL, 1) != 0)
    {
        LOG_E("Configure PA36 as GPTIM2_CH1 failed");
        return -RT_EIO;
    }

    pwm_device = (struct rt_device_pwm *)rt_device_find(INFRARED_PWM_NAME);
    if (pwm_device == RT_NULL)
    {
        LOG_E("PWM device %s not found", INFRARED_PWM_NAME);
        return -RT_ENOSYS;
    }

    result = rt_pwm_set(pwm_device, INFRARED_PWM_CHANNEL,
                        IR_CARRIER_PERIOD_NS, IR_CARRIER_PULSE_NS);
    if (result != RT_EOK)
    {
        LOG_E("Configure %s channel %d failed: %d", INFRARED_PWM_NAME,
              INFRARED_PWM_CHANNEL, result);
        return result;
    }

    result = rt_pwm_disable(pwm_device, INFRARED_PWM_CHANNEL);
    if (result != RT_EOK)
    {
        LOG_E("Disable %s channel %d failed: %d", INFRARED_PWM_NAME,
              INFRARED_PWM_CHANNEL, result);
        return result;
    }

    infrared_initialized = RT_TRUE;
    LOG_I("Infrared carrier ready: %s ch%d, period=%u ns, pulse=%u ns",
          INFRARED_PWM_NAME, INFRARED_PWM_CHANNEL, IR_CARRIER_PERIOD_NS,
          IR_CARRIER_PULSE_NS);
    return RT_EOK;
}

rt_err_t infrared_pwm_set(uint32_t period, uint32_t pulse)
{
    if (pwm_device == RT_NULL)
    {
        return -RT_ENOSYS;
    }

    return rt_pwm_set(pwm_device, INFRARED_PWM_CHANNEL, period, pulse);
}

rt_err_t infrared_pwm_enable(void)
{
    if (!infrared_initialized || pwm_device == RT_NULL)
    {
        return -RT_ENOSYS;
    }

    return rt_pwm_enable(pwm_device, INFRARED_PWM_CHANNEL);
}

rt_err_t infrared_pwm_disable(void)
{
    if (pwm_device == RT_NULL)
    {
        return -RT_ENOSYS;
    }

    return rt_pwm_disable(pwm_device, INFRARED_PWM_CHANNEL);
}

#ifdef FINSH_USING_MSH
static int infrared_carrier_test(int argc, char **argv)
{
    rt_uint32_t duration_ms = 1000;
    rt_err_t result;

    if (argc > 1)
    {
        duration_ms = (rt_uint32_t)atoi(argv[1]);
        if (duration_ms < 10 || duration_ms > 5000)
        {
            rt_kprintf("Usage: ir_carrier_test [10..5000 ms]\n");
            return -RT_EINVAL;
        }
    }

    result = infrared_init();
    if (result != RT_EOK)
    {
        rt_kprintf("IR carrier init failed: %d\n", result);
        return result;
    }

    result = infrared_pwm_enable();
    if (result != RT_EOK)
    {
        rt_kprintf("IR carrier enable failed: %d\n", result);
        return result;
    }

    rt_kprintf("IR carrier on PA36 for %u ms\n", duration_ms);
    rt_thread_mdelay(duration_ms);
    result = infrared_pwm_disable();
    rt_kprintf("IR carrier stopped: %d\n", result);
    return result;
}
MSH_CMD_EXPORT_ALIAS(infrared_carrier_test, ir_carrier_test,
                     output 38kHz carrier on PA36);
#endif
