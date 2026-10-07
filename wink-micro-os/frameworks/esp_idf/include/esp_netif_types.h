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
    IP_EVENT_AP_STAIPASSIGNED,
    IP_EVENT_ASSIGNED_IP_TO_CLIENT = IP_EVENT_AP_STAIPASSIGNED,
    IP_EVENT_MAX,
} ip_event_t;

struct esp_netif_obj;
typedef struct esp_netif_obj esp_netif_t;

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

typedef struct {
    esp_ip4_addr_t ip;
    uint8_t mac[6];
    char hostname[32];
} ip_event_assigned_ip_to_client_t;
typedef ip_event_assigned_ip_to_client_t ip_event_ap_staipassigned_t;

typedef struct {
    esp_ip4_addr_t ip;
} esp_netif_dns_info_t;

typedef enum {
    ESP_NETIF_DNS_MAIN = 0,
    ESP_NETIF_DNS_BACKUP,
    ESP_NETIF_DNS_FALLBACK,
    ESP_NETIF_DNS_MAX
} esp_netif_dns_type_t;

typedef enum {
    ESP_NETIF_OP_START = 0,
    ESP_NETIF_OP_SET,
    ESP_NETIF_OP_GET,
    ESP_NETIF_OP_MAX
} esp_netif_dhcp_option_mode_t;

typedef enum {
    ESP_NETIF_SUBNET_MASK = 1,
    ESP_NETIF_ROUTER = 3,
    ESP_NETIF_DOMAIN_NAME_SERVER = 6,
    ESP_NETIF_INTERFACE_MTU = 26,
    ESP_NETIF_BROADCAST_ADDRESS = 28,
    ESP_NETIF_PERMITTED_AGENT_OPTION = 82,
    ESP_NETIF_CAPTIVE_PORTAL = 114,
    ESP_NETIF_VENDOR_SPECIFIC_OPTION = 224,
    ESP_NETIF_IP_ADDRESS_LEASE_TIME = 51,
    ESP_NETIF_IP_REQUEST_RETRY_TIME = 52
} esp_netif_dhcp_option_id_t;

#ifdef __cplusplus
}
#endif
