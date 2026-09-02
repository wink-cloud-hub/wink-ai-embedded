/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 button-drives-LED — UNMODIFIED Keil C51 user source (ADR-0075).
 *
 * Production wasm-sim app: the classic 89C52 poll loop. A push button on
 * P3.2 (/INT0, active-low, 0 = pressed) is read each super-loop iteration;
 * while pressed the LED on P1.0 is driven on (low-drive-on, 0 = lit), else
 * off. There is no ISR and no peripheral setup.
 *
 * This is the same logic as test/mcs51/samples/gpio_in_out.c, but built as a
 * real production app: the cleanup pass (mcs51_cleanup.py) emits a .cpp copy in
 * the build tree that links against wink_mcs51_compat, and the resulting
 * wink_simulator.{js,wasm} is driven headlessly by UniSim with a real
 * PinArbiter + button/led plugins. This original is never edited in place.
 * Linear pins (ADR-0074 D3): KEY = P3.2 -> 26, LED = P1.0 -> 8.
 */
#include <wink_mcu.h>

sbit KEY = P3^2;    /* push button on P3.2 / INT0, active-low (0 = pressed) */
sbit LED = P1^0;    /* LED on P1.0, low-drive-on (0 = lit, 1 = off)          */

void main(void) {
    LED = 1;            /* initial: LED off */
    while (1) {
        if (KEY == 0) {
            LED = 0;    /* pressed  -> LED on  */
        } else {
            LED = 1;    /* released -> LED off */
        }
        _nop_();        /* microstep / cooperative yield point */
    }
}
