/* SPDX-License-Identifier: LGPL-3.0-only */
#include "sim_network_broker.h"
#include "esp_netif.h"
#include <string.h>

extern esp_netif_t* esp_netif_get_handle_sta(void);

typedef struct {
    sim_netif_event_cb_t cb;
    void *user_ctx;
    bool in_use;
} sim_network_cb_slot_t;

static bool s_broker_ready = false;
static sim_network_cb_slot_t s_slots[SIM_NETWORK_MAX_CBS];

void sim_network_broker_reset(void) {
    s_broker_ready = false;
    memset(s_slots, 0, sizeof(s_slots));
}

void sim_network_broker_notify_netif(esp_netif_t *netif, sim_netif_event_t event) {
    bool ready = (event == SIM_NETIF_EVT_UP);
    s_broker_ready = ready;
    for (size_t i = 0; i < SIM_NETWORK_MAX_CBS; i++) {
        if (s_slots[i].in_use && s_slots[i].cb) {
            s_slots[i].cb(netif, event, s_slots[i].user_ctx);
        }
    }
}

void sim_network_broker_set_ready(bool ready) {
    if (s_broker_ready == ready) {
        return;
    }
    s_broker_ready = ready;
    esp_netif_t *sta = esp_netif_get_handle_sta();
    sim_netif_event_t evt = ready ? SIM_NETIF_EVT_UP : SIM_NETIF_EVT_DOWN;
    for (size_t i = 0; i < SIM_NETWORK_MAX_CBS; i++) {
        if (s_slots[i].in_use && s_slots[i].cb) {
            s_slots[i].cb(sta, evt, s_slots[i].user_ctx);
        }
    }
}

bool sim_network_broker_is_ready(void) {
    return s_broker_ready;
}

int sim_network_broker_register_cb(sim_netif_event_cb_t cb, void *user_ctx) {
    if (!cb) {
        return -1;
    }
    for (size_t i = 0; i < SIM_NETWORK_MAX_CBS; i++) {
        if (s_slots[i].in_use && s_slots[i].cb == cb && s_slots[i].user_ctx == user_ctx) {
            return 0;
        }
    }
    for (size_t i = 0; i < SIM_NETWORK_MAX_CBS; i++) {
        if (!s_slots[i].in_use) {
            s_slots[i].cb = cb;
            s_slots[i].user_ctx = user_ctx;
            s_slots[i].in_use = true;
            return 0;
        }
    }
    return -1;
}

void sim_network_broker_unregister_cb(sim_netif_event_cb_t cb, void *user_ctx) {
    if (!cb) {
        return;
    }
    for (size_t i = 0; i < SIM_NETWORK_MAX_CBS; i++) {
        if (s_slots[i].in_use && s_slots[i].cb == cb && s_slots[i].user_ctx == user_ctx) {
            s_slots[i].in_use = false;
            s_slots[i].cb = NULL;
            s_slots[i].user_ctx = NULL;
        }
    }
}
