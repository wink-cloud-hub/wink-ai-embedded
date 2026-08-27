// SPDX-License-Identifier: Apache-2.0
// MCS-51 unsupported-feature mechanism (M3): see wink_mcs51_strict.h.
//
// All state is plain POD (zero-init BSS latch + counters) — static-init safe
// (ADR-0072 D5). The release path uses pal_log_w (the pal target is linked by
// the compat library); the STRICT path uses assert, so a debug/test build
// fails loudly at the exact unmodeled feature.
#include "wink_mcs51_strict.h"

#include <cassert>
#include <cstdint>

#ifndef WINK_MCS51_STRICT
#include "pal_log.h"
#endif

namespace {

// Feature ids are dense small integers (see the enum in wink_mcs51_strict.h);
// 64 slots leave ample headroom. OOB ids are tolerated (treated as id 0).
constexpr uint32_t FEAT_SLOTS = 64u;

bool     s_warned[FEAT_SLOTS] = {};   // once-per-id warning latch
uint32_t s_triggered[FEAT_SLOTS] = {};
uint32_t s_warn_count = 0;            // distinct ids warned (latched warnings)

}  // namespace

extern "C" {

void wink_mcs51_unsupported(uint32_t feature_id, const char* feature_name) {
    if (feature_id >= FEAT_SLOTS) {
        feature_id = 0;  // keep indexing in bounds; 0 is an unused bucket
    }
    ++s_triggered[feature_id];

#ifdef WINK_MCS51_STRICT
    // Debug/test configuration: fail loudly at the exact unmodeled feature.
    assert(0 && "unsupported 8051 feature used (WINK_MCS51_STRICT)");
#else
    // Release: one warning per feature id, then the feature stays a no-op.
    if (!s_warned[feature_id]) {
        s_warned[feature_id] = true;
        ++s_warn_count;
        pal_log_w("MCS51", "unsupported 8051 feature used: %s",
                  feature_name != nullptr ? feature_name : "unknown");
    }
#endif
}

uint32_t wink_mcs51_unsupported_warning_count(void) {
    return s_warn_count;
}

uint32_t wink_mcs51_unsupported_trigger_count(uint32_t feature_id) {
    if (feature_id >= FEAT_SLOTS) {
        return 0;
    }
    return s_triggered[feature_id];
}

void wink_mcs51_unsupported_reset(void) {
    for (uint32_t i = 0; i < FEAT_SLOTS; ++i) {
        s_warned[i] = false;
        s_triggered[i] = 0;
    }
    s_warn_count = 0;
}

}  // extern "C"
