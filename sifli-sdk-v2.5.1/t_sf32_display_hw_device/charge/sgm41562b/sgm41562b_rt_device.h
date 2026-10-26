/* Optional RT-Thread Device adapter for SGM41562B. */
#ifndef __SGM41562B_RT_DEVICE_H__
#define __SGM41562B_RT_DEVICE_H__

#include <rtdevice.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SGM41562B_RT_DEVICE_NAME
#define SGM41562B_RT_DEVICE_NAME "sgm41562b"
#endif

struct sgm41562b_rt_status
{
    rt_uint8_t system_status;
    rt_uint8_t fault_status;
    rt_uint8_t charge_status;
    rt_uint8_t power_good;
};

enum sgm41562b_rt_command
{
    SGM41562B_RT_CTRL_GET_DEVICE_ID = 0x100,
    SGM41562B_RT_CTRL_GET_STATUS,
    SGM41562B_RT_CTRL_GET_FAULT,
    SGM41562B_RT_CTRL_ENABLE_CHARGING,
    SGM41562B_RT_CTRL_SET_HIZ,
    SGM41562B_RT_CTRL_SET_INPUT_VOLTAGE,
    SGM41562B_RT_CTRL_SET_INPUT_CURRENT,
    SGM41562B_RT_CTRL_SET_CHARGE_VOLTAGE,
    SGM41562B_RT_CTRL_SET_CHARGE_CURRENT,
    SGM41562B_RT_CTRL_WATCHDOG_RESET,
    SGM41562B_RT_CTRL_SOFTWARE_RESET,
    SGM41562B_RT_CTRL_ENTER_SHIPPING,
};

/* Registers the lazy-initialized device SGM41562B_RT_DEVICE_NAME. */
int rt_hw_sgm41562b_device_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __SGM41562B_RT_DEVICE_H__ */
