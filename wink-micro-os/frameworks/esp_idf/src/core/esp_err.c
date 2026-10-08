/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_err.h"
#include "esp_idf_wink.h"
#include "wink_status.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

esp_err_t esp_err_from_wink(wink_status_t status) {
    switch (status) {
        case WINK_OK:                  return ESP_OK;
        case WINK_ERR_INVALID_ARG:     return ESP_ERR_INVALID_ARG;
        case WINK_ERR_NO_MEM:          return ESP_ERR_NO_MEM;
        case WINK_ERR_RESOURCE_EXHAUSTED: return ESP_ERR_NO_MEM;
        case WINK_ERR_FULL:            return ESP_ERR_NO_MEM;
        case WINK_ERR_INVALID_STATE:   return ESP_ERR_INVALID_STATE;
        case WINK_ERR_NOT_INITIALIZED: return ESP_ERR_INVALID_STATE;
        case WINK_ERR_BUSY:            return ESP_ERR_INVALID_STATE;
        case WINK_ERR_TIMEOUT:         return ESP_ERR_TIMEOUT;
        case WINK_ERR_NOT_FOUND:       return ESP_ERR_NOT_FOUND;
        case WINK_ERR_DISCONNECTED:    return ESP_ERR_NOT_FOUND;
        case WINK_ERR_EMPTY:           return ESP_ERR_NOT_FOUND;
        case WINK_ERR_CHECKSUM:        return ESP_ERR_INVALID_CRC;
        case WINK_ERR_UNSUPPORTED:     return ESP_ERR_NOT_SUPPORTED;
        case WINK_ERR_OUT_OF_RANGE:    return ESP_ERR_INVALID_ARG;
        case WINK_ERR_PERMISSION:      return ESP_ERR_NOT_ALLOWED;
        default:                       return ESP_FAIL;
    }
}

wink_status_t wink_status_from_esp(esp_err_t err) {
    switch (err) {
        case ESP_OK:                   return WINK_OK;
        case ESP_ERR_NO_MEM:           return WINK_ERR_NO_MEM;
        case ESP_ERR_INVALID_ARG:      return WINK_ERR_INVALID_ARG;
        case ESP_ERR_INVALID_STATE:    return WINK_ERR_INVALID_STATE;
        case ESP_ERR_TIMEOUT:          return WINK_ERR_TIMEOUT;
        case ESP_ERR_NOT_FOUND:        return WINK_ERR_NOT_FOUND;
        case ESP_ERR_NOT_SUPPORTED:    return WINK_ERR_UNSUPPORTED;
        case ESP_ERR_INVALID_CRC:      return WINK_ERR_CHECKSUM;
        case ESP_FAIL:                 return WINK_ERR_HARDWARE;
        default:                       return WINK_ERR_HARDWARE;
    }
}

const char *esp_err_to_name(esp_err_t code) {
    switch (code) {
        case ESP_OK:                   return "ESP_OK";
        case ESP_FAIL:                 return "ESP_FAIL";
        case ESP_ERR_NO_MEM:           return "ESP_ERR_NO_MEM";
        case ESP_ERR_INVALID_ARG:      return "ESP_ERR_INVALID_ARG";
        case ESP_ERR_INVALID_STATE:    return "ESP_ERR_INVALID_STATE";
        case ESP_ERR_INVALID_SIZE:     return "ESP_ERR_INVALID_SIZE";
        case ESP_ERR_NOT_FOUND:        return "ESP_ERR_NOT_FOUND";
        case ESP_ERR_NOT_SUPPORTED:    return "ESP_ERR_NOT_SUPPORTED";
        case ESP_ERR_TIMEOUT:          return "ESP_ERR_TIMEOUT";
        case ESP_ERR_INVALID_RESPONSE: return "ESP_ERR_INVALID_RESPONSE";
        case ESP_ERR_INVALID_CRC:      return "ESP_ERR_INVALID_CRC";
        case ESP_ERR_INVALID_VERSION:  return "ESP_ERR_INVALID_VERSION";
        case ESP_ERR_INVALID_MAC:      return "ESP_ERR_INVALID_MAC";
        case ESP_ERR_NOT_FINISHED:     return "ESP_ERR_NOT_FINISHED";

        /* NVS error codes */
        case ESP_ERR_NVS_NOT_INITIALIZED:   return "ESP_ERR_NVS_NOT_INITIALIZED";
        case ESP_ERR_NVS_NOT_FOUND:         return "ESP_ERR_NVS_NOT_FOUND";
        case ESP_ERR_NVS_TYPE_MISMATCH:     return "ESP_ERR_NVS_TYPE_MISMATCH";
        case ESP_ERR_NVS_READ_ONLY:         return "ESP_ERR_NVS_READ_ONLY";
        case ESP_ERR_NVS_NOT_ENOUGH_SPACE:  return "ESP_ERR_NVS_NOT_ENOUGH_SPACE";
        case ESP_ERR_NVS_INVALID_NAME:      return "ESP_ERR_NVS_INVALID_NAME";
        case ESP_ERR_NVS_INVALID_HANDLE:    return "ESP_ERR_NVS_INVALID_HANDLE";
        case ESP_ERR_NVS_REMOVE_FAILED:     return "ESP_ERR_NVS_REMOVE_FAILED";
        case ESP_ERR_NVS_KEY_TOO_LONG:      return "ESP_ERR_NVS_KEY_TOO_LONG";
        case ESP_ERR_NVS_PAGE_FULL:         return "ESP_ERR_NVS_PAGE_FULL";
        case ESP_ERR_NVS_INVALID_STATE:     return "ESP_ERR_NVS_INVALID_STATE";
        case ESP_ERR_NVS_INVALID_LENGTH:    return "ESP_ERR_NVS_INVALID_LENGTH";
        case ESP_ERR_NVS_NO_FREE_PAGES:     return "ESP_ERR_NVS_NO_FREE_PAGES";
        case ESP_ERR_NVS_VALUE_TOO_LONG:    return "ESP_ERR_NVS_VALUE_TOO_LONG";
        case ESP_ERR_NVS_PART_NOT_FOUND:    return "ESP_ERR_NVS_PART_NOT_FOUND";
        case ESP_ERR_NVS_NEW_VERSION_FOUND: return "ESP_ERR_NVS_NEW_VERSION_FOUND";
        case ESP_ERR_NVS_XTS_ENCR_FAILED:   return "ESP_ERR_NVS_XTS_ENCR_FAILED";
        case ESP_ERR_NVS_XTS_DECR_FAILED:   return "ESP_ERR_NVS_XTS_DECR_FAILED";
        case ESP_ERR_NVS_XTS_CFG_FAILED:    return "ESP_ERR_NVS_XTS_CFG_FAILED";
        case ESP_ERR_NVS_XTS_CFG_NOT_FOUND: return "ESP_ERR_NVS_XTS_CFG_NOT_FOUND";
        case ESP_ERR_NVS_ENCR_NOT_SUPPORTED:return "ESP_ERR_NVS_ENCR_NOT_SUPPORTED";
        case ESP_ERR_NVS_KEYS_NOT_INITIALIZED: return "ESP_ERR_NVS_KEYS_NOT_INITIALIZED";
        case ESP_ERR_NVS_CORRUPT_KEY_PART:  return "ESP_ERR_NVS_CORRUPT_KEY_PART";
        case ESP_ERR_NVS_CONTENT_DIFFERS:   return "ESP_ERR_NVS_CONTENT_DIFFERS";
        case ESP_ERR_NVS_WRONG_ENCRYPTION:  return "ESP_ERR_NVS_WRONG_ENCRYPTION";

        default:                       return "UNKNOWN ERROR";
    }
}

const char *esp_err_to_name_r(esp_err_t code, char *buf, size_t buflen) {
    if (!buf || buflen == 0) {
        return "";
    }
    const char *name = esp_err_to_name(code);
    strncpy(buf, name, buflen - 1);
    buf[buflen - 1] = '\0';
    return buf;
}

typedef void (*esp_error_check_failed_hook_t)(esp_err_t rc, const char *file, int line,
                                             const char *function, const char *expression);

static esp_error_check_failed_hook_t s_esp_error_check_failed_hook = NULL;

void esp_error_check_set_failed_hook(esp_error_check_failed_hook_t hook) {
    s_esp_error_check_failed_hook = hook;
}

void _esp_error_check_failed(esp_err_t rc, const char *file, int line, const char *function, const char *expression) {
    printf("ESP_ERROR_CHECK failed: esp_err_t 0x%x (%s) at %p\nfile: \"%s\" line %d\nfunc: %s\nexpression: %s\n",
           rc, esp_err_to_name(rc), (void *)_esp_error_check_failed,
           file ? file : "", line, function ? function : "", expression ? expression : "");
    if (s_esp_error_check_failed_hook != NULL) {
        s_esp_error_check_failed_hook(rc, file, line, function, expression);
    }
    abort();
}

void _esp_error_check_failed_without_abort(esp_err_t rc, const char *file, int line, const char *function, const char *expression) {
    printf("ESP_ERROR_CHECK_WITHOUT_ABORT failed: esp_err_t 0x%x (%s) at %p\nfile: \"%s\" line %d\nfunc: %s\nexpression: %s\n",
           rc, esp_err_to_name(rc), (void *)_esp_error_check_failed_without_abort,
           file ? file : "", line, function ? function : "", expression ? expression : "");
    if (s_esp_error_check_failed_hook != NULL) {
        s_esp_error_check_failed_hook(rc, file, line, function, expression);
    }
}
