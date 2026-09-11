// SPDX-License-Identifier: Apache-2.0
// Task F1: Timed edge event injection queue test.
#include <stdint.h>
#include <stdio.h>
#include <cstring>

#include "mcs51_context.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_edge_queue.h"

#ifndef __EMSCRIPTEN__
extern "C" uint8_t js_pal_gpio_read_state(uint16_t pin);
extern "C" void wink_mcs51_host_ext_pins_reset(void);
#endif

namespace {

int g_fails = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        printf("[mcs51-edge-queue] FAIL: %s\n", what);
        ++g_fails;
    }
}

} // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);
#ifndef __EMSCRIPTEN__
    wink_mcs51_host_ext_pins_reset();
#endif

    // ── Test 1: Ordering by fire_us ───────────────────────────────────────────
    check(mcs51_edge_queue_count(ctx) == 0, "Queue must start empty");

    // Insert out of order: 300us, 100us, 200us
    check(mcs51_edge_queue_push(ctx, 300, 26, 0) == WINK_OK, "Push 300us");
    check(mcs51_edge_queue_push(ctx, 100, 26, 1) == WINK_OK, "Push 100us");
    check(mcs51_edge_queue_push(ctx, 200, 26, 0) == WINK_OK, "Push 200us");

    check(mcs51_edge_queue_count(ctx) == 3, "Queue must have 3 items");

    McuEdgeEvent ev;
    check(mcs51_edge_queue_pop(ctx, &ev) && ev.fire_us == 100 && ev.level == 1, "First event must be 100us");
    check(mcs51_edge_queue_pop(ctx, &ev) && ev.fire_us == 200 && ev.level == 0, "Second event must be 200us");
    check(mcs51_edge_queue_pop(ctx, &ev) && ev.fire_us == 300 && ev.level == 0, "Third event must be 300us");
    check(!mcs51_edge_queue_pop(ctx, &ev), "Queue must now be empty");

    // ── Test 2: Queue capacity exhaustion ─────────────────────────────────────
    for (uint8_t i = 0; i < MCS51_EDGE_QUEUE_CAP; ++i) {
        check(mcs51_edge_queue_push(ctx, 1000 + i, 10, i % 2) == WINK_OK, "Push until full");
    }
    check(mcs51_edge_queue_count(ctx) == MCS51_EDGE_QUEUE_CAP, "Queue must be at capacity");
    check(mcs51_edge_queue_push(ctx, 9999, 10, 1) == WINK_ERR_RESOURCE_EXHAUSTED, "Pushing when full must fail");

    mcs51_edge_queue_clear(ctx);
    check(mcs51_edge_queue_count(ctx) == 0, "Clear must empty queue");

    // ── Test 3: Drain based on virtual_us ─────────────────────────────────────
    ctx->virtual_us = 150;
    mcs51_edge_queue_push(ctx, 100, 26, 0); // past -> must drain
    mcs51_edge_queue_push(ctx, 150, 26, 1); // current -> must drain
    mcs51_edge_queue_push(ctx, 200, 26, 0); // future -> must remain

    mcs51_edge_queue_drain(ctx);

    check(mcs51_edge_queue_count(ctx) == 1, "Only future event must remain after drain");
#ifndef __EMSCRIPTEN__
    check(js_pal_gpio_read_state(26) == 1, "Pin level must match last executed event (level=1)");
#endif
    check(ctx->extint.sample_due, "ExtInt sample_due flag must be asserted");

    if (g_fails != 0) {
        printf("[mcs51-edge-queue] FAILED with %d errors\n", g_fails);
        return 1;
    }
    printf("[mcs51-edge-queue] PASS: Timed edge queue verified\n");
    return 0;
}
