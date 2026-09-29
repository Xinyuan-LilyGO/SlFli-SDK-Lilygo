#ifndef RADIOLIB_MODULE_H
#define RADIOLIB_MODULE_H

#include "RadioLibTypes.h"

#include <rtdevice.h>
#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*RadioLibBoardInit_t)(void);

typedef struct {
    const char *spi_bus_name;
    const char *spi_device_name;
    int32_t cs;
    int32_t irq;
    int32_t reset;
    int32_t busy;
    int32_t enable;
    uint32_t spi_frequency;
    uint32_t busy_timeout_ms;
    RadioLibBoardInit_t board_init;
} ModuleConfig_t;

typedef struct {
    struct rt_spi_device *spi;
    ModuleConfig_t config;
    rt_mutex_t lock;
    RadioLibIrqAction_t irq_action;
    void *irq_context;
    uint8_t tx_buffer[258];
    uint8_t rx_buffer[258];
    bool initialized;
} Module_t;

void Module_configDefault(ModuleConfig_t *config);
void RadioLib_boardInitTDisplaySF32(void);
int16_t Module_init(Module_t *module, const ModuleConfig_t *config);
void Module_deinit(Module_t *module);
int16_t Module_reset(Module_t *module);
int16_t Module_waitForBusy(Module_t *module);
int16_t Module_setIrqAction(Module_t *module, RadioLibIrqAction_t action,
                            void *context);
void Module_clearIrqAction(Module_t *module);

int16_t Module_writeCommand(Module_t *module, uint8_t command,
                            const uint8_t *data, size_t length,
                            bool wait_after);
int16_t Module_readCommand(Module_t *module, uint8_t command,
                           uint8_t *status, uint8_t *data, size_t length);
int16_t Module_writeRegister(Module_t *module, uint16_t address,
                             const uint8_t *data, size_t length);
int16_t Module_readRegister(Module_t *module, uint16_t address,
                            uint8_t *data, size_t length);
int16_t Module_writeBuffer(Module_t *module, uint8_t offset,
                           const uint8_t *data, size_t length);
int16_t Module_readBuffer(Module_t *module, uint8_t offset,
                          uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif
