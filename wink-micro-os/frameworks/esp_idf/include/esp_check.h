/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_CHECK_H_
#define ESP_CHECK_H_

#include "esp_err.h"
#include "esp_log.h"
#include "esp_compiler.h"
#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSERT_FUNC
#if defined(__GNUC__) || defined(__clang__)
#define __ASSERT_FUNC __FUNCTION__
#elif defined(_MSC_VER)
#define __ASSERT_FUNC __FUNCSIG__
#else
#define __ASSERT_FUNC "??"
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#define ESP_CHECK_ATTR_NORETURN __attribute__((__noreturn__))
#elif defined(_MSC_VER)
#define ESP_CHECK_ATTR_NORETURN __declspec(noreturn)
#else
#define ESP_CHECK_ATTR_NORETURN
#endif

void _esp_error_check_failed(esp_err_t rc, const char *file, int line, const char *function, const char *expression) ESP_CHECK_ATTR_NORETURN;
void _esp_error_check_failed_without_abort(esp_err_t rc, const char *file, int line, const char *function, const char *expression);

#undef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(x) do {                                                \
        esp_err_t __err_rc = (x);                                              \
        if (unlikely(__err_rc != ESP_OK)) {                                    \
            _esp_error_check_failed(__err_rc, __FILE__, __LINE__,              \
                                    __ASSERT_FUNC, #x);                        \
        }                                                                      \
    } while (0)

#undef ESP_ERROR_CHECK_WITHOUT_ABORT
#define ESP_ERROR_CHECK_WITHOUT_ABORT(x) ({                                    \
        esp_err_t __err_rc = (x);                                              \
        if (unlikely(__err_rc != ESP_OK)) {                                    \
            _esp_error_check_failed_without_abort(__err_rc, __FILE__, __LINE__,\
                                                  __ASSERT_FUNC, #x);          \
        }                                                                      \
        __err_rc;                                                              \
    })

#define ESP_RETURN_ON_ERROR(x, log_tag, format, ...) do {                      \
        esp_err_t __err_rc = (x);                                              \
        if (unlikely(__err_rc != ESP_OK)) {                                    \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__,      \
                     ##__VA_ARGS__);                                           \
            return __err_rc;                                                   \
        }                                                                      \
    } while (0)

#define ESP_RETURN_ON_FALSE(a, err_code, log_tag, format, ...) do {            \
        if (unlikely(!(a))) {                                                  \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__,      \
                     ##__VA_ARGS__);                                           \
            return err_code;                                                   \
        }                                                                      \
    } while (0)

#define ESP_GOTO_ON_FALSE(a, err_code, goto_tag, log_tag, format, ...) do {   \
        if (unlikely(!(a))) {                                                  \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__,      \
                     ##__VA_ARGS__);                                           \
            ret = err_code;                                                    \
            goto goto_tag;                                                     \
        }                                                                      \
    } while (0)

#define ESP_GOTO_ON_ERROR(x, goto_tag, log_tag, format, ...) do {              \
        esp_err_t __err_rc = (x);                                              \
        if (unlikely(__err_rc != ESP_OK)) {                                    \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__,      \
                     ##__VA_ARGS__);                                           \
            ret = __err_rc;                                                    \
            goto goto_tag;                                                     \
        }                                                                      \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* ESP_CHECK_H_ */
