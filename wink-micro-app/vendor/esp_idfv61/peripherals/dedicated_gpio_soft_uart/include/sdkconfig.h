/* App-level Kconfig overlay for the upstream dedicated_gpio/soft_uart example (vendor suite). */
#ifndef CORPUS_PERIPHERALS_DEDICATED_GPIO_SOFT_UART_SDKCONFIG_H_
#define CORPUS_PERIPHERALS_DEDICATED_GPIO_SOFT_UART_SDKCONFIG_H_

#include "sdkconfig_base.h"

#ifndef CONFIG_IDF_TARGET
#define CONFIG_IDF_TARGET "esp32"
#endif

#ifndef CONFIG_IDF_TARGET_ESP32
#define CONFIG_IDF_TARGET_ESP32 1
#endif

#ifndef CONFIG_EMULATE_UART_GPIO_TX
#define CONFIG_EMULATE_UART_GPIO_TX 16
#endif

#ifndef CONFIG_EMULATE_UART_GPIO_RX
#define CONFIG_EMULATE_UART_GPIO_RX 17
#endif

#ifndef CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ
#define CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ 160
#endif

#ifndef CONFIG_SOC_DEDICATED_GPIO_SUPPORTED
#define CONFIG_SOC_DEDICATED_GPIO_SUPPORTED 1
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
        /* Hardware-level console bridge: ESP32 defaults stdout & logging to UART0 */
        size_t write_len = ((size_t)n < sizeof(buf) - 1) ? (size_t)n : (sizeof(buf) - 1);
        (void)pal_uart_write(0, (const uint8_t *)buf, (uint32_t)write_len);
    }
    return n;
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
#define printf(...) esp_sim_console_printf(__VA_ARGS__)

static inline void __attribute__((constructor)) esp_sim_init_uart_log_bridge(void) {
    esp_log_set_vprintf(esp_sim_console_vprintf);
}
#endif

#endif /* CORPUS_PERIPHERALS_DEDICATED_GPIO_SOFT_UART_SDKCONFIG_H_ */
