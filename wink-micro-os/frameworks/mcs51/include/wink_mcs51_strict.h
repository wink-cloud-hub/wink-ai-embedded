// SPDX-License-Identifier: Apache-2.0
// MCS-51 WINK_SIM_STRICT dual-mode unsupported-feature mechanism (M3,
// mcu-compat-plan.md §3.8 / acceptance #7).
//
// Some 8051 dialect features and hardware behaviors cannot be modeled at the
// functional level (PSW arithmetic flags, computed SFR addresses, external
// counter pin, Timer0 mode 3, ...). When cleaned user code or a peripheral
// model hits one, it calls wink_mcs51_unsupported(feature_id, name). Behavior
// is selected at compile time:
//
//   * WINK_MCS51_STRICT defined (CMake option WINK_MCS51_STRICT=ON; this is the
//     "WINK_SIM_STRICT" build in the plan/prose — the macro spelling differs
//     only to match the WINK_MCS51_ target-prefix convention): the call
//     asserts, failing the debug/test build loudly and pointing at the exact
//     feature. STRICT is a debug/test configuration — do not ship it.
//   * Otherwise (default release): the FIRST use of each feature id logs one
//     pal_log_w warning (latched, so a poll loop cannot flood the log) and
//     execution continues with the feature as a documented no-op. Counters
//     are exposed for tests.
//
// Feature ids are stable numeric values (never renumber; append new ids at the
// end). Dialect-level features (inline asm, _at_, generic pointers, ...) are
// rejected earlier by the cleanup pass/compiler and never reach this call at
// runtime; they are enumerated for documentation and future checks.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Unsupported feature ids (§3.8 list; stable numbers) ──────────────────────
enum {
    // Arithmetic that depends on PSW flags CY/AC/OV/P: the shadow SFRs cannot
    // emulate ALU flag side effects (multi-precision libraries only).
    MCS51_FEAT_PSW_FLAGS         = 1,
    // Computed SFR address: `*(unsigned char idata *)0x80 = x;` — only named
    // SFR proxies are intercepted; absolute idata writes bypass the shadow.
    MCS51_FEAT_COMPUTED_SFR_ADDR = 2,
    // Keil inline assembly (`#pragma asm` / `asm`): not parseable by the host
    // toolchain; rejected by the cleanup pass.
    MCS51_FEAT_INLINE_ASM        = 3,
    // `_at_` absolute variable placement: the macro erases the placement, so
    // code relying on a fixed link address (memory-mapped buffers) is wrong.
    MCS51_FEAT_AT_ABSOLUTE       = 4,
    // RC charge/discharge thermistor timing: functional-level sim cannot
    // reproduce analog RC curves (use an external ADC, e.g. ADC0832).
    MCS51_FEAT_RC_THERMAL        = 5,
    // Code depending on the Keil generic 3-byte pointer's internal tag byte:
    // host/wasm pointers are flat.
    MCS51_FEAT_GENERIC_POINTER   = 6,
    // `sbit name = 0xXX` absolute bit-address form: natively supported via
    // constexpr WinkSbit(int) since ADR-0072 D5 (retained for backward enum stability).
    MCS51_FEAT_ABS_SBIT_ADDR     = 7,
    // Sub-microsecond instruction-cycle timing (bit-banged WS2812/1-Wire
    // counting _nop_() cycles): the microstep granularity is too coarse.
    MCS51_FEAT_SUBUS_TIMING      = 8,
    // Timer0 mode 3 (split 8-bit counters): not modeled; Timer0 stays idle.
    MCS51_FEAT_TIMER_MODE3       = 9,
    // Timer external C/T pin counting (TMOD C/T == 1): no external pulse time
    // source at functional level; the timer stays idle.
    MCS51_FEAT_TIMER_EXT_CLK     = 10,
};

// Record use of an unsupported 8051 feature. STRICT: assert (debug/test).
// Release: warn once per feature id (rate-limited), then continue no-op.
void wink_mcs51_unsupported(uint32_t feature_id, const char* feature_name);

// ── Test observability ──────────────────────────────────────────────────────
// Total warnings that would/have been emitted (release: increments once per
// distinct feature id latched; STRICT builds abort before counting).
uint32_t wink_mcs51_unsupported_warning_count(void);
// Number of times the given feature id was triggered since reset.
uint32_t wink_mcs51_unsupported_trigger_count(uint32_t feature_id);
// Clear all latches/counters (test isolation; called from framework init).
void wink_mcs51_unsupported_reset(void);

#ifdef __cplusplus
}  // extern "C"
#endif
