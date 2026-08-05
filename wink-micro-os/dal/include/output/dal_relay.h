#ifndef DAL_RELAY_H
#define DAL_RELAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file dal_relay.h
 * @brief 二值继电器/开关驱动（直驱、SSR、双线圈磁保持）。
 *
 * 语义统一为 binary state switch：on/off/toggle/set/is_on，
 * 具体电气拓扑由 dal_relay_variant_t 消化（ADR：同类器件语义不变 + 拓扑枚举）。
 *
 * 继电器归类为**执行器**（is_actuator: true，ADR-0058）：暴露
 * dal_relay_safe_off() 并由 codegen 注册到 wink_actuator_registry，使系统在
 * watchdog/panic/assert/异常回滚路径统一去激励。
 *
 * 磁保持拓扑（LATCHING_DUAL_PIN）为非阻塞脉冲状态机：dal_relay_set 仅拉起目标
 * 线圈脚并记录时间戳，脉冲宽度由 dal_relay_poll() 在到点后拉回 inactive。
 * 因此磁保持继电器**必须**经 codegen 注册 poll 到 runtime tick（或由 App 主
 * 循环周期调用 dal_relay_poll），否则线圈会持续通电。
 */

/** 默认磁保持脉冲宽度 (ms)。 */
#define DAL_RELAY_DEFAULT_PULSE_MS 50u

/**
 * @brief 磁保持脉冲宽度上限 (ms)。
 * 线圈设计脉宽通常 30~100ms；uint16 最大 65535ms 会烧毁线圈，init 对越界值
 * 直接拒绝（WINK_ERR_INVALID_ARG），fail-closed。
 */
#define DAL_RELAY_MAX_PULSE_MS 1000u

/**
 * @brief 继电器电气拓扑枚举 (Topology Variant - 一等公民)
 *
 * @note 单线圈 H 桥磁保持（LATCHING_SINGLE_PIN）因无硬件验证、存在 H 桥
 *       穿通风险，已于 ADR-0058 移除，待真实硬件就绪后以独立变体
 *       （含 break-before-make 死区）加回。
 */
typedef enum {
    DAL_RELAY_VARIANT_DIRECT_GPIO       = 0, /**< 单 GPIO 直驱/光耦隔离 (经典单线圈, 默认) */
    DAL_RELAY_VARIANT_SSR               = 1, /**< 固态继电器 (SSR, 零机械触点) */
    DAL_RELAY_VARIANT_LATCHING_DUAL_PIN = 2, /**< 双线圈磁保持继电器 (双脚脉冲触发, 零静态功耗) */
} dal_relay_variant_t;

/**
 * @brief 继电器配置结构体 (POD config_t)
 * 成员按对齐降序排列：owner 指针 → uint16_t → int16_t → uint16_t → enum → bool 标志
 */
typedef struct {
    const char *owner;              /**< 资源占用者名称 (DAL-S-001: 必须为首成员指针) */
    uint16_t pin;                   /**< 主控制 / Set 引脚 (DAL-S-006: 必填 uint16_t) */
    int16_t reset_pin;              /**< Reset 引脚 (磁保持拓扑专用; 可选 → int16_t, -1 表示未绑定) */
    uint16_t pulse_duration_ms;     /**< 磁保持脉冲宽度 (ms, 0 → 默认 DAL_RELAY_DEFAULT_PULSE_MS; 上限 DAL_RELAY_MAX_PULSE_MS) */
    dal_relay_variant_t variant;    /**< 拓扑变体枚举 (DAL-S-001) */
    bool active_low;                /**< 触发极性: false=高有效, true=低有效 */
    bool initial_state;             /**< init 后的初始状态: true=默认吸合, false=默认断开 */
} dal_relay_config_t;

/**
 * @brief 继电器句柄结构体 (POD instance_t)
 * 支持 Flash 动态覆写 (ADR-0008)
 */
typedef struct {
    dal_relay_config_t config;      /**< 配置副本 (DAL-S-011: 值副本且 offsetof == 0) */
    uint32_t pulse_start_ms;        /**< 磁保持脉冲输出起始时间 (用于非阻塞关脉冲) */
    bool is_on;                     /**< 当前逻辑开关状态: true=吸合/导通, false=断开 */
    bool pulse_active;              /**< 磁保持脉冲是否处于输出中 */
    bool initialized;               /**< 初始化状态标记 (DAL-L-004) */
    volatile wink_status_t last_status; /**< 最近一次操作错误码 (DAL-B-025 可观测性) */
} dal_relay_t;

/* DAL-S-014: 首成员偏移静态断言 */
_Static_assert(offsetof(dal_relay_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_relay_config_t) == 20, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_relay_t, initialized) == 26, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_relay_t) == 32, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_relay_config_t) == 24, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_relay_t, initialized) == 30, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_relay_t) == 40, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief 初始化继电器外设并建立初始状态。
 *
 * - DIRECT_GPIO/SSR：将 pin 配置为推挽输出并写入 initial_state 对应电平。
 * - LATCHING_DUAL_PIN：配置 pin/reset_pin 为推挽输出，并按 initial_state
 *   发起一次 SET(initial_state=true) 或 RESET(initial_state=false) 脉冲以
 *   **建立已知物理触点态**（ADR-0058）。脉冲由后续 dal_relay_poll() 拉回，
 *   init 返回后两控制脚回到 inactive（零静态功耗）。默认 initial_state=false
 *   即断开，符合上电安全。
 *
 * @param dev 继电器句柄指针
 * @param cfg 静态配置指针（内部拷贝为值副本）
 * @return WINK_OK 成功；
 *         WINK_ERR_INVALID_ARG 参数为空/磁保持缺 reset_pin/脉宽越界；
 *         WINK_ERR_ALREADY_INITIALIZED 重复初始化；
 *         透传 PAL 错误（WINK_ERR_BUSY 引脚被占 / WINK_ERR_RESOURCE_EXHAUSTED / WINK_ERR_IO 等）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL；cfg->owner 非 NULL；dev 未 initialized。
 *     磁保持拓扑要求 reset_pin >= 0；pulse_duration_ms 为 0 或 <= DAL_RELAY_MAX_PULSE_MS。
 *   - Postconditions: WINK_OK 时 dev->initialized=true、config 深拷贝、GPIO 配置为推挽输出；
 *     直驱/SSR 输出 initial_state 对应电平；磁保持按 initial_state 发起一次 SET/RESET 脉冲
 *     （由 poll 清除）。失败时回滚 claim 与 GPIO，不置 initialized。
 *   - Range: pin/reset_pin 为有效逻辑 GPIO（上界由 pal_gpio_init 把关）。
 *   - Blocking: No。
 *   - Thread-safe: No（调用方串行化 init 与其它方法，DAL-C-040）。
 *   - ISR-safe: No。
 *   - Side-effects: claim GPIO 资源、配置方向、对磁保持发起一次初始脉冲。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_ALREADY_INITIALIZED /
 *     透传 WINK_ERR_BUSY / WINK_ERR_RESOURCE_EXHAUSTED / WINK_ERR_IO 等。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_init(dal_relay_t *dev, const dal_relay_config_t *cfg);

/**
 * @brief 释放继电器外设资源。
 *
 * 去激励语义因拓扑而异（ADR-0058，如实声明，不做统一"安全断开"承诺）：
 * - DIRECT_GPIO/SSR：先 best-effort 写 inactive（去激励），再复位引脚、释放 claim。
 * - LATCHING_DUAL_PIN：best-effort 发起 RESET 脉冲并写 inactive 后立即释放引脚。
 *   **非阻塞路径不保证 RESET 脉冲达到 pulse_duration_ms 宽度，因此不保证物理
 *   触点断开**（磁保持硬件特性）。运行期可靠断开应由 dal_relay_off() +
 *   dal_relay_poll()（或经 runtime 注册的自动 poll）完成；故障路径由
 *   dal_relay_safe_off() 处理。
 *
 * @param dev 继电器句柄指针
 * @return WINK_OK 成功；WINK_ERR_INVALID_ARG dev 为空。未 init 时为幂等 no-op（WINK_OK，DAL-L-010）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。
 *   - Postconditions: best-effort 去激励→pal_gpio_reset_pin（Hi-Z）→释放 resource claim→
 *     memset(dev,0)；dev->initialized=false。磁保持不保证物理触点断开（见上文）。
 *   - Blocking: No。
 *   - Thread-safe: No。
 *   - ISR-safe: No。
 *   - Idempotent: 未 init 返回 WINK_OK；deinit 后可再次 init。
 *   - Side-effects: 写 GPIO inactive、复位引脚、释放资源、清零句柄。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG。
 */
wink_status_t dal_relay_deinit(dal_relay_t *dev);

/**
 * @brief 设置继电器开关状态。
 *
 * - DIRECT_GPIO/SSR：直接写电平。
 * - LATCHING_DUAL_PIN：先 break-before-make 将两脚写 inactive，再对目标线圈脚
 *   发脉冲（on→SET 脚，off→RESET 脚），由 dal_relay_poll() 到点清除。快速反向
 *   切换不会出现两脚同时 active。
 *
 * @param dev 继电器句柄指针
 * @param on  true=吸合/导通, false=断开
 * @return WINK_OK 成功；WINK_ERR_INVALID_ARG dev 为空；
 *         WINK_ERR_NOT_INITIALIZED 未初始化；透传 pal_gpio_write 错误。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL 且已 initialized。
 *   - Postconditions: WINK_OK 时 dev->is_on==on；直驱/SSR 写入目标电平；
 *     磁保持 break-before-make 后对目标线圈发脉冲并置 pulse_active。
 *   - Blocking: No。
 *   - Thread-safe: No（调用方串行化；与 poll 也应串行）。
 *   - ISR-safe: No（磁保持路径读 pal_os_get_ms）。
 *   - Side-effects: 写 GPIO；磁保持记录 pulse_start_ms。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED / 透传 PAL 写错误。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_set(dal_relay_t *dev, bool on);

/**
 * @brief 吸合/导通继电器。
 * @return 同 dal_relay_set。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_on(dal_relay_t *dev);

/**
 * @brief 断开/释放继电器。
 * @return 同 dal_relay_set。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_off(dal_relay_t *dev);

/**
 * @brief 翻转继电器开关状态。
 *
 * 磁保持拓扑基于软件缓存 is_on 取反；若上电后从未建立过已知触点态（例如未经
 * init 已知态或外部改动了触点），toggle 方向可能与物理态不一致。调用方应确保
 * 此前已有一次已知方向的 dal_relay_set（init 已按 initial_state 建立已知态）。
 *
 * @return 同 dal_relay_set。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_toggle(dal_relay_t *dev);

/**
 * @brief 查询继电器当前是否处于吸合状态（逻辑缓存，非回读引脚）。
 * @param dev    继电器句柄指针
 * @param out_on 输出状态指针
 * @return WINK_OK 成功；WINK_ERR_INVALID_ARG 参数为空；WINK_ERR_NOT_INITIALIZED 未初始化。
 * @note API Contract:
 *   - Preconditions: dev、out_on 非 NULL；dev 已 initialized。
 *   - Postconditions: *out_on = dev->is_on。
 *   - Blocking: No。Thread-safe: No（与 set/poll 串行）。ISR-safe: No。
 *   - Side-effects: 无。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_is_on(const dal_relay_t *dev, bool *out_on);

/**
 * @brief 读取最近一次操作的状态码（DAL-B-025 可观测性）。
 * @param dev 继电器句柄指针
 * @return 最近一次 set/toggle/is_on 等操作的 wink_status_t；未 init 返回 WINK_ERR_NOT_INITIALIZED；
 *         dev 为空返回 WINK_ERR_INVALID_ARG。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL 且已 initialized。
 *   - Postconditions: 返回 dev->last_status。
 *   - Blocking: No。Thread-safe: No。ISR-safe: No。
 *   - Side-effects: 无。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_get_last_status(const dal_relay_t *dev);

/**
 * @brief 轮询继电器脉冲定时器（针对磁保持拓扑，非阻塞清除脉冲）。
 *
 * 当脉冲持续时间达到 pulse_duration_ms 时，将 set/reset 两脚写回 inactive 并
 * 清 pulse_active。对 DIRECT_GPIO/SSR 为廉价 no-op。本函数必须被周期调用：
 * codegen 会为磁保持继电器自动注册到 runtime tick，或由 App 主循环自行调用。
 *
 * @param dev 继电器句柄指针
 * @return WINK_OK 成功；WINK_ERR_INVALID_ARG dev 为空。未 init 时为幂等 no-op（WINK_OK）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。
 *   - Postconditions: 若脉冲到时，将 set/reset 两脚写 inactive 并清 pulse_active；
 *     否则保持。未 init 返回 WINK_OK（no-op）。
 *   - Blocking: No。
 *   - Thread-safe: No（应与 dal_relay_set 串行）。
 *   - ISR-safe: No。
 *   - Side-effects: 可能写 GPIO（磁保持脉冲到期时）。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG。
 */
wink_status_t dal_relay_poll(dal_relay_t *dev);

/**
 * @brief 应急/故障关断（ADR-0048 / ADR-0058）：best-effort 断开继电器。
 *
 * 由 wink_actuator_safe_off_all() 在 watchdog/panic/assert/异常回滚路径遍历调用。
 * 绑定到 dal_relay_off：直驱/SSR 立即写 inactive；磁保持发起 RESET 脉冲
 * （由后续 poll 清除）。
 *
 * 与 dal_relay_off() 的区别（对齐 dal_led_safe_off 约定 DAL-L-020~022）：
 *   - 不要求 dev 已初始化（未初始化即"无物可关"，返回 WINK_OK）；
 *   - 不标 WINK_WARN_UNUSED_RESULT（应急路径不强制检查返回值）；
 *   - best-effort，不依赖调度器与堆。
 *
 * @param dev 继电器句柄指针
 * @return WINK_OK（best-effort，已尽力断开）；WINK_ERR_INVALID_ARG dev 为空。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。不要求已 init。
 *   - Postconditions: best-effort 断开（已 init 则调 dal_relay_off）；未 init 返回 WINK_OK（DAL-L-022）。
 *   - Blocking: No。
 *   - Thread-safe: No。
 *   - ISR-safe: No（调 pal_gpio_write）。
 *   - Reentrancy: 幂等，可重复调用（DAL-L-022）。
 *   - Side-effects: 若已 init 则写 GPIO（直驱/SSR 去激励；磁保持发起 RESET 脉冲）。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG。
 */
wink_status_t dal_relay_safe_off(dal_relay_t *dev);

#ifdef __cplusplus
}
#endif

#if !defined(WINK_USE_RELAY) || !WINK_USE_RELAY
#define WINK_RELAY_DISABLED_MSG \
    "Relay driver not enabled; add a \"relay\" device to wink-app.json " \
    "(or set -DWINK_USE_RELAY=ON)."
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_init(dal_relay_t *dev, const dal_relay_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG)
wink_status_t dal_relay_deinit(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_set(dal_relay_t *dev, bool on);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_on(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_off(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_toggle(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_is_on(const dal_relay_t *dev, bool *out_on);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_get_last_status(const dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG)
wink_status_t dal_relay_poll(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG)
wink_status_t dal_relay_safe_off(dal_relay_t *dev);

#endif /* !WINK_USE_RELAY */

#endif /* DAL_RELAY_H */
