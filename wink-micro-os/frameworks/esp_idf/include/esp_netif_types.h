/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event_base.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(IP_EVENT);

typedef enum {
    IP_EVENT_STA_GOT_IP = 0,
    IP_EVENT_STA_LOST_IP,
    IP_EVENT_MAX,
} ip_event_t;

typedef struct esp_netif_obj* esp_netif_t;

/* 纯正 32-bit IPv4 地址结构体，兼顾 .addr 访问与指针输出 */
typedef struct {
    uint32_t addr;
} esp_ip4_addr_t;

#define ESP_IP4TOADDR(a,b,c,d) \
    (((uint32_t)((a) & 0xff)) | ((uint32_t)((b) & 0xff) << 8) | \
     ((uint32_t)((c) & 0xff) << 16) | ((uint32_t)((d) & 0xff) << 24))

#define IPSTR "%d.%d.%d.%d"
#define IP2STR(ipaddr) ((const uint8_t*)&((ipaddr)->addr))[0], \
                       ((const uint8_t*)&((ipaddr)->addr))[1], \
                       ((const uint8_t*)&((ipaddr)->addr))[2], \
                       ((const uint8_t*)&((ipaddr)->addr))[3]

typedef struct {
    esp_ip4_addr_t ip;
    esp_ip4_addr_t netmask;
    esp_ip4_addr_t gw;
} esp_netif_ip_info_t;

typedef struct {
    esp_netif_t *esp_netif;
    esp_netif_ip_info_t ip_info;
    bool ip_changed;
} ip_event_got_ip_t;

#ifdef __cplusplus
}
#endif
