// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx chip-package register entry (Stage4 S4-2 Step 3,
// PLAN-20260911-MCS51-S4, CPL-10).
//
// The single place that names cms8s_* peripheral symbols: core dispatch
// loops never reference them (total §3.1 one-way rule); production glue
// (generated mcs51_family_select.h, stage6) and the test harness call
// cms8s78xx_register() explicitly. Idempotent (registry dedups by name).
#include "mcs51_peripheral.h"

extern "C" {

// Stage4 Commit A: stub (no descriptors moved yet; static table still owns
// cms8s_adc/buzzer/sys). Commit C appends the chip descriptors here and the
// static rows shrink to the core three.
void cms8s78xx_register(void) {
}

}  // extern "C"
