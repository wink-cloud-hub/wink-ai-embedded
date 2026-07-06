#include "dal_servo.h"
#include "pal_hal.h"
#include "pal_resource.h"

#include <string.h>   /* memcpy（ADR-0008 apply_override 反序列化） */

#define SERVO_PWM_FREQ_HZ    50                             /* 50Hz -> 周期 20ms */
#define SERVO_PERIOD_MS      (1000.0f / SERVO_PWM_FREQ_HZ)  /* 派生：单一真相源，禁再写 20.0f */
#define SERVO_MIN_ANGLE_DEG  0.0f
#define SERVO_MAX_ANGLE_DEG  180.0f
#define SERVO_DUTY_FULL_PCT  100.0f

wink_status_t dal_servo_init(dal_servo_t *dev, const dal_servo_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->pwm_channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    if (cfg->min_pulse_ms <= 0.0f || cfg->max_pulse_ms <= cfg->min_pulse_ms) {
        return WINK_ERR_INVALID_ARG;
    }

    /* Track A（M1）：PWM 通道冲突治理——两舵机若配同 channel 不同 owner，此处 BUSY。 */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL,
                                          (uint32_t)cfg->pwm_channel, cfg->owner);
    if (wink_status_is_error(rs)) { return rs; }

    /* 一次性 PWM init（占用通道）；失败透传精确 PAL 错误（含 BUSY/EXHAUSTED）。 */
    wink_status_t status = pal_pwm_init(cfg->pwm_channel, SERVO_PWM_FREQ_HZ);
    if (wink_status_is_error(status)) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_PWM_CHANNEL,
                                                 (uint32_t)cfg->pwm_channel, cfg->owner));
        return status;
    }

    /* 深拷贝 config（含 owner 指针拷贝；owner 要求是静态存储，生命周期足够长）*/
    memcpy(&dev->config, cfg, sizeof(dal_servo_config_t));
    dev->current_angle = 0.0f;
    dev->initialized   = true;
    return WINK_OK;
}

wink_status_t dal_servo_set_angle(dal_servo_t *dev, float angle) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    if (angle < SERVO_MIN_ANGLE_DEG) { angle = SERVO_MIN_ANGLE_DEG; }
    if (angle > SERVO_MAX_ANGLE_DEG) { angle = SERVO_MAX_ANGLE_DEG; }
    dev->current_angle = angle;

    float pulse_width_ms = dev->config.min_pulse_ms +
        (angle / SERVO_MAX_ANGLE_DEG) * (dev->config.max_pulse_ms - dev->config.min_pulse_ms);
    float duty_percent = (pulse_width_ms / SERVO_PERIOD_MS) * SERVO_DUTY_FULL_PCT;

    /* Phase 2：PWM init 已在 dal_servo_init 一次性完成，set_angle 仅设占空比。 */
    wink_status_t status = pal_pwm_set_duty(dev->config.pwm_channel, duty_percent);
    if (wink_status_is_error(status)) { return status; }
    return WINK_OK;
}

wink_status_t dal_servo_safe_off(dal_servo_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    /* duty=0 → 舵机失保持力（limp）= 安全；不 sleep。仅适用舵机（见头文件语义边界注）。 */
    return pal_pwm_set_duty(dev->config.pwm_channel, 0.0f);
}

wink_status_t dal_servo_apply_override(void *dev, const uint8_t *params, uint16_t len) {
    dal_servo_t *s = (dal_servo_t *)dev;
    if (s == NULL || params == NULL) { return WINK_ERR_INVALID_ARG; }
    /* Blob 布局（共 9B，**不含** owner 指针 —— owner 由 cfg 正常传入，不参与 Flash 覆写）：
     *   byte 0    : pwm_channel (u8)
     *   byte 1..4 : min_pulse_ms (f32 little-endian)
     *   byte 5..8 : max_pulse_ms (f32 little-endian)
     * 字段偏移对应 config 内 pwm_channel 起的硬件参数子集，便于未来 config 字段扩展。 */
    if (len < 9u) { return WINK_ERR_INVALID_ARG; }

    uint8_t pwm_channel;
    float   min_pulse_ms;
    float   max_pulse_ms;
    memcpy(&pwm_channel,  params + 0, 1);
    memcpy(&min_pulse_ms, params + 1, 4);
    memcpy(&max_pulse_ms, params + 5, 4);

    /* 轻校验：非法不写（与 dal_servo_init 权威校验纵深一致） */
    if (pwm_channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    if (min_pulse_ms <= 0.0f || max_pulse_ms <= min_pulse_ms) { return WINK_ERR_INVALID_ARG; }

    s->config.pwm_channel  = pwm_channel;
    s->config.min_pulse_ms = min_pulse_ms;
    s->config.max_pulse_ms = max_pulse_ms;
    return WINK_OK;
}

wink_status_t dal_servo_deinit(dal_servo_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }  /* idempotent no-op on un-init dev */

    /* 1. Best-effort turn servo off (set duty to 0 -> limp state) */
    WINK_IGNORE_UNUSED(dal_servo_safe_off(dev));

    /* 2. Deinitialize PWM channel */
    pal_pwm_deinit(dev->config.pwm_channel);

    /* Keep channel and owner for resource release and GPIO lookup */
    uint8_t channel = dev->config.pwm_channel;
    const char *owner = dev->config.owner;

    /* 3. Query GPIO pin associated with this channel, and reset it to INPUT */
    wink_pin_t pin = -1;
    wink_status_t pin_st = pal_pwm_channel_pin(channel, &pin);
    if (pin_st == WINK_OK && pin >= 0) {
        WINK_IGNORE_UNUSED(pal_gpio_init(pin, PAL_GPIO_INPUT));
    }

    /* 4. Release PWM channel resource claim */
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, (uint32_t)channel, owner));

    /* 5. Clear the instance data completely to guarantee no residual state */
    memset(dev, 0, sizeof(dal_servo_t));

    return WINK_OK;
}
