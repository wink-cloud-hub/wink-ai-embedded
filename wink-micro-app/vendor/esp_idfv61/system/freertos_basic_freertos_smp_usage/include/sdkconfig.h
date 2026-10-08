/* SPDX-License-Identifier: Apache-2.0 */
#ifndef CORPUS_SYSTEM_FREERTOS_BASIC_FREERTOS_SMP_USAGE_SDKCONFIG_H_
#define CORPUS_SYSTEM_FREERTOS_BASIC_FREERTOS_SMP_USAGE_SDKCONFIG_H_

#include "sdkconfig_base.h"

#ifndef CONFIG_IDF_TARGET
#define CONFIG_IDF_TARGET "esp32"
#endif

#ifndef CONFIG_FREERTOS_NUMBER_OF_CORES
#define CONFIG_FREERTOS_NUMBER_OF_CORES 2
#endif

#ifndef CONFIG_ESP_CONSOLE_UART_DEFAULT
#define CONFIG_ESP_CONSOLE_UART_DEFAULT 1
#endif

#ifndef CONFIG_ESP_CONSOLE_UART_NUM
#define CONFIG_ESP_CONSOLE_UART_NUM 0
#endif

#if defined(__EMSCRIPTEN__)
#define __asm__
#define __volatile__(x)
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
        /* Hardware-level console bridge: ESP32 defaults stdout & logging to UART0 */
        size_t write_len = ((size_t)n < sizeof(buf) - 1) ? (size_t)n : (sizeof(buf) - 1);
        (void)pal_uart_write(0, (const uint8_t *)buf, (uint32_t)write_len);
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
        size_t write_len = ((size_t)n < sizeof(buf) - 1) ? (size_t)n : (sizeof(buf) - 1);
        (void)pal_uart_write(0, (const uint8_t *)buf, (uint32_t)write_len);
    }
    return n;
}

#undef printf
#define printf esp_sim_console_printf
#endif

#endif /* CORPUS_SYSTEM_FREERTOS_BASIC_FREERTOS_SMP_USAGE_SDKCONFIG_H_ */
