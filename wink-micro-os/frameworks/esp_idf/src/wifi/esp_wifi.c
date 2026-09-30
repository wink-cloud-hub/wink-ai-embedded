/* SPDX-License-Identifier: LGPL-3.0-only */
/* src/wifi/esp_wifi.c — 确定性虚拟空口与状态机加固版 */
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos_sync.h"
#include "sim_wifi_env.h"
#include "sim_network_broker.h"
#include <string.h>

ESP_EVENT_DEFINE_BASE(WIFI_EVENT);
ESP_EVENT_DEFINE_BASE(IP_EVENT);

static const uint8_t SIM_STA_MAC[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
static const uint8_t SIM_AP_MAC[6]  = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x02};

extern esp_netif_t* esp_netif_get_handle_sta(void);
extern void esp_netif_set_sta_ip_info(const esp_netif_ip_info_t *info);

typedef enum {
    WIFI_SIM_OFF = 0,
    WIFI_SIM_INIT,
    WIFI_SIM_STARTED,
    WIFI_SIM_CONNECTING,
    WIFI_SIM_CONNECTED,
    WIFI_SIM_GOT_IP,
    WIFI_SIM_DISCONNECTED
} wifi_sim_state_t;

typedef struct {
    wifi_sim_state_t state;
    wifi_mode_t mode;
    wifi_sta_config_t sta_cfg;
    wifi_ap_config_t  ap_cfg;
    uint8_t sta_mac[6];
    uint8_t ap_mac[6];
    bool initialized;
} esp_wifi_sim_t;

static esp_wifi_sim_t s_wifi = {.state = WIFI_SIM_OFF, .mode = WIFI_MODE_STA};
static uint32_t s_wifi_generation = 0;
static uint32_t s_wifi_work_id = 0;

static inline size_t safe_strnlen(const char *s, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && s[len] != '\0') {
        len++;
    }
    return len;
}

static void on_beacon_drop(const char *target_ssid, uint8_t reason) {
    if (!s_wifi.initialized || s_wifi.state < WIFI_SIM_CONNECTING) {
        return;
    }
    if (target_ssid && target_ssid[0] != '\0') {
        if (strcmp((const char*)s_wifi.sta_cfg.ssid, target_ssid) != 0) {
            return;
        }
    }
    s_wifi_generation++;
    if (s_wifi_work_id != 0) {
        esp_freertos_timer_cancel_work_item(s_wifi_work_id);
        s_wifi_work_id = 0;
    }
    s_wifi.state = WIFI_SIM_DISCONNECTED;
    sim_wifi_env_set_state(WIFI_SIM_DISCONNECTED);
    sim_network_broker_set_ready(false);

    wifi_event_sta_disconnected_t de;
    memset(&de, 0, sizeof(de));
    de.reason = reason;
    memcpy(de.ssid, s_wifi.sta_cfg.ssid, WIFI_SSID_LEN);
    de.ssid_len = (uint8_t)safe_strnlen((const char*)s_wifi.sta_cfg.ssid, WIFI_SSID_LEN);
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &de, sizeof(de), portMAX_DELAY);
}

static void wifi_connect_work_cb(void *arg, uint32_t work_token) {
    (void)work_token;
    uint32_t my_token = (uint32_t)(uintptr_t)arg;

    /* 临界安全检查：若中途断开/停止/重置或 token 不匹配，安全退出 */
    if (!s_wifi.initialized || s_wifi.state != WIFI_SIM_CONNECTING || s_wifi_generation != my_token) {
        return;
    }

    /* 查找虚拟 AP */
    const sim_wifi_ap_t *ap = sim_wifi_env_find_ap_by_ssid((const char*)s_wifi.sta_cfg.ssid);
    if (!ap || ap->drop_beacon) {
        /* AP 未找到或信标丢失 (Drop Beacon 故障注入中) */
        s_wifi.state = WIFI_SIM_DISCONNECTED;
        sim_wifi_env_set_state(WIFI_SIM_DISCONNECTED);
        sim_network_broker_set_ready(false);
        wifi_event_sta_disconnected_t de;
        memset(&de, 0, sizeof(de));
        de.reason = (ap && ap->drop_beacon && ap->fault_reason) ? ap->fault_reason : WIFI_REASON_NO_AP_FOUND;
        memcpy(de.ssid, s_wifi.sta_cfg.ssid, WIFI_SSID_LEN);
        de.ssid_len = (uint8_t)safe_strnlen((const char*)s_wifi.sta_cfg.ssid, WIFI_SSID_LEN);
        esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &de, sizeof(de), portMAX_DELAY);
        return;
    }

    /* 密码核对 */
    if (ap->authmode != WIFI_AUTH_OPEN) {
        if (strcmp((const char*)s_wifi.sta_cfg.password, ap->password) != 0) {
            /* 密码错误 / 4次握手超时 */
            s_wifi.state = WIFI_SIM_DISCONNECTED;
            sim_wifi_env_set_state(WIFI_SIM_DISCONNECTED);
            sim_network_broker_set_ready(false);
            wifi_event_sta_disconnected_t de;
            memset(&de, 0, sizeof(de));
            de.reason = WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT;
            memcpy(de.ssid, s_wifi.sta_cfg.ssid, WIFI_SSID_LEN);
            de.ssid_len = (uint8_t)safe_strnlen((const char*)s_wifi.sta_cfg.ssid, WIFI_SSID_LEN);
            esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &de, sizeof(de), portMAX_DELAY);
            return;
        }
    }

    /* 阶段 1：Association (关联完成) */
    s_wifi.state = WIFI_SIM_CONNECTED;
    sim_wifi_env_set_state(WIFI_SIM_CONNECTED);
    wifi_event_sta_connected_t ce;
    memset(&ce, 0, sizeof(ce));
    ce.channel = ap->channel;
    ce.authmode = ap->authmode;
    ce.ssid_len = (uint8_t)safe_strnlen((const char*)ap->ssid, WIFI_SSID_LEN);
    memcpy(ce.ssid, ap->ssid, WIFI_SSID_LEN);
    memcpy(ce.bssid, ap->bssid, 6);
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &ce, sizeof(ce), portMAX_DELAY);

    /* 再次校验，防止 STA_CONNECTED 回调中调用了 disconnect / stop */
    if (s_wifi.state == WIFI_SIM_CONNECTED && s_wifi_generation == my_token) {
        /* 阶段 2：DHCP (地址协商完成) */
        s_wifi.state = WIFI_SIM_GOT_IP;
        sim_wifi_env_set_state(WIFI_SIM_GOT_IP);

        esp_netif_ip_info_t ip_info = ap->dhcp_info;
        esp_netif_set_sta_ip_info(&ip_info);
        sim_wifi_env_set_sta_ip(ip_info.ip);

        ip_event_got_ip_t ie;
        memset(&ie, 0, sizeof(ie));
        ie.esp_netif = esp_netif_get_handle_sta();
        ie.ip_info = ip_info;
        ie.ip_changed = true;
        esp_event_post(IP_EVENT, IP_EVENT_STA_GOT_IP, &ie, sizeof(ie), portMAX_DELAY);

        /* 阶段 3：传输层网络级联就绪 */
        sim_network_broker_set_ready(true);
    }
}

esp_err_t esp_wifi_init(const wifi_init_config_t *config) {
    (void)config;
    if (s_wifi.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    memcpy(s_wifi.sta_mac, SIM_STA_MAC, 6);
    memcpy(s_wifi.ap_mac, SIM_AP_MAC, 6);
    s_wifi.state = WIFI_SIM_INIT;
    s_wifi.initialized = true;
    sim_wifi_env_set_state(WIFI_SIM_INIT);
    sim_wifi_env_set_beacon_drop_cb(on_beacon_drop);
    ESP_LOGI("WIFI_SIM", "Wi-Fi initialized in simulation mode");
    return ESP_OK;
}

esp_err_t esp_wifi_set_mode(wifi_mode_t mode) {
    if (mode >= WIFI_MODE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    s_wifi.mode = mode;
    return ESP_OK;
}

esp_err_t esp_wifi_get_mode(wifi_mode_t *mode) {
    if (!mode) {
        return ESP_ERR_INVALID_ARG;
    }
    *mode = s_wifi.mode;
    return ESP_OK;
}

esp_err_t esp_wifi_start(void) {
    if (!s_wifi.initialized || s_wifi.state != WIFI_SIM_INIT) {
        return ESP_ERR_INVALID_STATE;
    }
    s_wifi.state = WIFI_SIM_STARTED;
    sim_wifi_env_set_state(WIFI_SIM_STARTED);

    if (s_wifi.mode == WIFI_MODE_STA || s_wifi.mode == WIFI_MODE_APSTA) {
        esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_START, NULL, 0, portMAX_DELAY);
    }
    if (s_wifi.mode == WIFI_MODE_AP || s_wifi.mode == WIFI_MODE_APSTA) {
        esp_event_post(WIFI_EVENT, WIFI_EVENT_AP_START, NULL, 0, portMAX_DELAY);
    }
    return ESP_OK;
}

esp_err_t esp_wifi_connect(void) {
    if (!s_wifi.initialized || s_wifi.state < WIFI_SIM_STARTED) {
        return ESP_ERR_WIFI_NOT_STARTED;
    }
    if (s_wifi.state == WIFI_SIM_CONNECTING || s_wifi.state >= WIFI_SIM_CONNECTED) {
        return ESP_ERR_WIFI_CONN;
    }
    s_wifi.state = WIFI_SIM_CONNECTING;
    sim_wifi_env_set_state(WIFI_SIM_CONNECTING);

    uint32_t token = ++s_wifi_generation;
    if (s_wifi_work_id != 0) {
        esp_freertos_timer_cancel_work_item(s_wifi_work_id);
        s_wifi_work_id = 0;
    }
    BaseType_t rc = esp_freertos_timer_post_work_item(wifi_connect_work_cb,
                                                      (void*)(uintptr_t)token,
                                                      &s_wifi_work_id,
                                                      pdMS_TO_TICKS(100));
    if (rc != pdPASS) {
        s_wifi.state = WIFI_SIM_STARTED;
        sim_wifi_env_set_state(WIFI_SIM_STARTED);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t esp_wifi_disconnect(void) {
    if (s_wifi.state < WIFI_SIM_CONNECTING) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    s_wifi_generation++;
    if (s_wifi_work_id != 0) {
        esp_freertos_timer_cancel_work_item(s_wifi_work_id);
        s_wifi_work_id = 0;
    }
    s_wifi.state = WIFI_SIM_STARTED;
    sim_wifi_env_set_state(WIFI_SIM_DISCONNECTED);
    sim_network_broker_set_ready(false);

    wifi_event_sta_disconnected_t de;
    memset(&de, 0, sizeof(de));
    de.reason = WIFI_REASON_ASSOC_LEAVE;
    memcpy(de.ssid, s_wifi.sta_cfg.ssid, WIFI_SSID_LEN);
    de.ssid_len = (uint8_t)safe_strnlen((const char*)s_wifi.sta_cfg.ssid, WIFI_SSID_LEN);
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &de, sizeof(de), portMAX_DELAY);
    return ESP_OK;
}

esp_err_t esp_wifi_stop(void) {
    if (s_wifi.state < WIFI_SIM_STARTED) {
        return ESP_ERR_INVALID_STATE;
    }
    s_wifi_generation++;
    if (s_wifi_work_id != 0) {
        esp_freertos_timer_cancel_work_item(s_wifi_work_id);
        s_wifi_work_id = 0;
    }
    s_wifi.state = WIFI_SIM_INIT;
    sim_wifi_env_set_state(WIFI_SIM_INIT);
    sim_network_broker_set_ready(false);

    if (s_wifi.mode == WIFI_MODE_STA || s_wifi.mode == WIFI_MODE_APSTA) {
        esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_STOP, NULL, 0, portMAX_DELAY);
    }
    if (s_wifi.mode == WIFI_MODE_AP || s_wifi.mode == WIFI_MODE_APSTA) {
        esp_event_post(WIFI_EVENT, WIFI_EVENT_AP_STOP, NULL, 0, portMAX_DELAY);
    }
    return ESP_OK;
}

esp_err_t esp_wifi_deinit(void) {
    esp_wifi_sim_reset();
    return ESP_OK;
}

esp_err_t esp_wifi_get_mac(wifi_interface_t ifx, uint8_t mac[6]) {
    if (!mac) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ifx == WIFI_IF_STA) {
        memcpy(mac, s_wifi.sta_mac, 6);
        return ESP_OK;
    } else if (ifx == WIFI_IF_AP) {
        memcpy(mac, s_wifi.ap_mac, 6);
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_wifi_set_mac(wifi_interface_t ifx, const uint8_t mac[6]) {
    if (!mac) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ifx == WIFI_IF_STA) {
        memcpy(s_wifi.sta_mac, mac, 6);
        return ESP_OK;
    } else if (ifx == WIFI_IF_AP) {
        memcpy(s_wifi.ap_mac, mac, 6);
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_wifi_set_config(wifi_interface_t ifx, wifi_config_t *conf) {
    if (!conf) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ifx == WIFI_IF_STA) {
        s_wifi.sta_cfg = conf->sta;
        return ESP_OK;
    } else if (ifx == WIFI_IF_AP) {
        s_wifi.ap_cfg = conf->ap;
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_wifi_get_config(wifi_interface_t ifx, wifi_config_t *conf) {
    if (!conf) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ifx == WIFI_IF_STA) {
        conf->sta = s_wifi.sta_cfg;
        return ESP_OK;
    } else if (ifx == WIFI_IF_AP) {
        conf->ap = s_wifi.ap_cfg;
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_wifi_set_ps(wifi_ps_type_t type) {
    (void)type;
    return ESP_OK;
}

esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *config, bool block) {
    if (!s_wifi.initialized || s_wifi.state < WIFI_SIM_STARTED) {
        return ESP_ERR_WIFI_NOT_STARTED;
    }
    (void)block;
    esp_err_t err = sim_wifi_env_scan(config);
    if (err == ESP_OK) {
        esp_event_post(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, NULL, 0, portMAX_DELAY);
    }
    return err;
}

esp_err_t esp_wifi_scan_stop(void) {
    return ESP_OK;
}

esp_err_t esp_wifi_scan_get_ap_num(uint16_t *number) {
    if (!number) {
        return ESP_ERR_INVALID_ARG;
    }
    *number = sim_wifi_env_get_scan_num();
    return ESP_OK;
}

esp_err_t esp_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records) {
    return sim_wifi_env_get_scan_records(number, ap_records);
}

void esp_wifi_sim_reset(void) {
    s_wifi_generation++;
    if (s_wifi_work_id != 0) {
        esp_freertos_timer_cancel_work_item(s_wifi_work_id);
        s_wifi_work_id = 0;
    }
    memset(&s_wifi, 0, sizeof(s_wifi));
    s_wifi.mode = WIFI_MODE_STA;
    sim_wifi_env_reset();
}

bool esp_wifi_sim_is_connected(void) {
    return s_wifi.state == WIFI_SIM_CONNECTED || s_wifi.state == WIFI_SIM_GOT_IP;
}
