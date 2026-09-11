// SPDX-License-Identifier: Apache-2.0
// AT89C52 classic-family register entry (Stage4 S4-2 Step 3,
// PLAN-20260911-MCS51-S4, CPL-10).
//
// Classic is the zero-extension pure core: intentionally an empty function.
// The emptiness IS the protocol (total §3.1b-2) — every family exposes the
// same register surface so dispatch glue and harnesses stay uniform.
extern "C" {

void at89c52_register(void) {
}

}  // extern "C"
