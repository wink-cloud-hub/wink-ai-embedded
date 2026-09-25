/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_REGI2C_APLL_H
#define WINK_H_GUARD_SOC_REGI2C_APLL_H
#ifndef __WINK_HARVESTED_SOC_REGI2C_APLL_H__
#define __WINK_HARVESTED_SOC_REGI2C_APLL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef I2C_APLL
#define I2C_APLL 0X6D
#endif
#ifndef I2C_APLL_DSDM0
#define I2C_APLL_DSDM0 9
#endif
#ifndef I2C_APLL_DSDM0_LSB
#define I2C_APLL_DSDM0_LSB 0
#endif
#ifndef I2C_APLL_DSDM0_MSB
#define I2C_APLL_DSDM0_MSB 7
#endif
#ifndef I2C_APLL_DSDM1
#define I2C_APLL_DSDM1 8
#endif
#ifndef I2C_APLL_DSDM1_LSB
#define I2C_APLL_DSDM1_LSB 0
#endif
#ifndef I2C_APLL_DSDM1_MSB
#define I2C_APLL_DSDM1_MSB 7
#endif
#ifndef I2C_APLL_DSDM2
#define I2C_APLL_DSDM2 7
#endif
#ifndef I2C_APLL_DSDM2_LSB
#define I2C_APLL_DSDM2_LSB 0
#endif
#ifndef I2C_APLL_DSDM2_MSB
#define I2C_APLL_DSDM2_MSB 5
#endif
#ifndef I2C_APLL_EN_FAST_CAL
#define I2C_APLL_EN_FAST_CAL 4
#endif
#ifndef I2C_APLL_EN_FAST_CAL_LSB
#define I2C_APLL_EN_FAST_CAL_LSB 7
#endif
#ifndef I2C_APLL_EN_FAST_CAL_MSB
#define I2C_APLL_EN_FAST_CAL_MSB 7
#endif
#ifndef I2C_APLL_HOSTID
#define I2C_APLL_HOSTID 3
#endif
#ifndef I2C_APLL_IR_CAL_CK_DIV
#define I2C_APLL_IR_CAL_CK_DIV 2
#endif
#ifndef I2C_APLL_IR_CAL_CK_DIV_LSB
#define I2C_APLL_IR_CAL_CK_DIV_LSB 0
#endif
#ifndef I2C_APLL_IR_CAL_CK_DIV_MSB
#define I2C_APLL_IR_CAL_CK_DIV_MSB 3
#endif
#ifndef I2C_APLL_IR_CAL_DELAY
#define I2C_APLL_IR_CAL_DELAY 0
#endif
#ifndef I2C_APLL_IR_CAL_DELAY_LSB
#define I2C_APLL_IR_CAL_DELAY_LSB 0
#endif
#ifndef I2C_APLL_IR_CAL_DELAY_MSB
#define I2C_APLL_IR_CAL_DELAY_MSB 3
#endif
#ifndef I2C_APLL_IR_CAL_ENX_CAP
#define I2C_APLL_IR_CAL_ENX_CAP 1
#endif
#ifndef I2C_APLL_IR_CAL_ENX_CAP_LSB
#define I2C_APLL_IR_CAL_ENX_CAP_LSB 5
#endif
#ifndef I2C_APLL_IR_CAL_ENX_CAP_MSB
#define I2C_APLL_IR_CAL_ENX_CAP_MSB 5
#endif
#ifndef I2C_APLL_IR_CAL_EXT_CAP
#define I2C_APLL_IR_CAL_EXT_CAP 1
#endif
#ifndef I2C_APLL_IR_CAL_EXT_CAP_LSB
#define I2C_APLL_IR_CAL_EXT_CAP_LSB 0
#endif
#ifndef I2C_APLL_IR_CAL_EXT_CAP_MSB
#define I2C_APLL_IR_CAL_EXT_CAP_MSB 4
#endif
#ifndef I2C_APLL_IR_CAL_RSTB
#define I2C_APLL_IR_CAL_RSTB 0
#endif
#ifndef I2C_APLL_IR_CAL_RSTB_LSB
#define I2C_APLL_IR_CAL_RSTB_LSB 4
#endif
#ifndef I2C_APLL_IR_CAL_RSTB_MSB
#define I2C_APLL_IR_CAL_RSTB_MSB 4
#endif
#ifndef I2C_APLL_IR_CAL_START
#define I2C_APLL_IR_CAL_START 0
#endif
#ifndef I2C_APLL_IR_CAL_START_LSB
#define I2C_APLL_IR_CAL_START_LSB 5
#endif
#ifndef I2C_APLL_IR_CAL_START_MSB
#define I2C_APLL_IR_CAL_START_MSB 5
#endif
#ifndef I2C_APLL_IR_CAL_UNSTOP
#define I2C_APLL_IR_CAL_UNSTOP 0
#endif
#ifndef I2C_APLL_IR_CAL_UNSTOP_LSB
#define I2C_APLL_IR_CAL_UNSTOP_LSB 6
#endif
#ifndef I2C_APLL_IR_CAL_UNSTOP_MSB
#define I2C_APLL_IR_CAL_UNSTOP_MSB 6
#endif
#ifndef I2C_APLL_OC_DCHGP
#define I2C_APLL_OC_DCHGP 2
#endif
#ifndef I2C_APLL_OC_DCHGP_LSB
#define I2C_APLL_OC_DCHGP_LSB 4
#endif
#ifndef I2C_APLL_OC_DCHGP_MSB
#define I2C_APLL_OC_DCHGP_MSB 6
#endif
#ifndef I2C_APLL_OC_DHREF_SEL
#define I2C_APLL_OC_DHREF_SEL 5
#endif
#ifndef I2C_APLL_OC_DHREF_SEL_LSB
#define I2C_APLL_OC_DHREF_SEL_LSB 0
#endif
#ifndef I2C_APLL_OC_DHREF_SEL_MSB
#define I2C_APLL_OC_DHREF_SEL_MSB 1
#endif
#ifndef I2C_APLL_OC_DLREF_SEL
#define I2C_APLL_OC_DLREF_SEL 5
#endif
#ifndef I2C_APLL_OC_DLREF_SEL_LSB
#define I2C_APLL_OC_DLREF_SEL_LSB 2
#endif
#ifndef I2C_APLL_OC_DLREF_SEL_MSB
#define I2C_APLL_OC_DLREF_SEL_MSB 3
#endif
#ifndef I2C_APLL_OC_DVDD
#define I2C_APLL_OC_DVDD 6
#endif
#ifndef I2C_APLL_OC_DVDD_LSB
#define I2C_APLL_OC_DVDD_LSB 0
#endif
#ifndef I2C_APLL_OC_DVDD_MSB
#define I2C_APLL_OC_DVDD_MSB 4
#endif
#ifndef I2C_APLL_OC_ENB_FCAL
#define I2C_APLL_OC_ENB_FCAL 0
#endif
#ifndef I2C_APLL_OC_ENB_FCAL_LSB
#define I2C_APLL_OC_ENB_FCAL_LSB 7
#endif
#ifndef I2C_APLL_OC_ENB_FCAL_MSB
#define I2C_APLL_OC_ENB_FCAL_MSB 7
#endif
#ifndef I2C_APLL_OC_ENB_VCON
#define I2C_APLL_OC_ENB_VCON 2
#endif
#ifndef I2C_APLL_OC_ENB_VCON_LSB
#define I2C_APLL_OC_ENB_VCON_LSB 7
#endif
#ifndef I2C_APLL_OC_ENB_VCON_MSB
#define I2C_APLL_OC_ENB_VCON_MSB 7
#endif
#ifndef I2C_APLL_OC_LBW
#define I2C_APLL_OC_LBW 1
#endif
#ifndef I2C_APLL_OC_LBW_LSB
#define I2C_APLL_OC_LBW_LSB 6
#endif
#ifndef I2C_APLL_OC_LBW_MSB
#define I2C_APLL_OC_LBW_MSB 6
#endif
#ifndef I2C_APLL_OC_TSCHGP
#define I2C_APLL_OC_TSCHGP 4
#endif
#ifndef I2C_APLL_OC_TSCHGP_LSB
#define I2C_APLL_OC_TSCHGP_LSB 6
#endif
#ifndef I2C_APLL_OC_TSCHGP_MSB
#define I2C_APLL_OC_TSCHGP_MSB 6
#endif
#ifndef I2C_APLL_OR_CAL_CAP
#define I2C_APLL_OR_CAL_CAP 3
#endif
#ifndef I2C_APLL_OR_CAL_CAP_LSB
#define I2C_APLL_OR_CAL_CAP_LSB 0
#endif
#ifndef I2C_APLL_OR_CAL_CAP_MSB
#define I2C_APLL_OR_CAL_CAP_MSB 4
#endif
#ifndef I2C_APLL_OR_CAL_END
#define I2C_APLL_OR_CAL_END 3
#endif
#ifndef I2C_APLL_OR_CAL_END_LSB
#define I2C_APLL_OR_CAL_END_LSB 7
#endif
#ifndef I2C_APLL_OR_CAL_END_MSB
#define I2C_APLL_OR_CAL_END_MSB 7
#endif
#ifndef I2C_APLL_OR_CAL_OVF
#define I2C_APLL_OR_CAL_OVF 3
#endif
#ifndef I2C_APLL_OR_CAL_OVF_LSB
#define I2C_APLL_OR_CAL_OVF_LSB 6
#endif
#ifndef I2C_APLL_OR_CAL_OVF_MSB
#define I2C_APLL_OR_CAL_OVF_MSB 6
#endif
#ifndef I2C_APLL_OR_CAL_UDF
#define I2C_APLL_OR_CAL_UDF 3
#endif
#ifndef I2C_APLL_OR_CAL_UDF_LSB
#define I2C_APLL_OR_CAL_UDF_LSB 5
#endif
#ifndef I2C_APLL_OR_CAL_UDF_MSB
#define I2C_APLL_OR_CAL_UDF_MSB 5
#endif
#ifndef I2C_APLL_OR_OUTPUT_DIV
#define I2C_APLL_OR_OUTPUT_DIV 4
#endif
#ifndef I2C_APLL_OR_OUTPUT_DIV_LSB
#define I2C_APLL_OR_OUTPUT_DIV_LSB 0
#endif
#ifndef I2C_APLL_OR_OUTPUT_DIV_MSB
#define I2C_APLL_OR_OUTPUT_DIV_MSB 4
#endif
#ifndef I2C_APLL_SDM_DITHER
#define I2C_APLL_SDM_DITHER 5
#endif
#ifndef I2C_APLL_SDM_DITHER_LSB
#define I2C_APLL_SDM_DITHER_LSB 4
#endif
#ifndef I2C_APLL_SDM_DITHER_MSB
#define I2C_APLL_SDM_DITHER_MSB 4
#endif
#ifndef I2C_APLL_SDM_RSTB
#define I2C_APLL_SDM_RSTB 5
#endif
#ifndef I2C_APLL_SDM_RSTB_LSB
#define I2C_APLL_SDM_RSTB_LSB 6
#endif
#ifndef I2C_APLL_SDM_RSTB_MSB
#define I2C_APLL_SDM_RSTB_MSB 6
#endif
#ifndef I2C_APLL_SDM_STOP
#define I2C_APLL_SDM_STOP 5
#endif
#ifndef I2C_APLL_SDM_STOP_LSB
#define I2C_APLL_SDM_STOP_LSB 5
#endif
#ifndef I2C_APLL_SDM_STOP_MSB
#define I2C_APLL_SDM_STOP_MSB 5
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_REGI2C_APLL_H__ */
#endif /* WINK_H_GUARD_SOC_REGI2C_APLL_H */
