/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include <string.h>
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_netif.h"

static inline esp_err_t example_connect(void) {
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    memcpy(wifi_config.sta.ssid, "myssid", 6);
    memcpy(wifi_config.sta.password, "mypassword", 10);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    return esp_wifi_connect();
}

static inline esp_err_t example_disconnect(void) {
    return esp_wifi_disconnect();
}
