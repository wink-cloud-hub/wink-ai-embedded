/* SPDX-License-Identifier: Apache-2.0 */
#ifndef CORPUS_GPIO_GENERIC_GPIO_SDKCONFIG_H_
#define CORPUS_GPIO_GENERIC_GPIO_SDKCONFIG_H_

#include "sdkconfig_base.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t esp_get_minimum_free_heap_size(void);
#ifdef __cplusplus
}
#endif

#ifndef CONFIG_IDF_TARGET
#define CONFIG_IDF_TARGET "esp32"
#endif

#ifndef CONFIG_IDF_TARGET_ESP32
#define CONFIG_IDF_TARGET_ESP32 1
#endif

#ifndef CONFIG_GPIO_OUTPUT_0
#define CONFIG_GPIO_OUTPUT_0 18
#endif

#ifndef CONFIG_GPIO_OUTPUT_1
#define CONFIG_GPIO_OUTPUT_1 19
#endif

#ifndef CONFIG_GPIO_INPUT_0
#define CONFIG_GPIO_INPUT_0 4
#endif

#ifndef CONFIG_GPIO_INPUT_1
#define CONFIG_GPIO_INPUT_1 5
#endif

#if defined(__EMSCRIPTEN__)
#include <stdio.h>
#include <stdarg.h>
#include "hal/pal_uart.h"
#include "esp_log.h"

static inline int esp_sim_console_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        /* Standard stdout */
        fputs(buf, stdout);
        /* Hardware-level console bridge: ESP32 defaults stdout/printf to UART0 */
        (void)pal_uart_write(0, (const uint8_t *)buf, (uint32_t)n);
    }
    return n;
}

#undef printf
#define printf(...) esp_sim_console_printf(__VA_ARGS__)

static inline int esp_sim_console_vprintf(const char *fmt, va_list ap) {
    char buf[512];
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap_copy);
    va_end(ap_copy);
    if (n > 0) {
        fputs(buf, stdout);
        /* Hardware-level console bridge: ESP32 defaults stdout & logging to UART0 */
        (void)pal_uart_write(0, (const uint8_t *)buf, (uint32_t)n);
    }
    return n;
}

static inline void __attribute__((constructor)) esp_sim_init_uart_log_bridge(void) {
    esp_log_set_vprintf(esp_sim_console_vprintf);
}
#endif


#endif /* CORPUS_GPIO_GENERIC_GPIO_SDKCONFIG_H_ */
