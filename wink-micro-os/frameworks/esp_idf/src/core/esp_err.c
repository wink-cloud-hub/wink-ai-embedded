/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_err.h"
#include "wink_status.h"
#include <string.h>

esp_err_t esp_err_from_wink(wink_status_t status) {
    switch (status) {
        case WINK_OK:                  return ESP_OK;
        case WINK_ERR_INVALID_ARG:     return ESP_ERR_INVALID_ARG;
        case WINK_ERR_NO_MEM:          return ESP_ERR_NO_MEM;
        case WINK_ERR_INVALID_STATE:   return ESP_ERR_INVALID_STATE;
        case WINK_ERR_TIMEOUT:         return ESP_ERR_TIMEOUT;
        case WINK_ERR_NOT_FOUND:       return ESP_ERR_NOT_FOUND;
        case WINK_ERR_CHECKSUM:        return ESP_ERR_INVALID_CRC;
        case WINK_ERR_BUSY:            return ESP_ERR_INVALID_STATE;
        case WINK_ERR_UNSUPPORTED:     return ESP_ERR_NOT_SUPPORTED;
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
