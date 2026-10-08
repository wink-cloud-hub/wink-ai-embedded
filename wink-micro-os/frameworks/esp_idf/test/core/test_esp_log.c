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

#include "hal/pal_uart.h"

void stub_uart_get_last_tx(uint8_t port, uint8_t *out_data, size_t *out_len);
void stub_uart_force_failure(uint8_t port, wink_status_t err);

static int bridge_vprintf_wrapper(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap_copy);
    va_end(ap_copy);
    if (n > 0) {
        size_t write_len = ((size_t)n < sizeof(buf) - 1) ? (size_t)n : (sizeof(buf) - 1);
        (void)pal_uart_write(0, (const uint8_t *)buf, (uint32_t)write_len);
    }
    va_end(ap);
    return n;
}

void test_wave1_r1_log_uart_bridge_bounds(void) {
    (void)pal_uart_init(0, 1, 3, 115200);

    static char pattern[2048];
    memset(pattern, 'A', sizeof(pattern) - 1);
    pattern[sizeof(pattern) - 1] = '\0';

    static uint8_t tx_buf[1024];
    size_t tx_len = 0;

    /* 511 bytes */
    pattern[511] = '\0';
    int n = bridge_vprintf_wrapper("%s", pattern);
    TEST_ASSERT_EQUAL(511, n);
    tx_len = sizeof(tx_buf);
    stub_uart_get_last_tx(0, tx_buf, &tx_len);
    TEST_ASSERT_EQUAL_UINT32(511, tx_len);
    TEST_ASSERT_EQUAL_MEMORY(pattern, tx_buf, 511);

    /* 512 bytes: vsnprintf returns 512, but only 511 non-NUL chars written */
    pattern[511] = 'B';
    pattern[512] = '\0';
    n = bridge_vprintf_wrapper("%s", pattern);
    TEST_ASSERT_EQUAL(512, n);
    tx_len = sizeof(tx_buf);
    stub_uart_get_last_tx(0, tx_buf, &tx_len);
    TEST_ASSERT_EQUAL_UINT32(511, tx_len);
    TEST_ASSERT_EQUAL_MEMORY(pattern, tx_buf, 511);

    /* 513 bytes: returns 513, UART writes 511 bytes */
    pattern[512] = 'C';
    pattern[513] = '\0';
    n = bridge_vprintf_wrapper("%s", pattern);
    TEST_ASSERT_EQUAL(513, n);
    tx_len = sizeof(tx_buf);
    stub_uart_get_last_tx(0, tx_buf, &tx_len);
    TEST_ASSERT_EQUAL_UINT32(511, tx_len);
    TEST_ASSERT_EQUAL_MEMORY(pattern, tx_buf, 511);

    /* 1024 bytes: returns 1024, UART writes 511 bytes */
    pattern[513] = 'D';
    pattern[1024] = '\0';
    n = bridge_vprintf_wrapper("%s", pattern);
    TEST_ASSERT_EQUAL(1024, n);
    tx_len = sizeof(tx_buf);
    stub_uart_get_last_tx(0, tx_buf, &tx_len);
    TEST_ASSERT_EQUAL_UINT32(511, tx_len);

    /* --- L-02: Empty string & non-positive result -> no UART write --- */
    tx_len = 0;
    n = bridge_vprintf_wrapper("");
    TEST_ASSERT_EQUAL(0, n);

    /* --- L-03: PAL UART failure does not recurse or change n --- */
    stub_uart_force_failure(0, WINK_ERR_IO);
    pattern[10] = '\0';
    n = bridge_vprintf_wrapper("%s", pattern);
    TEST_ASSERT_EQUAL(10, n);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_log_level_and_timestamps);
    RUN_TEST(test_log_custom_vprintf_routing);
    RUN_TEST(test_log_buffer_helpers);
    RUN_TEST(test_wave1_r1_log_uart_bridge_bounds);
    return UNITY_END();
}
