// SPDX-License-Identifier: Apache-2.0
// mcs51_family_select.h — CMS8S78xx fixture (TEST USE ONLY).
//
// Checked-in stand-in for the codegen'd production header until the external
// wink-tools generator lands (stage6 prerequisite). The host test build puts
// this directory on wink_mcs51_core's PRIVATE include path so mcs51_bridge.cpp
// takes its __has_include("mcs51_family_select.h") branch and registers the
// chip package before the first context reset — exactly the production call
// order. Replace with the generated header when the generator is available.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void cms8s78xx_register(void);

#ifdef __cplusplus
}
#endif

#define MCS51_FAMILY_SELECT_REGISTER() cms8s78xx_register()
