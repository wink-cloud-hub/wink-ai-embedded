/**
 * @file pal_log.h
 * @brief PAL 分级日志接口：Error / Warn / Info / Debug 四级。
 *
 * 设计目标：
 *   - 统一跨平台日志入口，消除 APP 层对 ESP_LOG* / printf / console.log 的直接依赖。
 *   - Debug 日志在 release 构建（NDEBUG）下编译为零开销（宏展开为 ((void)0)）。
 *   - 与 pal_debug_printf 共存：pal_debug_printf 保持原样（过渡期），新代码使用 pal_log_*。
 *
 * 平台映射：
 *   - ESP32: 路由到 esp_log_writev()，复用 ESP-IDF 的颜色/时间戳/tag 过滤机制。
 *   - WASM:  格式化后通过 js_pal_log() 桥接到 JS console.error/warn/log/debug。
 *   - Host:  fprintf(stderr, ...) 带 ANSI 颜色前缀。
 *
 * 用法：
 *   @code
 *   #include "pal_log.h"
 *   static const char *TAG = "dal_servo";
 *
 *   pal_log_e(TAG, "init failed: pin=%d rc=%d", pin, rc);
 *   pal_log_w(TAG, "angle %d out of range, clamped", angle);
 *   pal_log_i(TAG, "servo initialized");
 *   pal_log_d(TAG, "set_angle=%d", angle);   // release 下编译为空
 *   @endcode
 *
 * @note 与 pal_debug_printf 的关系：
 *   - pal_debug_printf 是无级别裸 printf，保留用于快速调试输出；
 *   - pal_log_* 是长期生产级日志入口，带级别/tag/过滤/颜色。
 *   - P2 阶段会逐步将关键路径的 pal_debug_printf 迁移为 pal_log_w/e。
 */
#ifndef PAL_LOG_H
#define PAL_LOG_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 *  日志级别 —— 数值宏 + C 枚举双形式
 *
 *  数值必须与 ESP-IDF esp_log_level_t 对齐（NONE=0, ERROR=1, WARN=2, INFO=3, DEBUG=4），
 *  便于 ESP32 后端零成本映射。
 *
 *  注意：#if 预处理器指令看不到 C enum 值（enum 是编译期概念），因此
 *        PAL_LOG_LEVEL_* 必须是 #define 宏，供 #if PAL_LOG_COMPILE_LEVEL >= ... 使用。
 *        pal_log_level_t 枚举只在 C 代码类型检查时使用，引用同一批数值宏。
 * ========================================================================= */
#define PAL_LOG_LEVEL_NONE    0
#define PAL_LOG_LEVEL_ERROR   1
#define PAL_LOG_LEVEL_WARN    2
#define PAL_LOG_LEVEL_INFO    3
#define PAL_LOG_LEVEL_DEBUG   4

typedef enum {
    PAL_LOG_NONE  = PAL_LOG_LEVEL_NONE,
    PAL_LOG_ERROR = PAL_LOG_LEVEL_ERROR,
    PAL_LOG_WARN  = PAL_LOG_LEVEL_WARN,
    PAL_LOG_INFO  = PAL_LOG_LEVEL_INFO,
    PAL_LOG_DEBUG = PAL_LOG_LEVEL_DEBUG,
} pal_log_level_t;

/**
 * @brief 编译期最小日志级别。
 *
 * 低于该级别的日志宏展开为 ((void)0)，参数不求值，零运行时开销。
 * 可通过 -DPAL_LOG_COMPILE_LEVEL=N 在构建系统覆盖：
 *   - NDEBUG 构建（release）默认 PAL_LOG_LEVEL_INFO（debug 被裁剪）。
 *   - 非 NDEBUG 构建默认 PAL_LOG_LEVEL_DEBUG（四级全部保留）。
 */
#ifndef PAL_LOG_COMPILE_LEVEL
#  ifdef NDEBUG
#    define PAL_LOG_COMPILE_LEVEL PAL_LOG_LEVEL_INFO
#  else
#    define PAL_LOG_COMPILE_LEVEL PAL_LOG_LEVEL_DEBUG
#  endif
#endif

/**
 * @brief 日志后端函数 —— 每个 target 提供自己的实现。
 *
 * 不应由 APP 层直接调用；统一通过 pal_log_e/w/i/d 宏入口使用。
 *
 * @param level  日志级别（已过编译期门控，保证 >= PAL_LOG_COMPILE_LEVEL）
 * @param tag    模块标签（短字符串，通常是 static const char* TAG）
 * @param fmt    printf 格式串
 * @param ap     已 va_start 的可变参数列表
 */
void pal_log_vprintf(pal_log_level_t level, const char *tag,
                     const char *fmt, va_list ap);

/* =========================================================================
 *  日志宏 —— 编译期级别门控 + printf 格式检查
 * ========================================================================= */

/**
 * @brief 记录 Error 级日志（不可恢复错误、初始化失败、硬件异常）。
 * @param tag 模块标签（字符串字面量或 static const char*）
 * @param fmt printf 格式串
 * @param ... 可变参数
 */
#if PAL_LOG_COMPILE_LEVEL >= PAL_LOG_LEVEL_ERROR
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static inline void pal_log_e(const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    pal_log_vprintf(PAL_LOG_ERROR, tag, fmt, ap);
    va_end(ap);
}
#else
#define pal_log_e(tag, fmt, ...) ((void)0)
#endif

/**
 * @brief 记录 Warn 级日志（可恢复异常、参数越界被钳位、降级路径）。
 */
#if PAL_LOG_COMPILE_LEVEL >= PAL_LOG_LEVEL_WARN
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static inline void pal_log_w(const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    pal_log_vprintf(PAL_LOG_WARN, tag, fmt, ap);
    va_end(ap);
}
#else
#define pal_log_w(tag, fmt, ...) ((void)0)
#endif

/**
 * @brief 记录 Info 级日志（正常启动流程、状态变更摘要）。
 */
#if PAL_LOG_COMPILE_LEVEL >= PAL_LOG_LEVEL_INFO
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static inline void pal_log_i(const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    pal_log_vprintf(PAL_LOG_INFO, tag, fmt, ap);
    va_end(ap);
}
#else
#define pal_log_i(tag, fmt, ...) ((void)0)
#endif

/**
 * @brief 记录 Debug 级日志（调试细节、高频数据）。
 *
 * ⚠️ release 构建（NDEBUG）下编译为 ((void)0)，参数不求值，零运行时开销。
 *    不要在参数表达式里放有副作用的代码（自增/函数调用等）。
 */
#if PAL_LOG_COMPILE_LEVEL >= PAL_LOG_LEVEL_DEBUG
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static inline void pal_log_d(const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    pal_log_vprintf(PAL_LOG_DEBUG, tag, fmt, ap);
    va_end(ap);
}
#else
#define pal_log_d(tag, fmt, ...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* PAL_LOG_H */
