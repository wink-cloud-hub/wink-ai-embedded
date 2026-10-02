/* SPDX-License-Identifier: Apache-2.0 */
#ifndef CORPUS_HELLO_WORLD_SDKCONFIG_H_
#define CORPUS_HELLO_WORLD_SDKCONFIG_H_

#include "sdkconfig_base.h"

#ifndef CONFIG_IDF_TARGET
#define CONFIG_IDF_TARGET "esp32"
#endif

#if defined(__EMSCRIPTEN__)
#include <stdio.h>
#include <stdarg.h>
#include "hal/pal_uart.h"

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
#endif

#endif /* CORPUS_HELLO_WORLD_SDKCONFIG_H_ */
