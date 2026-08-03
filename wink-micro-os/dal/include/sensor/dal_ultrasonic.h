#ifndef DAL_ULTRASONIC_H
#define DAL_ULTRASONIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 非阻塞测量状态机（Phase 4） */
typedef enum {
    DAL_ULTRASONIC_IDLE      = 0,
    DAL_ULTRASONIC_MEASURING = 1,
    DAL_ULTRASONIC_READY     = 2,
    DAL_ULTRASONIC_ERROR     = 3,
} dal_ultrasonic_state_t;

/**
 * @brief 超声波配置结构体（标准化 config_t 模式，便于 Codegen 设备树生成）
 *
 * Phase 2 标准化：所有 DAL 外设统一采用 dal_xxx_config_t + dal_xxx_init(dev, cfg) 模式。
 * 便于代码生成器（app_codegen.py）输出结构化的初始化数据。
 *
 * 成员按对齐降序排列（uint16_t → bool）：自然对齐，无填充。
 */
typedef struct {
    const char *owner;    /* 资源占用 owner 静态字符串（device_tree 实例名，静态存储） */
    uint16_t trig_pin;
    uint16_t echo_pin;
    bool use_rmt;         ///< ESP32：true=RMT 硬件捕获，false=busy-wait 降级
} dal_ultrasonic_config_t;

/**
 * @brief 超声波非阻塞测量句柄（POD，ADR-0004 静态分发；DAL-C-040 默认非线程安全）
 *
 * 内嵌 config 副本，便于：
 *   1. Flash 动态覆写（ADR-0008）：从 Flash blob 读取 → 写入 config → dal_xxx_apply_override
 *   2. 运行时诊断：可直接打印当前生效的配置
 *
 * @note SMP cross-core snapshot / publication order contract (DAL-C-010):
 *   Four fields are cross-core shared via `volatile`:
 *     - `last_distance`  (float,  4B) — payload
 *     - `last_pulse_us`  (uint32, 4B) — payload
 *     - `last_status`    (wink_status_t, 4B) — payload
 *     - `state`          (dal_ultrasonic_state_t, 4B) — state machine
 *
 *   **Reader order** (in `dal_ultrasonic_get_cached_distance`): snapshot all
 *   three payload fields into locals FIRST, then snapshot `state`. State drives
 *   payload validity — when state == READY, the snapshotted payload is valid.
 *
 *   **Writer order** (in `dal_ultrasonic_request_measurement` success path):
 *   write `last_pulse_us` → `last_distance` → `last_status` (payload) FIRST,
 *   then `memw` compiler barrier, then `state = READY`. On Xtensa the explicit
 *   `__asm__ __volatile__("memw" ::: "memory")` ensures prior writes reach RAM
 *   before state becomes visible; on other targets the compiler barrier is
 *   sufficient since the fields are 4-byte aligned naturally.
 *
 *   The four fields are 4B each, single-writer (request_measurement or
 *   get_cached_distance paths only), so DAL-C-001 (single-writer tolerated
 *   stale-read) holds.
 */
typedef struct {
    /* config MUST be the first member (DAL-S-011, offsetof==0). */
    dal_ultrasonic_config_t config;          ///< 配置副本（trig_pin, echo_pin, use_rmt），由 init 从 cfg 拷贝
    /* —— 4B —— */
    volatile float          last_distance;   ///< 最近一次测量距离 (cm) (volatile: SMP cross-core reader)
    volatile uint32_t       last_pulse_us;   ///< Phase 4：上次 echo 脉宽 μs (volatile)
    volatile wink_status_t  last_status;     ///< Phase 4：上次测量结果状态（ERROR 时为具体错误码）(volatile)
    volatile dal_ultrasonic_state_t  state;  ///< Phase 4：非阻塞测量状态机 (volatile)
    /* —— 1B —— */
    bool                    initialized;     ///< Phase 2：dal_ultrasonic_init 成功后置 true
} dal_ultrasonic_t;

/* ABI stability (spec §2.3 / DAL-BC-010): config MUST remain the first member.
 * Offsets below are compiler-verified:
 *   - 64-bit host (LP64) measured: config=16, handle=40, initialized@32
 *     (config: ptr 8B + u16 2B + u16 2B + bool 1B + 3B pad = 16B;
 *      handle: 16B + float 4B + u32 4B + enum 4B + enum 4B + bool 1B + 3B pad = 40B).
 *   - 32-bit target (ILP32) derived: config=12 (ptr 4B + u16 + u16 + bool 1B + 3B pad);
 *     handle=32; initialized@24.
 * Recompute with the target compiler if the struct layout changes — enums are
 * int-sized (4B), pointers are 4B on ILP32 / 8B on LP64, bool is 1B. */
_Static_assert(offsetof(dal_ultrasonic_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_ultrasonic_config_t) == 12, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_ultrasonic_t, initialized) == 24, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_ultrasonic_t) == 32, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_ultrasonic_config_t) == 16, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_ultrasonic_t, initialized) == 32, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_ultrasonic_t) == 40, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief 初始化超声波：校验引脚、配置 GPIO 方向（真机）、置 initialized。
 *
 * Phase 2 标准化：统一采用 config_t 模式，简化 Codegen 设备树生成。
 * 旧 API（trig_pin + echo_pin 分离参数）已迁移至此。
 *
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL；cfg->owner 非 NULL（静态存储）；
 *                    cfg->trig_pin != cfg->echo_pin；dev 未 initialized。
 *   - Postconditions: WINK_OK 时 dev->initialized=true；trig/echo 方向已配置（真机）；
 *                     cfg 的内容已深拷贝到 dev->config；失败时 dev->initialized
 *                     保持 false 并回滚所有已 claim 的 PAL 资源（DAL-L-007/008）。
 *   - Range: trig_pin / echo_pin MUST 互不相等（具体值域由底层 PAL 校验）。
 *   - Blocking: No.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Reentrancy: No (caller MUST serialize init/deinit against other methods).
 *   - Side-effects: claim trig/echo GPIO 资源；配置 GPIO 方向（真机）；
 *                   触发 RMT 硬件 capture warm-up（仅 ESP32 且 cfg->use_rmt=true）；
 *                   writes dev->config / dev->state = IDLE / dev->last_status = OK。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(NULL/owner/同 pin) /
 *     WINK_ERR_ALREADY_INITIALIZED / 透传 PAL 错误
 *     （真机：WINK_ERR_IO / WINK_ERR_BUSY / WINK_ERR_RESOURCE_EXHAUSTED）。
 *   - Simulation-parity: WASM 仿真跳过物理 GPIO 配置（旁路最低物理信号层，
 *                       ADR-0003 决策2），仅置结构状态；ESP32 真机自动初始化
 *                       RMT 硬件脉冲捕获，RMT 失败自动降级到 busy-wait。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_init(dal_ultrasonic_t *dev, const dal_ultrasonic_config_t *cfg);

/**
 * @brief 请求一次测量（触发后立即返回；非阻塞）。
 * @note host：测量在 request 内经 pal_gpio_pulse_in 同步完成（虚拟时间下单 tick 即 READY），
 *       表现为「单 tick ready」——这是可接受的仿真保真（host 测状态机契约，非真实 wall-clock 异步）。
 *       ESP32：经 RMT 硬件测量，CPU 仅阻塞在信号量等待（不消耗 CPU，由 FreeRTOS 调度）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_ultrasonic_init() 已成功；
 *                    dev->state != MEASURING（DAL-B-021 BUSY guard）。
 *   - Postconditions: dev->state ∈ {READY, ERROR}（详见 Side-effects）；
 *                     触发已发出；结果经 get_cached_distance 读。
 *   - Range: N/A（无单位参数）。
 *   - Blocking: Yes（实测耗时；约等于 echo 脉宽 + 调度开销）。
 *       真实硬件 worst-case ≈ 60ms（ULTRASONIC_TIMEOUT_US + trigger 10µs）。
 *       RMT 后端不消耗 CPU，但函数仍同步等待 RMT done 中断——不满足协作式
 *       runtime loop 的"非阻塞"语义（DAL-B-001a 迁移期保留）。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Reentrancy: No.
 *   - Side-effects: 拉高 trig_pin 10µs（触发脉冲）→ 拉低 → 调 pal_gpio_pulse_in 等待 echo
 *                   脉宽 → 写 dev->last_pulse_us / dev->last_distance / dev->last_status
 *                   → Xtensa memw barrier → 写 dev->state ∈ {READY, ERROR}。
 *                   失败时 dev->state = ERROR 且 dev->last_status 保存具体错误码。
 *   - Error-codes: WINK_OK(请求已发出；状态码读 get_cached 查) /
 *     WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED / WINK_ERR_BUSY(并发请求)。
 *   - Simulation-parity: WASM 端 pal_gpio_pulse_in 委托 js_sim_* 旁路；ESP32 端
 *                       RMT 硬件测量；两端共享单位换算与超时判定逻辑。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_request_measurement(dal_ultrasonic_t *dev);

/**
 * @brief 非阻塞读取上次测量的缓存距离/状态。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；out_distance_cm 非 NULL；dal_ultrasonic_init() 已成功。
 *   - Postconditions: WINK_OK 时 *out_distance_cm 写入缓存距离；其他返码
 *                     MUST NOT 写入 *out_distance_cm（DAL-F-020）。
 *   - Range: *out_distance_cm ∈ [2.0, 400.0] cm（HC-SR04 物理量程，超出视为无效/钳位）；
 *            0 cm 表示尚未成功测量过（state == IDLE 返回 NO_DATA，*out_distance_cm 不被写入）。
 *   - Blocking: No.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Reentrancy: Yes (只读 volatile snapshot，无副作用)。
 *   - Side-effects: 仅 snapshot dev 的 4 个 volatile 字段到本地后做 switch；不碰硬件。
 *   - Error-codes:
 *       WINK_OK            (state==READY 且缓存有效)
 *       WINK_ERR_BUSY      (state==MEASURING 且尚无 READY 缓存)
 *       WINK_ERR_NO_DATA   (state==IDLE，从未 request_measurement 过)
 *       last_status         (state==ERROR，透传具体错误码如 TIMEOUT/IO 等)
 *       WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED
 *   - Simulation-parity: 同 request_measurement——两端共享状态机与单位换算；
 *                       WASM 单 tick 即 READY，ESP32 真异步。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_get_cached_distance(const dal_ultrasonic_t *dev, float *out_distance_cm);

/* ADR-0017：blocking API 三层硬隔离首个应用点。
 * 层 1（编译期）：WINK_BLOCKING → __attribute__((deprecated(msg)))，所有调用点告警。
 * 层 2（链接期）：-DWINK_STRICT_NONBLOCKING=1 时下面的声明整段消失 → 调用点 undefined reference。
 *                 host 默认构建**不**开该宏，过渡期单测（test_dal_ultrasonic{,_sim}.c）可继续调用，
 *                 靠 #pragma push 局部禁用 -Wdeprecated-declarations 保 -Werror 通过。
 *                 协作式调度器 T5 阶段在 runtime_cooperative_* sample 的 CMakeLists 追加
 *                 target_compile_definitions(<t> PRIVATE WINK_STRICT_NONBLOCKING=1) 开启剔除。
 * 层 3（运行期）：函数体首行 WINK_ASSERT_NONBLOCKING()，当前为 no-op 占位，T5 阶段替换为
 *                 PT context 检测 → wink_trace_fault + assert。
 */
#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief 获取障碍物距离 (cm) —— 阻塞 busy-wait，**@deprecated**。
 * @deprecated Runtime/App 10ms tick 不得调用本 API；保留仅供过渡/单测，App 完全迁移到非阻塞
 *             且 host 协作推进重构后移除（Phase 4 follow-up）。
 *             严格模式（-DWINK_STRICT_NONBLOCKING=1）下声明从头文件剔除 → 链接失败。
 * @see dal_ultrasonic_request_measurement + dal_ultrasonic_get_cached_distance（非阻塞替代路径）。
 * @note Blocking: Yes. Worst-case ≈ 2 * ULTRASONIC_TIMEOUT_US + trigger pulse (≈ 60ms+)。
 *       Not allowed in cooperative runtime loop.
 *       TWDT-safe at default 5s window (60ms ≪ 5s) — DTRT on apps that bump
 *       TWDT below 200ms; otherwise prefer request_measurement + get_cached_distance.
 * @note API Contract:
 *   - Preconditions: dev/out_distance_cm 非 NULL；dal_ultrasonic_init() 已成功。
 *   - Postconditions: WINK_OK 时 *out_distance_cm 与 dev->last_distance 均更新为新距离。
 *   - Range: *out_distance_cm ∈ [2.0, 400.0] cm (HC-SR04 量程); 0 仅在未触发时。
 *   - Thread-safe: No; ISR-safe: No (含阻塞 delay/polling)
 *   - Reentrancy: No.
 *   - Side-effects: 触发 trig 10µs 脉冲 → pal_gpio_pulse_in 阻塞等待 echo → 写
 *                   dev->last_distance 与 *out_distance_cm。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED / WINK_ERR_TIMEOUT
 *   - Simulation-parity: 同 request_measurement——两端共享 pal_gpio_pulse_in 实现。
 */
WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *out_distance_cm);
#endif  /* WINK_STRICT_NONBLOCKING */

/**
 * @brief ADR-0008 Flash 覆写：从 wire payload 反序列化并改写超声波 trig/echo 引脚。
 *
 * @note Wire format versions (DAL-BC-012):
 *   - v0 (legacy, 4B): trig_pin:u16@0, echo_pin:u16@2 — 无 version 字节.
 *   - v1 (current, 5B): schema_version:u8@0 (=0x01), trig_pin:u16@1, echo_pin:u16@3.
 *
 *   Version detection: 当 `len >= 5` 且 `params[0] == 0x01` 时按 v1 解析;
 *   否则按 v0 解析（len >= 4B）。v0 在字段为 0x00 起头时仍能正确解析。
 *   任何 v2 及以上版本当前拒绝（len >= 5 但 params[0] != 0x01 走 v0 路径，
 *   若 v0 字段意外命中 0x01 字节组合则按 v0 解析，可能产生伪数据——后续
 *   v2 引入时需收紧此规则）。
 *
 *   轻校验(trig≠echo) 与 dal_ultrasonic_init 权威校验纵深配合。
 *   非法 → 不写任何字段，返 WINK_ERR_INVALID_ARG。
 *
 *   void* 签名适配 wink_dev_override_fn 注册表（见 wink_dev_config.h），
 *   dev 在 dal_ultrasonic_init 之前被覆写。
 *
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；params 非 NULL；len >= 4（v0 最少）且 v1 时 len >= 5。
 *   - Postconditions: WINK_OK 时 u->config.trig_pin/echo_pin 已写入新值；其他返码字段保持不变。
 *   - Range: u16 (0..65535)，具体值域由底层 PAL 校验。
 *   - Blocking: No.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Reentrancy: No (MUST 串行于 init/deinit)。
 *   - Side-effects: 写入 dev->config.trig_pin 与 dev->config.echo_pin；不碰硬件，不
 *                   改变 dev->state / dev->initialized。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(NULL/len/version/同 pin)。
 *   - Simulation-parity: 与 init 一致——两端共用同一反序列化逻辑，无平台分支。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_apply_override(void *dev, const uint8_t *params, uint16_t len);

/**
 * @brief 反初始化超声波：拉低 trig、停 RMT、复位 GPIO、释放资源、memset 清零。
 *
 * @note 可在未 init 的 dev 上安全调用（直接返回 WINK_OK，no-op）。
 *       内部每个 step 失败时 LOG_W 并继续 best-effort 执行后续 step；最终
 *       返回值为第一个失败的 rc（若无失败则 WINK_OK）。无论返码如何，
 *       句柄 MUST 已被 memset 清零（DAL-L-015），调用方 MUST NOT 重试或继续使用。
 *
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。
 *   - Postconditions: dev 已 memset 清零；dev->initialized=false；trig/echo 资源已
 *                     释放（best-effort）；RMT 已 deinit（若曾启用）。
 *   - Range: N/A.
 *   - Blocking: No.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Reentrancy: Yes (idempotent; 未 init 返 WINK_OK).
 *   - Side-effects: 拉低 trig_pin (safe-off) → RMT deinit (force-stop DMA) →
 *                   pal_gpio_reset_pin(trig/echo) → pal_resource_release(trig/echo) →
 *                   memset dev 清零。
 *   - Error-codes: WINK_OK（无失败）/ WINK_ERR_INVALID_ARG (dev NULL) /
 *                  透传 first-fail rc（其余 step 已 best-effort 执行但 LOG_W 留痕）。
 *   - Simulation-parity: WASM 端 RMT 与 GPIO 复位为 no-op；ESP32 端 force-stop RMT DMA。
 */
wink_status_t dal_ultrasonic_deinit(dal_ultrasonic_t *dev);

#ifdef __cplusplus
}
#endif

/* ── Compile-time pruning stubs (P2-1 2026-07-06) ──────────────────────
 * See dal_led.h header comment for rationale.
 *
 * Note: dal_ultrasonic_read() is itself guarded by #ifndef WINK_STRICT_NONBLOCKING
 * in its live declaration above; the stub here re-declares it outside that
 * guard so it is ALSO reported as unavailable when the driver is compiled
 * out, regardless of strict-nonblocking mode.
 */
#if !defined(WINK_USE_ULTRASONIC) || !WINK_USE_ULTRASONIC
#define WINK_ULTRASONIC_DISABLED_MSG \
    "Ultrasonic driver not enabled; add an \"ultrasonic\" device to " \
    "wink-app.json (or set -DWINK_USE_ULTRASONIC=ON)."
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_init(dal_ultrasonic_t *dev, const dal_ultrasonic_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_request_measurement(dal_ultrasonic_t *dev);
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_get_cached_distance(const dal_ultrasonic_t *dev, float *out_distance_cm);
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG) WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *out_distance_cm);
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_apply_override(void *dev, const uint8_t *params, uint16_t len);
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG)
wink_status_t dal_ultrasonic_deinit(dal_ultrasonic_t *dev);
#endif /* !WINK_USE_ULTRASONIC */

#endif /* DAL_ULTRASONIC_H */
