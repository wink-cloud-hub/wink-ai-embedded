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
 *              **第三层已迁至 runtime 层**：包含 wink_pt_in_context() 前向声明
 *              与 WINK_ASSERT_NONBLOCKING() 宏的头文件是
 *              `runtime/include/wink_pt_debug.h`（2026-07-04 P1-P2 层级修正）。
 *              PAL 契约层不得引用 runtime 符号（消除层级反转）。
 *              需要 PT-context 断言的 DAL/App 源文件请 `#include "wink_pt_debug.h"`；
 *              大多数场景可通过 `#include "wink_runtime.h"` 传递获得。
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

#define WINK_IGNORE_UNUSED(expr) do { \
    wink_status_t _ignored_status = (expr); \
    (void)_ignored_status; \
} while (0)


/*
 * @brief Wink 平台统一状态码
 *
 * **AI Codegen 语义详表 SSOT**：见
 * `docs/design/07-platform-governance/02-error-fault-model.md` §11
 *（触发场景 / 恢复策略 / 是否可作 `WINK_PT_EXIT` 条件）。
 * 本 enum 每一值的 brief 与该表格首列 bit-for-bit 一致；调整任一侧
 * 必须同步另一侧。
 */
typedef enum {
    WINK_OK = 0,                              /**< 操作成功；`if (status)` 语义为假。 */

    WINK_ERR_INVALID_ARG        = -1,         /**< 参数校验失败（NULL / 越界 / 非法枚举）；caller bug。 */
    WINK_ERR_TIMEOUT            = -2,         /**< 操作超时（I2C ACK / GPIO wait / RMT）；短期重试 ≤ N 次后 fault。 */
    WINK_ERR_DISCONNECTED       = -3,         /**< 器件断线（探测 NACK / 长期无响应）；进 fail-safe，可 `WINK_PT_EXIT`。 */
    WINK_ERR_OUT_OF_RANGE       = -4,         /**< 数值越界（ADC 超量程 / 几何超行程）；限幅继续。 */
    WINK_ERR_IO                 = -5,         /**< 通用 I/O 错误（未归类的 bus 层错）；短期重试。 */
    WINK_ERR_BUSY               = -6,         /**< 资源被占用 / 未就绪；**同时是 PT yield 信号**，生成器需特判非错误。 */
    WINK_ERR_UNSUPPORTED        = -7,         /**< 当前 target/构建缺失能力；编译期应拦截，可 `WINK_PT_EXIT`。 */
    WINK_ERR_CHECKSUM           = -8,         /**< 校验失败（CRC / 数据完整性）；重试 ≤ 2 次。 */
    WINK_ERR_PERMISSION         = -9,         /**< 权限拒绝（沙箱越权 / Flash 保护区）；不会自愈，可 `WINK_PT_EXIT`。 */
    WINK_ERR_RESOURCE_EXHAUSTED = -10,        /**< 资源池耗尽（claim 表满 / PWM 通道用尽）；部署期 bug。 */
    WINK_ERR_NOT_INITIALIZED    = -11,        /**< 器件未 init 就被调用；caller bug，可 `WINK_PT_EXIT`。 */
    WINK_ERR_HARDWARE           = -12,        /**< 底层驱动返错（含 `esp_err_t != ESP_OK` 的统一映射）。 */
    WINK_ERR_NO_MEM             = -13,        /**< 内存不足；runtime path 禁止动态分配，出现即部署错。 */
    WINK_ERR_EMPTY              = -14,        /**< 容器/队列空；通常是 poll API 的正常返回。 */
    WINK_ERR_FULL               = -15,        /**< 容器/队列满；lossless 需扩容 / 加速消费。 */
    WINK_ERR_INVALID_STATE      = -16,        /**< 状态机非法转移（未 claim 引脚 / 已销毁器件被调用）；可 `WINK_PT_EXIT`。 */
    WINK_ERR_LOCKED             = -17,        /**< 资源被锁（boot safe-lock / 配置 flash 锁定）；不由 PT 自行解锁。 */

    WINK_ERR_OVERCURRENT        = -20,        /**< 过流（可恢复：限流重试）；持续则升级为致命。 */
    WINK_ERR_OVERTEMPERATURE    = -21,        /**< 过温（可恢复：降频 / 降占空比）；持续则关输出。 */

    WINK_ERR_WATCHDOG           = -30,        /**< 看门狗超时（致命）；由 boot safe-lock 复位处理。 */

    WINK_ERR_OVERFLOW           = -40,        /**< 数值溢出 / 计算 UB（致命）；停止不可信计算并 halt。 */

    WINK_ERR_CONFIG_CORRUPT_DEGRADED = -50,   /**< 配置损坏 → 用安全默认值继续（不停机，BAL 走保守分支）。 */
    WINK_ERR_FAILED_INIT             = -51,   /**< 器件 init 失败 → 器件隔离，系统继续；强依赖 PT 可 `WINK_PT_EXIT`。 */

    WINK_ERR_PANIC              = -99,        /**< 不可恢复内部错误（INVARIANT / 非法 PT 调用）；halt 等外部复位。 */
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
