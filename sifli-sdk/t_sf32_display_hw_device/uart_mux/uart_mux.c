#include "uart_mux.h"
#include "xl9555.h"
#include <rtdevice.h>
#include <string.h>

#define UART_MUX_DEBUG 1
#if UART_MUX_DEBUG
    #define MUX_LOG(...) rt_kprintf("[UART_MUX] " __VA_ARGS__)
#else
    #define MUX_LOG(...)
#endif

static char mux_uart_name[RT_NAME_MAX];
static rt_device_t mux_serial = RT_NULL;
static struct serial_configure mux_config = RT_SERIAL_CONFIG_DEFAULT;
static uart_mux_device_t current_device = UART_MUX_DEVICE_NONE;

static rt_thread_t rx_thread = RT_NULL;
#define RX_THREAD_STACK_SIZE 2048
#define RX_THREAD_PRIORITY RT_THREAD_PRIORITY_MIDDLE
#define RX_BUF_SIZE 256
#define UART_RX_BUFSZ 2048

static uart_mux_rx_callback rx_callbacks[3] = {RT_NULL};

static struct rt_mutex mux_mutex;
static struct rt_semaphore mux_rx_sem;
static rt_bool_t rx_thread_running = RT_FALSE;

static rt_err_t uart_mux_rx_indicate(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(&mux_rx_sem);
    return RT_EOK;
}

static void mux_hw_select(uart_mux_device_t dev)
{
    xl9555_pin_mode(XL9555_GPS_ESP32C6_SEL_PIN, XL9555_PIN_OUTPUT);
    if (dev == UART_MUX_DEVICE_GPS)
    {
        xl9555_digital_write(XL9555_GPS_ESP32C6_SEL_PIN, 0);
        MUX_LOG("HW -> GPS\n");
    }
    else if (dev == UART_MUX_DEVICE_ESP32C6)
    {
        xl9555_digital_write(XL9555_GPS_ESP32C6_SEL_PIN, 1);
        MUX_LOG("HW -> ESP32C6\n");
    }
    rt_thread_mdelay(100);
}

static void mux_close_uart(void)
{
    if (mux_serial != RT_NULL)
    {
        rt_device_set_rx_indicate(mux_serial, RT_NULL);
        rt_device_close(mux_serial);
        mux_serial = RT_NULL;
        MUX_LOG("UART closed\n");
    }
}

static int mux_open_uart(uint32_t baudrate)
{
    rt_err_t ret;

    mux_serial = rt_device_find(mux_uart_name);
    if (mux_serial == RT_NULL)
    {
        MUX_LOG("Device %s not found\n", mux_uart_name);
        return -RT_ERROR;
    }

    mux_config.baud_rate = baudrate;
    mux_config.data_bits = DATA_BITS_8;
    mux_config.stop_bits = STOP_BITS_1;
    mux_config.parity = PARITY_NONE;
    mux_config.bufsz = UART_RX_BUFSZ;

    ret = rt_device_control(mux_serial, RT_DEVICE_CTRL_CONFIG, &mux_config);
    if (ret != RT_EOK)
    {
        MUX_LOG("Config UART at %lu failed: %d\n", baudrate, ret);
        mux_serial = RT_NULL;
        return ret;
    }

    ret = rt_device_open(mux_serial, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
    if (ret != RT_EOK)
    {
        MUX_LOG("Open UART at %lu failed: %d\n", baudrate, ret);
        mux_serial = RT_NULL;
        return ret;
    }

    ret = rt_device_set_rx_indicate(mux_serial, uart_mux_rx_indicate);
    if (ret != RT_EOK)
    {
        MUX_LOG("Set RX indicate failed: %d\n", ret);
        rt_device_close(mux_serial);
        mux_serial = RT_NULL;
        return ret;
    }

    MUX_LOG("UART opened at %lu\n", baudrate);
    return RT_EOK;
}

static void uart_mux_rx_thread_entry(void *param)
{
    uint8_t buf[RX_BUF_SIZE];
    rt_size_t len;

    (void)param;

    while (rx_thread_running)
    {
        if (rt_sem_take(&mux_rx_sem, RT_WAITING_FOREVER) != RT_EOK)
            continue;

        while (rx_thread_running)
        {
            uart_mux_rx_callback cb;
            rt_size_t i;

            rt_mutex_take(&mux_mutex, RT_WAITING_FOREVER);
            if (mux_serial == RT_NULL || current_device == UART_MUX_DEVICE_NONE)
            {
                rt_mutex_release(&mux_mutex);
                break;
            }

            len = rt_device_read(mux_serial, 0, buf, sizeof(buf));
            cb = rx_callbacks[current_device];
            rt_mutex_release(&mux_mutex);

            if (len == 0)
                break;

            if (cb != RT_NULL)
            {
                for (i = 0; i < len; i++)
                    cb(buf[i]);
            }
        }
    }

    rx_thread = RT_NULL;
    MUX_LOG("RX thread exit\n");
}

int uart_mux_init(const char *uart_name)
{
    if (uart_name == RT_NULL)
        return -RT_EINVAL;

    rt_strncpy(mux_uart_name, uart_name, RT_NAME_MAX - 1);
    mux_uart_name[RT_NAME_MAX - 1] = '\0';

    rt_mutex_init(&mux_mutex, "mux_mtx", RT_IPC_FLAG_PRIO);
    rt_sem_init(&mux_rx_sem, "mux_rx", 0, RT_IPC_FLAG_PRIO);

    rx_thread_running = RT_TRUE;
    rx_thread = rt_thread_create("uart_mux_rx", uart_mux_rx_thread_entry, RT_NULL,
                                 RX_THREAD_STACK_SIZE, RX_THREAD_PRIORITY, 10);
    if (rx_thread == RT_NULL)
    {
        rx_thread_running = RT_FALSE;
        rt_sem_detach(&mux_rx_sem);
        rt_mutex_detach(&mux_mutex);
        return -RT_ERROR;
    }

    rt_thread_startup(rx_thread);

    MUX_LOG("Initialized for %s\n", uart_name);
    return RT_EOK;
}

int uart_mux_switch_to(uart_mux_device_t dev, uint32_t baudrate)
{
    rt_err_t ret = RT_EOK;

    if (dev == UART_MUX_DEVICE_NONE)
        return -RT_EINVAL;

    rt_mutex_take(&mux_mutex, RT_WAITING_FOREVER);

    if (current_device == dev && mux_serial != RT_NULL &&
        mux_config.baud_rate == baudrate)
    {
        rt_mutex_release(&mux_mutex);
        MUX_LOG("Already active with same baudrate\n");
        return RT_EOK;
    }

    MUX_LOG("Switching to device %d, baudrate %lu\n", dev, baudrate);

    mux_close_uart();
    mux_hw_select(dev);

    ret = mux_open_uart(baudrate);
    if (ret != RT_EOK)
    {
        current_device = UART_MUX_DEVICE_NONE;
        rt_mutex_release(&mux_mutex);
        return ret;
    }

    current_device = dev;

    rt_mutex_release(&mux_mutex);
    MUX_LOG("Switch completed\n");
    return RT_EOK;
}

int uart_mux_register_rx_callback(uart_mux_device_t dev,
                                  uart_mux_rx_callback callback)
{
    if (dev == UART_MUX_DEVICE_NONE)
        return -RT_EINVAL;

    rt_mutex_take(&mux_mutex, RT_WAITING_FOREVER);
    rx_callbacks[dev] = callback;
    rt_mutex_release(&mux_mutex);

    MUX_LOG("RX callback registered for device %d\n", dev);
    return RT_EOK;
}

int uart_mux_send(const uint8_t *data, size_t len)
{
    rt_size_t sent;

    if (data == RT_NULL || len == 0)
        return -RT_ERROR;

    rt_mutex_take(&mux_mutex, RT_WAITING_FOREVER);
    if (mux_serial == RT_NULL || current_device == UART_MUX_DEVICE_NONE)
    {
        rt_mutex_release(&mux_mutex);
        return -RT_ERROR;
    }

    sent = rt_device_write(mux_serial, 0, data, len);
    rt_mutex_release(&mux_mutex);

    if (sent != len)
        return -RT_ERROR;

    return (int)sent;
}

uart_mux_device_t uart_mux_get_current_device(void)
{
    uart_mux_device_t dev;

    rt_mutex_take(&mux_mutex, RT_WAITING_FOREVER);
    dev = current_device;
    rt_mutex_release(&mux_mutex);

    return dev;
}

void uart_mux_deinit(void)
{
    int i;

    rx_thread_running = RT_FALSE;

    rt_mutex_take(&mux_mutex, RT_WAITING_FOREVER);
    mux_close_uart();
    current_device = UART_MUX_DEVICE_NONE;
    memset(rx_callbacks, 0, sizeof(rx_callbacks));
    rt_mutex_release(&mux_mutex);

    rt_sem_release(&mux_rx_sem);

    for (i = 0; i < 10 && rx_thread != RT_NULL; i++)
        rt_thread_mdelay(10);

    if (rx_thread != RT_NULL)
    {
        rt_thread_delete(rx_thread);
        rx_thread = RT_NULL;
    }

    rt_sem_detach(&mux_rx_sem);
    rt_mutex_detach(&mux_mutex);
    MUX_LOG("Deinitialized\n");
}

static void uart_test(int argc, char **argv)
{
    int ret;

    if (argc < 2)
    {
        rt_kprintf("Usage: uart_test <string>\n");
        rt_kprintf("Example: uart_test hello\n");
        return;
    }

    if (uart_mux_get_current_device() == UART_MUX_DEVICE_NONE)
    {
        rt_kprintf("Error: No active UART device\n");
        rt_kprintf("Please switch to a device first using uart_mux_switch_to()\n");
        return;
    }

    ret = uart_mux_send((const uint8_t *)argv[1], strlen(argv[1]));
    if (ret < 0)
        rt_kprintf("Error: Failed to send data\n");
    else
        rt_kprintf("Successfully sent %d bytes: %s\n", ret, argv[1]);
}
MSH_CMD_EXPORT(uart_test, Test UART MUX send function);
