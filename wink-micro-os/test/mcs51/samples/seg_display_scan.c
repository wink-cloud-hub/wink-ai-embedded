/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 8-digit multiplexed 7-segment display scanner.
 * UNMODIFIED Keil C51 user source (Part 2 validation firmware).
 *
 * Hardware connections (AT89C52 typical teaching dev board):
 *   - P0 (P0.0..P0.7) -> 7-segment segment bus A, B, C, D, E, F, G, DP
 *   - P2 (P2.0..P2.7) -> Digit select DIG1..DIG8 (active-low drive)
 *
 * Display target: "12345678" across 8 digits.
 * Scanning architecture:
 *   - Timer0 mode-1 1ms periodic interrupt (TH0=0xFC, TL0=0x18 @ 12MHz).
 *   - Dynamic round-robin digit commutation with teaching-grade blanking.
 *   - When ENABLE_BLANKING=1: P2 is turned off before P0 changes (anti-ghosting).
 *   - When ENABLE_BLANKING=0: P0 updates while old digit is active, producing ghosting.
 */
#include <REGX52.H>
#include <intrins.h>

#ifndef ENABLE_BLANKING
#define ENABLE_BLANKING 1
#endif

/* Common cathode 0-9 segment lookup table (A=bit0 .. DP=bit7) */
unsigned char code SEG_TABLE[10] = {
    0x3F, /* 0: A+B+C+D+E+F */
    0x06, /* 1: B+C */
    0x5B, /* 2: A+B+D+E+G */
    0x4F, /* 3: A+B+C+D+G */
    0x66, /* 4: B+C+F+G */
    0x6D, /* 5: A+C+D+F+G */
    0x7D, /* 6: A+C+D+E+F+G */
    0x07, /* 7: A+B+C */
    0x7F, /* 8: A+B+C+D+E+F+G */
    0x6F  /* 9: A+B+C+D+F+G */
};

/* 8-digit display buffer holding '1', '2', '3', '4', '5', '6', '7', '8' */
unsigned char display_buf[8] = {1, 2, 3, 4, 5, 6, 7, 8};

/* Commutator state index 0..7 */
unsigned char current_digit = 0;

void Timer0_Init(void) {
    TMOD &= 0xF0; /* Clear Timer0 mode bits */
    TMOD |= 0x01; /* Timer0 mode 1 (16-bit timer) */
    TH0 = 0xFC;   /* 1ms reload at 12MHz: 65536 - 1000 = 64536 = 0xFC18 */
    TL0 = 0x18;
    ET0 = 1;      /* Enable Timer0 interrupt */
    EA  = 1;      /* Global interrupt enable */
    TR0 = 1;      /* Start Timer0 */
}

void Timer0_ISR(void) interrupt 1 {
    TH0 = 0xFC;   /* Reload 1ms */
    TL0 = 0x18;

#if ENABLE_BLANKING
    /* Step 1: Blank old digit by de-asserting all digits (active-low -> 0xFF) */
    P2 = 0xFF;
#endif

    /* Step 2: Put segment pattern for the target digit on segment bus P0 */
    P0 = SEG_TABLE[display_buf[current_digit]];

#if !ENABLE_BLANKING
    /* Inverted/improper timing fault: hold segments while old digit remains active */
    _nop_();
    _nop_();
#endif

    /* Step 3: Activate the new digit (active-low bit mask: ~(1 << current_digit)) */
    P2 = (unsigned char)~(1 << current_digit);

    /* Advance commutator to next digit (wrap around 0..7) */
    current_digit = (current_digit + 1) & 0x07;
}

void main(void) {
    /* Initial state: all segments dark (P0=0x00), all digits inactive (P2=0xFF) */
    P0 = 0x00;
    P2 = 0xFF;

    Timer0_Init();

    while (1) {
        _nop_(); /* Cooperative fiber yield point */
    }
}
