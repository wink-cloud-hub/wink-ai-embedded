/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef _SOC_PID_H_
#define _SOC_PID_H_
#ifndef __WINK_HARVESTED_SOC_PID_H__
#define __WINK_HARVESTED_SOC_PID_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef PROPID_CONFIG_INTERRUPT_ADDR_1
#define PROPID_CONFIG_INTERRUPT_ADDR_1 ((PROPID_GEN_BASE)+0x004)
#endif
#ifndef PROPID_CONFIG_INTERRUPT_ADDR_2
#define PROPID_CONFIG_INTERRUPT_ADDR_2 ((PROPID_GEN_BASE)+0x008)
#endif
#ifndef PROPID_CONFIG_INTERRUPT_ADDR_3
#define PROPID_CONFIG_INTERRUPT_ADDR_3 ((PROPID_GEN_BASE)+0x00C)
#endif
#ifndef PROPID_CONFIG_INTERRUPT_ADDR_4
#define PROPID_CONFIG_INTERRUPT_ADDR_4 ((PROPID_GEN_BASE)+0x010)
#endif
#ifndef PROPID_CONFIG_INTERRUPT_ADDR_5
#define PROPID_CONFIG_INTERRUPT_ADDR_5 ((PROPID_GEN_BASE)+0x014)
#endif
#ifndef PROPID_CONFIG_INTERRUPT_ADDR_6
#define PROPID_CONFIG_INTERRUPT_ADDR_6 ((PROPID_GEN_BASE)+0x018)
#endif
#ifndef PROPID_CONFIG_INTERRUPT_ADDR_7
#define PROPID_CONFIG_INTERRUPT_ADDR_7 ((PROPID_GEN_BASE)+0x01C)
#endif
#ifndef PROPID_CONFIG_INTERRUPT_ENABLE
#define PROPID_CONFIG_INTERRUPT_ENABLE ((PROPID_GEN_BASE)+0x000)
#endif
#ifndef PROPID_CONFIG_NMI_DELAY
#define PROPID_CONFIG_NMI_DELAY ((PROPID_GEN_BASE)+0x024)
#endif
#ifndef PROPID_CONFIG_PID_DELAY
#define PROPID_CONFIG_PID_DELAY ((PROPID_GEN_BASE)+0x020)
#endif
#ifndef PROPID_FROM_1
#define PROPID_FROM_1 ((PROPID_GEN_BASE)+0x02C)
#endif
#ifndef PROPID_FROM_2
#define PROPID_FROM_2 ((PROPID_GEN_BASE)+0x030)
#endif
#ifndef PROPID_FROM_3
#define PROPID_FROM_3 ((PROPID_GEN_BASE)+0x034)
#endif
#ifndef PROPID_FROM_4
#define PROPID_FROM_4 ((PROPID_GEN_BASE)+0x038)
#endif
#ifndef PROPID_FROM_5
#define PROPID_FROM_5 ((PROPID_GEN_BASE)+0x03C)
#endif
#ifndef PROPID_FROM_6
#define PROPID_FROM_6 ((PROPID_GEN_BASE)+0x040)
#endif
#ifndef PROPID_FROM_7
#define PROPID_FROM_7 ((PROPID_GEN_BASE)+0x044)
#endif
#ifndef PROPID_FROM_INT_MASK
#define PROPID_FROM_INT_MASK 0xF
#endif
#ifndef PROPID_FROM_INT_S
#define PROPID_FROM_INT_S 3
#endif
#ifndef PROPID_FROM_PID_MASK
#define PROPID_FROM_PID_MASK 0x7
#endif
#ifndef PROPID_FROM_PID_S
#define PROPID_FROM_PID_S 0
#endif
#ifndef PROPID_GEN_BASE
#define PROPID_GEN_BASE 0x3FF1F000
#endif
#ifndef PROPID_NMI_MASK_HW
#define PROPID_NMI_MASK_HW ((PROPID_GEN_BASE)+0x064)
#endif
#ifndef PROPID_PID
#define PROPID_PID ((PROPID_GEN_BASE)+0x060)
#endif
#ifndef PROPID_PID_CONFIRM
#define PROPID_PID_CONFIRM ((PROPID_GEN_BASE)+0x04c)
#endif
#ifndef PROPID_PID_NEW
#define PROPID_PID_NEW ((PROPID_GEN_BASE)+0x048)
#endif
#ifndef PROPID_PID_NMI_MASK_HW_DISABLE
#define PROPID_PID_NMI_MASK_HW_DISABLE ((PROPID_GEN_BASE)+0x058)
#endif
#ifndef PROPID_PID_NMI_MASK_HW_ENABLE
#define PROPID_PID_NMI_MASK_HW_ENABLE ((PROPID_GEN_BASE)+0x054)
#endif
#ifndef PROPID_PID_NMI_MASK_HW_REG
#define PROPID_PID_NMI_MASK_HW_REG ((PROPID_GEN_BASE)+0x05c)
#endif
#ifndef PROPID_PID_REG
#define PROPID_PID_REG ((PROPID_GEN_BASE)+0x050)
#endif
#ifndef PROPID_TABLE_LEVEL
#define PROPID_TABLE_LEVEL ((PROPID_GEN_BASE)+0x028)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_PID_H__ */
#endif /* _SOC_PID_H_ */
