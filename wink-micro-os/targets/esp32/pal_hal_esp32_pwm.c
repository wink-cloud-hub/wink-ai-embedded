/**
 * @file pal_hal_esp32_pwm.c
 * @brief ESP32 target 的 PWM (LEDC) 实现：pal_pwm_init/set_duty/deinit +
 *        pal_pwm_router 集成 + weak pal_pwm_pin_map 默认值。
 *
 * 由 targets/esp32/pal_hal_esp32.c 拆出（PLAN-20260701-PAL-TARGET-P1-MAINT Task 2 Step 4）。
 * 契约不变：仅物理位置调整；见 pal/include/pal_hal.h 与 pal/include/hal/pal_pwm_router.h。
 *
 * ✅ R-4：全文件仅 1 处最外层 `#if defined(ESP_PLATFORM)`。
 */
#include "pal_hal.h"
#include "pal_pwm_router.h"
#include "pal_resource.h"

#if defined(ESP_PLATFORM)
#include "driver/ledc.h"
#include "esp_err.h"

/* 板级路由弱默认：无 board_config.c 覆盖时使用，避免链接缺符号。
 * 强定义由 samples/<app>/board_config.c 提供。*/
__attribute__((weak)) const wink_pin_t pal_pwm_pin_map[PAL_PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};

/* owner 字符串常量：claim/release 必须逐字一致，否则 release 静默 no-op。*/
static const char *const PWM_OWNER = "pal_hal_esp32";

wink_status_t pal_pwm_init(uint8_t channel, uint32_t freq_hz) {
    uint8_t timer_num = 0;
    wink_status_t rs = pal_pwm_router_acquire(channel, freq_hz, &timer_num);
    if (wink_status_is_error(rs)) { return rs; }

    rs = pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL, channel, PWM_OWNER);
    if (wink_status_is_error(rs)) {
        pal_pwm_router_release(channel);
        return rs;
    }

    /* router 分配 timer，不再写死 LEDC_TIMER_0：同频复用、异频隔离。*/
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = (ledc_timer_t)timer_num,
        .freq_hz = freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        pal_pwm_router_release(channel);
        /* gcc16/xtensa-gcc 不因 (void) 抑制 warn_unused_result：先赋值再丢弃，best-effort 释放。*/
        wink_status_t _rel = pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, channel, PWM_OWNER);
        (void)_rel;
        return WINK_ERR_HARDWARE;
    }

    /* 物理路由来自 board_config.c 的强定义（无覆盖时回落至本 TU 弱默认 pal_pwm_pin_map）。*/
    ledc_channel_config_t ch_cfg = {
        .gpio_num = pal_pwm_pin_map[channel],
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)timer_num,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ch_cfg);
    if (err != ESP_OK) {
        pal_pwm_router_release(channel);
        /* gcc16/xtensa-gcc 不因 (void) 抑制 warn_unused_result：先赋值再丢弃，best-effort 释放。*/
        wink_status_t _rel = pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, channel, PWM_OWNER);
        (void)_rel;
        return WINK_ERR_HARDWARE;
    }
    return WINK_OK;
}

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_percent) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    if (duty_percent < 0.0f) { duty_percent = 0.0f; }
    if (duty_percent > 100.0f) { duty_percent = 100.0f; }

    uint32_t duty = (uint32_t)(duty_percent / 100.0f * 8191.0f); /* 13-bit = 8192 */
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
    return WINK_OK;
}

void pal_pwm_deinit(uint8_t channel) {
    if (!pal_pwm_router_channel_ready(channel)) { return; }   /* no-op if uninitialized */
    (void)ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, 0);
    (void)ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    /* gcc16/xtensa-gcc 不因 (void) 抑制 warn_unused_result：先赋值再丢弃，best-effort 释放/deinit 不失败。*/
    wink_status_t _rel = pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, channel, PWM_OWNER);
    (void)_rel;
    pal_pwm_router_release(channel);
}

#else /* !ESP_PLATFORM: non-IDF stub for static analysis. */

wink_status_t pal_pwm_init(uint8_t channel, uint32_t freq_hz)
{ (void)channel; (void)freq_hz; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_percent)
{ (void)channel; (void)duty_percent; return WINK_ERR_UNSUPPORTED; }

void pal_pwm_deinit(uint8_t channel) { (void)channel; }

#endif /* ESP_PLATFORM */
