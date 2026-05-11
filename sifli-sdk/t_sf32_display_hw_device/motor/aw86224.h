/*
 * aw86224.h - AW86224 LRA Haptic Driver Header File
 */

#ifndef __AW86224_H__
#define __AW86224_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* AW86224 I2C address: 7-bit address is 0x58, left shift 1 bit for RT-Thread
 * I2C API */
#define AW86224_I2C_ADDR (0x58)

/* AW86224 Register Map */
#define AW86224_REG_SRST 0x00
#define AW86224_REG_SYST 0x01
#define AW86224_REG_SYSINT 0x02
#define AW86224_REG_SYSINTM 0x03
#define AW86224_REG_SYST2 0x04
#define AW86224_REG_PLAYCFG2 0x07
#define AW86224_REG_PLAYCFG3 0x08
#define AW86224_REG_PLAYCFG4 0x09
#define AW86224_REG_WAVCFG1 0x0A
#define AW86224_REG_WAVCFG2 0x0B
#define AW86224_REG_WAVCFG3 0x0C
#define AW86224_REG_WAVCFG4 0x0D
#define AW86224_REG_WAVCFG5 0x0E
#define AW86224_REG_WAVCFG6 0x0F
#define AW86224_REG_WAVCFG7 0x10
#define AW86224_REG_WAVCFG8 0x11
#define AW86224_REG_WAVCFG9 0x12
#define AW86224_REG_WAVCFG10 0x13
#define AW86224_REG_WAVCFG11 0x14
#define AW86224_REG_WAVCFG12 0x15
#define AW86224_REG_WAVCFG13 0x16
#define AW86224_REG_CONTCFG1 0x18
#define AW86224_REG_CONTCFG2 0x19
#define AW86224_REG_CONTCFG3 0x1A
#define AW86224_REG_CONTCFG5 0x1C
#define AW86224_REG_CONTCFG6 0x1D
#define AW86224_REG_CONTCFG7 0x1E
#define AW86224_REG_CONTCFG8 0x1F
#define AW86224_REG_CONTCFG9 0x20
#define AW86224_REG_CONTCFG10 0x21
#define AW86224_REG_CONTCFG11 0x22
#define AW86224_REG_CONTRD14 0x25
#define AW86224_REG_CONTRD15 0x26
#define AW86224_REG_CONTRD16 0x27
#define AW86224_REG_CONTRD17 0x28
#define AW86224_REG_RTPCFG1 0x2D
#define AW86224_REG_RTPCFG2 0x2E
#define AW86224_REG_RTPCFG3 0x2F
#define AW86224_REG_RTPCFG4 0x30
#define AW86224_REG_RTPCFG5 0x31
#define AW86224_REG_RTPDATA 0x32
#define AW86224_REG_TRGCFG1 0x33
#define AW86224_REG_TRGCFG4 0x36
#define AW86224_REG_TRGCFG7 0x39
#define AW86224_REG_TRGCFG8 0x3A
#define AW86224_REG_GLBCFG2 0x3C
#define AW86224_REG_GLBCFG4 0x3E
#define AW86224_REG_GLBRD5 0x3F
#define AW86224_REG_RAMADDRH 0x40
#define AW86224_REG_RAMADDRL 0x41
#define AW86224_REG_RAMDATA 0x42
#define AW86224_REG_SYSCRTL1 0x43
#define AW86224_REG_SYSCRTL2 0x44
#define AW86224_REG_SYSCRTL7 0x49
#define AW86224_REG_PWMCFG1 0x4C
#define AW86224_REG_PWMCFG2 0x4D
#define AW86224_REG_PWMCFG3 0x4E
#define AW86224_REG_PWMCFG4 0x4F
#define AW86224_REG_DETCFG1 0x51
#define AW86224_REG_DETCFG2 0x52
#define AW86224_REG_DET_RL 0x53
#define AW86224_REG_DET_VBAT 0x55
#define AW86224_REG_DET_LO 0x57
#define AW86224_REG_TRIMCFG3 0x5A
#define AW86224_REG_CHIPID 0x64
#define AW86224_REG_ANACFG8 0x77

/* AW86224 Chip ID definitions */
#define AW86224_CHIPID_H_MASK 0x40
#define AW86224_CHIPID_L_MASK 0x01
#define AW86224_CHIPID_EXPECTED 0x00 /* AW86224: bit6=0, bit0=0 */

/* Play modes */
#define AW86224_PLAY_MODE_RAM 0
#define AW86224_PLAY_MODE_RTP 1
#define AW86224_PLAY_MODE_CONT 2

/* GLB_STATE values */
#define AW86224_STATE_STANDBY 0x00
#define AW86224_STATE_CONT 0x06
#define AW86224_STATE_RAM 0x07
#define AW86224_STATE_RTP 0x08
#define AW86224_STATE_TRIG 0x09
#define AW86224_STATE_BRAKE 0x0B

/* Waveform sample rate */
#define AW86224_SRATE_12K 2
#define AW86224_SRATE_24K 0
#define AW86224_SRATE_48K 1

    /* Public API functions */

    /**
     * @brief Initialize AW86224 device
     *
     * @param i2c_bus_name I2C bus name (e.g., "i2c1")
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_init();

    /**
     * @brief Deinitialize AW86224 device
     *
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_deinit(void);

    /**
     * @brief Load waveform data to RAM
     *
     * @param wave_data Waveform data array
     * @param len Length of waveform data
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_load_waveform(rt_uint8_t *wave_data, rt_uint16_t len);

    /**
     * @brief Set playback gain
     *
     * @param gain Gain value (0-255, 128 = 100%)
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_set_gain(rt_uint8_t gain);

    /**
     * @brief Play waveform in RAM mode
     *
     * @param wave_id Waveform ID (1-127)
     * @param loop Loop count (0-14: play N+1 times, 15: infinite loop)
     * @param auto_brake Enable auto brake after playback
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_play_ram(rt_uint8_t wave_id, rt_uint8_t loop,
                              rt_bool_t auto_brake);

    /**
     * @brief Play waveform in RTP mode (Real-Time Playback)
     *
     * @param data RTP data buffer
     * @param len Data length
     * @param auto_brake Enable auto brake after playback
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_play_rtp(rt_uint8_t *data, rt_uint16_t len,
                              rt_bool_t auto_brake);

    /**
     * @brief Play CONT mode (continuous vibration)
     *
     * @param f0_target Target F0 frequency in Hz*10 (e.g., 1700 for 170Hz)
     * @param duration_ms Duration in milliseconds
     * @param strength Strength (0-127)
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_play_cont(rt_uint32_t f0_target, rt_uint32_t duration_ms,
                               rt_uint8_t strength);

    /**
     * @brief Stop current playback
     *
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_stop_playback(void);

    /**
     * @brief Detect LRA F0 frequency
     *
     * @param f0 Output F0 frequency in Hz*10
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_f0_detect(rt_uint32_t *f0);

    /**
     * @brief Calibrate LRA F0 and save to chip
     *
     * @param f0_pre Expected F0 frequency in Hz*10
     * @param f0_measured Measured F0 frequency in Hz*10
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_f0_calibrate(rt_uint32_t f0_pre, rt_uint32_t f0_measured);

    /**
     * @brief Measure LRA resistance
     *
     * @param resistance Output resistance in mΩ
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_measure_resistance(rt_uint32_t *resistance);

    /**
     * @brief Measure battery voltage
     *
     * @param voltage Output voltage in mV
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_measure_vbat(rt_uint32_t *voltage);

    /**
     * @brief Get device state
     *
     * @return Current state (0=STANDBY, 6=CONT, 7=RAM, 8=RTP, 9=TRIG, 11=BRAKE)
     */
    rt_uint8_t aw86224_get_state(void);

    /**
     * @brief Check if device is playing
     *
     * @return RT_TRUE if playing, RT_FALSE otherwise
     */
    rt_bool_t aw86224_is_playing(void);

    /**
     * @brief Wait for playback to complete
     *
     * @param timeout_ms Timeout in milliseconds
     * @return RT_EOK on success, -RT_ETIMEOUT on timeout
     */
    rt_err_t aw86224_wait_complete(rt_uint32_t timeout_ms);

    /**
     * @brief Play RAM waveform once (short vibration)
     *
     * @param wave_id Waveform ID (1-127)
     * @param auto_brake Enable auto brake after playback
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_play_ram_short(rt_uint8_t wave_id, rt_bool_t auto_brake);

    /**
     * @brief Play RAM waveform continuously (long vibration)
     *
     * @param wave_id Waveform ID (1-127)
     * @param loop Loop count: 0 for infinite loop, 1-14 for specified times
     * @param auto_brake Enable auto brake after playback
     * @return RT_EOK on success, negative error code on failure
     */
    rt_err_t aw86224_play_ram_long(rt_uint8_t wave_id, rt_uint8_t loop,
                                   rt_bool_t auto_brake);

#ifdef __cplusplus
}
#endif

#endif /* __AW86224_H__ */