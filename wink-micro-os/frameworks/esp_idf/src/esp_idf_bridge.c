/* SPDX-License-Identifier: LGPL-3.0-only */
#include <stdbool.h>
#include <assert.h>
#include "pal_log.h"
#include "esp_system.h"
#include "esp_idf_wink.h"
#include "wink_sim_scheduler.h"
#include "freertos_sync.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "driver/gptimer.h"
#include "driver/spi_master.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include <stdlib.h>

/* Simulation reset hooks intentionally remain internal to the ESP-IDF facade. */
extern void esp_wifi_sim_reset(void);
extern void esp_mqtt_sim_reset(void);
extern void esp_http_client_sim_reset(void);
extern void esp_nimble_sim_reset(void);
extern void esp_event_loop_sim_reset(void);

static bool s_esp_pending_reset = false;
static int s_esp_reset_reason = 4; /* SOFTWARE */

void esp_freertos_pools_reset(void) {
    esp_freertos_task_pool_reset();
    esp_freertos_queue_pool_reset();
    esp_freertos_sem_pool_reset();
    esp_freertos_event_pool_reset();
    esp_freertos_spinlock_reset(); /* Phase 2: reset critical-depth + spin counters */
}

void esp_peripherals_reset(void) {
    esp_gpio_reset();
    esp_heap_caps_reset();
    esp_ledc_reset();
    esp_i2c_legacy_reset();
    esp_i2c_master_reset();
    esp_uart_reset();
    esp_gptimer_reset();
    esp_spi_reset();
    nvs_flash_deinit();
}

static void esp_sim_request_restart(void) {
    s_esp_pending_reset = true;
}

/* Test/host adapter for observing the reset request without violating the
 * noreturn contract of the public ESP-IDF esp_restart() API. */
void pal_wasm_target_request_reset(void) {
    pal_log_w("ESP_SYS", "simulation reset requested");
    esp_sim_request_restart();
}

void esp_restart(void) {
    pal_log_w("ESP_SYS", "esp_restart requested -> pending reset flag set");
    esp_sim_request_restart();
    /* Simulate noreturn: yield CPU so main loop can process the pending reset.
     * Guard with scheduler check in case called before scheduler starts. */
    if (sim_scheduler_current_id() != SIM_SCHED_NO_READY) {
        sim_scheduler_yield_context();
        for (;;) { sim_scheduler_yield_context(); } /* noreturn guard */
    }
    pal_log_e("ESP_SYS", "esp_restart called outside a scheduler fiber; aborting to honor noreturn");
    abort();
}

/* 供 targets/wasm 弱钩子查询的导出（命名不得带 wink_mcs51 前缀冲突） */
bool pal_wasm_target_has_pending_reset(void) { return s_esp_pending_reset; }
int pal_wasm_target_get_reset_reason(void) { return s_esp_reset_reason; }
void pal_wasm_target_clear_pending_reset(void) {
    assert(sim_scheduler_current_id() == SIM_SCHED_NO_READY &&
           "ESP-IDF soft reset must be applied at a scheduler boundary");
    s_esp_pending_reset = false;
    /* Tear down producers and queued callbacks before their dependencies. */
    esp_http_client_sim_reset();
    esp_mqtt_sim_reset();
    esp_wifi_sim_reset();
    (void)esp_netif_init();
    esp_nimble_sim_reset();
    esp_event_loop_sim_reset();
    /* Invalidate dormant fibers as well as module tokens before pool reuse. */
    sim_scheduler_reset(0);
    esp_peripherals_reset();
    esp_freertos_pools_reset();
}

esp_reset_reason_t esp_reset_reason(void) {
    return ESP_RST_SW;
}

const char *esp_get_idf_version(void) {
    return "v6.1-dev-winksim";
}
