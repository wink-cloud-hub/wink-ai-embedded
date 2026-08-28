/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 M4 end-to-end test (shared host + wasm/Node driver): a real Keil
 * bit-bang program (samples/adc0832_read.c) drives the 3-wire DIO ADC0832
 * (CS=P1.2, CLK=P1.1, DIO=P1.0) through the Level-2 instant trap state
 * machine, and the converted bytes come from the channel-3 injection rail.
 *
 * The sample reads CH0 and CH1 once and stores them in XDATA[0x10]/[0x11].
 * We bind the ADC0832 traps through the framework post-init hook (so they
 * survive framework init's mcs51_trap_reset), inject 0xA5 on CH0 and 0x5A on
 * CH1, run the super-loop, and assert the exact bytes. This closes the loop
 * user-code -> sbit/SFR proxy -> pin traps -> ADC0832 FSM -> mcs51_adc rail,
 * i.e. acceptance: "sbit 引脚位翻转驱动的 ADC0832 闭环读取相同 8 位转换值".
 */
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"
#include "ADC0832.H"
#include "mcs51_adc.h"
#include "mcs51_trap.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

/* XDATA shadow (C linkage, framework BSS) for the sample's XBYTE results. */
extern uint8_t wink_mcs51_xdata_shadow[65536];

#define RUN_TICKS 20u

/* Board wiring: CS=P1.2, CLK=P1.1, DI=DO=P1.0 (3-wire shared DIO). Runs
 * AFTER framework init (which does mcs51_trap_reset + mcs51_adc_reset), so the
 * traps survive and the injection is not wiped. */
static void bind_adc0832(void) {
    mcs51_adc0832_init(/*CS*/1,2, /*CLK*/1,1, /*DI*/1,0, /*DO*/1,0);
    mcs51_adc0832_set_value(0, 0xA5);   /* deterministic CH0/CH1 values */
    mcs51_adc0832_set_value(1, 0x5A);
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    mcs51_framework_set_post_init_hook(bind_adc0832);

    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51] FAIL: runtime run returned %d\n", (int)st);
        return 1;
    }

    int fails = 0;
    uint8_t ch0 = wink_mcs51_xdata_shadow[0x0010u];
    uint8_t ch1 = wink_mcs51_xdata_shadow[0x0011u];
    if (ch0 != 0xA5u) {
        printf("[mcs51] FAIL: ADC0832 CH0 read 0x%02X, want 0xA5\n",
               (unsigned)ch0);
        fails++;
    }
    if (ch1 != 0x5Au) {
        printf("[mcs51] FAIL: ADC0832 CH1 read 0x%02X, want 0x5A\n",
               (unsigned)ch1);
        fails++;
    }

    mcs51_framework_set_post_init_hook(NULL);

    if (fails) {
        return 1;
    }
    printf("[mcs51] PASS: ADC0832 3-wire DIO end-to-end — Keil bit-bang read "
           "CH0=0xA5 CH1=0x5A via instant trap FSM + channel-3 rail\n");
    return 0;
}
