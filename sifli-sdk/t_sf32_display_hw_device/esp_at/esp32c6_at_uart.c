#include "esp32c6_at_uart.h"
#include "uart_mux.h"
#include "xl9555.h"

/* ================================================================== */
/*  行缓冲区 (ISR 回调 -> 主处理线程)                                   */
/* ================================================================== */
#define RX_LINE_BUF_SIZE 256
#define LINE_QUEUE_SIZE  16

static char rx_line_buf[RX_LINE_BUF_SIZE];
static uint16_t rx_line_idx = 0;

static char line_queue[LINE_QUEUE_SIZE][RX_LINE_BUF_SIZE];
static volatile int line_queue_wr = 0;
static int line_queue_rd = 0;

/* ================================================================== */
/*  AT 命令队列 (应用层 -> 串口, 顺序发送)                             */
/* ================================================================== */
#define CMD_QUEUE_SIZE 16

typedef struct {
    char data[256];
    uint16_t len;
    int resp_id;
} cmd_entry_t;

static cmd_entry_t cmd_queue[CMD_QUEUE_SIZE];
static int cmd_queue_wr = 0;
static int cmd_queue_rd = 0;

/* 当前正在执行的命令 */
static bool cmd_busy = false;
static int current_resp_id = -1;
static char current_resp[256];
static uint16_t current_resp_len = 0;

static rt_sem_t at_sem;                /* 统一信号量：新行/新命令 */
static at_response_callback_t user_callback = NULL;

/* ================================================================== */
/*  硬件操作                                                           */
/* ================================================================== */
static void esp32c6_en(void)
{
    xl9555_pin_mode(XL9555_WIFI_EN_PIN, XL9555_PIN_OUTPUT);
    xl9555_digital_write(XL9555_WIFI_EN_PIN, 1);
    rt_thread_mdelay(100);
}

static void esp32c6_disable(void)
{
    xl9555_pin_mode(XL9555_WIFI_EN_PIN, XL9555_PIN_OUTPUT);
    xl9555_digital_write(XL9555_WIFI_EN_PIN, 0);
    rt_thread_mdelay(200);
}

static void esp32c6_reset(void)
{
    xl9555_pin_mode(XL9555_WIFI_RST_PIN, XL9555_PIN_OUTPUT);
    xl9555_digital_write(XL9555_WIFI_RST_PIN, 0);
    rt_thread_mdelay(500);
    xl9555_digital_write(XL9555_WIFI_RST_PIN, 1);
    rt_thread_mdelay(500);
}

/* ================================================================== */
/*  响应行解析                                                          */
/* ================================================================== */

static int check_response_terminal(const char *line)
{
    if (strcmp(line, "OK") == 0)
        return AT_RESULT_OK;
    if (strcmp(line, "ERROR") == 0)
        return AT_RESULT_ERROR;
    if (strncmp(line, "+CME ERROR:", 11) == 0)
        return AT_RESULT_ERROR;
    if (strncmp(line, "+CMS ERROR:", 11) == 0)
        return AT_RESULT_ERROR;
    return -1;
}

static void process_response_line(const char *line)
{
    if (line[0] == '\0')
        return;

    /* 没有正在执行的命令 -> URC */
    if (!cmd_busy)
    {
        AT_LOG("URC: %s\n", line);
        if (user_callback)
            user_callback(-1, line, AT_RESULT_URC);
        return;
    }

    /* 追加到当前响应缓冲区（多行用 \n 分隔） */
    {
        int len = strlen(line);
        if (current_resp_len + len + 2 < (int)sizeof(current_resp))
        {
            if (current_resp_len > 0)
                current_resp[current_resp_len++] = '\n';
            memcpy(current_resp + current_resp_len, line, len);
            current_resp_len += len;
            current_resp[current_resp_len] = '\0';
        }
    }

    /* 检查终止行 */
    {
        int terminal = check_response_terminal(line);
        if (terminal >= 0)
        {
            AT_LOG("CMD complete, ID: %d, result: %d\n%s\n",
                   current_resp_id, terminal, current_resp);

            if (user_callback)
                user_callback(current_resp_id, current_resp, terminal);

            /* 清除当前命令状态 */
            current_resp_id = -1;
            current_resp_len = 0;
            current_resp[0] = '\0';
            cmd_busy = false;
        }
    }
}

/* ================================================================== */
/*  UART 接收回调 (中断上下文)                                          */
/* ================================================================== */
static void at_rx_callback(uint8_t data)
{
    if (data == '\n')
    {
        if (rx_line_idx > 0 && rx_line_buf[rx_line_idx - 1] == '\r')
            rx_line_buf[rx_line_idx - 1] = '\0';
        else
            rx_line_buf[rx_line_idx] = '\0';
        rx_line_idx = 0;

        /* 存入行队列 */
        int next = (line_queue_wr + 1) % LINE_QUEUE_SIZE;
        if (next != line_queue_rd)
        {
            memcpy(line_queue[line_queue_wr], rx_line_buf, RX_LINE_BUF_SIZE);
            line_queue_wr = next;
            rt_sem_release(at_sem);
        }
    }
    else
    {
        if (rx_line_idx < RX_LINE_BUF_SIZE - 1)
            rx_line_buf[rx_line_idx++] = data;
    }
}

/* ================================================================== */
/*  主处理线程 (处理响应 + 发送命令)                                    */
/* ================================================================== */
static void at_main_thread(void *parameter)
{
    char line[RX_LINE_BUF_SIZE];

    AT_LOG("at_main_thread started\n");
    while (1)
    {
        rt_sem_take(at_sem, RT_WAITING_FOREVER);

        /* 1. 处理所有已收到的行 */
        while (line_queue_rd != line_queue_wr)
        {
            memcpy(line, line_queue[line_queue_rd], RX_LINE_BUF_SIZE);
            line_queue_rd = (line_queue_rd + 1) % LINE_QUEUE_SIZE;
            process_response_line(line);
        }

        /* 2. 空闲且有排队命令 -> 发送下一个 */
        if (!cmd_busy && (cmd_queue_wr != cmd_queue_rd))
        {
            cmd_entry_t *entry = &cmd_queue[cmd_queue_rd];
            cmd_queue_rd = (cmd_queue_rd + 1) % CMD_QUEUE_SIZE;

            cmd_busy = true;
            current_resp_id = entry->resp_id;
            current_resp_len = 0;
            current_resp[0] = '\0';

            AT_LOG("Send ID:%d -> %s", entry->resp_id, entry->data);
            uart_mux_send((const uint8_t *)entry->data, entry->len);
        }
    }
}

/* ================================================================== */
/*  对外 API                                                            */
/* ================================================================== */

void at_async_register_callback(at_response_callback_t cb)
{
    user_callback = cb;
}

bool at_async_is_busy(void)
{
    return cmd_busy || (cmd_queue_wr != cmd_queue_rd);
}

rt_err_t at_async_init(const char *uart_name, uint32_t baudrate)
{
    (void)uart_name;

    esp32c6_en();
    esp32c6_reset();

    uart_mux_register_rx_callback(UART_MUX_DEVICE_ESP32C6, at_rx_callback);

    if (uart_mux_switch_to(UART_MUX_DEVICE_ESP32C6, baudrate) != RT_EOK)
    {
        AT_LOG("Failed to switch to ESP32C6\n");
        return -RT_ERROR;
    }

    /* 初始化状态 */
    cmd_queue_wr = 0;
    cmd_queue_rd = 0;
    line_queue_wr = 0;
    line_queue_rd = 0;
    cmd_busy = false;
    current_resp_id = -1;
    current_resp_len = 0;
    current_resp[0] = '\0';

    at_sem = rt_sem_create("at_sem", 0, RT_IPC_FLAG_FIFO);
    if (at_sem == RT_NULL)
    {
        AT_LOG("Failed to create at_sem\n");
        return -RT_ERROR;
    }

    rt_thread_t at_main = rt_thread_create("at_main", at_main_thread, RT_NULL,
                                           2048, RT_THREAD_PRIORITY_MIDDLE, 20);
    if (at_main != RT_NULL)
        rt_thread_startup(at_main);
    else
        AT_LOG("Failed to create at_main thread\n");

    return RT_EOK;
}

void at_async_deinit(void)
{
    uart_mux_register_rx_callback(UART_MUX_DEVICE_ESP32C6, RT_NULL);
    AT_LOG("Deinitialized\n");
}

rt_err_t esp32_at_send(const uint8_t *data, size_t len, int resp_id)
{
    if (data == RT_NULL || len == 0)
        return -RT_ERROR;

    /* 入队等待顺序发送 */
    int next = (cmd_queue_wr + 1) % CMD_QUEUE_SIZE;
    if (next == cmd_queue_rd)
    {
        AT_LOG("Command queue full\n");
        return -RT_ERROR;
    }

    cmd_entry_t *entry = &cmd_queue[cmd_queue_wr];
    uint16_t copy_len = (len < sizeof(entry->data)) ? (uint16_t)len
                                                    : (uint16_t)sizeof(entry->data);
    memcpy(entry->data, data, copy_len);
    entry->len = copy_len;
    entry->resp_id = resp_id;

    cmd_queue_wr = next;

    rt_sem_release(at_sem);
    return RT_EOK;
}
