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
#include "mcs51_family.h"
#include "mcs51_xsfr_allowlist.h"
#include "wink_mcs51_classic_bus.h"
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

// Extended-SFR window facts (base/size) come from the family descriptor
// (M1): pin config PxxCFG @ 0xF000..0xF033, ADC LDO ADCLDO @ 0xF692, …
// GAP-09/GAP-23 family gating: the XSFR window and the unmodeled-XSFR
// tripwire exist ONLY on families exposing the window; on a classic 8052
// the same MOVX addresses are ordinary external RAM/IO space.
const mcs51_family_desc_t* active_family_desc() {
    return mcs51_family_desc(mcs51_get_context()->family);
}

bool     s_oob_warned[3] = {};   // once-per-kind warning latch
uint32_t s_oob_count = 0;

// Legal XDATA aperture for the active family (GAP-09):
//  - on-chip XRAM families: descriptor xram_size, then a hole, then the
//    XSFR window;
//  - classic: the configurable WINK_MCS51_XDATA_SIZE external aperture;
//    no XSFR window.
bool xsfr_window_present() {
    return mcs51_family_has_xsfr(active_family_desc());
}

uint64_t xram_aperture_size() {
    const uint32_t onchip = active_family_desc()->xram_size;
    return (onchip != 0u) ? static_cast<uint64_t>(onchip)
                          : static_cast<uint64_t>(WINK_MCS51_XDATA_SIZE);
}

// XSFR window range for the active family (empty when no window).
bool xsfr_addr_in_window(uint64_t addr) {
    const mcs51_family_desc_t* d = active_family_desc();
    return (d->xsfr_size != 0u) && (addr >= d->xsfr_base) &&
           (addr < (uint64_t)d->xsfr_base + (uint64_t)d->xsfr_size);
}

// Legal xdata: ordinary XRAM aperture OR the family XSFR window.
bool xdata_addr_legal(uint64_t addr) {
    if (addr < xram_aperture_size()) {
        return true;
    }
    return xsfr_addr_in_window(addr);
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

// GAP-24 classic external-bus conflict (process-level counter, M4).
// STRICT aborts before counting, so the counter stays 0 there by design.
uint32_t s_bus_conflict = 0u;
#ifndef WINK_MCS51_STRICT
bool s_bus_conflict_warned = false;
#endif

// External MOVX bus iff the part has no on-chip XRAM (classic AT89C52:
// descriptor xram_size == 0; CMS8S78xx has 1 KB internal XRAM and never
// touches pins for XBYTE). Branches on the descriptor, never a family id.
bool classic_external_bus(const Mcu51Context* ctx) {
    return mcs51_family_desc(ctx->family)->xram_size == 0u;
}

// Bus-pin bit allocation in Mcs51ClassicBusState::gpio_bus_mask:
// P0: bits 0-7, P2: bits 8-15, P3.6/7: bits 16-17.
uint32_t gpio_bus_bits(uint8_t port, uint8_t bitmask) {
    if (port == 0u) {
        return bitmask;
    }
    if (port == 2u) {
        return static_cast<uint32_t>(bitmask) << 8u;
    }
    if (port == 3u) {
        uint32_t bits = 0u;
        if ((bitmask & 0x40u) != 0u) {
            bits |= (1u << 16u);
        }
        if ((bitmask & 0x80u) != 0u) {
            bits |= (1u << 17u);
        }
        return bits;
    }
    return 0u;
}

void bus_conflict_policy(void) {
#ifdef WINK_MCS51_STRICT
    assert(0 && "classic MOVX bus conflict with GPIO use (WINK_MCS51_STRICT)");
    std::abort();
#else
    if (s_bus_conflict < 0xFFFFFFFFu) {
        ++s_bus_conflict;
    }
    if (!s_bus_conflict_warned) {
        s_bus_conflict_warned = true;
        pal_log_w("MCS51",
                  "classic MOVX bus conflict: XBYTE traffic shares "
                  "P0/P2/P3.6-7 with GPIO use (GAP-24); on silicon the "
                  "bus wins and GPIO is lost");
    }
#endif
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
        if (xsfr_addr_in_window(addr) && !xsfr_allowlisted(addr)) {
            unmodeled_xsfr_trap(addr, false);
        }
        // GAP-24: a legal classic-family MOVX read drives /RD + P0/P2.
        mcs51_classic_bus_notify_xbyte(mcs51_get_context());
        return mcs51_get_context()->xdata_shadow[addr];
    }
    oob_trap(addr, kind, false);
    return 0xFFu;
}

void wink_mcs51_xdata_write(uint64_t addr, uint8_t value, uint8_t kind) {
    wink_mcs51_microstep();
    if (xdata_addr_legal(addr)) {
        if (xsfr_addr_in_window(addr) && !xsfr_allowlisted(addr)) {
            unmodeled_xsfr_trap(addr, true);
        }
        // GAP-24: a legal classic-family MOVX write drives /WR + P0/P2.
        mcs51_classic_bus_notify_xbyte(mcs51_get_context());
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

void mcs51_classic_bus_notify_xbyte(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!classic_external_bus(ctx)) {
        return;  // on-chip XRAM: MOVX never reaches pins
    }
    if (ctx->classicBus.gpio_bus_mask != 0u) {
        bus_conflict_policy();
    }
    ctx->classicBus.xbus_used = 1u;
}

void mcs51_classic_bus_notify_gpio(struct Mcu51Context* ctx, uint8_t port,
                                   uint8_t bitmask) {
    if (!ctx) ctx = mcs51_get_context();
    const uint32_t bits = gpio_bus_bits(port, bitmask);
    if (bits == 0u) {
        return;  // P1 / P3.0-5 are never bus pins
    }
    if (!classic_external_bus(ctx)) {
        return;  // on-chip XRAM: GPIO never fights the bus
    }
    if (ctx->classicBus.xbus_used != 0u) {
        bus_conflict_policy();
    }
    ctx->classicBus.gpio_bus_mask |= bits;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
uint32_t wink_mcs51_classic_bus_conflict_total(void) {
    return s_bus_conflict;
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
    s_bus_conflict = 0u;
    for (uint8_t k = 0; k <= KIND_XSFR; ++k) {
        s_oob_warned[k] = false;
    }
    s_unmodeled_count = 0;
    s_unmodeled_first_n = 0;
#ifndef WINK_MCS51_STRICT
    s_unmodeled_warned = false;
    s_bus_conflict_warned = false;
#endif
}

}  // extern "C"
