/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_system.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_chip_info.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_rom_gpio.h"
#include "esp_task_wdt.h"
#include "esp_intr_alloc.h"
#include "pal_osal.h"
#include <string.h>

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
    return (int64_t)pal_os_get_us();
}

void esp_rom_delay_us(uint32_t us) {
    pal_os_busy_wait_us(us);
}

void esp_rom_gpio_pad_select_gpio(uint32_t gpio_num) {
    /* void ROM C-ABI: cannot signal failure; warn loudly (降级条目 5). */
    (void)gpio_num;
    ESP_LOGW("ESP_SYS", "esp_rom_gpio_pad_select_gpio: no-op in simulation (M0)");
}

/* Task watchdog / interrupt allocator: NOT supported in M0 simulation
 * (ADR-0012 降级条目 5). Fail-loud with ESP_ERR_NOT_SUPPORTED. */

esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t *config) {
    (void)config;
    ESP_LOGE("ESP_SYS", "esp_task_wdt_init: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_task_wdt_deinit(void) {
    ESP_LOGE("ESP_SYS", "esp_task_wdt_deinit: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_task_wdt_add(TaskHandle_t handle) {
    (void)handle;
    ESP_LOGE("ESP_SYS", "esp_task_wdt_add: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_task_wdt_reset(void) {
    ESP_LOGE("ESP_SYS", "esp_task_wdt_reset: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_task_wdt_delete(TaskHandle_t handle) {
    (void)handle;
    ESP_LOGE("ESP_SYS", "esp_task_wdt_delete: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

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
