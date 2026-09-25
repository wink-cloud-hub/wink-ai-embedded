// SPDX-License-Identifier: LGPL-3.0-only
#include "driver/ledc.h"
#include "hal/pal_pwm.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "esp_ledc";

typedef struct {
    bool configured;
    uint32_t freq_hz;
    ledc_timer_bit_t duty_resolution;
} esp_ledc_timer_state_t;

typedef struct {
    bool configured;
    int gpio_num;
    ledc_mode_t speed_mode;
    ledc_timer_t timer_sel;
    uint32_t pending_duty;
    uint32_t active_duty;
    uint32_t target_fade_duty;
    ledc_cbs_t cbs;
    void *cb_user_arg;
} esp_ledc_channel_state_t;

static esp_ledc_timer_state_t s_timers[LEDC_SPEED_MODE_MAX][LEDC_TIMER_MAX];
static esp_ledc_channel_state_t s_channels[LEDC_CHANNEL_MAX];
static bool s_fade_installed = false;

esp_err_t ledc_timer_config(const ledc_timer_config_t *timer_conf) {
    if (!timer_conf || timer_conf->speed_mode >= LEDC_SPEED_MODE_MAX ||
        timer_conf->timer_num >= LEDC_TIMER_MAX ||
        timer_conf->duty_resolution == 0 || timer_conf->duty_resolution > 20 ||
        timer_conf->freq_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }
#if !SOC_LEDC_SUPPORT_HS_MODE
    if (timer_conf->speed_mode == LEDC_HIGH_SPEED_MODE) {
        ESP_LOGE(TAG, "High speed mode not supported on current SoC");
        return ESP_ERR_INVALID_ARG;
    }
#endif
    esp_ledc_timer_state_t *t = &s_timers[timer_conf->speed_mode][timer_conf->timer_num];
    t->configured = true;
    t->freq_hz = timer_conf->freq_hz;
    t->duty_resolution = timer_conf->duty_resolution;
    return ESP_OK;
}

esp_err_t ledc_channel_config(const ledc_channel_config_t *ch_conf) {
    if (!ch_conf || ch_conf->channel >= SOC_LEDC_CHANNEL_NUM ||
        ch_conf->timer_sel >= LEDC_TIMER_MAX || ch_conf->speed_mode >= LEDC_SPEED_MODE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_ledc_timer_state_t *t = &s_timers[ch_conf->speed_mode][ch_conf->timer_sel];
    if (!t->configured) {
        ESP_LOGE(TAG, "Timer %d not configured for channel %d", (int)ch_conf->timer_sel, (int)ch_conf->channel);
        return ESP_ERR_INVALID_STATE;
    }

    pal_pwm_config_t cfg = {
        .struct_size = sizeof(pal_pwm_config_t),
        .pin = (wink_pin_t)ch_conf->gpio_num,
        .freq_hz = t->freq_hz,
        .resolution_bits = (uint8_t)t->duty_resolution,
        .clock_requirement = PAL_PWM_CLOCK_AUTO
    };

    wink_status_t st = pal_pwm_init_ex((uint8_t)ch_conf->channel, &cfg);
    if (st != WINK_OK) {
        return ESP_FAIL;
    }

    esp_ledc_channel_state_t *ch = &s_channels[ch_conf->channel];
    ch->configured = true;
    ch->gpio_num = ch_conf->gpio_num;
    ch->speed_mode = ch_conf->speed_mode;
    ch->timer_sel = ch_conf->timer_sel;
    ch->pending_duty = ch_conf->duty;

    return ledc_update_duty(ch_conf->speed_mode, ch_conf->channel);
}

esp_err_t ledc_set_duty(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t duty) {
    (void)speed_mode;
    if (channel >= SOC_LEDC_CHANNEL_NUM || !s_channels[channel].configured) {
        return ESP_ERR_INVALID_ARG;
    }
    s_channels[channel].pending_duty = duty;
    return ESP_OK;
}

esp_err_t ledc_update_duty(ledc_mode_t speed_mode, ledc_channel_t channel) {
    (void)speed_mode;
    if (channel >= SOC_LEDC_CHANNEL_NUM || !s_channels[channel].configured) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_ledc_channel_state_t *ch = &s_channels[channel];
    esp_ledc_timer_state_t *t = &s_timers[ch->speed_mode][ch->timer_sel];

    ch->active_duty = ch->pending_duty;
    uint32_t top = (1u << (uint32_t)t->duty_resolution) - 1u;
    if (top == 0) {
        top = 1;
    }

    // 纯定点万分比计算（0..10000），带四舍五入防溢出，严禁浮点
    uint32_t duty_clamped = (ch->active_duty > top) ? top : ch->active_duty;
    uint64_t prod = (uint64_t)duty_clamped * 10000ULL + (top / 2ULL);
    uint16_t basis_points = (uint16_t)(prod / top);
    if (basis_points > 10000u) {
        basis_points = 10000u;
    }

    wink_status_t st = pal_pwm_set_duty_bp((uint8_t)channel, basis_points);
    return (st == WINK_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t ledc_stop(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t idle_level) {
    (void)speed_mode;
    (void)idle_level;
    if (channel >= SOC_LEDC_CHANNEL_NUM || !s_channels[channel].configured) {
        return ESP_ERR_INVALID_ARG;
    }
    s_channels[channel].pending_duty = 0;
    s_channels[channel].active_duty = 0;
    wink_status_t st = pal_pwm_set_duty_bp((uint8_t)channel, 0);
    (void)st;
    return ESP_OK;
}

esp_err_t ledc_fade_func_install(int intr_alloc_flags) {
    (void)intr_alloc_flags;
    s_fade_installed = true;
    return ESP_OK;
}

void ledc_fade_func_uninstall(void) {
    s_fade_installed = false;
}

esp_err_t ledc_set_fade_with_time(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t target_duty, int max_fade_time_ms) {
    (void)speed_mode;
    (void)max_fade_time_ms;
    if (channel >= SOC_LEDC_CHANNEL_NUM || !s_channels[channel].configured) {
        return ESP_ERR_INVALID_ARG;
    }
    s_channels[channel].target_fade_duty = target_duty;
    return ESP_OK;
}

esp_err_t ledc_set_fade_with_step(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t target_duty, uint32_t scale, uint32_t cycle_num) {
    (void)speed_mode;
    (void)scale;
    (void)cycle_num;
    if (channel >= SOC_LEDC_CHANNEL_NUM || !s_channels[channel].configured) {
        return ESP_ERR_INVALID_ARG;
    }
    s_channels[channel].target_fade_duty = target_duty;
    return ESP_OK;
}

esp_err_t ledc_fade_start(ledc_mode_t speed_mode, ledc_channel_t channel, ledc_fade_mode_t fade_mode) {
    (void)fade_mode;
    if (channel >= SOC_LEDC_CHANNEL_NUM || !s_channels[channel].configured) {
        return ESP_ERR_INVALID_ARG;
    }
    // 仿真环境保真降级：瞬时更新到目标占空比并调用回调
    esp_ledc_channel_state_t *ch = &s_channels[channel];
    ch->pending_duty = ch->target_fade_duty;
    esp_err_t ret = ledc_update_duty(speed_mode, channel);
    if (ch->cbs.fade_cb) {
        ledc_cb_param_t param = {
            .event = 0,
            .speed_mode = (uint32_t)ch->speed_mode,
            .channel = (uint32_t)channel,
            .duty = ch->active_duty
        };
        ch->cbs.fade_cb(&param, ch->cb_user_arg);
    }
    return ret;
}

esp_err_t ledc_cb_register(ledc_mode_t speed_mode, ledc_channel_t channel, ledc_cbs_t *cbs, void *user_arg) {
    (void)speed_mode;
    if (channel >= SOC_LEDC_CHANNEL_NUM || !s_channels[channel].configured || !cbs) {
        return ESP_ERR_INVALID_ARG;
    }
    s_channels[channel].cbs = *cbs;
    s_channels[channel].cb_user_arg = user_arg;
    return ESP_OK;
}

uint32_t ledc_get_duty(ledc_mode_t speed_mode, ledc_channel_t channel) {
    (void)speed_mode;
    if (channel >= SOC_LEDC_CHANNEL_NUM || !s_channels[channel].configured) {
        return 0;
    }
    return s_channels[channel].active_duty;
}

esp_err_t ledc_set_freq(ledc_mode_t speed_mode, ledc_timer_t timer_num, uint32_t freq_hz) {
    if (speed_mode >= LEDC_SPEED_MODE_MAX || timer_num >= LEDC_TIMER_MAX || freq_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_ledc_timer_state_t *t = &s_timers[speed_mode][timer_num];
    if (!t->configured) {
        return ESP_ERR_INVALID_STATE;
    }
    t->freq_hz = freq_hz;
    for (int ch_idx = 0; ch_idx < SOC_LEDC_CHANNEL_NUM; ch_idx++) {
        esp_ledc_channel_state_t *ch = &s_channels[ch_idx];
        if (ch->configured && ch->speed_mode == speed_mode && ch->timer_sel == timer_num) {
            pal_pwm_config_t cfg = {
                .struct_size = sizeof(pal_pwm_config_t),
                .pin = (wink_pin_t)ch->gpio_num,
                .freq_hz = t->freq_hz,
                .resolution_bits = (uint8_t)t->duty_resolution,
                .clock_requirement = PAL_PWM_CLOCK_AUTO
            };
            wink_status_t st = pal_pwm_init_ex((uint8_t)ch_idx, &cfg);
            (void)st;
            ledc_update_duty(speed_mode, (ledc_channel_t)ch_idx);
        }
    }
    return ESP_OK;
}

uint32_t ledc_get_freq(ledc_mode_t speed_mode, ledc_timer_t timer_num) {
    if (speed_mode >= LEDC_SPEED_MODE_MAX || timer_num >= LEDC_TIMER_MAX) {
        return 0;
    }
    return s_timers[speed_mode][timer_num].configured ? s_timers[speed_mode][timer_num].freq_hz : 0;
}

void esp_ledc_reset(void) {
    for (int i = 0; i < SOC_LEDC_CHANNEL_NUM; i++) {
        if (s_channels[i].configured) {
            wink_status_t st = pal_pwm_deinit((uint8_t)i);
            (void)st;
        }
    }
    memset(s_timers, 0, sizeof(s_timers));
    memset(s_channels, 0, sizeof(s_channels));
    s_fade_installed = false;
}
