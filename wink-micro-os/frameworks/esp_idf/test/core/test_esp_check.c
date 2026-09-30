/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "esp_check.h"
#include <setjmp.h>
#include <string.h>

typedef void (*esp_error_check_failed_hook_t)(esp_err_t rc, const char *file, int line,
                                             const char *function, const char *expression);
extern void esp_error_check_set_failed_hook(esp_error_check_failed_hook_t hook);

static jmp_buf s_jmp_buf;
static esp_err_t s_captured_rc;
static int s_captured_line;
static const char *s_captured_file;
static const char *s_captured_func;
static const char *s_captured_expr;
static int s_hook_call_count;
static int s_without_abort_count;

static void test_failed_hook(esp_err_t rc, const char *file, int line,
                             const char *function, const char *expression) {
    s_hook_call_count++;
    s_captured_rc = rc;
    s_captured_file = file;
    s_captured_line = line;
    s_captured_func = function;
    s_captured_expr = expression;
    longjmp(s_jmp_buf, 1);
}

static void test_without_abort_hook(esp_err_t rc, const char *file, int line,
                                    const char *function, const char *expression) {
    (void)file;
    (void)line;
    (void)function;
    (void)expression;
    s_without_abort_count++;
    s_captured_rc = rc;
}

void setUp(void) {
    s_hook_call_count = 0;
    s_without_abort_count = 0;
    s_captured_rc = ESP_OK;
    s_captured_file = NULL;
    s_captured_line = 0;
    s_captured_func = NULL;
    s_captured_expr = NULL;
    esp_error_check_set_failed_hook(test_failed_hook);
}

void tearDown(void) {
    esp_error_check_set_failed_hook(NULL);
}

void test_esp_error_check_success(void) {
    int canary = 0;
    ESP_ERROR_CHECK(ESP_OK);
    canary = 42;
    TEST_ASSERT_EQUAL_INT(42, canary);
    TEST_ASSERT_EQUAL_INT(0, s_hook_call_count);
}

void test_esp_error_check_failure_aborts(void) {
    int canary = 0;
    if (setjmp(s_jmp_buf) == 0) {
        ESP_ERROR_CHECK(ESP_ERR_INVALID_ARG);
        canary = 100; /* Should never reach here */
    } else {
        canary = 200; /* Successfully intercepted via longjmp */
    }

    TEST_ASSERT_EQUAL_INT(200, canary);
    TEST_ASSERT_EQUAL_INT(1, s_hook_call_count);
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, s_captured_rc);
    TEST_ASSERT_NOT_NULL(s_captured_file);
    TEST_ASSERT_TRUE(strstr(s_captured_file, "test_esp_check.c") != NULL);
    TEST_ASSERT_TRUE(s_captured_line > 0);
    TEST_ASSERT_NOT_NULL(s_captured_func);
    TEST_ASSERT_NOT_NULL(s_captured_expr);
    TEST_ASSERT_EQUAL_STRING("ESP_ERR_INVALID_ARG", s_captured_expr);
}

void test_esp_error_check_without_abort_success(void) {
    s_hook_call_count = 0;
    esp_err_t ret = ESP_ERROR_CHECK_WITHOUT_ABORT(ESP_OK);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ret);
    TEST_ASSERT_EQUAL_INT(0, s_hook_call_count);
}

void test_esp_error_check_without_abort_failure(void) {
    s_without_abort_count = 0;
    esp_error_check_set_failed_hook(test_without_abort_hook);

    esp_err_t ret = ESP_ERROR_CHECK_WITHOUT_ABORT(ESP_ERR_TIMEOUT);
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_TIMEOUT, ret);
    TEST_ASSERT_EQUAL_INT(1, s_without_abort_count);
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_TIMEOUT, s_captured_rc);
}

static esp_err_t helper_return_on_error(esp_err_t val) {
    ESP_RETURN_ON_ERROR(val, "TAG", "error occurred");
    return ESP_OK;
}

void test_esp_return_on_error(void) {
    TEST_ASSERT_EQUAL_INT32(ESP_OK, helper_return_on_error(ESP_OK));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NO_MEM, helper_return_on_error(ESP_ERR_NO_MEM));
}

static esp_err_t helper_return_on_false(bool condition, esp_err_t err_code) {
    ESP_RETURN_ON_FALSE(condition, err_code, "TAG", "condition failed");
    return ESP_OK;
}

void test_esp_return_on_false(void) {
    TEST_ASSERT_EQUAL_INT32(ESP_OK, helper_return_on_false(true, ESP_ERR_INVALID_STATE));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_STATE, helper_return_on_false(false, ESP_ERR_INVALID_STATE));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_esp_error_check_success);
    RUN_TEST(test_esp_error_check_failure_aborts);
    RUN_TEST(test_esp_error_check_without_abort_success);
    RUN_TEST(test_esp_error_check_without_abort_failure);
    RUN_TEST(test_esp_return_on_error);
    RUN_TEST(test_esp_return_on_false);
    return UNITY_END();
}
