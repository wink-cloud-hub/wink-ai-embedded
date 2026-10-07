/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once

#include <sys/time.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_event_base.h"
#include "esp_netif_types.h"
#include "esp_netif.h"
#include "esp_sntp.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(NETIF_SNTP_EVENT);

typedef enum {
    NETIF_SNTP_TIME_SYNC = 0,
} esp_netif_sntp_event_t;

typedef struct esp_netif_sntp_time_sync {
    struct timeval tv;
} esp_netif_sntp_time_sync_t;

typedef void (*esp_sntp_time_cb_t)(struct timeval *tv);

#define ESP_SNTP_SERVER_LIST(...)   { __VA_ARGS__ }

#define ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(servers_in_list, list_of_servers)   {   \
            .smooth_sync = false,                       \
            .server_from_dhcp = false,                  \
            .wait_for_sync = true,                      \
            .start = true,                              \
            .sync_cb = NULL,                            \
            .renew_servers_after_new_IP = false,        \
            .ip_event_to_renew = IP_EVENT_STA_GOT_IP,   \
            .index_of_first_server = 0,                 \
            .num_of_servers = (servers_in_list),        \
            .servers = list_of_servers,                 \
}

#define ESP_NETIF_SNTP_DEFAULT_CONFIG(server) \
            ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(1, {server})

typedef struct esp_sntp_config {
    bool smooth_sync;
    bool server_from_dhcp;
    bool wait_for_sync;
    bool start;
    esp_sntp_time_cb_t sync_cb;
    bool renew_servers_after_new_IP;
    ip_event_t ip_event_to_renew;
    size_t index_of_first_server;
    size_t num_of_servers;
    const char* servers[CONFIG_LWIP_SNTP_MAX_SERVERS];
} esp_sntp_config_t;

esp_err_t esp_netif_sntp_init(const esp_sntp_config_t * config);
esp_err_t esp_netif_sntp_start(void);
void esp_netif_sntp_deinit(void);
esp_err_t esp_netif_sntp_sync_wait(TickType_t tout);
esp_err_t esp_netif_sntp_reachability(unsigned int index, unsigned int *reachability);

#ifdef __cplusplus
}
#endif
