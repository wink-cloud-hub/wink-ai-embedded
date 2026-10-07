/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

ESP_EVENT_DEFINE_BASE(NETIF_SNTP_EVENT);

static bool s_sntp_enabled = false;
static sntp_sync_mode_t s_sync_mode = SNTP_SYNC_MODE_IMMED;
static sntp_sync_status_t s_sync_status = SNTP_SYNC_STATUS_RESET;
static sntp_sync_time_cb_t s_notification_cb = NULL;
static esp_sntp_operatingmode_t s_opmode = ESP_SNTP_OPMODE_POLL;
static uint32_t s_sync_interval_ms = 15000;

static char s_server_names[CONFIG_LWIP_SNTP_MAX_SERVERS][64];
static ip_addr_t s_server_ips[CONFIG_LWIP_SNTP_MAX_SERVERS];
static bool s_has_server_ip[CONFIG_LWIP_SNTP_MAX_SERVERS];

static bool s_netif_inited = false;
static bool s_has_synced = false;

/* Virtual RTC timekeeper for deterministic simulation */
static struct timeval s_virtual_time = {0, 0};
static int64_t s_sync_timer_us = 0;
static bool s_time_set = false;

int settimeofday(const struct timeval *tv, const struct timezone *tz) {
    (void)tz;
    if (tv) {
        s_virtual_time = *tv;
        s_sync_timer_us = esp_timer_get_time();
        s_time_set = true;
    }
    return 0;
}

int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    if (tv) {
        int64_t now_us = esp_timer_get_time();
        if (!s_time_set) {
            tv->tv_sec = now_us / 1000000LL;
            tv->tv_usec = (suseconds_t)(now_us % 1000000LL);
        } else {
            int64_t elapsed_us = now_us - s_sync_timer_us;
            int64_t total_us = (int64_t)s_virtual_time.tv_sec * 1000000LL + s_virtual_time.tv_usec + elapsed_us;
            tv->tv_sec = total_us / 1000000LL;
            tv->tv_usec = (suseconds_t)(total_us % 1000000LL);
        }
    }
    return 0;
}

time_t time(time_t *tloc) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (tloc) {
        *tloc = tv.tv_sec;
    }
    return tv.tv_sec;
}

/* Weak symbol for sntp_sync_time so app can override if needed */
__attribute__((weak)) void sntp_sync_time(struct timeval *tv) {
    if (tv) {
        settimeofday(tv, NULL);
    }
    s_sync_status = SNTP_SYNC_STATUS_COMPLETED;
}

void sntp_set_sync_mode(sntp_sync_mode_t sync_mode) {
    s_sync_mode = sync_mode;
}

sntp_sync_mode_t sntp_get_sync_mode(void) {
    return s_sync_mode;
}

sntp_sync_status_t sntp_get_sync_status(void) {
    return s_sync_status;
}

void sntp_set_sync_status(sntp_sync_status_t sync_status) {
    s_sync_status = sync_status;
}

void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t callback) {
    s_notification_cb = callback;
}

void sntp_set_sync_interval(uint32_t interval_ms) {
    s_sync_interval_ms = (interval_ms < 15000) ? 15000 : interval_ms;
}

uint32_t sntp_get_sync_interval(void) {
    return s_sync_interval_ms;
}

bool sntp_restart(void) {
    return s_sntp_enabled;
}

void esp_sntp_setoperatingmode(esp_sntp_operatingmode_t operating_mode) {
    s_opmode = operating_mode;
}

esp_sntp_operatingmode_t esp_sntp_getoperatingmode(void) {
    return s_opmode;
}

void esp_sntp_servermode_dhcp(bool enable) {
    (void)enable;
}

void esp_sntp_init(void) {
    s_sntp_enabled = true;
    s_sync_status = SNTP_SYNC_STATUS_RESET;
}

void esp_sntp_stop(void) {
    s_sntp_enabled = false;
    s_sync_status = SNTP_SYNC_STATUS_RESET;
}

bool esp_sntp_enabled(void) {
    return s_sntp_enabled;
}

void esp_sntp_setserver(uint8_t idx, const ip_addr_t *addr) {
    if (idx >= CONFIG_LWIP_SNTP_MAX_SERVERS || !addr) return;
    s_server_ips[idx] = *addr;
    s_has_server_ip[idx] = true;
    s_server_names[idx][0] = '\0';
}

void esp_sntp_setservername(uint8_t idx, const char *server) {
    if (idx >= CONFIG_LWIP_SNTP_MAX_SERVERS || !server) return;
    strncpy(s_server_names[idx], server, sizeof(s_server_names[idx]) - 1);
    s_server_names[idx][sizeof(s_server_names[idx]) - 1] = '\0';
    s_has_server_ip[idx] = false;
}

const char *esp_sntp_getservername(uint8_t idx) {
    if (idx >= CONFIG_LWIP_SNTP_MAX_SERVERS) return NULL;
    if (s_server_names[idx][0] != '\0') return s_server_names[idx];
    return NULL;
}

const ip_addr_t* esp_sntp_getserver(uint8_t idx) {
    if (idx >= CONFIG_LWIP_SNTP_MAX_SERVERS) return NULL;
    if (s_has_server_ip[idx]) return &s_server_ips[idx];
    return NULL;
}

uint8_t esp_sntp_getreachability(uint8_t idx) {
    (void)idx;
    return 0xFF;
}

esp_err_t esp_netif_sntp_reachability(unsigned int index, unsigned int *reachability) {
    if (!reachability) return ESP_ERR_INVALID_ARG;
    if (!s_netif_inited) return ESP_ERR_INVALID_STATE;
    *reachability = esp_sntp_getreachability((uint8_t)index);
    return ESP_OK;
}

esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config) {
    if (!config) return ESP_ERR_INVALID_ARG;
    s_netif_inited = true;
    s_has_synced = false;
    s_sync_status = SNTP_SYNC_STATUS_RESET;

    if (config->smooth_sync) {
        sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    } else {
        sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    }
    if (config->sync_cb) {
        sntp_set_time_sync_notification_cb(config->sync_cb);
    }

    /* Configure servers */
    for (size_t i = 0; i < config->num_of_servers && i < CONFIG_LWIP_SNTP_MAX_SERVERS; ++i) {
        if (config->servers[i]) {
            esp_sntp_setservername((uint8_t)i, config->servers[i]);
        }
    }

    if (config->start) {
        return esp_netif_sntp_start();
    }
    return ESP_OK;
}

esp_err_t esp_netif_sntp_start(void) {
    if (!s_netif_inited) return ESP_ERR_INVALID_STATE;
    esp_sntp_init();
    return ESP_OK;
}

void esp_netif_sntp_deinit(void) {
    esp_sntp_stop();
    s_netif_inited = false;
    s_has_synced = false;
}

/* POSIX adjtime support in simulation */
int adjtime(const struct timeval *delta, struct timeval *olddelta) {
    if (olddelta) {
        olddelta->tv_sec = 0;
        olddelta->tv_usec = 0;
    }
    (void)delta;
    return 0;
}

static void trigger_sntp_sync(void) {
    /* 2026-10-07 10:15:30 UTC = 1791368130 */
    struct timeval tv = {
        .tv_sec = 1791368130,
        .tv_usec = 123456,
    };

    sntp_sync_time(&tv);

    if (s_notification_cb) {
        s_notification_cb(&tv);
    }

    esp_netif_sntp_time_sync_t evt = {
        .tv = tv,
    };
    (void)esp_event_post(NETIF_SNTP_EVENT, NETIF_SNTP_TIME_SYNC, &evt, sizeof(evt), 100 / portTICK_PERIOD_MS);
    (void)esp_event_loop_run_all_pending();
    vTaskDelay(50 / portTICK_PERIOD_MS);

    s_has_synced = true;
    s_sync_status = SNTP_SYNC_STATUS_COMPLETED;
}

esp_err_t esp_netif_sntp_sync_wait(TickType_t tout) {
    if (!s_sntp_enabled && !s_netif_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_has_synced) {
        return ESP_OK;
    }

    /* Simulate network delay: yield context for 50ms or tout */
    TickType_t delay = (tout < 50 / portTICK_PERIOD_MS) ? tout : (50 / portTICK_PERIOD_MS);
    if (delay > 0) {
        vTaskDelay(delay);
    }

    trigger_sntp_sync();
    return ESP_OK;
}
