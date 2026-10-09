/* SPDX-License-Identifier: LGPL-3.0-only */
/* Wink 门面扩展声明（手写通道 A）：
 * 原厂头文件不包含的仿真门面私有 API（错误码桥接 + 各驱动重置钩子）。
 * `include/` 的其余头文件由闭源 SDK Harvester 生成，禁止手工修改；
 * 本文件是唯一的手写扩展入口，供 `src/` 与 `test/` 引用。 */
#ifndef ESP_IDF_WINK_H_
#define ESP_IDF_WINK_H_

#if !defined(SIMULATION) && !defined(WINK_SIM_TEST)
#error "FATAL: This header is a WinkMicroOS simulation-only header! It cannot be included in physical ESP-IDF hardware builds."
#endif

#include "esp_err.h"
#include "wink_status.h"
#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 错误码桥接（ADR-0001 负数错误码 <-> ESP 0x101+ 体系） */
esp_err_t wink_status_to_esp_err(wink_status_t status);
wink_status_t esp_err_to_wink_status(esp_err_t err);
esp_err_t esp_err_from_wink(wink_status_t status);
wink_status_t wink_status_from_esp(esp_err_t err);

/* 复位钩子：清空各门面静态资源池（测试 setUp / 软复位复用） */
void esp_freertos_pools_reset(void);
void esp_peripherals_reset(void);
void esp_gpio_reset(void);
void esp_heap_caps_reset(void);
void esp_ledc_reset(void);
void esp_gptimer_reset(void);
void esp_i2c_legacy_reset(void);
void esp_i2c_master_reset(void);
void esp_uart_reset(void);
void esp_spi_reset(void);

/* Phase 3 GPIO ISR simulation edge injection (ISSUE-04) */
esp_err_t esp_sim_gpio_inject_edge(gpio_num_t pin, uint32_t from_level, uint32_t to_level);
void esp_sim_gpio_set_input_level(gpio_num_t pin, uint32_t level);

/* Phase 3 NVS simulation memory cache reset (for testing cold reload from sandbox file) */
void esp_sim_nvs_reset_memory(void);

/* Test Harness Observation Hooks (Dual Assertion Model: DoD-3) */
size_t esp_heap_caps_get_active_allocations(void);
uint32_t esp_freertos_get_active_task_count(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP_IDF_WINK_H_ */
