/**
 * @file tca8418.h
 * @brief TCA8418 I2C keypad controller driver header
 * @details This header file contains the declarations and definitions for the
 *          TCA8418 I2C keypad controller driver, supporting various
 *          keypad operations.
 * @author Cengiz Sinan Kostakoglu
 * @version 1.0
 * @date 2025-06-08
 */

#ifndef __TCA8418_H__
#define __TCA8418_H__

#ifdef __cplusplus
extern "C" {
#endif

/* For uint8_t, uint16_t, uint32_t */
#include <stdint.h> 
#include <stdbool.h>
/* For HAL functions */
#include "rtthread.h"
#include <rtdevice.h>


/* TCA8418 I2C Address */
#define TCA8418_ADDRESS 0x34

/* Register addresses */
#define CFG 0x01            //< Configuration Register
#define INT_STAT 0x02       //< Interrupt Status Register
#define KEY_LCK_EC 0x03     //< Key Lock AND Event Counter Register
#define KEY_EVENT_A 0x04    //< Key Event A Register
#define KEY_EVENT_B 0x05    //< Key Event B Register
#define KEY_EVENT_C 0x06    //< Key Event C Register
#define KEY_EVENT_D 0x07    //< Key Event D Register
#define KEY_EVENT_E 0x08    //< Key Event E Register
#define KEY_EVENT_F 0x09    //< Key Event F Register
#define KEY_EVENT_G 0x0A    //< Key Event G Register
#define KEY_EVENT_H 0x0B    //< Key Event H Register
#define KEY_EVENT_I 0x0C    //< Key Event I Register
#define KEY_EVENT_J 0x0D    //< Key Event J Register
#define KP_LCK_TIMER 0x0E   //< Keypad Lock 1 to Lock 2 Timer
#define UNLOCK1 0x0F        //< Unlock 1 Register
#define UNLOCK2 0x10        //< Unlock 2 Register
#define GPIO_INT_STAT1 0x11 //< GPIO Interrupt Status 1 Register
#define GPIO_INT_STAT2 0x12 //< GPIO Interrupt Status 2 Register
#define GPIO_INT_STAT3 0x13 //< GPIO Interrupt Status 3 Register
#define GPIO_DAT_STAT1 0x14 //< GPIO Data Status 1 Register
#define GPIO_DAT_STAT2 0x15 //< GPIO Data Status 2 Register
#define GPIO_DAT_STAT3 0x16 //< GPIO Data Status 3 Register
#define GPIO_DAT_OUT1 0x17  //< GPIO Data Output 1 Register
#define GPIO_DAT_OUT2 0x18  //< GPIO Data Output 2 Register
#define GPIO_DAT_OUT3 0x19  //< GPIO Data Output 3 Register
#define GPIO_INT_EN1 0x1A   //< GPIO Interrupt Enable 1 Register
#define GPIO_INT_EN2 0x1B   //< GPIO Interrupt Enable 2 Register
#define GPIO_INT_EN3 0x1C   //< GPIO Interrupt Enable 3 Register
#define KP_GPIO1 0x1D       //< Keypad or GPIO Selection 1 Register
#define KP_GPIO2 0x1E       //< Keypad or GPIO Selection 2 Register
#define KP_GPIO3 0x1F       //< Keypad or GPIO Selection 3 Register
#define GPIO_EM1 0x20       //< GPIO Event Mode 1 Register
#define GPIO_EM2 0x21       //< GPIO Event Mode 2 Register
#define GPIO_EM3 0x22       //< GPIO Event Mode 3 Register
#define GPIO_DIR1 0x23      //< GPIO Direction 1 Register
#define GPIO_DIR2 0x24      //< GPIO Direction 2 Register
#define GPIO_DIR3 0x25      //< GPIO Direction 3 Register
#define GPIO_INT_LVL1 0x26  //< GPIO Edge/Level Detect 1 Register
#define GPIO_INT_LVL2 0x27  //< GPIO Edge/Level Detect 2 Register
#define GPIO_INT_LVL3 0x28  //< GPIO Edge/Level Detect 3 Register
#define DEBOUNCE_DIS1 0x29  //< Debounce Disable 1 Register
#define DEBOUNCE_DIS2 0x2A  //< Debounce Disable 2 Register
#define DEBOUNCE_DIS3 0x2B  //< Debounce Disable 3 Register
#define GPIO_PULL1 0x2C     //< GPIO Pull-up Disable 1 Register
#define GPIO_PULL2 0x2D     //< GPIO Pull-up Disable 2 Register
#define GPIO_PULL3 0x2E     //< GPIO Pull-up Disable 3 Register

/*
key map:
        1      2      3      4
    ----------------------------- 
4  | KEY_41 KEY_42 KEY_43 KEY_44
3  | KEY_31 KEY_32 KEY_33 KEY_34
2  | KEY_21 KEY_22 KEY_23 KEY_24
1  | KEY_11 KEY_12 KEY_13 KEY_14
0  | KEY_1  KEY_2  KEY_3  KEY_4 
*/
typedef enum {
    KEY_PREV = 41,  KEY_ESC = 42, KEY_HOME = 43, KEY_MAIL = 44,\
    KEY_ENTER = 31, KEY_1 = 32,   KEY_2 = 33,    KEY_3 = 34,\
    KEY_NEXT = 21,  KEY_4 = 22,   KEY_5 = 23,    KEY_6 = 24,\
    KEY_COLSE = 11, KEY_7 = 12,   KEY_8 = 13,    KEY_9 = 14,\
    KEY_STOP = 1,   KEY_MUL = 2,  KEY_0 = 3,     KEY_WELL = 4,
} KeyCode;

typedef struct
{
    KeyCode code;     // 按键编码
    bool is_long_press; // 长按标志
} key_board_event_msg_t;

/**
 * @brief Initialize TCA8418 with key events interrupt enabled
 * @return rt_err_t HAL_OK if successful, otherwise error code
 */
rt_err_t key_board_tca8418_init(void);

rt_mq_t key_board_get_mq(void);

/**
 * @brief Read key events from TCA8418 FIFO
 * @param keyEvents Array to store key events (up to 10 events)
 * @param numEvents Pointer to store number of events read
 * @return rt_err_t HAL_OK if successful, otherwise error code
 */
rt_err_t TCA8418_ReadKeyEvents(uint8_t *keyEvents, uint8_t *numEvents);  

/**
 * @brief Lock TCA8418 keypad except POWER key
 * @return rt_err_t HAL_OK if successful, otherwise error code
 */
rt_err_t TCA8418_LockKeypad(void);

/**
 * @brief Unlock TCA8418 keypad
 * @return rt_err_t HAL_OK if successful, otherwise error code
 */
rt_err_t TCA8418_UnlockKeypad(void);

#ifdef __cplusplus
}
#endif

#endif
