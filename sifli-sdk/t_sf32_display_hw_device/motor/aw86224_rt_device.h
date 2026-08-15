/* Optional RT-Thread Device adapter for AW86224. */
#ifndef __AW86224_RT_DEVICE_H__
#define __AW86224_RT_DEVICE_H__

#include <rtdevice.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AW86224_RT_DEVICE_NAME
#define AW86224_RT_DEVICE_NAME "aw86224"
#endif

struct aw86224_rt_status
{
    rt_uint8_t state;
    rt_uint8_t gain;
    rt_uint8_t playing;
};

struct aw86224_rt_ram_effect
{
    rt_uint8_t wave_id;
    rt_uint8_t loop;
    rt_bool_t auto_brake;
};

struct aw86224_rt_cont_effect
{
    rt_uint32_t frequency_x10_hz;
    rt_uint32_t duration_ms;
    rt_uint8_t strength;
};

struct aw86224_rt_calibration
{
    rt_uint32_t expected_x10_hz;
    rt_uint32_t measured_x10_hz;
};

enum aw86224_rt_command
{
    AW86224_RT_CTRL_STOP = 0x100,
    AW86224_RT_CTRL_SET_GAIN,
    AW86224_RT_CTRL_PLAY_RAM,
    AW86224_RT_CTRL_PLAY_CONT,
    AW86224_RT_CTRL_GET_STATE,
    AW86224_RT_CTRL_GET_VBAT,
    AW86224_RT_CTRL_DETECT_F0,
    AW86224_RT_CTRL_CALIBRATE_F0,
    AW86224_RT_CTRL_GET_RESISTANCE,
};

/* Registers the lazy-initialized character device AW86224_RT_DEVICE_NAME. */
int rt_hw_aw86224_device_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __AW86224_RT_DEVICE_H__ */
