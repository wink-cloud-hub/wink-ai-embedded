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

typedef struct {
    esp_netif_t *netif;
    bool ready;
    bool in_use;
} sim_netif_state_t;

static sim_netif_state_t s_netifs[SIM_NETWORK_MAX_NETIFS];
static sim_network_cb_slot_t s_slots[SIM_NETWORK_MAX_CBS];
static bool s_routing_enabled = true;
static size_t s_routed_bytes = 0;
static size_t s_routed_packets = 0;

void sim_network_broker_reset(void) {
    memset(s_netifs, 0, sizeof(s_netifs));
    memset(s_slots, 0, sizeof(s_slots));
    s_routing_enabled = true;
    s_routed_bytes = 0;
    s_routed_packets = 0;
}

bool sim_network_broker_is_netif_ready(esp_netif_t *netif) {
    if (!netif) {
        netif = esp_netif_get_handle_sta();
    }
    for (size_t i = 0; i < SIM_NETWORK_MAX_NETIFS; i++) {
        if (s_netifs[i].in_use && s_netifs[i].netif == netif) {
            return s_netifs[i].ready;
        }
    }
    return false;
}

bool sim_network_broker_is_ready(void) {
    return sim_network_broker_is_netif_ready(esp_netif_get_handle_sta());
}

void sim_network_broker_notify_netif(esp_netif_t *netif, sim_netif_event_t event) {
    bool ready = (event == SIM_NETIF_EVT_UP);
    if (!netif) {
        netif = esp_netif_get_handle_sta();
    }

    bool found = false;
    for (size_t i = 0; i < SIM_NETWORK_MAX_NETIFS; i++) {
        if (s_netifs[i].in_use && s_netifs[i].netif == netif) {
            s_netifs[i].ready = ready;
            found = true;
            break;
        }
    }
    if (!found) {
        for (size_t i = 0; i < SIM_NETWORK_MAX_NETIFS; i++) {
            if (!s_netifs[i].in_use) {
                s_netifs[i].in_use = true;
                s_netifs[i].netif = netif;
                s_netifs[i].ready = ready;
                break;
            }
        }
    }

    for (size_t i = 0; i < SIM_NETWORK_MAX_CBS; i++) {
        if (s_slots[i].in_use && s_slots[i].cb) {
            s_slots[i].cb(netif, event, s_slots[i].user_ctx);
        }
    }
}

void sim_network_broker_set_netif_ready(esp_netif_t *netif, bool ready) {
    if (!netif) {
        netif = esp_netif_get_handle_sta();
    }
    if (sim_network_broker_is_netif_ready(netif) == ready) {
        return;
    }
    sim_netif_event_t evt = ready ? SIM_NETIF_EVT_UP : SIM_NETIF_EVT_DOWN;
    sim_network_broker_notify_netif(netif, evt);
}

void sim_network_broker_set_ready(bool ready) {
    sim_network_broker_set_netif_ready(esp_netif_get_handle_sta(), ready);
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

void sim_network_broker_set_routing(bool enabled) {
    s_routing_enabled = enabled;
}

bool sim_network_broker_is_routing_enabled(void) {
    return s_routing_enabled;
}

int sim_network_broker_route_packet(esp_netif_t *src, esp_netif_t *dst, const uint8_t *payload, size_t len) {
    if (!s_routing_enabled || !payload || len == 0) {
        return -1;
    }
    if (src && !sim_network_broker_is_netif_ready(src)) {
        return -1;
    }
    if (dst && !sim_network_broker_is_netif_ready(dst)) {
        return -1;
    }
    s_routed_bytes += len;
    s_routed_packets++;
    return (int)len;
}

size_t sim_network_broker_get_routed_bytes(void) {
    return s_routed_bytes;
}

size_t sim_network_broker_get_routed_packets(void) {
    return s_routed_packets;
}
