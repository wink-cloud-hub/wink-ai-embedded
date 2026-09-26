/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_SSID_LEN  32
#define WIFI_PASS_LEN  64

/* 错误码补全（基于 ESP_ERR_WIFI_BASE = 0x3000）*/
#ifndef ESP_ERR_WIFI_NOT_INIT
#  define ESP_ERR_WIFI_NOT_INIT       (ESP_ERR_WIFI_BASE + 1)
#  define ESP_ERR_WIFI_NOT_STARTED    (ESP_ERR_WIFI_BASE + 2)
#  define ESP_ERR_WIFI_NOT_STOPPED    (ESP_ERR_WIFI_BASE + 3)
#  define ESP_ERR_WIFI_CONN           (ESP_ERR_WIFI_BASE + 7)
#  define ESP_ERR_WIFI_NOT_CONNECT    (ESP_ERR_WIFI_BASE + 15)
#endif

typedef enum {
    WIFI_MODE_NULL = 0,
    WIFI_MODE_STA,
    WIFI_MODE_AP,
    WIFI_MODE_APSTA,
    WIFI_MODE_MAX
} wifi_mode_t;

typedef enum {
    WIFI_AUTH_OPEN = 0,
    WIFI_AUTH_WEP,
    WIFI_AUTH_WPA_PSK,
    WIFI_AUTH_WPA2_PSK,
    WIFI_AUTH_WPA_WPA2_PSK,
    WIFI_AUTH_WPA2_ENTERPRISE,
    WIFI_AUTH_WPA3_PSK,
    WIFI_AUTH_MAX
} wifi_auth_mode_t;

typedef enum {
    WPA3_SAE_PWE_UNSPECIFIED = 0,
    WPA3_SAE_PWE_HUNT_AND_PECK = 1,
    WPA3_SAE_PWE_HASH_TO_ELEMENT = 2,
    WPA3_SAE_PWE_BOTH = 3,
} wifi_sae_pwe_method_t;

typedef enum {
    WIFI_IF_STA = 0,
    WIFI_IF_AP,
    WIFI_IF_MAX
} wifi_interface_t;

typedef enum {
    WIFI_PS_NONE = 0,
    WIFI_PS_MIN_MODEM
} wifi_ps_type_t;

/* 乐鑫官方语料兼容的嵌套 threshold 结构体与 SAE 配置（零修改编译契约）*/
typedef struct {
    uint8_t ssid[WIFI_SSID_LEN];
    uint8_t password[WIFI_PASS_LEN];
    bool    bssid_set;
    uint8_t bssid[6];
    uint8_t channel;
    struct {
        wifi_auth_mode_t authmode;
    } threshold;
    uint8_t sae_pwe_h2e;
    uint8_t sae_h2e_identifier[32];
} wifi_sta_config_t;

typedef union {
    wifi_sta_config_t sta;
} wifi_config_t;

typedef struct {
    int twt_enabled;
} wifi_init_config_t;

#define WIFI_INIT_CONFIG_DEFAULT() { .twt_enabled = 0 }

typedef struct {
    uint8_t ssid[WIFI_SSID_LEN];
    uint8_t ssid_len;
    uint8_t bssid[6];
    uint8_t channel;
    wifi_auth_mode_t authmode;
} wifi_event_sta_connected_t;

#ifdef __cplusplus
}
#endif
