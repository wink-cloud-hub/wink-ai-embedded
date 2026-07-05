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

void test_virtual_ssd1306_display_commands_and_draw(void) {
    // SSD1306 standard init & page-column config I2C command packet
    // horizontal address mode, page 0..7, col 0..127
    uint8_t init_cmd[] = {
        0x00,              // Control byte: Command
        0x20, 0x00,        // Addressing mode: Horizontal
        0x21, 0x00, 0x7F,  // Column range 0..127
        0x22, 0x00, 0x07   // Page range 0..7
    };

    TEST_ASSERT_TRUE(wasm_sim_i2c_dev_exists(0x3C));
    TEST_ASSERT_TRUE(wasm_sim_i2c_dev_exists(0x3D));
    TEST_ASSERT_FALSE(wasm_sim_i2c_dev_exists(0x1F)); // Non-existent I2C address

    // Send command packet to virtual SSD1306 (address 0x3C)
    wink_status_t st = wasm_sim_i2c_dev_transfer(0, 0x3C, init_cmd, sizeof(init_cmd), NULL, 0);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    // Send page data: 128 bytes of pixel data
    uint8_t data_pkt[129];
    data_pkt[0] = 0x40; // Control byte: Data
    for (int i = 1; i <= 128; i++) {
        data_pkt[i] = (uint8_t)i;
    }
    st = wasm_sim_i2c_dev_transfer(0, 0x3C, data_pkt, sizeof(data_pkt), NULL, 0);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    // Retrieve FB pointer and dimensions
    uint32_t width = 0, height = 0;
    const uint8_t *fb = pal_wasm_get_ssd1306_fb(&width, &height);
    TEST_ASSERT_NOT_NULL(fb);
    TEST_ASSERT_EQUAL(128, width);
    TEST_ASSERT_EQUAL(64, height);

    // Check first page pixels
    for (int i = 0; i < 128; i++) {
        TEST_ASSERT_EQUAL((uint8_t)(i + 1), fb[i]);
    }
}

void test_virtual_servo_angle_conversion(void) {
    // SG90 Servo pulse limits: 500us (0 deg) to 2500us (180 deg)
    // Formula under 50Hz (20ms period):
    // pulse_us = duty_percent * 200.0f
    
    // Set 2.5% duty cycle -> 500us -> 0 degrees
    wasm_sim_pwm_set_duty(1, 2.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, pal_wasm_get_servo_angle(1));

    // Set 7.5% duty cycle -> 1500us -> 90 degrees
    wasm_sim_pwm_set_duty(1, 7.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, pal_wasm_get_servo_angle(1));

    // Set 12.5% duty cycle -> 2500us -> 180 degrees
    wasm_sim_pwm_set_duty(1, 12.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 180.0f, pal_wasm_get_servo_angle(1));

    // Out of bounds check: 1.0% duty cycle -> < 500us -> clamp to 0
    wasm_sim_pwm_set_duty(1, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, pal_wasm_get_servo_angle(1));

    // Out of bounds check: 15.0% duty cycle -> > 2500us -> clamp to 180
    wasm_sim_pwm_set_duty(1, 15.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 180.0f, pal_wasm_get_servo_angle(1));
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

    // Default states should be false
    TEST_ASSERT_FALSE(wasm_sim_gpio_get_input(pin));
    TEST_ASSERT_FALSE(pal_wasm_get_gpio_output(pin));

    // Inject input
    pal_wasm_set_gpio_input(pin, true);
    TEST_ASSERT_TRUE(wasm_sim_gpio_get_input(pin));

    // Write output
    wasm_sim_gpio_write(pin, true);
    TEST_ASSERT_TRUE(pal_wasm_get_gpio_output(pin));

    // Reset should clear states
    pal_wasm_sim_reset_all_devices();
    TEST_ASSERT_FALSE(wasm_sim_gpio_get_input(pin));
    TEST_ASSERT_FALSE(pal_wasm_get_gpio_output(pin));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_virtual_ssd1306_display_commands_and_draw);
    RUN_TEST(test_virtual_servo_angle_conversion);
    RUN_TEST(test_virtual_ultrasonic_distance_and_pulses);
    RUN_TEST(test_virtual_gpio_inputs_and_outputs);
    return UNITY_END();
}
