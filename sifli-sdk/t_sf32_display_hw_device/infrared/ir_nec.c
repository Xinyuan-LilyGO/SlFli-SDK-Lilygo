#include "ir_nec.h"
#include "infrared.h"
#include <rtdevice.h>
#include "rthw.h"

// 时间宏定义（单位：微秒）
#define NEC_HIGH_US    (9000)      // 引导码高电平 9ms
#define NEC_LOW_US     (4500)      // 引导码低电平 4.5ms
#define NEC_BIT_0_HIGH (560)       // 位 "0" 高电平 560us
#define NEC_BIT_0_LOW  (560)       // 位 "0" 低电平 560us
#define NEC_BIT_1_HIGH (560)       // 位 "1" 高电平 560us
#define NEC_BIT_1_LOW  (1690)      // 位 "1" 低电平 1.69ms
#define NEC_REPEAT_HIGH (9000)     // 重复码高电平 9ms
#define NEC_REPEAT_LOW  (2250)     // 重复码低电平 2.25ms
#define NEC_END_HIGH    (560)      // 结束位高电平 560us

static void us_delay(uint32_t us)
{
    rt_hw_us_delay(us);
}

// 发送一个电平（通过开关 PWM 实现载波发送）
static void send_level(int level, uint32_t duration_us)
{
    if (level) {
        infrared_pwm_enable();   // 高电平 -> 发射 38kHz 载波
    } else {
        infrared_pwm_disable();  // 低电平 -> 关闭载波
    }
    us_delay(duration_us);
}

// 发送 NEC 的一个位（0 或 1）
static void send_bit(uint8_t bit)
{
    if (bit) {
        send_level(1, NEC_BIT_1_HIGH);
        send_level(0, NEC_BIT_1_LOW);
    } else {
        send_level(1, NEC_BIT_0_HIGH);
        send_level(0, NEC_BIT_0_LOW);
    }
}

// 发送 8 位数据（低位在前）
static void send_byte(uint8_t data)
{
    for (int i = 0; i < 8; i++) {
        send_bit((data >> i) & 0x01);
    }
}

int ir_nec_init(void)
{
    return infrared_init();
}

int ir_nec_send(uint8_t address, uint8_t command)
{
    // 1. 发送引导码
    send_level(1, NEC_HIGH_US);
    send_level(0, NEC_LOW_US);

    // 2. 发送地址码和地址反码
    send_byte(address);
    send_byte(~address);

    // 3. 发送命令码和命令反码
    send_byte(command);
    send_byte(~command);

    // 4. 发送结束位（可选，有些接收器不要求）
    send_level(1, NEC_END_HIGH);
    send_level(0, 0);          // 关闭载波

    return 0;
}

void ir_nec_send_repeat(void)
{
    send_level(1, NEC_REPEAT_HIGH);
    send_level(0, NEC_REPEAT_LOW);
    send_level(1, NEC_END_HIGH);
    send_level(0, 0);
}