/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 M5 end-to-end test (shared host + wasm/Node driver): a vendor-style
 * Keil polled program (samples/cms8s_adc_test.c) drives the CMS8S78xx
 * on-chip 12-bit ADC through the REAL register map (ADCON0/ADCON1/ADCCHS/
 * ADRESH/ADRESL), and the converted codes come from the channel-3 injection
 * rail (12-bit, M5).
 *
 * The sample converts AN0 right-justify (-> XDATA 0x10/0x11), AN1
 * left-justify (-> 0x12/0x13), AN25 right-justify (-> 0x14/0x15). We inject
 * 0xABC / 0x801 / 0xFFF through the framework post-init hook (survives the
 * framework-init rail reset), run the super-loop, and recombine the result
 * bytes. This closes the loop: user-code -> SFR proxy -> ADCON0 write hook ->
 * 0-cycle instant model -> ADRESH/ADRESL packing per ADFM.
 */
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"
#include "mcs51_adc.h"
#include "mcs51_trap.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

/* XDATA shadow (C linkage, framework BSS) for the sample's XBYTE results. */
extern uint8_t wink_mcs51_xdata_shadow[65536];

#define RUN_TICKS 20u

/* Runs AFTER framework init (mcs51_trap_reset + mcs51_adc_reset), so the
 * injections survive. */
static void inject_cms8s_channels(void) {
    mcs51_adc_set_value(0,  0x0ABCu);
    mcs51_adc_set_value(1,  0x0801u);
    mcs51_adc_set_value(25, 0x0FFFu);
}

static int check_pair(uint16_t addr, uint16_t want, const char *tag) {
    uint16_t got = (uint16_t)(((uint16_t)wink_mcs51_xdata_shadow[addr] << 8) |
                              wink_mcs51_xdata_shadow[addr + 1u]);
    if (got != want) {
        printf("[mcs51] FAIL: CMS8S ADC %s read 0x%03X, want 0x%03X\n",
               tag, (unsigned)got, (unsigned)want);
        return 1;
    }
    return 0;
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    mcs51_framework_set_post_init_hook(inject_cms8s_channels);

    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51] FAIL: runtime run returned %d\n", (int)st);
        return 1;
    }

    int fails = 0;
    fails += check_pair(0x0010u, 0x0ABCu, "AN0 right-justify");
    fails += check_pair(0x0012u, 0x0801u, "AN1 left-justify");
    fails += check_pair(0x0014u, 0x0FFFu, "AN25 right-justify");

    mcs51_framework_set_post_init_hook(NULL);

    if (fails) {
        return 1;
    }
    printf("[mcs51] PASS: CMS8S78xx on-chip ADC end-to-end — vendor polled "
           "read AN0=0xABC (right), AN1=0x801 (left), AN25=0xFFF (right) "
           "via 0-cycle ADCON0 hook + 12-bit rail\n");
    return 0;
}
