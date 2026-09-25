/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef __ESP_ASSERT_H__
#define __ESP_ASSERT_H__
#ifndef __WINK_HARVESTED_ESP_ASSERT_H__
#define __WINK_HARVESTED_ESP_ASSERT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "assert.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_STATIC_ASSERT
#define ESP_STATIC_ASSERT static_assert
#endif
#ifndef TRY_STATIC_ASSERT
#define TRY_STATIC_ASSERT(CONDITION, MSG) do {                                                               ESP_STATIC_ASSERT(__builtin_choose_expr(__builtin_constant_p(CONDITION), (CONDITION), 1), #MSG);    assert(#MSG && (CONDITION));                                                                     } while(0)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_ASSERT_H__ */
#endif /* __ESP_ASSERT_H__ */
