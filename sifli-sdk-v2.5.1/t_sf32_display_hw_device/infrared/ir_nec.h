#ifndef __IR_NEC_H__
#define __IR_NEC_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 NEC 协议发送（会调用 infrared_init）
 * @return 0: 成功, -1: 失败
 */
int ir_nec_init(void);

/**
 * @brief 发送 NEC 格式的红外码
 * @param address  地址码（通常为 8 位）
 * @param command  命令码（8 位）
 * @return 0: 成功, -1: 失败
 */
int ir_nec_send(uint8_t address, uint8_t command);

/**
 * @brief 发送 NEC 重复码（长按时每 110ms 调用一次）
 */
void ir_nec_send_repeat(void);

#ifdef __cplusplus
}
#endif

#endif