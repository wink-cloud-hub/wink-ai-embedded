/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef SDKCONFIG_BASE_H_
#define SDKCONFIG_BASE_H_

/* CONFIG_IDF_TARGET_<SOC> / CONFIG_IDF_TARGET 由 CMake 注入（见 esp_idf_target.cmake，
   源自 WINK_ESP_TARGET），禁止在此硬编码，否则多 SoC 构建会宏污染（ADR-0085 D3）。 */

#define CONFIG_FREERTOS_HZ 100
#define CONFIG_LOG_DEFAULT_LEVEL 3
#define CONFIG_LOG_MAXIMUM_LEVEL 5
#define CONFIG_LOG_COLORS 1
#define CONFIG_LOG_VERSION 1
#define CONFIG_LOG_TIMESTAMP_SOURCE_RTOS 1

/* 注意：严格禁止在此处定义 ESP_PLATFORM，避免污染底层真机分支宏 */

#endif /* SDKCONFIG_BASE_H_ */
