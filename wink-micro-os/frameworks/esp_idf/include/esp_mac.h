/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_MAC_H
#define WINK_H_GUARD_ESP_MAC_H
#ifndef __WINK_HARVESTED_ESP_MAC_H__
#define __WINK_HARVESTED_ESP_MAC_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef FOUR_UNIVERSAL_MAC_ADDR
#define FOUR_UNIVERSAL_MAC_ADDR 4
#endif
#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#endif
#ifndef MACSTR
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
#ifndef ONE_UNIVERSAL_MAC_ADDR
#define ONE_UNIVERSAL_MAC_ADDR 1
#endif
#ifndef TWO_UNIVERSAL_MAC_ADDR
#define TWO_UNIVERSAL_MAC_ADDR 2
#endif
#ifndef UNIVERSAL_MAC_ADDR_NUM
#define UNIVERSAL_MAC_ADDR_NUM CONFIG_ESP_MAC_UNIVERSAL_MAC_ADDRESSES
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    ESP_MAC_WIFI_STA = 0,
    ESP_MAC_WIFI_SOFTAP = 1,
    ESP_MAC_BT = 2,
    ESP_MAC_ETH = 3,
    ESP_MAC_IEEE802154 = 4,
    ESP_MAC_BASE = 5,
    ESP_MAC_EFUSE_FACTORY = 6,
    ESP_MAC_EFUSE_CUSTOM = 7,
    ESP_MAC_EFUSE_EXT = 8,
} esp_mac_type_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
esp_err_t esp_base_mac_addr_get(uint8_t *mac) WINK_SLA_ERROR("Wink SLA Violation: esp_base_mac_addr_get out of Core 8 scope.");
#else
esp_err_t esp_base_mac_addr_get(uint8_t *mac);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_base_mac_addr_set(const uint8_t *mac) WINK_SLA_ERROR("Wink SLA Violation: esp_base_mac_addr_set out of Core 8 scope.");
#else
esp_err_t esp_base_mac_addr_set(const uint8_t *mac);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_derive_local_mac(uint8_t *local_mac, const uint8_t *universal_mac) WINK_SLA_ERROR("Wink SLA Violation: esp_derive_local_mac out of Core 8 scope.");
#else
esp_err_t esp_derive_local_mac(uint8_t *local_mac, const uint8_t *universal_mac);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_efuse_mac_get_custom(uint8_t *mac) WINK_SLA_ERROR("Wink SLA Violation: esp_efuse_mac_get_custom out of Core 8 scope.");
#else
esp_err_t esp_efuse_mac_get_custom(uint8_t *mac);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_efuse_mac_get_default(uint8_t *mac) WINK_SLA_ERROR("Wink SLA Violation: esp_efuse_mac_get_default out of Core 8 scope.");
#else
esp_err_t esp_efuse_mac_get_default(uint8_t *mac);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_iface_mac_addr_set(const uint8_t *mac, esp_mac_type_t type) WINK_SLA_ERROR("Wink SLA Violation: esp_iface_mac_addr_set out of Core 8 scope.");
#else
esp_err_t esp_iface_mac_addr_set(const uint8_t *mac, esp_mac_type_t type);
#endif

#if defined(__WINK_SIM__)
size_t esp_mac_addr_len_get(esp_mac_type_t type) WINK_SLA_ERROR("Wink SLA Violation: esp_mac_addr_len_get out of Core 8 scope.");
#else
size_t esp_mac_addr_len_get(esp_mac_type_t type);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type) WINK_SLA_ERROR("Wink SLA Violation: esp_read_mac out of Core 8 scope.");
#else
esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_MAC_H__ */
#endif /* WINK_H_GUARD_ESP_MAC_H */
