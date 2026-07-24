/**
 * @file test_wasm_devices_sim.c
 * @brief Unit tests for the WASM C-side virtual devices simulation models.
 */
#include "unity.h"
#include "devices/wasm_sim_registry.h"
#include "wasm_bridge.h"
#include <string.h>

// Declare internal reset/helpers from dev models for testing
void wasm_dev_ultrasonic_reset(void);
uint32_t wasm_dev_ultrasonic_get_pulse_us(uint8_t pin);

void setUp(void) {
    pal_wasm_sim_reset_all_devices();
}

void tearDown(void) {
    // No-op
}

void test_ssd1306_scheme_a_retired(void) {
    /* Phase E: C SSD1306 model + pal_wasm_get_ssd1306_fb removed.
     * I2C short-circuit stays false; observation SSOT is Unisim plugin. */
    TEST_ASSERT_FALSE(wasm_sim_i2c_dev_exists(0x3C));
    TEST_ASSERT_FALSE(wasm_sim_i2c_dev_exists(0x3D));

    uint8_t init_cmd[] = { 0x00, 0x20, 0x00 };
    wink_status_t st =
        wasm_sim_i2c_dev_transfer(0, 0x3C, init_cmd, sizeof(init_cmd), NULL, 0);
    TEST_ASSERT_EQUAL(WINK_ERR_UNSUPPORTED, st);
}

void test_virtual_servo_angle_conversion(void) {
    // SG90 Servo pulse limits: 500us (0 deg) to 2500us (180 deg)
    // Formula under 50Hz (20ms period):
    // pulse_us = duty_percent * 200.0f
    
    // Set 2.5% duty cycle -> 500us -> 0 degrees
    wasm_sim_pwm_set_duty(1, 2.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, pal_wasm_get_servo_angle(1));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.5f, pal_wasm_get_pwm_duty_percent(1));

    // Set 7.5% duty cycle -> 1500us -> 90 degrees
    wasm_sim_pwm_set_duty(1, 7.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, pal_wasm_get_servo_angle(1));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.5f, pal_wasm_get_pwm_duty_percent(1));

    // Set 12.5% duty cycle -> 2500us -> 180 degrees
    wasm_sim_pwm_set_duty(1, 12.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 180.0f, pal_wasm_get_servo_angle(1));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.5f, pal_wasm_get_pwm_duty_percent(1));

    // Out of bounds check: 1.0% duty cycle -> < 500us -> clamp to 0
    wasm_sim_pwm_set_duty(1, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, pal_wasm_get_servo_angle(1));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, pal_wasm_get_pwm_duty_percent(1));

    // Out of bounds check: 15.0% duty cycle -> > 2500us -> clamp to 180
    wasm_sim_pwm_set_duty(1, 15.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 180.0f, pal_wasm_get_servo_angle(1));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 15.0f, pal_wasm_get_pwm_duty_percent(1));
}

void test_virtual_ultrasonic_distance_and_pulses(void) {
    uint8_t pin = 12;

    // Default distance is -1.0f, should return 0 (triggers fallback)
    TEST_ASSERT_EQUAL(0, wasm_dev_ultrasonic_get_pulse_us(pin));

    // Inject distance = 15.5cm
    pal_wasm_set_ultrasonic_distance(pin, 15.5f);

    // Pulse width should be (uint32_t)(15.5 * 58) = 899 microseconds
    TEST_ASSERT_EQUAL(899, wasm_dev_ultrasonic_get_pulse_us(pin));

    // Out of bounds checks
    pal_wasm_set_ultrasonic_distance(pin, 1.0f);
    TEST_ASSERT_EQUAL(116, wasm_dev_ultrasonic_get_pulse_us(pin)); // Minimum clamp (2cm * 58 = 116us)

    pal_wasm_set_ultrasonic_distance(pin, 450.0f);
    TEST_ASSERT_EQUAL(0, wasm_dev_ultrasonic_get_pulse_us(pin)); // Timeout (>400cm)

    // Resetting should clear distance back to -1.0f
    pal_wasm_sim_reset_all_devices();
    TEST_ASSERT_EQUAL(0, wasm_dev_ultrasonic_get_pulse_us(pin));
}

void test_virtual_gpio_inputs_and_outputs(void) {
    uint8_t pin = 5;
    bool level = false;

    TEST_ASSERT_FALSE(wasm_sim_gpio_input_is_set(pin, &level));
    TEST_ASSERT_FALSE(pal_wasm_get_gpio_output(pin));

    /* P3: pal_wasm_set_gpio_input drives Arbiter only — C shadow stays unset */
    pal_wasm_set_gpio_input(pin, true);
    TEST_ASSERT_FALSE(wasm_sim_gpio_input_is_set(pin, &level));

    /* Direct shadow helper still works for leftover callers / tests */
    wasm_sim_gpio_set_input(pin, true);
    TEST_ASSERT_TRUE(wasm_sim_gpio_input_is_set(pin, &level));
    TEST_ASSERT_TRUE(level);

    wasm_sim_gpio_write(pin, true);
    TEST_ASSERT_TRUE(pal_wasm_get_gpio_output(pin));

    pal_wasm_sim_reset_all_devices();
    TEST_ASSERT_FALSE(wasm_sim_gpio_input_is_set(pin, &level));
    TEST_ASSERT_FALSE(pal_wasm_get_gpio_output(pin));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ssd1306_scheme_a_retired);
    RUN_TEST(test_virtual_servo_angle_conversion);
    RUN_TEST(test_virtual_ultrasonic_distance_and_pulses);
    RUN_TEST(test_virtual_gpio_inputs_and_outputs);
    return UNITY_END();
}
