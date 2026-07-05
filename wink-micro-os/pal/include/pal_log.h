/**
 * @file pal_log.h
 * @brief PAL 分级日志接口：Error / Warn / Info / Debug 四级。
 *
 * 设计目标：
 *   - 统一跨平台日志入口，消除 APP 层对 ESP_LOG* / printf / console.log 的直接依赖。
 *   - Debug 日志在 release 构建（NDEBUG）下编译为零开销（宏展开为 ((void)0)）。
 *   - 支持文件级隐式 TAG（LOG_TAG）一处定义、全文件复用，降低样板代码。
 *   - ISR 安全：在中断上下文中自动分流到无锁 ROM 打印（ERROR/WARN）或静默丢弃（INFO/DEBUG），
 *     禁止在 ISR 路径上获取互斥锁或执行阻塞 I/O。
 *
 * 平台映射：
 *   - ESP32: 路由到 esp_log_writev()，复用 ESP-IDF 的颜色/时间戳/tag 过滤机制。
 *   - WASM:  格式化后通过 js_pal_log() 桥接到 JS console.error/warn/log/debug。
 *   - Host:  带毫秒时间戳 + 线程 ID 的彩色 stderr 输出，单条原子写入防窜行。
 *
 * 用法：
 *   @code
 *   // 1. 文件顶部定义 LOG_TAG（推荐，作用于整个编译单元）
 *   #define LOG_TAG "dal_servo"
 *   #include "pal_log.h"
 *
 *   LOG_E("init failed: pin=%d rc=%d", pin, rc);   // Error
 *   LOG_W("angle %d out of range, clamped", angle); // Warn
 *   LOG_I("servo initialized");                    // Info
 *   LOG_D("set_angle=%d", angle);                  // Debug (release 下编译为空)
 *
 *   // 2. 或者显式传 tag（适用于跨文件内联函数、公共头等场景）
 *   pal_log_i("i2c", "bus=%d ready", bus);
 *   @endcode
 *
 * 约束：
 *   - fmt 必须是编译期字符串字面量（禁止传入动态拼装的 char*），为后续
 *     Tokenized/Dictionary 日志（Pigweed 风格字典压缩）预留可能。
 *     CI 通过 tools/check_log_format_literals.py 静态检查。
 *   - LOG_D 参数不能包含有副作用的表达式（release 下不求值）。
 */
#ifndef PAL_LOG_H
#define PAL_LOG_H

#include <stdarg.h>
#include <stdbool.h>

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
 * @brief 日志后端函数 —— 每个 target 提供自己的实现（同步路径）。
 *
 * 不应由 APP 层直接调用；统一通过 pal_log_e/w/i/d 或 LOG_E/W/I/D 宏入口使用。
 * ISR 上下文不走此路径（见 pal_log_in_isr() 分流）。
 *
 * @param level  日志级别（已过编译期门控，保证 >= PAL_LOG_COMPILE_LEVEL）
 * @param tag    模块标签（短字符串，通常是 static const char* TAG）
 * @param fmt    printf 格式串（必须为编译期字面量）
 * @param ap     已 va_start 的可变参数列表
 */
void pal_log_vprintf(pal_log_level_t level, const char *tag,
                     const char *fmt, va_list ap);

/* =========================================================================
 *  ISR 上下文探测 —— 平台相关实现
 *
 *  返回 true 表示当前执行流位于中断服务例程（或仿真模拟的 ISR 上下文）中。
 *  此状态下禁止调用任何可能阻塞、获取互斥锁或执行堆分配的函数。
 *
 *  实现：
 *    - ESP32:  封装 xPortInIsrContext()
 *    - Host/Wasm: 返回 pal_os_in_sim_isr_context()（仿真中断标志）
 *    - Baremetal: 始终返回 false（无 RTOS 中断模型）
 * ========================================================================= */

/**
 * @brief 查询当前是否处于 ISR（中断服务例程）上下文。
 * @return true  = 在 ISR 中；false = 普通线程/任务上下文。
 */
bool pal_log_in_isr(void);

/**
 * @brief ISR 上下文下的极简无锁日志输出（ERROR/WARN 专用）。
 *
 * 不获取互斥锁、不做动态内存分配、不做复杂格式化；
 * 在 ESP32 上路由到 esp_rom_printf，在其他平台退化为最佳努力的单次 write。
 *
 * @note 仅供 pal_log_e/pal_log_w 内部在 ISR 路径上调用；APP 层不应直接使用。
 */
void pal_log_isr_write(pal_log_level_t level, const char *tag,
                       const char *fmt, va_list ap);

/* =========================================================================
 *  日志宏 —— 编译期级别门控 + printf 格式检查 + ISR 分流
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
    if (pal_log_in_isr()) {
        pal_log_isr_write(PAL_LOG_ERROR, tag, fmt, ap);
    } else {
        pal_log_vprintf(PAL_LOG_ERROR, tag, fmt, ap);
    }
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
    if (pal_log_in_isr()) {
        pal_log_isr_write(PAL_LOG_WARN, tag, fmt, ap);
    } else {
        pal_log_vprintf(PAL_LOG_WARN, tag, fmt, ap);
    }
    va_end(ap);
}
#else
#define pal_log_w(tag, fmt, ...) ((void)0)
#endif

/**
 * @brief 记录 Info 级日志（正常启动流程、状态变更摘要）。
 *
 * ISR 上下文下静默丢弃（INFO 级别不应在中断中产生，避免引入任何开销）。
 */
#if PAL_LOG_COMPILE_LEVEL >= PAL_LOG_LEVEL_INFO
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static inline void pal_log_i(const char *tag, const char *fmt, ...)
{
    if (pal_log_in_isr()) {
        return;   /* ISR 中丢弃 INFO/DEBUG，仅保留 ERROR/WARN 的无锁通路 */
    }
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
 * ISR 上下文下同样静默丢弃。
 */
#if PAL_LOG_COMPILE_LEVEL >= PAL_LOG_LEVEL_DEBUG
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static inline void pal_log_d(const char *tag, const char *fmt, ...)
{
    if (pal_log_in_isr()) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    pal_log_vprintf(PAL_LOG_DEBUG, tag, fmt, ap);
    va_end(ap);
}
#else
#define pal_log_d(tag, fmt, ...) ((void)0)
#endif

/* =========================================================================
 *  隐式 TAG 快捷宏：LOG_E / LOG_W / LOG_I / LOG_D
 *
 *  若在 #include "pal_log.h" 之前定义了 LOG_TAG 宏，则自动使用该 TAG；
 *  否则回退到默认标签 "SYS"。这避免了每次调用都重复书写 TAG 参数。
 *
 *  推荐在源文件最顶部（include 之前）定义：
 *      #define LOG_TAG "dal_servo"
 * ========================================================================= */
#ifdef LOG_TAG
#  define LOG_E(fmt, ...) pal_log_e(LOG_TAG, fmt, ##__VA_ARGS__)
#  define LOG_W(fmt, ...) pal_log_w(LOG_TAG, fmt, ##__VA_ARGS__)
#  define LOG_I(fmt, ...) pal_log_i(LOG_TAG, fmt, ##__VA_ARGS__)
#  define LOG_D(fmt, ...) pal_log_d(LOG_TAG, fmt, ##__VA_ARGS__)
#else
#  define LOG_E(fmt, ...) pal_log_e("SYS", fmt, ##__VA_ARGS__)
#  define LOG_W(fmt, ...) pal_log_w("SYS", fmt, ##__VA_ARGS__)
#  define LOG_I(fmt, ...) pal_log_i("SYS", fmt, ##__VA_ARGS__)
#  define LOG_D(fmt, ...) pal_log_d("SYS", fmt, ##__VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif /* PAL_LOG_H */
