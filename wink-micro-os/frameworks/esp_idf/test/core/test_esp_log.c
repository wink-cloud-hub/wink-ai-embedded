/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "esp_log.h"
#include <stdarg.h>
#include <stdint.h>

static int s_vprintf_calls = 0;

static int test_vprintf(const char *fmt, va_list args) {
    (void)fmt;
    (void)args;
    s_vprintf_calls++;
    return 0;
}

void setUp(void) {
    esp_log_set_vprintf(NULL);
    esp_log_level_set(NULL, ESP_LOG_INFO);
    s_vprintf_calls = 0;
}

void tearDown(void) {
    esp_log_set_vprintf(NULL);
    esp_log_level_set(NULL, ESP_LOG_INFO);
}

void test_log_level_and_timestamps(void) {
    esp_log_level_set("tag", ESP_LOG_VERBOSE);
    TEST_ASSERT_EQUAL(ESP_LOG_VERBOSE, esp_log_level_get("tag"));
    esp_log_level_set(NULL, ESP_LOG_WARN);
    TEST_ASSERT_EQUAL(ESP_LOG_WARN, esp_log_level_get(NULL));
    TEST_ASSERT_GREATER_OR_EQUAL(0, (int)esp_log_timestamp());
    TEST_ASSERT_NOT_NULL(esp_log_system_timestamp());
    TEST_ASSERT_GREATER_OR_EQUAL(0, (int)esp_log_early_timestamp());
}

void test_log_custom_vprintf_routing(void) {
    vprintf_like_t old = esp_log_set_vprintf(test_vprintf);
    TEST_ASSERT_NULL(old);

    ESP_LOGW("unit", "hello %d", 1);
    TEST_ASSERT_EQUAL(1, s_vprintf_calls);

    esp_log_write(ESP_LOG_ERROR, "unit", "write %d", 2);
    TEST_ASSERT_EQUAL(2, s_vprintf_calls);

    /* Below current level: filtered, no vprintf call */
    esp_log_level_set("unit", ESP_LOG_ERROR);
    ESP_LOGD("unit", "hidden");
    TEST_ASSERT_EQUAL(2, s_vprintf_calls);

    TEST_ASSERT_TRUE(esp_log_set_vprintf(NULL) == test_vprintf);
}

void test_log_buffer_helpers(void) {
    uint8_t bytes[20] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 };

    esp_log_level_set("unit", ESP_LOG_VERBOSE);
    ESP_LOG_BUFFER_HEX("unit", bytes, sizeof(bytes)); /* >16 triggers line chunking */
    ESP_LOG_BUFFER_CHAR("unit", "abc", 3);
    ESP_LOG_BUFFER_HEXDUMP("unit", bytes, 4, ESP_LOG_INFO);

    /* Guard paths: NULL buffer / zero length are no-ops */
    esp_log_buffer_hex_internal("unit", NULL, 4, ESP_LOG_INFO);
    esp_log_buffer_hex_internal("unit", bytes, 0, ESP_LOG_INFO);
    esp_log_buffer_char_internal("unit", NULL, 4, ESP_LOG_INFO);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_log_level_and_timestamps);
    RUN_TEST(test_log_custom_vprintf_routing);
    RUN_TEST(test_log_buffer_helpers);
    return UNITY_END();
}
