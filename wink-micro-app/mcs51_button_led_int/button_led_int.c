/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 interrupt-driven button-drives-LED — UNMODIFIED Keil C51 user source
 * (Stage 2 T3 headless proof).
 *
 * Production wasm-sim app, interrupt variant of mcs51_button_led: the push
 * button on P3.2 (/INT0, active-low, 0 = pressed) is configured for EDGE
 * triggering (IT0 = 1); every falling edge vectors the INT0 ISR (vector 0),
 * which toggles the LED on P1.0. The main loop NEVER reads the button — the LED
 * can only change from inside the ISR, so headless LED transitions prove the
 * functional external-interrupt model (mcs51_extint.cpp) sampled the button's
 * external level through the live PinArbiter (js_pal_gpio_read_state), latched
 * IE0 on the falling edge, and vectored vector 0 gated by EA+EX0.
 *
 * Edge semantics that distinguish this from the polled app: a press toggles
 * the LED exactly once; RELEASING the button does nothing (rising edge is not
 * an interrupt request in edge mode), so the LED stays on until the NEXT press
 * toggles it off. Built as a real production app (cleanup -> .cpp -> links
 * wink_mcs51_compat -> wink_simulator.{js,wasm}); this original is never
 * edited in place. Linear pins (ADR-0074 D3): KEY = P3.2 -> 26, LED =
 * P1.0 -> 8.
 */
#include <wink_mcu.h>

sbit LED = P1^0;    /* LED on P1.0, low-drive-on (0 = lit, 1 = off) */

void INT0_ISR(void) interrupt 0 {
    LED = !LED;     /* toggle on each falling edge of /INT0 */
}

void main(void) {
    LED = 1;        /* initial: LED off */
    IT0 = 1;        /* /INT0 edge-triggered (falling latches IE0) */
    EX0 = 1;        /* enable INT0 source */
    EA  = 1;        /* global interrupt enable */
    while (1) {
        _nop_();    /* cooperative microstep / external-sample point */
    }
}
