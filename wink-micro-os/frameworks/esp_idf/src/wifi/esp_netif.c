/* SPDX-License-Identifier: LGPL-3.0-only */
/* src/wifi/esp_netif.c — 规范 C-ABI 多网卡实现 */
#include "esp_netif.h"
#include "esp_wifi.h"
#include <string.h>

#define MAX_NETIF_INSTANCES 4

struct esp_netif_obj {
    bool valid;
    bool is_ap;
    esp_netif_ip_info_t ip_info;
    uint8_t mac[6];
    char if_key[16];
};

static struct esp_netif_obj s_netifs[MAX_NETIF_INSTANCES];
static struct esp_netif_obj *s_sta_handle = NULL;
static struct esp_netif_obj *s_ap_handle = NULL;

esp_err_t esp_netif_init(void) {
    memset(s_netifs, 0, sizeof(s_netifs));
    s_sta_handle = NULL;
    s_ap_handle = NULL;
    return ESP_OK;
}

esp_err_t esp_netif_deinit(void) {
    memset(s_netifs, 0, sizeof(s_netifs));
    s_sta_handle = NULL;
    s_ap_handle = NULL;
    return ESP_OK;
}

esp_netif_t* esp_netif_create_default_wifi_sta(void) {
    if (s_sta_handle && s_sta_handle->valid) {
        return s_sta_handle;
    }
    struct esp_netif_obj *o = &s_netifs[0];
    memset(o, 0, sizeof(*o));
    o->valid = true;
    o->is_ap = false;
    strncpy(o->if_key, "WIFI_STA_DEF", sizeof(o->if_key) - 1);
    o->mac[0] = 0xDE; o->mac[1] = 0xAD; o->mac[2] = 0xBE;
    o->mac[3] = 0xEF; o->mac[4] = 0x00; o->mac[5] = 0x01;
    o->ip_info.ip.addr = ESP_IP4TOADDR(192, 168, 4, 2);
    o->ip_info.netmask.addr = ESP_IP4TOADDR(255, 255, 255, 0);
    o->ip_info.gw.addr = ESP_IP4TOADDR(192, 168, 4, 1);
    s_sta_handle = o;
    return s_sta_handle;
}

esp_netif_t* esp_netif_create_default_wifi_ap(void) {
    if (s_ap_handle && s_ap_handle->valid) {
        return s_ap_handle;
    }
    struct esp_netif_obj *o = &s_netifs[1];
    memset(o, 0, sizeof(*o));
    o->valid = true;
    o->is_ap = true;
    strncpy(o->if_key, "WIFI_AP_DEF", sizeof(o->if_key) - 1);
    o->mac[0] = 0xDE; o->mac[1] = 0xAD; o->mac[2] = 0xBE;
    o->mac[3] = 0xEF; o->mac[4] = 0x00; o->mac[5] = 0x02;
    o->ip_info.ip.addr = ESP_IP4TOADDR(192, 168, 4, 1);
    o->ip_info.netmask.addr = ESP_IP4TOADDR(255, 255, 255, 0);
    o->ip_info.gw.addr = ESP_IP4TOADDR(192, 168, 4, 1);
    s_ap_handle = o;
    return s_ap_handle;
}

esp_err_t esp_netif_destroy_default_wifi(esp_netif_t *netif) {
    if (!netif) {
        return ESP_ERR_INVALID_ARG;
    }
    struct esp_netif_obj *o = (struct esp_netif_obj*)netif;
    if (o == s_sta_handle) {
        s_sta_handle = NULL;
    } else if (o == s_ap_handle) {
        s_ap_handle = NULL;
    }
    memset(o, 0, sizeof(*o));
    return ESP_OK;
}

esp_err_t esp_netif_get_ip_info(esp_netif_t *netif, esp_netif_ip_info_t *ip_info) {
    if (!netif || !ip_info) {
        return ESP_ERR_INVALID_ARG;
    }
    struct esp_netif_obj *o = (struct esp_netif_obj*)netif;
    if (!o->valid) {
        return ESP_ERR_INVALID_STATE;
    }
    if (o->is_ap) {
        *ip_info = o->ip_info;
        return ESP_OK;
    }
    if (esp_wifi_sim_is_connected()) {
        *ip_info = o->ip_info;
    } else {
        memset(ip_info, 0, sizeof(*ip_info));
    }
    return ESP_OK;
}

/* 内部辅助函数 */
esp_netif_t* esp_netif_get_handle_sta(void) {
    if (s_sta_handle && s_sta_handle->valid) {
        return s_sta_handle;
    }
    return esp_netif_create_default_wifi_sta();
}

void esp_netif_set_sta_ip_info(const esp_netif_ip_info_t *info) {
    if (s_sta_handle && info) {
        s_sta_handle->ip_info = *info;
    }
}
