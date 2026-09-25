/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_NRX_REG_H
#define WINK_H_GUARD_SOC_NRX_REG_H
#ifndef __WINK_HARVESTED_SOC_NRX_REG_H__
#define __WINK_HARVESTED_SOC_NRX_REG_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef NRXPD_CTRL
#define NRXPD_CTRL (DR_REG_NRX_BASE + 0x00d4)
#endif
#ifndef NRX_CHAN_EST_FORCE_PD
#define NRX_CHAN_EST_FORCE_PD (BIT(6))
#endif
#ifndef NRX_CHAN_EST_FORCE_PD_M
#define NRX_CHAN_EST_FORCE_PD_M (BIT(6))
#endif
#ifndef NRX_CHAN_EST_FORCE_PD_S
#define NRX_CHAN_EST_FORCE_PD_S 6
#endif
#ifndef NRX_CHAN_EST_FORCE_PD_V
#define NRX_CHAN_EST_FORCE_PD_V 1
#endif
#ifndef NRX_CHAN_EST_FORCE_PU
#define NRX_CHAN_EST_FORCE_PU (BIT(7))
#endif
#ifndef NRX_CHAN_EST_FORCE_PU_M
#define NRX_CHAN_EST_FORCE_PU_M (BIT(7))
#endif
#ifndef NRX_CHAN_EST_FORCE_PU_S
#define NRX_CHAN_EST_FORCE_PU_S 7
#endif
#ifndef NRX_CHAN_EST_FORCE_PU_V
#define NRX_CHAN_EST_FORCE_PU_V 1
#endif
#ifndef NRX_DEMAP_FORCE_PD
#define NRX_DEMAP_FORCE_PD (BIT(0))
#endif
#ifndef NRX_DEMAP_FORCE_PD_M
#define NRX_DEMAP_FORCE_PD_M (BIT(0))
#endif
#ifndef NRX_DEMAP_FORCE_PD_S
#define NRX_DEMAP_FORCE_PD_S 0
#endif
#ifndef NRX_DEMAP_FORCE_PD_V
#define NRX_DEMAP_FORCE_PD_V 1
#endif
#ifndef NRX_DEMAP_FORCE_PU
#define NRX_DEMAP_FORCE_PU (BIT(1))
#endif
#ifndef NRX_DEMAP_FORCE_PU_M
#define NRX_DEMAP_FORCE_PU_M (BIT(1))
#endif
#ifndef NRX_DEMAP_FORCE_PU_S
#define NRX_DEMAP_FORCE_PU_S 1
#endif
#ifndef NRX_DEMAP_FORCE_PU_V
#define NRX_DEMAP_FORCE_PU_V 1
#endif
#ifndef NRX_RX_ROT_FORCE_PD
#define NRX_RX_ROT_FORCE_PD (BIT(4))
#endif
#ifndef NRX_RX_ROT_FORCE_PD_M
#define NRX_RX_ROT_FORCE_PD_M (BIT(4))
#endif
#ifndef NRX_RX_ROT_FORCE_PD_S
#define NRX_RX_ROT_FORCE_PD_S 4
#endif
#ifndef NRX_RX_ROT_FORCE_PD_V
#define NRX_RX_ROT_FORCE_PD_V 1
#endif
#ifndef NRX_RX_ROT_FORCE_PU
#define NRX_RX_ROT_FORCE_PU (BIT(5))
#endif
#ifndef NRX_RX_ROT_FORCE_PU_M
#define NRX_RX_ROT_FORCE_PU_M (BIT(5))
#endif
#ifndef NRX_RX_ROT_FORCE_PU_S
#define NRX_RX_ROT_FORCE_PU_S 5
#endif
#ifndef NRX_RX_ROT_FORCE_PU_V
#define NRX_RX_ROT_FORCE_PU_V 1
#endif
#ifndef NRX_VIT_FORCE_PD
#define NRX_VIT_FORCE_PD (BIT(2))
#endif
#ifndef NRX_VIT_FORCE_PD_M
#define NRX_VIT_FORCE_PD_M (BIT(2))
#endif
#ifndef NRX_VIT_FORCE_PD_S
#define NRX_VIT_FORCE_PD_S 2
#endif
#ifndef NRX_VIT_FORCE_PD_V
#define NRX_VIT_FORCE_PD_V 1
#endif
#ifndef NRX_VIT_FORCE_PU
#define NRX_VIT_FORCE_PU (BIT(3))
#endif
#ifndef NRX_VIT_FORCE_PU_M
#define NRX_VIT_FORCE_PU_M (BIT(3))
#endif
#ifndef NRX_VIT_FORCE_PU_S
#define NRX_VIT_FORCE_PU_S 3
#endif
#ifndef NRX_VIT_FORCE_PU_V
#define NRX_VIT_FORCE_PU_V 1
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_NRX_REG_H__ */
#endif /* WINK_H_GUARD_SOC_NRX_REG_H */
