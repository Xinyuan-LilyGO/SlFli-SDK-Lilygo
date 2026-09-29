#ifndef __DRV2605_H__
#define __DRV2605_H__

#include <rtdevice.h>
#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRV2605_I2C_ADDR                  0x5AU
#define DRV2605_REG_STATUS                0x00U
#define DRV2605_REG_MODE                  0x01U
#define DRV2605_REG_RTP_INPUT             0x02U
#define DRV2605_REG_LIBRARY               0x03U
#define DRV2605_REG_WAVESEQ1              0x04U
#define DRV2605_REG_WAVESEQ2              0x05U
#define DRV2605_REG_OVERDRIVE             0x0DU
#define DRV2605_REG_SUSTAIN_POS           0x0EU
#define DRV2605_REG_SUSTAIN_NEG           0x0FU
#define DRV2605_REG_BRAKE                 0x10U
#define DRV2605_REG_AUDIO_MAX_INPUT       0x13U
#define DRV2605_REG_FEEDBACK              0x1AU
#define DRV2605_REG_CONTROL3              0x1DU
#define DRV2605_STATUS_DEVICE_ID_MASK     0xE0U
#define DRV2605_STATUS_DEVICE_ID_SHIFT    5U
#define DRV2605_MODE_DEV_RESET            0x80U
#define DRV2605_MODE_STANDBY              0x40U
#define DRV2605_MODE_INTERNAL_TRIGGER     0x00U
#define DRV2605_FEEDBACK_N_ERM_LRA        0x80U
#define DRV2605_CONTROL3_ERM_OPEN_LOOP    0x20U
#define DRV2605_DEVICE_ID                 3U
#define DRV2605_DEVICE_ID_LOW_VOLTAGE     7U

#ifndef DRV2605_I2C_BUS_NAME
#define DRV2605_I2C_BUS_NAME              "i2c1"
#endif

#ifndef DRV2605_I2C_MAX_HZ
#define DRV2605_I2C_MAX_HZ                400000U
#endif

#ifndef DRV2605_ENABLE_PIN
/* XL9555 P13 maps to linear pin index 11. */
#define DRV2605_ENABLE_PIN                11U
#endif

#ifndef DRV2605_LIBRARY
#define DRV2605_LIBRARY                   1U
#endif

rt_err_t drv2605_init(void);
rt_err_t drv2605_deinit(void);
rt_err_t drv2605_read_reg(rt_uint8_t reg, rt_uint8_t *value);
rt_err_t drv2605_write_reg(rt_uint8_t reg, rt_uint8_t value);
rt_err_t drv2605_read_id(rt_uint8_t *device_id);
rt_err_t drv2605_diagnose(void);

#ifdef __cplusplus
}
#endif

#endif /* __DRV2605_H__ */
