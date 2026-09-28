/* SPDX-License-Identifier: Apache-2.0 */
#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

static bool queue_reuse_rejects_stale_handle(void) {
    uint32_t value = 0x12345678u;
    QueueHandle_t old = xQueueCreate(1, sizeof(value));
    if (!old) return false;
    vQueueDelete(old);
    QueueHandle_t current = xQueueCreate(1, sizeof(value));
    if (!current) return false;
    bool passed = old != current &&
                  xQueueSend(old, &value, 0) == errQUEUE_FULL &&
                  uxQueueMessagesWaiting(current) == 0 &&
                  xQueueSend(current, &value, 0) == pdPASS;
    vQueueDelete(current);
    return passed;
}

static bool semaphore_reuse_rejects_stale_handle(void) {
    SemaphoreHandle_t old = xSemaphoreCreateBinary();
    if (!old) return false;
    vSemaphoreDelete(old);
    SemaphoreHandle_t current = xSemaphoreCreateBinary();
    if (!current) return false;
    bool passed = old != current && xSemaphoreGive(old) == pdFALSE &&
                  uxSemaphoreGetCount(current) == 0 &&
                  xSemaphoreGive(current) == pdTRUE;
    vSemaphoreDelete(current);
    return passed;
}

static bool event_reuse_rejects_stale_handle(void) {
    EventGroupHandle_t old = xEventGroupCreate();
    if (!old) return false;
    vEventGroupDelete(old);
    EventGroupHandle_t current = xEventGroupCreate();
    if (!current) return false;
    bool passed = old != current && xEventGroupSetBits(old, 1u) == 0 &&
                  xEventGroupGetBits(current) == 0 &&
                  xEventGroupSetBits(current, 1u) == 1u;
    vEventGroupDelete(current);
    return passed;
}

static bool nvs_reuse_rejects_stale_handle(void) {
    if (nvs_flash_init() != ESP_OK) return false;
    nvs_handle_t old = 0;
    if (nvs_open("old", NVS_READWRITE, &old) != ESP_OK) return false;
    nvs_close(old);
    nvs_handle_t current = 0;
    if (nvs_open("current", NVS_READWRITE, &current) != ESP_OK) return false;
    bool passed = old != current &&
                  nvs_set_u32(old, "value", 1u) == ESP_ERR_INVALID_ARG;
    nvs_close(old);
    passed = passed && nvs_set_u32(current, "value", 2u) == ESP_OK &&
             nvs_commit(old) == ESP_ERR_INVALID_ARG;
    nvs_close(current);
    return passed;
}

void app_main(void) {
    const gpio_num_t pins[] = {GPIO_NUM_2, GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_18};
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        gpio_reset_pin(pins[i]);
        gpio_set_direction(pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(pins[i], 0);
    }
    if (queue_reuse_rejects_stale_handle()) gpio_set_level(GPIO_NUM_2, 1);
    if (semaphore_reuse_rejects_stale_handle()) gpio_set_level(GPIO_NUM_4, 1);
    if (event_reuse_rejects_stale_handle()) gpio_set_level(GPIO_NUM_5, 1);
    if (nvs_reuse_rejects_stale_handle()) gpio_set_level(GPIO_NUM_18, 1);
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
