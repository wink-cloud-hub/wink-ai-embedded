/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_SYSTEM_INTR_H
#define WINK_H_GUARD_SOC_SYSTEM_INTR_H
#ifndef __WINK_HARVESTED_SOC_SYSTEM_INTR_H__
#define __WINK_HARVESTED_SOC_SYSTEM_INTR_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef SYS_CPU_INTR_FROM_CPU_0_SOURCE
#define SYS_CPU_INTR_FROM_CPU_0_SOURCE ETS_FROM_CPU_INTR0_SOURCE
#endif
#ifndef SYS_CPU_INTR_FROM_CPU_1_SOURCE
#define SYS_CPU_INTR_FROM_CPU_1_SOURCE ETS_FROM_CPU_INTR1_SOURCE
#endif
#ifndef SYS_CPU_INTR_FROM_CPU_2_SOURCE
#define SYS_CPU_INTR_FROM_CPU_2_SOURCE ETS_FROM_CPU_INTR2_SOURCE
#endif
#ifndef SYS_CPU_INTR_FROM_CPU_3_SOURCE
#define SYS_CPU_INTR_FROM_CPU_3_SOURCE ETS_FROM_CPU_INTR3_SOURCE
#endif
#ifndef SYS_TG0_WDT_INTR_SOURCE
#define SYS_TG0_WDT_INTR_SOURCE ETS_TG0_WDT_LEVEL_INTR_SOURCE
#endif
#ifndef SYS_TG1_WDT_INTR_SOURCE
#define SYS_TG1_WDT_INTR_SOURCE ETS_TG1_WDT_LEVEL_INTR_SOURCE
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_SYSTEM_INTR_H__ */
#endif /* WINK_H_GUARD_SOC_SYSTEM_INTR_H */
