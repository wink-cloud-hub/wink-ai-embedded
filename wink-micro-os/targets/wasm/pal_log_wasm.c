/**
 * @file pal_log_wasm.c
 * @brief Wasm 分级日志后端：格式化后通过 js_pal_log() 桥接到 JS console。
 *
 * 设计：在 C 侧做 vsnprintf 格式化（wasm 线性内存），再把 NUL 终止的字符串指针
 * 通过 js_pal_log(level, msg_cstr) 传给 JS，JS 侧按 level 分派到
 * console.error/warn/log/debug。这样做避免在 C 侧持有 JS 函数引用或使用
 * EM_ASM 变参桥接，保持与现有 wasm_bridge.h extern 导入模式一致。
 *
 * 输出前缀（模拟时间戳）：
 *   "[%10llu ms] [L] [tag] message"
 *   其中时间戳取自 pal_os_get_ms()（虚拟时钟 ms），便于在仿真运行里按事件顺序
 *   对齐日志；JS 宿主可选择自行添加真实墙钟时间，但仿真因果一致性优先。
 *
 * 为什么不直接走 vprintf/emscripten stdout：
 *   1. 需要按级别分派到不同 console 方法（error/warn/log/debug 有不同颜色
 *      和堆栈追踪），Module.print 无法区分级别。
 *   2. js_pal_log 可以被宿主 Workbench 覆盖，把日志转发到 UI 面板。
 *
 * ISR 安全：
 *   - ISR 上下文（pal_os_in_sim_isr_context()==true）下，ERROR/WARN 通过
 *     同一 js_pal_log 桥发出（js_pal_log 同步、无锁、不 malloc），但前缀带
 *     "!ISR!" 标识；INFO/DEBUG 在 ISR 下静默丢弃。
 */
#include "pal_log.h"
#include "pal_osal.h"
#include "wasm_bridge.h"

#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

/* 格式化缓冲区：栈上 384 字节，满足绝大多数单行日志（含时间戳/TAG 前缀）。
 * WINK_STACK_USAGE_LIMIT=1536 字节（见 CMakeLists），384 B 可接受。
 * 超长消息会被截断（vsnprintf 返回值 < bufsz 时安全）。*/
#define PAL_LOG_BUF_SIZE 384

/* js_pal_log 的 extern 在 wasm_bridge.h 中声明（PAL 侧 JS 导入）。
 * JS 侧默认实现在 wink_sim_js.js。*/

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

    /* "[%10llu ms] [L] [tag] " 前缀 */
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
    /* 带 !ISR! 标识，便于在宿主 UI 中识别中断期日志 */
    int prefix_len = snprintf(buf, sizeof(buf), "!ISR! [%10" PRIu64 " ms] [%s] [%s] ",
                              ms, level_letter(level), tag_safe);
    if (prefix_len < 0) { prefix_len = 0; }
    if ((size_t)prefix_len < sizeof(buf) - 1) {
        vsnprintf(buf + prefix_len, sizeof(buf) - (size_t)prefix_len, fmt, ap);
    }
    buf[sizeof(buf) - 1] = '\0';
    /* 同步调用、无锁，不经过 stdio；JS 侧不得在该回调里 re-enter wasm。 */
    js_pal_log((uint8_t)level, buf);
}
