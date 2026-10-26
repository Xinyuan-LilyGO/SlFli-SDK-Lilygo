#include "ir_nec.h"
#include "infrared.h"
#include "rthw.h"
#include <rtdevice.h>

#define NEC_HIGH_US (9000)
#define NEC_LOW_US (4500)
#define NEC_BIT_0_HIGH (560)
#define NEC_BIT_0_LOW (560)
#define NEC_BIT_1_HIGH (560)
#define NEC_BIT_1_LOW (1690)
#define NEC_REPEAT_HIGH (9000)
#define NEC_REPEAT_LOW (2250)
#define NEC_END_HIGH (560)

static void us_delay(uint32_t us)
{
    rt_hw_us_delay(us);
}

static rt_err_t send_level(int level, uint32_t duration_us)
{
    rt_err_t result;

    if (level)
    {
        result = infrared_pwm_enable();
    }
    else
    {
        result = infrared_pwm_disable();
    }

    if (result != RT_EOK)
    {
        return result;
    }

    us_delay(duration_us);
    return RT_EOK;
}

static rt_err_t send_bit(uint8_t bit)
{
    rt_err_t result;

    result = send_level(1, bit ? NEC_BIT_1_HIGH : NEC_BIT_0_HIGH);
    if (result != RT_EOK)
    {
        return result;
    }

    return send_level(0, bit ? NEC_BIT_1_LOW : NEC_BIT_0_LOW);
}

static rt_err_t send_byte(uint8_t data)
{
    rt_err_t result;

    for (int i = 0; i < 8; i++)
    {
        result = send_bit((data >> i) & 0x01);
        if (result != RT_EOK)
        {
            return result;
        }
    }

    return RT_EOK;
}

int ir_nec_init(void)
{
    return infrared_init();
}

int ir_nec_send(uint8_t address, uint8_t command)
{
    rt_err_t result;

#ifdef RT_USING_PM
    rt_pm_request(PM_SLEEP_MODE_IDLE);
#endif

    result = send_level(1, NEC_HIGH_US);
    if (result == RT_EOK)
    {
        result = send_level(0, NEC_LOW_US);
    }
    if (result == RT_EOK)
    {
        result = send_byte(address);
    }
    if (result == RT_EOK)
    {
        result = send_byte((uint8_t)~address);
    }
    if (result == RT_EOK)
    {
        result = send_byte(command);
    }
    if (result == RT_EOK)
    {
        result = send_byte((uint8_t)~command);
    }
    if (result == RT_EOK)
    {
        result = send_level(1, NEC_END_HIGH);
    }

    infrared_pwm_disable();
#ifdef RT_USING_PM
    rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
    return result;
}

void ir_nec_send_repeat(void)
{
#ifdef RT_USING_PM
    rt_pm_request(PM_SLEEP_MODE_IDLE);
#endif
    send_level(1, NEC_REPEAT_HIGH);
    send_level(0, NEC_REPEAT_LOW);
    send_level(1, NEC_END_HIGH);
    infrared_pwm_disable();
#ifdef RT_USING_PM
    rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
}
