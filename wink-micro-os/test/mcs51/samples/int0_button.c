/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 /INT0 interrupt-driven button -> LED — UNMODIFIED Keil C51 user source
 * (Stage 2 T3).
 *
 * The classic button-drives-LED board, interrupt variant: a push button on
 * P3.2 (/INT0, active-low) is configured for edge triggering (IT0 = 1); every
 * falling edge vectors the INT0 ISR (vector 0), which toggles the LED on P1.0.
 * The main loop NEVER reads the button pin — the LED can only change from
 * inside the ISR, so observing a toggle proves the external-interrupt model
 * latched IE0 on the falling edge and vectored vector 0 gated by EA+EX0.
 *
 * Edge semantics under test: a press held low across many super-loop
 * iterations toggles exactly once (level mode would re-request every slice);
 * release (rising edge) does nothing. The external level arrives through the
 * channel-1 read seam (js_pal_gpio_read_state), sampled by the functional
 * model at the microstep interception point. The cleanup pass emits a .cpp
 * copy in the build tree; this original is never edited in place.
 */
#include <REGX52.H>

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
