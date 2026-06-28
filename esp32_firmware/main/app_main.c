/**
 * @file app_main.c
 * @brief ESP32 真机入口：启动 wink runtime
 *
 * 关键修复点（架构评审）：
 *   Issue #3: Runtime task 栈从 4096 → 8192 字节（防溢出）+ 水位监控
 *   Issue 增补: Heap 泄漏监控（运行5分钟内存变化 < 100字节）
 */
#include <stdio.h>
#include <inttypes.h>       /* PRIu32 / PRId32 (Xtensa uint32_t = unsigned long) */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"      /* ADR-0008：device tree 覆写 blob 的 NVS 存储依赖 */
#include "wink_runtime.h"
#include "wink_status.h"
#include "wink_trace.h"
#include "pal_osal.h"

/* 引入 samples App 回调工厂（符号由 WINK_APP 对应源文件提供） */
extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

/* 架构评审修复 #3：栈大小调优
 * ESP32 FreeRTOS 栈单位是字节，printf + runtime tick + DAL 驱动
 * 需要较大栈空间。起步 8192 字节，运行后通过水位调优。
 * 验收标准：运行 5 分钟后栈剩余 > 1024 字节
 */
#define WINK_TASK_STACK_SIZE    8192    /* Fixed: was 4096 */
#define WINK_TASK_PRIORITY      5
#define WINK_TICK_PERIOD_MS     10

static TaskHandle_t s_wink_task_handle = NULL;

/**
 * @brief Wink Runtime 主任务
 *
 * 运行频率：100Hz（每 10ms 一次 tick）
 * 功能：执行 App 回调、看门狗喂狗、故障检测
 */
static void wink_runtime_task(void *arg) {
    (void)arg;

    const wink_app_callbacks_t *app = wink_app_get_callbacks();

    printf("Wink-Micro-OS ESP32 Runtime started\n");
    printf("  Reset reason: %d\n", (int)pal_get_reset_reason());

    /* 标准入口 wdk_runtime_run(callbacks, max_ticks):
     * - 内部先执行 callbacks->init()
     * - 再无限循环 callbacks->loop() + 10ms 延时 (max_ticks==0 表示永久)
     * - ESP32 下 pal_delay_ms() 调用 vTaskDelay，满足 FreeRTOS 调度要求
     */
    wink_status_t rs = wink_runtime_run(app, 0);  /* runs forever */
    printf("Runtime exited (status=%d) — this should not happen\n", (int)rs);
    vTaskDelete(NULL);
}

/**
 * @brief ESP-IDF 应用入口（由 FreeRTOS 自动调用）
 */
void app_main(void) {
    printf("=== Wink-Micro-OS ESP32 Firmware ===\n");

    /* ADR-0008：NVS 初始化（device tree 覆写 blob 的持久存储依赖）。
     * 标准 erase-on-corrupt：NVS 无空闲页/版本不符时擦除重建。
     * 须在创建 runtime task（含 app_init → device_tree_apply_flash_config → pal_storage_read）之前。 */
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    /* 启动 Wink Runtime 任务 */
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

    /* 验收标准增补：Heap 泄漏监控基准值
     * 记录启动后（系统稳定时）的可用内存作为基准
     * 验收标准：运行 5 分钟后变化 < 100 字节
     */
    const uint32_t heap_free_base = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    printf("Heap baseline: %" PRIu32 " bytes\n", heap_free_base);

    /* app_main 任务：系统监控、日志输出 */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        /* 架构评审修复 #3：栈高水位检测
         * uxTaskGetStackHighWaterMark 返回剩余栈空间（words）
         * × sizeof(StackType_t) = 字节数
         * 验收标准：运行 5 分钟后 > 1024 字节
         */
        UBaseType_t stack_free_words = uxTaskGetStackHighWaterMark(s_wink_task_handle);
        uint32_t stack_free_bytes = stack_free_words * sizeof(StackType_t);
        uint32_t stack_used_bytes = WINK_TASK_STACK_SIZE - stack_free_bytes;

        /* 验收标准增补：Heap 泄漏检测
         * 监控可用内存变化量，超过阈值报警
         */
        uint32_t heap_free_now = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        int32_t heap_delta = (int32_t)heap_free_now - (int32_t)heap_free_base;

        printf("Uptime: %" PRIu32 "s  Stack: used=%" PRIu32 "B free=%" PRIu32 "B  Heap: %" PRIu32 "B (delta%+" PRId32 ")  Faults: %" PRIu32 "\n",
               (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ),
               stack_used_bytes,
               stack_free_bytes,
               heap_free_now,
               heap_delta,
               wink_trace_count());

        /* 栈安全门禁：剩余 < 1024 字节时报警 */
        if (stack_free_bytes < 1024) {
            printf("WARNING: Stack dangerously low! free=%" PRIu32 "B < 1024B\n", stack_free_bytes);
        }

        /* Heap 泄漏门禁：持续泄漏 > 512 字节时报警 */
        if (heap_delta < -512) {
            printf("WARNING: Possible heap leak! delta=%+" PRId32 "B\n", heap_delta);
        }
    }
}
