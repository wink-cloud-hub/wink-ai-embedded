// SPDX-License-Identifier: Apache-2.0
// MCS-51 shared SFR/XSFR address map (maintainability M5).
//
// Addresses used by TWO OR MORE translation units live HERE (single source).
// Model-private addresses (used by exactly one model) stay as local
// constexprs in that model's .cpp. Rule of thumb: before adding a new
// address constant, grep — if it already exists here, include it.
//
// C-compatible (#defines only) so C test TUs and C++ models share it.
#pragma once

// ── Direct SFR: port interrupt flags (extint model + W0C hook) ─────────────
#define MCS51_SFR_P0EXTIF 0xB4u
#define MCS51_SFR_P1EXTIF 0xB5u
#define MCS51_SFR_P2EXTIF 0xB6u
#define MCS51_SFR_P3EXTIF 0xB7u

// ── Direct SFR: shared timer/clock/interrupt control ───────────────────────
#define MCS51_SFR_CKCON 0x8Eu  // CKCON (timer T0M/T1M + context reset seed)
#define MCS51_SFR_T34MOD 0xD2u  // T34MOD (timer T3/T4 + UART TMR4 baud check)
#define MCS51_SFR_T2CON 0xC8u  // T2CON (timer + UART TMR2 baud check)
#define MCS51_SFR_EIE2 0xAAu  // EIE2 (timer + UART + IRQ map)
#define MCS51_SFR_EIF2 0xB2u  // EIF2 (timer + UART + IRQ map)

// ── XSFR: pin-share selectors (context seeds + extint/timer/adc models) ────
// Reset value 0x7F = no pin connected (ref manual).
#define MCS51_XSFR_PS_INT0 0xF0C0u
#define MCS51_XSFR_PS_INT1 0xF0C1u
#define MCS51_XSFR_PS_T0 0xF0C2u
#define MCS51_XSFR_PS_T0G 0xF0C3u
#define MCS51_XSFR_PS_T1 0xF0C4u
#define MCS51_XSFR_PS_T1G 0xF0C5u
#define MCS51_XSFR_PS_T2 0xF0C6u
#define MCS51_XSFR_PS_T2EX 0xF0C7u
#define MCS51_XSFR_PS_CAP0 0xF0C8u
#define MCS51_XSFR_PS_CAP1 0xF0C9u
#define MCS51_XSFR_PS_CAP2 0xF0CAu
#define MCS51_XSFR_PS_CAP3 0xF0CBu
#define MCS51_XSFR_PS_ADET 0xF0CCu
#define MCS51_XSFR_PS_RESET 0x7Fu
