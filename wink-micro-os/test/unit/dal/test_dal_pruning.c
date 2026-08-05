// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_pruning.c
 * @brief DAL header pruning self-consistency and symbol link checks.
 */
#include "dal_led.h"
#include "dal_button.h"
#include "dal_ultrasonic.h"
#include "dal_rc_servo.h"
#include "dal_mono_oled.h"
#include "dal_gps.h"
#include "dal_eeprom.h"
#include "actuator/dal_dc_motor.h"
#include "sensor/dal_encoder.h"

#define USE_FN(fn) do { volatile void *_p = (void *)(uintptr_t)(fn); (void)_p; } while (0)

int main(void) {
    USE_FN(dal_led_init);
    USE_FN(dal_led_on);
    USE_FN(dal_led_off);
    USE_FN(dal_led_set);
    USE_FN(dal_led_toggle);
    USE_FN(dal_led_deinit);

    USE_FN(dal_button_init);
    USE_FN(dal_button_poll);
    USE_FN(dal_button_is_pressed);
    USE_FN(dal_button_was_pressed);
    USE_FN(dal_button_on_event);
    USE_FN(dal_button_set_long_press_ms);
    USE_FN(dal_button_enable_isr_counter);
    USE_FN(dal_button_get_edge_count);
    USE_FN(dal_button_reset_edge_count);
    USE_FN(dal_button_deinit);

    USE_FN(dal_ultrasonic_init);
    USE_FN(dal_ultrasonic_request_measurement);
    USE_FN(dal_ultrasonic_get_cached_distance);
    USE_FN(dal_ultrasonic_apply_override);
    USE_FN(dal_ultrasonic_deinit);

    USE_FN(dal_rc_servo_init);
    USE_FN(dal_rc_servo_set_angle);
    USE_FN(dal_rc_servo_safe_off);
    USE_FN(dal_rc_servo_apply_override);
    USE_FN(dal_rc_servo_deinit);

    USE_FN(dal_mono_oled_init);
    USE_FN(dal_mono_oled_clear);
    USE_FN(dal_mono_oled_draw_text);
    USE_FN(dal_mono_oled_flush);
    USE_FN(dal_mono_oled_deinit);

    USE_FN(dal_gps_poll);
    USE_FN(dal_gps_get_position);
    USE_FN(dal_gps_deinit);

    USE_FN(dal_eeprom_deinit);

    USE_FN(dal_dc_motor_init);
    USE_FN(dal_dc_motor_set_speed_promille);
    USE_FN(dal_dc_motor_get_speed_promille);
    USE_FN(dal_dc_motor_brake);
    USE_FN(dal_dc_motor_coast);
    USE_FN(dal_dc_motor_safe_off);
    USE_FN(dal_dc_motor_deinit);

    USE_FN(dal_encoder_init);
    USE_FN(dal_encoder_get_count);
    USE_FN(dal_encoder_reset);
    USE_FN(dal_encoder_deinit);

    return 0;
}
