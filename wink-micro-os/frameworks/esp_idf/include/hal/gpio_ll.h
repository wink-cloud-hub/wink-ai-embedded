/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_GPIO_LL_H
#define WINK_H_GUARD_HAL_GPIO_LL_H
#ifndef __WINK_HARVESTED_HAL_GPIO_LL_H__
#define __WINK_HARVESTED_HAL_GPIO_LL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>

#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef GPIO_LL_APP_CPU_INTR_ENA
#define GPIO_LL_APP_CPU_INTR_ENA (BIT(0))
#endif
#ifndef GPIO_LL_APP_CPU_NMI_INTR_ENA
#define GPIO_LL_APP_CPU_NMI_INTR_ENA (BIT(1))
#endif
#ifndef GPIO_LL_GET_HW
#define GPIO_LL_GET_HW(num) (((num) == 0) ? (&GPIO) : NULL)
#endif
#ifndef GPIO_LL_INTR_SOURCE0
#define GPIO_LL_INTR_SOURCE0 ETS_GPIO_INTR_SOURCE
#endif
#ifndef GPIO_LL_PRO_CPU_INTR_ENA
#define GPIO_LL_PRO_CPU_INTR_ENA (BIT(2))
#endif
#ifndef GPIO_LL_PRO_CPU_NMI_INTR_ENA
#define GPIO_LL_PRO_CPU_NMI_INTR_ENA (BIT(3))
#endif
#ifndef GPIO_LL_SDIO_EXT_INTR_ENA
#define GPIO_LL_SDIO_EXT_INTR_ENA (BIT(4))
#endif
#ifndef gpio_ll_deep_sleep_hold_dis
#define gpio_ll_deep_sleep_hold_dis(...) (void)__DECLARE_RCC_ATOMIC_ENV; _gpio_ll_deep_sleep_hold_dis(__VA_ARGS__)
#endif
#ifndef gpio_ll_deep_sleep_hold_en
#define gpio_ll_deep_sleep_hold_en(...) (void)__DECLARE_RCC_ATOMIC_ENV; _gpio_ll_deep_sleep_hold_en(__VA_ARGS__)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

void _gpio_ll_deep_sleep_hold_dis(gpio_dev_t *hw);
void _gpio_ll_deep_sleep_hold_en(gpio_dev_t *hw);
void gpio_ll_clear_intr_status(gpio_dev_t *hw, uint32_t mask);
void gpio_ll_clear_intr_status_high(gpio_dev_t *hw, uint32_t mask);
bool gpio_ll_deep_sleep_hold_is_en(gpio_dev_t *hw);
void gpio_ll_func_sel(gpio_dev_t *hw, uint8_t gpio_num, uint32_t func);
void gpio_ll_get_drive_capability(gpio_dev_t *hw, uint32_t gpio_num, gpio_drive_cap_t *strength);
int gpio_ll_get_in_signal_connected_io(gpio_dev_t *hw, uint32_t in_sig_idx);
void gpio_ll_get_intr_status(gpio_dev_t *hw, uint32_t core_id, uint32_t *status);
void gpio_ll_get_intr_status_high(gpio_dev_t *hw, uint32_t core_id, uint32_t *status);
void gpio_ll_get_io_config(gpio_dev_t *hw, uint32_t gpio_num, gpio_io_config_t *io_config);
int gpio_ll_get_level(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_hold_dis(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_hold_en(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_input_disable(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_input_enable(gpio_dev_t *hw, uint32_t gpio_num);
bool gpio_ll_input_is_enabled(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_intr_disable(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_intr_enable_on_core(gpio_dev_t *hw, uint32_t core_id, uint32_t gpio_num);
bool gpio_ll_is_digital_io_hold(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_od_disable(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_od_enable(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_output_disable(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_output_enable(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_pulldown_dis(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_pulldown_en(gpio_dev_t *hw, uint32_t gpio_num);
bool gpio_ll_pulldown_is_enabled(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_pullup_dis(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_pullup_en(gpio_dev_t *hw, uint32_t gpio_num);
bool gpio_ll_pullup_is_enabled(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_set_drive_capability(gpio_dev_t *hw, uint32_t gpio_num, gpio_drive_cap_t strength);
void gpio_ll_set_input_signal_from(gpio_dev_t *hw, uint32_t signal_idx, bool from_gpio_matrix);
void gpio_ll_set_input_signal_matrix_source(gpio_dev_t *hw, uint32_t signal_idx, uint32_t gpio_num, bool in_inv);
void gpio_ll_set_intr_type(gpio_dev_t *hw, uint32_t gpio_num, gpio_int_type_t intr_type);
void gpio_ll_set_level(gpio_dev_t *hw, uint32_t gpio_num, uint32_t level);
void gpio_ll_set_output_enable_ctrl(gpio_dev_t *hw, uint8_t gpio_num, bool ctrl_by_periph, bool oen_inv);
void gpio_ll_set_output_signal_matrix_source(gpio_dev_t *hw, uint32_t gpio_num, uint32_t signal_idx, bool out_inv);
void gpio_ll_sleep_input_disable(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_sleep_input_enable(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_sleep_output_disable(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_sleep_output_enable(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_sleep_pulldown_dis(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_sleep_pulldown_en(gpio_dev_t *hw, uint32_t gpio_num);
bool gpio_ll_sleep_pulldown_is_enabled(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_sleep_pullup_dis(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_sleep_pullup_en(gpio_dev_t *hw, uint32_t gpio_num);
bool gpio_ll_sleep_pullup_is_enabled(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_sleep_sel_dis(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_sleep_sel_en(gpio_dev_t *hw, uint32_t gpio_num);
bool gpio_ll_sleep_sel_is_enabled(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_wakeup_disable(gpio_dev_t *hw, uint32_t gpio_num);
void gpio_ll_wakeup_enable(gpio_dev_t *hw, uint32_t gpio_num);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_GPIO_LL_H__ */
#endif /* WINK_H_GUARD_HAL_GPIO_LL_H */
