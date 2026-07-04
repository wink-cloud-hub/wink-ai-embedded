/**
 * @file pal_log_wasm.c
 * @brief Wasm 分级日志后端：格式化后通过 js_pal_log() 桥接到 JS console。
 *
 * 设计：在 C 侧做 vsnprintf 格式化（wasm 线性内存），再把 NUL 终止的字符串指针
 * 通过 js_pal_log(level, msg_cstr) 传给 JS，JS 侧按 level 分派到
 * console.error/warn/log/debug。这样做避免在 C 侧持有 JS 函数引用或使用
 * EM_ASM 变参桥接，保持与现有 wasm_bridge.h extern 导入模式一致。
 *
 * 为什么不直接走 vprintf/emscripten stdout：
 *   1. 需要按级别分派到不同 console 方法（error/warn/log/debug 有不同颜色
 *      和堆栈追踪），Module.print 无法区分级别。
 *   2. js_pal_log 可以被宿主 Workbench 覆盖，把日志转发到 UI 面板。
 */
#include "pal_log.h"
#include "wasm_bridge.h"
#include <stdio.h>
#include <stdarg.h>

/* 格式化缓冲区：栈上 256 字节，满足绝大多数单行日志。
 * WINK_STACK_USAGE_LIMIT=1536 字节（见 CMakeLists），256 B 可接受。
 * 超长消息会被截断（vsnprintf 返回值 < bufsz 时安全）。*/
#define PAL_LOG_BUF_SIZE 256

/* js_pal_log 的 extern 在 wasm_bridge.h 中声明（PAL 侧 JS 导入）。
 * JS 侧默认实现在 wink_sim_js.js。*/

void pal_log_vprintf(pal_log_level_t level, const char *tag,
                     const char *fmt, va_list ap)
{
    char buf[PAL_LOG_BUF_SIZE];
    /* 先写入 "TAG: " 前缀，再追加格式化消息 */
    int prefix_len = snprintf(buf, sizeof(buf), "%s: ", tag ? tag : "?");
    if (prefix_len < 0) {
        prefix_len = 0;
    }
    if ((size_t)prefix_len < sizeof(buf) - 1) {
        vsnprintf(buf + prefix_len, sizeof(buf) - (size_t)prefix_len, fmt, ap);
    }
    buf[sizeof(buf) - 1] = '\0';
    js_pal_log((uint8_t)level, buf);
}
