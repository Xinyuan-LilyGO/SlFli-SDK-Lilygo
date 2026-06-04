/*!
 * \file      lora-spi-board.c
 *
 * \brief     spi peripheral initlize,it depend on mcu platform.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * \author    Forest-Rain
 */
#include "lora-radio-rtos-config.h"
#ifdef LORA_RADIO_DRIVER_USING_LORA_CHIP_SX126X
    #include "sx126x-board.h"
#elif defined LORA_RADIO_DRIVER_USING_LORA_CHIP_SX127X
    #include "sx127x-board.h"
#endif

#define LOG_TAG "LoRa.SF32.SPI"
#define LOG_LEVEL LOG_LVL_DBG
#include "lora-radio-debug.h"

struct rt_spi_device *lora_radio_spi_init(const char *bus_name,
                                          const char *lora_device_name,
                                          rt_uint8_t param)
{
    rt_err_t res;
    struct rt_spi_device *lora_radio_spi_device;
    rt_device_t spi_bus = RT_NULL;

    RT_ASSERT(bus_name);
    spi_bus = rt_device_find(bus_name);
    if (RT_NULL == spi_bus)
    {
        return RT_NULL;
    }

    lora_radio_spi_device =
        (struct rt_spi_device *)rt_device_find(lora_device_name);
    if (lora_radio_spi_device == RT_NULL)
    {
        res = rt_hw_spi_device_attach(bus_name, lora_device_name);
        if (res != RT_EOK)
        {
            LORA_RADIO_DEBUG_LOG(LR_DBG_SPI, LOG_LEVEL,
                                 "rt_spi_bus_attach_device failed!\r\n");
            return RT_NULL;
        }
        lora_radio_spi_device =
            (struct rt_spi_device *)rt_device_find(lora_device_name);
    }

    if (lora_radio_spi_device == RT_NULL)
    {
        LORA_RADIO_DEBUG_LOG(LR_DBG_SPI, LOG_LEVEL,
                             "spi sample run failed! cant't find %s device!\n",
                             lora_device_name);
        return RT_NULL;
    }

    res = rt_device_open(&lora_radio_spi_device->parent,
                         RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_DMA_RX |
                             RT_DEVICE_FLAG_DMA_TX);
    if (res != RT_EOK)
    {
        /* Fallback to non-DMA */
        LOG_W("SPI DMA open failed (%d), try non-DMA", res);
        res =
            rt_device_open(&lora_radio_spi_device->parent, RT_DEVICE_FLAG_RDWR);
        if (res != RT_EOK)
        {
            LOG_W("SPI device open failed");
        }
    }
    // res = rt_device_open(&(lora_radio_spi_device->parent),
    // RT_DEVICE_FLAG_RDWR); if (res != RT_EOK)
    // {
    //     LORA_RADIO_DEBUG_LOG(LR_DBG_SPI, LOG_LEVEL,
    //                          "rt_device_open failed!\r\n");
    //     return RT_NULL;
    // }
    LORA_RADIO_DEBUG_LOG(LR_DBG_SPI, LOG_LEVEL, "find %s device!\n",
                         lora_device_name);

    struct rt_spi_configuration cfg;
    cfg.data_width = 8;
    cfg.mode = RT_SPI_MASTER | RT_SPI_MODE_0 | RT_SPI_MSB |
               RT_SPI_NO_CS;  /* SPI Compatible: Mode 0. */
    cfg.max_hz = 8 * 1000000; /* max 10M */

    res = rt_spi_configure(lora_radio_spi_device, &cfg);

    if (res != RT_EOK)
    {
        LORA_RADIO_DEBUG_LOG(LR_DBG_SPI, LOG_LEVEL,
                             "rt_spi_configure failed!\r\n");
    }
    // res = rt_spi_take_bus(lora_radio_spi_device);
    // if (res != RT_EOK)
    // {
    //     LORA_RADIO_DEBUG_LOG(LR_DBG_SPI, LOG_LEVEL,
    //                          "rt_spi_take_bus failed!\r\n");
    // }

    // res = rt_spi_release_bus(lora_radio_spi_device);

    // if (res != RT_EOK)
    // {
    //     LORA_RADIO_DEBUG_LOG(LR_DBG_SPI, LOG_LEVEL,
    //                          "rt_spi_release_bus failed!\r\n");
    // }

    return lora_radio_spi_device;
}

/**
 * This function releases memory
 *
 * @param dev the pointer of device driver structure
 */
void lora_radio_spi_deinit(struct rt_spi_device *dev)
{
    RT_ASSERT(dev);
    rt_spi_release_bus(dev);
}
