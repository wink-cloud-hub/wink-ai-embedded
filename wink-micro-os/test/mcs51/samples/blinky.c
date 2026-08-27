/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 blinky — UNMODIFIED Keil C51 user source (polling baseline, M1).
 *
 * This file is intentionally kept as a real Keil C51 translation unit: sbit,
 * code, `interrupt N using M`, RMW SFR access. The mcs51 cleanup pass emits a
 * .cpp copy in the build tree; this original is never edited in place.
 *
 * M1 scope: the Timer0 ISR is compiled and auto-registered (proving the
 * cleanup + WINK_ISR chain) but is not yet dispatched — timer interrupt
 * delivery arrives in M2. The polling super-loop exercises SFR proxies and the
 * _nop_() microstep so the fiber yields cooperatively.
 */
#include <REGX52.H>

sbit LED = P1^0;

unsigned char code seg_table[] = { 0x3F, 0x06, 0x5B, 0x4F };

void Timer0_ISR(void) interrupt 1 using 1 {
    LED = !LED;
}

void main(void) {
    P1 = 0x00;
    while (1) {
        P1 |= 0x01;          /* RMW set     */
        if (P1) {            /* read pin    */
            P1 = 0x55;       /* whole write */
        }
        _nop_();             /* microstep / cooperative yield point */
    }
}
