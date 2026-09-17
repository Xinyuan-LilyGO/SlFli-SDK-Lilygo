#include "drv2605.h"

#include "xl9555.h"

#define DBG_TAG "drv2605"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define DRV2605_ENABLE_DELAY_MS 10U
#define DRV2605_TEST_PATTERN 0xA5U
#define DRV2605_RESET_TIMEOUT_MS 10U

struct drv2605_device
{
    struct rt_i2c_bus_device *i2c_bus;
    rt_bool_t i2c_opened;
    rt_bool_t enable_ready;
};

struct drv2605_probe_register
{
    rt_uint8_t reg;
    rt_uint8_t reset_value;
    const char *name;
};

struct drv2605_register_value
{
    rt_uint8_t reg;
    rt_uint8_t value;
    const char *name;
};

static struct drv2605_device g_drv2605;

static const struct drv2605_probe_register g_probe_registers[] = {
    {0x00U, 0x60U, "STATUS"},           {0x01U, 0x40U, "MODE"},
    {0x16U, 0x3FU, "RATED_VOLTAGE"},    {0x17U, 0x89U, "OD_CLAMP"},
    {0x1AU, 0x36U, "FEEDBACK_CONTROL"}, {0x1BU, 0x93U, "CONTROL1"},
    {0x1CU, 0xF5U, "CONTROL2"},         {0x1DU, 0xA0U, "CONTROL3"},
    {0x1EU, 0x20U, "CONTROL4"},
};

static void drv2605_enable(rt_bool_t enabled)
{
    xl9555_pin_mode((rt_uint8_t)DRV2605_ENABLE_PIN, XL9555_PIN_OUTPUT);
    xl9555_digital_write((rt_uint8_t)DRV2605_ENABLE_PIN, enabled ? 1U : 0U);
}

static rt_err_t drv2605_prepare(void)
{
    rt_err_t result;
    rt_uint8_t enable_level;
    rt_thread_mdelay(100);
    drv2605_enable(RT_TRUE);

    if (g_drv2605.i2c_bus != RT_NULL)
        return RT_EOK;

    g_drv2605.i2c_bus =
        (struct rt_i2c_bus_device *)rt_device_find(DRV2605_I2C_BUS_NAME);
    if (g_drv2605.i2c_bus == RT_NULL)
    {
        LOG_E("I2C bus %s not found", DRV2605_I2C_BUS_NAME);
        drv2605_enable(RT_FALSE);
        return -RT_ENOSYS;
    }

    result =
        rt_device_open((rt_device_t)g_drv2605.i2c_bus, RT_DEVICE_OFLAG_RDWR);
    if (result != RT_EOK)
    {
        LOG_E("open I2C bus %s failed: %d", DRV2605_I2C_BUS_NAME, result);
        g_drv2605.i2c_bus = RT_NULL;
        drv2605_enable(RT_FALSE);
        return result;
    }
    g_drv2605.i2c_opened = RT_TRUE;

    struct rt_i2c_configuration configuration = {
        .mode = 0,
        .addr = 0,
        .timeout = 500,
        .max_hz = DRV2605_I2C_MAX_HZ,
    };

    result = rt_i2c_configure(g_drv2605.i2c_bus, &configuration);
    if (result != RT_EOK)
    {
        LOG_E("configure I2C bus failed: %d", result);
        (void)drv2605_deinit();
        return result;
    }

    LOG_I("I2C bus=%s addr=0x%02x speed=%u Hz", DRV2605_I2C_BUS_NAME,
          (unsigned int)DRV2605_I2C_ADDR, (unsigned int)DRV2605_I2C_MAX_HZ);
    return RT_EOK;
}

rt_err_t drv2605_read_reg(rt_uint8_t reg, rt_uint8_t *value)
{
    rt_size_t transferred;

    if (value == RT_NULL)
        return -RT_EINVAL;
    if (g_drv2605.i2c_bus == RT_NULL)
        return -RT_ERROR;

#ifdef RT_USING_PM
    rt_pm_request(PM_SLEEP_MODE_IDLE);
#endif
    transferred = rt_i2c_mem_read(g_drv2605.i2c_bus, DRV2605_I2C_ADDR, reg, 8U,
                                  value, 1U);
#ifdef RT_USING_PM
    rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif

    return (transferred == 1U) ? RT_EOK : -RT_ERROR;
}

/* Diagnostic-only path: the SF32 driver inserts STOP between these messages. */
static rt_err_t drv2605_transfer_read_reg(rt_uint8_t reg, rt_uint8_t *value)
{
    struct rt_i2c_msg messages[2];

    if (value == RT_NULL)
        return -RT_EINVAL;
    if (g_drv2605.i2c_bus == RT_NULL)
        return -RT_ERROR;

    messages[0].addr = DRV2605_I2C_ADDR;
    messages[0].flags = RT_I2C_WR;
    messages[0].buf = &reg;
    messages[0].len = 1U;

    messages[1].addr = DRV2605_I2C_ADDR;
    messages[1].flags = RT_I2C_RD;
    messages[1].buf = value;
    messages[1].len = 1U;

#ifdef RT_USING_PM
    rt_pm_request(PM_SLEEP_MODE_IDLE);
#endif
    if (rt_i2c_transfer(g_drv2605.i2c_bus, messages, 2U) != 2U)
    {
#ifdef RT_USING_PM
        rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
        return -RT_ERROR;
    }
#ifdef RT_USING_PM
    rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif

    return RT_EOK;
}

rt_err_t drv2605_write_reg(rt_uint8_t reg, rt_uint8_t value)
{
    struct rt_i2c_msg message;
    rt_uint8_t buffer[2] = {reg, value};

    if (g_drv2605.i2c_bus == RT_NULL)
        return -RT_ERROR;

    message.addr = DRV2605_I2C_ADDR;
    message.flags = RT_I2C_WR;
    message.buf = buffer;
    message.len = sizeof(buffer);

#ifdef RT_USING_PM
    rt_pm_request(PM_SLEEP_MODE_IDLE);
#endif
    if (rt_i2c_transfer(g_drv2605.i2c_bus, &message, 1U) != 1U)
    {
#ifdef RT_USING_PM
        rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
        return -RT_ERROR;
    }
#ifdef RT_USING_PM
    rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif

    return RT_EOK;
}

static rt_err_t drv2605_soft_reset(void)
{
    rt_uint8_t mode = DRV2605_MODE_DEV_RESET;
    rt_uint32_t elapsed_ms;
    rt_err_t result;

    result = drv2605_write_reg(DRV2605_REG_MODE, DRV2605_MODE_DEV_RESET);
    if (result != RT_EOK)
    {
        LOG_E("software reset write failed: %d", result);
        return result;
    }

    for (elapsed_ms = 0U; elapsed_ms < DRV2605_RESET_TIMEOUT_MS; elapsed_ms++)
    {
        rt_thread_mdelay(1U);
        result = drv2605_read_reg(DRV2605_REG_MODE, &mode);
        if (result == RT_EOK && mode == DRV2605_MODE_STANDBY)
        {
            LOG_I("software reset complete, MODE=0x%02x", (unsigned int)mode);
            return RT_EOK;
        }
    }

    LOG_E("software reset timeout: ret=%d MODE=0x%02x", result,
          (unsigned int)mode);
    return (result == RT_EOK) ? -RT_ETIMEOUT : result;
}

static rt_err_t drv2605_config(void)
{
    static const struct drv2605_register_value config[] = {
        {DRV2605_REG_MODE, DRV2605_MODE_INTERNAL_TRIGGER, "MODE"},
        {DRV2605_REG_LIBRARY, (rt_uint8_t)DRV2605_LIBRARY, "LIBRARY"},
        {DRV2605_REG_RTP_INPUT, 0x00U, "RTP_INPUT"},
        {DRV2605_REG_WAVESEQ1, 0x01U, "WAVESEQ1"},
        {DRV2605_REG_WAVESEQ2, 0x00U, "WAVESEQ2"},
        {DRV2605_REG_OVERDRIVE, 0x00U, "OVERDRIVE"},
        {DRV2605_REG_SUSTAIN_POS, 0x00U, "SUSTAIN_POS"},
        {DRV2605_REG_SUSTAIN_NEG, 0x00U, "SUSTAIN_NEG"},
        {DRV2605_REG_BRAKE, 0x00U, "BRAKE"},
        {DRV2605_REG_AUDIO_MAX_INPUT, 0x64U, "AUDIO_MAX_INPUT"},
    };
    rt_uint8_t value;
    rt_uint8_t readback;
    rt_size_t index;
    rt_err_t result;

    for (index = 0U; index < sizeof(config) / sizeof(config[0]); index++)
    {
        result = drv2605_write_reg(config[index].reg, config[index].value);
        if (result != RT_EOK)
        {
            LOG_E("configure %s write failed: %d", config[index].name, result);
            return result;
        }

        result = drv2605_read_reg(config[index].reg, &readback);
        if (result != RT_EOK)
        {
            LOG_E("configure %s readback failed: %d", config[index].name,
                  result);
            return result;
        }
        if (readback != config[index].value)
        {
            LOG_E("configure %s mismatch: wrote=0x%02x read=0x%02x",
                  config[index].name, (unsigned int)config[index].value,
                  (unsigned int)readback);
            return -RT_ERROR;
        }
    }

    result = drv2605_read_reg(DRV2605_REG_FEEDBACK, &value);
    if (result != RT_EOK)
    {
        LOG_E("read FEEDBACK failed: %d", result);
        return result;
    }
    value &= (rt_uint8_t)~DRV2605_FEEDBACK_N_ERM_LRA;
    result = drv2605_write_reg(DRV2605_REG_FEEDBACK, value);
    if (result != RT_EOK)
    {
        LOG_E("configure ERM feedback failed: %d", result);
        return result;
    }
    readback = 0U;
    result = drv2605_read_reg(DRV2605_REG_FEEDBACK, &readback);
    if (result != RT_EOK || readback != value)
    {
        LOG_E("FEEDBACK readback mismatch: ret=%d wrote=0x%02x "
              "read=0x%02x",
              result, (unsigned int)value, (unsigned int)readback);
        return (result == RT_EOK) ? -RT_ERROR : result;
    }

    result = drv2605_read_reg(DRV2605_REG_CONTROL3, &value);
    if (result != RT_EOK)
    {
        LOG_E("read CONTROL3 failed: %d", result);
        return result;
    }
    value |= DRV2605_CONTROL3_ERM_OPEN_LOOP;
    result = drv2605_write_reg(DRV2605_REG_CONTROL3, value);
    if (result != RT_EOK)
    {
        LOG_E("configure ERM open-loop failed: %d", result);
        return result;
    }
    readback = 0U;
    result = drv2605_read_reg(DRV2605_REG_CONTROL3, &readback);
    if (result != RT_EOK || readback != value)
    {
        LOG_E("CONTROL3 readback mismatch: ret=%d wrote=0x%02x "
              "read=0x%02x",
              result, (unsigned int)value, (unsigned int)readback);
        return (result == RT_EOK) ? -RT_ERROR : result;
    }

    LOG_I("configuration complete: library=%u, effect=1, ERM open-loop",
          (unsigned int)DRV2605_LIBRARY);
    return RT_EOK;
}

rt_err_t drv2605_read_id(rt_uint8_t *device_id)
{
    rt_uint8_t status;
    rt_err_t result;

    if (device_id == RT_NULL)
        return -RT_EINVAL;

    result = drv2605_read_reg(DRV2605_REG_STATUS, &status);
    if (result != RT_EOK)
    {
        LOG_E("read STATUS register failed: %d", result);
        return result;
    }

    *device_id = (rt_uint8_t)((status & DRV2605_STATUS_DEVICE_ID_MASK) >>
                              DRV2605_STATUS_DEVICE_ID_SHIFT);
    LOG_I("STATUS=0x%02x, DEVICE_ID=%u", (unsigned int)status,
          (unsigned int)*device_id);

    if (*device_id != DRV2605_DEVICE_ID &&
        *device_id != DRV2605_DEVICE_ID_LOW_VOLTAGE)
    {
        LOG_E("unexpected DEVICE_ID=%u", (unsigned int)*device_id);
        return -RT_ERROR;
    }

    return RT_EOK;
}

rt_err_t drv2605_diagnose(void)
{
    rt_uint8_t transfer_value;
    rt_uint8_t mem_value;
    rt_uint8_t saved_rtp;
    rt_uint8_t test_value;
    rt_uint8_t status_low = 0xEEU;
    rt_uint8_t status_high = 0xEEU;
    rt_uint8_t enable_low;
    rt_uint8_t enable_high;
    rt_bool_t all_zero = RT_TRUE;
    rt_err_t transfer_result;
    rt_err_t mem_result;
    rt_err_t result;
    rt_err_t restore_result;
    rt_size_t index;
    rt_size_t successful_reads = 0U;

    result = drv2605_prepare();
    if (result != RT_EOK)
        return result;

    LOG_I("diagnostic start");
    drv2605_enable(RT_FALSE);
    rt_thread_mdelay(2U);
    enable_low = xl9555_digital_read((rt_uint8_t)DRV2605_ENABLE_PIN);
    transfer_result =
        drv2605_transfer_read_reg(DRV2605_REG_STATUS, &status_low);

    drv2605_enable(RT_TRUE);
    rt_thread_mdelay(DRV2605_ENABLE_DELAY_MS);
    enable_high = xl9555_digital_read((rt_uint8_t)DRV2605_ENABLE_PIN);
    mem_result = drv2605_read_reg(DRV2605_REG_STATUS, &status_high);
    LOG_I("EN toggle: low=%u STATUS(ret=%d)=0x%02x; "
          "high=%u STATUS(ret=%d)=0x%02x",
          (unsigned int)enable_low, transfer_result, (unsigned int)status_low,
          (unsigned int)enable_high, mem_result, (unsigned int)status_high);

    for (index = 0U;
         index < sizeof(g_probe_registers) / sizeof(g_probe_registers[0]);
         index++)
    {
        transfer_value = 0xEEU;
        mem_value = 0xEEU;
        transfer_result = drv2605_transfer_read_reg(
            g_probe_registers[index].reg, &transfer_value);
        mem_result = drv2605_read_reg(g_probe_registers[index].reg, &mem_value);
        if ((transfer_result == RT_EOK && transfer_value != 0U) ||
            (mem_result == RT_EOK && mem_value != 0U))
            all_zero = RT_FALSE;
        if (transfer_result == RT_EOK)
            successful_reads++;
        if (mem_result == RT_EOK)
            successful_reads++;

        LOG_I("reg 0x%02x %-16s expected~0x%02x: "
              "transfer(%d)=0x%02x mem(%d)=0x%02x",
              (unsigned int)g_probe_registers[index].reg,
              g_probe_registers[index].name,
              (unsigned int)g_probe_registers[index].reset_value,
              transfer_result, (unsigned int)transfer_value, mem_result,
              (unsigned int)mem_value);
    }

    if (successful_reads == 0U)
        LOG_E("all diagnostic register transfers failed");
    else if (all_zero)
        LOG_E("all readable registers returned zero; check M_EN at the "
              "DRV2605 pin and I2C waveforms");

    result = drv2605_read_reg(DRV2605_REG_RTP_INPUT, &saved_rtp);
    if (result != RT_EOK)
    {
        LOG_E("cannot save RTP_INPUT for loopback: %d", result);
        return result;
    }

    result = drv2605_write_reg(DRV2605_REG_RTP_INPUT, DRV2605_TEST_PATTERN);
    if (result != RT_EOK)
    {
        LOG_E("RTP_INPUT test write failed: %d", result);
        return result;
    }

    test_value = 0U;
    result = drv2605_read_reg(DRV2605_REG_RTP_INPUT, &test_value);
    restore_result = drv2605_write_reg(DRV2605_REG_RTP_INPUT, saved_rtp);
    if (restore_result != RT_EOK)
        LOG_E("restore RTP_INPUT failed");

    LOG_I("RTP_INPUT loopback: wrote=0x%02x read=0x%02x restored=0x%02x",
          (unsigned int)DRV2605_TEST_PATTERN, (unsigned int)test_value,
          (unsigned int)saved_rtp);
    if (result != RT_EOK || restore_result != RT_EOK ||
        test_value != DRV2605_TEST_PATTERN)
    {
        LOG_E("register loopback failed; ACK alone does not prove register "
              "access (DRV2605 also ACKs while EN is low)");
        return -RT_ERROR;
    }

    LOG_I("register loopback passed; I2C register access is working");
    return RT_EOK;
}

rt_err_t drv2605_init(void)
{
    rt_uint8_t device_id;
    rt_err_t result;

    result = drv2605_prepare();
    if (result != RT_EOK)
        return result;

    result = drv2605_read_id(&device_id);
    if (result != RT_EOK)
    {
        (void)drv2605_deinit();
        return result;
    }

    result = drv2605_config();
    if (result != RT_EOK)
    {
        LOG_E("DRV2605 configuration failed: %d", result);
        return result;
    }

    LOG_I("%s detected at 0x%02x on %s",
          (device_id == DRV2605_DEVICE_ID_LOW_VOLTAGE) ? "DRV2605L" : "DRV2605",
          (unsigned int)DRV2605_I2C_ADDR, DRV2605_I2C_BUS_NAME);
    return RT_EOK;
}
MSH_CMD_EXPORT(drv2605_init, drv2605 init test);

rt_err_t drv2605_deinit(void)
{
    if (g_drv2605.i2c_bus != RT_NULL && g_drv2605.i2c_opened)
        (void)rt_device_close((rt_device_t)g_drv2605.i2c_bus);

    g_drv2605.i2c_opened = RT_FALSE;
    g_drv2605.i2c_bus = RT_NULL;
    drv2605_enable(RT_FALSE);
    return RT_EOK;
}
