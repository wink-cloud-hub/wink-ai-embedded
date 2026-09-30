/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "esp_netif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#  define WINK_SIM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#  define WINK_SIM_EXPORT
#endif

#define SIM_WIFI_MAX_APS 8

typedef struct {
    bool in_use;
    char ssid[33];
    uint8_t bssid[6];
    int8_t rssi;
    uint8_t channel;
    wifi_auth_mode_t authmode;
    char password[65];
    esp_netif_ip_info_t dhcp_info;
    bool drop_beacon;
    uint8_t fault_reason;
} sim_wifi_ap_t;

WINK_SIM_EXPORT void sim_wifi_env_reset(void);

WINK_SIM_EXPORT int sim_wifi_env_inject_ap(const char *json_str);
WINK_SIM_EXPORT int sim_wifi_env_inject_fault(const char *json_str);

const sim_wifi_ap_t* sim_wifi_env_find_ap_by_ssid(const char *ssid);

WINK_SIM_EXPORT esp_err_t sim_wifi_env_scan(const wifi_scan_config_t *config);
WINK_SIM_EXPORT uint16_t  sim_wifi_env_get_scan_num(void);
WINK_SIM_EXPORT esp_err_t sim_wifi_env_get_scan_records(uint16_t *number, wifi_ap_record_t *ap_records);

WINK_SIM_EXPORT const char* sim_wifi_env_get_state_str(void);
void sim_wifi_env_set_state(int state);

WINK_SIM_EXPORT void sim_wifi_env_get_sta_ip_str(char *buf, size_t maxlen);
WINK_SIM_EXPORT const char* sim_wifi_env_get_sta_ip_ptr(void);
void sim_wifi_env_set_sta_ip(esp_ip4_addr_t ip);

typedef void (*sim_wifi_beacon_drop_cb_t)(const char *ssid, uint8_t reason);
void sim_wifi_env_set_beacon_drop_cb(sim_wifi_beacon_drop_cb_t cb);

#ifdef __cplusplus
}
#endif
