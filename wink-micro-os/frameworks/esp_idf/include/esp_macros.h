/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_MACROS_H
#define WINK_H_GUARD_ESP_MACROS_H
#ifndef __WINK_HARVESTED_ESP_MACROS_H__
#define __WINK_HARVESTED_ESP_MACROS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <assert.h>

#include "esp_assert.h"
#include "esp_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef CHOOSE_MACRO_VA_ARG
#define CHOOSE_MACRO_VA_ARG(MACRO_WITH_ARGS, MACRO_WITH_NO_ARGS, ...) CHOOSE_MACRO_VA_ARG_INN(0, ##__VA_ARGS__, MACRO_WITH_ARGS, MACRO_WITH_NO_ARGS, 0)
#endif
#ifndef CHOOSE_MACRO_VA_ARG_INN
#define CHOOSE_MACRO_VA_ARG_INN(one, two, MACRO1, MACRO2, ...) MACRO1
#endif
#ifndef ESP_GET_NTH_ARG
#define ESP_GET_NTH_ARG (  _01,_02,_03,_04,_05,_06,_07,_08,_09,_10,  _11,_12,_13,_14,_15,_16,_17,_18,_19,_20,  _21,_22,_23,_24,_25,_26,_27,_28,_29,_30,  _31,_32,_33,_34,_35,_36,_37,_38,_39,_40,  _41,_42,_43,_44,_45,_46,_47,_48,_49,_50,  _51,_52,_53,_54,_55,_56,_57,_58,_59,_60,  _61,_62,_63,N,...) N
#endif
#ifndef ESP_INFINITE_LOOP
#define ESP_INFINITE_LOOP() do {  ESP_COMPILER_DIAGNOSTIC_PUSH_IGNORE("-Wanalyzer-infinite-loop")  while(1);  ESP_COMPILER_DIAGNOSTIC_POP("-Wanalyzer-infinite-loop")  } while(1)
#endif
#ifndef ESP_NARG
#define ESP_NARG(...) ESP_GET_NTH_ARG(__VA_ARGS__)
#endif
#ifndef ESP_RSEQ_N
#define ESP_RSEQ_N() 62,61,60,                       59,58,57,56,55,54,53,52,51,50,  49,48,47,46,45,44,43,42,41,40,  39,38,37,36,35,34,33,32,31,30,  29,28,27,26,25,24,23,22,21,20,  19,18,17,16,15,14,13,12,11,10,  9, 8, 7, 6, 5, 4, 3, 2, 1, 0
#endif
#ifndef ESP_UNUSED
#define ESP_UNUSED(x) ((void)(x))
#endif
#ifndef ESP_VA_NARG
#define ESP_VA_NARG(...) ESP_NARG(_0, ##__VA_ARGS__, ESP_RSEQ_N())
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_MACROS_H__ */
#endif /* WINK_H_GUARD_ESP_MACROS_H */
