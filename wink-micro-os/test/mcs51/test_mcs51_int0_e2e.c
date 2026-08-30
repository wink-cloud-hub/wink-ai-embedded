/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 /INT0 external-interrupt end-to-end test (shared host + wasm/Node
 * driver, Stage 2 T3): the UNMODIFIED Keil interrupt-driven button->LED sample
 * (samples/int0_button.c) configures IT0=1/EX0=1/EA=1 and toggles the P1.0 LED
 * from inside the INT0 ISR (vector 0). The main loop never reads the button —
 * an LED toggle proves the functional ext-int model sampled a falling edge on
 * P3.2 via the channel-1 read seam (js_pal_gpio_read_state), latched IE0, and
 * vectored vector 0 gated by EA+EX0, then hardware-cleared IE0.
 *
 * The button is driven as a real external level: host via the compat fallback
 * seam (mcs51_uni_bridge.cpp), wasm via the node library's callback into the
 * exported getter below. Each runtime run re-enters main() (LED re-initialised
 * to off); the edge baseline is world state kept across framework inits, so a
 * level held/changed between runs is seen as an edge on the first in-run
 * sample.
 *
 * Phases (KEY = P3.2 = linear pin 26; LED = P1.0 latch, SFR 0x90 bit0;
 * active-low button, low-drive-on LED so latch 1 = off / 0 = lit). Each run
 * re-enters main() which re-initialises LED to off (1), so the assertion is
 * "did a falling edge get sampled THIS run": a vectored edge toggles LED on
 * (0); no edge leaves it at the init value (1). Edge-mode hold/rising
 * no-re-trigger semantics per se are covered by the model unit test
 * (test_extint_model.cpp cases A/C); here the held-low and rising runs prove
 * no SPURIOUS edge fires across a re-init:
 *   1. released (HIGH) -> no edge      -> LED off (1)
 *   2. pressed  (LOW)  -> falling edge -> toggled on (0)
 *   3. still LOW across re-init        -> no new edge -> stays off (1)
 *   4. released (HIGH) -> rising edge  -> no edge -> stays off (1)
 *   5. pressed  (LOW)  -> falling edge -> toggled on (0)
 */
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"
#include "mcs51_trap.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

/* SFR shadow (C linkage, framework BSS; NOT reset across runs). P1 = 0x90. */
extern uint8_t wink_mcs51_sfr_shadow[256];

#define P1_SFR_ADDR 0x90u
#define LED_BIT     0x01u   /* P1.0; latch 1 = off, 0 = lit */

#define KEY_PIN     26u     /* P3.2 /INT0: port 3 * 8 + bit 2 */

/* JS_GPIO_STATE_* codes, mirrored from targets/wasm/wasm_bridge.h. */
#define EXT_STATE_LOW  0u
#define EXT_STATE_HIGH 1u
#define EXT_STATE_HIZ  2u

#define RUN_TICKS 30u

/* External button level for the current run (0 low/pressed, 1 high/released). */
static volatile uint8_t s_ext_key = EXT_STATE_HIGH;

#ifdef __EMSCRIPTEN__
/* Exported to JS: the node library's js_pal_gpio_read_state() calls back into
 * this to read the scripted external level (live). Kept alive by
 * -sEXPORTED_FUNCTIONS on this test only. */
int mcs51_wasm_ext_pin_state(unsigned pin) {
    return (pin == KEY_PIN) ? (int)s_ext_key : (int)EXT_STATE_HIZ;
}
#else
/* Host: push the scripted level into the compat fallback on each framework
 * init so it survives the run reset. */
void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state);
void wink_mcs51_host_ext_pins_reset(void);
#endif

static void inject_ext(void) {
#ifdef __EMSCRIPTEN__
    /* wasm: the JS stub pulls s_ext_key live via mcs51_wasm_ext_pin_state(). */
#else
    wink_mcs51_host_set_ext_pin((uint16_t)KEY_PIN, s_ext_key);
#endif
}

void setUp(void) {}
void tearDown(void) {}

static int run_phase(const wink_app_callbacks_t *cb, uint8_t ext_level,
                     uint8_t want_led, const char *label) {
    s_ext_key = ext_level;
    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51-int] FAIL: runtime run (%s) returned %d\n",
               label, (int)st);
        return 1;
    }
    uint8_t led_latch = (uint8_t)(wink_mcs51_sfr_shadow[P1_SFR_ADDR]
                                  & LED_BIT);
    if (led_latch != want_led) {
        printf("[mcs51-int] FAIL: %s — external KEY=%u but P1.0 latch=%u, "
               "want %u (INT0 edge not vectored?)\n",
               label, (unsigned)ext_level, (unsigned)led_latch,
               (unsigned)want_led);
        return 1;
    }
    return 0;
}

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51-int] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    mcs51_framework_set_post_init_hook(inject_ext);

    int fails = 0;
    fails += run_phase(cb, EXT_STATE_HIGH, 1u, "released");     /* no edge */
    fails += run_phase(cb, EXT_STATE_LOW,  0u, "pressed");      /* edge -> on */
    fails += run_phase(cb, EXT_STATE_LOW,  1u, "held-across-reinit"); /* none */
    fails += run_phase(cb, EXT_STATE_HIGH, 1u, "released");     /* rising: no */
    fails += run_phase(cb, EXT_STATE_LOW,  0u, "pressed-again"); /* edge -> on */

    mcs51_framework_set_post_init_hook(NULL);
#ifndef __EMSCRIPTEN__
    wink_mcs51_host_ext_pins_reset();
#endif

    if (fails) {
        return 1;
    }
    printf("[mcs51-int] PASS: /INT0 edge-triggered button toggles P1.0 LED via "
           "vector 0 — press toggles once, held-low does not re-trigger, "
           "release is a no-op (5x %u-tick runs)\n", (unsigned)RUN_TICKS);
    return 0;
}
