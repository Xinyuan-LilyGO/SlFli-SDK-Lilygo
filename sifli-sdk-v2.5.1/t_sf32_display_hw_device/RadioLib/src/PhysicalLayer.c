#include "PhysicalLayer.h"

static int16_t PhysicalLayer_validate(const PhysicalLayer_t *phy)
{
    if ((phy == NULL) || (phy->radio == NULL) || (phy->ops == NULL)) {
        return RADIOLIB_ERR_NOT_INITIALIZED;
    }
    return RADIOLIB_ERR_NONE;
}

#define PHY_CALL0(member)                                                     \
    do {                                                                      \
        int16_t state = PhysicalLayer_validate(phy);                          \
        if (state != RADIOLIB_ERR_NONE) return state;                         \
        if (phy->ops->member == NULL) return RADIOLIB_ERR_UNSUPPORTED;        \
        return phy->ops->member(phy->radio);                                  \
    } while (0)

int16_t PhysicalLayer_transmit(PhysicalLayer_t *phy, const uint8_t *data,
                               size_t length)
{
    int16_t state = PhysicalLayer_validate(phy);
    if (state != RADIOLIB_ERR_NONE) return state;
    if (phy->ops->transmit == NULL) return RADIOLIB_ERR_UNSUPPORTED;
    return phy->ops->transmit(phy->radio, data, length);
}

int16_t PhysicalLayer_receive(PhysicalLayer_t *phy, uint8_t *data,
                              size_t *length, uint32_t timeout_ms)
{
    int16_t state = PhysicalLayer_validate(phy);
    if (state != RADIOLIB_ERR_NONE) return state;
    if (phy->ops->receive == NULL) return RADIOLIB_ERR_UNSUPPORTED;
    return phy->ops->receive(phy->radio, data, length, timeout_ms);
}

int16_t PhysicalLayer_startTransmit(PhysicalLayer_t *phy,
                                    const uint8_t *data, size_t length)
{
    int16_t state = PhysicalLayer_validate(phy);
    if (state != RADIOLIB_ERR_NONE) return state;
    if (phy->ops->start_transmit == NULL) return RADIOLIB_ERR_UNSUPPORTED;
    return phy->ops->start_transmit(phy->radio, data, length);
}

int16_t PhysicalLayer_startReceive(PhysicalLayer_t *phy,
                                   uint32_t timeout_ms)
{
    int16_t state = PhysicalLayer_validate(phy);
    if (state != RADIOLIB_ERR_NONE) return state;
    if (phy->ops->start_receive == NULL) return RADIOLIB_ERR_UNSUPPORTED;
    return phy->ops->start_receive(phy->radio, timeout_ms);
}

int16_t PhysicalLayer_readData(PhysicalLayer_t *phy, uint8_t *data,
                               size_t *length)
{
    int16_t state = PhysicalLayer_validate(phy);
    if (state != RADIOLIB_ERR_NONE) return state;
    if (phy->ops->read_data == NULL) return RADIOLIB_ERR_UNSUPPORTED;
    return phy->ops->read_data(phy->radio, data, length);
}

int16_t PhysicalLayer_standby(PhysicalLayer_t *phy)
{
    PHY_CALL0(standby);
}

int16_t PhysicalLayer_sleep(PhysicalLayer_t *phy, bool retain_config)
{
    int16_t state = PhysicalLayer_validate(phy);
    if (state != RADIOLIB_ERR_NONE) return state;
    if (phy->ops->sleep == NULL) return RADIOLIB_ERR_UNSUPPORTED;
    return phy->ops->sleep(phy->radio, retain_config);
}

int16_t PhysicalLayer_setFrequency(PhysicalLayer_t *phy,
                                   uint32_t frequency_hz)
{
    int16_t state = PhysicalLayer_validate(phy);
    if (state != RADIOLIB_ERR_NONE) return state;
    if (phy->ops->set_frequency == NULL) return RADIOLIB_ERR_UNSUPPORTED;
    return phy->ops->set_frequency(phy->radio, frequency_hz);
}

int16_t PhysicalLayer_setOutputPower(PhysicalLayer_t *phy, int8_t power_dbm)
{
    int16_t state = PhysicalLayer_validate(phy);
    if (state != RADIOLIB_ERR_NONE) return state;
    if (phy->ops->set_output_power == NULL) return RADIOLIB_ERR_UNSUPPORTED;
    return phy->ops->set_output_power(phy->radio, power_dbm);
}

int16_t PhysicalLayer_scanChannel(PhysicalLayer_t *phy)
{
    PHY_CALL0(scan_channel);
}
