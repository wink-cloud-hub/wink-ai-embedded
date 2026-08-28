/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 RMW isolation — UNMODIFIED Keil C51 user source (M4, data-plane
 * SSOT §4.3 Zero False-Trigger Test Vectors).
 *
 * The 8051 quasi-bidirectional port pins 0..7 of P1 are imagined each to carry
 * an independent peripheral pin trap (LED, relay, ADC0832 CS/CLK/DI...). The
 * whole-port RMW idioms a real Keil program uses (`P1 |= 0x01;`, `P1 &= ~0x01;`,
 * `P1 = 0x00;`) must fire ONLY the addressed pin's on_write edge trap — never
 * the neighbouring pins — and a zero-delta write must fire nothing at all.
 *
 * The four vectors:
 *   V1: P1=0x00 -> P1|=0x01   expect only pin0 0->1
 *   V2: P1=0x01 -> P1&=~0x01  expect only pin0 1->0
 *   V3: P1=0x00 -> P1=0x00    expect NO edges (diff==0 fast path)
 *   V4: P1=0xFF with an external button holding pin2 LOW through on_read,
 *       then P1&=~0x01: RMW reads the LATCH (0xFF), not pins, so result 0xFE;
 *       pin2 latch bit stays 1 (input channel never corrupted), only pin0 edge.
 *
 * This is a Keil-style program: counters would be RAM on real hardware; for the
 * sandbox run they live in XDATA via XBYTE so the C test driver reads them
 * directly out of the linear XDATA shadow. There is deliberately no ISR.
 */
#include <REGX52.H>
#include <absacc.h>

/* Trap counter log: XBYTE[0x0000..0x0007] = per-pin on_write edge counts. */
#define TC(pin)        XBYTE[0x0000u + (pin)]
/* Result markers: 0x4D34 = "M4 OK" once a vector ran. */
#define V1_DONE        XBYTE[0x0010u]
#define V2_DONE        XBYTE[0x0011u]
#define V3_DONE        XBYTE[0x0012u]
#define V4_DONE        XBYTE[0x0013u]
/* Latch snapshot of P1 taken by the Keil code after V4. */
#define V4_LATCH       XBYTE[0x0014u]
/* Keil-side view of P1 during V4 (whole-port read; pin2 reconstructed low). */
#define V4_PINVIEW     XBYTE[0x0015u]
/* Per-vector edge-count snapshots: SNAP(v,pin) = TC(pin) after vector v. */
#define SNAP(v, pin)   XBYTE[0x0020u + (v) * 8u + (pin)]

static void snapshot(unsigned v) {
    SNAP(v,0)=TC(0); SNAP(v,1)=TC(1); SNAP(v,2)=TC(2); SNAP(v,3)=TC(3);
    SNAP(v,4)=TC(4); SNAP(v,5)=TC(5); SNAP(v,6)=TC(6); SNAP(v,7)=TC(7);
}

sbit OUT_PIN = P1^0;   /* the pin the program toggles */
sbit KEY_PIN = P1^2;   /* externally held low in V4 (Read-Pin input) */

void main(void) {
    /* ── Vector 1: single-bit SET isolation ─────────────────────────────── */
    P1 = 0x00;          /* all latches low; edges here are baseline, ignored */
    TC(0) = 0; TC(1) = 0; TC(2) = 0; TC(3) = 0;
    TC(4) = 0; TC(5) = 0; TC(6) = 0; TC(7) = 0;
    P1 |= 0x01;         /* RMW read-latch: 0x00 -> 0x01, only pin0 edges */
    snapshot(0);
    V1_DONE = 1;

    /* ── Vector 2: single-bit CLEAR isolation ───────────────────────────── */
    TC(0) = 0; TC(1) = 0; TC(2) = 0; TC(3) = 0;
    TC(4) = 0; TC(5) = 0; TC(6) = 0; TC(7) = 0;
    P1 &= ~0x01;        /* 0x01 -> 0x00, only pin0 edges */
    snapshot(1);
    V2_DONE = 1;

    /* ── Vector 3: zero-delta fast path (no edge at all) ────────────────── */
    TC(0) = 0; TC(1) = 0; TC(2) = 0; TC(3) = 0;
    TC(4) = 0; TC(5) = 0; TC(6) = 0; TC(7) = 0;
    P1 = 0x00;          /* already 0x00: diff == 0, silent */
    P1 &= ~0x01;        /* still 0x00: silent */
    snapshot(2);
    V3_DONE = 1;

    /* ── Vector 4: Read-Latch vs Read-Pin latch protection ──────────────── */
    /* The harness forces KEY_PIN's external level low via an on_read trap
       (button pressed), but the LATCH bit stays 1 (P1 = 0xFF). A whole-port
       RMW that only touches pin0 must read the latch, so the button's low
       level can never be written back into the pin2 latch (FET lock-up). */
    P1 = 0xFF;
    TC(0) = 0; TC(1) = 0; TC(2) = 0; TC(3) = 0;
    TC(4) = 0; TC(5) = 0; TC(6) = 0; TC(7) = 0;
    V4_PINVIEW = P1;    /* whole-port Read-Pin: sees key low (bit2 = 0) */
    P1 &= ~0x01;        /* RMW reads LATCH 0xFF -> writes 0xFE */
    V4_LATCH = P1;      /* snapshot post-RMW (Read-Pin view: bit2 still low) */
    snapshot(3);
    V4_DONE = 1;

    while (1) {
        _nop_();        /* hold; the driver asserts after the run */
    }
}
