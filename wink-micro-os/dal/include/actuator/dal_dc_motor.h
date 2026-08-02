#ifndef DAL_DC_MOTOR_H
#define DAL_DC_MOTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
#include "pal_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Brushed DC motor electrical topology (Phase 1: IN/IN only).
 */
typedef enum {
    DAL_DC_MOTOR_VARIANT_IN_IN = 0,        /* default — today's PWM + IN_A + IN_B */
    DAL_DC_MOTOR_VARIANT_PHASE_ENABLE = 1, /* reserved */
    DAL_DC_MOTOR_VARIANT_PWM_ON_IN = 2,    /* reserved */
} dal_dc_motor_variant_t;

/**
 * @brief Brushed DC motor (H-bridge PWM) configuration.
 *
 * Control semantic: open-loop duty / signed speed (ADR-0048).
 * Default topology: IN/IN (PWM speed + two direction inputs).
 *
 * IN/IN truth table (dir_pin_a = A, dir_pin_b = B):
 * @code
 *   dir_a  dir_b | state
 *     0      0   | coast
 *     1      0   | forward
 *     0      1   | reverse
 *     1      1   | brake (short)
 * @endcode
 *
 * ``enable_pin`` (optional STBY / nSLEEP etc.; assumed **high-active**):
 *
 * Init-time contract (before normalization):
 * - Preferred unused sentinel: ``-1``.
 * - ``0`` is also treated as unused (C zero-init / omitted designated-init
 *   field). GPIO 0 cannot be used as enable without a future explicit
 *   polarity or sentinel path.
 * - Active enable: ``enable_pin > 0``.
 *
 * After ``dal_dc_motor_init``, stored ``enable_pin`` is normalized:
 * ``-1`` = unused; ``>= 0`` (= input ``> 0``) = active. Omitted when the
 * module ties enable hard-high.
 *
 * No apply_override wire yet. Future serialization follows config member
 * order.
 */
typedef struct {
    const char *owner;     /* Resource-claim owner static string */
    uint8_t pwm_channel;   /* PWM channel for speed */
    wink_pin_t dir_pin_a;  /* Direction pin A */
    wink_pin_t dir_pin_b;  /* Direction pin B (optional; -1 if unused) */
    uint32_t pwm_freq_hz;  /* PWM freq; 0 → default 20000 Hz */
    dal_dc_motor_variant_t variant; /* 0 = IN_IN (today's path) */
    wink_pin_t enable_pin; /* STBY/nSLEEP; -1 unused; 0→-1 at init; >0 active */
    bool invert;           /* true = swap forward/reverse direction; default false */
} dal_dc_motor_config_t;

/**
 * @brief Brushed DC motor handle (POD).
 */
typedef struct {
    dal_dc_motor_config_t config; /* Config copy */
    float current_speed;          /* Last set speed in [-1.0, 1.0] */
    bool initialized;             /* Init succeeded */
} dal_dc_motor_t;

/* ABI stability: config MUST remain the first member (DAL-S-011). */
_Static_assert(offsetof(dal_dc_motor_t, config) == 0, "config must be the first member");

/**
 * @brief Initialize brushed DC motor driver.
 *
 * Non-IN_IN topologies return ``WINK_ERR_UNSUPPORTED`` (fail-closed).
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_init(dal_dc_motor_t *dev,
                                const dal_dc_motor_config_t *cfg);

/**
 * @brief Set open-loop speed.
 *
 * @param speed -1.0 (full reverse) … 1.0 (full forward).
 *        0.0 applies coast (both dir pins LOW / inactive, PWM duty 0).
 *        When ``config.invert == true``, the direction sense is swapped:
 *        positive speed drives the motor in the "reverse" direction.
 *
 * When an enable pin was configured at init (stored ``enable_pin >= 0``),
 * non-zero speed pulls enable HIGH before applying direction and duty.
 *
 * @note API Contract:
 *   - Thread-safe: No (caller must serialize; 多任务访问需外部互斥)
 *   - ISR-safe: No
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_set_speed(dal_dc_motor_t *dev, float speed);

/**
 * @brief Read back the current set speed (no I/O; reads cached value).
 *
 * @param dev DC motor instance handle
 * @param out_speed Output: current speed in [-1.0, 1.0]
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev/out_speed 非 NULL；dal_dc_motor_init() 已成功。
 *   - Blocking: No.
 *   - Thread-safe: No; ISR-safe: No.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_get_speed(const dal_dc_motor_t *dev, float *out_speed);

/**
 * @brief Short-brake: both direction pins HIGH, PWM duty 0.
 *
 * Requires ``dir_pin_b >= 0``. Single-dir configs return
 * ``WINK_ERR_UNSUPPORTED`` (never silently coast).
 *
 * When an enable pin was configured at init (stored ``enable_pin >= 0``),
 * enable is driven HIGH so the H-bridge can short the windings.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_brake(dal_dc_motor_t *dev);

/**
 * @brief Freewheel / coast: dir pins inactive (LOW), PWM duty 0.
 *
 * Same electrical state as ``dal_dc_motor_set_speed(dev, 0.0f)``.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_coast(dal_dc_motor_t *dev);

/**
 * @brief Fault / registry safe-off hierarchy (ADR-0048 + enable path).
 *
 * 1. Enable configured at init (stored ``enable_pin >= 0``; input ``> 0``):
 *    brake when ``dir_pin_b >= 0``, then drive enable LOW (hard off);
 *    always returns ``WINK_OK``.
 * 2. No enable and ``dir_pin_b >= 0``: ``dal_dc_motor_brake``.
 * 3. No enable and single dir pin: ``WINK_ERR_UNSUPPORTED`` (not coast).
 */
wink_status_t dal_dc_motor_safe_off(dal_dc_motor_t *dev);

/**
 * @brief Deinitialize brushed DC motor driver.
 *
 * ADR-0024 清场：safe_off → 停 PWM → GPIO reset → 释放资源 → memset 清零。
 *
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。
 *   - Blocking: No.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Idempotent: 未 init 时返回 WINK_OK。
 */
wink_status_t dal_dc_motor_deinit(dal_dc_motor_t *dev);

#ifdef __cplusplus
}
#endif

/* ── Compile-time pruning stubs ── */
#if !defined(WINK_USE_DC_MOTOR) || !WINK_USE_DC_MOTOR
#define WINK_DC_MOTOR_DISABLED_MSG \
    "DC motor driver not enabled; add a \"dc_motor\" device to " \
    "wink-app.json (or set -DWINK_USE_DC_MOTOR=ON)."
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_init(dal_dc_motor_t *dev,
                                const dal_dc_motor_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_set_speed(dal_dc_motor_t *dev, float speed);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_get_speed(const dal_dc_motor_t *dev, float *out_speed);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_brake(dal_dc_motor_t *dev);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_coast(dal_dc_motor_t *dev);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) wink_status_t dal_dc_motor_safe_off(dal_dc_motor_t *dev);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG)
wink_status_t dal_dc_motor_deinit(dal_dc_motor_t *dev);
#endif /* !WINK_USE_DC_MOTOR */

#endif /* DAL_DC_MOTOR_H */
