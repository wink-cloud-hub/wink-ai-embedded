/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_sim_handle.h"
#include <stddef.h>

#define ESP_SIM_HANDLE_MAX_SEQUENCE ((1u << 21) - 1u)
#define ESP_SIM_HANDLE_MAX_SLOTS 64u
#define ESP_SIM_HANDLE_MAX_KIND 15u

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(uint32_t, js_get_initial_sequence_base, (), {
    if (typeof Module !== 'undefined' && typeof Module['initialSequenceBase'] === 'number') {
        return Module['initialSequenceBase'] >>> 0;
    }
    return 0;
});
#endif

static uint32_t s_next_sequence;
static bool s_base_initialized;

EMSCRIPTEN_KEEPALIVE uint32_t esp_sim_handle_issue(uint32_t kind, uint32_t slot) {
    if (!s_base_initialized) {
        s_base_initialized = true;
#ifdef __EMSCRIPTEN__
        uint32_t base = js_get_initial_sequence_base();
        if (base > s_next_sequence && base <= ESP_SIM_HANDLE_MAX_SEQUENCE) {
            s_next_sequence = base;
        }
#endif
    }
    if (kind == 0 || kind > ESP_SIM_HANDLE_MAX_KIND || slot >= ESP_SIM_HANDLE_MAX_SLOTS ||
        s_next_sequence >= ESP_SIM_HANDLE_MAX_SEQUENCE) {
        return 0;
    }
    uint32_t sequence = s_next_sequence + 1u;
    uint32_t token = (sequence << 11) | (kind << 7) | (slot << 1) | 1u;
    if ((uintptr_t)(void *)(uintptr_t)token != (uintptr_t)token) return 0;
    s_next_sequence = sequence;
    return token;
}

EMSCRIPTEN_KEEPALIVE bool esp_sim_handle_decode(const void *handle, uint32_t expected_kind,
                                                uint32_t capacity, uint32_t *out_slot) {
    uintptr_t raw = (uintptr_t)handle;
    if (!out_slot || expected_kind == 0 || expected_kind > ESP_SIM_HANDLE_MAX_KIND ||
        capacity == 0 || capacity > ESP_SIM_HANDLE_MAX_SLOTS ||
        raw > UINT32_MAX || (raw & 1u) == 0 ||
        ((raw >> 7) & 15u) != expected_kind || (raw >> 11) == 0) {
        return false;
    }
    uint32_t slot = (uint32_t)((raw >> 1) & 63u);
    if (slot >= capacity) return false;
    *out_slot = slot;
    return true;
}

EMSCRIPTEN_KEEPALIVE uint32_t esp_sim_handle_get_sequence(void) {
    return s_next_sequence;
}

EMSCRIPTEN_KEEPALIVE void esp_sim_handle_set_sequence_base(uint32_t base) {
    if (base <= ESP_SIM_HANDLE_MAX_SEQUENCE) {
        s_next_sequence = base;
        s_base_initialized = true;
    }
}
