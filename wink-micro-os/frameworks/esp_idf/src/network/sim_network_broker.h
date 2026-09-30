/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_netif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SIM_NETIF_EVT_DOWN = 0,
    SIM_NETIF_EVT_UP   = 1,
} sim_netif_event_t;

typedef void (*sim_netif_event_cb_t)(esp_netif_t *netif, sim_netif_event_t event, void *user_ctx);

#define SIM_NETWORK_MAX_CBS 8

void sim_network_broker_reset(void);
void sim_network_broker_set_ready(bool ready);
bool sim_network_broker_is_ready(void);

int  sim_network_broker_register_cb(sim_netif_event_cb_t cb, void *user_ctx);
void sim_network_broker_unregister_cb(sim_netif_event_cb_t cb, void *user_ctx);
void sim_network_broker_notify_netif(esp_netif_t *netif, sim_netif_event_t event);

#ifdef __cplusplus
}
#endif
