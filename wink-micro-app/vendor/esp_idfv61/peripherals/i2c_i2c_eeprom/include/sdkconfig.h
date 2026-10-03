/* SPDX-License-Identifier: Apache-2.0 */
#ifndef CORPUS_I2C_EEPROM_SDKCONFIG_H_
#define CORPUS_I2C_EEPROM_SDKCONFIG_H_

#include "sdkconfig_base.h"

#define CONFIG_I2C_MASTER_SCL 22
#define CONFIG_I2C_MASTER_SDA 21
#define CONFIG_I2C_MASTER_FREQUENCY 400000

#ifndef CONFIG_IDF_TARGET
#define CONFIG_IDF_TARGET "esp32"
#endif

#ifndef CONFIG_IDF_TARGET_ESP32
#define CONFIG_IDF_TARGET_ESP32 1
#endif

#if defined(__EMSCRIPTEN__)
#include <stdio.h>
#include <stdarg.h>
#include "hal/pal_uart.h"
#include "esp_log.h"

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

static inline int esp_sim_console_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = esp_sim_console_vprintf(fmt, ap);
    va_end(ap);
    return n;
}

static inline void __attribute__((constructor)) esp_sim_init_uart_log_bridge(void) {
    esp_log_set_vprintf(esp_sim_console_vprintf);
}

#undef printf
#define printf(...) esp_sim_console_printf(__VA_ARGS__)
#endif

#endif /* CORPUS_I2C_EEPROM_SDKCONFIG_H_ */
