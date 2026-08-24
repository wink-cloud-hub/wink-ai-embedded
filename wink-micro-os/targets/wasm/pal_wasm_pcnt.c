// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_pcnt.c
 * @brief Wasm target PAL PCNT quadrature and 64-bit software accumulator.
 */
#include "hal/pal_pcnt.h"
#include "wasm_bridge.h"
#include <string.h>

#define WASM_PCNT_UNITS_MAX 4

struct pal_pcnt_unit_s {
    bool              in_use;
    pal_pcnt_config_t cfg;
    int64_t           count;
    uint32_t          filter_ns;
};

static struct pal_pcnt_unit_s s_pcnt[WASM_PCNT_UNITS_MAX];

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pcnt_init(const pal_pcnt_config_t *cfg, pal_pcnt_unit_handle_t *out_handle) {
    if (cfg == NULL || out_handle == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    struct pal_pcnt_unit_s *slot = NULL;
    for (int i = 0; i < WASM_PCNT_UNITS_MAX; i++) {
        if (!s_pcnt[i].in_use) {
            slot = &s_pcnt[i];
            break;
        }
    }
    if (slot == NULL) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    slot->in_use = true;
    slot->cfg = *cfg;
    slot->count = 0;
    slot->filter_ns = cfg->filter_ns;

    *out_handle = slot;
    return WINK_OK;
}

wink_status_t pal_pcnt_deinit(pal_pcnt_unit_handle_t handle) {
    if (handle == NULL || !handle->in_use) {
        return WINK_ERR_INVALID_ARG;
    }
    handle->in_use = false;
    handle->count = 0;
    return WINK_OK;
}

wink_status_t pal_pcnt_get_count(pal_pcnt_unit_handle_t handle, int64_t *count_out) {
    if (handle == NULL || !handle->in_use || count_out == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    *count_out = handle->count;
    return WINK_OK;
}

wink_status_t pal_pcnt_clear(pal_pcnt_unit_handle_t handle) {
    if (handle == NULL || !handle->in_use) {
        return WINK_ERR_INVALID_ARG;
    }
    handle->count = 0;
    return WINK_OK;
}

wink_status_t pal_pcnt_set_glitch_filter(pal_pcnt_unit_handle_t handle, uint32_t filter_ns) {
    if (handle == NULL || !handle->in_use) {
        return WINK_ERR_INVALID_ARG;
    }
    handle->filter_ns = filter_ns;
    return WINK_OK;
}

void pal_wasm_push_pcnt_edge(uint8_t unit, int32_t delta) {
    if (unit < WASM_PCNT_UNITS_MAX && s_pcnt[unit].in_use) {
        s_pcnt[unit].count += delta;
    }
}
