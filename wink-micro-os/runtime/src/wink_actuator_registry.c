// SPDX-License-Identifier: Apache-2.0
#include "wink_actuator_registry.h"
#include "wink_trace.h"

typedef struct {
    wink_actuator_safe_off_fn fn;
    void                     *ctx;
} wink_actuator_entry_t;

static wink_actuator_entry_t s_entries[WINK_ACTUATOR_REGISTRY_CAPACITY];
static uint32_t s_count = 0;

void wink_actuator_registry_reset(void) {
    s_count = 0;
}

WINK_WARN_UNUSED_RESULT
wink_status_t wink_actuator_register(wink_actuator_safe_off_fn fn, void *ctx) {
    if (fn == NULL) { return WINK_ERR_INVALID_ARG; }
    for (uint32_t i = 0; i < s_count; i++) {
        if (s_entries[i].fn == fn && s_entries[i].ctx == ctx) {
            return WINK_OK;
        }
    }
    if (s_count >= WINK_ACTUATOR_REGISTRY_CAPACITY) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }
    s_entries[s_count].fn  = fn;
    s_entries[s_count].ctx = ctx;
    s_count++;
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t wink_actuator_unregister(wink_actuator_safe_off_fn fn, void *ctx) {
    for (uint32_t i = 0; i < s_count; i++) {
        if (s_entries[i].fn == fn && s_entries[i].ctx == ctx) {
            for (uint32_t j = i + 1; j < s_count; j++) {
                s_entries[j - 1] = s_entries[j];
            }
            s_count--;
            return WINK_OK;
        }
    }
    return WINK_ERR_NOT_FOUND;
}

void wink_actuator_safe_off_all(void) {
    for (uint32_t i = 0; i < s_count; i++) {
        wink_status_t s = s_entries[i].fn(s_entries[i].ctx);
        if (wink_status_is_error(s)) {
            wink_trace_fault(7000u + i);
        }
    }
}
