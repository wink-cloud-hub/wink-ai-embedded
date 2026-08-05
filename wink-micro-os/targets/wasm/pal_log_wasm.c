// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_log_wasm.c
 * @brief Wasm target tiered logging backend implementation.
 */
#include "pal_log.h"
#include "pal_osal.h"
#include "wasm_bridge.h"

#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#define PAL_LOG_BUF_SIZE 384

static const char *level_letter(pal_log_level_t level)
{
    switch (level) {
    case PAL_LOG_ERROR: return "E";
    case PAL_LOG_WARN:  return "W";
    case PAL_LOG_INFO:  return "I";
    case PAL_LOG_DEBUG: return "D";
    default:            return "?";
    }
}

void pal_log_vprintf(pal_log_level_t level, const char *tag,
                     const char *fmt, va_list ap)
{
    char buf[PAL_LOG_BUF_SIZE];
    const char *tag_safe = tag ? tag : "?";
    uint64_t ms = pal_os_get_ms();

    int prefix_len = snprintf(buf, sizeof(buf), "[%10" PRIu64 " ms] [%s] [%s] ",
                              ms, level_letter(level), tag_safe);
    if (prefix_len < 0) { prefix_len = 0; }
    if ((size_t)prefix_len < sizeof(buf) - 1) {
        vsnprintf(buf + prefix_len, sizeof(buf) - (size_t)prefix_len, fmt, ap);
    }
    buf[sizeof(buf) - 1] = '\0';
    js_pal_log((uint8_t)level, buf);
}

bool pal_log_in_isr(void)
{
    return pal_os_in_sim_isr_context();
}

void pal_log_isr_write(pal_log_level_t level, const char *tag,
                       const char *fmt, va_list ap)
{
    char buf[PAL_LOG_BUF_SIZE];
    const char *tag_safe = tag ? tag : "?";
    uint64_t ms = pal_os_get_ms();
    int prefix_len = snprintf(buf, sizeof(buf), "!ISR! [%10" PRIu64 " ms] [%s] [%s] ",
                              ms, level_letter(level), tag_safe);
    if (prefix_len < 0) { prefix_len = 0; }
    if ((size_t)prefix_len < sizeof(buf) - 1) {
        vsnprintf(buf + prefix_len, sizeof(buf) - (size_t)prefix_len, fmt, ap);
    }
    buf[sizeof(buf) - 1] = '\0';
    js_pal_log((uint8_t)level, buf);
}
