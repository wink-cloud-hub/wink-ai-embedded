/* SPDX-License-Identifier: LGPL-3.0-only
 * SDCC compile-gate absacc adapter (GAP-03, Tier-S).
 *
 * Must be pointer casts (NOT dereferenced): user code indexes them as
 * XBYTE[addr] / XWORD[addr], so the macro must expand to a pointer.
 * Points at xdata address 0; addr is the absolute MOVX address. */
#ifndef MCS51_GATE_ABSACC_H
#define MCS51_GATE_ABSACC_H

#define XBYTE ((volatile unsigned char __xdata *)0)
#define XWORD ((volatile unsigned int  __xdata *)0)

#endif
