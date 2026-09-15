// SPDX-License-Identifier: GPL-3.0-only
// Task F1: MCS-51 timed edge queue implementation.
#include "wink_mcs51_edge_queue.h"

#include <cstring>
#include "wink_mcs51_extint.h"
#include "wink_mcs51_timer.h"

#ifndef __EMSCRIPTEN__
extern "C" void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state);
#endif

extern "C" {

wink_status_t mcs51_edge_queue_push(struct Mcu51Context* ctx, uint64_t fire_us, uint16_t pin, uint8_t level) {
    if (!ctx) ctx = mcs51_get_context();
    uint8_t count = mcs51_edge_queue_count(ctx);
    if (count >= MCS51_EDGE_QUEUE_CAP) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    // Insert sorted by fire_us ascending
    // Linearize into a temporary array, insert, and store back
    McuEdgeEvent temp[MCS51_EDGE_QUEUE_CAP];
    for (uint8_t i = 0; i < count; ++i) {
        temp[i] = ctx->edge_queue[(ctx->edge_tail + i) % MCS51_EDGE_QUEUE_CAP];
    }

    uint8_t insert_idx = count;
    for (uint8_t i = 0; i < count; ++i) {
        if (fire_us < temp[i].fire_us) {
            insert_idx = i;
            break;
        }
    }

    for (uint8_t i = count; i > insert_idx; --i) {
        temp[i] = temp[i - 1];
    }
    temp[insert_idx].fire_us = fire_us;
    temp[insert_idx].pin = pin;
    temp[insert_idx].level = level;

    // Write back to queue starting from index 0
    for (uint8_t i = 0; i <= count; ++i) {
        ctx->edge_queue[i] = temp[i];
    }
    ctx->edge_tail = 0;
    ctx->edge_head = static_cast<uint8_t>(count + 1);

    return WINK_OK;
}

bool mcs51_edge_queue_pop(struct Mcu51Context* ctx, McuEdgeEvent* out_ev) {
    if (!ctx) ctx = mcs51_get_context();
    if (ctx->edge_head == ctx->edge_tail) {
        return false;
    }
    if (out_ev != nullptr) {
        *out_ev = ctx->edge_queue[ctx->edge_tail];
    }
    ctx->edge_tail = static_cast<uint8_t>((ctx->edge_tail + 1) % MCS51_EDGE_QUEUE_CAP);
    return true;
}

uint8_t mcs51_edge_queue_count(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (ctx->edge_head >= ctx->edge_tail) {
        return static_cast<uint8_t>(ctx->edge_head - ctx->edge_tail);
    }
    return static_cast<uint8_t>(MCS51_EDGE_QUEUE_CAP - (ctx->edge_tail - ctx->edge_head));
}

void mcs51_edge_queue_drain(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    while (ctx->edge_head != ctx->edge_tail) {
        McuEdgeEvent ev = ctx->edge_queue[ctx->edge_tail];
        if (ev.fire_us > ctx->virtual_us) {
            break;
        }
        // Dequeue
        ctx->edge_tail = static_cast<uint8_t>((ctx->edge_tail + 1) % MCS51_EDGE_QUEUE_CAP);

#ifndef __EMSCRIPTEN__
        wink_mcs51_host_set_ext_pin(ev.pin, ev.level);
#endif
        // Request immediate extint re-poll without throttle
        ctx->extint.sample_due = true;

        // If this falling edge is on T0 (P3.4=pin 28), T1 (P3.5=pin 29), or T2 (P1.6=pin 14), trigger timer pulse
        if (ev.level == 0) {
            if (ev.pin == 28u) {
                wink_mcs51_timer_pulse(0);
            } else if (ev.pin == 29u) {
                wink_mcs51_timer_pulse(1);
            } else if (ev.pin == 14u) {
                wink_mcs51_timer_pulse(2);
            }
        }
    }
}

void mcs51_edge_queue_clear(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    ctx->edge_head = 0;
    ctx->edge_tail = 0;
}

} // extern "C"
