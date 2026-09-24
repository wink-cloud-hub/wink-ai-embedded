/* SPDX-License-Identifier: GPL-3.0-only
 * PLAN-20260924-MCS51-T01-OVERFLOW-REARM-FIX vendor e2e regression: the REAL
 * (transpiled, unmodified) vendor `timer0_timming_mode` firmware on the host
 * fiber runtime must dispatch the Timer0 ISR exactly once per 100 us period
 * (20 master ticks * 10 ms = 200 ms -> ~2000 overflows). The pre-fix code
 * dispatched twice per period (10 kHz instead of 5 kHz).
 *
 * This is the ISR shape the ctest suite was missing: mode 2 auto-reload with
 * a toggle-only ISR (no TH0/TL0 write), i.e. the canonical way to use mode 2.
 */
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "mcs51_trap.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);
extern uint32_t wink_mcs51_isr_dispatch_count(uint8_t vector_num);

#define P3_SFR_ADDR 0xB0u
#define P32_MASK    0x04u
#define RUN_TICKS   20u     /* 20 master ticks * 10 ms = 200 ms */
#define PERIOD_US   100u    /* T0 mode 2, 200 counts @ 24 MHz / 12 */

static uint32_t g_p32_edges;
static int g_fails;

static void check(int cond, const char *msg) {
    if (!cond) {
        printf("[mcs51-t0-rearm] FAIL: %s\n", msg);
        ++g_fails;
    }
}

/* P3 write hook: count P3.2 edges (init write + one toggle per dispatch).
 * Registered from the post-init hook: the framework reset wipes hooks and
 * re-runs the post-init hook afterwards (S4-H2/S4-H3 contract). */
static void p3_write_hook(struct Mcu51Context *ctx, uint8_t addr,
                          uint8_t old_val, uint8_t new_val) {
    (void)ctx;
    (void)addr;
    if (((old_val ^ new_val) & P32_MASK) != 0u) {
        ++g_p32_edges;
    }
}

static void boot_hook(void) {
    mcs51_trap_register_sfr_write(P3_SFR_ADDR, p3_write_hook);
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51-t0-rearm] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_framework_set_post_init_hook(boot_hook);
    g_p32_edges = 0u;
    g_fails = 0;

    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51-t0-rearm] FAIL: runtime run returned %d\n", (int)st);
        return 1;
    }

    const uint32_t dispatch = wink_mcs51_isr_dispatch_count(1u);
    const uint32_t expected = (RUN_TICKS * 10000u) / PERIOD_US;
    printf("[mcs51-t0-rearm] dispatch=%u expected=%u p32_edges=%u\n",
           (unsigned)dispatch, (unsigned)expected, (unsigned)g_p32_edges);

    check(dispatch + 20u >= expected && dispatch <= expected + 20u,
          "Timer0 dispatch count must be one per 100us period "
          "(pre-fix: twice per period)");
    check(g_p32_edges >= dispatch && g_p32_edges <= dispatch + 2u,
          "P3.2 edges must match the dispatch count (one toggle per ISR run)");

    if (g_fails == 0) {
        printf("[mcs51-t0-rearm] PASS: vendor timer0 firmware dispatches "
               "once per period\n");
        return 0;
    }
    printf("[mcs51-t0-rearm] FAILED: %d checks failed\n", g_fails);
    return 1;
}
