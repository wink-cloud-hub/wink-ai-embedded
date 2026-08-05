// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_nonblocking_strict.c
 * @brief Strict non-blocking mode compile-time contract tests.
 */
#define WINK_STRICT_NONBLOCKING 1

#include "wink_status.h"

#include "pal_hal.h"
#include "pal_osal.h"
#include "hal/pal_rmt.h"
#include "dal_ultrasonic.h"
#include "dal_eeprom.h"
#include "dal_gps.h"

#include <stddef.h>

void setUp(void) {}
void tearDown(void) {}

enum {
    NB_HAS_pal_gpio_init                 = sizeof(&pal_gpio_init),
    NB_HAS_pal_gpio_read                 = sizeof(&pal_gpio_read),
    NB_HAS_pal_gpio_write                = sizeof(&pal_gpio_write),
    NB_HAS_pal_pwm_init                  = sizeof(&pal_pwm_init),
    NB_HAS_pal_pwm_set_duty              = sizeof(&pal_pwm_set_duty),
    NB_HAS_pal_pwm_deinit                = sizeof(&pal_pwm_deinit),
    NB_HAS_pal_pwm_channel_pin           = sizeof(&pal_pwm_channel_pin),
    NB_HAS_pal_i2c_port_pins             = sizeof(&pal_i2c_port_pins),

    NB_HAS_pal_os_busy_wait_us           = sizeof(&pal_os_busy_wait_us),
    NB_HAS_pal_os_get_ms                 = sizeof(&pal_os_get_ms),
    NB_HAS_pal_os_get_us                 = sizeof(&pal_os_get_us),
    NB_HAS_pal_os_mutex_create           = sizeof(&pal_os_mutex_create),
    NB_HAS_pal_os_mutex_unlock           = sizeof(&pal_os_mutex_unlock),
    NB_HAS_pal_os_mutex_destroy          = sizeof(&pal_os_mutex_destroy),
    NB_HAS_pal_os_task_delete            = sizeof(&pal_os_task_delete),
    NB_HAS_pal_os_critical_enter         = sizeof(&pal_os_critical_enter),
    NB_HAS_pal_os_critical_exit          = sizeof(&pal_os_critical_exit),
    NB_HAS_pal_os_ringbuf_create         = sizeof(&pal_os_ringbuf_create),
    NB_HAS_pal_os_ringbuf_push           = sizeof(&pal_os_ringbuf_push),
    NB_HAS_pal_os_ringbuf_pop            = sizeof(&pal_os_ringbuf_pop),

    NB_HAS_pal_rmt_pulse_capture_init      = sizeof(&pal_rmt_pulse_capture_init),
    NB_HAS_pal_rmt_pulse_capture_deinit    = sizeof(&pal_rmt_pulse_capture_deinit),
    NB_HAS_pal_rmt_pulse_capture_is_active = sizeof(&pal_rmt_pulse_capture_is_active),

    NB_HAS_dal_ultrasonic_init             = sizeof(&dal_ultrasonic_init),
    NB_HAS_dal_ultrasonic_get_cached       = sizeof(&dal_ultrasonic_get_cached_distance),
    NB_HAS_dal_ultrasonic_apply_override   = sizeof(&dal_ultrasonic_apply_override),
    NB_HAS_dal_gps_poll                    = sizeof(&dal_gps_poll),
    NB_HAS_dal_gps_get_position            = sizeof(&dal_gps_get_position),
};

int main(void) { return 0; }
