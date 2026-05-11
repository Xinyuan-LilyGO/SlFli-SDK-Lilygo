#ifndef __BQ21080_H__
#define __BQ21080_H__

#include "rtthread.h"
#include "rtdevice.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BQ21080_DEVICE_ADDRESS 0x6A    

/* 寄存器地址定义 */
#define BQ21080_REG_STAT0        0x00
#define BQ21080_REG_STAT1        0x01
#define BQ21080_REG_FLAG0        0x02
#define BQ21080_REG_VBAT_CTRL    0x03
#define BQ21080_REG_ICHG_CTRL    0x04
#define BQ21080_REG_CHARGECTRL0  0x05
#define BQ21080_REG_CHARGECTRL1  0x06
#define BQ21080_REG_IC_CTRL      0x07
#define BQ21080_REG_TMR_ILIM     0x08
#define BQ21080_REG_SHIP_RST     0x09
#define BQ21080_REG_SYS_REG      0x0A
#define BQ21080_REG_TS_CONTROL   0x0B
#define BQ21080_REG_MASK_ID      0x0C

/* 充电状态 */
typedef enum {
    CHG_STATUS_NOT_CHARGING = 0,
    CHG_STATUS_CONSTANT_CURRENT,
    CHG_STATUS_CONSTANT_VOLTAGE,
    CHG_STATUS_CHARGE_DONE
} chg_status_t;

/* TS状态枚举 */
typedef enum {
    TS_STATUS_NORMAL = 0,
    TS_STATUS_SUSPEND,      // VTS < VHOT 或 VTS > VCOLD（充电暂停）
    TS_STATUS_COOL,         // VCOOL < VTS < VCOLD（充电电流减小）
    TS_STATUS_WARM          // VWARM > VTS > VHOT（充电电压减小）
} ts_status_t;

/* 设备结构体 */
typedef struct {
    rt_device_t i2c_dev;
    struct rt_i2c_bus_device *bq21080_i2c_bus;
    rt_mutex_t lock;
    rt_bool_t initialized;
} bq21080_dev_t;

/* 状态信息结构体 */
typedef struct {
    rt_bool_t ts_open;
    chg_status_t charge_status;
    rt_bool_t vin_power_good;
    rt_bool_t thermal_reg_active;
    rt_bool_t vindpm_active;
    rt_bool_t vdppm_active;
    rt_bool_t ilim_active;

    rt_bool_t safety_timer_fault;

    rt_bool_t bat_ocp_fault;
    rt_bool_t buvlo_fault;
    rt_bool_t vin_ovp_fault;

    rt_uint8_t ts_status;           // 温度传感器状态
} bq21080_status_t;

extern char *charge_status_str[];
extern char *ts_status_str[];

/* 函数声明 */
rt_err_t bq21080_init(void);
rt_err_t bq21080_set_charge_voltage(rt_uint16_t voltage_mv);
rt_err_t bq21080_set_charge_current(rt_uint16_t current_ma);
rt_err_t bq21080_set_input_current_limit(rt_uint16_t current_ma);
rt_err_t bq21080_get_status(bq21080_status_t *status);
rt_err_t bq21080_enable_charging(rt_bool_t enable);
rt_err_t bq21080_enter_ship_mode(void);
rt_err_t bq21080_hardware_reset(void);
rt_err_t bq21080_software_reset(void);
rt_err_t bq21080_set_termination_percent(rt_uint8_t percent);
rt_err_t bq21080_set_precharge_ratio(rt_bool_t double_term);
rt_err_t bq21080_set_precharge_voltage_threshold(rt_uint16_t threshold_mv);
uint16_t bq21080_get_charge_voltage(void);
uint16_t bq21080_get_charge_current(void);

#ifdef __cplusplus
}
#endif

#endif