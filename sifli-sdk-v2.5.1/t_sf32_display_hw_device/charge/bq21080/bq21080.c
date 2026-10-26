#include "bq21080.h"
#include "bf0_hal.h"
#include "log.h"

static bq21080_dev_t bq21080_dev;

char *charge_status_str[] = {"Not Charging", "Constant Current",
                             "Constant Voltage", "Charge Done"};

char *ts_status_str[] = {"Normal", "Charging Suspended (Too Hot/Cold)",
                         "Reduced Current (Cool)", "Reduced Voltage (Warm)"};

uint16_t bq21080_charge_voltage = 4200; // mV
uint16_t bq21080_charge_current = 500;  // mA

#ifdef PKG_USING_BQ21080

void bq21080_monitor_thread_entry(void *parameter);

/* 内部函数 */
static rt_err_t bq21080_write_reg(rt_uint8_t reg, rt_uint8_t value)
{
    HAL_StatusTypeDef ret;
    ret = rt_i2c_mem_write(bq21080_dev.bq21080_i2c_bus, BQ21080_DEVICE_ADDRESS,
                           reg, 1, &value, 1);
    if (ret != 1)
    {
        LOG_E("I2C wirte failed\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

static rt_err_t bq21080_read_reg(rt_uint8_t reg, rt_uint8_t *value)
{
    HAL_StatusTypeDef ret;
    ret = rt_i2c_mem_read(bq21080_dev.bq21080_i2c_bus, BQ21080_DEVICE_ADDRESS,
                          reg, 1, value, 1); // ret is data length
    if (ret != 1)
    {
        LOG_E("I2C read failed\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t bq21080_init(void)
{
    rt_err_t result;

    /* 查找I2C设备 */
    bq21080_dev.bq21080_i2c_bus = rt_i2c_bus_device_find(BQ21080_I2C_BUS_NAME);
    if (bq21080_dev.bq21080_i2c_bus == RT_NULL)
    {
        log_d("BQ21080: Cannot find I2C device %s\n", BQ21080_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    /* 打开I2C设备 */
    result = rt_device_open((rt_device_t)bq21080_dev.bq21080_i2c_bus,
                            RT_DEVICE_FLAG_RDWR);
    if (result != RT_EOK)
    {
        log_d("BQ21080: Open I2C device failed\n");
        return result;
    }

    struct rt_i2c_configuration configuration = {
        .mode = 0,
        .addr = 0,
        .timeout = 500,   // 超时时间（ms）
        .max_hz = 400000, // I2C速率（hz）
    };
    rt_i2c_configure(bq21080_dev.bq21080_i2c_bus, &configuration);

    /* 创建互斥锁 */
    bq21080_dev.lock = rt_mutex_create("bq21080", RT_IPC_FLAG_FIFO);
    if (bq21080_dev.lock == RT_NULL)
    {
        log_d("BQ21080: Create mutex failed\n");
        rt_device_close(bq21080_dev.i2c_dev);
        return -RT_ERROR;
    }

    /* 配置充电参数 */
    rt_mutex_take(bq21080_dev.lock, RT_WAITING_FOREVER);

    bq21080_set_charge_voltage(bq21080_charge_voltage);
    bq21080_set_charge_current(bq21080_charge_current);
    // bq21080_set_termination_percent(20);      // 终止电流 20% ICHG
    // bq21080_set_precharge_ratio(true);     // 预充电 = 2x 终止电流
    // bq21080_set_precharge_voltage_threshold(3000); // 3.0V 切换阈值
    bq21080_set_input_current_limit(800);

    // 配置充电控制1: 电池放电电流限制1A, BUVLO=3.0V
    bq21080_write_reg(BQ21080_REG_CHARGECTRL1, 0x56);

    // SYS调节: 4.5V, 正常模式
    bq21080_write_reg(BQ21080_REG_SYS_REG, 0x40);

    bq21080_enable_charging(true);

    rt_mutex_release(bq21080_dev.lock);
    return RT_EOK;
}

void bq21080_monitor_thread_entry(void *parameter)
{
    bq21080_status_t status;

    while (1)
    {
        if (bq21080_get_status(&status) == RT_EOK)
        {
            log_d("=== BQ21080 Charge Status ===\n");
            log_d("Charge Status: %s\n",
                  charge_status_str[status.charge_status]);
            log_d("VIN Power Good: %s\n", status.vin_power_good ? "Yes" : "No");

            log_d("=== Power Management Status ===\n");
            log_d("Thermal Regulation: %s\n",
                  status.thermal_reg_active ? "Active" : "Inactive");
            log_d("VINDPM Active: %s\n",
                  status.vindpm_active ? "Active" : "Inactive");
            log_d("VDPPM Active: %s\n",
                  status.vdppm_active ? "Active" : "Inactive");
            log_d("ILIM Active: %s\n",
                  status.ilim_active ? "Active" : "Inactive");

            log_d("--- Temperature Status ---\n");
            log_d("TS Open: %s\n", status.ts_open ? "YES - Check NTC!" : "No");
            log_d("TS Status: %s\n", ts_status_str[status.ts_status]);

            if (status.safety_timer_fault)
            {
                log_d("!!! Safety Timer Fault !!!\n");
            }
            if (status.bat_ocp_fault)
            {
                log_d("!!! Battery OCP Fault !!!\n");
            }
            if (status.buvlo_fault)
            {
                log_d("!!! Battery UVLO Fault !!!\n");
            }
            if (status.vin_ovp_fault)
            {
                log_d("!!! Input OVP Fault !!!\n");
            }

            log_d("========================\n\n");
        }
        else
        {
            log_d("Failed to read BQ21080 status\n");
        }

        rt_thread_mdelay(10000); // 5秒监控间隔
    }
}

/* 设置充电电压 */
rt_err_t bq21080_set_charge_voltage(rt_uint16_t voltage_mv)
{
    rt_uint8_t vbatreg_code;

    if (voltage_mv < 3500 || voltage_mv > 4650)
    {
        return -RT_EINVAL;
    }
    bq21080_charge_voltage = voltage_mv;
    vbatreg_code = (voltage_mv - 3500) / 10;

    rt_mutex_take(bq21080_dev.lock, RT_WAITING_FOREVER);
    rt_err_t result = bq21080_write_reg(BQ21080_REG_VBAT_CTRL, vbatreg_code);
    rt_mutex_release(bq21080_dev.lock);

    return result;
}

/* 设置充电电流 */
rt_err_t bq21080_set_charge_current(rt_uint16_t current_ma)
{
    rt_uint8_t ichg_code;

    if (current_ma < 5 || current_ma > 800)
    {
        return -RT_EINVAL;
    }
    bq21080_charge_current = current_ma;
    if (current_ma <= 35)
    {
        ichg_code = current_ma - 5;
    }
    else
    {
        ichg_code = 31 + (current_ma - 40) / 10;
    }

    rt_mutex_take(bq21080_dev.lock, RT_WAITING_FOREVER);
    rt_err_t result =
        bq21080_write_reg(BQ21080_REG_ICHG_CTRL, ichg_code & 0x7F);
    rt_mutex_release(bq21080_dev.lock);

    return result;
}

/* 设置输入电流限制 */
rt_err_t bq21080_set_input_current_limit(rt_uint16_t current_ma)
{
    rt_uint8_t ilim_code;

    // 根据数据手册表8-17选择最接近的值
    if (current_ma <= 50)
    {
        ilim_code = 0x00;
    }
    else if (current_ma <= 100)
    {
        ilim_code = 0x01;
    }
    else if (current_ma <= 200)
    {
        ilim_code = 0x02;
    }
    else if (current_ma <= 300)
    {
        ilim_code = 0x03;
    }
    else if (current_ma <= 400)
    {
        ilim_code = 0x04;
    }
    else if (current_ma <= 500)
    {
        ilim_code = 0x05;
    }
    else if (current_ma <= 700)
    {
        ilim_code = 0x06;
    }
    else
    {
        ilim_code = 0x07;
    }

    rt_mutex_take(bq21080_dev.lock, RT_WAITING_FOREVER);

    rt_uint8_t tmr_ilim;
    if (bq21080_read_reg(BQ21080_REG_TMR_ILIM, &tmr_ilim) == RT_EOK)
    {
        tmr_ilim = (tmr_ilim & 0xF8) | (ilim_code & 0x07);
        rt_err_t result = bq21080_write_reg(BQ21080_REG_TMR_ILIM, tmr_ilim);
        rt_mutex_release(bq21080_dev.lock);
        return result;
    }

    rt_mutex_release(bq21080_dev.lock);
    return -RT_ERROR;
}

/* 设置终止电流百分比 */
rt_err_t bq21080_set_termination_percent(rt_uint8_t percent)
{
    rt_uint8_t reg_val, new_val;
    rt_uint8_t iter_code;

    switch (percent)
    {
    case 0:  iter_code = 0x00; break;
    case 5:  iter_code = 0x01; break;
    case 10: iter_code = 0x02; break;
    case 20: iter_code = 0x03; break;
    default: return -RT_EINVAL;
    }

    rt_mutex_take(bq21080_dev.lock, RT_WAITING_FOREVER);
    if (bq21080_read_reg(BQ21080_REG_CHARGECTRL0, &reg_val) != RT_EOK)
    {
        rt_mutex_release(bq21080_dev.lock);
        return -RT_ERROR;
    }
    new_val = (reg_val & ~0x30) | (iter_code << 4);
    rt_err_t result = bq21080_write_reg(BQ21080_REG_CHARGECTRL0, new_val);
    rt_mutex_release(bq21080_dev.lock);
    return result;
}

/* 设置预充电电流比例（相对于终止电流） */
rt_err_t bq21080_set_precharge_ratio(rt_bool_t double_term)
{
    rt_uint8_t reg_val, new_val;

    rt_mutex_take(bq21080_dev.lock, RT_WAITING_FOREVER);
    if (bq21080_read_reg(BQ21080_REG_CHARGECTRL0, &reg_val) != RT_EOK)
    {
        rt_mutex_release(bq21080_dev.lock);
        return -RT_ERROR;
    }
    if (double_term)
        new_val = reg_val & ~0x40;      // IPRECHG=0 -> 2x Term
    else
        new_val = reg_val | 0x40;       // IPRECHG=1 -> 1x Term
    rt_err_t result = bq21080_write_reg(BQ21080_REG_CHARGECTRL0, new_val);
    rt_mutex_release(bq21080_dev.lock);
    return result;
}

/* 设置预充电电压阈值（从预充电切换到恒流的电池电压） */
rt_err_t bq21080_set_precharge_voltage_threshold(rt_uint16_t threshold_mv)
{
    rt_uint8_t reg_val, new_val;
    rt_uint8_t vlovv_bit;

    if (threshold_mv == 3000)
        vlovv_bit = 0;
    else if (threshold_mv == 2800)
        vlovv_bit = 1;
    else
        return -RT_EINVAL;

    rt_mutex_take(bq21080_dev.lock, RT_WAITING_FOREVER);
    if (bq21080_read_reg(BQ21080_REG_IC_CTRL, &reg_val) != RT_EOK)
    {
        rt_mutex_release(bq21080_dev.lock);
        return -RT_ERROR;
    }
    new_val = (reg_val & ~0x40) | (vlovv_bit << 6);
    rt_err_t result = bq21080_write_reg(BQ21080_REG_IC_CTRL, new_val);
    rt_mutex_release(bq21080_dev.lock);
    return result;
}

/* 获取状态信息 */
rt_err_t bq21080_get_status(bq21080_status_t *status)
{
    rt_uint8_t stat0, stat1, flag0;

    if (status == RT_NULL)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(bq21080_dev.lock, RT_WAITING_FOREVER);

    if (bq21080_read_reg(BQ21080_REG_STAT0, &stat0) != RT_EOK ||
        bq21080_read_reg(BQ21080_REG_STAT1, &stat1) != RT_EOK ||
        bq21080_read_reg(BQ21080_REG_FLAG0, &flag0) != RT_EOK)
    {
        rt_mutex_release(bq21080_dev.lock);
        return -RT_ERROR;
    }

    rt_mutex_release(bq21080_dev.lock);

    /* 解析STAT0寄存器 */
    status->ts_open = (stat0 >> 7) & 0x01;
    status->charge_status = (stat0 >> 5) & 0x03;
    status->ilim_active = (stat0 >> 4) & 0x01;
    status->vdppm_active = (stat0 >> 3) & 0x01;
    status->vindpm_active = (stat0 >> 2) & 0x01;
    status->thermal_reg_active = (stat0 >> 1) & 0x01;
    status->vin_power_good = stat0 & 0x01;

    /* 解析STAT1寄存器 */
    status->ts_status = (stat1 >> 3) & 0x03; // TS_STAT_1:0
    status->safety_timer_fault = (stat1 >> 2) & 0x01;

    /* 解析FLAG0寄存器 */
    status->bat_ocp_fault = flag0 & 0x01;
    status->buvlo_fault = (flag0 >> 1) & 0x01;
    status->vin_ovp_fault = (flag0 >> 2) & 0x01;

    return RT_EOK;
}

/* 启用/禁用充电 */

rt_err_t bq21080_enable_charging(rt_bool_t enable)
{
    rt_uint8_t ichg_ctrl;

    rt_mutex_take(bq21080_dev.lock, RT_WAITING_FOREVER);

    if (bq21080_read_reg(BQ21080_REG_ICHG_CTRL, &ichg_ctrl) == RT_EOK)
    {
        if (enable)
        {
            ichg_ctrl &= ~(1 << 7); // 清除CHG_DIS位
        }
        else
        {
            ichg_ctrl |= (1 << 7); // 设置CHG_DIS位
        }
        rt_err_t result = bq21080_write_reg(BQ21080_REG_ICHG_CTRL, ichg_ctrl);
        rt_mutex_release(bq21080_dev.lock);
        return result;
    }

    rt_mutex_release(bq21080_dev.lock);
    return -RT_ERROR;
}

rt_err_t bq21080_enter_ship_mode(void)
{
    rt_uint8_t ship_rst;
    rt_err_t result;
    rt_mutex_take(bq21080_dev.lock, RT_WAITING_FOREVER);

    if (bq21080_read_reg(BQ21080_REG_SHIP_RST, &ship_rst) == RT_EOK)
    {
        // ship_rst |= (0x02 << 5);
        ship_rst = (ship_rst & ~(0x03 << 5)) | (0x02 << 5); // EN_RST_SHIP = 10
        result = bq21080_write_reg(BQ21080_REG_SHIP_RST, ship_rst);
    }

    rt_mutex_release(bq21080_dev.lock);
    return result;
}

/* 执行硬件复位（完全电源循环） */
rt_err_t bq21080_hardware_reset(void)
{
    rt_uint8_t ship_rst;
    rt_err_t result;
    rt_mutex_take(bq21080_dev.lock, RT_WAITING_FOREVER);
    if (bq21080_read_reg(BQ21080_REG_SHIP_RST, &ship_rst) == RT_EOK)
    {
        // EN_RST_SHIP = 01: 软件复位
        ship_rst = (ship_rst & ~(0x03 << 5)) | (0x03 << 5);
        result = bq21080_write_reg(BQ21080_REG_SHIP_RST, ship_rst);
    }
    rt_mutex_release(bq21080_dev.lock);
    return result;
}

/* 软件复位（寄存器复位） only to reg*/
rt_err_t bq21080_software_reset(void)
{
    rt_uint8_t ship_rst;
    rt_err_t result;
    rt_mutex_take(bq21080_dev.lock, RT_WAITING_FOREVER);
    if (bq21080_read_reg(BQ21080_REG_SHIP_RST, &ship_rst) == RT_EOK)
    {
        // EN_RST_SHIP = 01: 软件复位
        ship_rst |= (0x01 << 7);

        result = bq21080_write_reg(BQ21080_REG_SHIP_RST, ship_rst);
    }

    rt_mutex_release(bq21080_dev.lock);
    return result;
}

uint16_t bq21080_get_charge_voltage(void)
{
    return bq21080_charge_voltage;
}

uint16_t bq21080_get_charge_current(void)
{
    return bq21080_charge_current;
}



#endif