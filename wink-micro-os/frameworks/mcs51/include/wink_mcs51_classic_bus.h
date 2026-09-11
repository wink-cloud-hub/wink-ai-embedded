// SPDX-License-Identifier: Apache-2.0
// Stage 3 Task 5 (A-08, GAP-24): classic-51 external MOVX bus occupancy.
//
// Parts without on-chip XRAM (AT89C52-class: descriptor xram_size == 0)
// drive P0 (AD0-7), P2 (A8-15), P3.6 (/WR) and P3.7 (/RD) on every MOVX
// access. Firmware that mixes XBYTE traffic with GPIO use of those pins
// is in silicon conflict: the sim shadow happily serves both sides while
// the real pins cannot. Both orders are caught — XBYTE after GPIO use and
// GPIO use after XBYTE — via two per-context sticky latches
// (Mcu51Context::classicBus); the conflict verdict itself is a
// process-level diagnostic counter (M4): STRICT aborts at the offending
// access, Release counts (saturating) + warns once and lets the access
// through so scenarios stay observable for the GAP-10 runner verdict.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Mcu51Context;

// A legal classic-family XBYTE/XWORD access happened (external bus driven).
// Counts a conflict when GPIO already touched bus pins, then latches bus use.
void mcs51_classic_bus_notify_xbyte(struct Mcu51Context* ctx);

// Firmware wrote `bitmask` to GPIO `port` (whole-port or single-bit path).
// Only P0/P2/P3.6-7 participate; other ports are a fast no-op. Counts a
// conflict when MOVX traffic was already seen, then latches the pins.
void mcs51_classic_bus_notify_gpio(struct Mcu51Context* ctx, uint8_t port,
                                   uint8_t bitmask);

#ifdef __EMSCRIPTEN__
__attribute__((used))
#endif
// GAP-10 firmware-health verdict: saturating bus-conflict count.
uint32_t wink_mcs51_classic_bus_conflict_total(void);

#ifdef __cplusplus
}  // extern "C"
#endif
