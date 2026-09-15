// SPDX-License-Identifier: GPL-3.0-only
// Task F1: Timed edge event injection queue for MCS-51 simulation.
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "mcs51_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MCS51_EDGE_QUEUE_CAP 64u

// Push a timed edge transition event (fire_us, pin, level).
// Automatically keeps the queue ordered by fire_us ascending.
wink_status_t mcs51_edge_queue_push(struct Mcu51Context* ctx, uint64_t fire_us, uint16_t pin, uint8_t level);

// Pop the next pending edge event. Returns false if queue is empty.
bool mcs51_edge_queue_pop(struct Mcu51Context* ctx, McuEdgeEvent* out_ev);

// Return number of events currently queued.
uint8_t mcs51_edge_queue_count(struct Mcu51Context* ctx);

// Process all edge events whose fire_us <= ctx->virtual_us.
// Injects pin state changes into external pin models and triggers extint sampling.
void mcs51_edge_queue_drain(struct Mcu51Context* ctx);

// Clear all queued events.
void mcs51_edge_queue_clear(struct Mcu51Context* ctx);

#ifdef __cplusplus
}
#endif
