/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CORPUS_ADC_CONTINUOUS_READ_SDKCONFIG_H_
#define CORPUS_ADC_CONTINUOUS_READ_SDKCONFIG_H_

#include "sdkconfig_base.h"

#define CONFIG_IDF_TARGET_ESP32 1

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
        /* Hardware-level console bridge: ESP32 defaults stdout & logging to UART0 */
        (void)pal_uart_write(0, (const uint8_t *)buf, (uint32_t)n);
    }
    return n;
}

static inline void __attribute__((constructor)) esp_sim_init_uart_log_bridge(void) {
    esp_log_set_vprintf(esp_sim_console_vprintf);
}
#endif

#endif /* CORPUS_ADC_CONTINUOUS_READ_SDKCONFIG_H_ */
