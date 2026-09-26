/* SPDX-License-Identifier: LGPL-3.0-only */
/* src/wifi/esp_netif.c — 规范 C-ABI 实现 */
#include "esp_netif.h"
#include "esp_wifi.h"
#include <string.h>

static struct esp_netif_obj {
    bool valid;
    esp_netif_ip_info_t ip_info;
} s_sta_obj;

static esp_netif_t s_sta_handle = NULL;

esp_err_t esp_netif_init(void) {
    memset(&s_sta_obj, 0, sizeof(s_sta_obj));
    s_sta_handle = NULL;
    return ESP_OK;
}

esp_err_t esp_netif_deinit(void) {
    s_sta_handle = NULL;
    return ESP_OK;
}

esp_netif_t esp_netif_create_default_wifi_sta(void) {
    s_sta_obj.valid = true;
    s_sta_obj.ip_info.ip.addr = ESP_IP4TOADDR(192, 168, 4, 2);
    s_sta_obj.ip_info.netmask.addr = ESP_IP4TOADDR(255, 255, 255, 0);
    s_sta_obj.ip_info.gw.addr = ESP_IP4TOADDR(192, 168, 4, 1);
    s_sta_handle = &s_sta_obj;
    return s_sta_handle;
}

esp_err_t esp_netif_destroy_default_wifi(esp_netif_t netif) {
    if (netif == s_sta_handle) {
        memset(&s_sta_obj, 0, sizeof(s_sta_obj));
        s_sta_handle = NULL;
    }
    return ESP_OK;
}

esp_err_t esp_netif_get_ip_info(esp_netif_t netif, esp_netif_ip_info_t *ip_info) {
    if (!netif || !ip_info) {
        return ESP_ERR_INVALID_ARG;
    }
    struct esp_netif_obj *o = (struct esp_netif_obj*)netif;
    if (!o->valid) {
        return ESP_ERR_INVALID_STATE;
    }
    if (esp_wifi_sim_is_connected()) {
        *ip_info = o->ip_info;
    } else {
        memset(ip_info, 0, sizeof(*ip_info));
    }
    return ESP_OK;
}
