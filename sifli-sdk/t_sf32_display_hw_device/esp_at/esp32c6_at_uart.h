#ifndef __ESP32C6_AT_UART_H__
#define __ESP32C6_AT_UART_H__
#include <rtdevice.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define AT_ASYNC_DEBUG 1
#if AT_ASYNC_DEBUG
    #define AT_LOG(...) rt_kprintf("[AT] " __VA_ARGS__)
#else
    #define AT_LOG(...)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /* AT 命令响应结果类型 */
    #define AT_RESULT_OK      0   /* 命令成功 */
    #define AT_RESULT_ERROR   1   /* 命令失败 */
    #define AT_RESULT_URC     2   /* 主动上报，resp_id = -1 */

    /**
     * 应用层响应回调
     * @param resp_id  命令 ID（发送时传入），URC 时为 -1
     * @param response 完整响应字符串（多行用 \n 分隔）
     * @param result   AT_RESULT_OK / ERROR / URC
     */
    typedef void (*at_response_callback_t)(int resp_id, const char *response, int result);

    rt_err_t at_async_init(const char *uart_name, uint32_t baudrate);
    void at_async_deinit(void);
    void at_async_register_callback(at_response_callback_t cb);

    /**
     * 发送 AT 命令（非阻塞，入队后等待顺序发送）
     * @param data    命令字符串（需包含 \r\n）
     * @param len     数据长度
     * @param resp_id 命令 ID，回调时原样返回
     * @return RT_EOK 入队成功，其他失败
     */
    rt_err_t esp32_at_send(const uint8_t *data, size_t len, int resp_id);

    /**
     * 查询当前是否有命令正在执行
     * @return true 有命令在等待响应，false 空闲
     */
    bool at_async_is_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* __ESP32C6_AT_UART_H__ */
