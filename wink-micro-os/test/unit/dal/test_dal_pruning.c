/*
 * test_dal_pruning.c — sanity check that DAL headers self-consistent under
 * default (all-ON) build.
 *
 * This TU does NOT actually test that WINK_UNAVAILABLE_MSG fires — that is a
 * compile-negative test (see test_dal_pruning.cmake, invoked via ctest -P).
 * Instead this file exists to:
 *   1. Guarantee all seven dal_*.h headers parse without errors when all
 *      WINK_USE_XXX are defined to 1 (the production default).
 *   2. Reference each driver's init symbol so the linker would complain if
 *      the driver's .c file were omitted from libdal.a (defence against
 *      accidentally dropping a source from dal/CMakeLists.txt).
 *
 * It intentionally does NOT call any init function (would require PAL to
 * be fully wired up); it just takes the address of every public API to
 * force an undefined-reference link error if a driver is compiled out
 * while WINK_USE_X=1 (should never happen; this is a build-system belt-
 * and-braces check).
 */
#include "dal_led.h"
#include "dal_button.h"
#include "dal_ultrasonic.h"
#include "dal_rc_servo.h"
#include "dal_ssd1306.h"
#include "dal_gps.h"
#include "dal_eeprom.h"
#include "actuator/dal_dc_motor.h"
#include "sensor/dal_encoder.h"

/* Silence unused-variable warnings in a single obvious macro. */
#define USE_FN(fn) do { volatile void *_p = (void *)(uintptr_t)(fn); (void)_p; } while (0)

int main(void) {
    /* LED */
    USE_FN(dal_led_init);
    USE_FN(dal_led_on);
    USE_FN(dal_led_off);
    USE_FN(dal_led_set);
    USE_FN(dal_led_toggle);
    USE_FN(dal_led_deinit);
    /* Button */
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
    /* Ultrasonic */
    USE_FN(dal_ultrasonic_init);
    USE_FN(dal_ultrasonic_request_measurement);
    USE_FN(dal_ultrasonic_get_cached_distance);
    USE_FN(dal_ultrasonic_apply_override);
    USE_FN(dal_ultrasonic_deinit);
    /* Servo */
    USE_FN(dal_rc_servo_init);
    USE_FN(dal_rc_servo_set_angle);
    USE_FN(dal_rc_servo_safe_off);
    USE_FN(dal_rc_servo_apply_override);
    USE_FN(dal_rc_servo_deinit);
    /* SSD1306 */
    USE_FN(dal_ssd1306_init);
    USE_FN(dal_ssd1306_clear);
    USE_FN(dal_ssd1306_draw_text);
    USE_FN(dal_ssd1306_flush);
    USE_FN(dal_ssd1306_deinit);
    /* GPS */
    USE_FN(dal_gps_poll);
    USE_FN(dal_gps_get_position);
    USE_FN(dal_gps_deinit);
    /* EEPROM */
    USE_FN(dal_eeprom_deinit);
    /* (all three other eeprom APIs are WINK_BLOCKING + WINK_STRICT_NONBLOCKING-gated;
     *  we don't reference them here to avoid polluting the all-ON sanity check
     *  with WINK_BLOCKING deprecation warnings.) */
    /* DC motor (ADR-0048) */
    USE_FN(dal_dc_motor_init);
    USE_FN(dal_dc_motor_set_speed);
    USE_FN(dal_dc_motor_brake);
    USE_FN(dal_dc_motor_coast);
    USE_FN(dal_dc_motor_safe_off);
    USE_FN(dal_dc_motor_deinit);
    /* Encoder */
    USE_FN(dal_encoder_init);
    USE_FN(dal_encoder_get_count);
    USE_FN(dal_encoder_reset);
    USE_FN(dal_encoder_deinit);

    return 0;
}
