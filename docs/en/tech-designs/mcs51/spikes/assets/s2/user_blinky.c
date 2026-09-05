/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Spike-S2 fixture: UNMODIFIED Keil C51 user source. This file must compile
 * as-is (after the CMake regex cleanup of `interrupt N`) on GCC / MSVC / emcc
 * in C++17 mode. Dialect is eaten by REGX52.H macros + C++ proxies; the ONLY
 * automatic transform is interrupt-syntax cleanup performed on a COPY in the
 * build dir (this source is never edited in place).
 */
#include <REGX52.H>

/* sbit: bit-level SFR access via WinkSfrBitProxy (operator^) */
sbit LED = P1^0;

/* `code` storage qualifier -> const (read-only table in .rodata) */
unsigned char code seg_table[] = { 0x3F, 0x06, 0x5B, 0x4F };

/* Keil ISR: `void name(void) interrupt N [using B]` -> regex -> WINK_ISR(N) */
void Timer0_ISR(void) interrupt 1 using 1
{
    LED = !LED;            /* bit toggle through proxy */
}

void main(void)
{
    P1 = 0x00;             /* whole-port write */
    while (1) {
        P1 |= 0x01;        /* RMW read-latch op */
        if (P1) {          /* whole-port read */
            P1 = 0x55;
        }
    }
}
