/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_err.h"
#include "esp_idf_wink.h"
#include "wink_status.h"

esp_err_t wink_status_to_esp_err(wink_status_t status) {
    switch (status) {
        case WINK_OK:                      return ESP_OK;
        case WINK_ERR_INVALID_ARG:         return ESP_ERR_INVALID_ARG;
        case WINK_ERR_NO_MEM:              return ESP_ERR_NO_MEM;
        case WINK_ERR_RESOURCE_EXHAUSTED:  return ESP_ERR_NO_MEM;
        case WINK_ERR_FULL:                return ESP_ERR_NO_MEM;
        case WINK_ERR_INVALID_STATE:       return ESP_ERR_INVALID_STATE;
        case WINK_ERR_NOT_INITIALIZED:     return ESP_ERR_INVALID_STATE;
        case WINK_ERR_BUSY:                return ESP_ERR_INVALID_STATE;
        case WINK_ERR_TIMEOUT:             return ESP_ERR_TIMEOUT;
        case WINK_ERR_NOT_FOUND:           return ESP_ERR_NOT_FOUND;
        case WINK_ERR_DISCONNECTED:        return ESP_ERR_NOT_FOUND;
        case WINK_ERR_EMPTY:               return ESP_ERR_NOT_FOUND;
        case WINK_ERR_CHECKSUM:            return ESP_ERR_INVALID_CRC;
        case WINK_ERR_UNSUPPORTED:         return ESP_ERR_NOT_SUPPORTED;
        case WINK_ERR_OUT_OF_RANGE:        return ESP_ERR_INVALID_SIZE;
        case WINK_ERR_PERMISSION:          return ESP_ERR_NOT_ALLOWED;
        default:                           return ESP_FAIL;
    }
}

wink_status_t esp_err_to_wink_status(esp_err_t err) {
    switch (err) {
        case ESP_OK:                       return WINK_OK;
        case ESP_ERR_NO_MEM:               return WINK_ERR_NO_MEM;
        case ESP_ERR_INVALID_ARG:          return WINK_ERR_INVALID_ARG;
        case ESP_ERR_INVALID_STATE:        return WINK_ERR_INVALID_STATE;
        case ESP_ERR_INVALID_SIZE:         return WINK_ERR_OUT_OF_RANGE;
        case ESP_ERR_TIMEOUT:              return WINK_ERR_TIMEOUT;
        case ESP_ERR_NOT_FOUND:            return WINK_ERR_NOT_FOUND;
        case ESP_ERR_NOT_SUPPORTED:        return WINK_ERR_UNSUPPORTED;
        case ESP_ERR_INVALID_CRC:          return WINK_ERR_CHECKSUM;
        case ESP_ERR_NOT_ALLOWED:          return WINK_ERR_PERMISSION;
        case ESP_FAIL:                     return WINK_ERR_HARDWARE;
        default:                           return WINK_ERR_HARDWARE;
    }
}
