/**
 * @file tca8418.c
 * @brief TCA8418 driver implementation
 * @details This file contains the implementation of the TCA8418 driver,
 *          supporting key status and configuration.
 * @author Lq
 * @version 1.0
 * @date 2025-11-08
 */

#include "tca8418.h"
#include "rtconfig.h"
#include "rthw.h"
// #include "ui.h"
#include "ulog.h"
#include <string.h>

static struct rt_i2c_bus_device *tca8418_i2c_bus = NULL;
static rt_sem_t key_sem;
static rt_mq_t key_mq = NULL;

#ifdef PKG_USING_PKG_KEY_BOARD

/**
 * @brief Read data from TCA8418 register(s)
 * @param reg Register address to read from
 * @param data Pointer to store read data
 * @param length Number of bytes to read
 * @return rt_err_t RT_EOK if successful, otherwise error code
 */
static inline rt_err_t TCA8418_ReadRegister(uint8_t reg, uint8_t *data,
                                            uint16_t length)
{
    rt_size_t ret = 0;
    ret =
        rt_i2c_mem_read(tca8418_i2c_bus, TCA8418_ADDRESS, reg, 1, data, length);
    if (ret != length)
    {
        LOG_E("read reg data failed");
        return -RT_ERROR;
    }
    return RT_EOK;
}

/**
 * @brief Write data to TCA8418 register(s)
 * @param reg Register address to write to
 * @param data Pointer to data to write
 * @param length Number of bytes to write
 * @return rt_err_t RT_EOK if successful, otherwise error code
 */
static inline rt_err_t TCA8418_WriteRegister(uint8_t reg, uint8_t *data,
                                             uint16_t length)
{
    rt_size_t ret = 0;
    uint8_t *buf = rt_malloc(length);
    if (!buf)
    {
        return -RT_ERROR;
    }
    memcpy(&buf[0], data, length);
    ret =
        rt_i2c_mem_write(tca8418_i2c_bus, TCA8418_ADDRESS, reg, 1, buf, length);
    rt_free(buf);
    if (ret != length)
    {
        LOG_E("write reg addr and data failed");
        return -RT_ERROR;
    }
    return RT_EOK;
}

static void tca8418_int_isr(void *args)
{
    rt_sem_release(key_sem); // 信号量必须是RT_IPC_FLAG_FIFO类型
    rt_pin_irq_enable(KEY_BOARD_IRQ_PIN, PIN_IRQ_DISABLE); // 临时关闭中断
}

/**
 * @brief Configure TCA8418 for keypad and GPIO operation
 * @return rt_err_t RT_EOK if successful, otherwise error code
 * @note This function configures the TCA8418 for keypad operation with pins
 * ROW0 and COL6:0.(7 keys in total) It also configures other unused pins as
 * GPIO inputs with pull-up enabled without interrupts.
 */
static inline rt_err_t TCA8418_KPConfig(void)
{
    rt_err_t status;
    uint8_t data = 0x1F; // ROW0[0] = 1 -> Keypad row is selected
    status = TCA8418_WriteRegister(KP_GPIO1, &data, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }

    data = 0x0F; // COL6:0[6:0] = 1 -> Keypad columns are selected
    status = TCA8418_WriteRegister(KP_GPIO2, &data, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }
    data = 0x1F; // R7:1IE[7:1] = 0 -> Row7:1 interrupts are disabled
    status = TCA8418_WriteRegister(GPIO_INT_EN1, &data, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }
    data = 0x0F; // C7IE[7] = 0 -> Column7 interrupt is disabled
    status = TCA8418_WriteRegister(GPIO_INT_EN2, &data, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }
    data = 0x00; // C9:8IE[1:0] = 0 -> Column9:8 interrupts are disabled
    status = TCA8418_WriteRegister(GPIO_INT_EN3, &data, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }

    uint8_t kp_gpio1, kp_gpio2;
    status = TCA8418_ReadRegister(KP_GPIO1, &kp_gpio1, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }
    status = TCA8418_ReadRegister(KP_GPIO2, &kp_gpio2, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }
    LOG_D("KP_GPIO1=0x%02X KP_GPIO2=0x%02X", kp_gpio1, kp_gpio2);

    return RT_EOK;
}

/**
 * @brief Enable TCA8418 key events interrupt
 * @return rt_err_t RT_EOK if successful, otherwise error code
 */
static inline rt_err_t TCA8418_EnableInterrupt(void)
{
    rt_err_t status;
    uint8_t data = 0x01; // KE_IEN[0] = 1
    status = TCA8418_WriteRegister(CFG, &data, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }

    status = TCA8418_ReadRegister(CFG, &data, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (data != 0x01)
    {
        LOG_D("CFG=0x%02X", data);
        return -RT_ERROR;
    }
    return RT_EOK;
}

static void key_scan_work(void)
{
    uint8_t keyEvents[10]; // Array to store up to 10 events
    uint8_t numEvents;     // Number of events actually read

    // Read all pending key events
    rt_err_t status = TCA8418_ReadKeyEvents(keyEvents, &numEvents);

    if (status == RT_EOK)
    {
        // Process each event
        for (int i = 0; i < numEvents; i++)
        {
            uint8_t keyNumber = keyEvents[i] & 0x7F;         // Bits 6:0
            uint8_t isPress = (keyEvents[i] & 0x80) ? 1 : 0; // Bit 7

            if (isPress)
            {
                // log_d("Key Pressed: %d", keyNumber);
                key_board_event_msg_t keyEvent;
                keyEvent.code = keyNumber;
                keyEvent.is_long_press = false;

                rt_mq_send(key_mq, &keyEvent, sizeof(keyEvent));
            }
        }
    }
    rt_pin_irq_enable(KEY_BOARD_IRQ_PIN, PIN_IRQ_ENABLE); // 重新启用中断
}

static void key_thread_entry(void *param)
{
    while (1)
    {
        if (rt_sem_take(key_sem, RT_WAITING_FOREVER) == RT_EOK)
        {
            key_scan_work();
        }
    }
}
/**
 * @brief Initialize TCA8418 with key events interrupt enabled
 * @return rt_err_t RT_EOK if successful, otherwise error code
 */
rt_err_t key_board_tca8418_init(void)
{
    int ret;
    rt_uint32_t ts = 0;
    uint8_t rst = 1;
    key_sem = rt_sem_create("key_sem", 0, RT_IPC_FLAG_FIFO);

    key_mq = rt_mq_create("key_mq", sizeof(key_board_event_msg_t), 10, RT_IPC_FLAG_FIFO);
    if (key_mq == RT_NULL)
    {
        LOG_E("Create message queue failed");
        return -1;
    }

    tca8418_i2c_bus = rt_i2c_bus_device_find(KEY_BOARD_I2C_BUS_NAME);
    if (tca8418_i2c_bus == RT_NULL)
    {
        LOG_E("find %s device failed", KEY_BOARD_I2C_BUS_NAME);
        return -RT_ERROR;
    }
    else
    {
        LOG_I("find %s device success", KEY_BOARD_I2C_BUS_NAME);
    }

    if (rt_device_open((rt_device_t)tca8418_i2c_bus, RT_DEVICE_OFLAG_RDWR) !=
        RT_EOK)
    {
        LOG_E("open %s device failed", KEY_BOARD_I2C_BUS_NAME);
        return -RT_ERROR;
    }
    else
    {
        LOG_I("open %s device success", KEY_BOARD_I2C_BUS_NAME);
    }

    struct rt_i2c_configuration configuration = {
        .mode = 0,
        .addr = 0,
        .timeout = 500,   // Waiting for timeout period (ms)
        .max_hz = 400000, // I2C rate (hz)
    };
    rt_i2c_configure(tca8418_i2c_bus, &configuration);

    rt_err_t status;
    status = TCA8418_KPConfig();
    if (status != RT_EOK)
    {
        log_e("TCA8418_KPConfig failed");
        // rt_device_close((rt_device_t)tca8418_i2c_bus);
        return -RT_ERROR;
    }
    status = TCA8418_EnableInterrupt();
    if (status != RT_EOK)
    {
        log_e("TCA8418_EnableInterrupt failed");
        return -RT_ERROR;
    }

    rt_pin_mode(KEY_BOARD_IRQ_PIN, PIN_MODE_INPUT_PULLUP);
    rt_pin_attach_irq(KEY_BOARD_IRQ_PIN, PIN_IRQ_MODE_FALLING, tca8418_int_isr,
                      RT_NULL);
    rt_pin_irq_enable(KEY_BOARD_IRQ_PIN, PIN_IRQ_ENABLE);

    rt_thread_t tid =
        rt_thread_create("key", key_thread_entry, RT_NULL, 1024, 12, 10);
    rt_thread_startup(tid);

    return RT_EOK;
}

rt_mq_t key_board_get_mq(void)
{
    if (key_mq != RT_NULL)
        return key_mq;
    else
        return RT_NULL;
}

/**
 * @brief Read key events from TCA8418 FIFO
 * @param keyEvents Array to store key events (up to 10 events)
 * @param numEvents Pointer to store number of events read
 * @return rt_err_t RT_EOK if successful, otherwise error code
 * @note This function reads all pending key events from the FIFO.
 *       Each event is stored as a byte where:
 *       - Bits 6:0 indicate the key number (0-80 for keypad, 97-114 for GPIO)
 *       - Bit 7 indicates event type (0=release, 1=press)
 */
rt_err_t TCA8418_ReadKeyEvents(uint8_t *keyEvents, uint8_t *numEvents)
{
    rt_err_t status;
    uint8_t intStatus;
    uint8_t eventCount;
    /* First check if there are any interrupts */
    status = TCA8418_ReadRegister(INT_STAT, &intStatus, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }
    /* Check if there are key events (KE_INT bit) */
    if (!(intStatus & 0x01))
    {
        *numEvents = 0;
        return RT_EOK; // No events
    }
    /* Read the event counter */
    status = TCA8418_ReadRegister(KEY_LCK_EC, &eventCount, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }
    /* Limit to maximum 10 events */
    if (eventCount > 10)
    {
        eventCount = 10;
    }
    /* Read all events from FIFO */
    for (uint8_t i = 0; i < eventCount; i++)
    {
        status = TCA8418_ReadRegister(KEY_EVENT_A, &keyEvents[i], 1);
        if (status != RT_EOK)
        {
            return -RT_ERROR;
        }
    }
    *numEvents = eventCount;
    /* Clear the interrupt by writing 1 to KE_INT bit */
    intStatus = 0x01;
    status = TCA8418_WriteRegister(INT_STAT, &intStatus, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }
    return RT_EOK;
}

/**
 * @brief Lock TCA8418 keypad except POWER key
 * @return rt_err_t RT_EOK if successful, otherwise error code
 * @note This function disables all keypad columns except column 0 (POWER key)
 */
rt_err_t TCA8418_LockKeypad(void)
{
    rt_err_t status;
    uint8_t data;
    /* Enable only column 0 (POWER key) for keypad operation */
    data = 0x01; // COL0[0] = 1 -> Enable column 0 for keypad
    status = TCA8418_WriteRegister(KP_GPIO2, &data, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }
    /* Disable interrupt for all columns except column 0 */
    data = 0x7F; // C7:1IE[7:1] = 0 -> Disable interrupts for columns 1-7
    status = TCA8418_WriteRegister(GPIO_INT_EN2, &data, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }
    return RT_EOK;
}

/**
 * @brief Unlock TCA8418 keypad
 * @return rt_err_t RT_EOK if successful, otherwise error code
 * @note This function enables all keypad columns (0-6) and their interrupts
 */
rt_err_t TCA8418_UnlockKeypad(void)
{
    rt_err_t status;
    uint8_t data;
    /* Enable all columns (0-6) for keypad operation */
    data = 0x7F; // COL6:0[6:0] = 1 -> Enable columns 0-6 for keypad
    status = TCA8418_WriteRegister(KP_GPIO2, &data, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }
    /* Disable interrupt for column 7 */
    data = 0x7F; // C7IE[7] = 0 -> Disable interrupt for column 7
    status = TCA8418_WriteRegister(GPIO_INT_EN2, &data, 1);
    if (status != RT_EOK)
    {
        return -RT_ERROR;
    }
    return RT_EOK;
}

#endif