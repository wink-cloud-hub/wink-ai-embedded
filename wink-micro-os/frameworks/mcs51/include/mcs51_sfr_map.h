// SPDX-License-Identifier: LGPL-3.0-only
// MCS-51 shared STANDARD SFR address map (maintainability M5, purified S3-1).
//
// Addresses used by TWO OR MORE translation units live HERE (single source).
// Model-private addresses (used by exactly one model) stay as local
// constexprs in that model's .cpp. Rule of thumb: before adding a new
// address constant, grep — if it already exists here, include it.
//
// S3-1 (CPL-13): this map carries ONLY Intel-standard / multi-vendor-generic
// addresses. Proprietary registers (port-interrupt flags, T3/T4 control,
// extended interrupt enables/flags, pin-share selectors) moved to the chip
// package map (chips/<family>/include/*_sfr_map.h); transitional core TUs
// (stripped to chips/ in stage4) include that header directly until their
// code moves with the addresses.
//
// C-compatible (#defines only) so C test TUs and C++ models share it.
#pragma once

// ── Direct SFR: Intel 8052 standard ────────────────────────────────────────
#define MCS51_SFR_T2CON 0xC8u  // T2CON (timer + UART TMR2 baud check)

// ── Direct SFR: multi-vendor-generic address, family-defined semantics ─────
// CKCON (0x8E) exists on many enhanced 51s with differing bit semantics;
// only the ADDRESS is generic here. Semantics (T0M/T1M, WTS) come from the
// family descriptor / chip package (context reset seed, timer, uart).
#define MCS51_SFR_CKCON 0x8Eu  // CKCON (timer T0M/T1M + context reset seed)
