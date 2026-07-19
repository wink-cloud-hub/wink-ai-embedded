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
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 12, "arduino_compat"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 13, "arduino_compat"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 14, "arduino_compat"));
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

void test_arduino_string_and_print(void) {
    // Test String allocation and concatenation
    String s1 = "Hello";
    String s2 = " World";
    String s3 = s1 + s2;
    TEST_ASSERT_EQUAL_STRING("Hello World", s3.c_str());

    // Test IPAddress printing compatibility
    IPAddress ip(192, 168, 1, 100);
    String ipStr = ip.toString();
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", ipStr.c_str());
}

void test_arduino_serial_loopback(void) {
    // Inject input into our mock HardwareSerial buffer
    Serial.injectInput("ArduinoTest");
    
    TEST_ASSERT_EQUAL_INT(11, Serial.available());
    TEST_ASSERT_EQUAL_INT('A', Serial.peek());
    TEST_ASSERT_EQUAL_INT('A', Serial.read());
    TEST_ASSERT_EQUAL_INT(10, Serial.available());
    
    // Read the rest of the buffer
    char buf[16] = {0};
    int idx = 0;
    while (Serial.available() > 0 && idx < 15) {
        buf[idx++] = (char)Serial.read();
    }
    TEST_ASSERT_EQUAL_STRING("rduinoTest", buf);
    TEST_ASSERT_EQUAL_INT(0, Serial.available());
}

extern "C" int run_arduino_compat_tests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_arduino_gpio_basic);
    RUN_TEST(test_arduino_timing_basic);
    RUN_TEST(test_arduino_string_and_print);
    RUN_TEST(test_arduino_serial_loopback);
    return UNITY_END();
}

extern "C" {
void setup(void) {}
void loop(void) {}
}
