#ifndef __ESP32C6_H__
#define __ESP32C6_H__

#include <rtthread.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 错误码 */
#define AT_ERR_OK           0
#define AT_ERR_TIMEOUT      -1
#define AT_ERR_NO_DEV       -2
#define AT_ERR_SEND         -3
#define AT_ERR_NO_RESP      -4
#define AT_ERR_PARSE        -5
#define AT_ERR_BUSY         -6      /* 挂起命令表已满 */

/* 内部事件标志（仅用于唤醒解析线程） */
#define AT_EVENT_RX_DATA    (1 << 0)

/* AT 响应消息最大数据长度 */
#define AT_MSG_MAX_DATA_LEN 256

/* AT 响应消息结构体 */
struct at_msg
{
    uint32_t cmd_id;                    /* 用户自定义命令标识 */
    char data[AT_MSG_MAX_DATA_LEN];     /* 匹配到的响应行内容 */
};
typedef struct at_msg at_msg_t;

enum {
    CMD_GET_NTP = 1,
    CMD_GET_WIFI_STATUS,
    CMD_GET_IP
};

/* 初始化 AT 组件 */
int at_async_init(const char *uart_name, uint32_t baudrate);

/* 反初始化 */
void at_async_deinit(void);

/**
 * 发送 AT 命令（非阻塞）
 * @param cmd           命令字符串（自动添加 \r\n）
 * @param expected_key  期望响应的关键字（例如 "OK", "+CIPSNTPTIME:"），若为 NULL 则匹配任意行
 * @param cmd_id        用户自定义命令标识，用于在响应消息中区分不同命令
 * @return              AT_ERR_OK 成功，否则失败
 */
int at_async_send_cmd(const char *cmd, const char *expected_key, uint32_t cmd_id);

/**
 * 接收 AT 响应消息（阻塞）
 * @param msg           输出参数，存放接收到的消息
 * @param timeout_ms    超时时间（毫秒）
 * @return              RT_EOK 成功，-RT_ETIMEOUT 超时，其他错误码
 */
rt_err_t at_async_recv_msg(at_msg_t *msg, int timeout_ms);

/**
 * 取消一个挂起的命令（释放槽位，防止泄漏）
 * @param cmd_id        要取消的命令标识
 */
void at_async_cancel_cmd(uint32_t cmd_id);

#ifdef __cplusplus
}
#endif

#endif /* __ESP32C6_H__ */