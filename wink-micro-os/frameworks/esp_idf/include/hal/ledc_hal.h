/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_LEDC_HAL_H
#define WINK_H_GUARD_HAL_LEDC_HAL_H
#ifndef __WINK_HARVESTED_HAL_LEDC_HAL_H__
#define __WINK_HARVESTED_HAL_LEDC_HAL_H__
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
#ifndef ledc_hal_bind_channel_timer
#define ledc_hal_bind_channel_timer(hal, speed_mode, channel_num, timer_sel) ledc_ll_bind_channel_timer((hal)->dev, speed_mode, channel_num, timer_sel)
#endif
#ifndef ledc_hal_get_channel_timer
#define ledc_hal_get_channel_timer(hal, speed_mode, channel_num, timer_sel) ledc_ll_get_channel_timer((hal)->dev, speed_mode, channel_num, timer_sel)
#endif
#ifndef ledc_hal_get_clock_divider
#define ledc_hal_get_clock_divider(hal, speed_mode, timer_sel, clock_divider) ledc_ll_get_clock_divider((hal)->dev, speed_mode, timer_sel, clock_divider)
#endif
#ifndef ledc_hal_get_clock_source
#define ledc_hal_get_clock_source(hal, speed_mode, timer_sel, clk_src) ledc_ll_get_clock_source((hal)->dev, speed_mode, timer_sel, clk_src)
#endif
#ifndef ledc_hal_get_duty_resolution
#define ledc_hal_get_duty_resolution(hal, speed_mode, timer_sel, duty_resolution) ledc_ll_get_duty_resolution((hal)->dev, speed_mode, timer_sel, duty_resolution)
#endif
#ifndef ledc_hal_get_hpoint
#define ledc_hal_get_hpoint(hal, speed_mode, channel_num, hpoint_val) ledc_ll_get_hpoint((hal)->dev, speed_mode, channel_num, hpoint_val)
#endif
#ifndef ledc_hal_get_max_duty
#define ledc_hal_get_max_duty(hal, speed_mode, timer_sel, max_duty) ledc_ll_get_max_duty((hal)->dev, speed_mode, timer_sel, max_duty)
#endif
#ifndef ledc_hal_get_slow_clk_sel
#define ledc_hal_get_slow_clk_sel(hal, slow_clk_sel) ledc_ll_get_slow_clk_sel((hal)->dev, slow_clk_sel)
#endif
#ifndef ledc_hal_ls_timer_update
#define ledc_hal_ls_timer_update(hal, speed_mode, timer_sel) ledc_ll_ls_timer_update((hal)->dev, speed_mode, timer_sel)
#endif
#ifndef ledc_hal_set_clock_divider
#define ledc_hal_set_clock_divider(hal, speed_mode, timer_sel, clock_divider) ledc_ll_set_clock_divider((hal)->dev, speed_mode, timer_sel, clock_divider)
#endif
#ifndef ledc_hal_set_clock_source
#define ledc_hal_set_clock_source(hal, speed_mode, timer_sel, clk_src) ledc_ll_set_clock_source((hal)->dev, speed_mode, timer_sel, clk_src)
#endif
#ifndef ledc_hal_set_duty_resolution
#define ledc_hal_set_duty_resolution(hal, speed_mode, timer_sel, duty_resolution) ledc_ll_set_duty_resolution((hal)->dev, speed_mode, timer_sel, duty_resolution)
#endif
#ifndef ledc_hal_set_idle_level
#define ledc_hal_set_idle_level(hal, speed_mode, channel_num, idle_level) ledc_ll_set_idle_level((hal)->dev, speed_mode, channel_num, idle_level)
#endif
#ifndef ledc_hal_set_sig_out_en
#define ledc_hal_set_sig_out_en(hal, speed_mode, channel_num, sig_out_en) ledc_ll_set_sig_out_en((hal)->dev, speed_mode, channel_num, sig_out_en)
#endif
#ifndef ledc_hal_set_slow_clk_sel
#define ledc_hal_set_slow_clk_sel(hal, slow_clk_sel) ledc_ll_set_slow_clk_sel((hal)->dev, slow_clk_sel)
#endif
#ifndef ledc_hal_timer_pause
#define ledc_hal_timer_pause(hal, speed_mode, timer_sel) ledc_ll_timer_pause((hal)->dev, speed_mode, timer_sel)
#endif
#ifndef ledc_hal_timer_resume
#define ledc_hal_timer_resume(hal, speed_mode, timer_sel) ledc_ll_timer_resume((hal)->dev, speed_mode, timer_sel)
#endif
#ifndef ledc_hal_timer_rst
#define ledc_hal_timer_rst(hal, speed_mode, timer_sel) ledc_ll_timer_rst((hal)->dev, speed_mode, timer_sel)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct ledc_dev_t * ledc_soc_handle_t;
typedef struct {
    ledc_soc_handle_t dev;
} ledc_hal_context_t;

void ledc_hal_channel_configure_maximum_timer_ovf_cnt(ledc_hal_context_t *hal, ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t max_ovf_cnt);
void ledc_hal_clear_left_off_fade_param(ledc_hal_context_t *hal, ledc_mode_t speed_mode, ledc_channel_t channel_num, uint32_t start_range);
void ledc_hal_get_clk_cfg(ledc_hal_context_t *hal, ledc_mode_t speed_mode, ledc_timer_t timer_sel, ledc_clk_cfg_t *clk_cfg);
void ledc_hal_get_duty(ledc_hal_context_t *hal, ledc_mode_t speed_mode, ledc_channel_t channel_num, uint32_t *duty_val);
void ledc_hal_get_fade_param(ledc_hal_context_t *hal, ledc_mode_t speed_mode, ledc_channel_t channel_num, uint32_t range, uint32_t *dir, uint32_t *cycle, uint32_t *scale, uint32_t *step);
void ledc_hal_get_range_number(ledc_hal_context_t *hal, ledc_mode_t speed_mode, ledc_channel_t channel_num, uint32_t *range_num);
void ledc_hal_init(ledc_hal_context_t *hal, int group_id);
void ledc_hal_ls_channel_update(ledc_hal_context_t *hal, ledc_mode_t speed_mode, ledc_channel_t channel_num);
void ledc_hal_set_duty_int_part(ledc_hal_context_t *hal, ledc_mode_t speed_mode, ledc_channel_t channel_num, uint32_t duty_val);
void ledc_hal_set_duty_start(ledc_hal_context_t *hal, ledc_mode_t speed_mode, ledc_channel_t channel_num);
void ledc_hal_set_fade_param(const ledc_hal_context_t *hal, ledc_mode_t speed_mode, ledc_channel_t channel_num, uint32_t range, uint32_t dir, uint32_t cycle, uint32_t scale, uint32_t step);
void ledc_hal_set_hpoint(ledc_hal_context_t *hal, ledc_mode_t speed_mode, ledc_channel_t channel_num, uint32_t hpoint_val);
void ledc_hal_set_range_number(ledc_hal_context_t *hal, ledc_mode_t speed_mode, ledc_channel_t channel_num, uint32_t range_num);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_LEDC_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_LEDC_HAL_H */
