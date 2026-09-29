#ifndef RADIOLIB_PHYSICAL_LAYER_H
#define RADIOLIB_PHYSICAL_LAYER_H

#include "RadioLibTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PhysicalLayer PhysicalLayer_t;

typedef struct {
    int16_t (*transmit)(void *radio, const uint8_t *data, size_t length);
    int16_t (*receive)(void *radio, uint8_t *data, size_t *length,
                       uint32_t timeout_ms);
    int16_t (*start_transmit)(void *radio, const uint8_t *data, size_t length);
    int16_t (*start_receive)(void *radio, uint32_t timeout_ms);
    int16_t (*read_data)(void *radio, uint8_t *data, size_t *length);
    int16_t (*standby)(void *radio);
    int16_t (*sleep)(void *radio, bool retain_config);
    int16_t (*set_frequency)(void *radio, uint32_t frequency_hz);
    int16_t (*set_output_power)(void *radio, int8_t power_dbm);
    int16_t (*scan_channel)(void *radio);
} PhysicalLayerOps_t;

struct PhysicalLayer {
    void *radio;
    const PhysicalLayerOps_t *ops;
};

int16_t PhysicalLayer_transmit(PhysicalLayer_t *phy, const uint8_t *data,
                               size_t length);
int16_t PhysicalLayer_receive(PhysicalLayer_t *phy, uint8_t *data,
                              size_t *length, uint32_t timeout_ms);
int16_t PhysicalLayer_startTransmit(PhysicalLayer_t *phy,
                                    const uint8_t *data, size_t length);
int16_t PhysicalLayer_startReceive(PhysicalLayer_t *phy,
                                   uint32_t timeout_ms);
int16_t PhysicalLayer_readData(PhysicalLayer_t *phy, uint8_t *data,
                               size_t *length);
int16_t PhysicalLayer_standby(PhysicalLayer_t *phy);
int16_t PhysicalLayer_sleep(PhysicalLayer_t *phy, bool retain_config);
int16_t PhysicalLayer_setFrequency(PhysicalLayer_t *phy,
                                   uint32_t frequency_hz);
int16_t PhysicalLayer_setOutputPower(PhysicalLayer_t *phy,
                                     int8_t power_dbm);
int16_t PhysicalLayer_scanChannel(PhysicalLayer_t *phy);

#ifdef __cplusplus
}
#endif

#endif
