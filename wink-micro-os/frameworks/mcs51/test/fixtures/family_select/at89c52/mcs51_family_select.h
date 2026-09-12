// SPDX-License-Identifier: Apache-2.0
// mcs51_family_select.h — classic AT89C52 fixture (TEST USE ONLY).
//
// Checked-in stand-in for the codegen'd production header until the external
// wink-tools generator lands (stage6 prerequisite). The wasm harness copies
// this header into the per-sample include dir for classic samples so
// mcs51_bridge.cpp takes its __has_include branch and exercises the uniform
// register protocol (at89c52_register() is intentionally empty).
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void at89c52_register(void);

#ifdef __cplusplus
}
#endif

#define MCS51_FAMILY_SELECT_REGISTER() at89c52_register()
