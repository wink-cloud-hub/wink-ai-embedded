extern "C" {
#include "unity.h"
}
#include "Arduino.h"

extern "C" {
#include "pal_resource.h"
#include "internal/pal_test_loopback.h"
}

void setUp(void) {
    wink_arduino_init();
    pal_resource_reset();
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 12, "test"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 13, "test"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 14, "test"));
}

void tearDown(void) {
    pal_resource_reset();
}

void test_arduino_gpio_basic(void) {
    // Enable loopback: output pin 13 linked to input pin 14
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(13, 14));

    pinMode(13, OUTPUT);
    pinMode(14, INPUT);

    digitalWrite(13, HIGH);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(14));
    
    digitalWrite(13, LOW);
    TEST_ASSERT_EQUAL(LOW, digitalRead(14));
    
    pinMode(12, INPUT_PULLUP);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(12));

    pal_test_disable_hardware_loopback(13, 14);
}

void test_arduino_timing_basic(void) {
    unsigned long start = millis();
    unsigned long start_us = micros();
    
    delayMicroseconds(100);
    
    TEST_ASSERT_TRUE(micros() > start_us);
    TEST_ASSERT_TRUE(millis() >= start);
}

extern "C" int run_arduino_compat_tests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_arduino_gpio_basic);
    RUN_TEST(test_arduino_timing_basic);
    return UNITY_END();
}
