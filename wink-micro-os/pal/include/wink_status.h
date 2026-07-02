#ifndef WINK_STATUS_H
#define WINK_STATUS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define WINK_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
    #define WINK_WARN_UNUSED_RESULT
#endif

/* ─────────────────────────────────────────────────────────
 * 阻塞 API 硬隔离（ADR-0017）
 * `WINK_DEPRECATED_MSG(msg)` — 通用 deprecation 属性，携带调用点解释。
 * `WINK_BLOCKING`             — 语义子集：单次调用 busy-wait > 一个 runtime tick
 *                               (10ms) / 硬件轮询未主动 yield / 违反 ADR-0007
 *                               协作式执行契约。
 *
 * 三层硬隔离（ADR-0017 §决策结论 §落地规则）：
 *   1. 编译期：GCC/Clang `__attribute__((deprecated(msg)))` /
 *              MSVC `__declspec(deprecated(msg))` → 所有调用点 warning。
 *   2. 链接期：`-DWINK_STRICT_NONBLOCKING=1` 时，API 头文件用 `#ifndef` 包围
 *              把声明从翻译单元剔除 → 调用点变 undefined reference。
 *              协作式调度器构建路径**必须**默认开启（PLAN R-8）。
 *   3. 运行期：`WINK_PT_DEBUG` 下 `WINK_ASSERT_NONBLOCKING()` 检测 PT 上下文
 *              误调 → `wink_trace_fault(WINK_ERR_PANIC) + assert`。
 *              **第三层由协作式调度器 T5 阶段落地**（ADR-0017 §阶段二），
 *              M3 内仅交付一 + 二层；PT context 检测钩子 `wink_pt_in_context()`
 *              暂未定义，故本文件先给出**无操作占位宏**以稳定 API 面：
 *                #define WINK_ASSERT_NONBLOCKING() ((void)0)
 *              T5 阶段直接把宏体替换为 §阶段二 §5 中的 assert 版本。
 *
 * 谁负责挂载：新增 DAL/PAL API 时若满足上述条件，**必须**由该 API 的 owner
 * 在头文件挂 `WINK_BLOCKING` 并加 `#ifndef WINK_STRICT_NONBLOCKING` 包围
 * （Code Review 卡口，见 PLAN §3.3 R-5）。
 */
#if defined(__GNUC__) || defined(__clang__)
    #define WINK_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
    #define WINK_DEPRECATED_MSG(msg) __declspec(deprecated(msg))
#else
    #define WINK_DEPRECATED_MSG(msg)
#endif

#define WINK_BLOCKING \
    WINK_DEPRECATED_MSG("Blocking API forbidden in cooperative runtime; use non-blocking variant")

/* Runtime PT-context 检测占位宏；协作式调度器 T5 阶段替换为真实实现。
 * 保留可调用性以便 blocking API 现在就能写 `WINK_ASSERT_NONBLOCKING();`
 * 作为函数体首行，T5 落地时无需再逐个 API 加行——只需替换宏体。 */
#define WINK_ASSERT_NONBLOCKING() ((void)0)

typedef enum {
    WINK_OK = 0,

    WINK_ERR_INVALID_ARG        = -1,
    WINK_ERR_TIMEOUT            = -2,
    WINK_ERR_DISCONNECTED       = -3,
    WINK_ERR_OUT_OF_RANGE       = -4,
    WINK_ERR_IO                 = -5,
    WINK_ERR_BUSY               = -6,
    WINK_ERR_UNSUPPORTED        = -7,
    WINK_ERR_CHECKSUM           = -8,
    WINK_ERR_PERMISSION         = -9,
    WINK_ERR_RESOURCE_EXHAUSTED = -10,
    WINK_ERR_NOT_INITIALIZED    = -11,
    WINK_ERR_HARDWARE           = -12,
    WINK_ERR_NO_MEM             = -13,
    WINK_ERR_EMPTY              = -14,
    WINK_ERR_FULL               = -15,
    WINK_ERR_INVALID_STATE      = -16,
    WINK_ERR_LOCKED             = -17,

    WINK_ERR_OVERCURRENT        = -20,
    WINK_ERR_OVERTEMPERATURE    = -21,

    WINK_ERR_WATCHDOG           = -30,

    WINK_ERR_OVERFLOW           = -40,

    WINK_ERR_CONFIG_CORRUPT_DEGRADED = -50,
    WINK_ERR_FAILED_INIT             = -51,

    WINK_ERR_PANIC              = -99,
} wink_status_t;

static inline int wink_status_is_error(wink_status_t s) {
    return s < 0;
}

#ifndef PAL_PWM_CHANNELS
#define PAL_PWM_CHANNELS 8
#endif

#ifndef WINK_MAX_SOFT_TIMERS
#define WINK_MAX_SOFT_TIMERS 16
#endif

#ifndef WINK_RUNTIME_TICK_MS
#define WINK_RUNTIME_TICK_MS 10
#endif

#ifdef __cplusplus
}
#endif

#endif
