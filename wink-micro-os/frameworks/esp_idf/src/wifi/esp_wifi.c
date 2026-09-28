/* SPDX-License-Identifier: LGPL-3.0-only */
/* src/wifi/esp_wifi.c — 健壮性加固版 */
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

ESP_EVENT_DEFINE_BASE(WIFI_EVENT);
ESP_EVENT_DEFINE_BASE(IP_EVENT);

static const uint8_t SIM_STA_MAC[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};

typedef enum {
    WIFI_SIM_OFF = 0,
    WIFI_SIM_INIT,
    WIFI_SIM_STARTED,
    WIFI_SIM_CONNECTING,
    WIFI_SIM_CONNECTED,
    WIFI_SIM_GOT_IP
} wifi_sim_state_t;

typedef struct {
    wifi_sim_state_t state;
    wifi_mode_t mode;
    wifi_sta_config_t sta_cfg;
    uint8_t sta_mac[6];
    bool initialized;
} esp_wifi_sim_t;

static esp_wifi_sim_t s_wifi = {.state = WIFI_SIM_OFF};
static uint32_t s_connect_token = 0;
static TaskHandle_t s_conn_task_handle = NULL;

static inline size_t safe_strnlen(const char *s, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && s[len] != '\0') {
        len++;
    }
    return len;
}

static void wifi_connect_task(void *arg) {
    uint32_t my_token = (uint32_t)(uintptr_t)arg;
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 临界安全检查：若中途断开/停止/重置或 token 不匹配，安全退出 */
    if (!s_wifi.initialized || s_wifi.state != WIFI_SIM_CONNECTING || s_connect_token != my_token) {
        s_conn_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    s_wifi.state = WIFI_SIM_CONNECTED;
    wifi_event_sta_connected_t ce = {
        .channel = 6,
        .authmode = WIFI_AUTH_WPA2_PSK,
        .ssid_len = (uint8_t)safe_strnlen((const char*)s_wifi.sta_cfg.ssid, WIFI_SSID_LEN)
    };
    memcpy(ce.ssid, s_wifi.sta_cfg.ssid, WIFI_SSID_LEN);
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &ce, sizeof(ce), portMAX_DELAY);

    /* 再次校验，防止 STA_CONNECTED 回调中调用了 disconnect */
    if (s_wifi.state == WIFI_SIM_CONNECTED && s_connect_token == my_token) {
        s_wifi.state = WIFI_SIM_GOT_IP;
        ip_event_got_ip_t ie = {
            .esp_netif = NULL,
            .ip_info = {
                .ip = {.addr = ESP_IP4TOADDR(192, 168, 4, 2)},
                .netmask = {.addr = ESP_IP4TOADDR(255, 255, 255, 0)},
                .gw = {.addr = ESP_IP4TOADDR(192, 168, 4, 1)},
            },
            .ip_changed = true,
        };
        esp_event_post(IP_EVENT, IP_EVENT_STA_GOT_IP, &ie, sizeof(ie), portMAX_DELAY);
    }

    s_conn_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t esp_wifi_init(const wifi_init_config_t *config) {
    (void)config;
    if (s_wifi.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    memcpy(s_wifi.sta_mac, SIM_STA_MAC, 6);
    s_wifi.state = WIFI_SIM_INIT;
    s_wifi.initialized = true;
    ESP_LOGI("WIFI_SIM", "Wi-Fi initialized in simulation mode");
    return ESP_OK;
}

esp_err_t esp_wifi_set_mode(wifi_mode_t mode) {
    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        ESP_LOGE("WIFI_SIM", "AP mode not supported in simulation (ADR-0012)");
        return ESP_ERR_NOT_SUPPORTED;
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
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_START, NULL, 0, portMAX_DELAY);
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
    uint32_t token = ++s_connect_token;
    BaseType_t rc = xTaskCreate(wifi_connect_task, "wifi_conn", 32768,
                                (void*)(uintptr_t)token, 1, &s_conn_task_handle);
    if (rc != pdPASS) {
        s_wifi.state = WIFI_SIM_STARTED;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t esp_wifi_disconnect(void) {
    if (s_wifi.state < WIFI_SIM_CONNECTING) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    s_connect_token++; /* 废弃未完成的连接任务 */
    s_wifi.state = WIFI_SIM_STARTED;
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL, 0, portMAX_DELAY);
    return ESP_OK;
}

esp_err_t esp_wifi_stop(void) {
    if (s_wifi.state < WIFI_SIM_STARTED) {
        return ESP_ERR_INVALID_STATE;
    }
    s_connect_token++; /* 废弃连接中任务 */
    s_wifi.state = WIFI_SIM_INIT;
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_STOP, NULL, 0, portMAX_DELAY);
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
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_wifi_set_config(wifi_interface_t ifx, wifi_config_t *conf) {
    if (ifx == WIFI_IF_STA && conf) {
        s_wifi.sta_cfg = conf->sta;
    }
    return ESP_OK;
}

esp_err_t esp_wifi_get_config(wifi_interface_t ifx, wifi_config_t *conf) {
    if (!conf) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ifx == WIFI_IF_STA) {
        conf->sta = s_wifi.sta_cfg;
    }
    return ESP_OK;
}

esp_err_t esp_wifi_set_ps(wifi_ps_type_t type) {
    (void)type;
    return ESP_OK;
}

esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *c, bool b) {
    (void)c;
    (void)b;
    ESP_LOGE("WIFI_SIM", "esp_wifi_scan_start: not supported (ADR-0012)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_wifi_scan_stop(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_wifi_scan_get_ap_records(uint16_t *n, void *r) {
    (void)r;
    if (n) {
        *n = 0;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

void esp_wifi_sim_reset(void) {
    s_connect_token++;
    memset(&s_wifi, 0, sizeof(s_wifi));
    s_conn_task_handle = NULL;
}

bool esp_wifi_sim_is_connected(void) {
    return s_wifi.state == WIFI_SIM_CONNECTED || s_wifi.state == WIFI_SIM_GOT_IP;
}
