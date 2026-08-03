#define LOG_TAG "dal_rc_servo"
#include "actuator/dal_rc_servo.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include "pal_log.h"

#include <string.h>   /* memcpy（ADR-0008 apply_override 反序列化） */

#define SERVO_PWM_FREQ_HZ             50u     /* 50Hz -> 周期 20ms */
#define SERVO_PERIOD_US               20000u  /* 20ms = 20000µs，单一真相源 */
#define SERVO_MIN_ANGLE_DDEG          0u
#define SERVO_DEFAULT_MAX_ANGLE_DDEG  1800u   /* 180.0° */
#define SERVO_DEFAULT_MIN_PULSE_US    500u
#define SERVO_DEFAULT_MAX_PULSE_US    2500u

static uint16_t servo_effective_max_angle_ddeg(const dal_rc_servo_config_t *cfg)
{
    if (cfg == NULL || cfg->max_angle_ddeg == 0) {
        return SERVO_DEFAULT_MAX_ANGLE_DDEG;
    }
    return cfg->max_angle_ddeg;
}

/** Map DAL servo config → PAL PWM config (no pal_* types in public headers). */
static wink_status_t servo_map_pwm_config(const dal_rc_servo_config_t *servo_cfg,
                                          pal_pwm_config_t *out_pwm_cfg)
{
    if (servo_cfg == NULL || out_pwm_cfg == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (servo_cfg->clock_requirement > DAL_RC_SERVO_CLOCK_STABLE_REQUIRED) {
        return WINK_ERR_INVALID_ARG;
    }

    out_pwm_cfg->freq_hz = SERVO_PWM_FREQ_HZ;
    out_pwm_cfg->resolution_bits = servo_cfg->resolution_bits; /* 0 = AUTO */
    if (servo_cfg->clock_requirement == DAL_RC_SERVO_CLOCK_STABLE_REQUIRED) {
        out_pwm_cfg->clock_requirement = PAL_PWM_CLOCK_STABLE_REQUIRED;
    } else {
        out_pwm_cfg->clock_requirement = PAL_PWM_CLOCK_AUTO;
    }
    return WINK_OK;
}

wink_status_t dal_rc_servo_init(dal_rc_servo_t *dev, const dal_rc_servo_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }
    if (cfg->pwm_channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }

    /* Normalize pulse range: 0/invalid → defaults. */
    uint16_t min_pulse = (cfg->min_pulse_us > 0) ? cfg->min_pulse_us : SERVO_DEFAULT_MIN_PULSE_US;
    uint16_t max_pulse = (cfg->max_pulse_us > min_pulse) ? cfg->max_pulse_us : SERVO_DEFAULT_MAX_PULSE_US;

    pal_pwm_config_t pwm_cfg;
    wink_status_t map_st = servo_map_pwm_config(cfg, &pwm_cfg);
    if (wink_status_is_error(map_st)) { return map_st; }

    /* PWM channel conflict detection — two servos on same channel with different owners → BUSY. */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL,
                                          (uint32_t)cfg->pwm_channel, cfg->owner);
    if (wink_status_is_error(rs)) {
        LOG_W("init: claim PWM ch%u for '%s' failed: %d",
              (unsigned)cfg->pwm_channel, cfg->owner, (int)rs);
        return rs;
    }

    /* One-shot PWM init_ex (occupies channel); failure propagates precise PAL error. */
    wink_status_t status = pal_pwm_init_ex(cfg->pwm_channel, &pwm_cfg);
    if (wink_status_is_error(status)) {
        LOG_W("init: pal_pwm_init_ex ch%u for '%s' failed: %d",
              (unsigned)cfg->pwm_channel, cfg->owner, (int)status);
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_PWM_CHANNEL,
                                                 (uint32_t)cfg->pwm_channel, cfg->owner));
        return status;
    }

    /* Deep-copy config and normalize pulse values. */
    memcpy(&dev->config, cfg, sizeof(dal_rc_servo_config_t));
    dev->config.min_pulse_us = min_pulse;
    dev->config.max_pulse_us = max_pulse;
    dev->current_angle_ddeg = 0;
    dev->initialized = true;

    /* DAL-L-006: explicitly write zero-energy output (duty=0 → limp).
     * Do not rely on PAL init default. */
    WINK_IGNORE_UNUSED(pal_pwm_set_duty(dev->config.pwm_channel, 0.0f));

    LOG_I("init: '%s' ready (ch%u, pulse %u-%u us, max %u ddeg)",
          cfg->owner, (unsigned)cfg->pwm_channel,
          (unsigned)min_pulse, (unsigned)max_pulse,
          (unsigned)servo_effective_max_angle_ddeg(&dev->config));
    return WINK_OK;
}

wink_status_t dal_rc_servo_set_angle(dal_rc_servo_t *dev, uint16_t angle_ddeg) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    /* Clamp to [0, effective_max] (DAL-U-011 saturate, no wrap). */
    uint16_t max_ddeg = servo_effective_max_angle_ddeg(&dev->config);
    if (angle_ddeg > max_ddeg) { angle_ddeg = max_ddeg; }

    /* Pulse-width mapping in integer µs.
     * pulse_us = min + (angle_ddeg * (max-min)) / max_ddeg
     * Intermediate product promoted to uint32_t to avoid 16-bit overflow (DAL-U-029). */
    uint32_t pulse_range_us = (uint32_t)dev->config.max_pulse_us - (uint32_t)dev->config.min_pulse_us;
    uint32_t pulse_us = (uint32_t)dev->config.min_pulse_us
                      + ((uint32_t)angle_ddeg * pulse_range_us) / (uint32_t)max_ddeg;

    /* Convert to duty percentage for PAL: (pulse_us / period_us) * 100. */
    float duty_percent = ((float)pulse_us * 100.0f) / (float)SERVO_PERIOD_US;

    /* Hardware first, cache last (F-1): only update current_angle after PAL success. */
    wink_status_t status = pal_pwm_set_duty(dev->config.pwm_channel, duty_percent);
    if (wink_status_is_error(status)) { return status; }

    dev->current_angle_ddeg = angle_ddeg;
    return WINK_OK;
}

wink_status_t dal_rc_servo_safe_off(dal_rc_servo_t *dev) {
    /* DAL-L-022: idempotent on uninitialized handles — invoked from
     * safe_off_all() on watchdog/panic/rollback paths where "nothing to
     * shut off" is success, not an error. */
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }
    /* duty=0 → servo limp = safe; no sleep. Servo-specific only (see header red-line note). */
    return pal_pwm_set_duty(dev->config.pwm_channel, 0.0f);
}

wink_status_t dal_rc_servo_apply_override(void *dev, const uint8_t *params, uint16_t len) {
    dal_rc_servo_t *s = (dal_rc_servo_t *)dev;
    if (s == NULL || params == NULL) { return WINK_ERR_INVALID_ARG; }

    /* DAL-S-015: config is immutable after init; override only valid before init. */
    if (s->initialized) { return WINK_ERR_INVALID_ARG; }

    /* Blob layout (7 bytes, **not** containing owner — owner comes via cfg):
     *   byte 0    : pwm_channel (u8)
     *   byte 1..2 : min_pulse_us (u16 little-endian)
     *   byte 3..4 : max_pulse_us (u16 little-endian)
     *   byte 5..6 : max_angle_ddeg (u16 little-endian)
     */
    if (len < 7u) { return WINK_ERR_INVALID_ARG; }

    uint8_t  pwm_channel;
    uint16_t min_pulse_us;
    uint16_t max_pulse_us;
    uint16_t max_angle_ddeg;
    memcpy(&pwm_channel,     params + 0, 1);
    memcpy(&min_pulse_us,    params + 1, 2);
    memcpy(&max_pulse_us,    params + 3, 2);
    memcpy(&max_angle_ddeg,  params + 5, 2);

    /* Light validation — illegal values do not write any field (defense in depth with init). */
    if (pwm_channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    if (min_pulse_us == 0 || max_pulse_us <= min_pulse_us) { return WINK_ERR_INVALID_ARG; }

    s->config.pwm_channel    = pwm_channel;
    s->config.min_pulse_us   = min_pulse_us;
    s->config.max_pulse_us   = max_pulse_us;
    s->config.max_angle_ddeg = max_angle_ddeg;
    return WINK_OK;
}

/* Best-effort PWM channel claim release: release and log on failure, but
 * never aborts the remaining teardown (DAL-L-014/015). */
static void release_pwm_claim_logged(uint8_t channel, const char *owner)
{
    if (channel >= PAL_PWM_CHANNELS) { return; }
    wink_status_t rs = pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, (uint32_t)channel, owner);
    if (wink_status_is_error(rs)) {
        LOG_W("deinit: release PWM ch%u for '%s' failed: %d",
              (unsigned)channel, owner ? owner : "(null)", (int)rs);
    }
}

wink_status_t dal_rc_servo_deinit(dal_rc_servo_t *dev) {
    /* ADR-0024 §4 deinit:
     * 1. safe_off (duty=0 limp)  2. pal_pwm_deinit  3. GPIO reset routed pin
     * 4. N/A (no ISR)  5. N/A (no DMA)  6. N/A (not shared bus)
     * 7. memset  8. NULL+uninit idempotent  9. synchronous <1ms  10. unified signature */
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }  /* idempotent no-op */

    /* Read fields before any mutation/memset. */
    uint8_t channel = dev->config.pwm_channel;
    const char *owner = dev->config.owner;

    /* 1. Best-effort safe-off (duty=0 limp). */
    WINK_IGNORE_UNUSED(dal_rc_servo_safe_off(dev));

    /* 2. Stop PWM peripheral (disconnects LEDC from GPIO matrix). */
    pal_pwm_deinit(channel);

    /* 3. Query GPIO pin associated with this channel, then reset it to release
     *    the esp_gpio_reserve bitmap and revert to Hi-Z (ADR-0024 §4 #2). */
    wink_pin_t pin = -1;
    wink_status_t pin_st = pal_pwm_channel_pin(channel, &pin);
    if (pin_st == WINK_OK && pin >= 0) {
        pal_gpio_reset_pin(pin);
    }

    /* 4. Release PWM channel SW resource claim (failure logged, not fatal). */
    release_pwm_claim_logged(channel, owner);

    /* 5. Clear the instance data completely. */
    memset(dev, 0, sizeof(dal_rc_servo_t));

    return WINK_OK;
}
