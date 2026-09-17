#include "lora_rt_device.h"

#define DBG_TAG "lora.dev"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define LORA_RT_RX_QUEUE_DEPTH 4U
#define LORA_RT_MAX_PAYLOAD    255U

struct lora_rt_device
{
    struct rt_device parent;
    struct rt_mutex init_lock;
    struct rt_mutex operation_lock;
    struct rt_mutex rx_lock;
    struct lora_rt_config config;
    lora_rx_info_t rx_queue[LORA_RT_RX_QUEUE_DEPTH];
    lora_rx_info_t last_rx;
    rt_size_t rx_head;
    rt_size_t rx_tail;
    rt_size_t rx_count;
    rt_bool_t init_attempted;
    rt_err_t init_result;
};

static struct lora_rt_device g_lora_device;

static void lora_rt_rx_queue_reset(struct lora_rt_device *device)
{
    rt_mutex_take(&device->rx_lock, RT_WAITING_FOREVER);
    device->rx_head = 0U;
    device->rx_tail = 0U;
    device->rx_count = 0U;
    rt_memset(&device->last_rx, 0, sizeof(device->last_rx));
    rt_mutex_release(&device->rx_lock);
}

static void lora_rt_rx_callback(lora_rx_info_t *info)
{
    struct lora_rt_device *device = &g_lora_device;
    rt_size_t received_size;

    if (info == RT_NULL || info->data_len == 0U)
        return;

    received_size = info->data_len;
    if (received_size > LORA_RT_MAX_PAYLOAD)
        received_size = LORA_RT_MAX_PAYLOAD;

    rt_mutex_take(&device->rx_lock, RT_WAITING_FOREVER);
    if (device->rx_count == LORA_RT_RX_QUEUE_DEPTH)
    {
        device->rx_head =
            (device->rx_head + 1U) % LORA_RT_RX_QUEUE_DEPTH;
        device->rx_count--;
    }

    device->rx_queue[device->rx_tail] = *info;
    device->rx_queue[device->rx_tail].data_len =
        (rt_uint16_t)received_size;
    device->rx_tail =
        (device->rx_tail + 1U) % LORA_RT_RX_QUEUE_DEPTH;
    device->rx_count++;
    rt_mutex_release(&device->rx_lock);

    if (device->parent.rx_indicate != RT_NULL)
        device->parent.rx_indicate(&device->parent, received_size);
}

static rt_err_t lora_rt_apply_config(struct lora_rt_device *device,
                                     const struct lora_rt_config *config)
{
    if (config == RT_NULL || config->frequency == 0U ||
            config->tx_power < -9 || config->tx_power > 22 ||
            config->bandwidth > LORA_BW_500KHZ ||
            config->spreading_factor < LORA_SPREADING_FACTOR_5 ||
            config->spreading_factor > LORA_SPREADING_FACTOR_12 ||
            config->coding_rate < LORA_CODINGRATE_4_5 ||
            config->coding_rate > LORA_CODINGRATE_4_8 ||
            config->preamble_length == 0U ||
            config->preamble_length > 255U)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&device->operation_lock, RT_WAITING_FOREVER);
    radio_set_mode(MODEM_LORA);
    radio_set_freq(config->frequency);
    radio_set_outpower(config->tx_power);
    radio_set_lora_bandwidth(config->bandwidth);
    radio_set_lora_sf(config->spreading_factor);
    radio_set_lora_cr(config->coding_rate);
    radio_set_lora_preamble((rt_uint8_t)config->preamble_length);
    radio_set_lora_sync_word(config->public_network != RT_FALSE);
    radio_set_lora_iq(config->iq_inverted != RT_FALSE);
    radio_set_lora_crc(config->crc_enabled != RT_FALSE);
    radio_set_rx_boost(config->rx_boost != RT_FALSE);
    device->config = *config;
    rt_mutex_release(&device->operation_lock);
    return RT_EOK;
}

static rt_err_t lora_rt_ensure_ready(struct lora_rt_device *device)
{
    rt_mutex_take(&device->init_lock, RT_WAITING_FOREVER);
    if (!device->init_attempted)
    {
        device->init_attempted = RT_TRUE;
        device->init_result = lora_app_init();
        if (device->init_result == RT_EOK)
        {
            radio_set_rx_callback(lora_rt_rx_callback);
            device->init_result =
                lora_rt_apply_config(device, &device->config);
        }
    }
    rt_mutex_release(&device->init_lock);
    return device->init_result;
}

static rt_err_t lora_rt_device_init(rt_device_t parent)
{
    return lora_rt_ensure_ready((struct lora_rt_device *)parent);
}

static rt_err_t lora_rt_device_open(rt_device_t parent, rt_uint16_t flags)
{
    (void)flags;
    return lora_rt_ensure_ready((struct lora_rt_device *)parent);
}

static rt_err_t lora_rt_device_close(rt_device_t parent)
{
    struct lora_rt_device *device = (struct lora_rt_device *)parent;

    if (device->init_result == RT_EOK)
    {
        rt_mutex_take(&device->operation_lock, RT_WAITING_FOREVER);
        radio_sleep();
        rt_mutex_release(&device->operation_lock);
    }
    lora_rt_rx_queue_reset(device);
    return RT_EOK;
}

static rt_size_t lora_rt_device_read(rt_device_t parent, rt_off_t position,
                                     void *buffer, rt_size_t size)
{
    struct lora_rt_device *device = (struct lora_rt_device *)parent;
    lora_rx_info_t packet;
    rt_size_t copy_size;

    (void)position;
    if (buffer == RT_NULL || size == 0U ||
            lora_rt_ensure_ready(device) != RT_EOK)
    {
        return 0U;
    }

    rt_mutex_take(&device->rx_lock, RT_WAITING_FOREVER);
    if (device->rx_count == 0U)
    {
        rt_mutex_release(&device->rx_lock);
        return 0U;
    }

    packet = device->rx_queue[device->rx_head];
    device->rx_head = (device->rx_head + 1U) % LORA_RT_RX_QUEUE_DEPTH;
    device->rx_count--;
    device->last_rx = packet;
    rt_mutex_release(&device->rx_lock);

    copy_size = packet.data_len;
    if (copy_size > size)
        copy_size = size;
    rt_memcpy(buffer, packet.data, copy_size);
    return copy_size;
}

static rt_size_t lora_rt_device_write(rt_device_t parent, rt_off_t position,
                                      const void *buffer, rt_size_t size)
{
    struct lora_rt_device *device = (struct lora_rt_device *)parent;

    (void)position;
    if (buffer == RT_NULL || size == 0U || size > LORA_RT_MAX_PAYLOAD ||
            lora_rt_ensure_ready(device) != RT_EOK)
    {
        return 0U;
    }

    rt_mutex_take(&device->operation_lock, RT_WAITING_FOREVER);
    radio_tx((rt_uint8_t *)buffer, (rt_uint16_t)size);
    rt_mutex_release(&device->operation_lock);
    return size;
}

static rt_err_t lora_rt_device_control(rt_device_t parent, int command,
                                       void *args)
{
    struct lora_rt_device *device = (struct lora_rt_device *)parent;
    rt_err_t result;

    if (command == RT_DEVICE_CTRL_SUSPEND || command == LORA_RT_CTRL_SLEEP)
    {
        if (device->init_result == RT_EOK)
        {
            rt_mutex_take(&device->operation_lock, RT_WAITING_FOREVER);
            radio_sleep();
            rt_mutex_release(&device->operation_lock);
        }
        return RT_EOK;
    }

    if (command == LORA_RT_CTRL_GET_CONFIG)
    {
        if (args == RT_NULL)
            return -RT_EINVAL;
        *(struct lora_rt_config *)args = device->config;
        return RT_EOK;
    }

    result = lora_rt_ensure_ready(device);
    if (result != RT_EOK)
        return result;

    switch (command)
    {
    case RT_DEVICE_CTRL_RESUME:
    case LORA_RT_CTRL_STANDBY:
        rt_mutex_take(&device->operation_lock, RT_WAITING_FOREVER);
        radio_stanby();
        rt_mutex_release(&device->operation_lock);
        return RT_EOK;
    case LORA_RT_CTRL_SET_CONFIG:
        return lora_rt_apply_config(device, args);
    case LORA_RT_CTRL_START_RX:
        rt_mutex_take(&device->operation_lock, RT_WAITING_FOREVER);
        radio_rx();
        rt_mutex_release(&device->operation_lock);
        return RT_EOK;
    case LORA_RT_CTRL_GET_STATUS:
        if (args == RT_NULL)
            return -RT_EINVAL;
        rt_mutex_take(&device->operation_lock, RT_WAITING_FOREVER);
        *(RadioState_t *)args = Radio.GetStatus();
        rt_mutex_release(&device->operation_lock);
        return RT_EOK;
    case LORA_RT_CTRL_GET_LAST_RX_INFO:
        if (args == RT_NULL)
            return -RT_EINVAL;
        rt_mutex_take(&device->rx_lock, RT_WAITING_FOREVER);
        *(lora_rx_info_t *)args = device->last_rx;
        rt_mutex_release(&device->rx_lock);
        return RT_EOK;
    case LORA_RT_CTRL_FLUSH_RX:
        lora_rt_rx_queue_reset(device);
        return RT_EOK;
    default:
        return -RT_EINVAL;
    }
}

#ifdef RT_USING_DEVICE_OPS
static const struct rt_device_ops g_lora_ops =
{
    .init = lora_rt_device_init,
    .open = lora_rt_device_open,
    .close = lora_rt_device_close,
    .read = lora_rt_device_read,
    .write = lora_rt_device_write,
    .control = lora_rt_device_control,
};
#endif

int rt_hw_lora_device_register(void)
{
    struct lora_rt_device *device = &g_lora_device;

    if (rt_device_find(LORA_RT_DEVICE_NAME) != RT_NULL)
        return RT_EOK;

    rt_memset(device, 0, sizeof(*device));
    rt_mutex_init(&device->init_lock, "lora_i", RT_IPC_FLAG_FIFO);
    rt_mutex_init(&device->operation_lock, "lora_op", RT_IPC_FLAG_FIFO);
    rt_mutex_init(&device->rx_lock, "lora_rx", RT_IPC_FLAG_FIFO);
    device->init_result = -RT_ERROR;
    device->config.frequency = RF_FREQUENCY;
    device->config.tx_power = TX_OUTPUT_POWER;
    device->config.bandwidth = LORA_BANDWIDTH;
    device->config.spreading_factor = LORA_SPREADING_FACTOR;
    device->config.coding_rate = LORA_CODINGRATE;
    device->config.preamble_length = LORA_PREAMBLE_LENGTH;
    device->config.public_network = RT_FALSE;
    device->config.iq_inverted = LORA_IQ_INVERSION_ON_DISABLE;
    device->config.crc_enabled = RT_FALSE;
    device->config.rx_boost = RT_TRUE;

    device->parent.type = RT_Device_Class_Miscellaneous;
#ifdef RT_USING_DEVICE_OPS
    device->parent.ops = &g_lora_ops;
#else
    device->parent.init = lora_rt_device_init;
    device->parent.open = lora_rt_device_open;
    device->parent.close = lora_rt_device_close;
    device->parent.read = lora_rt_device_read;
    device->parent.write = lora_rt_device_write;
    device->parent.control = lora_rt_device_control;
#endif

    return rt_device_register(&device->parent, LORA_RT_DEVICE_NAME,
                              RT_DEVICE_FLAG_RDWR |
                              RT_DEVICE_FLAG_INT_RX);
}
INIT_COMPONENT_EXPORT(rt_hw_lora_device_register);
