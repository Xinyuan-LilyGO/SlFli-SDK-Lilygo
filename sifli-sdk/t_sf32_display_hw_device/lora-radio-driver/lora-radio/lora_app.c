#include "lora_app.h"

#define LORA_DEBUG 1
#if LORA_DEBUG
    #define LORA_LOG(...) rt_kprintf("[LORA] " __VA_ARGS__)
#else
    #define LORA_LOG(...)
#endif

static uint32_t rx_timeout = RX_TIMEOUT_VALUE;
static uint32_t tx_timeout = TX_TIMEOUT_VALUE;
static uint8_t payload_len = 255; // 1~255

lora_rx_info_t lora_rx_info;
static void (*lora_rx_callback)(lora_rx_info_t *info) = RT_NULL;

static bool lora_chip_initialized;
static RadioEvents_t RadioEvents;
static struct rt_event radio_event;
static rt_thread_t lora_radio_thread = RT_NULL;
static radio_config_t radio_paras = {
    .modem = MODEM_LORA,
    .tx_frequency = RF_FREQUENCY,
    .rx_frequency = RF_FREQUENCY + TX_RX_FREQUENCE_OFFSET,
    .trx_frequency_offset = TX_RX_FREQUENCE_OFFSET,
    .txpower = TX_OUTPUT_POWER,
    .rx_boost = true,

    // lora
    .sf = LORA_SPREADING_FACTOR,
    .bw = LORA_BANDWIDTH,
    .cr = LORA_CODINGRATE,
    .iq_inversion = LORA_IQ_INVERSION_ON_DISABLE,
    .public_network = false,
    .crc_on = false,
    .lora_preamble_len = LORA_PREAMBLE_LENGTH,
};

static void OnTxDone(void);
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
static void OnTxTimeout(void);
static void OnRxTimeout(void);
static void OnRxError(void);
static void lora_radio_thread_entry(void *parameter);

int lora_app_init(void)
{
    int ret;
    rt_event_init(&radio_event, "ev_lora_test", RT_IPC_FLAG_FIFO);
    lora_radio_thread = rt_thread_create(
        "lora-radio-test", lora_radio_thread_entry, RT_NULL, 2048, RT_THREAD_PRIORITY_HIGH, 10);
    if (lora_radio_thread != RT_NULL)
    {
        rt_thread_startup(lora_radio_thread);
    }
    else
    {
        return -RT_ERROR;
    }

    RadioEvents.TxDone = OnTxDone;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;

    ret = Radio.Init(&RadioEvents);
    if (ret)
    {
        lora_chip_initialized = true;
    }
    else
    {
        return -RT_ERROR;
    }

    Radio.Standby();
    Radio.Sleep();
    return RT_EOK;
}

static void OnTxDone(void)
{
    Radio.Sleep();
    rt_event_send(&radio_event, EV_RADIO_TX_DONE);
}

static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    Radio.Sleep();
    lora_rx_info.data_len = size;
    rt_memcpy(lora_rx_info.data, payload, lora_rx_info.data_len);
    lora_rx_info.rssi = rssi;
    lora_rx_info.snr = snr;
    rt_event_send(&radio_event, EV_RADIO_RX_DONE);
}

static void OnTxTimeout(void)
{
    Radio.Sleep(); // 进入睡眠 BUSY拉高
    rt_event_send(&radio_event, EV_RADIO_TX_TIMEOUT);
}

static void OnRxTimeout(void)
{
    Radio.Sleep();
    rt_event_send(&radio_event, EV_RADIO_RX_TIMEOUT);
}

static void OnRxError(void)
{
    Radio.Sleep();
    rt_event_send(&radio_event, EV_RADIO_RX_ERROR);
}

void init_tx_rx_timeout(void)
{
    /* unit£º ms */
    uint32_t packet_toa =
        Radio.TimeOnAir(radio_paras.modem, radio_paras.bw, radio_paras.sf,
                        radio_paras.cr, radio_paras.lora_preamble_len,
                        LORA_FIX_LENGTH_PAYLOAD_ON_DISABLE, payload_len, radio_paras.crc_on);
    tx_timeout = rx_timeout = packet_toa + 1000;
}

static void lora_radio_thread_entry(void *parameter)
{
    rt_uint32_t ev = 0;
    while (1)
    {
        if (rt_event_recv(&radio_event, EV_RADIO_ALL,
                          RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                          RT_WAITING_FOREVER, &ev) == RT_EOK)
        {
            switch (ev)
            {
            case EV_RADIO_INIT:
            {
                LORA_LOG("EV_RADIO_INIT\n");
                init_tx_rx_timeout();
                Radio.SetChannel(radio_paras.tx_frequency);
                radio_paras.rx_frequency =
                    radio_paras.tx_frequency + radio_paras.trx_frequency_offset;
                LORA_LOG("Radio mode %s\n",
                         radio_paras.modem == MODEM_LORA ? "LORA" : "FSK");
                if (radio_paras.modem == MODEM_LORA)
                {
                    Radio.SetPublicNetwork(radio_paras.public_network);

                    Radio.SetTxConfig(
                        MODEM_LORA, radio_paras.txpower, 0, radio_paras.bw,
                        radio_paras.sf, radio_paras.cr, LORA_PREAMBLE_LENGTH,
                        LORA_FIX_LENGTH_PAYLOAD_ON_DISABLE, true, 0, 0,
                        radio_paras.iq_inversion, tx_timeout);

                    Radio.SetRxConfig(
                        MODEM_LORA, radio_paras.bw, radio_paras.sf,
                        radio_paras.cr, 0, LORA_PREAMBLE_LENGTH,
                        LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON_DISABLE,
                        0, true, 0, 0, radio_paras.iq_inversion, true);
                }
                else
                {
                    Radio.SetTxConfig(MODEM_FSK, radio_paras.txpower, FSK_FDEV,
                                      0, FSK_DATARATE, 0, FSK_PREAMBLE_LENGTH,
                                      FSK_FIX_LENGTH_PAYLOAD_ON, true, 0, 0, 0,
                                      3000);

                    Radio.SetRxConfig(MODEM_FSK, FSK_BANDWIDTH, FSK_DATARATE, 0,
                                      FSK_AFC_BANDWIDTH, FSK_PREAMBLE_LENGTH, 0,
                                      FSK_FIX_LENGTH_PAYLOAD_ON, 0, true, 0, 0,
                                      false, true);
                }
            }
            break;
            case EV_RADIO_TX_START:
                LORA_LOG("EV_RADIO_TX_START\n");
                break;
            case EV_RADIO_TX_DONE:
                LORA_LOG("EV_RADIO_TX_DONE\n");
                break;
            case EV_RADIO_RX_DONE:
                LORA_LOG("EV_RADIO_RX_DONE\n");
                if (lora_rx_callback != RT_NULL)
                {
                    lora_rx_callback(&lora_rx_info);
                }
                break;
            case EV_RADIO_TX_TIMEOUT:
                LORA_LOG("EV_RADIO_TX_TIMEOUT\n");
                break;
            case EV_RADIO_RX_TIMEOUT:
                LORA_LOG("EV_RADIO_RX_TIMEOUT\n");
                radio_rx();
                break;
            case EV_RADIO_RX_ERROR:
                LORA_LOG("EV_RADIO_RX_ERROR\n");
                radio_rx();
                break;
            }
        }
    }
}

/********API***********/
void radio_rx(void)
{
    rt_uint32_t timeout = 0;
    rt_memset(&lora_rx_info, 0, sizeof(lora_rx_info_t));
    Radio.SetChannel(radio_paras.rx_frequency);
    Radio.SetRxConfig(MODEM_LORA, radio_paras.bw, radio_paras.sf,
                      radio_paras.cr, 0, radio_paras.lora_preamble_len,
                      LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON_DISABLE,
                      0, radio_paras.crc_on, 0, 0, radio_paras.iq_inversion,
                      true);
    if (radio_paras.rx_boost)
    {
        Radio.RxBoosted(timeout);
    }
    else
    {
        Radio.Rx(timeout);
    }
}

void get_radio_rx_info(lora_rx_info_t *rx_info)
{
    *rx_info = lora_rx_info;
}

void radio_set_rx_boost(bool boost)
{
    radio_paras.rx_boost = boost;
}

void radio_tx(uint8_t *data, uint16_t size)
{
    init_tx_rx_timeout();
    Radio.SetChannel(radio_paras.tx_frequency);
    radio_paras.rx_frequency =
        radio_paras.tx_frequency + radio_paras.trx_frequency_offset;
    if (radio_paras.modem == MODEM_LORA)
    {
        Radio.SetPublicNetwork(radio_paras.public_network);

        Radio.SetTxConfig(MODEM_LORA, radio_paras.txpower, 0, radio_paras.bw,
                          radio_paras.sf, radio_paras.cr,
                          radio_paras.lora_preamble_len,
                          radio_paras.iq_inversion, radio_paras.crc_on, 0, 0,
                          radio_paras.iq_inversion, tx_timeout);
    }
    else
    {
        Radio.SetTxConfig(MODEM_FSK, radio_paras.txpower, radio_paras.fdev, 0,
                          radio_paras.datarate, 0, radio_paras.fsk_preamble_len,
                          FSK_FIX_LENGTH_PAYLOAD_ON, radio_paras.crc_on, 0, 0,
                          0, 3000);
    }
    LORA_LOG("tx(%d):%s\n", size, data);

    Radio.Send(data, size);
}

void radio_sleep(void)
{
    Radio.Sleep();
}

void radio_stanby(void)
{
    Radio.Standby();
}

void radio_set_mode(uint8_t modem)
{
    radio_paras.modem = modem;
}

void radio_set_freq(uint32_t freq)
{
    radio_paras.tx_frequency = freq;
    radio_paras.rx_frequency = freq;
}

void radio_set_outpower(int8_t outpower)
{
    radio_paras.txpower = outpower;
}

void radio_set_lora_bandwidth(uint32_t bw)
{
    radio_paras.bw = bw;
}

void radio_set_lora_sf(uint8_t sf)
{
    radio_paras.sf = sf;
}

void radio_set_lora_cr(uint8_t cr)
{
    radio_paras.cr = cr;
}

void radio_set_lora_preamble(uint8_t preamble)
{
    radio_paras.lora_preamble_len = preamble;
}

void radio_set_lora_sync_word(bool network)
{
    radio_paras.public_network = network;
}

void radio_set_lora_iq(bool iq)
{
    radio_paras.iq_inversion = iq;
}

void radio_set_lora_crc(bool crc)
{
    radio_paras.crc_on = crc;
}

void radio_set_rx_callback(void (*callback)(lora_rx_info_t *info))
{
    lora_rx_callback = callback;
}