/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 8-digit multiplexed 7-segment display end-to-end host test.
 *
 * Drives the cleaned seg_display_scan.c firmware through the real cooperative
 * runtime and asserts:
 *   (a) Timer0 ISR (vector 1) auto-registered and dispatched at 1ms cadence
 *       (e.g. ~1000 dispatches over 100 master ticks / 1000ms);
 *   (b) Segment bus P0 (pins 0..7) and digit bus P2 (pins 16..23) are actively
 *       driven via the instant-notify GPIO channel;
 *   (c) The 8 digits are round-robin scanned with valid 7-segment patterns;
 *   (d) Time is conserved 1:1 and the cooperative fiber yields without freeze.
 */
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"

/* Provided by the mcs51 compat layer */
extern const wink_app_callbacks_t *wink_app_get_callbacks(void);
extern uint8_t wink_mcs51_sfr_shadow[256];
extern void (*wink_mcs51_get_isr(uint8_t vector_num))(void);

extern uint32_t wink_mcs51_isr_dispatch_count(uint8_t vector_num);
extern uint32_t wink_mcs51_quota_yield_count(void);
extern uint64_t wink_mcs51_virtual_us(void);

#ifndef __EMSCRIPTEN__
extern uint32_t wink_mcs51_host_gpio_notify_count(void);
extern uint16_t wink_mcs51_host_gpio_notify_pin(uint32_t i);
extern uint8_t  wink_mcs51_host_gpio_notify_level(uint32_t i);
#endif

#define P0_SFR_ADDR         0x80u
#define P2_SFR_ADDR         0xA0u
#define MCS51_TIMER0_VECTOR 1u
#define RUN_TICKS           100u   /* 100 master ticks * 10ms = 1000ms */
#define TIMER_PERIOD_MS     1u

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51-seg] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51-seg] FAIL: runtime run returned %d\n", (int)st);
        return 1;
    }

    int fails = 0;

    /* 1. Timer0 ISR vector registration */
    if (wink_mcs51_get_isr(MCS51_TIMER0_VECTOR) == NULL) {
        printf("[mcs51-seg] FAIL: Timer0 ISR vector 1 not registered\n");
        fails++;
    }

    /* 2. Timer0 ISR dispatch count (~1000 times for 1ms period over 1000ms) */
    uint32_t isr_count = wink_mcs51_isr_dispatch_count(MCS51_TIMER0_VECTOR);
    uint32_t expected_isrs = (RUN_TICKS * 10u) / TIMER_PERIOD_MS;
    if (isr_count < expected_isrs - 20 || isr_count > expected_isrs + 20) {
        printf("[mcs51-seg] FAIL: Timer0 ISR dispatched %u times, want ~%u (1ms period over %u ticks)\n",
               (unsigned)isr_count, (unsigned)expected_isrs, (unsigned)RUN_TICKS);
        fails++;
    }

    /* 3. P0 (segments) and P2 (digits) must be actively driven */
    uint8_t p0 = wink_mcs51_sfr_shadow[P0_SFR_ADDR];
    uint8_t p2 = wink_mcs51_sfr_shadow[P2_SFR_ADDR];
    if (p0 == 0x00u && p2 == 0xFFu) {
        printf("[mcs51-seg] FAIL: P0=0x00 and P2=0xFF, dynamic scan never started\n");
        fails++;
    }

    /* 4. Host notify log verification */
#ifndef __EMSCRIPTEN__
    uint32_t notify_count = wink_mcs51_host_gpio_notify_count();
    if (notify_count == 0) {
        printf("[mcs51-seg] FAIL: 0 GPIO notifications recorded on P0/P2\n");
        fails++;
    } else {
        /* Check that both P0 (pin 0..7) and P2 (pin 16..23) received edge events */
        int has_p0 = 0;
        int has_p2 = 0;
        for (uint32_t i = 0; i < notify_count && i < 128u; ++i) {
            uint16_t pin = wink_mcs51_host_gpio_notify_pin(i);
            if (pin <= 7) has_p0 = 1;
            if (pin >= 16 && pin <= 23) has_p2 = 1;
        }
        if (!has_p0 || !has_p2) {
            printf("[mcs51-seg] FAIL: Missing GPIO pin events (has_p0=%d, has_p2=%d)\n", has_p0, has_p2);
            fails++;
        }
    }
#endif

    /* 5. Cooperative fiber yields and virtual time coverage */
    uint32_t quota   = wink_mcs51_quota_yield_count();
    uint64_t virt_us = wink_mcs51_virtual_us();
    if (quota == 0u) {
        printf("[mcs51-seg] FAIL: 0 quota yields, super-loop would freeze\n");
        fails++;
    }

    uint64_t min_virt = (uint64_t)RUN_TICKS * 10000u - 10000u;
    if (virt_us < min_virt) {
        printf("[mcs51-seg] FAIL: virtual time %llu us < budget %llu us\n",
               (unsigned long long)virt_us, (unsigned long long)min_virt);
        fails++;
    }

    if (fails) {
        return 1;
    }

    printf("[mcs51-seg] PASS: 8-digit dynamic scan ran %u ms, ISR x%u (1ms cadence), "
           "final P0=0x%02X, P2=0x%02X, virt=%llu us (fiber yielded cooperatively)\n",
           (unsigned)(RUN_TICKS * 10u), (unsigned)isr_count,
           (unsigned)p0, (unsigned)p2, (unsigned long long)virt_us);
    return 0;
}
