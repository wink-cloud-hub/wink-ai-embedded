/**
 * @file pal_debug.h
 * @brief PAL 调试输出接口：跨平台统一的 printf 封装。
 *
 * 所有平台都有此接口：
 *   - ESP32: 输出到 UART
 *   - WASM: 映射到 JS console.log
 *   - host: 输出到 stdout
 *
 * 用法：用 pal_debug_printf() 替代 APP 层的 printf，
 *       消除编译时平台分支，同时保留调试输出能力。
 */
#ifndef PAL_DEBUG_H
#define PAL_DEBUG_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 格式化调试输出（printf 语义）。
 *
 * @param fmt 格式化字符串，同 printf
 * @param ... 可变参数
 *
 * 平台差异：
 *   - ESP32: 调用 ets_printf 或 printf (UART 输出)
 *   - WASM: 映射到 EM_ASM 或 console.log
 *   - host: 标准 printf
 *
 * 注意：此接口仅用于调试输出，不应用于生产逻辑。
 *       生产代码应使用 wink_trace 故障追踪系统。
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
void pal_debug_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* PAL_DEBUG_H */
