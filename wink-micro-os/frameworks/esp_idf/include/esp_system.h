/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_SYSTEM_H
#define ESP_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_idf_version.h"

#ifdef __cplusplus
extern "C" {
#endif

void esp_restart(void);

typedef enum {
    ESP_RST_UNKNOWN,    //!< Reset reason can not be determined
    ESP_RST_POWERON,    //!< Reset due to power-on event
    ESP_RST_EXT,        //!< Reset by external pin (not applicable for ESP32)
    ESP_RST_SW,         //!< Software reset via esp_restart
    ESP_RST_PANIC,      //!< Software reset due to exception/panic
    ESP_RST_INT_WDT,    //!< Reset (software or hardware) due to interrupt watchdog
    ESP_RST_TASK_WDT,   //!< Reset due to task watchdog
    ESP_RST_WDT,        //!< Reset due to other watchdogs
    ESP_RST_DEEPSLEEP,  //!< Reset after exiting deep sleep mode
    ESP_RST_BROWNOUT,   //!< Brownout reset (software or hardware)
    ESP_RST_SDIO,       //!< Reset over SDIO
} esp_reset_reason_t;

esp_reset_reason_t esp_reset_reason(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP_SYSTEM_H */
