// SPDX-License-Identifier: Apache-2.0
/**
 * @file app_main.c
 * @brief ESP32 hardware entry point: initializes FreeRTOS tasks and starts wink runtime.
 */
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "wink_runtime.h"
#include "wink_status.h"
#include "wink_trace.h"
#include "pal_osal.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

#define WINK_TASK_STACK_SIZE    8192
#define WINK_TASK_PRIORITY      5
#define WINK_TICK_PERIOD_MS     10

static TaskHandle_t s_wink_task_handle = NULL;

/**
 * @brief Wink Runtime Main Task
 */
static void wink_runtime_task(void *arg) {
    (void)arg;

    const wink_app_callbacks_t *app = wink_app_get_callbacks();

    printf("Wink-Micro-OS ESP32 Runtime started\n");
    printf("  Reset reason: %d\n", (int)pal_os_get_reset_reason());

    wink_status_t rs = wink_runtime_run(app, 0);  /* Runs forever */
    printf("Runtime exited (status=%d) — unexpected\n", (int)rs);
    vTaskDelete(NULL);
}

/**
 * @brief ESP-IDF Application Entry Point
 */
void app_main(void) {
    printf("=== Wink-Micro-OS ESP32 Firmware ===\n");

    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    BaseType_t ret = xTaskCreate(
        wink_runtime_task,
        "wink_runtime",
        WINK_TASK_STACK_SIZE,
        NULL,
        WINK_TASK_PRIORITY,
        &s_wink_task_handle
    );

    if (ret != pdPASS) {
        printf("Failed to create wink_runtime_task!\n");
        return;
    }

    printf("Runtime task created (stack=%u bytes, handle=%p)\n",
           (unsigned)WINK_TASK_STACK_SIZE, (void*)s_wink_task_handle);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        UBaseType_t stack_free_words = uxTaskGetStackHighWaterMark(s_wink_task_handle);
        uint32_t stack_free_bytes = stack_free_words * sizeof(StackType_t);
        uint32_t stack_used_bytes = WINK_TASK_STACK_SIZE - stack_free_bytes;

        uint32_t heap_free_now = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        static uint32_t heap_free_base = 0;
        static bool baseline_set = false;
        if (!baseline_set) {
            heap_free_base = heap_free_now;
            baseline_set = true;
            printf("Heap baseline (post-init): %" PRIu32 " bytes\n", heap_free_base);
        }
        int32_t heap_delta = (int32_t)heap_free_now - (int32_t)heap_free_base;

        printf("Uptime: %" PRIu32 "s  Stack: used=%" PRIu32 "B free=%" PRIu32 "B  Heap: %" PRIu32 "B (delta%+" PRId32 ")  Faults: %" PRIu32 "  Warns: %" PRIu32 "\n",
               (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ),
               stack_used_bytes,
               stack_free_bytes,
               heap_free_now,
               heap_delta,
               wink_trace_count(),
               wink_warn_count());

        if (stack_free_bytes < 1024) {
            printf("WARNING: Stack dangerously low! free=%" PRIu32 "B < 1024B\n", stack_free_bytes);
        }

        if (heap_delta < -2048) {
            printf("WARNING: Possible heap leak! delta=%+" PRId32 "B\n", heap_delta);
        }
    }
}
