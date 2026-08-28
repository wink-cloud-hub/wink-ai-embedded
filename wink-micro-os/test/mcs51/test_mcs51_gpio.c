/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 M3 host test: GPIO in->out synchronisation (mcu-compat-plan §3.10
 * item 3: "P3 按键输入 -> P1 LED 输出，验证 sync").
 *
 * The unmodified Keil sample (gpio_in_out.c) polls a push button on P3.2
 * (/INT0, active-low) each super-loop iteration and drives an LED on P1.0
 * (pressed -> LED on / latch 0, released -> LED off / latch 1).
 *
 * The functional M3 model has no external pin-injection data plane yet (that is
 * M4's read-pin plane, ADR-0071), so the "button" is driven by writing the P3
 * latch bit directly in the SFR shadow — the agreed M3-level injection. The
 * Keil read of KEY observes that latch and the Keil write of LED lands in the
 * P1 latch we read back.
 *
 * Repeated-run flow: wink_runtime_run() can be called multiple times in one
 * process. Each call (a) runs the framework init callback, which resets the
 * clock/timer/uart/xdata models, and (b) registers a FRESH app fiber; the
 * previously-suspended super-loop fiber is torn down by sim_scheduler_reset()
 * at the top of the next run (Win32 DeleteFiber / emscripten free of the
 * suspended fiber). The new fiber restarts wink_mcs51_user_main from its first
 * instruction (LED = 1; then the loop). The SFR shadow is BSS and is NOT reset
 * by framework init, so the button state injected between runs persists. That
 * lets us exercise all three phases (released -> pressed -> released) in one
 * process. ctest gates on the process exit code.
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

void setUp(void) {}
void tearDown(void) {}

/* Drive the simulated button: 1 = released (active-high latch), 0 = pressed. */
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
        printf("[mcs51] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    int fails = 0;

    /* Phase 1: button RELEASED before/through the run -> LED off (latch 1). */
    set_key(1u);
    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51] FAIL: runtime run (released) returned %d\n", (int)st);
        return 1;
    }
    uint8_t led1 = read_led();
    if (led1 != 1u) {
        printf("[mcs51] FAIL: key released but P1.0=%u, want 1 (LED off)\n",
               (unsigned)led1);
        fails++;
    }

    /* Phase 2: button PRESSED -> LED on (latch 0). */
    set_key(0u);
    st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51] FAIL: runtime run (pressed) returned %d\n", (int)st);
        return 1;
    }
    uint8_t led2 = read_led();
    if (led2 != 0u) {
        printf("[mcs51] FAIL: key pressed but P1.0=%u, want 0 (LED on)\n",
               (unsigned)led2);
        fails++;
    }

    /* Phase 3: button RELEASED again -> LED off (latch 1). */
    set_key(1u);
    st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51] FAIL: runtime run (re-release) returned %d\n", (int)st);
        return 1;
    }
    uint8_t led3 = read_led();
    if (led3 != 1u) {
        printf("[mcs51] FAIL: key re-released but P1.0=%u, want 1 (LED off)\n",
               (unsigned)led3);
        fails++;
    }

    if (fails) {
        return 1;
    }
    printf("[mcs51] PASS: GPIO in->out sync — P3.2 key released(1)->P1.0=1, "
           "pressed(0)->P1.0=0, released(1)->P1.0=1 (3x %u-tick runs)\n",
           (unsigned)RUN_TICKS);
    return 0;
}
