/*
 * Copyright (c) 2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-12-18     YourName     First version for SGM41562B charger driver
 */

#ifndef __SGM41562B_H__
#define __SGM41562B_H__

#include <rtdevice.h>
#include <rtthread.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* I2C slave address (7-bit) */
#define SGM41562B_I2C_ADDR 0x03

/* Register Map */
#define SGM41562B_REG_INPUT_SOURCE 0x00   /* Input source control */
#define SGM41562B_REG_POWER_ON_CFG 0x01   /* Power-on configuration */
#define SGM41562B_REG_CHARGE_CURRENT 0x02 /* Charge current control */
#define SGM41562B_REG_DISCHARGE_TERM                                           \
    0x03                                   /* Discharge / termination current  \
                                            */
#define SGM41562B_REG_CHARGE_VOLTAGE 0x04  /* Charge voltage control */
#define SGM41562B_REG_TERM_TIMER_CTRL 0x05 /* Termination / timer control */
#define SGM41562B_REG_MISC_CTRL 0x06       /* Miscellaneous operation control */
#define SGM41562B_REG_SYS_VOLTAGE 0x07     /* System voltage regulation */
#define SGM41562B_REG_SYSTEM_STATUS 0x08   /* System status (read) */
#define SGM41562B_REG_FAULT 0x09           /* Fault register (read) */
#define SGM41562B_REG_IIC_MISC_CFG                                             \
    0x0A                             /* I2C address and misc configuration     \
                                      */
#define SGM41562B_REG_DEVICE_ID 0x0B /* Device ID */

/* Device ID values */
#define SGM41562B_DEVICE_ID 0x00
#define SGM41562A_DEVICE_ID 0x02

/* --------------------------------------------------------------------------
 * REG00: Input Source Control Register (R/W)
 * -------------------------------------------------------------------------- */
#define SGM41562B_VIN_MIN_SHIFT 4
#define SGM41562B_VIN_MIN_MASK (0x0F << SGM41562B_VIN_MIN_SHIFT)
#define SGM41562B_IIN_LIM_SHIFT 0
#define SGM41562B_IIN_LIM_MASK (0x0F << SGM41562B_IIN_LIM_SHIFT)

/* REG01: Power-On Configuration Register (R/W) */
#define SGM41562B_TRST_DGL_SHIFT 6
#define SGM41562B_TRST_DGL_MASK (0x03 << SGM41562B_TRST_DGL_SHIFT)
#define SGM41562B_TRST_DUR (1 << 5)
#define SGM41562B_EN_HIZ (1 << 4)
#define SGM41562B_CEB (1 << 3)
#define SGM41562B_VBAT_UVLO_SHIFT 0
#define SGM41562B_VBAT_UVLO_MASK (0x07 << SGM41562B_VBAT_UVLO_SHIFT)

/* REG02: Charge Current Control Register (R/W) */
#define SGM41562B_REG_RST (1 << 7)
#define SGM41562B_WD_RST (1 << 6)
#define SGM41562B_ICC_SHIFT 0
#define SGM41562B_ICC_MASK (0x3F << SGM41562B_ICC_SHIFT)

/* REG03: Discharge/Termination Current Register (R/W) */
#define SGM41562B_IDSCHG_SHIFT 4
#define SGM41562B_IDSCHG_MASK (0x0F << SGM41562B_IDSCHG_SHIFT)
#define SGM41562B_ITERM_SHIFT 0
#define SGM41562B_ITERM_MASK (0x0F << SGM41562B_ITERM_SHIFT)

/* REG04: Charge Voltage Control Register (R/W) */
#define SGM41562B_VBAT_REG_SHIFT 2
#define SGM41562B_VBAT_REG_MASK (0x3F << SGM41562B_VBAT_REG_SHIFT)
#define SGM41562B_VBAT_PRE (1 << 1)
#define SGM41562B_VRECH (1 << 0)

/* REG05: Termination/Timer Control Register (R/W) */
#define SGM41562B_EN_WD_DISCHG (1 << 7)
#define SGM41562B_WATCHDOG_SHIFT 5
#define SGM41562B_WATCHDOG_MASK (0x03 << SGM41562B_WATCHDOG_SHIFT)
#define SGM41562B_EN_TERM (1 << 4)
#define SGM41562B_EN_TIMER (1 << 3)
#define SGM41562B_CHG_TMR_SHIFT 1
#define SGM41562B_CHG_TMR_MASK (0x03 << SGM41562B_CHG_TMR_SHIFT)
#define SGM41562B_TERM_TMR (1 << 0)

/* REG06: Miscellaneous Operation Control Register (R/W) */
#define SGM41562B_EN_NTC (1 << 7)
#define SGM41562B_TMR2X_EN (1 << 6)
#define SGM41562B_FET_DIS (1 << 5)
#define SGM41562B_PG_INT_CTL (1 << 4)
#define SGM41562B_EOC_INT_CTL (1 << 3)
#define SGM41562B_CHG_STATUS_INT_CTL (1 << 2)
#define SGM41562B_NTC_INT_CTL (1 << 1)
#define SGM41562B_BATOVP_INT_CTL (1 << 0)

/* REG07: System Voltage Regulation Register (R/W) */
#define SGM41562B_EN_PCB_OTP (1 << 7)
#define SGM41562B_EN_VINLOOP (1 << 6)
#define SGM41562B_TJ_REG_SHIFT 4
#define SGM41562B_TJ_REG_MASK (0x03 << SGM41562B_TJ_REG_SHIFT)
#define SGM41562B_VSYS_REG_SHIFT 0
#define SGM41562B_VSYS_REG_MASK (0x0F << SGM41562B_VSYS_REG_SHIFT)

/* REG08: System Status Register (Read only) */
#define SGM41562B_WTD_FAULT (1 << 7)
#define SGM41562B_IIN_LIM_REL (1 << 6)
#define SGM41562B_IIN_LIM_ADD200 (1 << 5)
#define SGM41562B_CHG_STAT_SHIFT 3
#define SGM41562B_CHG_STAT_MASK (0x03 << SGM41562B_CHG_STAT_SHIFT)
#define SGM41562B_PPM_STAT (1 << 2)
#define SGM41562B_PG_STAT (1 << 1)
#define SGM41562B_THERM_STAT (1 << 0)

/* REG09: Fault Register (Read only) */
#define SGM41562B_EN_SHIP_DGL_SHIFT 6
#define SGM41562B_EN_SHIP_DGL_MASK (0x03 << SGM41562B_EN_SHIP_DGL_SHIFT)
#define SGM41562B_VIN_FAULT (1 << 5)
#define SGM41562B_THEM_SD (1 << 4)
#define SGM41562B_BAT_FAULT (1 << 3)
#define SGM41562B_STMR_FAULT (1 << 2)
#define SGM41562B_NTC_FAULT_HOT (1 << 1)
#define SGM41562B_NTC_FAULT_COLD (1 << 0)

/* REG0A: I2C Address and Miscellaneous Configuration Register (R/W) */
#define SGM41562B_ADDR_SHIFT 5
#define SGM41562B_ADDR_MASK (0x07 << SGM41562B_ADDR_SHIFT)
#define SGM41562B_COLD_RESET (1 << 4)
#define SGM41562B_SWITCH_MODE (1 << 3)
#define SGM41562B_DIS_VDD (1 << 2)
#define SGM41562B_DIS_VINOVP (1 << 1)
#define SGM41562B_CC_FINE (1 << 0)

/* Charging status values (CHG_STAT bits) */
#define CHG_STAT_NOT_CHARGING 0
#define CHG_STAT_PRE_CHARGE 1
#define CHG_STAT_FAST_CHARGE 2
#define CHG_STAT_CHARGE_DONE 3

/* Watchdog timer options */
#define WATCHDOG_DISABLE 0
#define WATCHDOG_40S 1
#define WATCHDOG_80S 2
#define WATCHDOG_160S 3

/* Charge timer options */
#define CHARGE_TIMER_3HRS 0
#define CHARGE_TIMER_5HRS 1
#define CHARGE_TIMER_8HRS 2
#define CHARGE_TIMER_12HRS 3

/* Thermal regulation options */
#define THERMAL_REG_60C 0
#define THERMAL_REG_80C 1
#define THERMAL_REG_100C 2
#define THERMAL_REG_120C 3

/* tRST_DGL options */
#define TRST_DGL_8S 0
#define TRST_DGL_12S 1
#define TRST_DGL_16S 2
#define TRST_DGL_20S 3

/* Shipping mode delay options */
#define SHIP_DGL_1S 0
#define SHIP_DGL_2S 1
#define SHIP_DGL_4S 2
#define SHIP_DGL_8S 3

    /* Driver device structure */
    struct sgm41562b_device
    {
        struct rt_i2c_bus_device *i2c_bus;
        rt_uint8_t i2c_addr;
        struct rt_mutex lock;
        rt_base_t irq_pin; /* Interrupt pin number (optional) */
        rt_bool_t irq_enabled;

        /* Cached status for interrupt handling */
        rt_uint8_t system_status;
        rt_uint8_t fault_status;

        /* Callback for status change notification */
        void (*status_callback)(rt_uint8_t status_reg, rt_uint8_t fault_reg,
                                void *user_data);
        void *user_data;
        struct rt_semaphore irq_sem; // 新增，用于中断唤醒线程
        rt_thread_t monitor_thread;  // 监控线程句柄
    };

    typedef struct sgm41562b_device *sgm41562b_handle_t;

    /* Public API functions */

    /**
     * @brief Initialize SGM41562B driver with specified I2C bus name.
     * @param i2c_bus_name I2C bus device name (e.g., "i2c1")
     * @param irq_pin Interrupt pin number (optional, set to -1 if not used)
     * @return Handle to device, or RT_NULL on failure
     */
    sgm41562b_handle_t sgm41562b_init(const char *i2c_bus_name,
                                      rt_base_t irq_pin);

    /**
     * @brief Deinitialize driver and release resources.
     * @param handle Device handle
     * @return RT_EOK on success
     */
    rt_err_t sgm41562b_deinit(sgm41562b_handle_t handle);

    /**
     * @brief Read a single register.
     * @param handle Device handle
     * @param reg Register address
     * @param value Pointer to store read value
     * @return RT_EOK on success, error code otherwise
     */
    rt_err_t sgm41562b_read_reg(sgm41562b_handle_t handle, rt_uint8_t reg,
                                rt_uint8_t *value);

    /**
     * @brief Write a single register.
     * @param handle Device handle
     * @param reg Register address
     * @param value Value to write
     * @return RT_EOK on success
     */
    rt_err_t sgm41562b_write_reg(sgm41562b_handle_t handle, rt_uint8_t reg,
                                 rt_uint8_t value);

    /**
     * @brief Update specific bits in a register (read-modify-write).
     * @param handle Device handle
     * @param reg Register address
     * @param mask Bit mask of bits to modify
     * @param value New value (already shifted to correct position)
     * @return RT_EOK on success
     */
    rt_err_t sgm41562b_update_reg(sgm41562b_handle_t handle, rt_uint8_t reg,
                                  rt_uint8_t mask, rt_uint8_t value);

    /**
     * @brief Read device ID.
     * @param handle Device handle
     * @param id Pointer to store ID
     * @return RT_EOK on success
     */
    rt_err_t sgm41562b_get_device_id(sgm41562b_handle_t handle, rt_uint8_t *id);

    /**
     * @brief Software reset (REG_RST). Resets most registers to default.
     * @param handle Device handle
     * @return RT_EOK
     */
    rt_err_t sgm41562b_software_reset(sgm41562b_handle_t handle);

    /**
     * @brief Reset watchdog timer (feed the dog).
     * @param handle Device handle
     * @return RT_EOK
     */
    rt_err_t sgm41562b_watchdog_reset(sgm41562b_handle_t handle);

    rt_err_t sgm41562b_set_nint_rst_time(sgm41562b_handle_t handle, rt_uint8_t value);

    /**
     * @brief Enable or disable charging.
     * @param handle Device handle
     * @param enable RT_TRUE to enable charging, RT_FALSE to disable
     * @return RT_EOK
     */
    rt_err_t sgm41562b_enable_charging(sgm41562b_handle_t handle,
                                       rt_bool_t enable);

    /**
     * @brief Enable or disable high-impedance mode (input power path off).
     * @param handle Device handle
     * @param enable RT_TRUE to enable HIZ (disconnect input)
     * @return RT_EOK
     */
    rt_err_t sgm41562b_set_hiz_mode(sgm41562b_handle_t handle,
                                    rt_bool_t enable);

    /**
     * @brief Enter shipping mode (disconnect battery).
     * @param handle Device handle
     * @return RT_EOK
     */
    rt_err_t sgm41562b_enter_shipping_mode(sgm41562b_handle_t handle);

    /**
     * @brief Exit shipping mode (call when power applied or nINT pulled low).
     * @note This is automatically handled by hardware, function provided for
     * consistency.
     * @param handle Device handle
     * @return RT_EOK
     */
    rt_err_t sgm41562b_exit_shipping_mode(sgm41562b_handle_t handle);

    /**
     * @brief Force system power recycle (turn off/on SYS).
     * @param handle Device handle
     * @return RT_EOK
     */
    rt_err_t sgm41562b_power_recycle(sgm41562b_handle_t handle);

    /* Configuration Setters */

    rt_err_t sgm41562b_set_input_voltage_limit(sgm41562b_handle_t handle,
                                               rt_uint16_t voltage_mv);
    rt_err_t sgm41562b_set_input_current_limit(sgm41562b_handle_t handle,
                                               rt_uint16_t current_ma);
    rt_err_t sgm41562b_set_charge_voltage(sgm41562b_handle_t handle,
                                          rt_uint16_t voltage_mv);
    rt_err_t sgm41562b_set_charge_current(sgm41562b_handle_t handle,
                                          rt_uint16_t current_ma);
    rt_err_t sgm41562b_set_precharge_term_current(sgm41562b_handle_t handle,
                                                  rt_uint8_t current_ma);
    rt_err_t sgm41562b_set_discharge_current_limit(sgm41562b_handle_t handle,
                                                   rt_uint16_t current_ma);
    rt_err_t sgm41562b_set_system_voltage(sgm41562b_handle_t handle,
                                          rt_uint16_t voltage_mv);
    rt_err_t sgm41562b_set_battery_uvlo(sgm41562b_handle_t handle,
                                        rt_uint16_t voltage_mv);
    rt_err_t
    sgm41562b_set_thermal_regulation_threshold(sgm41562b_handle_t handle,
                                               rt_uint8_t threshold);
    rt_err_t sgm41562b_set_watchdog_timer(sgm41562b_handle_t handle,
                                          rt_uint8_t option);
    rt_err_t sgm41562b_set_charge_safety_timer(sgm41562b_handle_t handle,
                                               rt_bool_t enable,
                                               rt_uint8_t hours_option);
    rt_err_t sgm41562b_enable_ntc(sgm41562b_handle_t handle, rt_bool_t enable);
    rt_err_t sgm41562b_enable_pcb_otp(sgm41562b_handle_t handle,
                                      rt_bool_t enable);
    rt_err_t
    sgm41562b_set_recharge_threshold(sgm41562b_handle_t handle,
                                     rt_bool_t high); /* 0:100mV, 1:200mV */
    rt_err_t
    sgm41562b_set_precharge_threshold(sgm41562b_handle_t handle,
                                      rt_bool_t high); /* 0:2.8V, 1:3.0V */
    rt_err_t
    sgm41562b_set_charge_fine_scale(sgm41562b_handle_t handle,
                                    rt_bool_t enable); /* Divide ICC by 4 */

    /* Status Getters */

    rt_err_t sgm41562b_get_system_status(sgm41562b_handle_t handle,
                                         rt_uint8_t *status);
    rt_err_t sgm41562b_get_fault_status(sgm41562b_handle_t handle,
                                        rt_uint8_t *fault);
    rt_uint8_t sgm41562b_get_charge_status(sgm41562b_handle_t handle);
    rt_bool_t sgm41562b_is_power_good(sgm41562b_handle_t handle);
    rt_bool_t sgm41562b_is_in_thermal_regulation(sgm41562b_handle_t handle);
    rt_bool_t sgm41562b_is_watchdog_fault(sgm41562b_handle_t handle);

    /* 状态字符串获取函数 */
    const char *sgm41562b_get_charge_status_str(sgm41562b_handle_t handle);
    const char *sgm41562b_get_power_good_str(sgm41562b_handle_t handle);
    const char *sgm41562b_get_thermal_regulation_str(sgm41562b_handle_t handle);
    const char *sgm41562b_get_watchdog_fault_str(sgm41562b_handle_t handle);

    /* 故障状态字符串（一次获取所有故障描述，或单独查询） */
    const char *sgm41562b_get_vin_fault_str(sgm41562b_handle_t handle);
    const char *sgm41562b_get_thermal_shutdown_str(sgm41562b_handle_t handle);
    const char *sgm41562b_get_battery_fault_str(sgm41562b_handle_t handle);
    const char *sgm41562b_get_safety_timer_fault_str(sgm41562b_handle_t handle);
    const char *sgm41562b_get_ntc_fault_str(sgm41562b_handle_t handle);
    rt_int16_t sgm41562b_get_charge_vlotage(sgm41562b_handle_t handle);
    rt_int16_t sgm41562b_get_charge_current(sgm41562b_handle_t handle);
    sgm41562b_handle_t sgm41562b_get_handle(void);
    rt_err_t device_enter_shipping_mode(sgm41562b_handle_t handle);

    /* Interrupt callback registration */
    void sgm41562b_attach_status_callback(
        sgm41562b_handle_t handle,
        void (*callback)(rt_uint8_t status_reg, rt_uint8_t fault_reg,
                         void *user_data),
        void *user_data);

    /* Interrupt service routine (to be called from GPIO ISR) */
    void sgm41562b_isr_handler(sgm41562b_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* __SGM41562B_H__ */