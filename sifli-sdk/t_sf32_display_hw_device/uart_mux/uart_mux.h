#ifndef __UART_MUX_H__
#define __UART_MUX_H__

#include <rtthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 设备类型 */
typedef enum {
    UART_MUX_DEVICE_NONE = 0,
    UART_MUX_DEVICE_GPS,
    UART_MUX_DEVICE_ESP32C6
} uart_mux_device_t;

/* 数据接收回调函数类型 */
typedef void (*uart_mux_rx_callback)(uint8_t data);

/**
 * 初始化串口多路复用管理器
 * @param uart_name 物理串口设备名称，如 "uart2"
 * @return RT_EOK 成功，其他失败
 */
int uart_mux_init(const char *uart_name);

/**
 * 切换到指定设备，并设置对应波特率
 * @param dev      目标设备
 * @param baudrate 目标波特率
 * @return RT_EOK 成功，其他失败
 */
int uart_mux_switch_to(uart_mux_device_t dev, uint32_t baudrate);

/**
 * 注册指定设备的数据接收回调
 * @param dev      设备类型
 * @param callback 回调函数指针，每收到一个字节调用一次
 * @return RT_EOK 成功
 */
int uart_mux_register_rx_callback(uart_mux_device_t dev, uart_mux_rx_callback callback);

/**
 * 发送数据到当前激活的设备（阻塞发送）
 * @param data 数据缓冲区
 * @param len  数据长度
 * @return 实际发送的字节数，失败返回负数
 */
int uart_mux_send(const uint8_t *data, size_t len);

/**
 * 获取当前激活的设备类型
 */
uart_mux_device_t uart_mux_get_current_device(void);

/**
 * 反初始化
 */
void uart_mux_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __UART_MUX_H__ */