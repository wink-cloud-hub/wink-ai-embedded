// SPDX-License-Identifier: Apache-2.0
// MCS-51 XDATA shadow + bounds-checked absolute access (M3, R-008).
//
// See absacc.h. A 64 KB linear xdata shadow (BSS) backs XBYTE/XWORD. Two
// apertures are legal (M5, CMS8S78xx XSFR):
//   * [0, WINK_MCS51_XDATA_SIZE) — ordinary XRAM/XRAM aperture (R-008);
//   * [0xF000, 0x10000) — extended-SFR window (pin mux PxxCFG @ 0xF000..,
//     ADCLDO @ 0xF692, …), reached via MOVX @DPTR on enhanced 8051s.
// Each checked access charges one interception microstep — the same
// interception rationale as the SFR proxy, so a tight `while(XBYTE[f] != x){}`
// poll advances virtual time and yields the fiber. Out-of-bounds accesses
// (R-008): STRICT traps (assert message in debug + unconditional abort so a
// release/NDEBUG STRICT build still fails loudly); release warns once per
// access kind (XBYTE vs XWORD), drops writes, and returns 0xFF for reads.
//
// M4 hook note: an external-xdata-peripheral write hook would attach here,
// before the shadow store, trapping writes to externally-mapped addresses.
#include "absacc.h"
#include "mcs51_context.h"
#include "mcs51_xsfr_allowlist.h"
#include "wink_mcs51_clock.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifndef WINK_MCS51_STRICT
#include "pal_log.h"
#endif

namespace {

constexpr uint8_t KIND_BYTE = 0u;  // XBYTE accessor
constexpr uint8_t KIND_WORD = 1u;  // XWORD accessor
constexpr uint8_t KIND_XSFR = 2u;  // XSFR proxy (WinkXsfr / REG_CMS8S78XX.H)

// CMS8S78xx extended-SFR window (MOVX @DPTR, manual §2.2): pin config
// PxxCFG @ 0xF000..0xF033, ADC LDO ADCLDO @ 0xF692, …
// GAP-09/GAP-23 family gating: the XSFR window and the unmodeled-XSFR
// tripwire exist ONLY on the enhanced CMS8S family; on a classic 8052 the
// same MOVX addresses are ordinary external RAM/IO space.
constexpr uint64_t XSFR_WINDOW_BASE = MCS51_XRAM_WINDOW_BASE;

bool     s_oob_warned[3] = {};   // once-per-kind warning latch
uint32_t s_oob_count = 0;

// Legal XDATA aperture for the active family (GAP-09):
//  - CMS8S78xx: 1 KB on-chip XRAM, then a hole, then the 0xF000 XSFR window.
//  - Classic:   the configurable WINK_MCS51_XDATA_SIZE external aperture;
//               no XSFR window.
bool xsfr_window_present() {
    return mcs51_context_get_family() == MCS51_FAMILY_CMS8S78XX;
}

uint64_t xram_aperture_size() {
    return xsfr_window_present()
             ? static_cast<uint64_t>(MCS51_XRAM_SIZE_CMS8S78XX)
             : static_cast<uint64_t>(WINK_MCS51_XDATA_SIZE);
}

// Legal xdata: ordinary XRAM aperture OR (CMS8S only) the XSFR window.
bool xdata_addr_legal(uint64_t addr) {
    if (addr < xram_aperture_size()) {
        return true;
    }
    return xsfr_window_present() &&
           (addr >= XSFR_WINDOW_BASE && addr < 0x10000ull);
}

// GAP-23 unmodeled-XSFR tripwire state (plain POD BSS, same policy pattern
// as the UART TX-link gate and mcs51_unsupported.cpp). STRICT builds abort
// before counting, so the counters stay 0 there by design.
uint32_t s_unmodeled_count = 0;
uint16_t s_unmodeled_first[8] = {};  // first-offender addresses, in order
uint8_t  s_unmodeled_first_n = 0;
#ifndef WINK_MCS51_STRICT
bool     s_unmodeled_warned = false;  // single category latch (not per address)
#endif

// Allowlist membership: the audit-generated DECLARED set
// (mcs51_xsfr_allowlist.h). Sorted, so binary search.
bool xsfr_allowlisted(uint64_t addr) {
    if (addr > 0xFFFFull) {
        return false;
    }
    const uint16_t a = static_cast<uint16_t>(addr);
    // Raw-array pointer arithmetic (no <iterator> dependency for std::begin).
    const uint16_t* first = kMcs51XsfrAllowlist;
    return std::binary_search(first, first + kMcs51XsfrAllowlistCount, a);
}

void unmodeled_xsfr_trap(uint64_t addr, bool is_write) {
    if (s_unmodeled_count < 0xFFFFFFFFu) {
        ++s_unmodeled_count;
    }
    if (s_unmodeled_first_n < 8u) {
        s_unmodeled_first[s_unmodeled_first_n++] =
            static_cast<uint16_t>(addr & 0xFFFFull);
    }
#ifdef WINK_MCS51_STRICT
    // Debug/test configuration: fail loudly at the offending access. assert
    // gives the diagnostic in debug builds; std::abort is unconditional so a
    // STRICT build compiled with NDEBUG still traps. The offending address
    // is retained in s_unmodeled_first for post-mortem inspection.
    (void)is_write;
    assert(0 && "unmodeled XSFR access (WINK_MCS51_STRICT)");
    std::abort();
#else
    if (!s_unmodeled_warned) {
        s_unmodeled_warned = true;
        pal_log_w("MCS51",
                  "unmodeled XSFR %s at 0x%04X: no framework model owns this "
                  "register (GAP-23); access lands in the shadow, behavior "
                  "is NOT simulated",
                  is_write ? "write" : "read",
                  (unsigned)(addr & 0xFFFFull));
    }
#endif
}

void oob_trap(uint64_t addr, uint8_t kind, bool is_write) {
    ++s_oob_count;
#ifdef WINK_MCS51_STRICT
    // Debug/test configuration: fail loudly at the offending access. assert
    // gives the diagnostic in debug builds; std::abort is unconditional so a
    // STRICT build compiled with NDEBUG (assert compiled out) still traps
    // instead of silently falling through to the drop/0xFF release behavior.
    (void)addr;
    (void)kind;
    (void)is_write;
    assert(0 && "XDATA access outside legal aperture (WINK_MCS51_STRICT)");
    std::abort();
#else
    if (kind <= KIND_XSFR && !s_oob_warned[kind]) {
        s_oob_warned[kind] = true;
        const char *what = kind == KIND_WORD ? "XWORD"
                         : kind == KIND_XSFR ? "XSFR" : "XBYTE";
        const unsigned aperture = static_cast<unsigned>(xram_aperture_size());
        if (xsfr_window_present()) {
            pal_log_w("MCS51",
                      "XDATA %s %s out of bounds (addr=0x%04llX, legal: "
                      "[0,%u) XRAM and [0xF000,0x10000) XSFR): %s",
                      what, is_write ? "write" : "read",
                      (unsigned long long)addr, aperture,
                      is_write ? "write dropped" : "returning 0xFF");
        } else {
            pal_log_w("MCS51",
                      "XDATA %s %s out of bounds (addr=0x%04llX, legal: "
                      "[0,%u) external XDATA; classic family has no XSFR "
                      "window): %s",
                      what, is_write ? "write" : "read",
                      (unsigned long long)addr, aperture,
                      is_write ? "write dropped" : "returning 0xFF");
        }
    }
#endif
}

}  // namespace

extern "C" {

uint8_t wink_mcs51_xdata_read(uint64_t addr, uint8_t kind) {
    wink_mcs51_microstep();
    if (xdata_addr_legal(addr)) {
        // GAP-23: reads and writes share the check — polling an unmodeled
        // status register is as silent as configuring one. Gated on the
        // address, not the accessor kind, so raw XBYTE and WinkXsfr proxies
        // are covered identically.
        if (addr >= XSFR_WINDOW_BASE && !xsfr_allowlisted(addr)) {
            unmodeled_xsfr_trap(addr, false);
        }
        return mcs51_get_context()->xdata_shadow[addr];
    }
    oob_trap(addr, kind, false);
    return 0xFFu;
}

void wink_mcs51_xdata_write(uint64_t addr, uint8_t value, uint8_t kind) {
    wink_mcs51_microstep();
    if (xdata_addr_legal(addr)) {
        if (addr >= XSFR_WINDOW_BASE && !xsfr_allowlisted(addr)) {
            unmodeled_xsfr_trap(addr, true);
        }
        mcs51_get_context()->xdata_shadow[addr] = value;
        return;
    }
    oob_trap(addr, kind, true);
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
uint32_t wink_mcs51_xdata_oob_count(void) {
    return s_oob_count;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
uint32_t wink_mcs51_xsfr_unmodeled_count(void) {
    return s_unmodeled_count;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
uint16_t wink_mcs51_xsfr_unmodeled_addr(uint32_t i) {
    return (i < s_unmodeled_first_n) ? s_unmodeled_first[i] : 0xFFFFu;
}

void wink_mcs51_xdata_reset(void) {
    std::memset(mcs51_get_context()->xdata_shadow, 0, sizeof(mcs51_get_context()->xdata_shadow));
    s_oob_count = 0;
    for (uint8_t k = 0; k <= KIND_XSFR; ++k) {
        s_oob_warned[k] = false;
    }
    s_unmodeled_count = 0;
    s_unmodeled_first_n = 0;
#ifndef WINK_MCS51_STRICT
    s_unmodeled_warned = false;
#endif
}

}  // extern "C"
