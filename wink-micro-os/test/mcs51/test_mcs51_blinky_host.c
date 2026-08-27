/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 M1 host test: drive the cleaned blinky sample through the real
 * cooperative runtime and assert (a) the Timer0 ISR auto-registered via
 * WINK_ISR, and (b) the SFR proxy reflects the user program's writes while the
 * fiber yields cooperatively (no freeze). Bounded by max_ticks.
 */
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"

/* Provided by the mcs51 compat layer (weak/selectany). */
extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

/* C-ABI SFR shadow + ISR table (boundaries ②/③). */
extern uint8_t wink_mcs51_sfr_shadow[256];
extern void (*wink_mcs51_get_isr(uint8_t vector_num))(void);

#define P1_SFR_ADDR 0x90u
#define MCS51_TIMER0_VECTOR 1u
#define RUN_TICKS 200u

/* Unity is linked by the host-test helper but this driver uses its own main;
 * supply the no-op fixtures Unity's runner expects. */
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

    /* (a) Timer0 ISR (vector 1) auto-registered before/at runtime start. */
    if (wink_mcs51_get_isr(MCS51_TIMER0_VECTOR) == NULL) {
        printf("[mcs51] FAIL: Timer0 ISR vector 1 not registered\n");
        fails++;
    }

    /* (b) blinky drives P1 to 0x55 each super-loop pass via the SFR proxy. */
    uint8_t p1 = wink_mcs51_sfr_shadow[P1_SFR_ADDR];
    if (p1 != 0x55u) {
        printf("[mcs51] FAIL: P1 shadow = 0x%02X, want 0x55\n", (unsigned)p1);
        fails++;
    }

    if (fails) {
        return 1;
    }
    printf("[mcs51] PASS: blinky ran %u ticks, ISR vector1 registered, "
           "P1=0x%02X (fiber yielded cooperatively, no freeze)\n",
           (unsigned)RUN_TICKS, (unsigned)p1);
    return 0;
}
