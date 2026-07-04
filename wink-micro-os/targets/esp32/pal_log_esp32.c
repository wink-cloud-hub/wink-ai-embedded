/**
 * @file pal_log_esp32.c
 * @brief ESP32 分级日志后端：路由到 esp_log_writev（复用 IDF 颜色/时间戳/tag 过滤）。
 *
 * 设计选择：独立 TU 而非合并到 pal_hal_esp32.c，因为 pal_hal_esp32.c 刻意保持
 * "无 IDF 私有头依赖"的同源可编译不变量，pal_log_vprintf 需要 include esp_log.h。
 */
#include "pal_log.h"

#if defined(ESP_PLATFORM)
#include "esp_log.h"

/* ESP_LOG* 宏的底层：esp_log_writev(level, tag, fmt, va_list)。
 * 数值级别与 ESP-IDF 的 esp_log_level_t 一一对应（ERROR=1, WARN=2, INFO=3, DEBUG=4），
 * 见 pal_log_level_t 定义，可直接强转无映射开销。 */
void pal_log_vprintf(pal_log_level_t level, const char *tag,
                     const char *fmt, va_list ap)
{
    esp_log_writev((esp_log_level_t)level, tag, fmt, ap);
}

#else
/* 非 ESP-IDF 环境（静态分析/CTags）：提供 stub 避免链接错误。
 * 此路径在生产构建永远不会命中（只有 ESP-IDF CMake 才编译本文件）。 */
void pal_log_vprintf(pal_log_level_t level, const char *tag,
                     const char *fmt, va_list ap)
{
    (void)level; (void)tag; (void)fmt; (void)ap;
}
#endif
