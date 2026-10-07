/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#define CONFIG_IDF_TARGET_ESP32 1
#define CONFIG_IDF_TARGET "esp32"

/* Deep sleep wakeup configuration */
#define CONFIG_EXAMPLE_GPIO_WAKEUP 1
#define CONFIG_EXAMPLE_GPIO_WAKEUP_PIN 2
#define CONFIG_EXAMPLE_GPIO_WAKEUP_MODE 1

#define CONFIG_EXAMPLE_EXT0_WAKEUP 1
#define CONFIG_EXAMPLE_EXT1_WAKEUP 0

#define CONFIG_LIBC_TIME_SYSCALL_USE_RTC_HRT 1
#define CONFIG_RTC_CLK_SRC_INT_RC 1
#define CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP 1

#define CONFIG_PARTITION_TABLE_CUSTOM 1

#if defined(__EMSCRIPTEN__)
#include <stdio.h>
#include <stdarg.h>
#include "hal/pal_uart.h"

int (*esp_log_set_vprintf(int (*func)(const char *, va_list)))(const char *, va_list);

static inline int esp_sim_console_vprintf(const char *fmt, va_list ap) {
    char buf[512];
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap_copy);
    va_end(ap_copy);
    if (n > 0) {
        fputs(buf, stdout);
        (void)pal_uart_write(0, (const uint8_t *)buf, (uint32_t)n);
    }
    return n;
}

static inline void __attribute__((constructor)) esp_sim_init_uart_log_bridge(void) {
    esp_log_set_vprintf(esp_sim_console_vprintf);
}

static inline int esp_sim_console_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        fputs(buf, stdout);
        (void)pal_uart_write(0, (const uint8_t *)buf, (uint32_t)n);
    }
    return n;
}

#undef printf
#define printf esp_sim_console_printf
#endif

