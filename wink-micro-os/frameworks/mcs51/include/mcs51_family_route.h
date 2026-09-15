// SPDX-License-Identifier: LGPL-3.0-only
// mcs51_family_route.h — MCS-51-family-only register routing (Stage3 S3-1
// Step 3, PLAN-20260911-MCS51-S3).
//
// Slimmed rename of the 51 branches of wink_mcu.h: routes to the concrete
// 8051 register dialect from the "mcu" field in wink-app.json (injected via
// -DWINK_MCU_* compile definitions or wink_config.h). The full-platform
// facade (51 + Padauk + gatekeeper) moved up to
// wink-micro-os/runtime/include/wink_mcu.h; no file named wink_mcu.h remains
// inside frameworks/mcs51/ (avoids facade/route confusion).
//
// Who includes what: framework-internal and 51 test code include THIS
// header; applications keep including <wink_mcu.h> unchanged.
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(__has_include)
#  if __has_include("wink_config.h")
#    include "wink_config.h"
#  endif
#endif

// ── CMS8S78xx family ──────────────────────────────────────────────────────
#if defined(WINK_MCU_CMS8S78XX) || defined(WINK_MCU_CMS8S) || defined(__CMS8S78XX__) || defined(CMS8S78XX)
    #include "REG_CMS8S78XX.H"

// ── Classic 8051/8052 family ──────────────────────────────────────────────
#elif defined(WINK_MCU_AT89C52) || defined(WINK_MCU_STC89C52) || defined(WINK_MCU_MCS51) || defined(WINK_MCU_8051) || defined(__AT89C52__)
    #include "REGX52.H"

// ── Fallback / Gatekeeper (51 families only; other families route through
// ── the platform facade <wink_mcu.h>) ────────────────────────────────────
#else
    #error "[mcs51_family_route.h] No valid MCS-51 target defined! Please specify 'mcu' in wink-app.json (e.g. 'cms8s78xx', 'at89c52')."
#endif
