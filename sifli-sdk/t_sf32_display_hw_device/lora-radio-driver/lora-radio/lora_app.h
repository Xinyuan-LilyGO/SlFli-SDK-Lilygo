#ifndef __LORA_APP_H__
#define __LORA_APP_H__

#include "lora-radio-debug.h"
#include "lora-radio-rtos-config.h"
#include "lora-radio-config.h"
#include "lora-radio-timer.h"
#include "lora-radio.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct lora_rx_info
    {
        int16_t rssi;
        int8_t snr;
        uint8_t data[BUFFER_SIZE];
        uint16_t data_len;
    } lora_rx_info_t;

    int lora_app_init(void);
    void radio_sleep(void);
    void radio_stanby(void);
    void radio_rx(void);
    void radio_set_rx_boost(bool boost);
    void radio_tx(uint8_t *data, uint16_t size);
    void get_radio_rx_info(lora_rx_info_t *rx_info);
    void radio_set_mode(uint8_t modem);
    void radio_set_freq(uint32_t freq);
    void radio_set_outpower(int8_t outpower);
    void radio_set_lora_bandwidth(uint32_t bw);
    void radio_set_lora_sf(uint8_t sf);
    void radio_set_lora_cr(uint8_t cr);
    void radio_set_lora_preamble(uint8_t preamble);
    void radio_set_lora_sync_word(bool network);
    void radio_set_lora_iq(bool iq);
    void radio_set_lora_crc(bool crc);
    void radio_set_rx_callback(void (*callback)(lora_rx_info_t *info));

#ifdef __cplusplus
}
#endif

#endif