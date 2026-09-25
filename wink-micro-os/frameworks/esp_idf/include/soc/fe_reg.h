/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_FE_REG_H
#define WINK_H_GUARD_SOC_FE_REG_H
#ifndef __WINK_HARVESTED_SOC_FE_REG_H__
#define __WINK_HARVESTED_SOC_FE_REG_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef FE2_TX_INF_FORCE_PD
#define FE2_TX_INF_FORCE_PD (BIT(9))
#endif
#ifndef FE2_TX_INF_FORCE_PD_M
#define FE2_TX_INF_FORCE_PD_M (BIT(9))
#endif
#ifndef FE2_TX_INF_FORCE_PD_S
#define FE2_TX_INF_FORCE_PD_S 9
#endif
#ifndef FE2_TX_INF_FORCE_PD_V
#define FE2_TX_INF_FORCE_PD_V 1
#endif
#ifndef FE2_TX_INF_FORCE_PU
#define FE2_TX_INF_FORCE_PU (BIT(10))
#endif
#ifndef FE2_TX_INF_FORCE_PU_M
#define FE2_TX_INF_FORCE_PU_M (BIT(10))
#endif
#ifndef FE2_TX_INF_FORCE_PU_S
#define FE2_TX_INF_FORCE_PU_S 10
#endif
#ifndef FE2_TX_INF_FORCE_PU_V
#define FE2_TX_INF_FORCE_PU_V 1
#endif
#ifndef FE2_TX_INTERP_CTRL
#define FE2_TX_INTERP_CTRL (DR_REG_FE2_BASE + 0x00f0)
#endif
#ifndef FE_GEN_CTRL
#define FE_GEN_CTRL (DR_REG_FE_BASE + 0x0090)
#endif
#ifndef FE_IQ_EST_FORCE_PD
#define FE_IQ_EST_FORCE_PD (BIT(4))
#endif
#ifndef FE_IQ_EST_FORCE_PD_M
#define FE_IQ_EST_FORCE_PD_M (BIT(4))
#endif
#ifndef FE_IQ_EST_FORCE_PD_S
#define FE_IQ_EST_FORCE_PD_S 4
#endif
#ifndef FE_IQ_EST_FORCE_PD_V
#define FE_IQ_EST_FORCE_PD_V 1
#endif
#ifndef FE_IQ_EST_FORCE_PU
#define FE_IQ_EST_FORCE_PU (BIT(5))
#endif
#ifndef FE_IQ_EST_FORCE_PU_M
#define FE_IQ_EST_FORCE_PU_M (BIT(5))
#endif
#ifndef FE_IQ_EST_FORCE_PU_S
#define FE_IQ_EST_FORCE_PU_S 5
#endif
#ifndef FE_IQ_EST_FORCE_PU_V
#define FE_IQ_EST_FORCE_PU_V 1
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_FE_REG_H__ */
#endif /* WINK_H_GUARD_SOC_FE_REG_H */
