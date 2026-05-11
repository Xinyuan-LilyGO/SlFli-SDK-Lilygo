#include "cst922.h"
#include "board.h"
#include "drv_io.h"
#include "drv_touch.h"
#include <rtthread.h>

#define DBG_LEVEL DBG_ERROR // DBG_LOG //
#define LOG_TAG "drv.cst922"
#include <drv_log.h>

static struct rt_i2c_bus_device *ft_bus = NULL;
static struct touch_drivers cst922_driver;

rt_err_t i2c_base_write(rt_uint8_t *buf, rt_uint16_t len)
{
    rt_int8_t res = 0;
    struct rt_i2c_msg msgs;

    msgs.addr  = TOUCH_SLAVE_ADDRESS;    /* slave address */
    msgs.flags = RT_I2C_WR;        /* write flag */
    msgs.buf   = buf;              /* Send data pointer */
    msgs.len   = len;

    if (rt_i2c_transfer(ft_bus, &msgs, 1) == 1)
    {
        res = RT_EOK;
    }
    else
    {
        res = -RT_ERROR;
    }
    return res;
}

rt_err_t i2c_base_read(rt_uint8_t *buf, rt_uint16_t len)
{
    rt_int8_t res = 0;
    struct rt_i2c_msg msgs;

    msgs.addr  = TOUCH_SLAVE_ADDRESS;    /* Slave address */
    msgs.flags = RT_I2C_RD;        /* Read flag */
    msgs.buf   = buf;              /* Read data pointer */
    msgs.len   = len;              /* Number of bytes read */

    if (rt_i2c_transfer(ft_bus, &msgs, 1) == 1)
    {
        res = RT_EOK;
    }
    else
    {
        res = -RT_ERROR;
    }
    return res;
}

uint32_t cst922_i2c_write(uint16_t reg, uint8_t *p_data, uint16_t len)
{
    if ((p_data) && (len != 0))
    {
        uint32_t res;

        res = rt_i2c_mem_write(ft_bus, TOUCH_SLAVE_ADDRESS, reg, 16, p_data,
                               len); /* not I2C_MEMADD_SIZE_16BIT !!!  */

        return res;
    }
    else
    {
        rt_uint8_t buf[2];

        buf[0] = reg >> 8; // cmd
        buf[1] = reg;      // cmd

        return i2c_base_write(buf, 2);
    }
}

uint32_t cst922_i2c_read(const uint16_t reg, uint8_t *p_data, uint8_t len)
{
    uint32_t res;

    res = rt_i2c_mem_read(ft_bus, TOUCH_SLAVE_ADDRESS, reg, 16, p_data,
                          len); /* not I2C_MEMADD_SIZE_16BIT !!!  */

    return res;
}

void cst922_irq_handler(void *arg)
{
    rt_err_t ret = RT_ERROR;

    LOG_D("cst922 touch_irq_handler\n");

    rt_touch_irq_pin_enable(0);

    ret = rt_sem_release(cst922_driver.isr_sem);
    RT_ASSERT(RT_EOK == ret);
}

static rt_err_t init(void)
{
    struct touch_message msg;
    rt_uint8_t test_buff[8];

    LOG_D("cst922 init");

    BSP_TP_Reset(0);
    rt_thread_mdelay(3);
    BSP_TP_Reset(1);
    rt_thread_mdelay(50);

#ifdef CST922_UPDATA_ENABLE
    cst9xx_boot_update_fw();
#else
    cst922_i2c_write(FTS_REG_MODE_DEBUG_INFO, test_buff, 0);
#endif

    cst922_i2c_write(FTS_REG_MODE_NORMOL, test_buff, 0);

    rt_touch_irq_pin_attach(PIN_IRQ_MODE_FALLING, cst922_irq_handler, NULL);
    rt_touch_irq_pin_enable(1); // Must enable before read I2C

    LOG_D("cst922 init OK");
    return RT_EOK;
}

static rt_err_t deinit(void)
{
    LOG_D("cst922 deinit");
    rt_touch_irq_pin_enable(0);
    return RT_EOK;
}

static rt_err_t read_point(touch_msg_t p_msg)
{
    rt_err_t ret = RT_ERROR;

    LOG_D("cst922 read_point");
    rt_touch_irq_pin_enable(1);

    {
        uint8_t rbuf[4] = {0};
        uint8_t press = 0;
        uint8_t data = 0xAB;

        cst922_i2c_read(FTS_REG_GET_POSI, (uint8_t *)rbuf, 4);
        cst922_i2c_write(FTS_REG_GET_POSI, &data, 1);

        press = rbuf[0] & 0x0F;
        if (press == 0x06)
        {
            p_msg->event = TOUCH_EVENT_DOWN;
            //touch_info->state = 1;
        }
        else
        {
            p_msg->event = TOUCH_EVENT_UP;
            //touch_info->state = 0;
        }

        p_msg->x = (((uint16_t)(rbuf[1])) << 4) | (rbuf[3] >> 4);
        p_msg->y = (((uint16_t)(rbuf[2])) << 4) | (rbuf[3] & 0x0f);

        LOG_D("event=%d, X=%d, Y=%d\n", p_msg->event, p_msg->x, p_msg->y);

    }

    return RT_EEMPTY; //No more data to be read
}

static rt_bool_t probe(void)
{
    rt_err_t err;

    ft_bus = (struct rt_i2c_bus_device *)rt_device_find(TOUCH_DEVICE_NAME);
    if (RT_Device_Class_I2CBUS != ft_bus->parent.type)
    {
        ft_bus = NULL;
    }
    if (ft_bus)
    {
        rt_device_open((rt_device_t)ft_bus, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
    }
    else
    {
        LOG_I("bus not find\n");
        return RT_FALSE;
    }

    {
        struct rt_i2c_configuration configuration =
        {
            .mode = 0,
            .addr = 0,
            .timeout = 500,
            .max_hz  = 400000,
        };

        rt_i2c_configure(ft_bus, &configuration);
    }

    LOG_I("cst922 probe OK");

    return RT_TRUE;
}

static struct touch_ops ops = {read_point, init, deinit};

static int rt_cst922_init(void)
{
    cst922_driver.probe = probe;
    cst922_driver.ops = &ops;
    cst922_driver.user_data = RT_NULL;
    cst922_driver.isr_sem = rt_sem_create("cst922", 0, RT_IPC_FLAG_FIFO);

    rt_touch_drivers_register(&cst922_driver);

    return 0;
}
INIT_COMPONENT_EXPORT(rt_cst922_init);
