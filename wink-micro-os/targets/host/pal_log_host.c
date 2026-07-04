/**
 * @file pal_log_host.c
 * @brief Host target 分级日志后端：fprintf(stderr, ...) 带 ANSI 颜色。
 *
 * 颜色映射（与 ESP-IDF 默认日志配色一致，便于在终端识别级别）：
 *   ERROR — 红底白字（高亮）
 *   WARN  — 黄色
 *   INFO  — 绿色
 *   DEBUG — 普通白/灰
 *
 * 输出格式：[颜色]LEVEL tag: formatted message[reset]\n
 *
 * ⚠️ ANSI 转义在 Windows 10+ 控制台（ConHost v2 / Windows Terminal）默认可用；
 *    旧版本 Windows 可能显示转义字符乱码——host target 主要面向开发机和 CI，
 *    不做 isatty() / ENABLE_VIRTUAL_TERMINAL_PROCESSING 检测（保持代码简单）。
 */
#include "pal_log.h"
#include <stdio.h>

/* ANSI 颜色转义序列 */
#define ANSI_RESET   "\033[0m"
#define ANSI_RED_BG  "\033[41;37m"   /* ERROR: 红底白字 */
#define ANSI_YELLOW  "\033[33m"      /* WARN: 黄色 */
#define ANSI_GREEN   "\033[32m"      /* INFO: 绿色 */
#define ANSI_DIM     "\033[2m"       /* DEBUG: 暗色 */

static const char *level_prefix(pal_log_level_t level)
{
    switch (level) {
    case PAL_LOG_ERROR: return "E";
    case PAL_LOG_WARN:  return "W";
    case PAL_LOG_INFO:  return "I";
    case PAL_LOG_DEBUG: return "D";
    default:            return "?";
    }
}

static const char *level_color(pal_log_level_t level)
{
    switch (level) {
    case PAL_LOG_ERROR: return ANSI_RED_BG;
    case PAL_LOG_WARN:  return ANSI_YELLOW;
    case PAL_LOG_INFO:  return ANSI_GREEN;
    case PAL_LOG_DEBUG: return ANSI_DIM;
    default:            return ANSI_RESET;
    }
}

void pal_log_vprintf(pal_log_level_t level, const char *tag,
                     const char *fmt, va_list ap)
{
    fprintf(stderr, "%s%s (%s): ", level_color(level), level_prefix(level), tag);
    vfprintf(stderr, fmt, ap);
    fputs(ANSI_RESET "\n", stderr);
    fflush(stderr);
}
