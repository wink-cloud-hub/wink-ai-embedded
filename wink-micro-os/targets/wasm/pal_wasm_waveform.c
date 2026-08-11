// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_waveform.c
 * @brief Sub-step high-fidelity waveform event queue implementation.
 */

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "pal_wasm_waveform.h"
#include "wasm_bridge.h"
#include "pal_wasm_common.h"

static waveform_edge_t s_ring[WAVEFORM_RING_SIZE];
static uint32_t s_ring_head = 0;
static uint32_t s_ring_count = 0;
static uint32_t s_overflow_count = 0;
static uint32_t s_current_generation[WASM_SIM_MAX_PINS] = {0};
static bool     s_in_pulse_measurement = false;

void pal_wasm_waveform_reset(void) {
    memset(s_ring, 0, sizeof(s_ring));
    s_ring_head = 0;
    s_ring_count = 0;
    s_overflow_count = 0;
    memset(s_current_generation, 0, sizeof(s_current_generation));
    s_in_pulse_measurement = false;
}

void pal_wasm_set_pulse_measurement_active(bool active) {
    s_in_pulse_measurement = active;
}

bool pal_wasm_is_pulse_measurement_active(void) {
    return s_in_pulse_measurement;
}

EMSCRIPTEN_KEEPALIVE
void pal_wasm_push_waveform_edge(uint16_t pin, uint64_t t_us, uint8_t level, uint32_t generation) {
    WASM_FAULT_GUARD_VOID();
    if (pin >= WASM_SIM_MAX_PINS) return;

    if (generation > 0 && generation != s_current_generation[pin]) {
        pal_wasm_cancel_waveform_generation(pin, s_current_generation[pin]);
        s_current_generation[pin] = generation;
    }

    if (s_ring_count >= WAVEFORM_RING_SIZE) {
        s_overflow_count++;
        // Drop oldest item at head
        s_ring_head = (s_ring_head + 1) % WAVEFORM_RING_SIZE;
        s_ring_count--;
    }

    uint32_t insert_idx = (s_ring_head + s_ring_count) % WAVEFORM_RING_SIZE;
    s_ring[insert_idx].t_us = t_us;
    s_ring[insert_idx].pin = pin;
    s_ring[insert_idx].level = level;
    s_ring[insert_idx].generation = generation;
    s_ring[insert_idx].valid = true;
    s_ring_count++;
}

EMSCRIPTEN_KEEPALIVE
void pal_wasm_cancel_waveform_generation(uint16_t pin, uint32_t generation) {
    WASM_FAULT_GUARD_VOID();
    if (pin >= WASM_SIM_MAX_PINS) return;

    for (uint32_t i = 0; i < s_ring_count; i++) {
        uint32_t idx = (s_ring_head + i) % WAVEFORM_RING_SIZE;
        if (s_ring[idx].valid && s_ring[idx].pin == pin && s_ring[idx].generation <= generation) {
            s_ring[idx].valid = false;
        }
    }
}

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_drain_due_waveform_edges(uint64_t until_us) {
    uint32_t drained = 0;
    uint32_t i = 0;

    while (i < s_ring_count) {
        uint32_t idx = (s_ring_head + i) % WAVEFORM_RING_SIZE;
        if (!s_ring[idx].valid) {
            if (i == 0) {
                s_ring_head = (s_ring_head + 1) % WAVEFORM_RING_SIZE;
                s_ring_count--;
                continue;
            } else {
                i++;
                continue;
            }
        }

        if (s_ring[idx].t_us <= until_us) {
            s_ring[idx].valid = false;
            drained++;

            js_pal_notify_pin_edge(s_ring[idx].pin, s_ring[idx].level, s_ring[idx].t_us);

            if (i == 0) {
                s_ring_head = (s_ring_head + 1) % WAVEFORM_RING_SIZE;
                s_ring_count--;
                continue;
            } else {
                i++;
            }
        } else {
            i++;
        }
    }

    return drained;
}

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_get_waveform_overflow_count(void) {
    return s_overflow_count;
}

EMSCRIPTEN_KEEPALIVE
int32_t pal_wasm_export_waveform_state(uint8_t *out_buf, uint32_t max_len) {
    WASM_FAULT_GUARD_INT32();
    uint32_t required_size = sizeof(uint32_t) * 2 + s_ring_count * sizeof(waveform_edge_t);
    if (out_buf == NULL || max_len == 0) {
        return (int32_t)required_size;
    }
    if (max_len < required_size) {
        return -1;
    }

    uint32_t offset = 0;
    memcpy(out_buf + offset, &s_ring_count, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(out_buf + offset, &s_overflow_count, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    for (uint32_t i = 0; i < s_ring_count; i++) {
        uint32_t idx = (s_ring_head + i) % WAVEFORM_RING_SIZE;
        memcpy(out_buf + offset, &s_ring[idx], sizeof(waveform_edge_t));
        offset += sizeof(waveform_edge_t);
    }

    return (int32_t)offset;
}

EMSCRIPTEN_KEEPALIVE
int32_t pal_wasm_restore_waveform_state(const uint8_t *in_buf, uint32_t len) {
    WASM_FAULT_GUARD_INT32();
    if (in_buf == NULL || len < sizeof(uint32_t) * 2) {
        return -1;
    }

    uint32_t offset = 0;
    uint32_t count = 0;
    memcpy(&count, in_buf + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    if (count > WAVEFORM_RING_SIZE) {
        return -1;
    }

    memcpy(&s_overflow_count, in_buf + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint32_t required_len = offset + count * sizeof(waveform_edge_t);
    if (len < required_len) {
        return -1;
    }

    s_ring_head = 0;
    s_ring_count = count;
    for (uint32_t i = 0; i < count; i++) {
        memcpy(&s_ring[i], in_buf + offset, sizeof(waveform_edge_t));
        offset += sizeof(waveform_edge_t);
    }

    return (int32_t)offset;
}
