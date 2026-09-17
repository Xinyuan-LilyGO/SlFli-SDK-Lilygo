#ifndef __LORA_RT_DEVICE_H__
#define __LORA_RT_DEVICE_H__

#include <rtdevice.h>

#include "lora_app.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LORA_RT_DEVICE_NAME
#define LORA_RT_DEVICE_NAME "lora"
#endif

struct lora_rt_config
{
    rt_uint32_t frequency;
    rt_int8_t tx_power;
    rt_uint8_t bandwidth;
    rt_uint8_t spreading_factor;
    rt_uint8_t coding_rate;
    rt_uint16_t preamble_length;
    rt_bool_t public_network;
    rt_bool_t iq_inverted;
    rt_bool_t crc_enabled;
    rt_bool_t rx_boost;
};

enum lora_rt_command
{
    LORA_RT_CTRL_SET_CONFIG = 0x100,
    LORA_RT_CTRL_GET_CONFIG,
    LORA_RT_CTRL_START_RX,
    LORA_RT_CTRL_SLEEP,
    LORA_RT_CTRL_STANDBY,
    LORA_RT_CTRL_GET_STATUS,
    LORA_RT_CTRL_GET_LAST_RX_INFO,
    LORA_RT_CTRL_FLUSH_RX,
};

int rt_hw_lora_device_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __LORA_RT_DEVICE_H__ */
