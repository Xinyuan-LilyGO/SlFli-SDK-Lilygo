/* Optional RT-Thread Device adapter for XL9555. */
#ifndef __XL9555_RT_DEVICE_H__
#define __XL9555_RT_DEVICE_H__

#include "xl9555.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XL9555_RT_DEVICE_NAME
#define XL9555_RT_DEVICE_NAME "xl9555"
#endif

struct xl9555_rt_pin_mode
{
    rt_uint8_t pin;
    rt_uint8_t mode;
};

struct xl9555_rt_irq_config
{
    xl9555_irq_callback_t callback;
    void *user_data;
};

enum xl9555_rt_command
{
    XL9555_RT_CTRL_SET_PIN_MODE = 0x100,
    XL9555_RT_CTRL_READ_ALL,
    XL9555_RT_CTRL_ATTACH_IRQ,
    XL9555_RT_CTRL_ENABLE_IRQ,
};

/*
 * rt_device_read/write use position as pin number (0-15) and transfer one
 * byte. READ_ALL returns the 16 pin states by polling each pin.
 */
int rt_hw_xl9555_device_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __XL9555_RT_DEVICE_H__ */
