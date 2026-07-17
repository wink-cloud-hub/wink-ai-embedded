#ifndef DAL_MOTOR_H
#define DAL_MOTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "pal_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 电机配置结构体
 */
typedef struct {
    const char *owner;     /* 资源占用 owner 静态字符串 */
    uint8_t pwm_channel;   /* 用于调速的 PWM 通道 */
    wink_pin_t dir_pin_a;  /* 方向控制引脚 A */
    wink_pin_t dir_pin_b;  /* 方向控制引脚 B (可选，若不使用设为 -1) */
    uint32_t pwm_freq_hz;  /* PWM 频率，默认可使用 20000 (20kHz) */
} dal_motor_config_t;

/**
 * @brief 电机逻辑句柄
 */
typedef struct {
    dal_motor_config_t config; /* 配置副本 */
    float current_speed;       /* 当前设置的速度，范围为 -1.0 到 1.0 */
    bool initialized;          /* 是否初始化成功 */
} dal_motor_t;

/**
 * @brief 初始化电机驱动
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_motor_init(dal_motor_t *dev, const dal_motor_config_t *cfg);

/**
 * @brief 设置电机速度
 * 
 * @param speed 范围为 -1.0 (全速反转) 到 1.0 (全速正转)，0.0 为停止/刹车。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_motor_set_speed(dal_motor_t *dev, float speed);

/**
 * @brief 紧急停止或释放电机使能（滑行/刹车）
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_motor_safe_off(dal_motor_t *dev);

/**
 * @brief 反初始化电机驱动
 */
wink_status_t dal_motor_deinit(dal_motor_t *dev);

#ifdef __cplusplus
}
#endif

/* ── Compile-time pruning stubs ── */
#if !defined(WINK_USE_MOTOR) || !WINK_USE_MOTOR
#define WINK_MOTOR_DISABLED_MSG \
    "Motor driver not enabled; add a \"motor\" device to wink-app.json " \
    "(or set -DWINK_USE_MOTOR=ON)."
WINK_UNAVAILABLE_MSG(WINK_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_motor_init(dal_motor_t *dev, const dal_motor_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_motor_set_speed(dal_motor_t *dev, float speed);
WINK_UNAVAILABLE_MSG(WINK_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_motor_safe_off(dal_motor_t *dev);
WINK_UNAVAILABLE_MSG(WINK_MOTOR_DISABLED_MSG)
wink_status_t dal_motor_deinit(dal_motor_t *dev);
#endif /* !WINK_USE_MOTOR */

#endif /* DAL_MOTOR_H */
