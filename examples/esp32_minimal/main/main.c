// SPDX-License-Identifier: Apache-2.0
/**
 * @file main.c
 * @brief ESP32 Minimal test project — verifies PAL compilation.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

void app_main(void) {
    ESP_LOGI("MINIMAL", "ESP32 Minimal Test - PAL Compilation OK");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
