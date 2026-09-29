#include "SX126x.h"
#include "SX126x_commands.h"

#include <rtthread.h>
#include <string.h>

#ifndef RADIOLIB_DEFAULT_FREQUENCY
#define RADIOLIB_DEFAULT_FREQUENCY 868000000U
#endif
#ifndef RADIOLIB_SX1262_TCXO_VOLTAGE_MV
#define RADIOLIB_SX1262_TCXO_VOLTAGE_MV 3000
#endif
#ifndef RADIOLIB_SX1262_TCXO_DELAY_MS
#define RADIOLIB_SX1262_TCXO_DELAY_MS 5
#endif

static const uint8_t sx126x_bandwidth_values[] = {
    0x00, 0x08, 0x01, 0x09, 0x02, 0x0A, 0x03, 0x04, 0x05, 0x06
};

static const uint32_t sx126x_bandwidth_hz[] = {
    7810, 10420, 15630, 20830, 31250, 41670, 62500, 125000, 250000, 500000
};

static int16_t SX126x_write(SX126x_t *radio, uint8_t command,
                            const uint8_t *data, size_t length)
{
    if ((radio == RT_NULL) || (radio->module == RT_NULL)) {
        return RADIOLIB_ERR_NOT_INITIALIZED;
    }
    return Module_writeCommand(radio->module, command, data, length,
                               command != SX126X_CMD_SET_SLEEP);
}

static int16_t SX126x_read(SX126x_t *radio, uint8_t command, uint8_t *status,
                           uint8_t *data, size_t length)
{
    if ((radio == RT_NULL) || (radio->module == RT_NULL)) {
        return RADIOLIB_ERR_NOT_INITIALIZED;
    }
    return Module_readCommand(radio->module, command, status, data, length);
}

static uint32_t SX126x_timeoutTicks(uint32_t timeout_ms, bool continuous)
{
    uint64_t ticks;

    if (continuous) {
        return 0xFFFFFFU;
    }
    ticks = (uint64_t)timeout_ms * 64U;
    if (ticks == 0U) {
        ticks = 1U;
    } else if (ticks > 0xFFFFFEU) {
        ticks = 0xFFFFFEU;
    }
    return (uint32_t)ticks;
}

static int16_t SX126x_setTimeoutCommand(SX126x_t *radio, uint8_t command,
                                        uint32_t timeout_ms, bool continuous)
{
    uint32_t ticks = SX126x_timeoutTicks(timeout_ms, continuous);
    uint8_t data[3];

    data[0] = (uint8_t)(ticks >> 16);
    data[1] = (uint8_t)(ticks >> 8);
    data[2] = (uint8_t)ticks;
    return SX126x_write(radio, command, data, sizeof(data));
}

static int16_t SX126x_setIrqParams(SX126x_t *radio, uint16_t irq_mask,
                                   uint16_t dio1_mask)
{
    uint8_t data[8];

    data[0] = (uint8_t)(irq_mask >> 8);
    data[1] = (uint8_t)irq_mask;
    data[2] = (uint8_t)(dio1_mask >> 8);
    data[3] = (uint8_t)dio1_mask;
    data[4] = 0;
    data[5] = 0;
    data[6] = 0;
    data[7] = 0;
    return SX126x_write(radio, SX126X_CMD_SET_DIO_IRQ_PARAMS, data,
                        sizeof(data));
}

static int16_t SX126x_getIrqStatus(SX126x_t *radio, uint16_t *irq)
{
    uint8_t data[2];
    int16_t state;

    if (irq == RT_NULL) {
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    }
    state = SX126x_read(radio, SX126X_CMD_GET_IRQ_STATUS, RT_NULL, data,
                        sizeof(data));
    if (state == RADIOLIB_ERR_NONE) {
        *irq = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    }
    return state;
}

static int16_t SX126x_clearIrqStatus(SX126x_t *radio, uint16_t irq)
{
    uint8_t data[2] = {(uint8_t)(irq >> 8), (uint8_t)irq};
    return SX126x_write(radio, SX126X_CMD_CLEAR_IRQ_STATUS, data, sizeof(data));
}

static int16_t SX126x_waitForIrq(SX126x_t *radio, uint16_t done_mask,
                                 uint32_t timeout_ms, uint16_t *irq_result)
{
    rt_tick_t start = rt_tick_get();
    rt_tick_t timeout = rt_tick_from_millisecond(timeout_ms);
    uint16_t irq = 0;
    int16_t state;

    do {
        state = SX126x_getIrqStatus(radio, &irq);
        if (state != RADIOLIB_ERR_NONE) {
            return state;
        }
        if ((irq & (done_mask | SX126X_IRQ_TIMEOUT | SX126X_IRQ_CRC_ERROR |
                    SX126X_IRQ_HEADER_ERROR)) != 0U) {
            if (irq_result != RT_NULL) {
                *irq_result = irq;
            }
            return RADIOLIB_ERR_NONE;
        }
        rt_thread_mdelay(1);
    } while ((rt_tick_get() - start) < timeout);

    return RADIOLIB_ERR_RX_TIMEOUT;
}

static int16_t SX126x_applyModulation(SX126x_t *radio)
{
    uint8_t data[4];
    uint32_t bandwidth;
    uint64_t symbol_us;

    if ((uint32_t)radio->config.bandwidth >=
        (sizeof(sx126x_bandwidth_values) / sizeof(sx126x_bandwidth_values[0]))) {
        return RADIOLIB_ERR_INVALID_BANDWIDTH;
    }
    bandwidth = sx126x_bandwidth_hz[radio->config.bandwidth];
    symbol_us = ((uint64_t)1000000U << radio->config.spreading_factor) /
                bandwidth;
    data[0] = radio->config.spreading_factor;
    data[1] = sx126x_bandwidth_values[radio->config.bandwidth];
    data[2] = (uint8_t)(radio->config.coding_rate - 4U);
    data[3] = (symbol_us >= 16000U) ? 1U : 0U;
    return SX126x_write(radio, SX126X_CMD_SET_MODULATION_PARAMS, data,
                        sizeof(data));
}

static int16_t SX126x_applyPacket(SX126x_t *radio, uint8_t payload_length)
{
    uint8_t data[6];

    data[0] = (uint8_t)(radio->config.preamble_length >> 8);
    data[1] = (uint8_t)radio->config.preamble_length;
    data[2] = radio->implicit_header ? 1U : 0U;
    data[3] = radio->implicit_header ? radio->implicit_length : payload_length;
    data[4] = radio->config.crc_enabled ? 1U : 0U;
    data[5] = radio->config.invert_iq ? 1U : 0U;
    return SX126x_write(radio, SX126X_CMD_SET_PACKET_PARAMS, data,
                        sizeof(data));
}

static int16_t SX126x_readPacketStatus(SX126x_t *radio)
{
    uint8_t data[3];
    uint8_t freq_data[3];
    int32_t raw;
    int16_t state;

    state = SX126x_read(radio, SX126X_CMD_GET_PACKET_STATUS, RT_NULL, data,
                        sizeof(data));
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }
    radio->packet_rssi_dbm = -(int16_t)data[0] / 2;
    radio->packet_snr_db = ((int8_t)data[1]) / 4;

    state = Module_readRegister(radio->module, SX126X_REG_FREQUENCY_ERROR_MSB,
                                freq_data, sizeof(freq_data));
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }
    raw = ((int32_t)(freq_data[0] & 0x0FU) << 16) |
          ((int32_t)freq_data[1] << 8) | freq_data[2];
    if ((raw & 0x80000L) != 0L) {
        raw -= 0x100000L;
    }
    radio->frequency_error_hz = (int32_t)(((int64_t)raw *
        sx126x_bandwidth_hz[radio->config.bandwidth]) / 16777216L);
    return RADIOLIB_ERR_NONE;
}

static uint8_t SX126x_tcxoCode(uint16_t voltage_mv)
{
    if (voltage_mv <= 1600U) return 0;
    if (voltage_mv <= 1700U) return 1;
    if (voltage_mv <= 1800U) return 2;
    if (voltage_mv <= 2200U) return 3;
    if (voltage_mv <= 2400U) return 4;
    if (voltage_mv <= 2700U) return 5;
    if (voltage_mv <= 3000U) return 6;
    return 7;
}

static int16_t SX126x_configureTcxo(SX126x_t *radio)
{
    uint32_t delay_ticks;
    uint8_t data[4];
    int16_t state;

    if (radio->config.tcxo_voltage_mv == 0U) {
        return RADIOLIB_ERR_NONE;
    }
    delay_ticks = (uint32_t)radio->config.tcxo_delay_ms * 64U;
    data[0] = SX126x_tcxoCode(radio->config.tcxo_voltage_mv);
    data[1] = (uint8_t)(delay_ticks >> 16);
    data[2] = (uint8_t)(delay_ticks >> 8);
    data[3] = (uint8_t)delay_ticks;
    state = SX126x_write(radio, SX126X_CMD_SET_DIO3_TCXO, data, sizeof(data));
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }
    data[0] = 0x7F;
    return SX126x_write(radio, SX126X_CMD_CALIBRATE, data, 1);
}

static int16_t SX126x_configurePower(SX126x_t *radio)
{
    uint8_t value;
    uint8_t pa[4] = {0x04, 0x07, 0x00, 0x01};
    uint8_t tx[2];
    int16_t state;

    state = Module_readRegister(radio->module, SX126X_REG_TX_CLAMP_CONFIG,
                                &value, 1);
    if (state != RADIOLIB_ERR_NONE) return state;
    value |= 0x1E;
    state = Module_writeRegister(radio->module, SX126X_REG_TX_CLAMP_CONFIG,
                                 &value, 1);
    if (state != RADIOLIB_ERR_NONE) return state;
    value = 0x38;
    state = Module_writeRegister(radio->module, SX126X_REG_OCP_CONFIGURATION,
                                 &value, 1);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_write(radio, SX126X_CMD_SET_PA_CONFIG, pa, sizeof(pa));
    if (state != RADIOLIB_ERR_NONE) return state;
    tx[0] = (uint8_t)radio->config.output_power_dbm;
    tx[1] = 0x09;
    return SX126x_write(radio, SX126X_CMD_SET_TX_PARAMS, tx, sizeof(tx));
}

void SX1262_configDefault(SX1262Config_t *config)
{
    if (config == RT_NULL) return;
    memset(config, 0, sizeof(*config));
    config->frequency_hz = RADIOLIB_DEFAULT_FREQUENCY;
    config->bandwidth = RADIOLIB_SX126X_BW_125;
    config->spreading_factor = 9;
    config->coding_rate = 7;
    config->sync_word = 0x12;
    config->output_power_dbm = 14;
    config->preamble_length = 8;
    config->crc_enabled = true;
#ifdef RADIOLIB_SX1262_USE_DCDC
    config->use_dcdc = true;
#endif
#ifdef RADIOLIB_SX1262_USE_DIO2_RF_SWITCH
    config->use_dio2_rf_switch = true;
#endif
    config->tcxo_voltage_mv = RADIOLIB_SX1262_TCXO_VOLTAGE_MV;
    config->tcxo_delay_ms = RADIOLIB_SX1262_TCXO_DELAY_MS;
    config->tx_timeout_ms = 5000;
}

static int16_t SX126x_phyTransmit(void *radio, const uint8_t *data,
                                  size_t length)
{
    return SX126x_transmit((SX126x_t *)radio, data, length);
}

static int16_t SX126x_phyReceive(void *radio, uint8_t *data, size_t *length,
                                 uint32_t timeout_ms)
{
    return SX126x_receive((SX126x_t *)radio, data, length, timeout_ms);
}

static int16_t SX126x_phyStartTransmit(void *radio, const uint8_t *data,
                                       size_t length)
{
    return SX126x_startTransmit((SX126x_t *)radio, data, length);
}

static int16_t SX126x_phyStartReceive(void *radio, uint32_t timeout_ms)
{
    return SX126x_startReceive((SX126x_t *)radio, timeout_ms);
}

static int16_t SX126x_phyReadData(void *radio, uint8_t *data, size_t *length)
{
    return SX126x_readData((SX126x_t *)radio, data, length);
}

static int16_t SX126x_phyStandby(void *radio)
{
    return SX126x_standby((SX126x_t *)radio);
}

static int16_t SX126x_phySleep(void *radio, bool retain_config)
{
    return SX126x_sleep((SX126x_t *)radio, retain_config);
}

static int16_t SX126x_phySetFrequency(void *radio, uint32_t frequency_hz)
{
    return SX126x_setFrequency((SX126x_t *)radio, frequency_hz);
}

static int16_t SX126x_phySetPower(void *radio, int8_t power_dbm)
{
    return SX126x_setOutputPower((SX126x_t *)radio, power_dbm);
}

static int16_t SX126x_phyScan(void *radio)
{
    return SX126x_scanChannel((SX126x_t *)radio);
}

static const PhysicalLayerOps_t sx126x_phy_ops = {
    SX126x_phyTransmit, SX126x_phyReceive, SX126x_phyStartTransmit,
    SX126x_phyStartReceive, SX126x_phyReadData, SX126x_phyStandby,
    SX126x_phySleep, SX126x_phySetFrequency, SX126x_phySetPower,
    SX126x_phyScan
};

int16_t SX1262_begin(SX126x_t *radio, Module_t *module)
{
    SX1262Config_t config;
    SX1262_configDefault(&config);
    return SX1262_beginConfig(radio, module, &config);
}

int16_t SX1262_beginConfig(SX126x_t *radio, Module_t *module,
                           const SX1262Config_t *config)
{
    uint8_t data[2];
    uint8_t status;
    int16_t state;

    if ((radio == RT_NULL) || (module == RT_NULL) || (config == RT_NULL) ||
        !module->initialized) {
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    }
    memset(radio, 0, sizeof(*radio));
    radio->module = module;
    radio->config = *config;
    radio->phy.radio = radio;
    radio->phy.ops = &sx126x_phy_ops;

    if ((config->spreading_factor < 5U) || (config->spreading_factor > 12U))
        return RADIOLIB_ERR_INVALID_SPREADING_FACTOR;
    if ((config->coding_rate < 5U) || (config->coding_rate > 8U))
        return RADIOLIB_ERR_INVALID_CODING_RATE;
    if (config->bandwidth > RADIOLIB_SX126X_BW_500)
        return RADIOLIB_ERR_INVALID_BANDWIDTH;
    if ((config->output_power_dbm < -9) || (config->output_power_dbm > 22))
        return RADIOLIB_ERR_INVALID_OUTPUT_POWER;

    state = Module_reset(module);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_read(radio, SX126X_CMD_GET_STATUS, &status, RT_NULL, 0);
    if (state != RADIOLIB_ERR_NONE) return state;
    if ((status == 0x00U) || (status == 0xFFU))
        return RADIOLIB_ERR_CHIP_NOT_FOUND;

    data[0] = SX126X_STANDBY_RC;
    state = SX126x_write(radio, SX126X_CMD_SET_STANDBY, data, 1);
    if (state != RADIOLIB_ERR_NONE) return state;
    data[0] = 0;
    data[1] = 0;
    state = SX126x_write(radio, SX126X_CMD_CLEAR_DEVICE_ERRORS, data, 2);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_configureTcxo(radio);
    if (state != RADIOLIB_ERR_NONE) return state;
    data[0] = config->use_dcdc ? SX126X_REGULATOR_DCDC : SX126X_REGULATOR_LDO;
    state = SX126x_write(radio, SX126X_CMD_SET_REGULATOR_MODE, data, 1);
    if (state != RADIOLIB_ERR_NONE) return state;
    data[0] = config->use_dio2_rf_switch ? 1U : 0U;
    state = SX126x_write(radio, SX126X_CMD_SET_DIO2_RF_SWITCH, data, 1);
    if (state != RADIOLIB_ERR_NONE) return state;
    data[0] = 0;
    data[1] = 0;
    state = SX126x_write(radio, SX126X_CMD_SET_BUFFER_BASE, data, 2);
    if (state != RADIOLIB_ERR_NONE) return state;
    data[0] = SX126X_PACKET_TYPE_LORA;
    state = SX126x_write(radio, SX126X_CMD_SET_PACKET_TYPE, data, 1);
    if (state != RADIOLIB_ERR_NONE) return state;

    radio->initialized = true;
    state = SX126x_setFrequency(radio, config->frequency_hz);
    if (state != RADIOLIB_ERR_NONE) goto begin_failed;
    state = SX126x_applyModulation(radio);
    if (state != RADIOLIB_ERR_NONE) goto begin_failed;
    state = SX126x_applyPacket(radio, 255);
    if (state != RADIOLIB_ERR_NONE) goto begin_failed;
    state = SX126x_setSyncWord(radio, config->sync_word);
    if (state != RADIOLIB_ERR_NONE) goto begin_failed;
    state = SX126x_configurePower(radio);
    if (state != RADIOLIB_ERR_NONE) goto begin_failed;
    state = SX126x_clearIrqStatus(radio, SX126X_IRQ_ALL);
    if (state != RADIOLIB_ERR_NONE) goto begin_failed;
    state = SX126x_setIrqParams(radio, SX126X_IRQ_ALL, 0);
    if (state != RADIOLIB_ERR_NONE) goto begin_failed;
    return RADIOLIB_ERR_NONE;

begin_failed:
    radio->initialized = false;
    return state;
}

int16_t SX126x_startTransmit(SX126x_t *radio, const uint8_t *data,
                             size_t length)
{
    int16_t state;

    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    if ((data == RT_NULL) || (length == 0U))
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    if (length > 255U) return RADIOLIB_ERR_PACKET_TOO_LONG;
    if (radio->implicit_header && (length != radio->implicit_length))
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    state = SX126x_standby(radio);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_applyPacket(radio, (uint8_t)length);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_clearIrqStatus(radio, SX126X_IRQ_ALL);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_setIrqParams(radio, SX126X_IRQ_TX_DONE | SX126X_IRQ_TIMEOUT,
                                SX126X_IRQ_TX_DONE | SX126X_IRQ_TIMEOUT);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = Module_writeBuffer(radio->module, 0, data, length);
    if (state != RADIOLIB_ERR_NONE) return state;
    return SX126x_setTimeoutCommand(radio, SX126X_CMD_SET_TX,
                                    radio->config.tx_timeout_ms, false);
}

int16_t SX126x_finishTransmit(SX126x_t *radio)
{
    uint16_t irq;
    int16_t state = SX126x_getIrqStatus(radio, &irq);
    if (state != RADIOLIB_ERR_NONE) return state;
    SX126x_clearIrqStatus(radio, irq);
    SX126x_standby(radio);
    if ((irq & SX126X_IRQ_TIMEOUT) != 0U) return RADIOLIB_ERR_TX_TIMEOUT;
    return ((irq & SX126X_IRQ_TX_DONE) != 0U) ? RADIOLIB_ERR_NONE :
           RADIOLIB_ERR_INVALID_STATE;
}

int16_t SX126x_transmit(SX126x_t *radio, const uint8_t *data, size_t length)
{
    uint16_t irq;
    int16_t state = SX126x_startTransmit(radio, data, length);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_waitForIrq(radio, SX126X_IRQ_TX_DONE,
                              radio->config.tx_timeout_ms + 100U, &irq);
    if (state != RADIOLIB_ERR_NONE) {
        SX126x_standby(radio);
        return RADIOLIB_ERR_TX_TIMEOUT;
    }
    SX126x_clearIrqStatus(radio, irq);
    SX126x_standby(radio);
    return ((irq & SX126X_IRQ_TX_DONE) != 0U) ? RADIOLIB_ERR_NONE :
           RADIOLIB_ERR_TX_TIMEOUT;
}

int16_t SX126x_startReceive(SX126x_t *radio, uint32_t timeout_ms)
{
    uint16_t mask = SX126X_IRQ_RX_DONE | SX126X_IRQ_TIMEOUT |
                    SX126X_IRQ_CRC_ERROR | SX126X_IRQ_HEADER_ERROR;
    int16_t state;

    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    state = SX126x_standby(radio);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_applyPacket(radio, 255);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_clearIrqStatus(radio, SX126X_IRQ_ALL);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_setIrqParams(radio, mask, mask);
    if (state != RADIOLIB_ERR_NONE) return state;
    return SX126x_setTimeoutCommand(radio, SX126X_CMD_SET_RX, timeout_ms,
                                    timeout_ms == 0U);
}

int16_t SX126x_readData(SX126x_t *radio, uint8_t *data, size_t *length)
{
    uint8_t status[2];
    uint16_t irq;
    size_t capacity;
    int16_t state;

    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    if ((data == RT_NULL) || (length == RT_NULL) || (*length == 0U))
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    capacity = *length;
    state = SX126x_getIrqStatus(radio, &irq);
    if (state != RADIOLIB_ERR_NONE) return state;
    if ((irq & SX126X_IRQ_CRC_ERROR) != 0U) {
        SX126x_clearIrqStatus(radio, irq);
        return RADIOLIB_ERR_CRC_MISMATCH;
    }
    if ((irq & SX126X_IRQ_HEADER_ERROR) != 0U) {
        SX126x_clearIrqStatus(radio, irq);
        return RADIOLIB_ERR_HEADER_DAMAGED;
    }
    if ((irq & SX126X_IRQ_TIMEOUT) != 0U) {
        SX126x_clearIrqStatus(radio, irq);
        return RADIOLIB_ERR_RX_TIMEOUT;
    }
    if ((irq & SX126X_IRQ_RX_DONE) == 0U)
        return RADIOLIB_ERR_INVALID_STATE;

    state = SX126x_read(radio, SX126X_CMD_GET_RX_BUFFER_STATUS, RT_NULL,
                        status, sizeof(status));
    if (state != RADIOLIB_ERR_NONE) return state;
    radio->rx_packet_length = status[0];
    if (radio->rx_packet_length > capacity) {
        *length = radio->rx_packet_length;
        SX126x_clearIrqStatus(radio, irq);
        return RADIOLIB_ERR_PACKET_TOO_LONG;
    }
    if (radio->rx_packet_length > 0U) {
        state = Module_readBuffer(radio->module, status[1], data,
                                  radio->rx_packet_length);
        if (state != RADIOLIB_ERR_NONE) return state;
    }
    *length = radio->rx_packet_length;
    state = SX126x_readPacketStatus(radio);
    SX126x_clearIrqStatus(radio, irq);
    return state;
}

int16_t SX126x_receive(SX126x_t *radio, uint8_t *data, size_t *length,
                       uint32_t timeout_ms)
{
    uint16_t irq = 0;
    int16_t state;

    if (timeout_ms == 0U) timeout_ms = 5000U;
    state = SX126x_startReceive(radio, timeout_ms);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_waitForIrq(radio, SX126X_IRQ_RX_DONE, timeout_ms + 100U,
                              &irq);
    if (state != RADIOLIB_ERR_NONE) {
        SX126x_standby(radio);
        return RADIOLIB_ERR_RX_TIMEOUT;
    }
    state = SX126x_readData(radio, data, length);
    SX126x_standby(radio);
    return state;
}

int16_t SX126x_scanChannel(SX126x_t *radio)
{
    uint8_t data[7] = {0x02, 0x16, 0x0A, 0x00, 0, 0, 0};
    uint16_t irq = 0;
    int16_t state;

    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    state = SX126x_clearIrqStatus(radio, SX126X_IRQ_ALL);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_setIrqParams(radio, SX126X_IRQ_CAD_DONE |
                                SX126X_IRQ_CAD_DETECTED,
                                SX126X_IRQ_CAD_DONE |
                                SX126X_IRQ_CAD_DETECTED);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_write(radio, SX126X_CMD_SET_CAD_PARAMS, data, sizeof(data));
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_write(radio, SX126X_CMD_SET_CAD, RT_NULL, 0);
    if (state != RADIOLIB_ERR_NONE) return state;
    state = SX126x_waitForIrq(radio, SX126X_IRQ_CAD_DONE, 1000, &irq);
    SX126x_clearIrqStatus(radio, irq);
    SX126x_standby(radio);
    if (state != RADIOLIB_ERR_NONE) return state;
    return ((irq & SX126X_IRQ_CAD_DETECTED) != 0U) ?
           RADIOLIB_LORA_DETECTED : RADIOLIB_ERR_NONE;
}

int16_t SX126x_standby(SX126x_t *radio)
{
    uint8_t mode = SX126X_STANDBY_RC;
    uint8_t status;
    int16_t state;

    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    /* GetStatus also wakes a radio that was put into warm sleep. */
    state = SX126x_read(radio, SX126X_CMD_GET_STATUS, &status, RT_NULL, 0);
    if (state != RADIOLIB_ERR_NONE) return state;
    return SX126x_write(radio, SX126X_CMD_SET_STANDBY, &mode, 1);
}

int16_t SX126x_sleep(SX126x_t *radio, bool retain_config)
{
    uint8_t value = retain_config ? 0x04U : 0x00U;
    int16_t state;
    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    state = Module_writeCommand(radio->module, SX126X_CMD_SET_SLEEP, &value, 1,
                                false);
    if ((state == RADIOLIB_ERR_NONE) && !retain_config) {
        radio->initialized = false;
    }
    return state;
}

int16_t SX126x_setFrequency(SX126x_t *radio, uint32_t frequency_hz)
{
    uint8_t cal[2];
    uint8_t data[4];
    uint32_t raw;
    int16_t state;

    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    if ((frequency_hz < 150000000U) || (frequency_hz > 960000000U))
        return RADIOLIB_ERR_INVALID_FREQUENCY;
    if (frequency_hz > 900000000U) { cal[0] = 0xE1; cal[1] = 0xE9; }
    else if (frequency_hz > 850000000U) { cal[0] = 0xD7; cal[1] = 0xDB; }
    else if (frequency_hz > 770000000U) { cal[0] = 0xC1; cal[1] = 0xC5; }
    else if (frequency_hz > 460000000U) { cal[0] = 0x75; cal[1] = 0x81; }
    else { cal[0] = 0x6B; cal[1] = 0x6F; }
    state = SX126x_write(radio, SX126X_CMD_CALIBRATE_IMAGE, cal, sizeof(cal));
    if (state != RADIOLIB_ERR_NONE) return state;
    raw = (uint32_t)(((uint64_t)frequency_hz << 25) / 32000000U);
    data[0] = (uint8_t)(raw >> 24);
    data[1] = (uint8_t)(raw >> 16);
    data[2] = (uint8_t)(raw >> 8);
    data[3] = (uint8_t)raw;
    state = SX126x_write(radio, SX126X_CMD_SET_RF_FREQUENCY, data, sizeof(data));
    if (state == RADIOLIB_ERR_NONE) radio->config.frequency_hz = frequency_hz;
    return state;
}

int16_t SX126x_setBandwidth(SX126x_t *radio, SX126xBandwidth_t bandwidth)
{
    SX126xBandwidth_t old;
    int16_t state;
    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    if (bandwidth > RADIOLIB_SX126X_BW_500)
        return RADIOLIB_ERR_INVALID_BANDWIDTH;
    old = radio->config.bandwidth;
    radio->config.bandwidth = bandwidth;
    state = SX126x_applyModulation(radio);
    if (state != RADIOLIB_ERR_NONE) radio->config.bandwidth = old;
    return state;
}

int16_t SX126x_setSpreadingFactor(SX126x_t *radio, uint8_t sf)
{
    uint8_t old;
    int16_t state;
    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    if ((sf < 5U) || (sf > 12U)) return RADIOLIB_ERR_INVALID_SPREADING_FACTOR;
    old = radio->config.spreading_factor;
    radio->config.spreading_factor = sf;
    state = SX126x_applyModulation(radio);
    if (state != RADIOLIB_ERR_NONE) radio->config.spreading_factor = old;
    return state;
}

int16_t SX126x_setCodingRate(SX126x_t *radio, uint8_t denominator)
{
    uint8_t old;
    int16_t state;
    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    if ((denominator < 5U) || (denominator > 8U))
        return RADIOLIB_ERR_INVALID_CODING_RATE;
    old = radio->config.coding_rate;
    radio->config.coding_rate = denominator;
    state = SX126x_applyModulation(radio);
    if (state != RADIOLIB_ERR_NONE) radio->config.coding_rate = old;
    return state;
}

int16_t SX126x_setOutputPower(SX126x_t *radio, int8_t power_dbm)
{
    int8_t old;
    int16_t state;
    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    if ((power_dbm < -9) || (power_dbm > 22))
        return RADIOLIB_ERR_INVALID_OUTPUT_POWER;
    old = radio->config.output_power_dbm;
    radio->config.output_power_dbm = power_dbm;
    state = SX126x_configurePower(radio);
    if (state != RADIOLIB_ERR_NONE) radio->config.output_power_dbm = old;
    return state;
}

int16_t SX126x_setPreambleLength(SX126x_t *radio, uint16_t length)
{
    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    if (length == 0U) return RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH;
    radio->config.preamble_length = length;
    return SX126x_applyPacket(radio, 255);
}

int16_t SX126x_setSyncWord(SX126x_t *radio, uint8_t sync_word)
{
    uint8_t data[2];
    int16_t state;
    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    data[0] = (uint8_t)((sync_word & 0xF0U) | 0x04U);
    data[1] = (uint8_t)(((sync_word & 0x0FU) << 4) | 0x04U);
    state = Module_writeRegister(radio->module, SX126X_REG_LORA_SYNC_WORD_MSB,
                                 data, sizeof(data));
    if (state == RADIOLIB_ERR_NONE) radio->config.sync_word = sync_word;
    return state;
}

int16_t SX126x_setCRC(SX126x_t *radio, bool enabled)
{
    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    radio->config.crc_enabled = enabled;
    return SX126x_applyPacket(radio, 255);
}

int16_t SX126x_setInvertIQ(SX126x_t *radio, bool enabled)
{
    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    radio->config.invert_iq = enabled;
    return SX126x_applyPacket(radio, 255);
}

int16_t SX126x_implicitHeader(SX126x_t *radio, uint8_t length)
{
    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    if (length == 0U) return RADIOLIB_ERR_INVALID_ARGUMENT;
    radio->implicit_header = true;
    radio->implicit_length = length;
    return SX126x_applyPacket(radio, length);
}

int16_t SX126x_explicitHeader(SX126x_t *radio)
{
    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    radio->implicit_header = false;
    radio->implicit_length = 0;
    return SX126x_applyPacket(radio, 255);
}

int16_t SX126x_setDio1Action(SX126x_t *radio, RadioLibIrqAction_t action,
                             void *context)
{
    if ((radio == RT_NULL) || !radio->initialized)
        return RADIOLIB_ERR_NOT_INITIALIZED;
    return Module_setIrqAction(radio->module, action, context);
}

void SX126x_clearDio1Action(SX126x_t *radio)
{
    if ((radio != RT_NULL) && radio->initialized)
        Module_clearIrqAction(radio->module);
}

uint8_t SX126x_getPacketLength(const SX126x_t *radio)
{
    return (radio != RT_NULL) ? radio->rx_packet_length : 0;
}

int16_t SX126x_getRSSI(const SX126x_t *radio)
{
    return (radio != RT_NULL) ? radio->packet_rssi_dbm : 0;
}

int8_t SX126x_getSNR(const SX126x_t *radio)
{
    return (radio != RT_NULL) ? radio->packet_snr_db : 0;
}

int32_t SX126x_getFrequencyError(const SX126x_t *radio)
{
    return (radio != RT_NULL) ? radio->frequency_error_hz : 0;
}

PhysicalLayer_t *SX126x_getPhysicalLayer(SX126x_t *radio)
{
    return (radio != RT_NULL) ? &radio->phy : RT_NULL;
}
