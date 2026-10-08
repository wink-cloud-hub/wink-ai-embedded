/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include "sdkconfig_base.h"

#define CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL "http://192.168.1.100:8070/firmware.bin"
#define CONFIG_EXAMPLE_USE_CERT_BUNDLE      1
#define CONFIG_EXAMPLE_CONNECT_WIFI         1
#define CONFIG_BOOTLOADER_OFFSET_IN_FLASH   0x1000
#define CONFIG_PARTITION_TABLE_OFFSET       0x8000

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
        size_t write_len = ((size_t)n < sizeof(buf) - 1) ? (size_t)n : (sizeof(buf) - 1);
        (void)pal_uart_write(0, (const uint8_t *)buf, (uint32_t)write_len);
    }
    return n;
}

static inline void __attribute__((constructor)) esp_sim_init_uart_log_bridge(void) {
    esp_log_set_vprintf(esp_sim_console_vprintf);
}
#endif
