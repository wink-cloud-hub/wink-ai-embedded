/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_SDM_HAL_H
#define WINK_H_GUARD_HAL_SDM_HAL_H
#ifndef __WINK_HARVESTED_HAL_SDM_HAL_H__
#define __WINK_HARVESTED_HAL_SDM_HAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct gpio_sd_dev_t * sdm_soc_handle_t;
typedef struct {
    sdm_soc_handle_t dev;
} sdm_hal_context_t;
typedef struct {
    int group_id;
} sdm_hal_init_config_t;



#if defined(__WINK_SIM__)
void sdm_hal_deinit(sdm_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: sdm_hal_deinit out of Core 8 scope.");
#else
void sdm_hal_deinit(sdm_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void sdm_hal_init(sdm_hal_context_t *hal, const sdm_hal_init_config_t *config) WINK_SLA_ERROR("Wink SLA Violation: sdm_hal_init out of Core 8 scope.");
#else
void sdm_hal_init(sdm_hal_context_t *hal, const sdm_hal_init_config_t *config);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_SDM_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_SDM_HAL_H */
