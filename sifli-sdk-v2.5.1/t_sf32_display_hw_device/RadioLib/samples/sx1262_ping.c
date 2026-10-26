#include "RadioLib.h"

#include <finsh.h>
#include <stdlib.h>
#include <string.h>

static Module_t radiolib_sample_module;
static SX1262_t radiolib_sample_radio;
static bool radiolib_sample_ready;

static int radiolib_sample_init(void)
{
    ModuleConfig_t module_config;
    int16_t state;

    if (radiolib_sample_ready) {
        return RADIOLIB_ERR_NONE;
    }
    Module_configDefault(&module_config);
    module_config.board_init = RadioLib_boardInitTDisplaySF32;
    state = Module_init(&radiolib_sample_module, &module_config);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }
    state = SX1262_begin(&radiolib_sample_radio, &radiolib_sample_module);
    if (state != RADIOLIB_ERR_NONE) {
        Module_deinit(&radiolib_sample_module);
        return state;
    }
    radiolib_sample_ready = true;
    return RADIOLIB_ERR_NONE;
}

static int radiolib_lora(int argc, char **argv)
{
    uint8_t buffer[256];
    size_t length;
    uint32_t timeout;
    int16_t state;

    if (argc < 2) {
        rt_kprintf("radiolib_lora init | tx <text> | rx [timeout_ms]\n");
        return -1;
    }
    state = radiolib_sample_init();
    if (state != RADIOLIB_ERR_NONE) {
        rt_kprintf("RadioLib init failed: %d\n", state);
        return state;
    }
    if (strcmp(argv[1], "init") == 0) {
        rt_kprintf("SX1262 ready\n");
        return 0;
    }
    if ((strcmp(argv[1], "tx") == 0) && (argc >= 3)) {
        state = SX126x_transmit(&radiolib_sample_radio,
                                (const uint8_t *)argv[2], strlen(argv[2]));
        rt_kprintf("TX: %d\n", state);
        return state;
    }
    if (strcmp(argv[1], "rx") == 0) {
        timeout = (argc >= 3) ? (uint32_t)strtoul(argv[2], NULL, 10) : 5000U;
        length = sizeof(buffer) - 1U;
        state = SX126x_receive(&radiolib_sample_radio, buffer, &length,
                               timeout);
        if (state == RADIOLIB_ERR_NONE) {
            buffer[length] = '\0';
            rt_kprintf("RX[%d] RSSI=%d SNR=%d: %s\n", (int)length,
                       SX126x_getRSSI(&radiolib_sample_radio),
                       SX126x_getSNR(&radiolib_sample_radio), buffer);
        } else {
            rt_kprintf("RX: %d\n", state);
        }
        return state;
    }
    rt_kprintf("invalid command\n");
    return -1;
}
MSH_CMD_EXPORT(radiolib_lora, RadioLib SX1262 test);
