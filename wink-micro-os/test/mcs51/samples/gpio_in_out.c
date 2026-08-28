/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 GPIO in->out — UNMODIFIED Keil C51 user source (M3).
 *
 * Classic 89C52 button-drives-LED poll loop (mcu-compat-plan §3.10 item 3:
 * "P3 按键输入 -> P1 LED 输出，验证 sync"): a push button on P3.2 (/INT0) is
 * read each super-loop iteration; when pressed (active-low, as on the classic
 * dev boards) the LED on P1.0 is turned on, otherwise off. There is no ISR and
 * no peripheral setup — the synchronisation under test is the functional
 * model's read-latch/write-latch path: a value the test injects into the P3 SFR
 * latch is observed by the Keil read of KEY, and the Keil write of LED lands in
 * the P1 latch the test reads back.
 *
 * M3 scope note: the functional model has no external pin-injection data plane
 * yet (that is M4's read-pin plane, ADR-0071); the "button" is driven by
 * writing the P3.2 latch bit directly. Read-Pin vs Read-Latch distinction and
 * RMW pin isolation likewise arrive in M4. The cleanup pass emits a .cpp copy
 * in the build tree; this original is never edited in place. This sample has no
 * ISR, so the cleanup pass is near-identity.
 */
#include <REGX52.H>

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
