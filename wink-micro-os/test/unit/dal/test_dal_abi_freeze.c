// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_abi_freeze.c
 * @brief DAL struct layout ABI freeze and static assertion unit tests.
 */
#include "unity.h"
#include "wink_status.h"

#include "comm/dal_gps.h"
#include "storage/dal_eeprom.h"
#include "actuator/dal_dc_motor.h"
#include "actuator/dal_rc_servo.h"
#include "input/dal_button.h"
#include "output/dal_led.h"
#include "display/dal_mono_oled.h"
#include "sensor/dal_encoder.h"
#include "sensor/dal_ultrasonic.h"
#include "sensor/dal_load_cell.h"
#include "input/dal_analog_knob.h"
#include "input/dal_keypad.h"

#include <stddef.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define DAL_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#  define DAL_STATIC_ASSERT(cond, msg) typedef char dal_static_assert_##__LINE__[(cond) ? 1 : -1]
#endif

DAL_STATIC_ASSERT(sizeof(dal_analog_knob_config_t) > 0, "dal_analog_knob_config_t size check");
DAL_STATIC_ASSERT(sizeof(dal_analog_knob_t) > 0, "dal_analog_knob_t size check");
DAL_STATIC_ASSERT(offsetof(dal_analog_knob_t, config) == 0, "dal_analog_knob_t config at offset 0");

DAL_STATIC_ASSERT(sizeof(dal_keypad_config_t) > 0, "dal_keypad_config_t size check");
DAL_STATIC_ASSERT(sizeof(dal_keypad_t) > 0, "dal_keypad_t size check");
DAL_STATIC_ASSERT(offsetof(dal_keypad_t, config) == 0, "dal_keypad_t config at offset 0");

DAL_STATIC_ASSERT(sizeof(dal_led_config_t) > 0, "dal_led_config_t size check");
DAL_STATIC_ASSERT(sizeof(dal_led_t) > 0, "dal_led_t size check");
DAL_STATIC_ASSERT(offsetof(dal_led_t, config) == 0, "dal_led_t config at offset 0");

DAL_STATIC_ASSERT(sizeof(dal_button_config_t) > 0, "dal_button_config_t size check");
DAL_STATIC_ASSERT(sizeof(dal_button_t) > 0, "dal_button_t size check");
DAL_STATIC_ASSERT(offsetof(dal_button_t, config) == 0, "dal_button_t config at offset 0");

DAL_STATIC_ASSERT(sizeof(dal_dc_motor_config_t) > 0, "dal_dc_motor_config_t size check");
DAL_STATIC_ASSERT(sizeof(dal_dc_motor_t) > 0, "dal_dc_motor_t size check");
DAL_STATIC_ASSERT(offsetof(dal_dc_motor_t, config) == 0, "dal_dc_motor_t config at offset 0");

DAL_STATIC_ASSERT(sizeof(dal_rc_servo_config_t) > 0, "dal_rc_servo_config_t size check");
DAL_STATIC_ASSERT(sizeof(dal_rc_servo_t) > 0, "dal_rc_servo_t size check");
DAL_STATIC_ASSERT(offsetof(dal_rc_servo_t, config) == 0, "dal_rc_servo_t config at offset 0");

DAL_STATIC_ASSERT(sizeof(dal_encoder_config_t) > 0, "dal_encoder_config_t size check");
DAL_STATIC_ASSERT(sizeof(dal_encoder_t) > 0, "dal_encoder_t size check");
DAL_STATIC_ASSERT(offsetof(dal_encoder_t, config) == 0, "dal_encoder_t config at offset 0");

DAL_STATIC_ASSERT(sizeof(dal_ultrasonic_config_t) > 0, "dal_ultrasonic_config_t size check");
DAL_STATIC_ASSERT(sizeof(dal_ultrasonic_t) > 0, "dal_ultrasonic_t size check");
DAL_STATIC_ASSERT(offsetof(dal_ultrasonic_t, config) == 0, "dal_ultrasonic_t config at offset 0 (DAL-S-011)");

DAL_STATIC_ASSERT(sizeof(dal_mono_oled_config_t) > 0, "dal_mono_oled_config_t size check");
DAL_STATIC_ASSERT(sizeof(dal_mono_oled_t) > 0, "dal_mono_oled_t size check");
DAL_STATIC_ASSERT(offsetof(dal_mono_oled_t, config) == 0, "dal_mono_oled_t config at offset 0 (DAL-S-011)");

DAL_STATIC_ASSERT(sizeof(dal_gps_config_t) > 0, "dal_gps_config_t size check");
DAL_STATIC_ASSERT(sizeof(dal_gps_position_t) > 0, "dal_gps_position_t size check");
DAL_STATIC_ASSERT(sizeof(dal_gps_t) > 0, "dal_gps_t size check");
DAL_STATIC_ASSERT(offsetof(dal_gps_t, config) == 0, "dal_gps_t config offset 0");

DAL_STATIC_ASSERT(sizeof(dal_load_cell_config_t) > 0, "dal_load_cell_config_t size check");
DAL_STATIC_ASSERT(sizeof(dal_load_cell_t) > 0, "dal_load_cell_t size check");
DAL_STATIC_ASSERT(offsetof(dal_load_cell_t, config) == 0, "dal_load_cell_t config at offset 0");

DAL_STATIC_ASSERT(sizeof(dal_eeprom_config_t) > 0, "dal_eeprom_config_t size check");
DAL_STATIC_ASSERT(sizeof(dal_eeprom_t) > 0, "dal_eeprom_t size check");
DAL_STATIC_ASSERT(offsetof(dal_eeprom_t, config) == 0, "dal_eeprom_t config offset 0");

void setUp(void) {}
void tearDown(void) {}

void test_abi_layout_freeze_member_positions(void) {
    dal_analog_knob_t knob = {0};
    dal_keypad_t keypad = {0};
    dal_led_t led = {0};
    dal_button_t btn = {0};
    dal_dc_motor_t motor = {0};
    dal_rc_servo_t servo = {0};
    dal_encoder_t enc = {0};
    dal_ultrasonic_t ultra = {0};
    dal_load_cell_t lc = {0};
    dal_mono_oled_t oled = {0};
    dal_gps_t gps = {0};
    dal_eeprom_t ee = {0};

    TEST_ASSERT_EQUAL_PTR((void *)&knob, (void *)&knob.config);
    TEST_ASSERT_EQUAL_PTR((void *)&keypad, (void *)&keypad.config);
    TEST_ASSERT_EQUAL_PTR((void *)&led, (void *)&led.config);
    TEST_ASSERT_EQUAL_PTR((void *)&btn, (void *)&btn.config);
    TEST_ASSERT_EQUAL_PTR((void *)&motor, (void *)&motor.config);
    TEST_ASSERT_EQUAL_PTR((void *)&servo, (void *)&servo.config);
    TEST_ASSERT_EQUAL_PTR((void *)&enc, (void *)&enc.config);
    TEST_ASSERT_EQUAL_PTR((void *)&oled, (void *)&oled.config);
    TEST_ASSERT_EQUAL_PTR((void *)&gps, (void *)&gps.config);
    TEST_ASSERT_EQUAL_PTR((void *)&ee, (void *)&ee.config);
    TEST_ASSERT_EQUAL_PTR((void *)&ultra, (void *)&ultra.config);
    TEST_ASSERT_EQUAL_PTR((void *)&lc, (void *)&lc.config);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_abi_layout_freeze_member_positions);
    return UNITY_END();
}
