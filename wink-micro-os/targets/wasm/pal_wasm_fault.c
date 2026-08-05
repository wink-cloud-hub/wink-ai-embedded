// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_fault.c
 * @brief Wasm simulation fault fast-fail subsystem implementation.
 */
#include "pal_wasm_internal.h"
#include "wink_trace.h"

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

struct wink_app_callbacks;
extern void wink_runtime_fault(const struct wink_app_callbacks* callbacks, uint32_t fault_code);

static bool s_wasm_faulted = false;
static const struct wink_app_callbacks* s_app_callbacks = NULL;

EMSCRIPTEN_KEEPALIVE
bool pal_wasm_is_faulted(void) { return s_wasm_faulted; }

void pal_wasm_clear_fault_latch(void) { s_wasm_faulted = false; s_app_callbacks = NULL; }

void pal_wasm_fault_set_callbacks(const struct wink_app_callbacks* cb) {
    s_app_callbacks = cb;
}

void pal_wasm_invoke_fault(uint32_t code) {
    if (s_wasm_faulted) {
        wink_trace_fault(code);
        return;
    }
    s_wasm_faulted = true;
    wink_runtime_fault(s_app_callbacks, code);
}

EMSCRIPTEN_KEEPALIVE
void pal_wasm_host_fault(uint32_t code, const char* msg_cstr) {
    (void)msg_cstr;
    if (s_wasm_faulted) {
        wink_trace_fault(code);
        return;
    }
    s_wasm_faulted = true;
    wink_runtime_fault(s_app_callbacks, code);
}

static wasm_fault_event_t s_fault_log[WASM_FAULT_LOG_SIZE];
static uint32_t s_fault_log_head;
static uint32_t s_fault_log_count;
static uint32_t s_fault_sequence;

EMSCRIPTEN_KEEPALIVE
void pal_wasm_reset_fault_log(void) {
    WASM_FAULT_GUARD_VOID();
    memset(s_fault_log, 0, sizeof(s_fault_log));
    s_fault_log_head = 0;
    s_fault_log_count = 0;
    s_fault_sequence = 0;
}

void pal_wasm_log_fault(uint8_t fault_type, uint16_t pin_or_bus) {
    wasm_fault_event_t *evt = &s_fault_log[s_fault_log_head];

    evt->timestamp_us = pal_wasm_get_virtual_clock_us();
    evt->fault_type   = fault_type;
    evt->pin_or_bus   = pin_or_bus;
    evt->sequence     = ++s_fault_sequence;

    s_fault_log_head = (s_fault_log_head + 1u) % WASM_FAULT_LOG_SIZE;
    if (s_fault_log_count < WASM_FAULT_LOG_SIZE) {
        s_fault_log_count++;
    }
}

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_get_fault_log_count(void) {
    return s_fault_log_count;
}

EMSCRIPTEN_KEEPALIVE
bool pal_wasm_get_fault_event(uint32_t index, wasm_fault_event_t *out_event) {
    if (out_event == NULL || index >= s_fault_log_count) {
        return false;
    }
    uint32_t oldest = (s_fault_log_head + WASM_FAULT_LOG_SIZE - s_fault_log_count)
                       % WASM_FAULT_LOG_SIZE;
    uint32_t actual_idx = (oldest + index) % WASM_FAULT_LOG_SIZE;
    *out_event = s_fault_log[actual_idx];
    return true;
}

EMSCRIPTEN_KEEPALIVE
uint64_t pal_wasm_fault_event_get_timestamp(uint32_t index) {
    wasm_fault_event_t evt;
    return pal_wasm_get_fault_event(index, &evt) ? evt.timestamp_us : 0u;
}

EMSCRIPTEN_KEEPALIVE
uint8_t pal_wasm_fault_event_get_type(uint32_t index) {
    wasm_fault_event_t evt;
    return pal_wasm_get_fault_event(index, &evt) ? evt.fault_type : 0u;
}

EMSCRIPTEN_KEEPALIVE
uint16_t pal_wasm_fault_event_get_pin_or_bus(uint32_t index) {
    wasm_fault_event_t evt;
    return pal_wasm_get_fault_event(index, &evt) ? evt.pin_or_bus : 0u;
}

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_fault_event_get_sequence(uint32_t index) {
    wasm_fault_event_t evt;
    return pal_wasm_get_fault_event(index, &evt) ? evt.sequence : 0u;
}

#endif
