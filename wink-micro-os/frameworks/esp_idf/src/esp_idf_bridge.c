/* SPDX-License-Identifier: LGPL-3.0-only */
#include <stdbool.h>
#include "pal_log.h"
#include "esp_system.h"
#include "esp_idf_wink.h"
#include "freertos_sync.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "driver/gptimer.h"
#include "driver/spi_master.h"
#include "nvs_flash.h"

static bool s_esp_pending_reset = false;
static int s_esp_reset_reason = 4; /* SOFTWARE */

void esp_freertos_pools_reset(void) {
    esp_freertos_task_pool_reset();
    esp_freertos_queue_pool_reset();
    esp_freertos_sem_pool_reset();
    esp_freertos_event_pool_reset();
}

void esp_peripherals_reset(void) {
    esp_ledc_reset();
    esp_i2c_legacy_reset();
    esp_i2c_master_reset();
    esp_uart_reset();
    esp_gptimer_reset();
    esp_spi_reset();
    nvs_flash_deinit();
}

void esp_restart(void) {
    pal_log_w("ESP_SYS", "esp_restart requested -> pending reset flag set");
    s_esp_pending_reset = true;
    /* Simulate noreturn: yield CPU so main loop can process the pending reset.
     * Guard with scheduler check in case called before scheduler starts. */
    if (sim_scheduler_current_id() != SIM_SCHED_NO_READY) {
        sim_scheduler_yield_context();
        for (;;) { sim_scheduler_yield_context(); } /* noreturn guard */
    }
}

/* 供 targets/wasm 弱钩子查询的导出（命名不得带 wink_mcs51 前缀冲突） */
bool pal_wasm_target_has_pending_reset(void) { return s_esp_pending_reset; }
int pal_wasm_target_get_reset_reason(void) { return s_esp_reset_reason; }
void pal_wasm_target_clear_pending_reset(void) {
    s_esp_pending_reset = false;
    esp_freertos_pools_reset();
    esp_peripherals_reset();
}

esp_reset_reason_t esp_reset_reason(void) {
    return ESP_RST_SW;
}

const char *esp_get_idf_version(void) {
    return "v6.1-dev-winksim";
}
