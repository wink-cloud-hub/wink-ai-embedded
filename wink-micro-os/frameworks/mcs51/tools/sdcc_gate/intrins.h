/* SPDX-License-Identifier: LGPL-3.0-only
 * SDCC compile-gate intrins adapter (GAP-03, Tier-S).
 *
 * SDCC's mcs51 port does not ship Keil's <intrins.h>. The vendor StdDriver
 * headers (and portable carriers) only use _nop_; _crol_/_cror_ appear in
 * framework shim examples. Keil semantics: rotate count taken mod 8. */
#ifndef MCS51_GATE_INTRINS_H
#define MCS51_GATE_INTRINS_H

#define _nop_() do { __asm nop __endasm; } while (0)

static unsigned char _crol_(unsigned char v, unsigned char n) {
    n &= 7u;
    return n ? (unsigned char)((v << n) | (v >> (8u - n))) : v;
}

static unsigned char _cror_(unsigned char v, unsigned char n) {
    n &= 7u;
    return n ? (unsigned char)((v >> n) | (v << (8u - n))) : v;
}

/* Keil _testbit_ tests bit 0 and clears it (JBC semantics). */
#define _testbit_(b) (((b) != 0) ? ((b) = 0, 1) : 0)

#endif
