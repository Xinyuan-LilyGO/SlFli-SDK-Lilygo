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

/* 串口设备 */
static char mux_uart_name[RT_NAME_MAX];
static rt_device_t mux_serial = RT_NULL;
static struct serial_configure mux_config = RT_SERIAL_CONFIG_DEFAULT;

/* 当前激活设备 */
static uart_mux_device_t current_device = UART_MUX_DEVICE_NONE;

/* 接收线程 */
static rt_thread_t rx_thread = RT_NULL;
#define RX_THREAD_STACK_SIZE 1024
#define RX_THREAD_PRIORITY 12

/* 接收回调表 */
static uart_mux_rx_callback rx_callbacks[3] = {RT_NULL}; /* 索引对应 enum */

/* 互斥锁保护切换和发送 */
static struct rt_mutex mux_mutex;

/* 事件集用于通知接收线程退出 */
static struct rt_event mux_event;
#define EVENT_STOP_RX (1 << 0)

/* 硬件 GPIO 切换 */
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

/* 关闭串口 */
static void mux_close_uart(void)
{
    if (mux_serial != RT_NULL)
    {
        rt_device_close(mux_serial);
        mux_serial = RT_NULL;
        MUX_LOG("UART closed\n");
    }
}

/* 以指定波特率打开串口 */
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
    mux_config.bufsz = 512;
    rt_device_control(mux_serial, RT_DEVICE_CTRL_CONFIG, &mux_config);

    ret = rt_device_open(mux_serial,
                         RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
    if (ret != RT_EOK)
    {
        MUX_LOG("Open UART at %lu failed\n", baudrate);
        mux_serial = RT_NULL;
        return ret;
    }

    MUX_LOG("UART opened at %lu\n", baudrate);
    return RT_EOK;
}

/* 接收线程入口：不断读取串口数据并调用当前设备回调 */
static void uart_mux_rx_thread_entry(void *param)
{
    uint8_t ch;
    rt_size_t len;
    rt_uint32_t e;

    while (1)
    {
        /* 如果没有激活设备或串口未打开，跳过 */
        rt_mutex_take(&mux_mutex, RT_WAITING_FOREVER);
        rt_device_t ser = mux_serial;
        uart_mux_device_t dev = current_device;
        rt_mutex_release(&mux_mutex);

        if (ser == RT_NULL || dev == UART_MUX_DEVICE_NONE)
        {
            rt_thread_mdelay(20);
            continue;
        }

        /* 读取一个字节（阻塞超时短，避免卡死） */
        len = rt_device_read(ser, 0, &ch, 1);
        if (len == 1)
        {
            /* 调用对应设备的回调 */
            uart_mux_rx_callback cb = rx_callbacks[dev];
            if (cb != RT_NULL)
            {
                cb(ch);
            }
        }
        else
        {
            rt_thread_mdelay(10);
        }
    }

    MUX_LOG("RX thread exit\n");
}

/*----------------------------------------------------------------------------*/
/* 对外 API                                                                   */
/*----------------------------------------------------------------------------*/

int uart_mux_init(const char *uart_name)
{
    if (uart_name == RT_NULL)
        return -RT_EINVAL;

    rt_strncpy(mux_uart_name, uart_name, RT_NAME_MAX - 1);
    rt_mutex_init(&mux_mutex, "mux_mtx", RT_IPC_FLAG_PRIO);
    rt_event_init(&mux_event, "mux_evt", RT_IPC_FLAG_PRIO);

    /* 创建接收线程 */
    rx_thread =
        rt_thread_create("uart_mux_rx", uart_mux_rx_thread_entry, RT_NULL,
                         RX_THREAD_STACK_SIZE, RX_THREAD_PRIORITY, 10);
    if (rx_thread == RT_NULL)
    {
        rt_mutex_detach(&mux_mutex);
        rt_event_detach(&mux_event);
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

    /* 如果已经激活目标设备且波特率相同，则无需操作 */
    if (current_device == dev && mux_serial != RT_NULL &&
        mux_config.baud_rate == baudrate)
    {
        rt_mutex_release(&mux_mutex);
        MUX_LOG("Already active with same baudrate\n");
        return RT_EOK;
    }

    MUX_LOG("Switching to device %d, baudrate %lu\n", dev, baudrate);

    /* 1. 关闭当前串口 */
    mux_close_uart();

    /* 2. 切换硬件 GPIO */
    mux_hw_select(dev);

    /* 3. 以新波特率打开串口 */
    ret = mux_open_uart(baudrate);
    if (ret != RT_EOK)
    {
        current_device = UART_MUX_DEVICE_NONE;
        rt_mutex_release(&mux_mutex);
        return ret;
    }

    /* 4. 更新当前设备 */
    current_device = dev;

    rt_mutex_release(&mux_mutex);
    MUX_LOG("Switch completed\n");
    return RT_EOK;
}

int uart_mux_register_rx_callback(uart_mux_device_t dev,
                                  uart_mux_rx_callback callback)
{
    if (dev == UART_MUX_DEVICE_NONE || callback == RT_NULL)
        return -RT_EINVAL;

    rt_mutex_take(&mux_mutex, RT_WAITING_FOREVER);
    rx_callbacks[dev] = callback;
    rt_mutex_release(&mux_mutex);

    MUX_LOG("RX callback registered for device %d\n", dev);
    return RT_EOK;
}

int uart_mux_send(const uint8_t *data, size_t len)
{
    rt_device_t ser;
    rt_size_t sent;

    if (data == RT_NULL || len == 0)
        return 0;

    rt_mutex_take(&mux_mutex, RT_WAITING_FOREVER);
    ser = mux_serial;
    if (ser == RT_NULL || current_device == UART_MUX_DEVICE_NONE)
    {
        rt_mutex_release(&mux_mutex);
        return -RT_ERROR;
    }

    sent = rt_device_write(ser, 0, data, len);
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
    /* 通知接收线程停止 */
    rt_event_send(&mux_event, EVENT_STOP_RX);
    if (rx_thread != RT_NULL)
    {
        rt_thread_delete(rx_thread);
        rx_thread = RT_NULL;
    }

    rt_mutex_take(&mux_mutex, RT_WAITING_FOREVER);
    mux_close_uart();
    current_device = UART_MUX_DEVICE_NONE;
    memset(rx_callbacks, 0, sizeof(rx_callbacks));
    rt_mutex_release(&mux_mutex);

    rt_mutex_detach(&mux_mutex);
    rt_event_detach(&mux_event);
    MUX_LOG("Deinitialized\n");
}

static void uart_test(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("Usage: uart_test <string>\n");
        rt_kprintf("Example: uart_test hello\n");
        return;
    }

    /* 检查当前是否有激活的设备 */
    uart_mux_device_t current_dev = uart_mux_get_current_device();
    if (current_dev == UART_MUX_DEVICE_NONE)
    {
        rt_kprintf("Error: No active UART device\n");
        rt_kprintf(
            "Please switch to a device first using uart_mux_switch_to()\n");
        return;
    }

    // /* 发送数据 */
    // int ret = uart_mux_send((const uint8_t *)argv[1], strlen(argv[1]));
    // if (ret < 0)
    // {
    //     rt_kprintf("Error: Failed to send data\n");
    // }
    // else
    // {
    //     rt_kprintf("Successfully sent %d bytes: %s\n", ret, argv[1]);
    // }
    rt_size_t sent = rt_device_write(mux_serial, 0, (const uint8_t *)argv[1],
                                     strlen(argv[1]));
    if (sent != strlen(argv[1]))
        rt_kprintf("Error: Failed to send data\n");
    else
        rt_kprintf("Successfully sent %d bytes: %s\n", sent, argv[1]);
}
MSH_CMD_EXPORT(uart_test, Test UART MUX send function);
