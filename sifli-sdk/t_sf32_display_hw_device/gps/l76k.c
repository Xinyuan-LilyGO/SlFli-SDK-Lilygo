#include "l76k.h"
#include "tinygps.h"
#include "xl9555.h"
#include "uart_mux.h"

#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include <stdlib.h>

/* 调试开关 */
#define L76K_DEBUG 1
#if L76K_DEBUG
#define L76K_LOG(...)      rt_kprintf("[L76K] " __VA_ARGS__)
#else
#define L76K_LOG(...)
#endif

/* 互斥锁保护 GPS 数据结构 */
static struct rt_mutex l76k_mutex;

/* TinyGPS 对象 */
static TinyGPSPlus l76k_gps;

/* 工作模式 */
static L76K_Mode l76k_cur_mode = L76K_MODE_NORMAL;

/* 支持的波特率列表 */
static const uint32_t supported_baudrates[] = {4800, 9600, 19200, 38400, 57600, 115200};
#define L76K_DEFAULT_BAUDRATES  supported_baudrates[1]  /* 9600 */

/* 计算 NMEA 校验和 */
static uint8_t nmea_checksum(const char *nmea)
{
    uint8_t cs = 0;
    if (*nmea == '$') nmea++;
    while (*nmea && *nmea != '*')
        cs ^= *nmea++;
    return cs;
}

/* 硬件控制 */
static void l76k_en(void)
{
    xl9555_pin_mode(XL9555_GPS_EN_PIN, XL9555_PIN_OUTPUT);
    xl9555_digital_write(XL9555_GPS_EN_PIN, 0);
    rt_thread_mdelay(100);
    xl9555_digital_write(XL9555_GPS_EN_PIN, 1);
    rt_thread_mdelay(200);
}

static void l76k_reset(void)
{
    xl9555_pin_mode(XL9555_GPS_RST_PIN, XL9555_PIN_OUTPUT);
    xl9555_digital_write(XL9555_GPS_RST_PIN, 0);
    rt_thread_mdelay(100);
    xl9555_digital_write(XL9555_GPS_RST_PIN, 1);
    rt_thread_mdelay(500);
}

/* 设置 L76K 模块波特率 */
static int l76k_set_module_baudrate(uint32_t new_baudrate)
{
    char cmd[32];
    uint8_t cs;
    int cmd_value;

    switch (new_baudrate)
    {
    case 4800:   cmd_value = 0; break;
    case 9600:   cmd_value = 1; break;
    case 19200:  cmd_value = 2; break;
    case 38400:  cmd_value = 3; break;
    case 57600:  cmd_value = 4; break;
    case 115200: cmd_value = 5; break;
    default:
        L76K_LOG("Unsupported baudrate %lu\n", new_baudrate);
        return -1;
    }

    rt_snprintf(cmd, sizeof(cmd), "$PCAS01,%d", cmd_value);
    cs = nmea_checksum(cmd);
    rt_snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "*%02X\r\n", cs);

    L76K_LOG("Sending baudrate change command: %s", cmd);

    if (l76k_send_cmd(cmd, strlen(cmd) + 1) != 0)
        return -2;

    rt_thread_mdelay(500);

    /* 通知 uart_mux 切换波特率 */
    if (uart_mux_switch_to(UART_MUX_DEVICE_GPS, new_baudrate) != RT_EOK)
    {
        L76K_LOG("Failed to switch mux baudrate\n");
        return -3;
    }

    L76K_LOG("Module baudrate changed to %lu\n", new_baudrate);
    return 0;
}

int l76k_set_baudrate(uint32_t new_baudrate)
{
    if (uart_mux_get_current_device() != UART_MUX_DEVICE_GPS)
        return -1;
    return l76k_set_module_baudrate(new_baudrate);
}

int l76k_set_mode(L76K_Mode mode)
{
    const char *cmd = NULL;
    size_t len = 0;

    switch (mode)
    {
    case L76K_MODE_STANDBY: cmd = "$PMTK161,0*28\r\n"; len = strlen(cmd); break;
    case L76K_MODE_NORMAL:  cmd = "$PMTK010,1*2E\r\n"; len = strlen(cmd); break;
    case L76K_MODE_SLEEP:   cmd = "$PMTK161,1*29\r\n"; len = strlen(cmd); break;
    case L76K_MODE_BACKUP:  cmd = "$PMTK161,2*2A\r\n"; len = strlen(cmd); break;
    default: return -1;
    }

    if (l76k_send_cmd(cmd, len) == 0)
    {
        l76k_cur_mode = mode;
        return 0;
    }
    return -2;
}

/* 获取定位数据 */
int l76k_get_location(double *lat, double *lon)
{
    if (!lat || !lon) return -1;
    rt_mutex_take(&l76k_mutex, RT_WAITING_FOREVER);
    *lat = TinyGPSLocation_lat(&l76k_gps.location);
    *lon = TinyGPSLocation_lng(&l76k_gps.location);
    rt_mutex_release(&l76k_mutex);
    return 0;
}

int l76k_get_time(int *year, int *month, int *day, int *hour, int *minute, int *second)
{
    if (!year || !month || !day || !hour || !minute || !second) return -1;
    rt_mutex_take(&l76k_mutex, RT_WAITING_FOREVER);
    *year  = TinyGPSDate_year(&l76k_gps.date);
    *month = TinyGPSDate_month(&l76k_gps.date);
    *day   = TinyGPSDate_day(&l76k_gps.date);
    *hour  = TinyGPSTime_hour(&l76k_gps.time);
    *minute= TinyGPSTime_minute(&l76k_gps.time);
    *second= TinyGPSTime_second(&l76k_gps.time);
    rt_mutex_release(&l76k_mutex);
    return 0;
}

int l76k_get_altitude(double *altitude)
{
    if (!altitude) return -1;
    rt_mutex_take(&l76k_mutex, RT_WAITING_FOREVER);
    *altitude = TinyGPSAltitude_meters(&l76k_gps.altitude);
    rt_mutex_release(&l76k_mutex);
    return 0;
}

int l76k_get_speed(double *speed)
{
    if (!speed) return -1;
    rt_mutex_take(&l76k_mutex, RT_WAITING_FOREVER);
    *speed = TinyGPSSpeed_kmph(&l76k_gps.speed);
    rt_mutex_release(&l76k_mutex);
    return 0;
}

int l76k_get_course(double *course)
{
    if (!course) return -1;
    rt_mutex_take(&l76k_mutex, RT_WAITING_FOREVER);
    *course = TinyGPSCourse_deg(&l76k_gps.course);
    rt_mutex_release(&l76k_mutex);
    return 0;
}

int l76k_get_satellites(uint32_t *satellites)
{
    if (!satellites) return -1;
    rt_mutex_take(&l76k_mutex, RT_WAITING_FOREVER);
    *satellites = TinyGPSInteger_value(&l76k_gps.satellites);
    rt_mutex_release(&l76k_mutex);
    return 0;
}

int l76k_get_hdop(double *hdop)
{
    if (!hdop) return -1;
    rt_mutex_take(&l76k_mutex, RT_WAITING_FOREVER);
    *hdop = TinyGPSHDOP_hdop(&l76k_gps.hdop);
    rt_mutex_release(&l76k_mutex);
    return 0;
}

bool l76k_is_fixed(void)
{
    bool valid;
    rt_mutex_take(&l76k_mutex, RT_WAITING_FOREVER);
    valid = TinyGPSLocation_isValid(&l76k_gps.location);
    rt_mutex_release(&l76k_mutex);
    return valid;
}

int l76k_get_gps_info(GPSInfo *info)
{
    if (!info) return -1;
    info->valid = l76k_is_fixed();
    l76k_get_location(&info->latitude, &info->longitude);
    l76k_get_time(&info->time.year, &info->time.month, &info->time.day,
                  &info->time.hour, &info->time.minute, &info->time.second);
    l76k_get_altitude(&info->altitude);
    l76k_get_speed(&info->speed);
    l76k_get_course(&info->course);
    l76k_get_satellites(&info->satellites);
    l76k_get_hdop(&info->hdop);
    return 0;
}

/* 接收回调：每收到一个字节，喂给 TinyGPS */
static void l76k_rx_callback(uint8_t data)
{
    rt_mutex_take(&l76k_mutex, RT_WAITING_FOREVER);
    TinyGPSPlus_encode(&l76k_gps, data);
    rt_mutex_release(&l76k_mutex);
    /* 可在此打印调试信息 */
    // rt_kprintf("%c", data);
}

/* 发送命令（使用 uart_mux 发送接口） */
int l76k_send_cmd(const char *cmd, size_t len)
{
    if (uart_mux_get_current_device() != UART_MUX_DEVICE_GPS)
        return -1;
    int sent = uart_mux_send((const uint8_t *)cmd, len);
    if (sent != (int)len)
    {
        L76K_LOG("send cmd failed, sent=%d, len=%d\n", sent, len);
        return -2;
    }
    return 0;
}

/* 初始化 */
int l76k_init(const char *uart_name, uint32_t baudrate)
{
    /* 硬件使能 */
    l76k_en();
    l76k_reset();

    rt_mutex_init(&l76k_mutex, "l76k_mtx", RT_IPC_FLAG_PRIO);
    TinyGPSPlus_init(&l76k_gps);

    /* 注册接收回调到 uart_mux */
    uart_mux_register_rx_callback(UART_MUX_DEVICE_GPS, l76k_rx_callback);

    /* 切换到 GPS 通路，默认先用 L76K_DEFAULT_BAUDRATES 与模块通信 */
    if (uart_mux_switch_to(UART_MUX_DEVICE_GPS, L76K_DEFAULT_BAUDRATES) != RT_EOK)
    {
        L76K_LOG("Failed to switch to GPS\n");
        rt_mutex_detach(&l76k_mutex);
        return -1;
    }

    /* 如果目标波特率不是 L76K_DEFAULT_BAUDRATES，则配置模块 */
    if (baudrate != L76K_DEFAULT_BAUDRATES)
    {
        if (l76k_set_module_baudrate(baudrate) != 0)
        {
            L76K_LOG("Failed to set module baudrate to %lu\n", baudrate);
            l76k_deinit();
            return -2;
        }
    }

    L76K_LOG("Initialized, baudrate=%lu\n", baudrate);
    return 0;
}

void l76k_deinit(void)
{
    /* 注销回调 */
    uart_mux_register_rx_callback(UART_MUX_DEVICE_GPS, RT_NULL);
    rt_mutex_detach(&l76k_mutex);
    L76K_LOG("Deinitialized\n");
}

#ifdef RT_USING_FINSH
#include <finsh.h>
static void l76k_info(void)
{
    double lat, lon, alt, speed, course, hdop;
    int y, m, d, h, min, sec;
    uint32_t sats;

    if (!l76k_is_fixed())
    {
        rt_kprintf("No fix yet.\n");
        return;
    }

    l76k_get_location(&lat, &lon);
    l76k_get_altitude(&alt);
    l76k_get_speed(&speed);
    l76k_get_course(&course);
    l76k_get_hdop(&hdop);
    l76k_get_time(&y, &m, &d, &h, &min, &sec);
    l76k_get_satellites(&sats);

    rt_kprintf("Time: %04d-%02d-%02d %02d:%02d:%02d UTC\n", y, m, d, h, min, sec);
    rt_kprintf("Lat: %.6f, Lon: %.6f\n", lat, lon);
    rt_kprintf("Alt: %.2f m\n", alt);
    rt_kprintf("Speed: %.2f km/h\n", speed);
    rt_kprintf("Course: %.1f deg\n", course);
    rt_kprintf("Satellites: %d\n", sats);
    rt_kprintf("HDOP: %.2f\n", hdop);
}
MSH_CMD_EXPORT(l76k_info, show L76K GPS information);

static void l76k_set_baud(int argc, char **argv)
{
    if (argc != 2)
    {
        rt_kprintf("Usage: l76k_set_baud <baudrate>\n");
        return;
    }
    uint32_t baud = atoi(argv[1]);
    if (l76k_set_baudrate(baud) == 0)
        rt_kprintf("Set baudrate to %lu success\n", baud);
    else
        rt_kprintf("Set baudrate failed\n");
}
MSH_CMD_EXPORT(l76k_set_baud, l76k_baud);
#endif