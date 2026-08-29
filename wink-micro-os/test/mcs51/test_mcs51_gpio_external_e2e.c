/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 channel-1 external Read-Pin end-to-end test (shared host + wasm/Node
 * driver): the UNMODIFIED Keil button->LED sample (samples/gpio_in_out.c) polls
 * a push button on P3.2 (/INT0, active-low) and drives an LED on P1.0.
 *
 * Unlike test_mcs51_gpio*.c — which injects the button by writing the P3.2
 * LATCH directly (the pre-data-plane M3 hack) — this test drives the button as
 * a real external driver through the channel-1 read seam: the SFR proxy's
 * Read-Pin path calls js_pal_gpio_read_state(linear pin), the same JS import
 * the production PinArbiter / button plugin uses. On host the compat library
 * fallback (mcs51_uni_bridge.cpp) holds a scriptable external level; under
 * emscripten the node JS library delegates to the exported getter below.
 *
 * Resolution order asserted implicitly: the sample never writes P3 (pure
 * input) and the external level is driven to a definite 0/1, so the LED must
 * follow it. HiZ (2) would fall back to the latch.
 *
 * Three phases across three repeated runtime runs (KEY = P3.2 = linear pin
 * 3*8+2 = 26; LED = P1.0 latch in SFR shadow 0x90 bit0):
 *   1. released (external HIGH, 1) -> LED off (latch 1)
 *   2. pressed  (external LOW,  0) -> LED on  (latch 0)
 *   3. released (external HIGH, 1) -> LED off (latch 1)
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
#define LED_BIT     0x01u   /* P1.0 */

#define KEY_PIN     26u     /* P3.2: port 3 * 8 + bit 2 */

/* JS_GPIO_STATE_* codes, mirrored from targets/wasm/wasm_bridge.h. */
#define EXT_STATE_LOW 0u
#define EXT_STATE_HIGH 1u
#define EXT_STATE_HIZ 2u

#define RUN_TICKS 30u

/* External button level for the current run (0 low/pressed, 1 high/released). */
static volatile uint8_t s_ext_key = EXT_STATE_HIZ;

#ifdef __EMSCRIPTEN__
/* Exported to JS: the node library's js_pal_gpio_read_state() calls back into
 * this to read the scripted external level (live, so cross-run changes show).
 * Kept alive by -sEXPORTED_FUNCTIONS on this test only. */
int mcs51_wasm_ext_pin_state(unsigned pin) {
    return (pin == KEY_PIN) ? (int)s_ext_key : (int)EXT_STATE_HIZ;
}
#else
/* Host: the compat fallback keeps its own external-level array; push the
 * scripted level there on every framework init so it survives the reset. */
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
        printf("[mcs51] FAIL: runtime run (%s) returned %d\n", label, (int)st);
        return 1;
    }
    uint8_t led_latch = (uint8_t)(wink_mcs51_sfr_shadow[P1_SFR_ADDR]
                                   & LED_BIT);
    if (led_latch != want_led) {
        printf("[mcs51] FAIL: %s — external KEY=%u but P1.0 latch=%u, want %u "
               "(channel-1 read seam not wired?)\n",
               label, (unsigned)ext_level, (unsigned)led_latch,
               (unsigned)want_led);
        return 1;
    }
    return 0;
}

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    mcs51_framework_set_post_init_hook(inject_ext);

    int fails = 0;
    fails += run_phase(cb, EXT_STATE_HIGH, 1u, "released");
    fails += run_phase(cb, EXT_STATE_LOW,  0u, "pressed");
    fails += run_phase(cb, EXT_STATE_HIGH, 1u, "re-released");

    mcs51_framework_set_post_init_hook(NULL);
#ifndef __EMSCRIPTEN__
    wink_mcs51_host_ext_pins_reset();
#endif

    if (fails) {
        return 1;
    }
    printf("[mcs51] PASS: external pin P3.2 button drives P1.0 LED via "
           "channel-1 read seam — released(1)->LED off, pressed(0)->LED on, "
           "released(1)->LED off (3x %u-tick runs)\n", (unsigned)RUN_TICKS);
    return 0;
}
