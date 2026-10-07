/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include "esp_err.h"
#include "lwip/ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_LWIP_SNTP_MAX_SERVERS
#define CONFIG_LWIP_SNTP_MAX_SERVERS 3
#endif

#define SNTP_MAX_SERVERS CONFIG_LWIP_SNTP_MAX_SERVERS

typedef enum {
    SNTP_SYNC_MODE_IMMED,
    SNTP_SYNC_MODE_SMOOTH,
} sntp_sync_mode_t;

typedef enum {
    SNTP_SYNC_STATUS_RESET,
    SNTP_SYNC_STATUS_COMPLETED,
    SNTP_SYNC_STATUS_IN_PROGRESS,
} sntp_sync_status_t;

typedef enum {
    ESP_SNTP_OPMODE_POLL,
    ESP_SNTP_OPMODE_LISTENONLY,
} esp_sntp_operatingmode_t;

typedef void (*sntp_sync_time_cb_t)(struct timeval *tv);

void sntp_sync_time(struct timeval *tv);
void sntp_set_sync_mode(sntp_sync_mode_t sync_mode);
sntp_sync_mode_t sntp_get_sync_mode(void);
sntp_sync_status_t sntp_get_sync_status(void);
void sntp_set_sync_status(sntp_sync_status_t sync_status);
void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t callback);
void sntp_set_sync_interval(uint32_t interval_ms);
uint32_t sntp_get_sync_interval(void);
bool sntp_restart(void);
void esp_sntp_setoperatingmode(esp_sntp_operatingmode_t operating_mode);
void esp_sntp_init(void);
void esp_sntp_stop(void);
void esp_sntp_setserver(uint8_t idx, const ip_addr_t *addr);
void esp_sntp_setservername(uint8_t idx, const char *server);
const char *esp_sntp_getservername(uint8_t idx);
const ip_addr_t* esp_sntp_getserver(uint8_t idx);
bool esp_sntp_enabled(void);
uint8_t esp_sntp_getreachability(uint8_t idx);
esp_sntp_operatingmode_t esp_sntp_getoperatingmode(void);
void esp_sntp_servermode_dhcp(bool enable);

#define esp_sntp_sync_time sntp_sync_time
#define esp_sntp_set_sync_mode sntp_set_sync_mode
#define esp_sntp_get_sync_mode sntp_get_sync_mode
#define esp_sntp_get_sync_status sntp_get_sync_status
#define esp_sntp_set_sync_status sntp_set_sync_status
#define esp_sntp_set_time_sync_notification_cb sntp_set_time_sync_notification_cb
#define esp_sntp_set_sync_interval sntp_set_sync_interval
#define esp_sntp_get_sync_interval sntp_get_sync_interval
#define esp_sntp_restart sntp_restart

#ifdef __cplusplus
}
#endif
