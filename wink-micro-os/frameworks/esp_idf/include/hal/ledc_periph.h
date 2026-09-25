/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_LEDC_PERIPH_H
#define WINK_H_GUARD_HAL_LEDC_PERIPH_H
#ifndef __WINK_HARVESTED_HAL_LEDC_PERIPH_H__
#define __WINK_HARVESTED_HAL_LEDC_PERIPH_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "hal/ledc_types.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef LEDC_RETENTION_ENTRY
#define LEDC_RETENTION_ENTRY (ENTRY(0) | ENTRY(2))
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
const int irq_id;
    struct {
        const int sig_out_idx[SOC_LEDC_CHANNEL_NUM];
    } speed_mode[LEDC_SPEED_MODE_MAX];
} ledc_signal_conn_t;
typedef struct {
    const regdma_entries_config_t * regdma_entry_array;
    uint32_t array_size;
} ledc_sub_reg_retention_info_t;
typedef struct {
    ledc_sub_reg_retention_info_t common;
    ledc_sub_reg_retention_info_t timer[SOC_LEDC_TIMER_NUM];
    ledc_sub_reg_retention_info_t channel[SOC_LEDC_CHANNEL_NUM];
    const periph_retention_module_t module_id;
} ledc_reg_retention_info_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_LEDC_PERIPH_H__ */
#endif /* WINK_H_GUARD_HAL_LEDC_PERIPH_H */
