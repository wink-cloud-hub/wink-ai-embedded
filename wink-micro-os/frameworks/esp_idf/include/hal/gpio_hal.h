/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_GPIO_HAL_H
#define WINK_H_GUARD_HAL_GPIO_HAL_H
#ifndef __WINK_HARVESTED_HAL_GPIO_HAL_H__
#define __WINK_HARVESTED_HAL_GPIO_HAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "hal/gpio_types.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef GPIO_HAL_GET_HW
#define GPIO_HAL_GET_HW(num) GPIO_LL_GET_HW(num)
#endif
#ifndef gpio_hal_clear_intr_status_bit
#define gpio_hal_clear_intr_status_bit(hal, gpio_num) (((gpio_num) < 32) ? gpio_ll_clear_intr_status((hal)->dev, 1 << gpio_num)  : gpio_ll_clear_intr_status_high((hal)->dev, 1 << (gpio_num - 32)))
#endif
#ifndef gpio_hal_deep_sleep_hold_dis
#define gpio_hal_deep_sleep_hold_dis(hal) gpio_ll_deep_sleep_hold_dis((hal)->dev)
#endif
#ifndef gpio_hal_deep_sleep_hold_en
#define gpio_hal_deep_sleep_hold_en(hal) gpio_ll_deep_sleep_hold_en((hal)->dev)
#endif
#ifndef gpio_hal_deep_sleep_hold_is_en
#define gpio_hal_deep_sleep_hold_is_en(hal) gpio_ll_deep_sleep_hold_is_en((hal)->dev)
#endif
#ifndef gpio_hal_force_hold_all
#define gpio_hal_force_hold_all() gpio_ll_force_hold_all()
#endif
#ifndef gpio_hal_force_unhold_all
#define gpio_hal_force_unhold_all() gpio_ll_force_unhold_all()
#endif
#ifndef gpio_hal_func_sel
#define gpio_hal_func_sel(hal, gpio_num, func) gpio_ll_func_sel((hal)->dev, gpio_num, func)
#endif
#ifndef gpio_hal_get_drive_capability
#define gpio_hal_get_drive_capability(hal, gpio_num, strength) gpio_ll_get_drive_capability((hal)->dev, gpio_num, strength)
#endif
#ifndef gpio_hal_get_in_signal_connected_io
#define gpio_hal_get_in_signal_connected_io(hal, in_sig_idx) gpio_ll_get_in_signal_connected_io((hal)->dev, in_sig_idx)
#endif
#ifndef gpio_hal_get_intr_status
#define gpio_hal_get_intr_status(hal, core_id, status) gpio_ll_get_intr_status((hal)->dev, core_id, status)
#endif
#ifndef gpio_hal_get_intr_status_high
#define gpio_hal_get_intr_status_high(hal, core_id, status) gpio_ll_get_intr_status_high((hal)->dev, core_id, status)
#endif
#ifndef gpio_hal_get_io_config
#define gpio_hal_get_io_config(hal, gpio_num, out_io_config) gpio_ll_get_io_config((hal)->dev, gpio_num, out_io_config)
#endif
#ifndef gpio_hal_get_level
#define gpio_hal_get_level(hal, gpio_num) gpio_ll_get_level((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_hold_dis
#define gpio_hal_hold_dis(hal, gpio_num) gpio_ll_hold_dis((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_hold_en
#define gpio_hal_hold_en(hal, gpio_num) gpio_ll_hold_en((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_hysteresis_from_efuse
#define gpio_hal_hysteresis_from_efuse(hal, gpio_num) gpio_ll_pin_input_hysteresis_ctrl_sel_efuse((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_input_disable
#define gpio_hal_input_disable(hal, gpio_num) gpio_ll_input_disable((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_input_enable
#define gpio_hal_input_enable(hal, gpio_num) gpio_ll_input_enable((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_input_is_enabled
#define gpio_hal_input_is_enabled(hal, gpio_num) gpio_ll_input_is_enabled((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_is_digital_io_hold
#define gpio_hal_is_digital_io_hold(hal, gpio_num) gpio_ll_is_digital_io_hold((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_matrix_out_default
#define gpio_hal_matrix_out_default(hal, gpio_num) gpio_ll_set_output_signal_matrix_source((hal)->dev, gpio_num, SIG_GPIO_OUT_IDX, false)
#endif
#ifndef gpio_hal_od_disable
#define gpio_hal_od_disable(hal, gpio_num) gpio_ll_od_disable((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_od_enable
#define gpio_hal_od_enable(hal, gpio_num) gpio_ll_od_enable((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_output_disable
#define gpio_hal_output_disable(hal, gpio_num) gpio_ll_output_disable((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_output_enable
#define gpio_hal_output_enable(hal, gpio_num) gpio_ll_output_enable((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_pulldown_dis
#define gpio_hal_pulldown_dis(hal, gpio_num) gpio_ll_pulldown_dis((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_pulldown_en
#define gpio_hal_pulldown_en(hal, gpio_num) gpio_ll_pulldown_en((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_pullup_dis
#define gpio_hal_pullup_dis(hal, gpio_num) gpio_ll_pullup_dis((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_pullup_en
#define gpio_hal_pullup_en(hal, gpio_num) gpio_ll_pullup_en((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_set_drive_capability
#define gpio_hal_set_drive_capability(hal, gpio_num, strength) gpio_ll_set_drive_capability((hal)->dev, gpio_num, strength)
#endif
#ifndef gpio_hal_set_intr_type
#define gpio_hal_set_intr_type(hal, gpio_num, intr_type) gpio_ll_set_intr_type((hal)->dev, gpio_num, intr_type)
#endif
#ifndef gpio_hal_set_level
#define gpio_hal_set_level(hal, gpio_num, level) gpio_ll_set_level((hal)->dev, gpio_num, level)
#endif
#ifndef gpio_hal_set_output_enable_ctrl
#define gpio_hal_set_output_enable_ctrl(hal, gpio_num, ctrl_by_periph, oen_inv) gpio_ll_set_output_enable_ctrl((hal)->dev, gpio_num, ctrl_by_periph, oen_inv)
#endif
#ifndef gpio_hal_sleep_input_disable
#define gpio_hal_sleep_input_disable(hal, gpio_num) gpio_ll_sleep_input_disable((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_sleep_input_enable
#define gpio_hal_sleep_input_enable(hal, gpio_num) gpio_ll_sleep_input_enable((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_sleep_output_disable
#define gpio_hal_sleep_output_disable(hal, gpio_num) gpio_ll_sleep_output_disable((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_sleep_output_enable
#define gpio_hal_sleep_output_enable(hal, gpio_num) gpio_ll_sleep_output_enable((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_sleep_pulldown_dis
#define gpio_hal_sleep_pulldown_dis(hal, gpio_num) gpio_ll_sleep_pulldown_dis((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_sleep_pulldown_en
#define gpio_hal_sleep_pulldown_en(hal, gpio_num) gpio_ll_sleep_pulldown_en((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_sleep_pullup_dis
#define gpio_hal_sleep_pullup_dis(hal, gpio_num) gpio_ll_sleep_pullup_dis((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_sleep_pullup_en
#define gpio_hal_sleep_pullup_en(hal, gpio_num) gpio_ll_sleep_pullup_en((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_sleep_sel_dis
#define gpio_hal_sleep_sel_dis(hal, gpio_num) gpio_ll_sleep_sel_dis((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_sleep_sel_en
#define gpio_hal_sleep_sel_en(hal, gpio_num) gpio_ll_sleep_sel_en((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_wakeup_disable
#define gpio_hal_wakeup_disable(hal, gpio_num) gpio_ll_wakeup_disable((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_wakeup_disable_on_hp_periph_powerdown_sleep
#define gpio_hal_wakeup_disable_on_hp_periph_powerdown_sleep(hal, gpio_num) gpio_ll_wakeup_disable_on_hp_periph_powerdown_sleep((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_wakeup_enable
#define gpio_hal_wakeup_enable(hal, gpio_num) gpio_ll_wakeup_enable((hal)->dev, gpio_num)
#endif
#ifndef gpio_hal_wakeup_enable_on_hp_periph_powerdown_sleep
#define gpio_hal_wakeup_enable_on_hp_periph_powerdown_sleep(hal, gpio_num, intr_type) gpio_ll_wakeup_enable_on_hp_periph_powerdown_sleep((hal)->dev, gpio_num, intr_type)
#endif
#ifndef gpio_hal_wakeup_is_enabled_on_hp_periph_powerdown_sleep
#define gpio_hal_wakeup_is_enabled_on_hp_periph_powerdown_sleep(hal, gpio_num) gpio_ll_hp_periph_powerdown_sleep_wakeup_is_enabled((hal)->dev, gpio_num)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    gpio_dev_t * dev;
} gpio_hal_context_t;

void gpio_hal_hysteresis_soft_enable(gpio_hal_context_t *hal, uint32_t gpio_num, bool enable);
void gpio_hal_intr_disable(gpio_hal_context_t *hal, uint32_t gpio_num);
void gpio_hal_intr_enable_on_core(gpio_hal_context_t *hal, uint32_t gpio_num, uint32_t core_id);
void gpio_hal_iomux_in(gpio_hal_context_t *hal, uint32_t gpio_num, int func, uint32_t signal_idx);
void gpio_hal_iomux_out(gpio_hal_context_t *hal, uint32_t gpio_num, int func);
void gpio_hal_isolate_in_sleep(gpio_hal_context_t *hal, uint32_t gpio_num);
void gpio_hal_matrix_in(gpio_hal_context_t *hal, uint32_t gpio_num, uint32_t signal_idx, bool in_inv);
void gpio_hal_matrix_interconnect(gpio_hal_context_t *hal, uint32_t sig_src_pin, uint32_t sig_dst_pin, uint32_t signal_idx);
void gpio_hal_matrix_out(gpio_hal_context_t *hal, uint32_t gpio_num, uint32_t signal_idx, bool out_inv, bool oen_inv);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_GPIO_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_GPIO_HAL_H */
