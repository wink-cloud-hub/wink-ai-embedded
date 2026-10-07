/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_system.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_rom_gpio.h"
#include "esp_task_wdt.h"
#include "esp_intr_alloc.h"
#include "esp_heap_caps.h"
#include "pal_osal.h"
#include "wink_sim_scheduler.h"  /* sim_scheduler_yield_context */
#include "freertos_sync.h"        /* esp_sim_spin_wait_account (ISSUE-06) */
#include <string.h>
extern void esp_idf_ensure_framework_ready(void); /* Phase 2 ISSUE-13 */

/* Deterministic xorshift32 for simulation replayability */
static uint32_t s_random_state = 2463534242UL;

uint32_t esp_random(void) {
    uint32_t x = s_random_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_random_state = x;
    return x;
}

void esp_fill_random(void *buf, size_t len) {
    if (!buf || len == 0) {
        return;
    }
    uint8_t *p = (uint8_t *)buf;
    while (len >= 4) {
        uint32_t r = esp_random();
        memcpy(p, &r, 4);
        p += 4;
        len -= 4;
    }
    if (len > 0) {
        uint32_t r = esp_random();
        memcpy(p, &r, len);
    }
}

void esp_chip_info(esp_chip_info_t *out_info) {
    if (!out_info) {
        return;
    }
    out_info->model = CHIP_ESP32;
    out_info->features = CHIP_FEATURE_WIFI_BGN | CHIP_FEATURE_BT | CHIP_FEATURE_BLE;
    out_info->cores = 2;
    out_info->revision = 3;
}

int64_t esp_timer_get_time(void) {
    /* Phase 2 Task 4.3 + 3.2: order MUST be cold-start first, then spin-account.
     * sim_scheduler_current_id() is undefined before the scheduler is ready;
     * ensure_framework_ready() guarantees it is valid before spin accounting. */
    esp_idf_ensure_framework_ready();
    esp_sim_spin_wait_account();
    return (int64_t)pal_os_get_us();
}

void esp_rom_delay_us(uint32_t us) {
    /* Phase 2 Task 3.2 M2: tiered busy-wait strategy (ISSUE-06).
     *  us <= 100  : advance virtual clock only — preserve µs timing precision.
     *  100 < us <= 5000 : advance + count as spin (auto-yield at threshold).
     *  us > 5000  : advance + immediate forced yield (prevent browser freeze). */
    pal_os_busy_wait_us(us);
    if (us > 5000U) {
        if (sim_scheduler_current_ctx() != NULL) {
            sim_scheduler_yield_context();   /* >5ms: force yield unconditionally */
        }
    } else if (us > 100U) {
        esp_sim_spin_wait_account();     /* mid-range: contribute to spin count */
    }
    /* us <= 100: only clock advance, no yield trigger */
}

void esp_rom_gpio_pad_select_gpio(uint32_t gpio_num) {
    /* void ROM C-ABI: cannot signal failure; warn loudly (降级条目 5). */
    (void)gpio_num;
    ESP_LOGW("ESP_SYS", "esp_rom_gpio_pad_select_gpio: no-op in simulation (M0)");
}

void esp_rom_gpio_connect_out_signal(uint32_t gpio_num, uint32_t signal_idx, bool out_inv, bool oen_inv) {
    (void)gpio_num;
    (void)signal_idx;
    (void)out_inv;
    (void)oen_inv;
}

void esp_rom_gpio_connect_in_signal(uint32_t gpio_num, uint32_t signal_idx, bool inv) {
    (void)gpio_num;
    (void)signal_idx;
    (void)inv;
}

uint32_t esp_get_free_heap_size(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
}

uint32_t esp_get_minimum_free_heap_size(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
}

esp_err_t esp_flash_get_size(esp_flash_t *chip, uint32_t *out_size) {
    (void)chip;
    if (!out_size) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Standard ESP32 DevKit 4MB external SPI flash */
    *out_size = 4 * 1024 * 1024;
    return ESP_OK;
}

/* Interrupt allocator: NOT supported in M0 simulation
 * (ADR-0012 降级条目 5). Fail-loud with ESP_ERR_NOT_SUPPORTED. */

esp_err_t esp_intr_alloc(int source, int flags, intr_handler_t handler, void *arg, intr_handle_t *ret_handle) {
    (void)source;
    (void)flags;
    (void)handler;
    (void)arg;
    if (ret_handle) {
        *ret_handle = (intr_handle_t)0;
    }
    ESP_LOGE("ESP_SYS", "esp_intr_alloc: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_intr_free(intr_handle_t handle) {
    (void)handle;
    ESP_LOGE("ESP_SYS", "esp_intr_free: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

int usleep(useconds_t us) {
    if (us == 0) {
        return 0;
    }
    TickType_t ticks = (TickType_t)(us / (portTICK_PERIOD_MS * 1000ULL));
    if (ticks == 0) {
        ticks = 1;
    }
    vTaskDelay(ticks);
    return 0;
}

unsigned int sleep(unsigned int seconds) {
    if (seconds == 0) {
        return 0;
    }
    TickType_t ticks = (TickType_t)(seconds * configTICK_RATE_HZ);
    if (ticks == 0) {
        ticks = 1;
    }
    vTaskDelay(ticks);
    return 0;
}

