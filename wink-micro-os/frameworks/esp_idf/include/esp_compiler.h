/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_COMPILER_H
#define WINK_H_GUARD_ESP_COMPILER_H
#ifndef __WINK_HARVESTED_ESP_COMPILER_H__
#define __WINK_HARVESTED_ESP_COMPILER_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_STR
#define ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_STR(member, value) .member = value,
#endif
#ifndef ESP_COMPILER_DIAGNOSTIC_POP
#define ESP_COMPILER_DIAGNOSTIC_POP(warning) __COMPILER_PRAGMA__(clang diagnostic pop)
#endif
#ifndef ESP_COMPILER_DIAGNOSTIC_PUSH_IGNORE
#define ESP_COMPILER_DIAGNOSTIC_PUSH_IGNORE(warning) __COMPILER_PRAGMA__(clang diagnostic push)  __COMPILER_PRAGMA__(clang diagnostic ignored "-Wunknown-warning-option")  __COMPILER_PRAGMA__(clang diagnostic ignored warning)
#endif
#ifndef _COMPILER_PRAGMA_
#define _COMPILER_PRAGMA_(string) __COMPILER_PRAGMA__(string)
#endif
#ifndef __COMPILER_PRAGMA__
#define __COMPILER_PRAGMA__(string) _Pragma(#string)
#endif
#ifndef likely
#define likely(x) (x)
#endif
#ifndef unlikely
#define unlikely(x) (x)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_COMPILER_H__ */
#endif /* WINK_H_GUARD_ESP_COMPILER_H */
