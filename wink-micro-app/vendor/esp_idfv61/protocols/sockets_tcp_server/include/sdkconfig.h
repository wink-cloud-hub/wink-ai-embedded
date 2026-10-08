/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include "sdkconfig_base.h"

#define CONFIG_EXAMPLE_IPV4                 1
#define CONFIG_EXAMPLE_PORT                 3333
#define CONFIG_EXAMPLE_KEEPALIVE_IDLE       5
#define CONFIG_EXAMPLE_KEEPALIVE_INTERVAL   5
#define CONFIG_EXAMPLE_KEEPALIVE_COUNT      3
#define CONFIG_EXAMPLE_CONNECT_WIFI         1

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
        size_t write_len = ((size_t)n < sizeof(buf) - 1) ? (size_t)n : (sizeof(buf) - 1);
        (void)pal_uart_write(0, (const uint8_t *)buf, (uint32_t)write_len);
    }
    return n;
}

static inline void __attribute__((constructor)) esp_sim_init_uart_log_bridge(void) {
    esp_log_set_vprintf(esp_sim_console_vprintf);
}
#endif
