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
    WIFI_AUTH_ENTERPRISE,
    WIFI_AUTH_WPA2_ENTERPRISE = WIFI_AUTH_ENTERPRISE,
    WIFI_AUTH_WPA3_PSK,
    WIFI_AUTH_WPA2_WPA3_PSK,
    WIFI_AUTH_WAPI_PSK,
    WIFI_AUTH_OWE,
    WIFI_AUTH_WPA3_ENT_192,
    WIFI_AUTH_DUMMY_1,
    WIFI_AUTH_DUMMY_2,
    WIFI_AUTH_DPP,
    WIFI_AUTH_WPA3_ENTERPRISE,
    WIFI_AUTH_WPA2_WPA3_ENTERPRISE,
    WIFI_AUTH_WPA_ENTERPRISE,
    WIFI_AUTH_UNKNOWN,
    WIFI_AUTH_MAX
} wifi_auth_mode_t;

typedef enum {
    WPA3_SAE_PWE_UNSPECIFIED = 0,
    WPA3_SAE_PWE_HUNT_AND_PECK = 1,
    WPA3_SAE_PWE_HASH_TO_ELEMENT = 2,
    WPA3_SAE_PWE_BOTH = 3,
} wifi_sae_pwe_method_t;

typedef struct {
    bool capable;
    bool required;
} wifi_pmf_config_t;

typedef struct {
    uint16_t period;
    bool protected_keep_alive;
} wifi_bss_max_idle_config_t;

typedef enum {
    WIFI_IF_STA = 0,
    WIFI_IF_AP,
    WIFI_IF_MAX
} wifi_interface_t;

typedef enum {
    WIFI_PS_NONE = 0,
    WIFI_PS_MIN_MODEM
} wifi_ps_type_t;

typedef enum {
    WIFI_FAST_SCAN = 0,
    WIFI_ALL_CHANNEL_SCAN,
} wifi_scan_method_t;

typedef enum {
    WIFI_CONNECT_AP_BY_SIGNAL = 0,
    WIFI_CONNECT_AP_BY_SECURITY,
} wifi_sort_method_t;

typedef struct {
    int8_t           rssi;
    wifi_auth_mode_t authmode;
    uint8_t          rssi_5g_adjustment;
} wifi_scan_threshold_t;

/* 乐鑫官方语料兼容的嵌套 threshold 结构体与 SAE 配置（零修改编译契约）*/
typedef struct {
    uint8_t               ssid[WIFI_SSID_LEN];
    uint8_t               password[WIFI_PASS_LEN];
    wifi_scan_method_t    scan_method;
    bool                  bssid_set;
    uint8_t               bssid[6];
    uint8_t               channel;
    wifi_sort_method_t    sort_method;
    wifi_scan_threshold_t threshold;
    uint8_t               sae_pwe_h2e;
    uint8_t               sae_h2e_identifier[32];
} wifi_sta_config_t;

typedef struct {
    uint8_t ssid[WIFI_SSID_LEN];
    uint8_t password[WIFI_PASS_LEN];
    uint8_t ssid_len;
    uint8_t channel;
    wifi_auth_mode_t authmode;
    uint8_t ssid_hidden;
    uint8_t max_connection;
    uint16_t beacon_interval;
    uint8_t sae_pwe_h2e;
    wifi_pmf_config_t pmf_cfg;
    wifi_bss_max_idle_config_t bss_max_idle_cfg;
    uint16_t gtk_rekey_interval;
} wifi_ap_config_t;

typedef union {
    wifi_sta_config_t sta;
    wifi_ap_config_t  ap;
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

typedef enum {
    WIFI_REASON_UNSPECIFIED              = 1,
    WIFI_REASON_AUTH_EXPIRE              = 2,
    WIFI_REASON_AUTH_LEAVE               = 3,
    WIFI_REASON_ASSOC_EXPIRE             = 4,
    WIFI_REASON_ASSOC_TOOMANY            = 5,
    WIFI_REASON_NOT_AUTHED               = 6,
    WIFI_REASON_NOT_ASSOCED              = 7,
    WIFI_REASON_ASSOC_LEAVE              = 8,
    WIFI_REASON_ASSOC_NOT_AUTHED         = 9,
    WIFI_REASON_DISASSOC_PWRCAP_BAD      = 10,
    WIFI_REASON_DISASSOC_SUPCHAN_BAD     = 11,
    WIFI_REASON_IE_INVALID               = 13,
    WIFI_REASON_MIC_FAILURE              = 14,
    WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT   = 15,
    WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT = 16,
    WIFI_REASON_IE_IN_4WAY_DIFFERS       = 17,
    WIFI_REASON_GROUP_CIPHER_INVALID     = 18,
    WIFI_REASON_PAIRWISE_CIPHER_INVALID  = 19,
    WIFI_REASON_AKMP_INVALID             = 20,
    WIFI_REASON_UNSUPP_RSN_IE_VERSION    = 21,
    WIFI_REASON_INVALID_RSN_IE_CAP       = 22,
    WIFI_REASON_802_1X_AUTH_FAILED       = 23,
    WIFI_REASON_CIPHER_SUITE_REJECTED    = 24,
    WIFI_REASON_INVALID_PMKID            = 53,
    WIFI_REASON_BEACON_TIMEOUT           = 200,
    WIFI_REASON_NO_AP_FOUND              = 201,
    WIFI_REASON_AUTH_FAIL                = 202,
    WIFI_REASON_ASSOC_FAIL               = 203,
    WIFI_REASON_HANDSHAKE_TIMEOUT        = 204,
    WIFI_REASON_CONNECTION_FAIL          = 205,
} wifi_err_reason_t;

typedef struct {
    uint8_t ssid[WIFI_SSID_LEN];
    uint8_t ssid_len;
    uint8_t bssid[6];
    uint8_t reason;
    int8_t  rssi;
} wifi_event_sta_disconnected_t;

typedef struct {
    uint8_t mac[6];
    uint8_t aid;
    bool is_mesh_child;
} wifi_event_ap_staconnected_t;

typedef struct {
    uint8_t mac[6];
    uint8_t aid;
    bool is_mesh_child;
    uint8_t reason;
} wifi_event_ap_stadisconnected_t;

typedef enum {
    WIFI_CIPHER_TYPE_NONE = 0,
    WIFI_CIPHER_TYPE_WEP40,
    WIFI_CIPHER_TYPE_WEP104,
    WIFI_CIPHER_TYPE_TKIP,
    WIFI_CIPHER_TYPE_CCMP,
    WIFI_CIPHER_TYPE_TKIP_CCMP,
    WIFI_CIPHER_TYPE_AES_CMAC128,
    WIFI_CIPHER_TYPE_SMS4,
    WIFI_CIPHER_TYPE_GCMP,
    WIFI_CIPHER_TYPE_GCMP256,
    WIFI_CIPHER_TYPE_AES_GMAC128,
    WIFI_CIPHER_TYPE_AES_GMAC256,
    WIFI_CIPHER_TYPE_UNKNOWN,
} wifi_cipher_type_t;

typedef enum {
    WIFI_SECOND_CHAN_NONE = 0,
    WIFI_SECOND_CHAN_ABOVE,
    WIFI_SECOND_CHAN_BELOW,
} wifi_second_chan_t;

typedef enum {
    WIFI_ANT_ANT0 = 0,
    WIFI_ANT_ANT1 = 1,
    WIFI_ANT_MAX,
} wifi_ant_t;

typedef struct {
    char cc[3];
    uint8_t schan;
    uint8_t nchan;
    int8_t max_tx_power;
    uint8_t policy;
} wifi_country_t;

typedef struct {
    uint8_t bssid[6];
    uint8_t ssid[33];
    uint8_t primary;
    wifi_second_chan_t second;
    int8_t  rssi;
    wifi_auth_mode_t authmode;
    wifi_cipher_type_t pairwise_cipher;
    wifi_cipher_type_t group_cipher;
    wifi_ant_t ant;
    uint32_t phy_11b:1;
    uint32_t phy_11g:1;
    uint32_t phy_11n:1;
    uint32_t phy_lr:1;
    uint32_t wps:1;
    uint32_t ftm_responder:1;
    uint32_t ftm_initiator:1;
    uint32_t akm_dpp:1;
    uint32_t reserved:24;
    wifi_country_t country;
} wifi_ap_record_t;

typedef struct {
    uint16_t ghz_2_channels;
    uint32_t ghz_5_channels;
} wifi_scan_channel_bitmap_t;

typedef struct {
    const uint8_t *ssid;
    const uint8_t *bssid;
    uint8_t channel;
    uint8_t show_hidden;
    uint8_t scan_type;
    wifi_scan_channel_bitmap_t channel_bitmap;
} wifi_scan_config_t;

#ifdef __cplusplus
}
#endif
