#ifndef __ESP32C6_AT_CMD_H__
#define __ESP32C6_AT_CMD_H__

#include "esp32c6_at_uart.h"
#include "rtthread.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* 预定义的 AT 命令响应 ID */
    enum
    {
        /* 基本 */
        AT_RESP_ID_TEST = 0,
        AT_RESP_ID_RST,
        AT_RESP_ID_GMR,
        AT_RESP_ID_ATE0,

        /* Wi-Fi */
        AT_RESP_ID_CWMODE,
        AT_RESP_ID_CWMODE_Q,
        AT_RESP_ID_CWSTATE,
        AT_RESP_ID_CWINIT,
        AT_RESP_ID_CWJAP,
        AT_RESP_ID_CWJAP_Q,
        AT_RESP_ID_CWQAP,
        AT_RESP_ID_CWDHCP,
        AT_RESP_ID_CIPAP,
        AT_RESP_ID_CIPSTA,

        /* TCP/IP */
        AT_RESP_ID_CIPSTART,
        AT_RESP_ID_CIPSEND,
        AT_RESP_ID_CIPCLOSE,
        AT_RESP_ID_CIPSTATUS,
        AT_RESP_ID_CIFSR,
        AT_RESP_ID_CIPDOMAIN,

        /* 时间 */
        AT_RESP_ID_SNTPCFG,
        AT_RESP_ID_SNTPTIME_Q,

        /* 蓝牙经典 */
        AT_RESP_ID_BTINIT,
        AT_RESP_ID_BTNAME,
        AT_RESP_ID_BTSCANMODE,
        AT_RESP_ID_BTSTATUS,
        AT_RESP_ID_BTSPPCONN,
        AT_RESP_ID_BTSPPSEND,
        AT_RESP_ID_BTSPPDISCONN,

        /* BLE */
        AT_RESP_ID_BLEINIT,
        AT_RESP_ID_BLENAME,
        AT_RESP_ID_BLESCAN,
        AT_RESP_ID_BLEADV,
        AT_RESP_ID_BLECONN,
        AT_RESP_ID_BLEWRITE,
        AT_RESP_ID_BLEREAD,

        /* 其他 */
        AT_RESP_ID_PING,
        AT_RESP_ID_CIUPDATE,
        AT_RESP_ID_SLEEP,
        AT_RESP_ID_DEEP_SLEEP,
        AT_RESP_ID_RFPOWER,

        AT_RESP_ID_USER, /* 用户自定义起始 ID */
    };

    /* ================================================================ */
    /*  基本 AT 命令                                                      */
    /* ================================================================ */
    rt_err_t esp32_at_test(int resp_id);
    rt_err_t esp32_at_rst(int resp_id);
    rt_err_t esp32_at_gmr(int resp_id);
    rt_err_t esp32_at_ate0(int resp_id);

    /* ================================================================ */
    /*  Wi-Fi                                                            */
    /* ================================================================ */
    rt_err_t esp32_at_cwmode(int resp_id, int mode);
    rt_err_t esp32_at_cwmode_q(int resp_id);
    rt_err_t esp32_at_cwstate(int resp_id);
    rt_err_t esp32_at_cwinit(int resp_id);
    rt_err_t esp32_at_cwjap(int resp_id, const char *ssid, const char *pwd);
    rt_err_t esp32_at_cwjap_q(int resp_id);
    rt_err_t esp32_at_cwqap(int resp_id);
    rt_err_t esp32_at_cwdhcp(int resp_id, int enable, int type);
    rt_err_t esp32_at_cipap(int resp_id, const char *ip, const char *gateway,
                            const char *netmask);
    rt_err_t esp32_at_cipsta(int resp_id, const char *ip, const char *gateway,
                             const char *netmask);

    /* ================================================================ */
    /*  TCP/IP                                                           */
    /* ================================================================ */
    rt_err_t esp32_at_cipstart(int resp_id, const char *type, const char *addr,
                               uint16_t port);
    rt_err_t esp32_at_cipsend(int resp_id, const char *data);
    rt_err_t esp32_at_cipclose(int resp_id);
    rt_err_t esp32_at_cipstatus(int resp_id);
    rt_err_t esp32_at_cifsr(int resp_id);
    rt_err_t esp32_at_cipdomain(int resp_id, const char *domain);
    rt_err_t esp32_at_ping(int resp_id, const char *host);

    /* ================================================================ */
    /*  时间 / SNTP                                                       */
    /* ================================================================ */
    rt_err_t esp32_at_sntp_cfg(int resp_id, int enable, int zone,
                               const char *server);
    rt_err_t esp32_at_sntp_time_q(int resp_id);

    /* ================================================================ */
    /*  蓝牙经典                                                          */
    /* ================================================================ */
    rt_err_t esp32_at_btinit(int resp_id);
    rt_err_t esp32_at_btname(int resp_id, const char *name);
    rt_err_t esp32_at_btscanmode(int resp_id, int mode);
    rt_err_t esp32_at_btstatus_q(int resp_id);
    rt_err_t esp32_at_btsppconn(int resp_id, int role, const char *remote_addr);
    rt_err_t esp32_at_btsppsend(int resp_id, int len);
    rt_err_t esp32_at_btsppdisconn(int resp_id);

    /* ================================================================ */
    /*  BLE                                                              */
    /* ================================================================ */
    rt_err_t esp32_at_bleinit(int resp_id, int role);
    rt_err_t esp32_at_blename(int resp_id, const char *name);
    rt_err_t esp32_at_blescan(int resp_id, int enable);
    rt_err_t esp32_at_bleadv(int resp_id, int enable);
    rt_err_t esp32_at_bleconn(int resp_id, const char *addr);
    rt_err_t esp32_at_blewrite(int resp_id, const char *service_uuid,
                               const char *char_uuid, const char *data);
    rt_err_t esp32_at_ble_read(int resp_id, const char *service_uuid,
                               const char *char_uuid);

    /* ================================================================ */
    /*  其他                                                             */
    /* ================================================================ */
    rt_err_t esp32_at_ciupdate(int resp_id);
    rt_err_t esp32_at_sleep(int resp_id, int mode);
    rt_err_t esp32_at_deep_sleep(int resp_id, int durationms);
    rt_err_t esp32_at_rfpower(int resp_id, int power);

#ifdef __cplusplus
}
#endif

#endif /* __ESP32C6_AT_CMD_H__ */
