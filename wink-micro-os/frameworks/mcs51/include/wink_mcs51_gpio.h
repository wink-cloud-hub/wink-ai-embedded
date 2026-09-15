// SPDX-License-Identifier: GPL-3.0-only
// MCS-51 GPIO dual-read path and pin arbitration services (Task R0, ADR-0077).
//
// Separates Read-Pin (external pin level 3-way arbitration for MOV A, Pn / MOV C, bit)
// from Read-Latch (direct port latch read for RMW instructions like ANL/ORL/XRL Pn, CPL/CLR bit).
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Drive strengths for 8051 quasi-bidirectional ports ───────────────────────
#define MCS51_DRIVE_WEAK   1u  /**< Latch=1: weak internal pull-up */
#define MCS51_DRIVE_SUPPLY 3u  /**< Latch=0: strong NMOS pull-down */

// ── UniSim channel-1 GPIO imports ───────────────────────────────────────────
void js_pal_gpio_write(uint16_t pin, bool level, uint8_t strength);
uint8_t js_pal_gpio_read_state(uint16_t pin);
// P3 (PLAN-20260912-MCS51-P3-TRIS): drop the MCU driver on a pin (TRIS 1->0
// input switch, open-drain release, reset). Mirrors wasm_bridge.h; host gets
// a recording fallback in mcs51_uni_bridge.cpp.
void js_pal_gpio_release_mcu(uint16_t pin);

// ── Write Path ──────────────────────────────────────────────────────────────
void mcs51_gpio_sfr_write(uint8_t port, uint8_t new_val);
void mcs51_gpio_bit_write(uint8_t port, uint8_t bit, uint8_t level);

// ── Read Path (Caller must select based on instruction semantics) ────────────
// Read-Pin: External pin 3-way arbitration (MOV A, Pn / MOV C, bit):
//   Priority 1: internal on_read trap (e.g. ADC0832 DO line)
//   Priority 2: UniSim channel-1 external driven level (js_pal_gpio_read_state)
//   Priority 3: HiZ / Conflict fallback to port latch shadow
uint8_t mcs51_gpio_read_pin(uint8_t port);
uint8_t mcs51_gpio_bit_read_pin(uint8_t port, uint8_t bit);

// Read-Latch: RMW instructions only (ANL/ORL/XRL Pn, CPL/SETB/CLR/JBC Pn.bit, MOV Pn.bit, C):
//   Directly reads port latch shadow; never traverses external pin arbitration!
uint8_t mcs51_gpio_read_latch(uint8_t port);
uint8_t mcs51_gpio_bit_read_latch(uint8_t port, uint8_t bit);

// A-05 GPIO direction modeling (GAP-08 + GAP-25 analog sub-item):
//   TRIS-gated output suppression counter (input latch writes that produce
//   no external drive) + analog-pin digital-read counter. STRICT aborts on
//   analog digital-read; TRIS-suppressed writes count + warn-once (legal
//   latch writes, no drive). Counters feed the GAP-10 runner verdict.
uint32_t wink_mcs51_gpio_output_suppressed_count(void);
uint32_t wink_mcs51_gpio_analog_read_count(void);
uint32_t wink_mcs51_gpio_diag_total(void);
void wink_mcs51_gpio_diag_reset(void);

#ifdef __cplusplus
}
#endif
