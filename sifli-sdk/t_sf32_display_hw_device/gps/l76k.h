#ifndef __L76K_H__
#define __L76K_H__

#include "rtthread.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* L76K 工作模式 */
    typedef enum
    {
        L76K_MODE_STANDBY = 0,
        L76K_MODE_NORMAL,
        L76K_MODE_SLEEP,
        L76K_MODE_BACKUP
    } L76K_Mode;

    typedef struct GPSTime
    {
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
    } gpstime;

    /* GPS信息结构体 */
    typedef struct GPSInfo
    {
        double latitude;     // 纬度（度）
        double longitude;    // 经度（度）
        double altitude;     // 海拔高度（米）
        double speed;        // 速度（km/h）
        double course;       // 航向（度）
        double hdop;         // 水平精度因子
        uint32_t satellites; // 可见卫星数
        bool valid;          // 定位是否有效
        struct GPSTime time; // UTC时间
    } GPSInfo;

    /* 初始化 L76K 设备
     * uart_name : 串口设备名称，如 "uart2"（仅用于注册，实际管理由 uart_mux 负责）
     * baudrate  : 目标波特率（模块最终使用的波特率，如 115200）
     * 返回值    : 0 成功，其他失败
     */
    int l76k_init(const char *uart_name, uint32_t baudrate);
    
    void uatr_select_gps(void); // 已弃用，请使用 uart_mux_switch_to

    /* 反初始化，释放资源 */
    void l76k_deinit(void);

    /* 发送原始命令到 L76K（使用当前已设置的波特率） */
    int l76k_send_cmd(const char *cmd, size_t len);

    /* 设置工作模式 */
    int l76k_set_mode(L76K_Mode mode);

    /* 运行时重新设置模块波特率（同时切换主机串口） */
    int l76k_set_baudrate(uint32_t new_baudrate);

    /* 获取定位数据（经纬度，单位：度） */
    int l76k_get_location(double *lat, double *lon);

    /* 获取时间（UTC 时间） */
    int l76k_get_time(int *year, int *month, int *day, int *hour, int *minute,
                      int *second);

    /* 获取海拔高度（米） */
    int l76k_get_altitude(double *altitude);

    /* 获取速度（km/h） */
    int l76k_get_speed(double *speed);

    /* 获取航向（度） */
    int l76k_get_course(double *course);

    /* 获取可见卫星数 */
    int l76k_get_satellites(uint32_t *satellites);

    /* 获取水平精度因子 HDOP */
    int l76k_get_hdop(double *hdop);

    /* 检查定位是否有效 */
    bool l76k_is_fixed(void);

    /* 获取所有GPS信息 */
    int l76k_get_gps_info(GPSInfo *info);

#ifdef __cplusplus
}
#endif

#endif /* __L76K_H__ */