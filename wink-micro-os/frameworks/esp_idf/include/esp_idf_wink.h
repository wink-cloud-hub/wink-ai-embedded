/* SPDX-License-Identifier: LGPL-3.0-only */
/* Wink 门面扩展声明（手写通道 A）：
 * 原厂头文件不包含的仿真门面私有 API（错误码桥接 + 各驱动重置钩子）。
 * `include/` 的其余头文件由闭源 SDK Harvester 生成，禁止手工修改；
 * 本文件是唯一的手写扩展入口，供 `src/` 与 `test/` 引用。 */
#ifndef ESP_IDF_WINK_H_
#define ESP_IDF_WINK_H_

#include "esp_err.h"
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 错误码桥接（ADR-0001 负数错误码 <-> ESP 0x101+ 体系） */
esp_err_t esp_err_from_wink(wink_status_t status);
wink_status_t wink_status_from_esp(esp_err_t err);

/* 复位钩子：清空各门面静态资源池（测试 setUp / 软复位复用） */
void esp_freertos_pools_reset(void);
void esp_peripherals_reset(void);
void esp_ledc_reset(void);
void esp_gptimer_reset(void);
void esp_i2c_legacy_reset(void);
void esp_i2c_master_reset(void);
void esp_uart_reset(void);
void esp_spi_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP_IDF_WINK_H_ */
