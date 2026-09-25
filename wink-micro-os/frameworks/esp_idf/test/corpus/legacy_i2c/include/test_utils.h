/* SPDX-License-Identifier: CC0-1.0 */
#ifndef TEST_UTILS_H_
#define TEST_UTILS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "unity.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#elif defined(_MSC_VER)
#pragma warning(disable: 4244 4245 4305 4312 4505 4189 4101)
#endif

#define UNITY_TEST_UID_(a, b) a##b
#define UNITY_TEST_UID(a, b) UNITY_TEST_UID_(a, b)

#if defined(__GNUC__) || defined(__clang__)
#define TEST_CASE(name_, desc_) static void __attribute__((unused)) UNITY_TEST_UID(test_func_, __LINE__)(void)
#define TEST_CASE_MULTIPLE_DEVICES(name_, desc_, ...) static void __attribute__((unused)) UNITY_TEST_UID(test_func_multidev_, __LINE__)(void)
#else
#define TEST_CASE(name_, desc_) static void UNITY_TEST_UID(test_func_, __LINE__)(void)
#define TEST_CASE_MULTIPLE_DEVICES(name_, desc_, ...) static void UNITY_TEST_UID(test_func_multidev_, __LINE__)(void)
#endif

#define TEST_ESP_OK(x) TEST_ASSERT_EQUAL_INT32(ESP_OK, (x))

static inline void test_utils_record_free_mem(void) {}
static inline size_t test_utils_get_leak_level(void) { return 0; }

static inline void unity_send_signal(const char *signal) { (void)signal; }
static inline void unity_wait_for_signal(const char *signal) { (void)signal; }

#ifdef __cplusplus
}
#endif

#endif /* TEST_UTILS_H_ */
