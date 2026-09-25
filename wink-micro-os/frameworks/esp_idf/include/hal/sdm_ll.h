/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_SDM_LL_H
#define WINK_H_GUARD_HAL_SDM_LL_H
#ifndef __WINK_HARVESTED_HAL_SDM_LL_H__
#define __WINK_HARVESTED_HAL_SDM_LL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef SDM_LL_GET_HW
#define SDM_LL_GET_HW(group_id) ((group_id == 0) ? (&SDM) : NULL)
#endif
#ifndef SDM_LL_PRESCALE_MAX
#define SDM_LL_PRESCALE_MAX (GPIO_SD0_PRESCALE_V + 1)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void sdm_ll_enable_clock(gpio_sd_dev_t *hw, bool en) WINK_SLA_ERROR("Wink SLA Violation: sdm_ll_enable_clock out of Core 8 scope.");
#else
void sdm_ll_enable_clock(gpio_sd_dev_t *hw, bool en);
#endif

#if defined(__WINK_SIM__)
void sdm_ll_set_prescale(gpio_sd_dev_t *hw, int channel, uint32_t prescale) WINK_SLA_ERROR("Wink SLA Violation: sdm_ll_set_prescale out of Core 8 scope.");
#else
void sdm_ll_set_prescale(gpio_sd_dev_t *hw, int channel, uint32_t prescale);
#endif

#if defined(__WINK_SIM__)
void sdm_ll_set_pulse_density(gpio_sd_dev_t *hw, int channel, int8_t density) WINK_SLA_ERROR("Wink SLA Violation: sdm_ll_set_pulse_density out of Core 8 scope.");
#else
void sdm_ll_set_pulse_density(gpio_sd_dev_t *hw, int channel, int8_t density);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_SDM_LL_H__ */
#endif /* WINK_H_GUARD_HAL_SDM_LL_H */
