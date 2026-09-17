#include "esp32c6_at_uart.h"
#include "uart_mux.h"
#include "xl9555.h"

/* ================================================================== */
/*  行缓冲区 (ISR 回调 -> 主处理线程)                                   */
/* ================================================================== */
#define RX_LINE_BUF_SIZE 256
#define LINE_QUEUE_SIZE 16

static char rx_line_buf[RX_LINE_BUF_SIZE];
static uint16_t rx_line_idx = 0;

static char line_queue[LINE_QUEUE_SIZE][RX_LINE_BUF_SIZE];
static volatile int line_queue_wr = 0;
static int line_queue_rd = 0;

/* ================================================================== */
/*  AT 命令队列 (应用层 -> 串口, 顺序发送)                             */
/* ================================================================== */
#define CMD_QUEUE_SIZE 16
#define CURRENT_RESP_SIZE 2048

typedef struct
{
    char data[256];
    uint16_t len;
    int resp_id;
} cmd_entry_t;

static cmd_entry_t *cmd_queue;
static int cmd_queue_wr = 0;
static int cmd_queue_rd = 0;

/* 当前正在执行的命令 */
static bool cmd_busy = false;
static int current_resp_id = -1;
/* Wi-Fi scans return one line per AP. Keep the complete response available
 * to the service parser instead of silently truncating it after one AP. */
static char *current_resp;
static uint16_t current_resp_len = 0;

static rt_sem_t at_sem; /* 统一信号量：新行/新命令 */
static rt_thread_t at_main;
static volatile bool at_running;
static at_response_callback_t user_callback = NULL;
static at_calloc_callback_t at_calloc = rt_calloc;
static at_free_callback_t at_free = rt_free;

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
        if (current_resp != RT_NULL &&
                current_resp_len + len + 2 < CURRENT_RESP_SIZE)
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
            AT_LOG("CMD complete, ID: %d, result: %d\n%s\n", current_resp_id,
                   terminal, current_resp);

            if (user_callback)
                user_callback(current_resp_id, current_resp, terminal);
                
#ifdef RT_USING_PM
            rt_pm_release(PM_SLEEP_MODE_IDLE); // 新增：命令完成，允许深度睡眠
#endif
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
    while (at_running)
    {
        rt_sem_take(at_sem, RT_WAITING_FOREVER);
        if (!at_running)
            break;

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
#ifdef RT_USING_PM
            rt_pm_request(PM_SLEEP_MODE_IDLE); // 新增：阻止进入高于 IDLE 的睡眠
#endif
            AT_LOG("Send ID:%d -> %s", entry->resp_id, entry->data);
            uart_mux_send((const uint8_t *)entry->data, entry->len);
        }
    }
    at_main = RT_NULL;
}

/* ================================================================== */
/*  对外 API                                                            */
/* ================================================================== */

void at_async_register_callback(at_response_callback_t cb)
{
    user_callback = cb;
}

rt_err_t at_async_set_allocator(at_calloc_callback_t calloc_callback,
                                at_free_callback_t free_callback)
{
    if (calloc_callback == RT_NULL || free_callback == RT_NULL)
        return -RT_EINVAL;
    if (at_main != RT_NULL || cmd_queue != RT_NULL ||
            current_resp != RT_NULL)
    {
        return -RT_EBUSY;
    }

    at_calloc = calloc_callback;
    at_free = free_callback;
    return RT_EOK;
}

bool at_async_is_busy(void)
{
    if (!at_running || cmd_queue == RT_NULL)
        return false;
    return cmd_busy || (cmd_queue_wr != cmd_queue_rd);
}

rt_err_t at_async_init(const char *uart_name, uint32_t baudrate)
{
    (void)uart_name;

    if (at_main != RT_NULL)
        return -RT_EBUSY;

    cmd_queue = at_calloc(CMD_QUEUE_SIZE, sizeof(*cmd_queue));
    current_resp = at_calloc(CURRENT_RESP_SIZE,
                             sizeof(*current_resp));
    if (cmd_queue == RT_NULL || current_resp == RT_NULL)
    {
        at_free(cmd_queue);
        at_free(current_resp);
        cmd_queue = RT_NULL;
        current_resp = RT_NULL;
        AT_LOG("Failed to allocate PSRAM buffers\n");
        return -RT_ENOMEM;
    }

    at_sem = rt_sem_create("at_sem", 0, RT_IPC_FLAG_FIFO);
    if (at_sem == RT_NULL)
    {
        AT_LOG("Failed to create at_sem\n");
        at_free(cmd_queue);
        at_free(current_resp);
        cmd_queue = RT_NULL;
        current_resp = RT_NULL;
        return -RT_ENOMEM;
    }

    esp32c6_en();
    esp32c6_reset();

    uart_mux_register_rx_callback(UART_MUX_DEVICE_ESP32C6, at_rx_callback);

    if (uart_mux_switch_to(UART_MUX_DEVICE_ESP32C6, baudrate) != RT_EOK)
    {
        AT_LOG("Failed to switch to ESP32C6\n");
        uart_mux_register_rx_callback(UART_MUX_DEVICE_ESP32C6, RT_NULL);
        at_free(cmd_queue);
        at_free(current_resp);
        cmd_queue = RT_NULL;
        current_resp = RT_NULL;
        rt_sem_delete(at_sem);
        at_sem = RT_NULL;
        return -RT_ERROR;
    }

    /* 初始化状态 */
    cmd_queue_wr = 0;
    cmd_queue_rd = 0;
    line_queue_wr = 0;
    line_queue_rd = 0;
#ifdef RT_USING_PM
    if (cmd_busy)
        rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
    cmd_busy = false;
    current_resp_id = -1;
    current_resp_len = 0;
    current_resp[0] = '\0';

    at_running = true;
    at_main = rt_thread_create("at_main", at_main_thread, RT_NULL,
                               2048, RT_THREAD_PRIORITY_MIDDLE, 20);
    if (at_main != RT_NULL)
        rt_thread_startup(at_main);
    else
    {
        AT_LOG("Failed to create at_main thread\n");
        at_running = false;
        uart_mux_register_rx_callback(UART_MUX_DEVICE_ESP32C6, RT_NULL);
        rt_sem_delete(at_sem);
        at_sem = RT_NULL;
        at_free(cmd_queue);
        at_free(current_resp);
        cmd_queue = RT_NULL;
        current_resp = RT_NULL;
        return -RT_ENOMEM;
    }

    return RT_EOK;
}

void at_async_deinit(void)
{
    uint32_t wait_count = 50U;

    uart_mux_register_rx_callback(UART_MUX_DEVICE_ESP32C6, RT_NULL);
    at_running = false;
    if (at_sem != RT_NULL)
        rt_sem_release(at_sem);
    while (at_main != RT_NULL && wait_count-- > 0U)
        rt_thread_mdelay(10U);
    if (at_main != RT_NULL)
    {
        AT_LOG("AT thread stop timeout, keep PSRAM buffers\n");
        return;
    }
    if (at_sem != RT_NULL)
    {
        rt_sem_delete(at_sem);
        at_sem = RT_NULL;
    }
    at_free(cmd_queue);
    at_free(current_resp);
    cmd_queue = RT_NULL;
    current_resp = RT_NULL;
    current_resp_len = 0U;
    cmd_queue_wr = 0;
    cmd_queue_rd = 0;
    line_queue_wr = 0;
    line_queue_rd = 0;
#ifdef RT_USING_PM
    if (cmd_busy)
        rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
    cmd_busy = false;
    AT_LOG("Deinitialized\n");
}

rt_err_t esp32_at_send(const uint8_t *data, size_t len, int resp_id)
{
    if (data == RT_NULL || len == 0 || !at_running ||
            cmd_queue == RT_NULL || at_sem == RT_NULL)
        return -RT_ERROR;

    /* 入队等待顺序发送 */
    int next = (cmd_queue_wr + 1) % CMD_QUEUE_SIZE;
    if (next == cmd_queue_rd)
    {
        AT_LOG("Command queue full\n");
        return -RT_ERROR;
    }

    cmd_entry_t *entry = &cmd_queue[cmd_queue_wr];
    uint16_t copy_len = (len < sizeof(entry->data))
                            ? (uint16_t)len
                            : (uint16_t)sizeof(entry->data);
    memcpy(entry->data, data, copy_len);
    entry->len = copy_len;
    entry->resp_id = resp_id;

    cmd_queue_wr = next;

    rt_sem_release(at_sem);
    return RT_EOK;
}
