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
    const char *owner;              /* Resource-claim owner static string */
    uint32_t pwm_freq_hz;           /* PWM freq; 0 → default 20000 Hz */
    dal_dc_motor_variant_t variant; /* 0 = IN_IN (today's path) */
    wink_pin_t dir_pin_a;           /* Direction pin A */
    wink_pin_t dir_pin_b;           /* Direction pin B (optional; -1 if unused) */
    wink_pin_t enable_pin;          /* STBY/nSLEEP; -1 unused; 0→-1 at init; >0 active */
    uint8_t pwm_channel;            /* PWM channel for speed */
    bool invert;                    /* true = swap forward/reverse direction; default false */
} dal_dc_motor_config_t;

/**
 * @brief Brushed DC motor handle (POD).
 */
typedef struct {
    dal_dc_motor_config_t config; /* Config copy */
    int16_t current_speed_promille; /* Last set speed in promille [-1000, 1000] */
    bool initialized;             /* Init succeeded */
} dal_dc_motor_t;

/* ABI stability: config MUST remain the first member (DAL-S-011).
 * Offsets below are compiler-verified (spec §2.3): enum is int (4B),
 * wink_pin_t is int16_t (2B). Recompute with the target compiler if the
 * struct layout changes. */
_Static_assert(offsetof(dal_dc_motor_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_dc_motor_config_t) == 20, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_dc_motor_t, initialized) == 22, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_dc_motor_t) == 24, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_dc_motor_config_t) == 24, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_dc_motor_t, initialized) == 26, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_dc_motor_t) == 32, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief Initialize brushed DC motor driver.
 *
 * Claims PWM channel + direction GPIO (and optional enable pin) via the PAL
 * resource manager, then programs the hardware. Non-IN_IN topologies return
 * ``WINK_ERR_UNSUPPORTED`` (fail-closed).
 *
 * On failure all resources claimed during this call are rolled back
 * (DAL-L-008); the handle is left zeroed / safe-to-deinit.
 *
 * @param dev   Motor instance handle (caller-owned storage).
 * @param cfg   Configuration (deep-copied into dev->config on success).
 * @return wink_status_t
 *
 * @note API Contract:
 *   - Preconditions: dev/cfg/cfg->owner non-NULL; cfg->pwm_channel <
 *     PAL_PWM_CHANNELS; cfg->dir_pin_a >= 0; cfg->variant == IN_IN;
 *     dev not already initialized.
 *   - Postconditions: WINK_OK → dev->initialized=true; config deep-copied;
 *     motor starts at zero energy (coast: both dir pins inactive, PWM duty 0,
 *     enable driven LOW if present). Init-to-Ready: accepts set_speed_promille
 *     immediately without a separate arm/enable step (DAL-BC-001).
 *   - Range: pwm_freq_hz: 0 → default 20000 Hz; enable_pin: -1 unused,
 *     0 normalized to -1 (zero-init sentinel), >0 active.
 *   - Blocking: No (PAL claim + GPIO/PWM init, no busy-waits).
 *   - Thread-safe: No (caller must serialize init against other methods,
 *     DAL-C-040).
 *   - ISR-safe: No.
 *   - Side-effects: claims PAL PWM-channel + GPIO-pin resources; programs
 *     PWM frequency and GPIO direction; writes enable LOW.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_UNSUPPORTED /
 *     WINK_ERR_ALREADY_INITIALIZED / WINK_ERR_BUSY /
 *     WINK_ERR_RESOURCE_EXHAUSTED (propagated from PAL).
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_init(dal_dc_motor_t *dev,
                                const dal_dc_motor_config_t *cfg);

/**
 * @brief Set open-loop speed in promille [-1000, 1000].
 *
 * @param dev             Motor instance handle.
 * @param speed_promille  -1000 (full reverse) … 1000 (full forward).
 *                        0 applies coast (both dir pins LOW / inactive, PWM duty 0).
 *                        When ``config.invert == true``, direction sense is swapped.
 * @return wink_status_t
 *
 * @note API Contract:
 *   - Preconditions: dev non-NULL; dal_dc_motor_init() succeeded.
 *   - Postconditions: WINK_OK → dev->current_speed_promille holds the (saturated)
 *     requested speed; hardware direction + PWM duty updated.
 *   - Range: speed_promille [-1000, 1000] normalized; out-of-range saturates (clamps).
 *     speed_promille == 0 → coast. When config.invert == true, direction sense is
 *     swapped.
 *   - Blocking: No.
 *   - Thread-safe: No (caller must serialize).
 *   - ISR-safe: No (calls pal_pwm_set_duty).
 *   - Side-effects: updates GPIO direction pins + PWM duty; may drive enable
 *     HIGH; writes dev->current_speed_promille.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED
 *     / propagated PAL errors.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_set_speed_promille(dal_dc_motor_t *dev, int16_t speed_promille);

/**
 * @brief Read back the current set speed in promille (reads cached value).
 *
 * @param dev                DC motor instance handle.
 * @param out_speed_promille Output: current speed in promille [-1000, 1000].
 * @return wink_status_t
 *
 * @note API Contract:
 *   - Preconditions: dev / out_speed_promille non-NULL; dal_dc_motor_init() succeeded.
 *   - Postconditions: WINK_OK → *out_speed_promille holds dev->current_speed_promille.
 *   - Range: *out_speed_promille ∈ [-1000, 1000].
 *   - Blocking: No.
 *   - Thread-safe: No.
 *   - ISR-safe: No.
 *   - Side-effects: Writes *out_speed_promille.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_get_speed_promille(const dal_dc_motor_t *dev, int16_t *out_speed_promille);

/**
 * @brief Short-brake: both direction pins HIGH, PWM duty 0.
 *
 * Requires ``dir_pin_b >= 0``. Single-dir configs return
 * ``WINK_ERR_UNSUPPORTED`` (never silently coast).
 *
 * When an enable pin was configured at init (stored ``enable_pin >= 0``),
 * enable is driven HIGH so the H-bridge can short the windings.
 *
 * @param dev Motor instance handle.
 * @return wink_status_t
 *
 * @note API Contract:
 *   - Preconditions: dev non-NULL; dal_dc_motor_init() succeeded;
 *     config.dir_pin_b >= 0 (else WINK_ERR_UNSUPPORTED).
 *   - Postconditions: WINK_OK → both dir pins HIGH, PWM duty 0,
 *     dev->current_speed_promille = 0.
 *   - Blocking: No.
 *   - Thread-safe: No (caller must serialize).
 *   - ISR-safe: No (calls pal_pwm_set_duty).
 *   - Side-effects: drives dir pins + PWM duty; may drive enable HIGH;
 *     writes dev->current_speed_promille = 0.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED
 *     / WINK_ERR_UNSUPPORTED (single-dir) / propagated PAL errors.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_brake(dal_dc_motor_t *dev);

/**
 * @brief Freewheel / coast: dir pins inactive (LOW), PWM duty 0.
 *
 * Same electrical state as ``dal_dc_motor_set_speed_promille(dev, 0)``.
 *
 * @param dev Motor instance handle.
 * @return wink_status_t
 *
 * @note API Contract:
 *   - Preconditions: dev non-NULL; dal_dc_motor_init() succeeded.
 *   - Postconditions: WINK_OK → both dir pins LOW/inactive, PWM duty 0,
 *     dev->current_speed_promille = 0.
 *   - Blocking: No.
 *   - Thread-safe: No (caller must serialize).
 *   - ISR-safe: No (calls pal_pwm_set_duty).
 *   - Side-effects: drives dir pins + PWM duty; may drive enable HIGH;
 *     writes dev->current_speed_promille = 0.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED
 *     / propagated PAL errors.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_coast(dal_dc_motor_t *dev);

/**
 * @brief Emergency/fault safe-off (ADR-0048): bound to brake (+ enable LOW).
 *
 * Shutdown hierarchy:
 * 1. Enable configured at init (stored ``enable_pin >= 0``; input ``> 0``):
 *    brake when ``dir_pin_b >= 0`` (coast when single-dir), then drive
 *    enable LOW (hard off); always returns ``WINK_OK``.
 * 2. No enable and ``dir_pin_b >= 0``: ``dal_dc_motor_brake``.
 * 3. No enable and single dir pin: ``WINK_ERR_UNSUPPORTED`` (not coast).
 *
 * @param dev Motor instance handle.
 * @return wink_status_t
 *
 * @note API Contract:
 *   - Preconditions: dev non-NULL. Does NOT require prior init.
 *   - Postconditions: best-effort zero-energy output.
 *   - Blocking: No.
 *   - Thread-safe: No.
 *   - ISR-safe: No (calls pal_pwm_set_duty / pal_gpio_write).
 *   - Reentrancy: Yes (idempotent; safe to call repeatedly).
 *   - Idempotent: returns WINK_OK when dev is uninitialized (DAL-L-022);
 *     invoked by wink_actuator_safe_off_all() on watchdog/panic/rollback
 *     paths, where "nothing to shut off" is success, not an error.
 *   - Side-effects: drives dir pins/PWM duty to a brake/coast state; pulls
 *     enable LOW when present; writes dev->current_speed_promille = 0.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG (dev NULL) /
 *     WINK_ERR_UNSUPPORTED (case 3: no enable, single-dir).
 */
wink_status_t dal_dc_motor_safe_off(dal_dc_motor_t *dev);

/**
 * @brief Deinitialize brushed DC motor driver.
 *
 * ADR-0024 清场：safe_off → 停 PWM → GPIO reset → 释放资源 → memset 清零。
 *
 * @param dev Motor instance handle.
 * @return wink_status_t
 *
 * @note API Contract:
 *   - Preconditions: dev non-NULL.
 *   - Postconditions: WINK_OK → dev memset to 0; initialized=false; resources released.
 *   - Blocking: No.
 *   - Thread-safe: No.
 *   - ISR-safe: No (calls PAL release/deinit functions).
 *   - Idempotent: Returns WINK_OK when dev is uninitialized.
 *   - Side-effects: Releases PAL PWM channel & GPIO pin claims; resets GPIO pins; zeros dev memory.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG.
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
wink_status_t dal_dc_motor_set_speed_promille(dal_dc_motor_t *dev, int16_t speed_promille);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_get_speed_promille(const dal_dc_motor_t *dev, int16_t *out_speed_promille);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_brake(dal_dc_motor_t *dev);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_coast(dal_dc_motor_t *dev);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) wink_status_t dal_dc_motor_safe_off(dal_dc_motor_t *dev);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG)
wink_status_t dal_dc_motor_deinit(dal_dc_motor_t *dev);
#endif /* !WINK_USE_DC_MOTOR */

#endif /* DAL_DC_MOTOR_H */
