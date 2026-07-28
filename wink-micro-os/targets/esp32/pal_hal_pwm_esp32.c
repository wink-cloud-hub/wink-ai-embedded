/**
 * @file pal_hal_pwm_esp32.c
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

#if defined(ESP_PLATFORM)
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h"

#ifndef SIG_GPIO_OUT_IDX
#define SIG_GPIO_OUT_IDX 256
#endif

/* 板级路由弱默认：无 board_config.c 覆盖时使用，避免链接缺符号。
 * 强定义由 samples/<app>/board_config.c 提供。*/
__attribute__((weak)) const wink_pin_t pal_pwm_pin_map[PAL_PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};

/* Track A（M1）：DAL 是资源占用 SSOT，PAL 层不再自 claim PWM 通道 —— 语义 owner 由 DAL 层
 * （dal_rc_servo 等）持有。这样两个 DAL 实例配同 channel 才能在 DAL init 阶段真正触发 BUSY。 */

static uint8_t s_ch_bits[PAL_PWM_CHANNELS];

static bool pwm_map_ledc_bits(uint8_t bits, ledc_timer_bit_t *out)
{
    if (out == NULL) { return false; }
    switch (bits) {
        case 8:  *out = LEDC_TIMER_8_BIT;  return true;
        case 9:  *out = LEDC_TIMER_9_BIT;  return true;
        case 10: *out = LEDC_TIMER_10_BIT; return true;
        case 11: *out = LEDC_TIMER_11_BIT; return true;
        case 12: *out = LEDC_TIMER_12_BIT; return true;
        case 13: *out = LEDC_TIMER_13_BIT; return true;
        case 14: *out = LEDC_TIMER_14_BIT; return true;
        case 15: *out = LEDC_TIMER_15_BIT; return true;
        default: return false;
    }
}

wink_status_t pal_pwm_init(uint8_t channel, uint32_t freq_hz)
{
    pal_pwm_config_t cfg = { .freq_hz = freq_hz };
    return pal_pwm_init_ex(channel, &cfg);
}

wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg)
{
    if (cfg == NULL || cfg->freq_hz == 0u) {
        return WINK_ERR_INVALID_ARG;
    }

    uint8_t bits = cfg->resolution_bits ? cfg->resolution_bits : 13u;
    ledc_timer_bit_t duty_res;
    if (!pwm_map_ledc_bits(bits, &duty_res)) {
        return WINK_ERR_INVALID_ARG;
    }

    uint8_t eff_clk = PAL_PWM_EFF_CLK_PLATFORM_AUTO;
    ledc_clk_cfg_t clk_cfg = LEDC_AUTO_CLK;
    if (cfg->clock_requirement == PAL_PWM_CLOCK_STABLE_REQUIRED) {
        eff_clk = PAL_PWM_EFF_CLK_REF_TICK;
        clk_cfg = LEDC_USE_REF_TICK;
    }

    pal_pwm_timer_profile_t prof = {
        .freq_hz = cfg->freq_hz,
        .resolution_bits = bits,
        .clock_source = eff_clk,
    };

    uint8_t timer_num = 0;
    wink_status_t rs = pal_pwm_router_acquire(channel, &prof, &timer_num);
    if (wink_status_is_error(rs)) { return rs; }

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = duty_res,
        .timer_num = (ledc_timer_t)timer_num,
        .freq_hz = cfg->freq_hz,
        .clk_cfg = clk_cfg,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        pal_pwm_router_release(channel);
        return WINK_ERR_HARDWARE;
    }

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
        return WINK_ERR_HARDWARE;
    }

    s_ch_bits[channel] = bits;
    return WINK_OK;
}

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_percent) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    if (duty_percent < 0.0f) { duty_percent = 0.0f; }
    if (duty_percent > 100.0f) { duty_percent = 100.0f; }

    uint32_t duty = pal_pwm_percent_to_raw(duty_percent, s_ch_bits[channel]);
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
    (void)ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, 0);
    /* Track A（M1）：PAL 不再持 PWM claim；release 归 DAL 层未来 deinit。 */
    pal_pwm_router_release(channel);

    /* Fully release the pin back to GPIO + clear the LEDC driver's software
     * reservation (esp_gpio_reserve bit). IDF v6 LEDC only revokes its bit via
     * ledc_channel_config({.deconfigure=true}) or gpio_reset_pin(); ledc_stop()
     * alone leaves the bit set, which makes any subsequent driver bind on the
     * same pin emit "GPIO N is not usable, maybe conflict with others".
     * gpio_reset_pin() also:
     *   - disconnects the LEDC output signal from the pin's func_out mux,
     *   - disables input/output drivers,
     *   - disconnects any RTC/IOMUX alternate function,
     *   - revokes the reservation bit.
     * After this, the pin is in its reset (GPIO, input-only, no-pull) state
     * and the next pal_gpio_init() / driver bind starts from a clean slate. */
    wink_pin_t pin = pal_pwm_pin_map[channel];
    if (pin >= 0 && pin < GPIO_NUM_MAX) {
        (void)gpio_reset_pin((gpio_num_t)pin);
    }
}

/* P1-P4 (2026-07-04)：pin_map 数组不再暴露到公共头，改经 getter。
 * board_config.c 仍以强定义覆盖弱默认 pal_pwm_pin_map（linker 层，无 forward decl 必要）。*/
wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin) {
    if (out_pin == NULL) { return WINK_ERR_INVALID_ARG; }
    if (channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    *out_pin = pal_pwm_pin_map[channel];
    return WINK_OK;
}

#else /* !ESP_PLATFORM: non-IDF stub for static analysis. */

wink_status_t pal_pwm_init(uint8_t channel, uint32_t freq_hz)
{ (void)channel; (void)freq_hz; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg)
{ (void)channel; (void)cfg; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_percent)
{ (void)channel; (void)duty_percent; return WINK_ERR_UNSUPPORTED; }

void pal_pwm_deinit(uint8_t channel) { (void)channel; }

wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin)
{ (void)channel; (void)out_pin; return WINK_ERR_UNSUPPORTED; }

#endif /* ESP_PLATFORM */
