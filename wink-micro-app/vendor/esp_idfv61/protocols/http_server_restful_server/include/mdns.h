/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *key;
    const char *value;
} mdns_txt_item_t;

static inline esp_err_t mdns_init(void) {
    return ESP_OK;
}

static inline esp_err_t mdns_hostname_set(const char *hostname) {
    (void)hostname;
    return ESP_OK;
}

static inline esp_err_t mdns_instance_name_set(const char *instance_name) {
    (void)instance_name;
    return ESP_OK;
}

static inline esp_err_t mdns_service_add(const char *instance_name, const char *service_type,
                                         const char *proto, uint16_t port,
                                         mdns_txt_item_t txt[], size_t num_items) {
    (void)instance_name;
    (void)service_type;
    (void)proto;
    (void)port;
    (void)txt;
    (void)num_items;
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif
