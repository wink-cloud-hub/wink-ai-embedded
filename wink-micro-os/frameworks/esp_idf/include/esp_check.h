/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_CHECK_H_
#define ESP_CHECK_H_

#include "esp_err.h"
#include "esp_log.h"
#include "esp_compiler.h"
#include "wink_runtime.h"
#include "wink_fault.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ERROR_CHECK(x) do {                                                \
        esp_err_t __err_rc = (x);                                              \
        if (unlikely(__err_rc != ESP_OK)) {                                    \
            ESP_LOGE("ESP_ERROR_CHECK", "%s failed: esp_err_t 0x%x (%s)",      \
                     #x, __err_rc, esp_err_to_name(__err_rc));                 \
            wink_runtime_raise_fault(WINK_FAULT_RUNTIME(1));                   \
        }                                                                      \
    } while (0)

#define ESP_ERROR_CHECK_WITHOUT_ABORT(x) ({                                    \
        esp_err_t __err_rc = (x);                                              \
        if (unlikely(__err_rc != ESP_OK)) {                                    \
            ESP_LOGE("ESP_ERROR_CHECK", "%s failed: esp_err_t 0x%x (%s)",      \
                     #x, __err_rc, esp_err_to_name(__err_rc));                 \
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

#ifdef __cplusplus
}
#endif

#endif /* ESP_CHECK_H_ */
