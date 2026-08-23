// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_pcnt.c
 * @brief Wasm target PAL PCNT quadrature and 64-bit software accumulator.
 */
#include "hal/pal_pcnt.h"
#include "wasm_bridge.h"
#include <string.h>

#define WASM_PCNT_UNITS_MAX 4

typedef struct {
    bool             in_use;
    bool             is_running;
    pal_pcnt_config_t cfg;
    int64_t          count;
} wasm_pcnt_slot_t;

static wasm_pcnt_slot_t s_pcnt[WASM_PCNT_UNITS_MAX];

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pcnt_init(const pal_pcnt_config_t *cfg, pal_pcnt_handle_t *out_handle) {
    if (cfg == NULL || out_handle == NULL || cfg->unit >= WASM_PCNT_UNITS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }
    wasm_pcnt_slot_t *slot = &s_pcnt[cfg->unit];
    if (slot->in_use) {
        return WINK_ERR_BUSY;
    }
    slot->in_use = true;
    slot->is_running = false;
    slot->cfg = *cfg;
    slot->count = 0;

    *out_handle = (pal_pcnt_handle_t)slot;
    return WINK_OK;
}

wink_status_t pal_pcnt_start(pal_pcnt_handle_t handle) {
    if (handle == NULL) return WINK_ERR_INVALID_ARG;
    wasm_pcnt_slot_t *slot = (wasm_pcnt_slot_t *)handle;
    slot->is_running = true;
    return WINK_OK;
}

wink_status_t pal_pcnt_stop(pal_pcnt_handle_t handle) {
    if (handle == NULL) return WINK_ERR_INVALID_ARG;
    wasm_pcnt_slot_t *slot = (wasm_pcnt_slot_t *)handle;
    slot->is_running = false;
    return WINK_OK;
}

wink_status_t pal_pcnt_get_count(pal_pcnt_handle_t handle, int64_t *out_count) {
    if (handle == NULL || out_count == NULL) return WINK_ERR_INVALID_ARG;
    wasm_pcnt_slot_t *slot = (wasm_pcnt_slot_t *)handle;
    *out_count = slot->count;
    return WINK_OK;
}

wink_status_t pal_pcnt_clear_count(pal_pcnt_handle_t handle) {
    if (handle == NULL) return WINK_ERR_INVALID_ARG;
    wasm_pcnt_slot_t *slot = (wasm_pcnt_slot_t *)handle;
    slot->count = 0;
    return WINK_OK;
}

void pal_pcnt_deinit(pal_pcnt_handle_t handle) {
    if (handle == NULL) return;
    wasm_pcnt_slot_t *slot = (wasm_pcnt_slot_t *)handle;
    slot->in_use = false;
    slot->is_running = false;
}

void pal_wasm_push_pcnt_edge(uint8_t unit, int32_t delta) {
    if (unit < WASM_PCNT_UNITS_MAX && s_pcnt[unit].in_use && s_pcnt[unit].is_running) {
        s_pcnt[unit].count += delta;
    }
}
