/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sim_network_change_cb_t)(bool ready, void *user_ctx);

#define SIM_NETWORK_MAX_CBS 4

void sim_network_broker_reset(void);
void sim_network_broker_set_ready(bool ready);
bool sim_network_broker_is_ready(void);

int  sim_network_broker_register_cb(sim_network_change_cb_t cb, void *user_ctx);
void sim_network_broker_unregister_cb(sim_network_change_cb_t cb);

#ifdef __cplusplus
}
#endif
