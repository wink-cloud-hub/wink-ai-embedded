/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 M3 wasm test: GPIO in->out synchronisation under emscripten fibers
 * (ASYNCIFY), mirroring test/mcs51/test_mcs51_gpio.c. The unmodified Keil
 * sample polls a push button on P3.2 (/INT0, active-low) and drives an LED on
 * P1.0 (pressed -> LED on / latch 0, released -> LED off / latch 1).
 *
 * The functional M3 model has no external pin-injection data plane yet (M4,
 * ADR-0071), so the button is driven by writing the P3.2 latch bit directly in
 * the SFR shadow — the agreed M3-level injection. Repeated wink_runtime_run()
 * calls work under emscripten too: each run resets the clock/timer/uart models
 * and registers a fresh app fiber (the prior suspended fiber is torn down by
 * sim_scheduler_reset), restarting the user super-loop from the top; the SFR
 * shadow is BSS and persists, so the injected button state survives. Assertions
 * run in C (ctest gates on the exit code under Node, not on text matching).
 * Built by test/mcs51/wasm/add_wink_wasm_mcs51_test.cmake; no Unity — own main.
 */
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

/* SFR shadow (C linkage, defined in mcs51_proxy.cpp). Index = SFR address. */
extern uint8_t wink_mcs51_sfr_shadow[256];

#define P3_SFR_ADDR 0xB0u   /* P3 port */
#define P1_SFR_ADDR 0x90u   /* P1 port */
#define KEY_BIT     0x04u   /* P3.2 / INT0 */
#define LED_BIT     0x01u   /* P1.0 */

#define RUN_TICKS   30u     /* 30 master ticks * 10 ms = 300 ms virtual/phase */

/* Drive the simulated button: 1 = released, 0 = pressed (active-low). */
static void set_key(uint8_t released) {
    if (released) {
        wink_mcs51_sfr_shadow[P3_SFR_ADDR] |= KEY_BIT;
    } else {
        wink_mcs51_sfr_shadow[P3_SFR_ADDR] &= (uint8_t)(~KEY_BIT & 0xFFu);
    }
}

static uint8_t read_led(void) {
    return (uint8_t)(wink_mcs51_sfr_shadow[P1_SFR_ADDR] & LED_BIT);
}

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51-wasm] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    int fails = 0;

    /* Phase 1: button RELEASED -> LED off (latch 1). */
    set_key(1u);
    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51-wasm] FAIL: runtime run (released) returned %d\n", (int)st);
        return 1;
    }
    uint8_t led1 = read_led();
    if (led1 != 1u) {
        printf("[mcs51-wasm] FAIL: key released but P1.0=%u, want 1 (LED off)\n",
               (unsigned)led1);
        fails++;
    }

    /* Phase 2: button PRESSED -> LED on (latch 0). */
    set_key(0u);
    st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51-wasm] FAIL: runtime run (pressed) returned %d\n", (int)st);
        return 1;
    }
    uint8_t led2 = read_led();
    if (led2 != 0u) {
        printf("[mcs51-wasm] FAIL: key pressed but P1.0=%u, want 0 (LED on)\n",
               (unsigned)led2);
        fails++;
    }

    /* Phase 3: button RELEASED again -> LED off (latch 1). */
    set_key(1u);
    st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51-wasm] FAIL: runtime run (re-release) returned %d\n", (int)st);
        return 1;
    }
    uint8_t led3 = read_led();
    if (led3 != 1u) {
        printf("[mcs51-wasm] FAIL: key re-released but P1.0=%u, want 1 (LED off)\n",
               (unsigned)led3);
        fails++;
    }

    if (fails) {
        return 1;
    }
    printf("[mcs51-wasm] PASS: GPIO in->out sync — P3.2 key released(1)->P1.0=1, "
           "pressed(0)->P1.0=0, released(1)->P1.0=1 (3x %u-tick runs, node)\n",
           (unsigned)RUN_TICKS);
    return 0;
}
