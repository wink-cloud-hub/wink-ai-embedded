/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef _SOC_FRC_TIMER_REG_H_
#define _SOC_FRC_TIMER_REG_H_
#ifndef __WINK_HARVESTED_SOC_FRC_TIMER_REG_H__
#define __WINK_HARVESTED_SOC_FRC_TIMER_REG_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef FRC_TIMER_ALARM
#define FRC_TIMER_ALARM 0xFFFFFFFF
#endif
#ifndef FRC_TIMER_ALARM_REG
#define FRC_TIMER_ALARM_REG(i) (REG_FRC_TIMER_BASE(i) + 0x10)
#endif
#ifndef FRC_TIMER_ALARM_S
#define FRC_TIMER_ALARM_S 0
#endif
#ifndef FRC_TIMER_AUTOLOAD
#define FRC_TIMER_AUTOLOAD (BIT(6))
#endif
#ifndef FRC_TIMER_COUNT
#define FRC_TIMER_COUNT ((i == 0)?0x007FFFFF:0xffffffff)
#endif
#ifndef FRC_TIMER_COUNT_REG
#define FRC_TIMER_COUNT_REG(i) (REG_FRC_TIMER_BASE(i) + 0x4)
#endif
#ifndef FRC_TIMER_COUNT_S
#define FRC_TIMER_COUNT_S 0
#endif
#ifndef FRC_TIMER_CTRL_REG
#define FRC_TIMER_CTRL_REG(i) (REG_FRC_TIMER_BASE(i) + 0x8)
#endif
#ifndef FRC_TIMER_ENABLE
#define FRC_TIMER_ENABLE (BIT(7))
#endif
#ifndef FRC_TIMER_INT_CLR
#define FRC_TIMER_INT_CLR (BIT(0))
#endif
#ifndef FRC_TIMER_INT_REG
#define FRC_TIMER_INT_REG(i) (REG_FRC_TIMER_BASE(i) + 0xC)
#endif
#ifndef FRC_TIMER_INT_STATUS
#define FRC_TIMER_INT_STATUS (BIT(8))
#endif
#ifndef FRC_TIMER_LEVEL_INT
#define FRC_TIMER_LEVEL_INT (BIT(0))
#endif
#ifndef FRC_TIMER_LOAD_REG
#define FRC_TIMER_LOAD_REG(i) (REG_FRC_TIMER_BASE(i) + 0x0)
#endif
#ifndef FRC_TIMER_LOAD_VALUE
#define FRC_TIMER_LOAD_VALUE(i) ((i == 0)?0x007FFFFF:0xffffffff)
#endif
#ifndef FRC_TIMER_LOAD_VALUE_S
#define FRC_TIMER_LOAD_VALUE_S 0
#endif
#ifndef FRC_TIMER_PRESCALER
#define FRC_TIMER_PRESCALER 0x00000007
#endif
#ifndef FRC_TIMER_PRESCALER_1
#define FRC_TIMER_PRESCALER_1 (0 << FRC_TIMER_PRESCALER_S)
#endif
#ifndef FRC_TIMER_PRESCALER_16
#define FRC_TIMER_PRESCALER_16 (2 << FRC_TIMER_PRESCALER_S)
#endif
#ifndef FRC_TIMER_PRESCALER_256
#define FRC_TIMER_PRESCALER_256 (4 << FRC_TIMER_PRESCALER_S)
#endif
#ifndef FRC_TIMER_PRESCALER_S
#define FRC_TIMER_PRESCALER_S 1
#endif
#ifndef REG_FRC_TIMER_BASE
#define REG_FRC_TIMER_BASE(i) (DR_REG_FRC_TIMER_BASE + i*0x20)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_FRC_TIMER_REG_H__ */
#endif /* _SOC_FRC_TIMER_REG_H_ */
