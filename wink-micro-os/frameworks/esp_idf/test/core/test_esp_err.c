/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "esp_err.h"
#include "esp_idf_wink.h"
#include "wink_status.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_esp_err_from_wink(void) {
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_err_from_wink(WINK_OK));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, esp_err_from_wink(WINK_ERR_INVALID_ARG));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NO_MEM, esp_err_from_wink(WINK_ERR_NO_MEM));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_STATE, esp_err_from_wink(WINK_ERR_INVALID_STATE));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_TIMEOUT, esp_err_from_wink(WINK_ERR_TIMEOUT));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_FOUND, esp_err_from_wink(WINK_ERR_NOT_FOUND));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_CRC, esp_err_from_wink(WINK_ERR_CHECKSUM));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_STATE, esp_err_from_wink(WINK_ERR_BUSY));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED, esp_err_from_wink(WINK_ERR_UNSUPPORTED));
    TEST_ASSERT_EQUAL_INT32(ESP_FAIL, esp_err_from_wink((wink_status_t)-999));
}

void test_wink_status_from_esp(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_status_from_esp(ESP_OK));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NO_MEM, wink_status_from_esp(ESP_ERR_NO_MEM));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_status_from_esp(ESP_ERR_INVALID_ARG));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, wink_status_from_esp(ESP_ERR_INVALID_STATE));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, wink_status_from_esp(ESP_ERR_TIMEOUT));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_FOUND, wink_status_from_esp(ESP_ERR_NOT_FOUND));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, wink_status_from_esp(ESP_ERR_NOT_SUPPORTED));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_CHECKSUM, wink_status_from_esp(ESP_ERR_INVALID_CRC));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_HARDWARE, wink_status_from_esp(ESP_FAIL));
}

void test_esp_err_to_name(void) {
    TEST_ASSERT_EQUAL_STRING("ESP_OK", esp_err_to_name(ESP_OK));
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_INVALID_ARG", esp_err_to_name(ESP_ERR_INVALID_ARG));
    char buf[64];
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_TIMEOUT", esp_err_to_name_r(ESP_ERR_TIMEOUT, buf, sizeof(buf)));
}

void test_esp_err_to_name_full_table(void) {
    TEST_ASSERT_EQUAL_STRING("ESP_FAIL", esp_err_to_name(ESP_FAIL));
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_NO_MEM", esp_err_to_name(ESP_ERR_NO_MEM));
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_INVALID_STATE", esp_err_to_name(ESP_ERR_INVALID_STATE));
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_INVALID_SIZE", esp_err_to_name(ESP_ERR_INVALID_SIZE));
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_NOT_FOUND", esp_err_to_name(ESP_ERR_NOT_FOUND));
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_NOT_SUPPORTED", esp_err_to_name(ESP_ERR_NOT_SUPPORTED));
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_INVALID_RESPONSE", esp_err_to_name(ESP_ERR_INVALID_RESPONSE));
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_INVALID_CRC", esp_err_to_name(ESP_ERR_INVALID_CRC));
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_INVALID_VERSION", esp_err_to_name(ESP_ERR_INVALID_VERSION));
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_INVALID_MAC", esp_err_to_name(ESP_ERR_INVALID_MAC));
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_NOT_FINISHED", esp_err_to_name(ESP_ERR_NOT_FINISHED));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN ERROR", esp_err_to_name((esp_err_t)0x7FFFFFFF));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_HARDWARE, wink_status_from_esp((esp_err_t)0x7FFFFFFF));

    char buf[32];
    TEST_ASSERT_EQUAL_STRING("", esp_err_to_name_r(ESP_OK, NULL, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("", esp_err_to_name_r(ESP_OK, buf, 0));
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_INVALID_STATE",
                             esp_err_to_name_r(ESP_ERR_INVALID_STATE, buf, sizeof(buf)));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_esp_err_from_wink);
    RUN_TEST(test_wink_status_from_esp);
    RUN_TEST(test_esp_err_to_name);
    RUN_TEST(test_esp_err_to_name_full_table);
    return UNITY_END();
}
