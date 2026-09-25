/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef _SOC_BB_REG_H_
#define _SOC_BB_REG_H_
#ifndef __WINK_HARVESTED_SOC_BB_REG_H__
#define __WINK_HARVESTED_SOC_BB_REG_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef BBPD_CTRL
#define BBPD_CTRL (DR_REG_BB_BASE + 0x0054)
#endif
#ifndef BB_DC_EST_FORCE_PD
#define BB_DC_EST_FORCE_PD (BIT(0))
#endif
#ifndef BB_DC_EST_FORCE_PD_M
#define BB_DC_EST_FORCE_PD_M (BIT(0))
#endif
#ifndef BB_DC_EST_FORCE_PD_S
#define BB_DC_EST_FORCE_PD_S 0
#endif
#ifndef BB_DC_EST_FORCE_PD_V
#define BB_DC_EST_FORCE_PD_V 1
#endif
#ifndef BB_DC_EST_FORCE_PU
#define BB_DC_EST_FORCE_PU (BIT(1))
#endif
#ifndef BB_DC_EST_FORCE_PU_M
#define BB_DC_EST_FORCE_PU_M (BIT(1))
#endif
#ifndef BB_DC_EST_FORCE_PU_S
#define BB_DC_EST_FORCE_PU_S 1
#endif
#ifndef BB_DC_EST_FORCE_PU_V
#define BB_DC_EST_FORCE_PU_V 1
#endif
#ifndef BB_FFT_FORCE_PD
#define BB_FFT_FORCE_PD (BIT(2))
#endif
#ifndef BB_FFT_FORCE_PD_M
#define BB_FFT_FORCE_PD_M (BIT(2))
#endif
#ifndef BB_FFT_FORCE_PD_S
#define BB_FFT_FORCE_PD_S 2
#endif
#ifndef BB_FFT_FORCE_PD_V
#define BB_FFT_FORCE_PD_V 1
#endif
#ifndef BB_FFT_FORCE_PU
#define BB_FFT_FORCE_PU (BIT(3))
#endif
#ifndef BB_FFT_FORCE_PU_M
#define BB_FFT_FORCE_PU_M (BIT(3))
#endif
#ifndef BB_FFT_FORCE_PU_S
#define BB_FFT_FORCE_PU_S 3
#endif
#ifndef BB_FFT_FORCE_PU_V
#define BB_FFT_FORCE_PU_V 1
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_BB_REG_H__ */
#endif /* _SOC_BB_REG_H_ */
