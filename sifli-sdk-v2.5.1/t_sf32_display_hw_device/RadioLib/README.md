# RadioLib C port for SiFli RT-Thread

This package follows the layer layout and public naming style of
[jgromes/RadioLib](https://github.com/jgromes/RadioLib), but it is an original C
implementation for the SiFli RT-Thread SDK. It does not require C++.

## Support matrix

| Radio | Status |
| --- | --- |
| SX1262 | LoRa TX/RX, async start/read, CAD, IRQ, sleep, RF parameters and packet metrics |
| LR1121 | API placeholder; `LR1121_begin()` returns `RADIOLIB_ERR_UNSUPPORTED` |
| LR2021 | API placeholder; `LR2021_begin()` returns `RADIOLIB_ERR_UNSUPPORTED` |
| SX1276 | API placeholder; `SX1276_begin()` returns `RADIOLIB_ERR_UNSUPPORTED` |

The main layers are:

- `Module`: RT-Thread SPI, GPIO, reset, BUSY and DIO1 IRQ transport.
- `PhysicalLayer`: chip-independent function table for application code.
- `modules/SX126x`: SX1262 command and LoRa implementation.

## Configuration

Enable `PKG_USING_RADIOLIB` in menuconfig. The default T-Display-SF32 settings
match the existing board driver:

| Signal | Default |
| --- | --- |
| SPI bus | `spi1` |
| Enable | PA26 |
| NSS | PA29 |
| DIO1 | PA30 |
| BUSY | PA31 |
| RESET | PA32 |
| RF frequency | 868 MHz |

Do not enable the old `PKG_USING_LORA_RADIO_DRIVER` on the same module at the
same time. Both drivers would control the same SPI peripheral and GPIOs.

## Basic use

```c
#include "RadioLib.h"

static Module_t module;
static SX1262_t radio;

int radio_init(void)
{
    ModuleConfig_t module_config;
    int16_t state;

    Module_configDefault(&module_config);
    module_config.board_init = RadioLib_boardInitTDisplaySF32;

    state = Module_init(&module, &module_config);
    if (state != RADIOLIB_ERR_NONE)
        return state;

    return SX1262_begin(&radio, &module);
}
```

Transmit and receive use the RadioLib-style SX126x names. For receive,
`length` is the input buffer capacity and is replaced with the received length.

```c
uint8_t rx[255];
size_t rx_len = sizeof(rx);

SX126x_transmit(&radio, (const uint8_t *)"ping", 4);
SX126x_receive(&radio, rx, &rx_len, 5000);
```

For asynchronous operation, register a short DIO1 action with
`SX126x_setDio1Action()`, call `SX126x_startTransmit()` or
`SX126x_startReceive()`, and perform `SX126x_finishTransmit()` or
`SX126x_readData()` from thread context. The IRQ callback must not block or call
SPI APIs.

The optional MSH sample provides:

```text
radiolib_lora init
radiolib_lora tx hello
radiolib_lora rx 5000
```

## License

SPDX-License-Identifier: Apache-2.0
