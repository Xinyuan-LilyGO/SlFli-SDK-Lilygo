#include "bhi260ap.h"
#include "bhi260ap_firmware.h"
#include "common.h"
#include "rthw.h"
#include "ulog.h"

#define BHI260AP_ADDR 0x28

#ifdef PKG_USING_BHI260AP
volatile bool bhi260ap_thread_should_run = true;

rt_thread_t bhi260ap_sensor_thread = RT_NULL;
static struct bhy2_dev bhi260ap_dev;
static struct rt_i2c_bus_device *i2c_bus = NULL;
enum bhy2_intf intf = BHY2_I2C_INTERFACE;
uint8_t *work_buffer;
uint16_t work_buffer_size = 1024;
static uint8_t sensor_available_num = 0;
    // 定义数据缓冲区
    #define SENSOR_DATA_BUFFER_SIZE 32
static struct sensor_data_t sensor_data_buffer[SENSOR_DATA_BUFFER_SIZE];
static rt_uint16_t data_write_index = 0;
static rt_uint16_t data_read_index = 0;
static rt_sem_t data_process_sem = RT_NULL;
static rt_mutex_t sensor_data_mutex = RT_NULL;

static struct bhy2_data_xyz_float acc_xyz;
static struct bhy2_data_xyz_float gyro_xyz;
static struct bhy2_data_orientation_float orientation;
static struct bhy2_data_quaternion_float qaternion;
static uint32_t step_counter;

    #if defined(BHI260AP_IRQ_PIN)
static rt_sem_t bhi260ap_sem = RT_NULL;
    #endif

static BHY2_INTF_RET_TYPE bhy2_i2c_read(uint8_t reg_addr, uint8_t *reg_data,
                                        uint32_t length, void *intf_ptr)
{
    rt_size_t ret = 0;
    ret =
        rt_i2c_mem_read(i2c_bus, BHI260AP_ADDR, reg_addr, 1, reg_data, length);
    if (ret != length)
    {
        LOG_E("read reg data failed");
        return -1;
    }
    return BHY2_OK;
}

static BHY2_INTF_RET_TYPE bhy2_i2c_write(uint8_t reg_addr,
                                         const uint8_t *reg_data,
                                         uint32_t length, void *intf_ptr)
{
    rt_size_t ret = 0;
    uint8_t *buf = rt_malloc(length);
    if (!buf)
    {
        return BHY2_E_NULL_PTR;
    }
    memcpy(&buf[0], reg_data, length);
    ret = rt_i2c_mem_write(i2c_bus, BHI260AP_ADDR, reg_addr, 1, buf, length);
    rt_free(buf);
    if (ret != length)
    {
        LOG_E("write reg addr and data failed");
    }
    return BHY2_OK;
}

/* 微秒延迟函数 */
static void bhy2_delay_us(uint32_t period_us, void *intf_ptr)
{
    // RT-Thread的微秒级延迟实现
    rt_hw_us_delay(period_us);
}

    #if defined(BHI260AP_IRQ_PIN)
static void bhi260ap_int_handler(void *parameter)
{
    rt_sem_release(bhi260ap_sem); // 释放信号量
}
    #endif

static void process_sensor_data(const struct bhy2_fifo_parse_data_info *data)
{
    sensor_data_buffer[data_write_index].sensor_id = data->sensor_id;
    sensor_data_buffer[data_write_index].timestamp = data->time_stamp;

    // LOG_D("sensor_id: %d\n", sensor_data_buffer[data_write_index].sensor_id);
    switch (sensor_data_buffer[data_write_index].sensor_id)
    {
    case BHY2_SENSOR_ID_ACC:
    {
        struct bhy2_data_xyz raw_data;

        memcpy(&sensor_data_buffer[data_write_index].data.xyz, data->data_ptr,
               sizeof(struct sensor_data_t));

        bhy2_parse_xyz(data->data_ptr, &raw_data);

        float scaling = bhi260ap_get_scaling(BHY2_SENSOR_ID_ACC);

        // 获取互斥锁
        rt_mutex_take(sensor_data_mutex, RT_WAITING_FOREVER);

        acc_xyz.x = (float)raw_data.x * scaling;
        acc_xyz.y = (float)raw_data.y * scaling;
        acc_xyz.z = (float)raw_data.z * scaling;

        rt_mutex_release(sensor_data_mutex);
        // LOG_D("Acc: X=%.2f, Y=%.2f, Z=%.2f\n", acc_xyz.x, acc_xyz.y,
        // acc_xyz.z); LOG_D("Acc: X=%.2f, Y=%.2f, Z=%.2f\n", (float)raw_data.x
        // * scaling,
        //       (float)raw_data.y * scaling, (float)raw_data.z * scaling);
        break;
    }
    case BHY2_SENSOR_ID_GYRO:
    {
        struct bhy2_data_xyz raw_data;

        memcpy(&sensor_data_buffer[data_write_index].data.xyz, data->data_ptr,
               sizeof(struct sensor_data_t));

        float scaling = bhi260ap_get_scaling(BHY2_SENSOR_ID_GYRO);

        bhy2_parse_xyz(data->data_ptr, &raw_data);

        rt_mutex_take(sensor_data_mutex, RT_WAITING_FOREVER);

        gyro_xyz.x = (float)raw_data.x * scaling;
        gyro_xyz.y = (float)raw_data.y * scaling;
        gyro_xyz.z = (float)raw_data.z * scaling;

        rt_mutex_release(sensor_data_mutex);

        // LOG_D("Gyro: X=%.2f, Y=%.2f, Z=%.2f\n", gyro_x, gyro_y, gyro_z);
        // LOG_D("Gyor: X=%.2f, Y=%.2f, Z=%.2f\n", (float)raw_data.x * scaling,
        //       (float)raw_data.y * scaling, (float)raw_data.z * scaling);

        break;
    }
    case BHY2_SENSOR_ID_MAG_PASS:
    case BHY2_SENSOR_ID_MAG_RAW:
    case BHY2_SENSOR_ID_MAG:
    case BHY2_SENSOR_ID_MAG_BIAS:
    case BHY2_SENSOR_ID_MAG_WU:
    case BHY2_SENSOR_ID_MAG_RAW_WU:
    case BHY2_SENSOR_ID_MAG_BIAS_WU:
    {
        break;
    }

    case BHY2_SENSOR_ID_STC:
    {
        step_counter = bhy2_parse_step_counter(data->data_ptr);
        break;
    }

    case BHY2_SENSOR_ID_RV:
    case BHY2_SENSOR_ID_RV_WU:
    case BHY2_SENSOR_ID_GAMERV:
    case BHY2_SENSOR_ID_GAMERV_WU:
    case BHY2_SENSOR_ID_GEORV:
    case BHY2_SENSOR_ID_GEORV_WU:
    {
        struct bhy2_data_orientation orientation_raw_data;
        struct bhy2_data_quaternion quaternion_raw_data;
        bhy2_parse_orientation(data->data_ptr, &orientation_raw_data);

        orientation.heading =
            (float)orientation_raw_data.heading * (360.0f / 32768.0f);
        orientation.pitch =
            (float)orientation_raw_data.pitch * (360.0f / 32768.0f);
        orientation.roll =
            (float)orientation_raw_data.roll * (360.0f / 32768.0f);
        // LOG_D("orientation: %.2f,%.2f,%.2f\n", orientation.heading,
        //       orientation.pitch, orientation.roll);

        bhy2_parse_quaternion(data->data_ptr, &quaternion_raw_data);
        qaternion.x = (float)quaternion_raw_data.x / 16384.0f;
        qaternion.y = (float)quaternion_raw_data.y / 16384.0f;
        qaternion.z = (float)quaternion_raw_data.z / 16384.0f;
        qaternion.w = (float)quaternion_raw_data.w / 16384.0f;
        qaternion.accuracy = (float)quaternion_raw_data.accuracy / 16384.0f;
        // LOG_D("qaternion: %.2f,%.2f,%.2f,%.2f,%.2f\n", qaternion.x,
        // qaternion.y, qaternion.z, qaternion.w,qaternion.accuracy);
        break;
    }

    case BHY2_SENSOR_ID_ORI:
    case BHY2_SENSOR_ID_ORI_WU:
    {
        break;
    }

    case BHY2_SENSOR_ID_TEMP:
    case BHY2_SENSOR_ID_TEMP_WU:
    {
        break;
    }

    case BHY2_SENSOR_ID_BARO:
    case BHY2_SENSOR_ID_BARO_WU:
    {
        break;
    }
    case BHY2_SENSOR_ID_HUM:
    case BHY2_SENSOR_ID_HUM_WU:
    {
        break;
    }
    case BHY2_SENSOR_ID_GAS:
    case BHY2_SENSOR_ID_GAS_WU:
    {
        break;
    }
    case BHY2_SENSOR_ID_LIGHT:
    case BHY2_SENSOR_ID_LIGHT_WU:
    {
        break;
    }
    case BHY2_SENSOR_ID_PROX:
    case BHY2_SENSOR_ID_PROX_WU:
    {
        break;
    }
    case BHY2_SENSOR_ID_SI_ACCEL:
    case BHY2_SENSOR_ID_SI_GYROS:
    {
        break;
    }
    case BHY2_SENSOR_ID_HEAD_ORI_MIS_ALG:
    case BHY2_SENSOR_ID_IMU_HEAD_ORI_Q:
    case BHY2_SENSOR_ID_NDOF_HEAD_ORI_Q:
    {
        break;
    }
    case BHY2_SENSOR_ID_IMU_HEAD_ORI_E:
    case BHY2_SENSOR_ID_NDOF_HEAD_ORI_E:
    {
        break;
    }
    }
}

/* 传感器数据回调函数 */
static void sensor_data_callback(const struct bhy2_fifo_parse_data_info *callback_info,
                     void *private_data)
{
    rt_uint16_t next_index = (data_write_index + 1) % SENSOR_DATA_BUFFER_SIZE;

    // 检查缓冲区是否已满
    if (next_index == data_read_index)
    {
        LOG_W("Sensor data buffer full");
        return;
    }

    process_sensor_data(callback_info);

    data_write_index = next_index;

    data_read_index = (data_read_index + 1) % SENSOR_DATA_BUFFER_SIZE;
}

/* 传感器事件回调函数 */
static void sensor_meta_event_callback(
    const struct bhy2_fifo_parse_data_info *callback_info, void *private_data)
{
    uint8_t meta_event_type = callback_info->data_ptr[0];
    uint8_t byte1 = callback_info->data_ptr[1];
    uint8_t byte2 = callback_info->data_ptr[2];
    const char *event_text;
    if (callback_info->sensor_id == BHY2_SYS_ID_META_EVENT)
    {
        event_text = "[Meta Event]";
    }
    else if (callback_info->sensor_id == BHY2_SYS_ID_META_EVENT_WU)
    {
        event_text = "[Meta Event Wakeup]";
    }
    else
    {
        return;
    }

    switch (meta_event_type)
    {
    case BHY2_META_EVENT_FLUSH_COMPLETE:
        LOG_D("%s Flush complete for sensor id %u", event_text, byte1);
        break;
    case BHY2_META_EVENT_SAMPLE_RATE_CHANGED:
        LOG_D("%s Sample rate changed for sensor id %u", event_text, byte1);
        break;
    case BHY2_META_EVENT_POWER_MODE_CHANGED:
        LOG_D("%s Power mode changed for sensor id %u", event_text, byte1);
        break;
    case BHY2_META_EVENT_ALGORITHM_EVENTS:
        LOG_D("%s Algorithm event", event_text);
        break;
    case BHY2_META_EVENT_SENSOR_STATUS:
        LOG_D("%s Accuracy for sensor id %u changed to %u", event_text, byte1,
              byte2);
        break;
    case BHY2_META_EVENT_BSX_DO_STEPS_MAIN:
        LOG_D("%s BSX event (do steps main)", event_text);
        break;
    case BHY2_META_EVENT_BSX_DO_STEPS_CALIB:
        LOG_D("%s BSX event (do steps calib)", event_text);
        break;
    case BHY2_META_EVENT_BSX_GET_OUTPUT_SIGNAL:
        LOG_D("%s BSX event (get output signal)", event_text);
        break;
    case BHY2_META_EVENT_SENSOR_ERROR:
        LOG_D("%s Sensor id %u reported error 0x%02X", event_text, byte1,
              byte2);
        break;
    case BHY2_META_EVENT_FIFO_OVERFLOW:
        LOG_D("%s FIFO overflow", event_text);
        break;
    case BHY2_META_EVENT_DYNAMIC_RANGE_CHANGED:
        LOG_D("%s Dynamic range changed for sensor id %u", event_text, byte1);
        break;
    case BHY2_META_EVENT_FIFO_WATERMARK:
        LOG_D("%s FIFO watermark reached", event_text);
        break;
    case BHY2_META_EVENT_INITIALIZED:
        LOG_D("%s Firmware initialized. Firmware version %u", event_text,
              ((uint16_t)byte2 << 8) | byte1);
        break;
    case BHY2_META_TRANSFER_CAUSE:
        LOG_D("%s Transfer cause for sensor id %u", event_text, byte1);
        break;
    case BHY2_META_EVENT_SENSOR_FRAMEWORK:
        LOG_D("%s Sensor framework event for sensor id %u", event_text, byte1);
        break;
    case BHY2_META_EVENT_RESET:
        LOG_D("%s Reset event", event_text);
        break;
    case BHY2_META_EVENT_SPACER:
        return;
    default:
        LOG_D("%s Unknown meta event with id: %u", event_text, meta_event_type);
        break;
    }
}

static int bhi260ap_init(void)
{
    int8_t ret;
    uint8_t chip_id;

    i2c_bus = rt_i2c_bus_device_find(BHI260AP_I2C_BUS_NAME);
    if (i2c_bus == RT_NULL)
    {
        LOG_E("find %s device failed", BHI260AP_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    if (rt_device_open((rt_device_t)i2c_bus, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        LOG_E("open %s device failed", BHI260AP_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    struct rt_i2c_configuration configuration = {
        .mode = 0,
        .addr = 0,
        .timeout = 500,   // Waiting for timeout period (ms)
        .max_hz = 400000, // I2C rate (hz)
    };

    rt_i2c_configure(i2c_bus, &configuration);

    #if defined(BHI260AP_IRQ_PIN)
    rt_pin_mode(BHI260AP_IRQ_PIN, PIN_MODE_INPUT);
    rt_pin_attach_irq(BHI260AP_IRQ_PIN, PIN_IRQ_MODE_RISING,
                      bhi260ap_int_handler, RT_NULL);
    rt_pin_irq_enable(BHI260AP_IRQ_PIN, PIN_IRQ_ENABLE);
    #endif
    ret = bhy2_hif_init(intf, bhy2_i2c_read, bhy2_i2c_write, bhy2_delay_us, 64,
                        i2c_bus, &bhi260ap_dev.hif);

    if (ret != BHY2_OK)
    {
        LOG_E("bhy2_hif_init failed");
    }

    ret = bhy2_soft_reset(&bhi260ap_dev);
    if (ret != BHY2_OK)
    {
        LOG_E("bhy2_soft_reset failed");
    }

    ret = bhy2_get_chip_id(&chip_id, &bhi260ap_dev);
    if (ret != BHY2_OK || chip_id != BHY2_PRODUCT_ID)
    {
        LOG_D("Product ID read 0x%02X. Expected 0x%02X", chip_id,
              BHY2_PRODUCT_ID);
        return -RT_ERROR;
    }

    uint8_t fifo_status = 0, check_fifo_status = 0;

    bhy2_get_host_interrupt_ctrl(&fifo_status, &bhi260ap_dev);
    fifo_status &= ~BHY2_ICTL_DISABLE_STATUS_FIFO;
    fifo_status &= ~BHY2_ICTL_DISABLE_DEBUG;
    // fifo_status |= BHY2_ICTL_DISABLE_DEBUG;
    fifo_status &= ~BHY2_ICTL_EDGE;
    fifo_status &= ~BHY2_ICTL_ACTIVE_LOW;
    fifo_status &= ~BHY2_ICTL_OPEN_DRAIN;
    check_fifo_status = fifo_status;
    bhy2_set_host_interrupt_ctrl(fifo_status, &bhi260ap_dev);
    rt_thread_mdelay(10);
    bhy2_get_host_interrupt_ctrl(&fifo_status, &bhi260ap_dev);
    if (check_fifo_status != fifo_status)
    {
        LOG_E("set host_interrupt_ctrl(0x07),fifo_status: 0x%02X "
              ",check_fifo_status: 0x%02X",
              fifo_status, check_fifo_status);
    }

    /* Config status channel */
    bhy2_set_host_intf_ctrl(BHY2_HIF_CTRL_ASYNC_STATUS_CHANNEL, &bhi260ap_dev);
    bhy2_get_host_intf_ctrl(&fifo_status, &bhi260ap_dev);
    if (!(fifo_status & BHY2_HIF_CTRL_ASYNC_STATUS_CHANNEL))
    {
        LOG_E("Expected Host Interface Control (0x06) to have bit 0x%x to be "
              "set\r\n",
              BHY2_HIF_CTRL_ASYNC_STATUS_CHANNEL);
    }

    uint8_t bhy2_error_value = 0;

    // write firmware ram
    if (bhy2_firmware_image != NULL)
    {
        LOG_D("Loading firmware into RAM.");
        uint32_t bhy2_firmware_image_size = sizeof(bhy2_firmware_image);
        ret = bhy2_upload_firmware_to_ram(
            bhy2_firmware_image, bhy2_firmware_image_size, &bhi260ap_dev);
        if (ret != BHY2_OK)
        {
            LOG_E("bhy2_load_firmware_ram failed");
        }
        LOG_D("Loading firmware into RAM Done");

        ret = bhy2_get_error_value(&bhy2_error_value, &bhi260ap_dev);
        if (ret != BHY2_OK)
        {
            LOG_E("bhy2_get_error_value failed");
            LOG_E("%s", get_sensor_error_text(bhy2_error_value));
            return -RT_ERROR;
        }

        ret = bhy2_boot_from_ram(&bhi260ap_dev);
        if (ret != BHY2_OK)
        {
            LOG_E("bhy2_boot_from_ram failed");
        }

        ret = bhy2_get_error_value(&bhy2_error_value, &bhi260ap_dev);
        if (ret != BHY2_OK)
        {
            LOG_E("bhy2_get_error_value failed");
            LOG_E("%s", get_sensor_error_text(bhy2_error_value));
            return -RT_ERROR;
        }
    }

    uint16_t kernel_version;
    ret = bhy2_get_kernel_version(&kernel_version, &bhi260ap_dev);
    if (ret)
    {
        LOG_E("Get Kernel error");
        return -RT_ERROR;
    }
    LOG_D("Kernel version: %d", kernel_version);

    ret = bhy2_register_fifo_parse_callback(BHY2_SYS_ID_META_EVENT,
                                            sensor_meta_event_callback, NULL,
                                            &bhi260ap_dev);
    if (ret != BHY2_OK)
    {
        LOG_E(
            "bhy2_register_fifo_parse_callback BHY2_SYS_ID_META_EVENT failed");
    }

    ret = bhy2_register_fifo_parse_callback(BHY2_SYS_ID_META_EVENT_WU,
                                            sensor_meta_event_callback, NULL,
                                            &bhi260ap_dev);
    if (ret != BHY2_OK)
    {
        LOG_E("bhy2_register_fifo_parse_callback BHY2_SYS_ID_META_EVENT_WU "
              "failed");
    }

    bhy2_update_virtual_sensor_list(&bhi260ap_dev);

    bhy2_get_virt_sensor_list(&bhi260ap_dev);

    for (uint8_t i = 0; i < BHY2_SENSOR_ID_MAX; i++)
    {
        if (bhy2_is_sensor_available(i, &bhi260ap_dev))
        {
            sensor_available_num++;
            bhy2_register_fifo_parse_callback(i, sensor_data_callback, NULL,
                                              &bhi260ap_dev);
            // LOG_D("Sensor %d (%s) is available", sensor_available_num,
            //       bhi260ap_get_sensor_name(sensor_available_num));
        }
    }
    LOG_D("sensor_available_num: %d", sensor_available_num);

    rt_kprintf("BHI260AP init success\n");

    return BHY2_OK;
}

/* 定期读取FIFO数据的线程 */
static void bhi260ap_thread_entry(void *parameter)
{
    work_buffer = rt_malloc(work_buffer_size);
    if (!work_buffer)
    {
        LOG_E("Failed to allocate memory for work_buffer");
    }

    while (1)
    {
        if (!bhi260ap_thread_should_run)
        {
            // 主动自挂起
            rt_thread_suspend(rt_thread_self());
            rt_schedule(); // 立即切换
            continue;
        }

    #if defined(BHI260AP_IRQ_PIN)
        if (rt_sem_take(bhi260ap_sem, RT_WAITING_FOREVER) == RT_EOK) // RT_TICK_PER_SECOND / 100
        {
            bhy2_get_and_process_fifo(work_buffer, work_buffer_size, &bhi260ap_dev);
        }
    #else
        bhy2_get_and_process_fifo(work_buffer, work_buffer_size, &bhi260ap_dev);
    #endif
        rt_thread_mdelay(20);
    }
}

void bhi260ap_thread_pause(void)
{
    bhi260ap_thread_should_run = false;
    // 线程会在下次循环自动挂起
}

void bhi260ap_thread_resume(void)
{
    bhi260ap_thread_should_run = true;
    if (bhi260ap_sensor_thread->stat == RT_THREAD_SUSPEND)
        rt_thread_resume(bhi260ap_sensor_thread);
}

rt_err_t rt_bhi260ap_init(void)
{
    #if defined(BHI260AP_IRQ_PIN)
    bhi260ap_sem = rt_sem_create("bhi260ap_sem", 0, RT_IPC_FLAG_FIFO);
    if (bhi260ap_sem == RT_NULL)
    {
        LOG_E("Create semaphore failed");
        return -RT_ERROR;
    }
    #endif
    // 创建互斥锁
    sensor_data_mutex = rt_mutex_create("sensor_data_mutex", RT_IPC_FLAG_FIFO);
    if (sensor_data_mutex == RT_NULL)
    {
        LOG_E("Create sensor data mutex failed");
        return -RT_ERROR;
    }

    data_process_sem = rt_sem_create("data_sem", 0, RT_IPC_FLAG_FIFO);
    if (data_process_sem == RT_NULL)
    {
        LOG_E("Create data process semaphore failed");
        return -RT_ERROR;
    }

    if (bhi260ap_init() != RT_EOK)
        return -RT_ERROR;

    /* 创建数据处理线程 */
    bhi260ap_sensor_thread = rt_thread_create("bhi260ap", bhi260ap_thread_entry, RT_NULL, 2048, RT_THREAD_PRIORITY_MAX / 2, 20);
    if (bhi260ap_sensor_thread != RT_NULL)
    {
        rt_thread_startup(bhi260ap_sensor_thread);
        rt_thread_suspend(bhi260ap_sensor_thread);
    }
 

    return RT_EOK;
}
// INIT_APP_EXPORT(rt_bhi260ap_init);

/****************************************/
/*Bosch Sensortec BHI260AP API */
/****************************************/

/**
 * @brief  bhi260ap_configure
 * @note   Sensor Configuration
 * @param  sensor_id: Sensor ID , see enum BoschSensorID
 * @param  sample_rate: Data output rate, unit: HZ
 * @param  report_latency_ms: Report interval in milliseconds
 * @return bool true-> Success false-> failure
 */
bool bhi260ap_configure(uint8_t sensor_id, uint8_t sample_rate,
                        uint32_t report_latency_ms)
{
    uint8_t ret;

    if (!bhy2_is_sensor_available(sensor_id, &bhi260ap_dev))
    {
        LOG_E("sensor_id: %d is not available", sensor_id);
        return false;
    }

    ret = bhy2_set_virt_sensor_cfg(sensor_id, sample_rate, report_latency_ms,
                                   &bhi260ap_dev);
    if (ret != BHY2_OK)
    {
        LOG_E("bhy2_set_virt_sensor_cfg failed");
        return false;
    }
    LOG_D("Enable sensor id : %d ,%s at %dHz.", sensor_id,
          get_sensor_name(sensor_id), sample_rate);

    return true;
}

/**
 * @brief  bhi260ap_configure_range
 * @note   Set range of the sensor
 * @param  sensor_id: Sensor ID , see enum BoschSensorID
 * @param  range:     Range for selected SensorID. See Table 79 in BHY260
 * datasheet 109 page
 * @retval  bool true-> Success false-> failure
 */
bool bhi260ap_configure_range(uint8_t sensor_id, uint8_t range)
{
    uint8_t ret;
    if (!bhy2_is_sensor_available(sensor_id, &bhi260ap_dev))
    {
        LOG_E("sensor_id: %d is not available", sensor_id);
        return false;
    }

    ret = bhy2_set_virt_sensor_range(sensor_id, range, &bhi260ap_dev);
    if (ret != BHY2_OK)
    {
        LOG_E("bhy2_set_virt_sensor_range failed");
        return false;
    }
    LOG_D("Set %s range to %d.", get_sensor_name(sensor_id), range);
    return true;
}

/**
 * @brief  bhi260ap_get_Configure
 * @note   Get sensor configuration
 * @param  sensor_id: Sensor ID , see enum BoschSensorID
 * @retval  SensorConfig
 */
struct bhy2_virt_sensor_conf bhi260ap_get_configure(uint8_t sensor_id)
{
    struct bhy2_virt_sensor_conf conf;
    bhy2_get_virt_sensor_cfg(sensor_id, &conf, &bhi260ap_dev);
    log_d("range:%u sample_rate:%f latency:%lu sensitivity:%u\n", conf.range,
          conf.sample_rate, conf.latency, conf.sensitivity);
    return conf;
}

/**
 * @brief  bhi260ap_get_scaling
 * @note   Get sensor scale factor
 * @param  sensor_id: Sensor ID , see enum BoschSensorID
 * @retval scale factor
 */
float bhi260ap_get_scaling(uint8_t sensor_id)
{
    return get_sensor_default_scaling(sensor_id);
}

/**
 * @brief  bhi260ap_get_sensor_name
 * @note   Get sensor name
 * @param  sensor_id: Sensor ID , see enum BoschSensorID
 * @retval sensor name
 */
const char *bhi260ap_get_sensor_name(uint8_t sensor_id)
{
    return get_sensor_name(sensor_id);
}

/**
 * @brief Set the axis remapping for the sensor based on the specified
 * orientation.
 *
 * This function allows you to configure the sensor's axis remapping according
 * to a specific physical orientation of the chip. By passing one of the values
 * from the SensorRemap enum, you can ensure that the sensor data is correctly
 * interpreted based on how the chip is placed.
 *
 * @param remap An enumeration value from SensorRemap that specifies the desired
 * axis remapping.
 * @return Returns true if the axis remapping is successfully set; false
 * otherwise.
 */
bool bhi260ap_set_remap_axes(enum SensorRemap remap)
{
    int8_t ret;
    if (remap > BOTTOM_LAYER_BOTTOM_LEFT_CORNER)
    {
        log_e("Invalid SensorRemap value passed to setRemapAxes!");
        return false;
    }

    // Acceleration - related orientation matrices for different mounting
    // directions
    struct bhy2_orient_matrix acc_matrices[] = {
        // P0 mounting direction, default direction, axis direction is
        // consistent with the default coordinate system
        {1, 0, 0, 0, 1, 0, 0, 0, 1},
        // P1 mounting direction, P0 is rotated 90° clockwise around the Z -
        // axis
        {0, -1, 0, 1, 0, 0, 0, 0, 1},
        // P2 mounting direction, P0 is rotated 180° clockwise around the Z -
        // axis
        {-1, 0, 0, 0, -1, 0, 0, 0, 1},
        // P3 mounting direction, P0 is rotated 270° clockwise around the Z -
        // axis
        {0, 1, 0, -1, 0, 0, 0, 0, 1},
        // P4 mounting direction, P0 is flipped vertically (rotated 180° around
        // the X - axis)
        {1, 0, 0, 0, -1, 0, 0, 0, -1},
        // P5 mounting direction, P4 is rotated 90° clockwise around the Z -
        // axis
        {0, 1, 0, 1, 0, 0, 0, 0, -1},
        // P6 mounting direction, P4 is rotated 180° clockwise around the Z -
        // axis
        {-1, 0, 0, 0, 1, 0, 0, 0, -1},
        // P7 mounting direction, P4 is rotated 270° clockwise around the Z -
        // axis
        {0, -1, 0, -1, 0, 0, 0, 0, -1}};

    // Gyroscope - related orientation matrices for different mounting
    // directions
    struct bhy2_orient_matrix gyro_matrices[] = {
        // P0 mounting direction, default direction, axis direction is
        // consistent with the default coordinate system
        {1, 0, 0, 0, 1, 0, 0, 0, 1},
        // P1 mounting direction, P0 is rotated 90° clockwise around the Z -
        // axis
        {0, -1, 0, 1, 0, 0, 0, 0, 1},
        // P2 mounting direction, P0 is rotated 180° clockwise around the Z -
        // axis
        {-1, 0, 0, 0, -1, 0, 0, 0, 1},
        // P3 mounting direction, P0 is rotated 270° clockwise around the Z -
        // axis
        {0, 1, 0, -1, 0, 0, 0, 0, 1},
        // P4 mounting direction, P0 is flipped vertically (rotated 180° around
        // the X - axis)
        {1, 0, 0, 0, -1, 0, 0, 0, -1},
        // P5 mounting direction, P4 is rotated 90° clockwise around the Z -
        // axis
        {0, 1, 0, 1, 0, 0, 0, 0, -1},
        // P6 mounting direction, P4 is rotated 180° clockwise around the Z -
        // axis
        {-1, 0, 0, 0, 1, 0, 0, 0, -1},
        // P7 mounting direction, P4 is rotated 270° clockwise around the Z -
        // axis
        {0, -1, 0, -1, 0, 0, 0, 0, -1}};

    // Set the orientation matrix for the accelerometer
    ret = bhy2_set_orientation_matrix(BHY2_PHYS_SENSOR_ID_ACCELEROMETER,
                                      acc_matrices[remap], &bhi260ap_dev);
    if (ret != BHY2_OK)
    {
        log_e("Set acceleration orientation matrix failed!");
        return false;
    }
    // Set the orientation matrix for the gyroscope
    ret = bhy2_set_orientation_matrix(BHY2_PHYS_SENSOR_ID_GYROSCOPE,
                                      gyro_matrices[remap], &bhi260ap_dev);
    if (ret != BHY2_OK)
    {
        log_e("Set gyroscope orientation matrix failed!");
        return false;
    }
    return true;
}

/**
 * @brief  bhi260ap_set_interrupt_ctrl
 * @note   Set the interrupt control mask
 * @param  data:
 *               BHY2_ICTL_DISABLE_FIFO_W
 *               BHY2_ICTL_DISABLE_FIFO_NW
 *               BHY2_ICTL_DISABLE_STATUS_FIFO
 *               BHY2_ICTL_DISABLE_DEBUG
 *               BHY2_ICTL_DISABLE_FAULT
 *               BHY2_ICTL_ACTIVE_LOW
 *               BHY2_ICTL_EDGE
 *               BHY2_ICTL_OPEN_DRAIN
 *
 * @retval true is success , false is failed
 */
bool bhi260ap_set_interrupt_ctrl(uint8_t data)
{
    int8_t ret;
    ret = bhy2_set_host_interrupt_ctrl(data, &bhi260ap_dev);
    if (ret != BHY2_OK)
    {
        return false;
    }
    return true;
}

/**
 * @brief  bhi260ap_get_interrupt_ctrl
 * @note   Get interrupt control info(reg: 0x07)
 * @retval SensorBHI260APCtrl class
 */
int8_t bhi260ap_get_interrupt_ctrl(void)
{
    int8_t ret;
    uint8_t data;
    ret = bhy2_get_host_interrupt_ctrl(&data, &bhi260ap_dev);
    if (ret != BHY2_OK)
    {
        return -1;
    }
    return data;
}

bool bhi260ap_is_sensor_available(uint8_t sensor_id)
{
    if (!bhy2_is_sensor_available(sensor_id, &bhi260ap_dev))
    {
        return false;
    }
    return true;
}

struct bhy2_data_xyz_float bhi260ap_get_acc_sensor_data()
{
    struct bhy2_data_xyz_float data;
    rt_mutex_take(sensor_data_mutex, RT_WAITING_FOREVER);
    data = acc_xyz;
    rt_mutex_release(sensor_data_mutex);
    return data;
}

struct bhy2_data_xyz_float bhi260ap_get_gyro_sensor_data()
{
    struct bhy2_data_xyz_float data;
    rt_mutex_take(sensor_data_mutex, RT_WAITING_FOREVER);
    data = gyro_xyz;
    rt_mutex_release(sensor_data_mutex);
    return data;
}

struct bhy2_data_orientation_float bhi260ap_get_orient_sensor_data()
{
    struct bhy2_data_orientation_float data;
    rt_mutex_take(sensor_data_mutex, RT_WAITING_FOREVER);
    data = orientation;
    rt_mutex_release(sensor_data_mutex);
    // LOG_D("orientation: pitch=%d, roll=%d, yaw=%d\n", orientation.pitch,
    //       orientation.roll, orientation.heading);
    return data;
}

struct bhy2_data_quaternion_float bhi260ap_get_quaternion_sensor_data()
{
    struct bhy2_data_quaternion_float data;
    rt_mutex_take(sensor_data_mutex, RT_WAITING_FOREVER);
    data = qaternion;
    rt_mutex_release(sensor_data_mutex);
    // LOG_D("qaternion: %.2f,%.2f,%.2f,%.2f,%.2f\n", qaternion.x, qaternion.y,
    // qaternion.z, qaternion.w,qaternion.accuracy);
    return data;
}

uint32_t bhi260ap_get_step_counter_sensor_data()
{
    // LOG_D("step_counter: %d\n", step_counter);
    return step_counter;
}

#endif