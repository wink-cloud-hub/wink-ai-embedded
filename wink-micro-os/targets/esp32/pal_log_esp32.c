// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_log_esp32.c
 * @brief ESP32 target tiered logging backend implementation.
 */
#include "pal_log.h"

#if defined(ESP_PLATFORM)
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void pal_log_vprintf(pal_log_level_t level, const char *tag,
                     const char *fmt, va_list ap)
{
    esp_log_writev((esp_log_level_t)level, tag, fmt, ap);
}

bool pal_log_in_isr(void)
{
    return xPortInIsrContext() != 0;
}

void pal_log_isr_write(pal_log_level_t level, const char *tag,
                       const char *fmt, va_list ap)
{
    static const char k_letter[] = { '?', 'E', 'W', 'I', 'D' };
    char l = (level >= PAL_LOG_ERROR && level <= PAL_LOG_DEBUG)
             ? k_letter[level] : '?';
    char msg[192];
    vsnprintf(msg, sizeof(msg), fmt, ap);
    msg[sizeof(msg) - 1] = '\0';
    esp_rom_printf("!ISR! %c [%s] %s\n", l, tag ? tag : "?", msg);
}

#else
void pal_log_vprintf(pal_log_level_t level, const char *tag,
                     const char *fmt, va_list ap)
{
    (void)level; (void)tag; (void)fmt; (void)ap;
}

bool pal_log_in_isr(void)
{
    return false;
}

void pal_log_isr_write(pal_log_level_t level, const char *tag,
                       const char *fmt, va_list ap)
{
    (void)level; (void)tag; (void)fmt; (void)ap;
}
#endif
