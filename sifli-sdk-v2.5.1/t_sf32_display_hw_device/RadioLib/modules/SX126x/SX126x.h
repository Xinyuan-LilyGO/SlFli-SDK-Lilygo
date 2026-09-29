#ifndef RADIOLIB_SX126X_H
#define RADIOLIB_SX126X_H

#include "Module.h"
#include "PhysicalLayer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RADIOLIB_SX126X_BW_7_8 = 0,
    RADIOLIB_SX126X_BW_10_4,
    RADIOLIB_SX126X_BW_15_6,
    RADIOLIB_SX126X_BW_20_8,
    RADIOLIB_SX126X_BW_31_25,
    RADIOLIB_SX126X_BW_41_7,
    RADIOLIB_SX126X_BW_62_5,
    RADIOLIB_SX126X_BW_125,
    RADIOLIB_SX126X_BW_250,
    RADIOLIB_SX126X_BW_500
} SX126xBandwidth_t;

typedef struct {
    uint32_t frequency_hz;
    SX126xBandwidth_t bandwidth;
    uint8_t spreading_factor;
    uint8_t coding_rate;
    uint8_t sync_word;
    int8_t output_power_dbm;
    uint16_t preamble_length;
    bool crc_enabled;
    bool invert_iq;
    bool use_dcdc;
    bool use_dio2_rf_switch;
    uint16_t tcxo_voltage_mv;
    uint16_t tcxo_delay_ms;
    uint32_t tx_timeout_ms;
} SX1262Config_t;

typedef struct {
    Module_t *module;
    PhysicalLayer_t phy;
    SX1262Config_t config;
    uint8_t rx_packet_length;
    int16_t packet_rssi_dbm;
    int8_t packet_snr_db;
    int32_t frequency_error_hz;
    bool initialized;
    bool implicit_header;
    uint8_t implicit_length;
} SX126x_t;

void SX1262_configDefault(SX1262Config_t *config);
int16_t SX1262_begin(SX126x_t *radio, Module_t *module);
int16_t SX1262_beginConfig(SX126x_t *radio, Module_t *module,
                           const SX1262Config_t *config);

int16_t SX126x_transmit(SX126x_t *radio, const uint8_t *data, size_t length);
int16_t SX126x_receive(SX126x_t *radio, uint8_t *data, size_t *length,
                       uint32_t timeout_ms);
int16_t SX126x_startTransmit(SX126x_t *radio, const uint8_t *data,
                             size_t length);
int16_t SX126x_finishTransmit(SX126x_t *radio);
int16_t SX126x_startReceive(SX126x_t *radio, uint32_t timeout_ms);
int16_t SX126x_readData(SX126x_t *radio, uint8_t *data, size_t *length);
int16_t SX126x_scanChannel(SX126x_t *radio);

int16_t SX126x_standby(SX126x_t *radio);
int16_t SX126x_sleep(SX126x_t *radio, bool retain_config);
int16_t SX126x_setFrequency(SX126x_t *radio, uint32_t frequency_hz);
int16_t SX126x_setBandwidth(SX126x_t *radio, SX126xBandwidth_t bandwidth);
int16_t SX126x_setSpreadingFactor(SX126x_t *radio, uint8_t sf);
int16_t SX126x_setCodingRate(SX126x_t *radio, uint8_t denominator);
int16_t SX126x_setOutputPower(SX126x_t *radio, int8_t power_dbm);
int16_t SX126x_setPreambleLength(SX126x_t *radio, uint16_t length);
int16_t SX126x_setSyncWord(SX126x_t *radio, uint8_t sync_word);
int16_t SX126x_setCRC(SX126x_t *radio, bool enabled);
int16_t SX126x_setInvertIQ(SX126x_t *radio, bool enabled);
int16_t SX126x_implicitHeader(SX126x_t *radio, uint8_t length);
int16_t SX126x_explicitHeader(SX126x_t *radio);
int16_t SX126x_setDio1Action(SX126x_t *radio, RadioLibIrqAction_t action,
                             void *context);
void SX126x_clearDio1Action(SX126x_t *radio);

uint8_t SX126x_getPacketLength(const SX126x_t *radio);
int16_t SX126x_getRSSI(const SX126x_t *radio);
int8_t SX126x_getSNR(const SX126x_t *radio);
int32_t SX126x_getFrequencyError(const SX126x_t *radio);
PhysicalLayer_t *SX126x_getPhysicalLayer(SX126x_t *radio);

#ifdef __cplusplus
}
#endif

#endif
