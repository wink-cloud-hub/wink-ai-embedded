// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_mcpwm.h
 * @brief PAL Motor Control PWM (MCPWM) subsystem interface.
 *
 * Provides complementary pairs, dead-time generators, hardware asynchronous brake,
 * multi-timer phase locking, and capture capabilities for FOC and power electronics.
 */

#ifndef PAL_MCPWM_H
#define PAL_MCPWM_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "wink_compiler.h"
#include "hal/pal_pin_types.h"
#include "pal_osal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pal_mcpwm_timer_s    *pal_mcpwm_timer_handle_t;
typedef struct pal_mcpwm_oper_s     *pal_mcpwm_oper_handle_t;
typedef struct pal_mcpwm_cmp_s      *pal_mcpwm_cmp_handle_t;
typedef struct pal_mcpwm_fault_s    *pal_mcpwm_fault_handle_t;
typedef struct pal_mcpwm_cap_s      *pal_mcpwm_cap_handle_t;

typedef struct {
    uint8_t          mcpwm_unit;        /**< 0 or 1 on ESP32 */
    uint8_t          timer_id;          /**< 0..2 */
    uint32_t         pwm_freq_hz;       /**< e.g. 20000 Hz for FOC */
    uint16_t         counter_top;       /**< Peak counter ticks */
    pal_os_core_id_t core_affinity;     /**< Core affinity */
    bool             iram_safe;         /**< true = allocate in IRAM */
} pal_mcpwm_timer_cfg_t;

typedef struct {
    pal_mcpwm_timer_handle_t timer;
    uint8_t                  operator_id;          /**< 0..2 */
    wink_pin_t               pin_pwm_a;
    wink_pin_t               pin_pwm_b;            /**< Complementary pin (-1 if unused) */
    uint16_t                 deadtime_red_ticks;   /**< Rising edge delay ticks */
    uint16_t                 deadtime_fed_ticks;   /**< Falling edge delay ticks */
    bool                     complementary_enable; /**< true = enable hardware dead-time generator */
} pal_mcpwm_oper_cfg_t;

typedef struct {
    pal_mcpwm_oper_handle_t oper;
    uint32_t                initial_duty_ticks;
} pal_mcpwm_cmp_cfg_t;

typedef struct {
    uint8_t     fault_id;                          /**< 0..2 */
    wink_pin_t  fault_pin;
    bool        active_level;                      /**< true = active HIGH, false = active LOW */
    bool        async_brake;                       /**< true = hardware async brake without CPU */
    bool        safe_level_a;                      /**< Output level for PWM_A during fault */
    bool        safe_level_b;                      /**< Output level for PWM_B during fault */
    void      (*on_brake_isr)(void *arg);          /**< Optional PAL_ISR notification */
    void       *on_brake_arg;
} pal_mcpwm_fault_cfg_t;

typedef struct {
    wink_pin_t  cap_pin;
    uint8_t     cap_channel;                       /**< 0..2 */
    bool        pull_up;
    void      (*on_capture_isr)(void *arg, uint32_t ts_ns, bool rising);
    void       *on_capture_arg;
} pal_mcpwm_cap_cfg_t;

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_timer(const pal_mcpwm_timer_cfg_t *cfg, pal_mcpwm_timer_handle_t *out_timer);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_oper(const pal_mcpwm_oper_cfg_t *cfg, pal_mcpwm_oper_handle_t *out_oper);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_cmp(const pal_mcpwm_cmp_cfg_t *cfg, pal_mcpwm_cmp_handle_t *out_cmp);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_fault(const pal_mcpwm_fault_cfg_t *cfg, pal_mcpwm_fault_handle_t *out_fault);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_capture(const pal_mcpwm_cap_cfg_t *cfg, pal_mcpwm_cap_handle_t *out_cap);

wink_status_t pal_mcpwm_timer_start(pal_mcpwm_timer_handle_t t);
wink_status_t pal_mcpwm_timer_stop(pal_mcpwm_timer_handle_t t);

/**
 * @brief Set compare duty ticks (ISR-safe and IRAM-safe for fast-loop update).
 */
wink_status_t pal_mcpwm_set_duty_ticks(pal_mcpwm_cmp_handle_t cmp, uint32_t duty_ticks);

/**
 * @brief Global hardware sync GPIO configuration.
 */
wink_status_t pal_mcpwm_sync_gpio_config(wink_pin_t sync_gpio, bool active_level);

/**
 * @brief Enable multi-timer phase locking.
 */
wink_status_t pal_mcpwm_timer_enable_phase_lock(pal_mcpwm_timer_handle_t t, uint32_t phase_ticks);

/**
 * @brief Trigger software synchronization event for all phase-locked timers.
 */
wink_status_t pal_mcpwm_trigger_software_sync(void);

/**
 * @brief Clear a latched fault state and restore normal PWM operation.
 */
wink_status_t pal_mcpwm_fault_clear(pal_mcpwm_fault_handle_t f);

/**
 * @brief Delete timer and release all associated operators and resources.
 */
void pal_mcpwm_del_timer(pal_mcpwm_timer_handle_t t);

#ifdef __cplusplus
}
#endif

#endif /* PAL_MCPWM_H */
