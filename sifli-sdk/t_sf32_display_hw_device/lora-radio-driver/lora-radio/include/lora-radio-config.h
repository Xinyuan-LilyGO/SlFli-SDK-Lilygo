/*!
 * \file      lora-radio-tester.h
 *
 * \brief     lora radio test implementation
 *
 * \copyright SPDX-License-Identifier: Apache-2.0
 *
 * \author    Forest-Rain
 */

#ifndef __LORA_RADIO_TESTER_H__
#define __LORA_RADIO_TESTER_H__

#include "lora-radio.h"

/* application debug */
#ifndef LR_DBG_SHELL
    #define LR_DBG_SHELL 1
#endif

// #define PHY_REGION_RU864 1
// #define PHY_REGION_IN865 1
#define PHY_REGION_EU868 1
// #define PHY_REGION_US915 1
// #define PHY_REGION_AS920 1
// #define PHY_REGION_KR923 1

#if defined(PHY_REGION_AS923)

    #define RF_FREQUENCY 923000000 // Hz

#elif defined(PHY_REGION_AU915)

    #define RF_FREQUENCY 915000000 // Hz

#elif defined(PHY_REGION_CN470) || defined(PHY_REGION_CN470S)

    #define RF_FREQUENCY 470300000 // Hz

#elif defined(PHY_REGION_CN779)

    #define RF_FREQUENCY 779000000 // Hz

#elif defined(PHY_REGION_EU433)

    #define RF_FREQUENCY 433000000 // Hz

#elif defined(PHY_REGION_EU868)

    #define RF_FREQUENCY 868000000 // Hz

#elif defined(PHY_REGION_KR920)

    #define RF_FREQUENCY 920000000 // Hz

#elif defined(PHY_REGION_IN865)

    #define RF_FREQUENCY 865000000 // Hz

#elif defined(PHY_REGION_US915)

    #define RF_FREQUENCY 915000000 // Hz

#elif defined(PHY_REGION_RU864)

    #define RF_FREQUENCY 864000000 // Hz

#else
    // #error "Please define a frequency band in the compiler options."
#endif

#define TX_RX_FREQUENCE_OFFSET                                                 \
    0                      // 0           TX = RX
                           // 1800000     RX = TX+1.8M
#define TX_OUTPUT_POWER 22 // dBm

enum
{
    LORA_BW_62KHZ = 0,
    LORA_BW_125KHZ,
    LORA_BW_250KHZ,
    LORA_BW_500KHZ,
};
#define LORA_BANDWIDTH                                                         \
    1 // [0: 62 kHz,
      //  1: 125 kHz,
      //  2: 250 kHz,
      //  3: 500 kHz,
      //  4: Reserved]

enum
{
    LORA_SPREADING_FACTOR_5 = 5,
    LORA_SPREADING_FACTOR_6,
    LORA_SPREADING_FACTOR_7,
    LORA_SPREADING_FACTOR_8,
    LORA_SPREADING_FACTOR_9,
    LORA_SPREADING_FACTOR_10,
    LORA_SPREADING_FACTOR_11,
    LORA_SPREADING_FACTOR_12,
};
#define LORA_SPREADING_FACTOR 5 // [SF7..SF12]

enum
{
    LORA_CODINGRATE_4_5 = 1,
    LORA_CODINGRATE_4_6,
    LORA_CODINGRATE_4_7,
    LORA_CODINGRATE_4_8,
};
#define LORA_CODINGRATE                                                        \
    1                           // [1: 4/5,
                                //  2: 4/6,
                                //  3: 4/7,
                                //  4: 4/8]
#define LORA_PREAMBLE_LENGTH 12 // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT 0   // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON_DISABLE false
#define LORA_IQ_INVERSION_ON_DISABLE false

#define FSK_FDEV 25000     // Hz
#define FSK_DATARATE 50000 // bps

#if defined(LORA_RADIO_DRIVER_USING_LORA_CHIP_SX127X)

    #define FSK_BANDWIDTH 50000     // Hz >> SSB in sx127x
    #define FSK_AFC_BANDWIDTH 83333 // Hz

#elif defined(LORA_RADIO_DRIVER_USING_LORA_CHIP_SX126X) ||                     \
    defined(LORA_RADIO_DRIVER_USING_LORA_CHIP_LLCC68)

    #define FSK_BANDWIDTH 100000     // Hz >> DSB in sx126x
    #define FSK_AFC_BANDWIDTH 166666 // Hz >> Unused in sx126x

#elif defined(LORA_RADIO_DRIVER_USING_LORA_SOC_STM32WL)

    #define FSK_BANDWIDTH 100000     // Hz >> DSB in sx126x
    #define FSK_AFC_BANDWIDTH 166666 // Hz >> Unused in sx126x
#elif defined(LORA_RADIO_DRIVER_USING_LORA_CHIP_SX128X)

    #define FSK_BANDWIDTH 100000     // Hz >> DSB in sx126x
    #define FSK_AFC_BANDWIDTH 166666 // Hz >> Unused in sx126x
#else
    #error "Please define a lora-shield in the compiler options."
#endif

#define FSK_PREAMBLE_LENGTH 5 // Same for Tx and Rx
#define FSK_FIX_LENGTH_PAYLOAD_ON false

#define TX_TIMEOUT_VALUE 2000
#define RX_TIMEOUT_VALUE 2000
#define BUFFER_SIZE 256           // Define the payload size here
#define MIN_TETS_APP_DATA_SIZE 17 // for PING protocol

#define LORA_MASTER_DEVADDR 0x11223344
#define LORA_SLAVER_DEVADDR 0x01020304
#define MAC_HEADER_OVERHEAD 13

// Ping pong event
#define EV_RADIO_INIT 0x0001
#define EV_RADIO_TX_START 0x0002
#define EV_RADIO_TX_DONE 0x0004
#define EV_RADIO_TX_TIMEOUT 0x0008
#define EV_RADIO_RX_START 0x0010
#define EV_RADIO_RX_DONE 0x0020
#define EV_RADIO_RX_TIMEOUT 0x0040
#define EV_RADIO_RX_ERROR 0x0080
#define EV_RADIO_SLEEP 0x0100
// #define EV_RADIO_RX_DONE         0x0010
// #define EV_RADIO_RX_TIMEOUT      0x0020
// #define EV_RADIO_RX_ERROR        0x0040
#define EV_RADIO_ALL                                                           \
    (EV_RADIO_INIT | EV_RADIO_TX_START | EV_RADIO_TX_DONE |                    \
     EV_RADIO_TX_TIMEOUT | EV_RADIO_RX_START | EV_RADIO_RX_DONE |              \
     EV_RADIO_RX_TIMEOUT | EV_RADIO_RX_ERROR | EV_RADIO_SLEEP)

typedef struct
{
    RadioModems_t modem; // LoRa Modem \ FSK modem
    uint32_t tx_frequency;
    uint32_t rx_frequency;
    int32_t trx_frequency_offset;
    int8_t txpower;
    bool rx_boost;

    // LoRa
    uint8_t sf; // spreadfactor
    uint8_t bw; // bandwidth
    uint8_t cr; // coderate
    uint8_t iq_inversion;
    bool public_network;
    bool crc_on;
    uint16_t lora_preamble_len;

    // FSK
    uint32_t fdev;
    uint32_t datarate;
    uint32_t fsk_bandwidth;
    uint32_t fsk_afc_bandwidth;
    uint16_t fsk_preamble_len;

} radio_config_t;

#endif