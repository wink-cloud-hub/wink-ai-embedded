/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 M1 wasm test: drive the cleaned blinky through the cooperative
 * runtime under emscripten fibers (ASYNCIFY) and assert ISR registration plus
 * SFR proxy writes, mirroring the host test. Built by the emcc+Node harness
 * (test/mcs51/wasm/add_wink_wasm_mcs51_test.cmake); no Unity — own main().
 */
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);
extern uint8_t wink_mcs51_sfr_shadow[256];
extern void (*wink_mcs51_get_isr(uint8_t vector_num))(void);

#define P1_SFR_ADDR 0x90u
#define MCS51_TIMER0_VECTOR 1u
#define RUN_TICKS 200u

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51-wasm] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51-wasm] FAIL: runtime run returned %d\n", (int)st);
        return 1;
    }

    int fails = 0;
    if (wink_mcs51_get_isr(MCS51_TIMER0_VECTOR) == NULL) {
        printf("[mcs51-wasm] FAIL: Timer0 ISR vector 1 not registered\n");
        fails++;
    }
    uint8_t p1 = wink_mcs51_sfr_shadow[P1_SFR_ADDR];
    if (p1 != 0x55u) {
        printf("[mcs51-wasm] FAIL: P1 shadow = 0x%02X, want 0x55\n", (unsigned)p1);
        fails++;
    }

    if (fails) {
        return 1;
    }
    printf("[mcs51-wasm] PASS: blinky ran %u ticks under emscripten fiber, "
           "ISR vector1 registered, P1=0x%02X\n",
           (unsigned)RUN_TICKS, (unsigned)p1);
    return 0;
}
