// SPDX-License-Identifier: Apache-2.0
// wink_mcu.h — Unified MCU facade header for Wink Micro OS.
//
// Automatically routes to the concrete microcontroller register definitions
// and dialect proxies based on the "mcu" field in wink-app.json (injected via
// -DWINK_MCU_* compile definitions or wink_config.h).
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(__has_include)
#  if __has_include("wink_config.h")
#    include "wink_config.h"
#  endif
#endif

// ── 1) MCS-51 Family ────────────────────────────────────────────────────────
#if defined(WINK_MCU_CMS8S78XX) || defined(WINK_MCU_CMS8S) || defined(__CMS8S78XX__) || defined(CMS8S78XX)
    #include "REG_CMS8S78XX.H"

#elif defined(WINK_MCU_AT89C52) || defined(WINK_MCU_STC89C52) || defined(WINK_MCU_MCS51) || defined(WINK_MCU_8051) || defined(__AT89C52__)
    #include "REGX52.H"

// ── 2) Padauk (PDK) Family ──────────────────────────────────────────────────
#elif defined(WINK_MCU_PFS154) || defined(PFS154)
    #ifndef PFS154
    #define PFS154
    #endif
    #include <pfs154.h>

#elif defined(WINK_MCU_PMS150C) || defined(PMS150C)
    #ifndef PMS150C
    #define PMS150C
    #endif
    #include <pms150c.h>

// ── 3) Fallback / Gatekeeper ────────────────────────────────────────────────
#else
    #error "[wink_mcu.h] No valid MCU target defined! Please specify 'mcu' in wink-app.json (e.g. 'cms8s78xx', 'at89c52', 'pfs154')."
#endif
