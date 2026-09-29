/* Optional RT-Thread Device adapter for TCA8418. */
#ifndef __TCA8418_RT_DEVICE_H__
#define __TCA8418_RT_DEVICE_H__

#include "tca8418.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TCA8418_RT_DEVICE_NAME
#define TCA8418_RT_DEVICE_NAME "tca8418"
#endif

enum tca8418_rt_command
{
    /* args: rt_int32_t *, timeout in ticks; RT_WAITING_FOREVER is valid. */
    TCA8418_RT_CTRL_SET_READ_TIMEOUT = 0x100,
    TCA8418_RT_CTRL_LOCK,
    TCA8418_RT_CTRL_UNLOCK,
    TCA8418_RT_CTRL_FLUSH,
};

/*
 * rt_device_read() uses size as key_board_event_msg_t capacity and returns
 * the number of events copied.
 */
int rt_hw_tca8418_device_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __TCA8418_RT_DEVICE_H__ */
