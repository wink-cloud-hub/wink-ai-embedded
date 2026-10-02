/* SPDX-License-Identifier: Apache-2.0 */
#ifndef CORPUS_SYSTEM_ESP_TIMER_SDKCONFIG_H_
#define CORPUS_SYSTEM_ESP_TIMER_SDKCONFIG_H_

#include "sdkconfig_base.h"

#ifndef CONFIG_IDF_TARGET
#define CONFIG_IDF_TARGET "esp32"
#endif

#ifndef CONFIG_FREERTOS_NUMBER_OF_CORES
#define CONFIG_FREERTOS_NUMBER_OF_CORES 1
#endif

/* Disable light sleep RTC hardware check in simulation */
#ifndef SOC_LIGHT_SLEEP_SUPPORTED
#define SOC_LIGHT_SLEEP_SUPPORTED 0
#endif

#if defined(__EMSCRIPTEN__)
#define __asm__
#define __volatile__(x)
#include <stdio.h>
#include <stdarg.h>
#include "hal/pal_uart.h"
#include "pal_log.h"

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
#define printf(...) esp_sim_console_printf(__VA_ARGS__)
#endif

#endif /* CORPUS_SYSTEM_ESP_TIMER_SDKCONFIG_H_ */
