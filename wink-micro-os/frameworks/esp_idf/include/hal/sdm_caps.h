/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_SDM_CAPS_H
#define WINK_H_GUARD_HAL_SDM_CAPS_H
#ifndef __WINK_HARVESTED_HAL_SDM_CAPS_H__
#define __WINK_HARVESTED_HAL_SDM_CAPS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef SDM_CAPS_GET
#define SDM_CAPS_GET(_attr) _SDM_ ## _attr
#endif
#ifndef _SDM_CHANS_PER_INST
#define _SDM_CHANS_PER_INST 8
#endif
#ifndef _SDM_FUNC_CLOCK_SUPPORT_APB
#define _SDM_FUNC_CLOCK_SUPPORT_APB 1
#endif
#ifndef _SDM_INST_NUM
#define _SDM_INST_NUM 1
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_SDM_CAPS_H__ */
#endif /* WINK_H_GUARD_HAL_SDM_CAPS_H */
