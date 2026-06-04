#include "esp32c6_at_cmd.h"

/* ================================================================ */
/*  基本 AT 命令                                                      */
/* ================================================================ */

rt_err_t esp32_at_test(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_rst(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+RST\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+RST\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_gmr(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+GMR\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+GMR\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_ate0(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "ATE0\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send ATE0\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

/* ================================================================ */
/*  Wi-Fi                                                            */
/* ================================================================ */

rt_err_t esp32_at_cwmode(int resp_id, int mode)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d\r\n", mode);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CWMODE\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cwmode_q(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+CWMODE?\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CWMODE?\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cwstate(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+CWSTATE?\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CWSTATE\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cwinit(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+CWINIT=1\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CWINIT\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cwjap(int resp_id, const char *ssid, const char *pwd)
{
    char cmd[256];
    rt_snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CWJAP\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cwjap_q(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+CWJAP?\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CWJAP?\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cwqap(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+CWQAP\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CWQAP\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cwdhcp(int resp_id, int enable, int type)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+CWDHCP=%d,%d\r\n", enable, type);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CWDHCP\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cipap(int resp_id, const char *ip, const char *gateway,
                        const char *netmask)
{
    char cmd[128];
    rt_snprintf(cmd, sizeof(cmd), "AT+CIPAP=\"%s\",\"%s\",\"%s\"\r\n", ip,
                gateway, netmask);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CIPAP\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cipsta(int resp_id, const char *ip, const char *gateway,
                         const char *netmask)
{
    char cmd[128];
    rt_snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"\r\n", ip,
                gateway, netmask);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CIPSTA\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

/* ================================================================ */
/*  TCP/IP                                                           */
/* ================================================================ */

rt_err_t esp32_at_cipstart(int resp_id, const char *type, const char *addr,
                           uint16_t port)
{
    char cmd[256];
    rt_snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"%s\",\"%s\",%u\r\n", type,
                addr, port);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CIPSTART\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cipsend(int resp_id, const char *data)
{
    char cmd[512];
    rt_snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", (int)strlen(data));
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CIPSEND\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cipclose(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+CIPCLOSE\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CIPCLOSE\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cipstatus(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+CIPSTATUS\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CIPSTATUS\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cifsr(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+CIFSR\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CIFSR\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_cipdomain(int resp_id, const char *domain)
{
    char cmd[256];
    rt_snprintf(cmd, sizeof(cmd), "AT+CIPDOMAIN=\"%s\"\r\n", domain);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CIPDOMAIN\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_ping(int resp_id, const char *host)
{
    char cmd[256];
    rt_snprintf(cmd, sizeof(cmd), "AT+PING=\"%s\"\r\n", host);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+PING\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

/* ================================================================ */
/*  时间 / SNTP                                                       */
/* ================================================================ */

rt_err_t esp32_at_sntp_cfg(int resp_id, int enable, int zone,
                           const char *server)
{
    char cmd[256];
    if (server != RT_NULL && server[0] != '\0')
        rt_snprintf(cmd, sizeof(cmd), "AT+CIPSNTPCFG=%d,%d,\"%s\"\r\n", enable,
                    zone, server);
    else
        rt_snprintf(cmd, sizeof(cmd), "AT+CIPSNTPCFG=%d,%d,\r\n", enable, zone);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CIPSNTPCFG\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_sntp_time_q(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+CIPSNTPTIME?\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CIPSNTPTIME?\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

/* ================================================================ */
/*  蓝牙经典                                                          */
/* ================================================================ */

rt_err_t esp32_at_btinit(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+BTINIT=1\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BTINIT\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_btname(int resp_id, const char *name)
{
    char cmd[128];
    rt_snprintf(cmd, sizeof(cmd), "AT+BTNAME=\"%s\"\r\n", name);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BTNAME\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_btscanmode(int resp_id, int mode)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+BTSCANMODE=%d\r\n", mode);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BTSCANMODE\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_btstatus_q(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+BTSTATUS?\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BTSTATUS?\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_btsppconn(int resp_id, int role, const char *remote_addr)
{
    char cmd[256];
    rt_snprintf(cmd, sizeof(cmd), "AT+BTSPPCONN=%d,\"%s\"\r\n", role,
                remote_addr);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BTSPPCONN\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_btsppsend(int resp_id, int len)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+BTSPPSEND=%d\r\n", len);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BTSPPSEND\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_btsppdisconn(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+BTSPPDISCONN\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BTSPPDISCONN\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

/* ================================================================ */
/*  BLE                                                              */
/* ================================================================ */

rt_err_t esp32_at_bleinit(int resp_id, int role)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+BLEINIT=%d\r\n", role);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BLEINIT\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_blename(int resp_id, const char *name)
{
    char cmd[128];
    rt_snprintf(cmd, sizeof(cmd), "AT+BLENAME=\"%s\"\r\n", name);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BLENAME\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_blescan(int resp_id, int enable)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+BLESCAN=%d\r\n", enable);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BLESCAN\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_bleadv(int resp_id, int enable)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+BLEADV=%d\r\n", enable);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BLEADV\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_bleconn(int resp_id, const char *addr)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+BLECONN=\"%s\"\r\n", addr);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BLECONN\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_blewrite(int resp_id, const char *service_uuid,
                           const char *char_uuid, const char *data)
{
    char cmd[512];
    rt_snprintf(cmd, sizeof(cmd), "AT+BLEWRITE=\"%s\",\"%s\",\"%s\"\r\n",
                service_uuid, char_uuid, data);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BLEWRITE\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_ble_read(int resp_id, const char *service_uuid,
                           const char *char_uuid)
{
    char cmd[256];
    rt_snprintf(cmd, sizeof(cmd), "AT+BLEREAD=\"%s\",\"%s\"\r\n", service_uuid,
                char_uuid);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+BLEREAD\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

/* ================================================================ */
/*  其他                                                             */
/* ================================================================ */

rt_err_t esp32_at_ciupdate(int resp_id)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+CIUPDATE\r\n");
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+CIUPDATE\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_sleep(int resp_id, int mode)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+SLEEP=%d\r\n", mode);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+SLEEP\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_deep_sleep(int resp_id, int durationms)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+GSLP=%d\r\n", durationms);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+GSLP\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t esp32_at_rfpower(int resp_id, int power)
{
    char cmd[64];
    rt_snprintf(cmd, sizeof(cmd), "AT+RFPOWER=%d\r\n", power);
    if (esp32_at_send((const uint8_t *)cmd, strlen(cmd), resp_id) != RT_EOK)
    {
        AT_LOG("Failed to send AT+RFPOWER\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}
