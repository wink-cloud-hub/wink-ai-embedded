/**
 * @file pal_log_esp32.c
 * @brief ESP32 分级日志后端：路由到 esp_log_writev（复用 IDF 颜色/时间戳/tag 过滤）；
 *        同时提供 ISR 上下文探测与 ISR 无锁 ROM 通路。
 *
 * 设计选择：独立 TU 而非合并到 pal_hal_esp32.c，因为 pal_hal_esp32.c 刻意保持
 * "无 IDF 私有头依赖"的同源可编译不变量，pal_log_vprintf 需要 include esp_log.h。
 *
 * ISR 路径：
 *   - pal_log_in_isr() 封装 xPortInIsrContext()。
 *   - 在 ISR 中，ERROR/WARN 级别走 pal_log_isr_write() → esp_rom_printf（ROM 代码，
 *     无互斥锁、无堆分配），INFO/DEBUG 被静默丢弃（避免高频中断打挂系统）。
 */
#include "pal_log.h"

#if defined(ESP_PLATFORM)
#include "esp_log.h"
#include "esp_rom_sys.h"   /* esp_rom_printf */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" /* xPortInIsrContext */

/* ESP_LOG* 宏的底层：esp_log_writev(level, tag, fmt, va_list)。
 * 数值级别与 ESP-IDF 的 esp_log_level_t 一一对应（ERROR=1, WARN=2, INFO=3, DEBUG=4），
 * 见 pal_log_level_t 定义，可直接强转无映射开销。 */
void pal_log_vprintf(pal_log_level_t level, const char *tag,
                     const char *fmt, va_list ap)
{
    esp_log_writev((esp_log_level_t)level, tag, fmt, ap);
}

bool pal_log_in_isr(void)
{
    return xPortInIsrContext() != 0;
}

/* ISR 兜底通路：ERROR/WARN 在中断中仍需可见（尤其是硬件故障、看门狗喂狗失败等）。
 * 严禁调用 esp_log_writev（它内部会拿锁）——先在栈上 vsnprintf 格式化（vsnprintf
 * 在 ESP32 ROM/newlib 中是 ISR-safe 的纯计算函数，不拿锁、不 malloc），
 * 再 esp_rom_printf("%s") 一次性写出到 UART，绕过 IDF 上层缓冲。 */
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
/* 非 ESP-IDF 环境（静态分析/CTags）：提供 stub 避免链接错误。
 * 此路径在生产构建永远不会命中（只有 ESP-IDF CMake 才编译本文件）。 */
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
