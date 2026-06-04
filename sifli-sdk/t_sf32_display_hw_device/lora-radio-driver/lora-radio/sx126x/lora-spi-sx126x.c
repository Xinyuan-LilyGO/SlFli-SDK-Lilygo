/*!
 * \file      lora-spi-SX126x.c
 *
 * \brief     spi driver implementation for SX126X
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * \author    Forest-Rain
 */

#include "sx126x-board.h"

#define LOG_TAG "LoRa.SX126X.SPI"
#define LOG_LEVEL LOG_LVL_DBG
#include "lora-radio-debug.h"
uint8_t _wr_buf[256]; /* max 255 byte payload + 2 byte header */

void SX126xWakeup(void)
{
#ifdef RT_USING_SPI
    uint8_t msg[2] = {RADIO_GET_STATUS, 0x00};

    rt_pin_mode(LORA_RADIO_NSS_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_LOW);

    rt_spi_transfer(SX126x.spi, msg, RT_NULL, 2);

    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_HIGH);

    // Wait for chip to be ready.
    SX126xWaitOnBusy();

    // Update operating mode context variable
    SX126xSetOperatingMode(MODE_STDBY_RC);
#else
#endif
}

void SX126xWriteCommand(RadioCommands_t command, uint8_t *buffer, uint16_t size)
{
#ifdef RT_USING_SPI
    SX126xCheckDeviceReady();

    rt_pin_mode(LORA_RADIO_NSS_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_LOW);

    rt_spi_send_then_send(SX126x.spi, &command, 1, buffer, size);

    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_HIGH);

    if (command != RADIO_SET_SLEEP)
    {
        SX126xWaitOnBusy();
    }

#else
#endif
}

uint8_t SX126xReadCommand(RadioCommands_t command, uint8_t *buffer,
                          uint16_t size)
{
#ifdef RT_USING_SPI
    uint8_t status = 0;
    uint8_t buffer_temp[16] = {0}; // command size is 2 size

    SX126xCheckDeviceReady();

    rt_pin_mode(LORA_RADIO_NSS_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_LOW);

    rt_spi_send_then_recv(SX126x.spi, &command, 1, buffer_temp, size + 1);

    status = buffer_temp[0];

    rt_memcpy(buffer, buffer_temp + 1, size);

    SX126xWaitOnBusy();

    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_HIGH);
    return status;
#else
#endif
}

void SX126xWriteRegisters(uint16_t address, uint8_t *buffer, uint16_t size)
{
#ifdef RT_USING_SPI
    uint8_t msg[3] = {0};

    msg[0] = RADIO_WRITE_REGISTER;
    msg[1] = (address & 0xFF00) >> 8;
    msg[2] = address & 0x00FF;

    SX126xCheckDeviceReady();

    rt_pin_mode(LORA_RADIO_NSS_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_LOW);
    rt_spi_send_then_send(SX126x.spi, msg, 3, buffer, size);
    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_HIGH);

    SX126xWaitOnBusy();
#else
#endif
}

void SX126xWriteRegister(uint16_t address, uint8_t value)
{
    SX126xWriteRegisters(address, &value, 1);
}

void SX126xReadRegisters(uint16_t address, uint8_t *buffer, uint16_t size)
{
#ifdef RT_USING_SPI
    uint8_t msg[4] = {0};

    msg[0] = RADIO_READ_REGISTER;
    msg[1] = (address & 0xFF00) >> 8;
    msg[2] = address & 0x00FF;
    msg[3] = 0;

    SX126xCheckDeviceReady();
    
    rt_pin_mode(LORA_RADIO_NSS_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_LOW);
    rt_spi_send_then_recv(SX126x.spi, msg, 4, buffer, size);

    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_HIGH);

    SX126xWaitOnBusy();

#else
#endif
}

uint8_t SX126xReadRegister(uint16_t address)
{
    uint8_t data;
    SX126xReadRegisters(address, &data, 1);
    return data;
}

void SX126xWriteBuffer(uint8_t offset, uint8_t *buffer, uint8_t size)
{
#ifdef RT_USING_SPI
    /* Combine header + payload into single buffer to avoid rt_spi_send_then_send
     * multi-message CS hold issue (HOLD_FRAME_LOW) that can cause SPI_EndRxTxTransaction
     * timeout with larger payloads.
     */
    _wr_buf[0] = RADIO_WRITE_BUFFER;
    _wr_buf[1] = offset;
    rt_memcpy(_wr_buf + 2, buffer, size);

    SX126xCheckDeviceReady();

    rt_pin_mode(LORA_RADIO_NSS_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_LOW);
    rt_spi_send(SX126x.spi, _wr_buf, size + 2);
    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_HIGH);

    SX126xWaitOnBusy();

#else

#endif
}

void SX126xReadBuffer(uint8_t offset, uint8_t *buffer, uint8_t size)
{
#ifdef RT_USING_SPI
    uint8_t msg[3] = {0};

    msg[0] = RADIO_READ_BUFFER;
    msg[1] = offset;
    msg[2] = 0;

    SX126xCheckDeviceReady();
    rt_pin_mode(LORA_RADIO_NSS_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_LOW);
    rt_spi_send_then_recv(SX126x.spi, msg, 3, buffer, size);
    rt_pin_write(LORA_RADIO_NSS_PIN, PIN_HIGH);
    SX126xWaitOnBusy();


#else

#endif
}
