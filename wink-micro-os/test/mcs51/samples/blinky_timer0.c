/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 blinky — UNMODIFIED Keil C51 user source (Timer0 ISR edition, M2).
 *
 * Classic 89C52 Timer0 periodic interrupt: mode-1 (16-bit), 12 MHz teaching
 * crystal, 50 ms overflow (reload 65536 - 50000 = 15536 = 0x3CB0). The ISR
 * reloads TH0/TL0 and toggles LED (P1.0); the super-loop is idle — all LED
 * activity is driven by virtual timer overflows dispatched through catch-up
 * (ADR-0072). The cleanup pass emits a .cpp copy; this original is never
 * edited in place.
 */
#include <REGX52.H>

sbit LED = P1^0;

void Timer0_Init(void) {
    TMOD &= 0xF0;   /* clear Timer0 control nibble */
    TMOD |= 0x01;   /* Timer0 mode 1 (16-bit), internal clock */
    TH0 = 0x3C;     /* 50 ms reload @ 12 MHz (1 count = 1 us) */
    TL0 = 0xB0;
    ET0 = 1;        /* enable Timer0 interrupt */
    EA  = 1;        /* global interrupt enable */
    TR0 = 1;        /* start Timer0 */
}

void Timer0_ISR(void) interrupt 1 {
    TH0 = 0x3C;     /* reload */
    TL0 = 0xB0;
    LED = !LED;     /* toggle every 50 ms -> 100 ms full period */
}

void main(void) {
    P1 = 0x00;
    Timer0_Init();
    while (1) {
        _nop_();    /* idle loop; interception point keeps the fiber yielding */
    }
}
