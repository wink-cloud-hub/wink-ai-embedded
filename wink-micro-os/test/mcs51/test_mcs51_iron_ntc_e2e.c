/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 M6 end-to-end test (shared host + wasm/Node driver): a closed-loop
 * NTC thermostat (samples/iron_ntc.c) reads an external ADC0832 through the
 * board-codegen seam and drives a heater/relay on P1.0, with open/short
 * sensor safety states.
 *
 * This driver proves the codegen seam end to end: it does NOT call
 * mcs51_adc0832_init() itself. The framework bridge (mcs51_bridge.cpp) binds
 * the ADC0832 to the codegen-generated pins (CS=P2.0, CLK=P2.1, DIO=P2.2 from
 * mcs51_board_config.h -> MCS51_HAS_ADC0832) during framework init. The
 * post-init hook — which runs AFTER that binding and after mcs51_adc_reset —
 * only injects the NTC code onto the channel-3 rail. If the seam were not
 * wired, every read would return 0 (short) and the cold phase would fail.
 *
 * Four phases across four repeated runtime runs:
 *   1. cold  (code 200 -> ~80 C < 180): heater ON,  no fault
 *   2. hot   (code  20 -> ~300 C > 180): heater OFF, no fault
 *   3. open  (code 255 >= 250):          heater OFF, fault=1 (safe state)
 *   4. short (code   0 <= 8):            heater OFF, fault=2 (safe state)
 */
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"
#include "mcs51_adc.h"
#include "mcs51_trap.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

/* SFR shadow (C linkage, framework BSS; NOT reset across runs). P1 = 0x90. */
extern uint8_t wink_mcs51_sfr_shadow[256];
/* XDATA shadow (C linkage; reset each run; the sample rewrites it every loop). */
extern uint8_t wink_mcs51_xdata_shadow[65536];

#define P1_SFR_ADDR  0x90u
#define HEATER_BIT   0x01u   /* P1.0 */

#define TLM_CODE    0x0010u
#define TLM_HEATER  0x0011u
#define TLM_FAULT   0x0012u

#define RUN_TICKS 20u

/* NTC code to inject for the current run; read by the post-init hook. */
static volatile uint8_t s_inject_code = 0u;

/* Runs on EVERY wink_runtime_run(), after framework init binds the codegen
 * ADC0832 and after mcs51_adc_reset() wipes the rail — so the injection
 * survives into the super-loop. */
static void inject_ntc(void) {
    mcs51_adc0832_set_value(0, s_inject_code);
}

void setUp(void) {}
void tearDown(void) {}

static int run_phase(const wink_app_callbacks_t *cb, uint8_t code,
                     uint8_t want_heater, uint8_t want_fault,
                     const char *label) {
    s_inject_code = code;
    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51] FAIL: runtime run (%s) returned %d\n", label, (int)st);
        return 1;
    }

    uint8_t heater_latch = (uint8_t)(wink_mcs51_sfr_shadow[P1_SFR_ADDR]
                                     & HEATER_BIT);
    uint8_t tlm_code   = wink_mcs51_xdata_shadow[TLM_CODE];
    uint8_t tlm_heater = wink_mcs51_xdata_shadow[TLM_HEATER];
    uint8_t tlm_fault  = wink_mcs51_xdata_shadow[TLM_FAULT];

    int fails = 0;
    if (tlm_code != code) {
        printf("[mcs51] FAIL: %s — read code 0x%02X, want 0x%02X (codegen "
               "ADC0832 seam not wired?)\n", label, (unsigned)tlm_code,
               (unsigned)code);
        fails++;
    }
    if (heater_latch != want_heater || tlm_heater != want_heater) {
        printf("[mcs51] FAIL: %s — heater latch=%u tlm=%u, want %u\n",
               label, (unsigned)heater_latch, (unsigned)tlm_heater,
               (unsigned)want_heater);
        fails++;
    }
    if (tlm_fault != want_fault) {
        printf("[mcs51] FAIL: %s — fault=%u, want %u\n", label,
               (unsigned)tlm_fault, (unsigned)want_fault);
        fails++;
    }
    return fails;
}

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    mcs51_framework_set_post_init_hook(inject_ntc);

    int fails = 0;
    fails += run_phase(cb, 200u, 1u, 0u, "cold  80C");
    fails += run_phase(cb,  20u, 0u, 0u, "hot  300C");
    fails += run_phase(cb, 255u, 0u, 1u, "open");
    fails += run_phase(cb,   0u, 0u, 2u, "short");

    mcs51_framework_set_post_init_hook(NULL);

    if (fails) {
        return 1;
    }
    printf("[mcs51] PASS: iron_ntc closed-loop via codegen ADC0832 seam — "
           "cold(200)->heater ON, hot(20)->OFF, open(255)->fault1 safe, "
           "short(0)->fault2 safe\n");
    return 0;
}
