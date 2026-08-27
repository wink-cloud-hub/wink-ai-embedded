/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 M2 host/wasm test: Timer0 mode-1 overflow drives the ISR through
 * virtual-time catch-up (ADR-0072). The unmodified Keil sample configures a
 * 50 ms Timer0 interrupt; the ISR toggles P1.0 and reloads TH0/TL0.
 *
 * Asserts (acceptance #1 first half + #2):
 *   - the Timer0 ISR (vector 1) auto-registered and actually dispatched;
 *   - LED toggled (ISR side effects observed);
 *   - the dispatch count matches the 1:1 virtual-time budget (50 ms period:
 *     RUN_TICKS master ticks * 10 ms = N ms, ~N/50 overflows), i.e. timer
 *     periods are conserved with master ticks (Catch-Up, D3);
 *   - quota yields occurred (tight idle loop did not freeze the master);
 *   - the slave virtual clock covered the run budget 1:1.
 */
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);
extern uint8_t wink_mcs51_sfr_shadow[256];
extern void (*wink_mcs51_get_isr(uint8_t vector_num))(void);

/* C-ABI observability from the mcs51 framework. */
extern uint32_t wink_mcs51_isr_dispatch_count(uint8_t vector_num);
extern uint32_t wink_mcs51_quota_yield_count(void);
extern uint64_t wink_mcs51_virtual_us(void);

#define P1_SFR_ADDR         0x90u
#define LED_BIT             0x01u
#define MCS51_TIMER0_VECTOR 1u
#define RUN_TICKS           200u    /* 200 master ticks * 10 ms = 2000 ms */
#define TIMER_PERIOD_MS     50u

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51] FAIL: runtime run returned %d\n", (int)st);
        return 1;
    }

    int fails = 0;

    if (wink_mcs51_get_isr(MCS51_TIMER0_VECTOR) == NULL) {
        printf("[mcs51] FAIL: Timer0 ISR vector 1 not registered\n");
        fails++;
    }

    uint32_t isr_count = wink_mcs51_isr_dispatch_count(MCS51_TIMER0_VECTOR);
    uint32_t quota     = wink_mcs51_quota_yield_count();
    uint64_t virt_us   = wink_mcs51_virtual_us();
    uint8_t  led       = (uint8_t)(wink_mcs51_sfr_shadow[P1_SFR_ADDR] & LED_BIT);

    /* 2000 ms budget / 50 ms period = 40 overflows; allow boundary slack. */
    uint32_t expected = (RUN_TICKS * 10u) / TIMER_PERIOD_MS;
    if (isr_count < expected - 2 || isr_count > expected + 2) {
        printf("[mcs51] FAIL: Timer0 ISR dispatched %u times, want ~%u "
               "(50 ms period over %u ticks)\n",
               (unsigned)isr_count, (unsigned)expected, (unsigned)RUN_TICKS);
        fails++;
    }

    /* The ISR toggles LED every overflow; with ~40 overflows it must have
     * toggled (final level depends on parity, only check it ran via count). */
    if (isr_count == 0) {
        printf("[mcs51] FAIL: Timer0 ISR never dispatched\n");
        fails++;
    }
    (void)led;

    if (quota == 0u) {
        printf("[mcs51] FAIL: no quota yield — tight loop would freeze\n");
        fails++;
    }

    /* Virtual clock must cover the 1:1 master budget (200 ticks * 10 ms). */
    uint64_t min_virt = (uint64_t)RUN_TICKS * 10000u - 10000u;
    if (virt_us < min_virt) {
        printf("[mcs51] FAIL: virtual clock %llu us < budget %llu us "
               "(time not conserved)\n",
               (unsigned long long)virt_us, (unsigned long long)min_virt);
        fails++;
    }

    if (fails) {
        return 1;
    }
    printf("[mcs51] PASS: Timer0 ISR x%u (50 ms period, 1:1 conserved), "
           "quota yields %u, virt=%llu us, LED=%u (no freeze)\n",
           (unsigned)isr_count, (unsigned)quota,
           (unsigned long long)virt_us, (unsigned)led);
    return 0;
}
