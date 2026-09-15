// SPDX-License-Identifier: LGPL-3.0-only
// AT89C52 classic-family register entry (Stage4 S4-2 Step 3,
// PLAN-20260911-MCS51-S4, CPL-10).
//
// Classic is the zero-extension pure core: intentionally an empty function.
// The emptiness IS the protocol (total §3.1b-2) — every family exposes the
// same register surface and the same link-time self-registration contract
// (Stage7 S7-1), so dispatch glue and harnesses stay uniform even though this
// family appends no descriptors.
extern "C" {

void at89c52_register(void) {
}

}  // extern "C"

namespace {

[[maybe_unused]] const bool s_at89_register_at_link =
    (at89c52_register(), true);

}  // namespace
