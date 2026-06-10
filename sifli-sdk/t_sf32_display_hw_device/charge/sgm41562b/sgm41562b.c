/*
 * Copyright (c) 2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-12-18     YourName     First version for SGM41562B charger driver
 */

#include "sgm41562b.h"

#define DBG_ENABLE
#define DBG_SECTION_NAME "SGM41562B"
#define DBG_LEVEL DBG_LOG
#include <rtdbg.h>

/* Default I2C timeout (ms) */
#define I2C_TIMEOUT_MS 100

/* Internal helper macros */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Private function prototypes */
static rt_err_t read_reg(sgm41562b_handle_t handle, rt_uint8_t reg,
                         rt_uint8_t *value);
static rt_err_t write_reg(sgm41562b_handle_t handle, rt_uint8_t reg,
                          rt_uint8_t value);
static rt_err_t update_reg(sgm41562b_handle_t handle, rt_uint8_t reg,
                           rt_uint8_t mask, rt_uint8_t value);
static void default_config(sgm41562b_handle_t handle);
static void charger_monitor_thread_entry(void *parameter);

static sgm41562b_handle_t _handle;
static rt_int16_t _charge_voltage = 4200;
static rt_int16_t _charge_current = 200;

/* --------------------------------------------------------------------------
 * I2C low-level operations
 * -------------------------------------------------------------------------- */
static rt_err_t read_reg(sgm41562b_handle_t handle, rt_uint8_t reg,
                         rt_uint8_t *value)
{
    struct rt_i2c_msg msgs[2];

    RT_ASSERT(handle != RT_NULL);
    RT_ASSERT(value != RT_NULL);

    msgs[0].addr = handle->i2c_addr;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = &reg;
    msgs[0].len = 1;

    msgs[1].addr = handle->i2c_addr;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = value;
    msgs[1].len = 1;

    if (rt_i2c_transfer(handle->i2c_bus, msgs, 2) == 2)
    {
        return RT_EOK;
    }
    else
    {
        LOG_E("I2C read reg 0x%02X failed", reg);
        return -RT_ERROR;
    }
}

static rt_err_t write_reg(sgm41562b_handle_t handle, rt_uint8_t reg,
                          rt_uint8_t value)
{
    struct rt_i2c_msg msgs[1];
    rt_uint8_t buf[2];

    RT_ASSERT(handle != RT_NULL);

    buf[0] = reg;
    buf[1] = value;

    msgs[0].addr = handle->i2c_addr;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = buf;
    msgs[0].len = 2;

    if (rt_i2c_transfer(handle->i2c_bus, msgs, 1) == 1)
    {
        return RT_EOK;
    }
    else
    {
        LOG_E("I2C write reg 0x%02X failed", reg);
        return -RT_ERROR;
    }
}

static rt_err_t update_reg(sgm41562b_handle_t handle, rt_uint8_t reg,
                           rt_uint8_t mask, rt_uint8_t value)
{
    rt_uint8_t temp;
    rt_err_t ret;

    ret = read_reg(handle, reg, &temp);
    if (ret != RT_EOK)
        return ret;

    temp = (temp & ~mask) | (value & mask);

    return write_reg(handle, reg, temp);
}

/* --------------------------------------------------------------------------
 * Public API implementations
 * -------------------------------------------------------------------------- */
sgm41562b_handle_t sgm41562b_init(const char *i2c_bus_name, rt_base_t irq_pin)
{
    rt_uint8_t device_id;

    _handle = rt_calloc(1, sizeof(struct sgm41562b_device));
    if (_handle == RT_NULL)
    {
        LOG_E("Failed to allocate memory for device handle");
        return RT_NULL;
    }

    /* Find I2C bus */
    _handle->i2c_bus = rt_i2c_bus_device_find(i2c_bus_name);
    if (_handle->i2c_bus == RT_NULL)
    {
        LOG_E("I2C bus '%s' not found", i2c_bus_name);
        rt_free(_handle);
        return RT_NULL;
    }

    _handle->i2c_addr = SGM41562B_I2C_ADDR;
    _handle->irq_pin = irq_pin;
    _handle->irq_enabled = RT_FALSE;
    _handle->status_callback = RT_NULL;

    rt_sem_init(&_handle->irq_sem, "sgm_sem", 0, RT_IPC_FLAG_FIFO);
    rt_mutex_init(&_handle->lock, "sgm41562b", RT_IPC_FLAG_PRIO);

    /* Open I2C bus device (if not already opened) */
    if (rt_device_open((rt_device_t)_handle->i2c_bus, RT_DEVICE_OFLAG_RDWR) !=
        RT_EOK)
    {
        LOG_E("Failed to open I2C bus '%s'", i2c_bus_name);
        rt_mutex_detach(&_handle->lock);
        rt_free(_handle);
        return RT_NULL;
    }

    struct rt_i2c_configuration configuration = {
        .mode = 0,
        .addr = 0,
        .timeout = 500,   // ms
        .max_hz = 400000, // 400 kHz
    };
    rt_i2c_configure(_handle->i2c_bus, &configuration);

    /* Verify device ID */
    if (sgm41562b_get_device_id(_handle, &device_id) != RT_EOK)
    {
        LOG_E("Failed to read device ID");
        rt_device_close((rt_device_t)_handle->i2c_bus);
        rt_mutex_detach(&_handle->lock);
        rt_free(_handle);
        return RT_NULL;
    }

    LOG_I("SGM41562B device ID: 0x%02X", device_id);
    if (device_id != SGM41562B_DEVICE_ID && device_id != SGM41562A_DEVICE_ID)
    {
        LOG_W("Unexpected device ID (expected 0x00 or 0x02)");
    }

    /* Apply default configuration */
    default_config(_handle);

    /* Setup interrupt pin if provided */
    if (irq_pin >= 0)
    {
        rt_pin_mode(irq_pin, PIN_MODE_INPUT_PULLUP);
        rt_pin_attach_irq(irq_pin, PIN_IRQ_MODE_FALLING,
                          (void (*)(void *))sgm41562b_isr_handler, _handle);
        rt_pin_irq_enable(irq_pin, PIN_IRQ_ENABLE);
        _handle->irq_enabled = RT_TRUE;
    }
    
    _handle->monitor_thread = rt_thread_create(
        "sgm_mon", charger_monitor_thread_entry, _handle, 1024, RT_THREAD_PRIORITY_MIDDLE, 10);

    if (_handle->monitor_thread)
        rt_thread_startup(_handle->monitor_thread);

    return _handle;
}

rt_err_t sgm41562b_deinit(sgm41562b_handle_t handle)
{
    RT_ASSERT(handle != RT_NULL);

    if (handle->irq_enabled)
    {
        rt_pin_irq_enable(handle->irq_pin, PIN_IRQ_DISABLE);
        rt_pin_detach_irq(handle->irq_pin);
    }

    rt_device_close((rt_device_t)handle->i2c_bus);
    rt_mutex_detach(&handle->lock);
    rt_free(handle);

    return RT_EOK;
}

static void charger_monitor_thread_entry(void *parameter)
{
    sgm41562b_handle_t handle = (sgm41562b_handle_t)parameter;
    rt_uint8_t status, fault;

    while (1)
    {
        if (rt_sem_take(&handle->irq_sem, RT_WAITING_FOREVER) == RT_EOK)
        {
            /* 在线程上下文中安全读取状态 */
            sgm41562b_get_system_status(handle, &status);
            sgm41562b_get_fault_status(handle, &fault);

            /* 调用用户回调 */
            if (handle->status_callback)
            {
                handle->status_callback(status, fault, handle->user_data);
            }
        }
    }
}

rt_err_t sgm41562b_read_reg(sgm41562b_handle_t handle, rt_uint8_t reg,
                            rt_uint8_t *value)
{
    rt_err_t ret;

    RT_ASSERT(handle != RT_NULL);

    rt_mutex_take(&handle->lock, RT_WAITING_FOREVER);
    ret = read_reg(handle, reg, value);
    rt_mutex_release(&handle->lock);

    return ret;
}

rt_err_t sgm41562b_write_reg(sgm41562b_handle_t handle, rt_uint8_t reg,
                             rt_uint8_t value)
{
    rt_err_t ret;

    RT_ASSERT(handle != RT_NULL);

    rt_mutex_take(&handle->lock, RT_WAITING_FOREVER);
    ret = write_reg(handle, reg, value);
    rt_mutex_release(&handle->lock);

    return ret;
}

rt_err_t sgm41562b_update_reg(sgm41562b_handle_t handle, rt_uint8_t reg,
                              rt_uint8_t mask, rt_uint8_t value)
{
    rt_err_t ret;

    RT_ASSERT(handle != RT_NULL);

    rt_mutex_take(&handle->lock, RT_WAITING_FOREVER);
    ret = update_reg(handle, reg, mask, value);
    rt_mutex_release(&handle->lock);

    return ret;
}

rt_err_t sgm41562b_get_device_id(sgm41562b_handle_t handle, rt_uint8_t *id)
{
    return sgm41562b_read_reg(handle, SGM41562B_REG_DEVICE_ID, id);
}

rt_err_t sgm41562b_software_reset(sgm41562b_handle_t handle)
{
    rt_uint8_t reg_val;
    rt_err_t ret;
    ret = sgm41562b_read_reg(handle, SGM41562B_REG_CHARGE_CURRENT, &reg_val);
    if (ret != RT_EOK)
        return ret;

    reg_val |= SGM41562B_REG_RST; // 0x80

    ret = sgm41562b_write_reg(handle, SGM41562B_REG_CHARGE_CURRENT, reg_val);

    rt_thread_mdelay(5);

    return ret;
}

rt_err_t sgm41562b_watchdog_reset(sgm41562b_handle_t handle)
{
    rt_uint8_t val;
    rt_err_t ret;

    /* Set WD_RST bit (write 1 to reset) */
    ret = sgm41562b_read_reg(handle, SGM41562B_REG_CHARGE_CURRENT, &val);
    if (ret != RT_EOK)
        return ret;

    val |= SGM41562B_WD_RST;
    ret = sgm41562b_write_reg(handle, SGM41562B_REG_CHARGE_CURRENT, val);
    if (ret == RT_EOK)
    {
        /* Clear WD_RST after reset (bit is self-clearing according to
           datasheet, but we ensure it's cleared) */
        val &= ~SGM41562B_WD_RST;
        ret = sgm41562b_write_reg(handle, SGM41562B_REG_CHARGE_CURRENT, val);
    }
    return ret;
}

rt_err_t sgm41562b_set_nint_rst_time(sgm41562b_handle_t handle, rt_uint8_t value)
{
    return sgm41562b_update_reg(handle, SGM41562B_REG_POWER_ON_CFG,
                                SGM41562B_TRST_DGL_MASK, value << SGM41562B_TRST_DGL_SHIFT);
}

rt_err_t sgm41562b_enable_charging(sgm41562b_handle_t handle, rt_bool_t enable)
{
    /* CEB bit: 0 = enable charging, 1 = disable */
    rt_uint8_t val = enable ? 0 : SGM41562B_CEB;
    return sgm41562b_update_reg(handle, SGM41562B_REG_POWER_ON_CFG,
                                SGM41562B_CEB, val);
}

rt_err_t sgm41562b_set_hiz_mode(sgm41562b_handle_t handle, rt_bool_t enable)
{
    rt_uint8_t val = enable ? SGM41562B_EN_HIZ : 0;
    return sgm41562b_update_reg(handle, SGM41562B_REG_POWER_ON_CFG,
                                SGM41562B_EN_HIZ, val);
}

rt_err_t sgm41562b_enter_shipping_mode(sgm41562b_handle_t handle)
{
    rt_err_t ret;

    /* Set FET_DIS bit to 1 to enter shipping mode after delay */
    ret = sgm41562b_update_reg(handle, SGM41562B_REG_MISC_CTRL,
                               SGM41562B_FET_DIS, SGM41562B_FET_DIS);
    return ret;
}

rt_err_t sgm41562b_exit_shipping_mode(sgm41562b_handle_t handle)
{
    /* Exiting shipping mode is automatic when input power is applied
       or nINT pin pulled low. This function is a no-op. */
    return RT_EOK;
}

rt_err_t sgm41562b_power_recycle(sgm41562b_handle_t handle)
{
    /* Set COLD_RESET bit to 1 (self-clearing) */
    return sgm41562b_update_reg(handle, SGM41562B_REG_IIC_MISC_CFG,
                                SGM41562B_COLD_RESET, SGM41562B_COLD_RESET);
}

/* --------------------------------------------------------------------------
 * Configuration functions
 * -------------------------------------------------------------------------- */
rt_err_t sgm41562b_set_input_voltage_limit(sgm41562b_handle_t handle,
                                           rt_uint16_t voltage_mv)
{
    rt_uint8_t reg_val;

    if (voltage_mv < 3880 || voltage_mv > 5080)
        return -RT_EINVAL;

    /* VIN_MIN = 3.88V + n * 80mV, n = 0..15 */
    reg_val = (rt_uint8_t)((voltage_mv - 3880) / 80);
    return sgm41562b_update_reg(handle, SGM41562B_REG_INPUT_SOURCE,
                                SGM41562B_VIN_MIN_MASK,
                                reg_val << SGM41562B_VIN_MIN_SHIFT);
}

rt_err_t sgm41562b_set_input_current_limit(sgm41562b_handle_t handle,
                                           rt_uint16_t current_ma)
{
    rt_uint8_t reg_val;

    if (current_ma < 50 || current_ma > 500)
        return -RT_EINVAL;

    /* IIN_LIM = 50mA + n * 30mA, n = 0..15 */
    reg_val = (rt_uint8_t)((current_ma - 50) / 30);
    return sgm41562b_update_reg(handle, SGM41562B_REG_INPUT_SOURCE,
                                SGM41562B_IIN_LIM_MASK,
                                reg_val << SGM41562B_IIN_LIM_SHIFT);
}

rt_err_t sgm41562b_set_charge_voltage(sgm41562b_handle_t handle,
                                      rt_uint16_t voltage_mv)
{
    rt_uint8_t reg_val;

    if (voltage_mv < 3600 || voltage_mv > 4545)
        return -RT_EINVAL;

    _charge_voltage = voltage_mv;
    /* VBAT_REG = 3.60V + n * 15mV, n = 0..63 */
    reg_val = (rt_uint8_t)((voltage_mv - 3600) / 15);
    return sgm41562b_update_reg(handle, SGM41562B_REG_CHARGE_VOLTAGE,
                                SGM41562B_VBAT_REG_MASK,
                                reg_val << SGM41562B_VBAT_REG_SHIFT);
}

rt_err_t sgm41562b_set_charge_current(sgm41562b_handle_t handle,
                                      rt_uint16_t current_ma)
{
    rt_uint8_t reg_val;

    if (current_ma < 8 || current_ma > 456)
        return -RT_EINVAL;

    /* ICC = 8mA + n * 8mA, n = 0..56 (clamped) */
    reg_val = (rt_uint8_t)((current_ma - 8) / 8);
    if (reg_val > 56)
        reg_val = 56;
    _charge_current = current_ma;
    return sgm41562b_update_reg(handle, SGM41562B_REG_CHARGE_CURRENT,
                                SGM41562B_ICC_MASK,
                                reg_val << SGM41562B_ICC_SHIFT);
}

rt_err_t sgm41562b_set_precharge_term_current(sgm41562b_handle_t handle,
                                              rt_uint8_t current_ma)
{
    rt_uint8_t reg_val;

    if (current_ma < 1 || current_ma > 31)
        return -RT_EINVAL;

    /* ITERM = 1mA + n * 2mA, n = 0..15 */
    reg_val = (rt_uint8_t)((current_ma - 1) / 2);
    return sgm41562b_update_reg(handle, SGM41562B_REG_DISCHARGE_TERM,
                                SGM41562B_ITERM_MASK,
                                reg_val << SGM41562B_ITERM_SHIFT);
}

rt_err_t sgm41562b_set_discharge_current_limit(sgm41562b_handle_t handle,
                                               rt_uint16_t current_ma)
{
    rt_uint8_t reg_val;

    if (current_ma < 400 || current_ma > 3200)
        return -RT_EINVAL;

    /* IDSCHG = 200mA + n * 200mA, n=1..15 (n=0 is invalid) */
    reg_val = (rt_uint8_t)((current_ma - 400) / 200) + 1;
    if (reg_val > 15)
        reg_val = 15;

    return sgm41562b_update_reg(handle, SGM41562B_REG_DISCHARGE_TERM,
                                SGM41562B_IDSCHG_MASK,
                                reg_val << SGM41562B_IDSCHG_SHIFT);
}

rt_err_t sgm41562b_set_system_voltage(sgm41562b_handle_t handle,
                                      rt_uint16_t voltage_mv)
{
    rt_uint8_t reg_val;

    if (voltage_mv < 4200 || voltage_mv > 4950)
        return -RT_EINVAL;

    /* VSYS_REG = 4.20V + n * 50mV, n = 0..15 */
    reg_val = (rt_uint8_t)((voltage_mv - 4200) / 50);
    return sgm41562b_update_reg(handle, SGM41562B_REG_SYS_VOLTAGE,
                                SGM41562B_VSYS_REG_MASK,
                                reg_val << SGM41562B_VSYS_REG_SHIFT);
}

rt_err_t sgm41562b_set_battery_uvlo(sgm41562b_handle_t handle,
                                    rt_uint16_t voltage_mv)
{
    rt_uint8_t reg_val;

    if (voltage_mv < 2400 || voltage_mv > 3030)
        return -RT_EINVAL;

    /* VBAT_UVLO = 2.40V + n * 90mV, n = 0..7 */
    reg_val = (rt_uint8_t)((voltage_mv - 2400) / 90);
    return sgm41562b_update_reg(handle, SGM41562B_REG_POWER_ON_CFG,
                                SGM41562B_VBAT_UVLO_MASK,
                                reg_val << SGM41562B_VBAT_UVLO_SHIFT);
}

rt_err_t sgm41562b_set_thermal_regulation_threshold(sgm41562b_handle_t handle,
                                                    rt_uint8_t threshold)
{
    if (threshold > THERMAL_REG_120C)
        return -RT_EINVAL;

    return sgm41562b_update_reg(handle, SGM41562B_REG_SYS_VOLTAGE,
                                SGM41562B_TJ_REG_MASK,
                                threshold << SGM41562B_TJ_REG_SHIFT);
}

rt_err_t sgm41562b_set_watchdog_timer(sgm41562b_handle_t handle,
                                      rt_uint8_t option)
{
    if (option > WATCHDOG_160S)
        return -RT_EINVAL;

    return sgm41562b_update_reg(handle, SGM41562B_REG_TERM_TIMER_CTRL,
                                SGM41562B_WATCHDOG_MASK,
                                option << SGM41562B_WATCHDOG_SHIFT);
}

rt_err_t sgm41562b_set_charge_safety_timer(sgm41562b_handle_t handle,
                                           rt_bool_t enable,
                                           rt_uint8_t hours_option)
{
    rt_uint8_t mask = SGM41562B_EN_TIMER | SGM41562B_CHG_TMR_MASK;
    rt_uint8_t val;

    if (hours_option > CHARGE_TIMER_12HRS)
        return -RT_EINVAL;

    val = enable ? SGM41562B_EN_TIMER : 0;
    val |= (hours_option << SGM41562B_CHG_TMR_SHIFT) & SGM41562B_CHG_TMR_MASK;

    return sgm41562b_update_reg(handle, SGM41562B_REG_TERM_TIMER_CTRL, mask,
                                val);
}

rt_err_t sgm41562b_enable_ntc(sgm41562b_handle_t handle, rt_bool_t enable)
{
    rt_uint8_t val = enable ? SGM41562B_EN_NTC : 0;
    return sgm41562b_update_reg(handle, SGM41562B_REG_MISC_CTRL,
                                SGM41562B_EN_NTC, val);
}

rt_err_t sgm41562b_enable_pcb_otp(sgm41562b_handle_t handle, rt_bool_t enable)
{
    /* EN_PCB_OTP bit: 0 = enable, 1 = disable */
    rt_uint8_t val = enable ? 0 : SGM41562B_EN_PCB_OTP;
    return sgm41562b_update_reg(handle, SGM41562B_REG_SYS_VOLTAGE,
                                SGM41562B_EN_PCB_OTP, val);
}

rt_err_t sgm41562b_set_recharge_threshold(sgm41562b_handle_t handle,
                                          rt_bool_t high)
{
    rt_uint8_t val = high ? SGM41562B_VRECH : 0;
    return sgm41562b_update_reg(handle, SGM41562B_REG_CHARGE_VOLTAGE,
                                SGM41562B_VRECH, val);
}

rt_err_t sgm41562b_set_precharge_threshold(sgm41562b_handle_t handle,
                                           rt_bool_t high)
{
    rt_uint8_t val = high ? SGM41562B_VBAT_PRE : 0;
    return sgm41562b_update_reg(handle, SGM41562B_REG_CHARGE_VOLTAGE,
                                SGM41562B_VBAT_PRE, val);
}

rt_err_t sgm41562b_set_charge_fine_scale(sgm41562b_handle_t handle,
                                         rt_bool_t enable)
{
    rt_uint8_t val = enable ? SGM41562B_CC_FINE : 0;
    return sgm41562b_update_reg(handle, SGM41562B_REG_IIC_MISC_CFG,
                                SGM41562B_CC_FINE, val);
}

/* --------------------------------------------------------------------------
 * Status query functions
 * -------------------------------------------------------------------------- */
rt_err_t sgm41562b_get_system_status(sgm41562b_handle_t handle,
                                     rt_uint8_t *status)
{
    rt_err_t ret;

    ret = sgm41562b_read_reg(handle, SGM41562B_REG_SYSTEM_STATUS, status);
    if (ret == RT_EOK)
    {
        handle->system_status = *status;
    }
    return ret;
}

rt_err_t sgm41562b_get_fault_status(sgm41562b_handle_t handle,
                                    rt_uint8_t *fault)
{
    rt_err_t ret;

    ret = sgm41562b_read_reg(handle, SGM41562B_REG_FAULT, fault);
    if (ret == RT_EOK)
    {
        handle->fault_status = *fault;
    }
    return ret;
}

rt_uint8_t sgm41562b_get_charge_status(sgm41562b_handle_t handle)
{
    rt_uint8_t status;

    if (sgm41562b_get_system_status(handle, &status) == RT_EOK)
    {
        return (status & SGM41562B_CHG_STAT_MASK) >> SGM41562B_CHG_STAT_SHIFT;
    }
    return CHG_STAT_NOT_CHARGING;
}

rt_bool_t sgm41562b_is_power_good(sgm41562b_handle_t handle)
{
    rt_uint8_t status;

    if (sgm41562b_get_system_status(handle, &status) == RT_EOK)
    {
        return (status & SGM41562B_PG_STAT) ? RT_TRUE : RT_FALSE;
    }
    return RT_FALSE;
}

rt_bool_t sgm41562b_is_in_thermal_regulation(sgm41562b_handle_t handle)
{
    rt_uint8_t status;

    if (sgm41562b_get_system_status(handle, &status) == RT_EOK)
    {
        return (status & SGM41562B_THERM_STAT) ? RT_TRUE : RT_FALSE;
    }
    return RT_FALSE;
}

rt_bool_t sgm41562b_is_watchdog_fault(sgm41562b_handle_t handle)
{
    rt_uint8_t status;

    if (sgm41562b_get_system_status(handle, &status) == RT_EOK)
    {
        return (status & SGM41562B_WTD_FAULT) ? RT_TRUE : RT_FALSE;
    }
    return RT_FALSE;
}

/**
 * @brief 获取充电状态的字符串描述
 */
const char *sgm41562b_get_charge_status_str(sgm41562b_handle_t handle)
{
    rt_uint8_t status = sgm41562b_get_charge_status(handle);
    switch (status)
    {
    case CHG_STAT_NOT_CHARGING:
        return "Not Charging";
    case CHG_STAT_PRE_CHARGE:
        return "Pre-charge";
    case CHG_STAT_FAST_CHARGE:
        return "Fast Charge";
    case CHG_STAT_CHARGE_DONE:
        return "Charge Done";
    default:
        return "Unknown";
    }
}

/**
 * @brief 获取输入电源状态的字符串描述
 */
const char *sgm41562b_get_power_good_str(sgm41562b_handle_t handle)
{
    rt_bool_t pg = sgm41562b_is_power_good(handle);
    return pg ? "Power Good" : "Power Fail";
}

/**
 * @brief 获取热调节状态的字符串描述
 */
const char *sgm41562b_get_thermal_regulation_str(sgm41562b_handle_t handle)
{
    rt_bool_t therm = sgm41562b_is_in_thermal_regulation(handle);
    return therm ? "In Thermal Regulation" : "Normal";
}

/**
 * @brief 获取看门狗故障状态的字符串描述
 */
const char *sgm41562b_get_watchdog_fault_str(sgm41562b_handle_t handle)
{
    rt_bool_t fault = sgm41562b_is_watchdog_fault(handle);
    return fault ? "Watchdog Timeout" : "Normal";
}

/**
 * @brief 获取输入电压故障状态的字符串描述
 */
const char *sgm41562b_get_vin_fault_str(sgm41562b_handle_t handle)
{
    rt_uint8_t fault;
    if (sgm41562b_get_fault_status(handle, &fault) == RT_EOK)
        return (fault & SGM41562B_VIN_FAULT) ? "Input Fault (OVP/Bad)"
                                             : "Normal";
    return "Read Error";
}

/**
 * @brief 获取热关断故障状态的字符串描述
 */
const char *sgm41562b_get_thermal_shutdown_str(sgm41562b_handle_t handle)
{
    rt_uint8_t fault;
    if (sgm41562b_get_fault_status(handle, &fault) == RT_EOK)
        return (fault & SGM41562B_THEM_SD) ? "Thermal Shutdown" : "Normal";
    return "Read Error";
}

/**
 * @brief 获取电池过压故障状态的字符串描述
 */
const char *sgm41562b_get_battery_fault_str(sgm41562b_handle_t handle)
{
    rt_uint8_t fault;
    if (sgm41562b_get_fault_status(handle, &fault) == RT_EOK)
        return (fault & SGM41562B_BAT_FAULT) ? "Battery OVP" : "Normal";
    return "Read Error";
}

/**
 * @brief 获取充电安全定时器故障状态的字符串描述
 */
const char *sgm41562b_get_safety_timer_fault_str(sgm41562b_handle_t handle)
{
    rt_uint8_t fault;
    if (sgm41562b_get_fault_status(handle, &fault) == RT_EOK)
        return (fault & SGM41562B_STMR_FAULT) ? "Safety Timer Expired"
                                              : "Normal";
    return "Read Error";
}

/**
 * @brief 获取 NTC 故障状态的字符串描述（区分冷、热）
 */
const char *sgm41562b_get_ntc_fault_str(sgm41562b_handle_t handle)
{
    rt_uint8_t fault;
    if (sgm41562b_get_fault_status(handle, &fault) != RT_EOK)
        return "Read Error";

    if (fault & SGM41562B_NTC_FAULT_HOT)
        return "NTC Hot";
    else if (fault & SGM41562B_NTC_FAULT_COLD)
        return "NTC Cold";
    else
        return "Normal";
}

rt_err_t device_enter_shipping_mode(sgm41562b_handle_t handle)
{
    RT_ASSERT(handle != RT_NULL);

    // 1. 暂停监控线程，避免它进行 I2C 操作
    if (handle->monitor_thread != RT_NULL)
    {
        rt_thread_suspend(handle->monitor_thread);
    }

    // 2. 明确禁止 nINT 引脚的中断
    if (handle->irq_pin >= 0)
    {
        rt_pin_irq_enable(handle->irq_pin, PIN_IRQ_DISABLE);
        // 也可以考虑将其重新配置为普通上拉输入，彻底杜绝中断源
        // rt_pin_mode(handle->irq_pin, PIN_MODE_INPUT_PULLUP);
    }

    // 3. 禁用芯片看门狗（如果之前被启用了）
    sgm41562b_set_watchdog_timer(handle, WATCHDOG_DISABLE);

    // 4. 最后，发送命令进入 Shipping Mode
    return sgm41562b_enter_shipping_mode(handle);
}

rt_int16_t sgm41562b_get_charge_vlotage(sgm41562b_handle_t handle)
{
    return _charge_voltage;
}

rt_int16_t sgm41562b_get_charge_current(sgm41562b_handle_t handle)
{
    return _charge_current;
}

sgm41562b_handle_t sgm41562b_get_handle(void)
{
    return _handle;
}

/* --------------------------------------------------------------------------
 * Interrupt handling
 * -------------------------------------------------------------------------- */
void sgm41562b_attach_status_callback(sgm41562b_handle_t handle,
                                      void (*callback)(rt_uint8_t status_reg,
                                                       rt_uint8_t fault_reg,
                                                       void *user_data),
                                      void *user_data)
{
    RT_ASSERT(handle != RT_NULL);
    handle->status_callback = callback;
    handle->user_data = user_data;
}

void sgm41562b_isr_handler(sgm41562b_handle_t handle)
{
    if (handle == RT_NULL)
        return;
    rt_sem_release(&handle->irq_sem);
}

/* --------------------------------------------------------------------------
 * Default configuration (applied at init)
 * -------------------------------------------------------------------------- */
static void default_config(sgm41562b_handle_t handle)
{
    sgm41562b_set_nint_rst_time(handle, 0); /* nINT reset time = 8s */
    /* Apply safe default settings */
    sgm41562b_set_input_voltage_limit(handle, 4600);  /* 4.6V */
    sgm41562b_set_input_current_limit(handle, 500);   /* 500mA */
    sgm41562b_set_charge_voltage(handle, 4200);       /* 4.2V */
    sgm41562b_set_charge_current(handle, 200);        /* 200mA */
    sgm41562b_set_precharge_term_current(handle, 31);
    sgm41562b_set_system_voltage(handle, 4200);     
    sgm41562b_set_battery_uvlo(handle, 3000);
    sgm41562b_set_thermal_regulation_threshold(handle, THERMAL_REG_120C);
    sgm41562b_set_watchdog_timer(
        handle, WATCHDOG_DISABLE); /* Disable watchdog by default */
    sgm41562b_set_charge_safety_timer(handle, RT_TRUE, CHARGE_TIMER_8HRS);
    sgm41562b_enable_ntc(handle, RT_TRUE);
    sgm41562b_enable_pcb_otp(handle, RT_FALSE);
    sgm41562b_set_recharge_threshold(handle, RT_TRUE);  /* 200mV */
    sgm41562b_set_precharge_threshold(handle, RT_TRUE); /* 3.0V */
    sgm41562b_enable_charging(handle,
                              RT_TRUE);

    LOG_I("SGM41562B default configuration applied");
}