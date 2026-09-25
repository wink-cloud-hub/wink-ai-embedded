// SPDX-License-Identifier: LGPL-3.0-only
#ifndef DRIVER_LEDC_H
#define DRIVER_LEDC_H

#include "esp_err.h"
#include "soc/soc_caps.h"
#include "hal/ledc_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ledc_mode_t      speed_mode;
    ledc_timer_bit_t duty_resolution;
    ledc_timer_t     timer_num;
    uint32_t         freq_hz;
    uint32_t         clk_cfg;
} ledc_timer_config_t;

typedef struct {
    int           gpio_num;
    ledc_mode_t   speed_mode;
    ledc_channel_t channel;
    uint32_t      intr_type;
    ledc_timer_t  timer_sel;
    uint32_t      duty;
    int           hpoint;
    uint32_t      flags;
    uint32_t      sleep_mode;
} ledc_channel_config_t;

typedef struct {
    uint32_t event;
    uint32_t speed_mode;
    uint32_t channel;
    uint32_t duty;
} ledc_cb_param_t;

typedef bool (*ledc_cb_t)(const ledc_cb_param_t *param, void *user_arg);

typedef struct {
    ledc_cb_t fade_cb;
} ledc_cbs_t;

esp_err_t ledc_timer_config(const ledc_timer_config_t *timer_conf);
esp_err_t ledc_channel_config(const ledc_channel_config_t *ch_conf);
esp_err_t ledc_set_duty(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t duty);
esp_err_t ledc_update_duty(ledc_mode_t speed_mode, ledc_channel_t channel);
esp_err_t ledc_stop(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t idle_level);
uint32_t  ledc_get_duty(ledc_mode_t speed_mode, ledc_channel_t channel);
esp_err_t ledc_set_freq(ledc_mode_t speed_mode, ledc_timer_t timer_num, uint32_t freq_hz);
uint32_t  ledc_get_freq(ledc_mode_t speed_mode, ledc_timer_t timer_num);

/* Fade 渐变函数族 */
esp_err_t ledc_fade_func_install(int intr_alloc_flags);
void      ledc_fade_func_uninstall(void);
esp_err_t ledc_set_fade_with_time(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t target_duty, int max_fade_time_ms);
esp_err_t ledc_set_fade_with_step(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t target_duty, uint32_t scale, uint32_t cycle_num);
esp_err_t ledc_fade_start(ledc_mode_t speed_mode, ledc_channel_t channel, ledc_fade_mode_t fade_mode);
esp_err_t ledc_cb_register(ledc_mode_t speed_mode, ledc_channel_t channel, ledc_cbs_t *cbs, void *user_arg);

/* 仿真环境状态清理钩子 */
void      esp_ledc_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_LEDC_H */
