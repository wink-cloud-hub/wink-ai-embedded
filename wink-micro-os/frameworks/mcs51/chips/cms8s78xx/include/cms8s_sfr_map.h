// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx proprietary SFR/XSFR address map (Stage3 S3-1 Step 2,
// PLAN-20260911-MCS51-S3, CPL-13).
//
// Sunk from the generic include/mcs51_sfr_map.h, which now carries ONLY
// Intel-standard addresses (T2CON) plus the multi-vendor-generic CKCON
// address (semantics per family descriptor). Everything here is CMS8S-only
// silicon: port-interrupt flags, T3/T4 control, extended interrupt
// enables/flags and pin-share selectors.
//
// C-compatible (#defines only) so C test TUs and C++ models share it.
// Dependency direction: chips -> core (this header is standalone; chip TUs
// combine it with the generic map, never the reverse).
#pragma once

// ── Direct SFR: port interrupt flags (extint model + W0C hook) ─────────────
#define CMS8S_SFR_P0EXTIF 0xB4u
#define CMS8S_SFR_P1EXTIF 0xB5u
#define CMS8S_SFR_P2EXTIF 0xB6u
#define CMS8S_SFR_P3EXTIF 0xB7u

// ── Direct SFR: CMS8S-only timer/clock/interrupt control ───────────────────
#define CMS8S_SFR_T34MOD 0xD2u  // T34MOD (timer T3/T4 + UART TMR4 baud check)
#define CMS8S_SFR_EIE2 0xAAu  // EIE2 (timer + UART + IRQ map)
#define CMS8S_SFR_EIF2 0xB2u  // EIF2 (timer + UART + IRQ map)

// ── XSFR: pin-share selectors ──────────────────────────────────────────────
// Reset value 0x7F = no pin connected (ref manual).
#define CMS8S_XSFR_PS_INT0 0xF0C0u
#define CMS8S_XSFR_PS_INT1 0xF0C1u
#define CMS8S_XSFR_PS_T0 0xF0C2u
#define CMS8S_XSFR_PS_T0G 0xF0C3u
#define CMS8S_XSFR_PS_T1 0xF0C4u
#define CMS8S_XSFR_PS_T1G 0xF0C5u
#define CMS8S_XSFR_PS_T2 0xF0C6u
#define CMS8S_XSFR_PS_T2EX 0xF0C7u
#define CMS8S_XSFR_PS_CAP0 0xF0C8u
#define CMS8S_XSFR_PS_CAP1 0xF0C9u
#define CMS8S_XSFR_PS_CAP2 0xF0CAu
#define CMS8S_XSFR_PS_CAP3 0xF0CBu
#define CMS8S_XSFR_PS_ADET 0xF0CCu
#define CMS8S_XSFR_PS_RESET 0x7Fu
