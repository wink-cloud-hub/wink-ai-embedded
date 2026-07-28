#ifndef DAL_DC_MOTOR_H
#define DAL_DC_MOTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "pal_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Brushed DC motor electrical topology (Phase 1: IN/IN only).
 */
typedef enum {
    DAL_DC_MOTOR_MODE_IN_IN = 0,        /* default — today's PWM + IN_A + IN_B */
    DAL_DC_MOTOR_MODE_PHASE_ENABLE = 1, /* reserved */
    DAL_DC_MOTOR_MODE_PWM_ON_IN = 2,    /* reserved */
} dal_dc_motor_drive_mode_t;

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
 * ``enable_pin`` (optional, default -1): STBY / nSLEEP etc.; assumed
 * **high-active** (pull HIGH to enable the H-bridge). Omitted when the
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
    dal_dc_motor_drive_mode_t drive_mode; /* 0 = IN_IN (today's path) */
    wink_pin_t enable_pin; /* STBY/nSLEEP; -1 if unused */
} dal_dc_motor_config_t;

/**
 * @brief Brushed DC motor handle (POD).
 */
typedef struct {
    dal_dc_motor_config_t config; /* Config copy */
    float current_speed;          /* Last set speed in [-1.0, 1.0] */
    bool initialized;             /* Init succeeded */
} dal_dc_motor_t;

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
 *
 * When ``enable_pin >= 0``, non-zero speed pulls enable HIGH before
 * applying direction and duty.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_set_speed(dal_dc_motor_t *dev, float speed);

/**
 * @brief Short-brake: both direction pins HIGH, PWM duty 0.
 *
 * Requires ``dir_pin_b >= 0``. Single-dir configs return
 * ``WINK_ERR_UNSUPPORTED`` (never silently coast).
 *
 * When ``enable_pin >= 0``, enable is driven HIGH so the H-bridge can
 * short the windings.
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
 * 1. ``enable_pin >= 0``: brake when ``dir_pin_b >= 0``, then drive enable
 *    LOW (hard off); always returns ``WINK_OK``.
 * 2. No enable and ``dir_pin_b >= 0``: ``dal_dc_motor_brake``.
 * 3. No enable and single dir pin: ``WINK_ERR_UNSUPPORTED`` (not coast).
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_safe_off(dal_dc_motor_t *dev);

/**
 * @brief Deinitialize brushed DC motor driver.
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
wink_status_t dal_dc_motor_brake(dal_dc_motor_t *dev);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_coast(dal_dc_motor_t *dev);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_safe_off(dal_dc_motor_t *dev);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG)
wink_status_t dal_dc_motor_deinit(dal_dc_motor_t *dev);
#endif /* !WINK_USE_DC_MOTOR */

#endif /* DAL_DC_MOTOR_H */
