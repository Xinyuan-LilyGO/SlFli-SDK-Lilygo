#include "esp32c6.h"
#include "xl9555.h"
#include "uart_mux.h"

#include <rtdevice.h>
#include <stdlib.h>
#include <string.h>

/* 调试输出 */
#define AT_ASYNC_DEBUG 1
#if AT_ASYNC_DEBUG
    #define AT_LOG(...) rt_kprintf("[AT] " __VA_ARGS__)
#else
    #define AT_LOG(...)
#endif

/* 消息队列句柄 */
static rt_mq_t at_msg_mq = RT_NULL;

/* 接收行缓冲区 */
#define RX_LINE_BUF_SIZE 512
static char rx_line_buf[RX_LINE_BUF_SIZE];
static uint16_t rx_line_idx = 0;

/* 挂起命令表 */
#define MAX_PENDING_CMDS 4
struct at_pending_cmd
{
    uint32_t cmd_id;
    char expected_key[64];
    rt_bool_t used;
};
static struct at_pending_cmd pending_cmds[MAX_PENDING_CMDS];
static struct rt_mutex pending_mutex;

/* 内部函数 */
static void process_line(char *line);

/* 硬件初始化 */
static void esp32c6_en(void)
{
    xl9555_pin_mode(XL9555_WIFI_EN_PIN, XL9555_PIN_OUTPUT);
    xl9555_digital_write(XL9555_WIFI_EN_PIN, 1);
    rt_thread_mdelay(100);
}

static void esp32c6_reset(void)
{
    xl9555_pin_mode(XL9555_WIFI_RST_PIN, XL9555_PIN_OUTPUT);
    xl9555_digital_write(XL9555_WIFI_RST_PIN, 0);
    rt_thread_mdelay(100);
    xl9555_digital_write(XL9555_WIFI_RST_PIN, 1);
    rt_thread_mdelay(500);
}

/* 接收回调：uart_mux 每收到一个字节会调用此函数 */
static void at_rx_callback(uint8_t data)
{
    if (data == '\n')
    {
        if (rx_line_idx > 0 && rx_line_buf[rx_line_idx - 1] == '\r')
            rx_line_buf[rx_line_idx - 1] = '\0';
        else
            rx_line_buf[rx_line_idx] = '\0';

        AT_LOG("RX: %s\n", rx_line_buf);
        process_line(rx_line_buf);
        rx_line_idx = 0;
    }
    else
    {
        if (rx_line_idx < RX_LINE_BUF_SIZE - 1)
            rx_line_buf[rx_line_idx++] = data;
    }
}

static void process_line(char *line)
{
    int i;
    at_msg_t msg;
    rt_bool_t matched = RT_FALSE;
    uint32_t matched_cmd_id = 0;

    if (line == RT_NULL || strlen(line) == 0)
        return;

    rt_mutex_take(&pending_mutex, 2000);
    for (i = 0; i < MAX_PENDING_CMDS; i++)
    {
        if (pending_cmds[i].used)
        {
            if (strlen(pending_cmds[i].expected_key) > 0 &&
                strstr(line, pending_cmds[i].expected_key) != RT_NULL)
            {
                matched = RT_TRUE;
                matched_cmd_id = pending_cmds[i].cmd_id;
                pending_cmds[i].used = RT_FALSE;
                break;
            }
        }
    }
    rt_mutex_release(&pending_mutex);

    if (matched)
    {
        msg.cmd_id = matched_cmd_id;
        strncpy(msg.data, line, AT_MSG_MAX_DATA_LEN - 1);
        msg.data[AT_MSG_MAX_DATA_LEN - 1] = '\0';

        if (rt_mq_send(at_msg_mq, &msg, sizeof(at_msg_t)) != RT_EOK)
        {
            AT_LOG("Message queue full, drop response for cmd_id %u\n", matched_cmd_id);
        }
        else
        {
            AT_LOG("Response for cmd_id %u enqueued\n", matched_cmd_id);
        }
    }
}


/* 初始化 AT 组件 */
int at_async_init(const char *uart_name, uint32_t baudrate)
{
    esp32c6_en();
    esp32c6_reset();

    /* 创建消息队列 */
    at_msg_mq = rt_mq_create("at_mq", sizeof(at_msg_t), 10, RT_IPC_FLAG_PRIO);
    if (at_msg_mq == RT_NULL)
    {
        AT_LOG("Create message queue failed\n");
        return AT_ERR_NO_DEV;
    }

    rt_mutex_init(&pending_mutex, "pend_mtx", RT_IPC_FLAG_PRIO);
    // memset(pending_cmds, 0, sizeof(pending_cmds));
    for (int i = 0; i < MAX_PENDING_CMDS; i++)
    {
        pending_cmds[i].used = RT_FALSE;
        pending_cmds[i].expected_key[0] = '\0';
    }

    /* 注册接收回调 */
    uart_mux_register_rx_callback(UART_MUX_DEVICE_ESP32C6, at_rx_callback);

    /* 切换到 ESP32C6 通路 */
    if (uart_mux_switch_to(UART_MUX_DEVICE_ESP32C6, baudrate) != RT_EOK)
    {
        AT_LOG("Failed to switch to ESP32C6\n");
        rt_mq_delete(at_msg_mq);
        rt_mutex_detach(&pending_mutex);
        at_msg_mq = RT_NULL;
        return AT_ERR_NO_DEV;
    }

    AT_LOG("Initialized, baud=%lu\n", baudrate);
    return AT_ERR_OK;
}

void at_async_deinit(void)
{
    uart_mux_register_rx_callback(UART_MUX_DEVICE_ESP32C6, RT_NULL);
    if (at_msg_mq)
    {
        rt_mq_delete(at_msg_mq);
        at_msg_mq = RT_NULL;
    }
    rt_mutex_detach(&pending_mutex);
    memset(pending_cmds, 0, sizeof(pending_cmds));
    AT_LOG("Deinitialized\n");
}

int at_async_send_cmd(const char *cmd, const char *expected_key, uint32_t cmd_id)
{
    int i, free_slot = -1;
    char full_cmd[128];

    if (cmd == RT_NULL)
        return AT_ERR_NO_DEV;

    rt_mutex_take(&pending_mutex, RT_WAITING_FOREVER);
    for (i = 0; i < MAX_PENDING_CMDS; i++)
    {
        if (!pending_cmds[i].used)
        {
            free_slot = i;
            break;
        }
    }

    if (free_slot == -1)
    {
        rt_mutex_release(&pending_mutex);
        AT_LOG("Too many pending commands\n");
        return AT_ERR_BUSY;
    }

    pending_cmds[free_slot].cmd_id = cmd_id;
    if (expected_key != RT_NULL)
    {
        strncpy(pending_cmds[free_slot].expected_key, expected_key,
                sizeof(pending_cmds[free_slot].expected_key) - 1);
        pending_cmds[free_slot].expected_key[sizeof(pending_cmds[free_slot].expected_key) - 1] = '\0';
    }
    else
    {
        pending_cmds[free_slot].expected_key[0] = '\0';
    }
    pending_cmds[free_slot].used = RT_TRUE;
    rt_mutex_release(&pending_mutex);

    rt_snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", cmd);
    int sent = uart_mux_send((const uint8_t *)full_cmd, strlen(full_cmd));
    if (sent != (int)strlen(full_cmd))
    {
        rt_mutex_take(&pending_mutex, RT_WAITING_FOREVER);
        pending_cmds[free_slot].used = RT_FALSE;
        rt_mutex_release(&pending_mutex);
        AT_LOG("Send command failed\n");
        return AT_ERR_SEND;
    }

    AT_LOG("Sent: %s", full_cmd);
    return AT_ERR_OK;
}

rt_err_t at_async_recv_msg(at_msg_t *msg, int timeout_ms)
{
    if (at_msg_mq == RT_NULL || msg == RT_NULL)
        return -RT_ERROR;

    return rt_mq_recv(at_msg_mq, msg, sizeof(at_msg_t),
                      rt_tick_from_millisecond(timeout_ms));
}

void at_async_cancel_cmd(uint32_t cmd_id)
{
    int i;
    rt_mutex_take(&pending_mutex, RT_WAITING_FOREVER);
    for (i = 0; i < MAX_PENDING_CMDS; i++)
    {
        if (pending_cmds[i].used && pending_cmds[i].cmd_id == cmd_id)
        {
            pending_cmds[i].used = RT_FALSE;
            AT_LOG("Cancelled pending cmd_id %u\n", cmd_id);
            break;
        }
    }
    rt_mutex_release(&pending_mutex);
}