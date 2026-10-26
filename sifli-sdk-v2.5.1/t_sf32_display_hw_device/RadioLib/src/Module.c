#include "Module.h"

#include <board.h>
#include <drv_spi.h>
#include <drivers/pin.h>
#include <drivers/spi.h>
#include <string.h>

#ifndef RADIOLIB_SPI_BUS_NAME
#define RADIOLIB_SPI_BUS_NAME "spi1"
#endif
#ifndef RADIOLIB_SPI_DEVICE_NAME
#define RADIOLIB_SPI_DEVICE_NAME "radiolib"
#endif
#ifndef RADIOLIB_SPI_FREQUENCY
#define RADIOLIB_SPI_FREQUENCY 10000000
#endif
#ifndef RADIOLIB_ENABLE_PIN
#define RADIOLIB_ENABLE_PIN 26
#endif
#ifndef RADIOLIB_NSS_PIN
#define RADIOLIB_NSS_PIN 29
#endif
#ifndef RADIOLIB_DIO1_PIN
#define RADIOLIB_DIO1_PIN 30
#endif
#ifndef RADIOLIB_BUSY_PIN
#define RADIOLIB_BUSY_PIN 31
#endif
#ifndef RADIOLIB_RESET_PIN
#define RADIOLIB_RESET_PIN 32
#endif
#ifndef RADIOLIB_SX1262_BUSY_TIMEOUT_MS
#define RADIOLIB_SX1262_BUSY_TIMEOUT_MS 1000
#endif

#define RADIOLIB_CMD_WRITE_REGISTER 0x0D
#define RADIOLIB_CMD_READ_REGISTER  0x1D
#define RADIOLIB_CMD_WRITE_BUFFER   0x0E
#define RADIOLIB_CMD_READ_BUFFER    0x1E

static void Module_irqHandler(void *args)
{
    Module_t *module = (Module_t *)args;

    if ((module != RT_NULL) && (module->irq_action != RT_NULL)) {
        module->irq_action(module->irq_context);
    }
}

static int16_t Module_lock(Module_t *module)
{
    if ((module == RT_NULL) || !module->initialized || (module->lock == RT_NULL)) {
        return RADIOLIB_ERR_NOT_INITIALIZED;
    }
    return (rt_mutex_take(module->lock, RT_WAITING_FOREVER) == RT_EOK) ?
           RADIOLIB_ERR_NONE : RADIOLIB_ERR_RTTHREAD;
}

static void Module_unlock(Module_t *module)
{
    if ((module != RT_NULL) && (module->lock != RT_NULL)) {
        rt_mutex_release(module->lock);
    }
}

static void Module_select(Module_t *module)
{
    rt_pin_write(module->config.cs, PIN_LOW);
}

static void Module_deselect(Module_t *module)
{
    rt_pin_write(module->config.cs, PIN_HIGH);
}

static int16_t Module_beginSpiTransaction(Module_t *module)
{
    if ((module == RT_NULL) || (module->spi == RT_NULL)) {
        return RADIOLIB_ERR_NOT_INITIALIZED;
    }
    if (rt_spi_take_bus(module->spi) != RT_EOK) {
        return RADIOLIB_ERR_RTTHREAD;
    }
#ifdef RT_USING_SPI_MSD_CS_PIN
    rt_pin_write(MSD_SPI_CS_PIN, PIN_HIGH);
#endif
    rt_pin_mode(module->config.cs, PIN_MODE_OUTPUT);
    Module_select(module);
    return RADIOLIB_ERR_NONE;
}

static void Module_endSpiTransaction(Module_t *module)
{
    Module_deselect(module);
    rt_spi_release_bus(module->spi);
}

static int16_t Module_transfer(Module_t *module, const uint8_t *tx,
                               uint8_t *rx, size_t length, int16_t error)
{
    if (length == 0U) {
        return RADIOLIB_ERR_NONE;
    }
    return (rt_spi_transfer(module->spi, tx, rx, (rt_size_t)length) ==
            (rt_size_t)length) ? RADIOLIB_ERR_NONE : error;
}

void Module_configDefault(ModuleConfig_t *config)
{
    if (config == RT_NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->spi_bus_name = RADIOLIB_SPI_BUS_NAME;
    config->spi_device_name = RADIOLIB_SPI_DEVICE_NAME;
    config->cs = RADIOLIB_NSS_PIN;
    config->irq = RADIOLIB_DIO1_PIN;
    config->reset = RADIOLIB_RESET_PIN;
    config->busy = RADIOLIB_BUSY_PIN;
    config->enable = RADIOLIB_ENABLE_PIN;
    config->spi_frequency = RADIOLIB_SPI_FREQUENCY;
    config->busy_timeout_ms = RADIOLIB_SX1262_BUSY_TIMEOUT_MS;
}

int16_t Module_init(Module_t *module, const ModuleConfig_t *config)
{
    struct rt_spi_configuration spi_config;
    rt_err_t result;

    if ((module == RT_NULL) || (config == RT_NULL) ||
        (config->spi_bus_name == RT_NULL) ||
        (config->spi_device_name == RT_NULL) || (config->cs < 0) ||
        (config->irq < 0) || (config->reset < 0) || (config->busy < 0)) {
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    }

    memset(module, 0, sizeof(*module));
    module->config = *config;
    if (module->config.spi_frequency == 0U) {
        module->config.spi_frequency = 8000000U;
    }
    if (module->config.busy_timeout_ms == 0U) {
        module->config.busy_timeout_ms = 1000U;
    }

    if (module->config.board_init != RT_NULL) {
        module->config.board_init();
    }

    if (module->config.enable >= 0) {
        rt_pin_mode(module->config.enable, PIN_MODE_OUTPUT);
        rt_pin_write(module->config.enable, PIN_HIGH);
        rt_thread_mdelay(5);
    }
    rt_pin_mode(module->config.cs, PIN_MODE_OUTPUT);
    rt_pin_write(module->config.cs, PIN_HIGH);
    rt_pin_mode(module->config.reset, PIN_MODE_OUTPUT);
    rt_pin_write(module->config.reset, PIN_HIGH);
    rt_pin_mode(module->config.busy, PIN_MODE_INPUT);
    rt_pin_mode(module->config.irq, PIN_MODE_INPUT_PULLDOWN);

    module->spi = (struct rt_spi_device *)rt_device_find(
        module->config.spi_device_name);
    if (module->spi == RT_NULL) {
        result = rt_hw_spi_device_attach(module->config.spi_bus_name,
                                         module->config.spi_device_name);
        if (result != RT_EOK) {
            return RADIOLIB_ERR_RTTHREAD;
        }
        module->spi = (struct rt_spi_device *)rt_device_find(
            module->config.spi_device_name);
    }
    if (module->spi == RT_NULL) {
        return RADIOLIB_ERR_CHIP_NOT_FOUND;
    }

    result = rt_device_open(&module->spi->parent, RT_DEVICE_FLAG_RDWR);
    if ((result != RT_EOK) && (result != -RT_EBUSY)) {
        module->spi = RT_NULL;
        return RADIOLIB_ERR_RTTHREAD;
    }

    memset(&spi_config, 0, sizeof(spi_config));
    spi_config.data_width = 8;
    spi_config.mode = RT_SPI_MASTER | RT_SPI_MODE_0 | RT_SPI_MSB | RT_SPI_NO_CS;
    spi_config.max_hz = module->config.spi_frequency;
    if (rt_spi_configure(module->spi, &spi_config) != RT_EOK) {
        rt_device_close(&module->spi->parent);
        module->spi = RT_NULL;
        return RADIOLIB_ERR_RTTHREAD;
    }

    module->lock = rt_mutex_create("rl_mod", RT_IPC_FLAG_PRIO);
    if (module->lock == RT_NULL) {
        rt_device_close(&module->spi->parent);
        module->spi = RT_NULL;
        return RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED;
    }

    result = rt_pin_attach_irq(module->config.irq, PIN_IRQ_MODE_RISING,
                               Module_irqHandler, module);
    if (result != RT_EOK) {
        rt_mutex_delete(module->lock);
        rt_device_close(&module->spi->parent);
        module->lock = RT_NULL;
        module->spi = RT_NULL;
        return RADIOLIB_ERR_RTTHREAD;
    }
    rt_pin_irq_enable(module->config.irq, PIN_IRQ_DISABLE);
    module->initialized = true;
    return RADIOLIB_ERR_NONE;
}

void Module_deinit(Module_t *module)
{
    if (module == RT_NULL) {
        return;
    }

    if (module->config.irq >= 0) {
        rt_pin_irq_enable(module->config.irq, PIN_IRQ_DISABLE);
        rt_pin_detach_irq(module->config.irq);
    }
    if (module->spi != RT_NULL) {
        rt_device_close(&module->spi->parent);
    }
    if (module->lock != RT_NULL) {
        rt_mutex_delete(module->lock);
    }
    if (module->config.enable >= 0) {
        rt_pin_write(module->config.enable, PIN_LOW);
    }
    memset(module, 0, sizeof(*module));
}

int16_t Module_reset(Module_t *module)
{
    if ((module == RT_NULL) || !module->initialized) {
        return RADIOLIB_ERR_NOT_INITIALIZED;
    }
    rt_pin_mode(module->config.reset, PIN_MODE_OUTPUT);
    rt_pin_write(module->config.reset, PIN_HIGH);
    rt_thread_mdelay(1);
    rt_pin_write(module->config.reset, PIN_LOW);
    rt_thread_mdelay(2);
    rt_pin_write(module->config.reset, PIN_HIGH);
    rt_thread_mdelay(10);
    return Module_waitForBusy(module);
}

int16_t Module_waitForBusy(Module_t *module)
{
    rt_tick_t start;
    rt_tick_t timeout;

    if ((module == RT_NULL) || !module->initialized) {
        return RADIOLIB_ERR_NOT_INITIALIZED;
    }
    start = rt_tick_get();
    timeout = rt_tick_from_millisecond(module->config.busy_timeout_ms);
    while (rt_pin_read(module->config.busy) == PIN_HIGH) {
        if ((rt_tick_get() - start) >= timeout) {
            return RADIOLIB_ERR_BUSY_TIMEOUT;
        }
        rt_thread_mdelay(1);
    }
    return RADIOLIB_ERR_NONE;
}

int16_t Module_setIrqAction(Module_t *module, RadioLibIrqAction_t action,
                            void *context)
{
    if ((module == RT_NULL) || !module->initialized || (action == RT_NULL)) {
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    }
    rt_pin_irq_enable(module->config.irq, PIN_IRQ_DISABLE);
    module->irq_context = context;
    module->irq_action = action;
    return (rt_pin_irq_enable(module->config.irq, PIN_IRQ_ENABLE) == RT_EOK) ?
           RADIOLIB_ERR_NONE : RADIOLIB_ERR_RTTHREAD;
}

void Module_clearIrqAction(Module_t *module)
{
    if ((module == RT_NULL) || !module->initialized) {
        return;
    }
    rt_pin_irq_enable(module->config.irq, PIN_IRQ_DISABLE);
    module->irq_action = RT_NULL;
    module->irq_context = RT_NULL;
}

int16_t Module_writeCommand(Module_t *module, uint8_t command,
                            const uint8_t *data, size_t length,
                            bool wait_after)
{
    int16_t state;

    if ((length > 0U) && (data == RT_NULL)) {
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    }
    state = Module_lock(module);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }
    state = Module_waitForBusy(module);
    if (state == RADIOLIB_ERR_NONE) {
        state = Module_beginSpiTransaction(module);
        if (state == RADIOLIB_ERR_NONE) {
            state = Module_transfer(module, &command, module->rx_buffer, 1U,
                                    RADIOLIB_ERR_SPI_WRITE_FAILED);
            if ((state == RADIOLIB_ERR_NONE) && (length > 0U)) {
                state = Module_transfer(module, data, module->rx_buffer,
                                        length,
                                        RADIOLIB_ERR_SPI_WRITE_FAILED);
            }
            Module_endSpiTransaction(module);
        }
    }
    if ((state == RADIOLIB_ERR_NONE) && wait_after) {
        state = Module_waitForBusy(module);
    }
    Module_unlock(module);
    return state;
}

int16_t Module_readCommand(Module_t *module, uint8_t command,
                           uint8_t *status, uint8_t *data, size_t length)
{
    uint8_t response[17];
    int16_t state;

    if ((length > 16U) || ((length > 0U) && (data == RT_NULL))) {
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    }
    state = Module_lock(module);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }
    state = Module_waitForBusy(module);
    if (state == RADIOLIB_ERR_NONE) {
        state = Module_beginSpiTransaction(module);
        if (state == RADIOLIB_ERR_NONE) {
            state = Module_transfer(module, &command, module->rx_buffer, 1U,
                                    RADIOLIB_ERR_SPI_READ_FAILED);
            if (state == RADIOLIB_ERR_NONE) {
                memset(module->tx_buffer, 0, length + 1U);
                state = Module_transfer(module, module->tx_buffer, response,
                                        length + 1U,
                                        RADIOLIB_ERR_SPI_READ_FAILED);
            }
            Module_endSpiTransaction(module);
        }
    }
    if (state == RADIOLIB_ERR_NONE) {
        if (status != RT_NULL) {
            *status = response[0];
        }
        if (length > 0U) {
            memcpy(data, &response[1], length);
        }
        state = Module_waitForBusy(module);
    }
    Module_unlock(module);
    return state;
}

int16_t Module_writeRegister(Module_t *module, uint16_t address,
                             const uint8_t *data, size_t length)
{
    uint8_t header[3];
    int16_t state;

    if ((data == RT_NULL) || (length == 0U)) {
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    }
    header[0] = RADIOLIB_CMD_WRITE_REGISTER;
    header[1] = (uint8_t)(address >> 8);
    header[2] = (uint8_t)address;
    state = Module_lock(module);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }
    state = Module_waitForBusy(module);
    if (state == RADIOLIB_ERR_NONE) {
        state = Module_beginSpiTransaction(module);
        if (state == RADIOLIB_ERR_NONE) {
            state = Module_transfer(module, header, module->rx_buffer,
                                    sizeof(header),
                                    RADIOLIB_ERR_SPI_WRITE_FAILED);
            if (state == RADIOLIB_ERR_NONE) {
                state = Module_transfer(module, data, module->rx_buffer,
                                        length,
                                        RADIOLIB_ERR_SPI_WRITE_FAILED);
            }
            Module_endSpiTransaction(module);
        }
    }
    if (state == RADIOLIB_ERR_NONE) {
        state = Module_waitForBusy(module);
    }
    Module_unlock(module);
    return state;
}

int16_t Module_readRegister(Module_t *module, uint16_t address,
                            uint8_t *data, size_t length)
{
    uint8_t header[4];
    int16_t state;

    if ((data == RT_NULL) || (length == 0U)) {
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    }
    header[0] = RADIOLIB_CMD_READ_REGISTER;
    header[1] = (uint8_t)(address >> 8);
    header[2] = (uint8_t)address;
    header[3] = 0;
    state = Module_lock(module);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }
    state = Module_waitForBusy(module);
    if (state == RADIOLIB_ERR_NONE) {
        state = Module_beginSpiTransaction(module);
        if (state == RADIOLIB_ERR_NONE) {
            state = Module_transfer(module, header, module->rx_buffer,
                                    sizeof(header),
                                    RADIOLIB_ERR_SPI_READ_FAILED);
            if (state == RADIOLIB_ERR_NONE) {
                memset(module->tx_buffer, 0, length);
                state = Module_transfer(module, module->tx_buffer, data,
                                        length,
                                        RADIOLIB_ERR_SPI_READ_FAILED);
            }
            Module_endSpiTransaction(module);
        }
    }
    if (state == RADIOLIB_ERR_NONE) {
        state = Module_waitForBusy(module);
    }
    Module_unlock(module);
    return state;
}

int16_t Module_writeBuffer(Module_t *module, uint8_t offset,
                           const uint8_t *data, size_t length)
{
    int16_t state;

    if ((data == RT_NULL) || (length == 0U) ||
        (length > (256U - offset))) {
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    }
    state = Module_lock(module);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }
    state = Module_waitForBusy(module);
    if (state == RADIOLIB_ERR_NONE) {
        module->tx_buffer[0] = RADIOLIB_CMD_WRITE_BUFFER;
        module->tx_buffer[1] = offset;
        memcpy(&module->tx_buffer[2], data, length);
        state = Module_beginSpiTransaction(module);
        if (state == RADIOLIB_ERR_NONE) {
            state = Module_transfer(module, module->tx_buffer,
                                    module->rx_buffer, length + 2U,
                                    RADIOLIB_ERR_SPI_WRITE_FAILED);
            Module_endSpiTransaction(module);
        }
    }
    if (state == RADIOLIB_ERR_NONE) {
        state = Module_waitForBusy(module);
    }
    Module_unlock(module);
    return state;
}

int16_t Module_readBuffer(Module_t *module, uint8_t offset,
                          uint8_t *data, size_t length)
{
    uint8_t header[3];
    int16_t state;

    if ((data == RT_NULL) || (length == 0U) ||
        (length > (256U - offset))) {
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    }
    header[0] = RADIOLIB_CMD_READ_BUFFER;
    header[1] = offset;
    header[2] = 0;
    state = Module_lock(module);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }
    state = Module_waitForBusy(module);
    if (state == RADIOLIB_ERR_NONE) {
        state = Module_beginSpiTransaction(module);
        if (state == RADIOLIB_ERR_NONE) {
            state = Module_transfer(module, header, module->rx_buffer,
                                    sizeof(header),
                                    RADIOLIB_ERR_SPI_READ_FAILED);
            if (state == RADIOLIB_ERR_NONE) {
                memset(module->tx_buffer, 0, length);
                state = Module_transfer(module, module->tx_buffer, data,
                                        length,
                                        RADIOLIB_ERR_SPI_READ_FAILED);
            }
            Module_endSpiTransaction(module);
        }
    }
    if (state == RADIOLIB_ERR_NONE) {
        state = Module_waitForBusy(module);
    }
    Module_unlock(module);
    return state;
}
