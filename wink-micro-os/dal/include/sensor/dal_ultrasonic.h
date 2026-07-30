#ifndef DAL_ULTRASONIC_H
#define DAL_ULTRASONIC_H

#include <stdint.h>
#include <stdbool.h>
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
 * 成员按对齐需求降序排列（c-code.md §4）：4B(float/uint32/enum) → 2B(config) → 1B(bool)，
 * 消除内部 padding。仅重排顺序、未改字段名，designated initializer 与
 * 所有 `dev->xxx` 访问均不受影响（非破坏性）。
 *
 * Phase 2 改进：内嵌 config 副本，便于：
 *   1. Flash 动态覆写（ADR-0008）：从 Flash blob 读取 → 写入 config → dal_xxx_apply_override
 *   2. 运行时诊断：可直接打印当前生效的配置
 */
typedef struct {
    /* —— 4B —— */
    volatile float          last_distance;   ///< 最近一次测量距离 (cm) (volatile: SMP cross-core reader)
    volatile uint32_t       last_pulse_us;   ///< Phase 4：上次 echo 脉宽 μs (volatile)
    volatile wink_status_t  last_status;     ///< Phase 4：上次测量结果状态（ERROR 时为具体错误码）(volatile)
    volatile dal_ultrasonic_state_t  state;  ///< Phase 4：非阻塞测量状态机 (volatile)
    /* —— 2B —— */
    dal_ultrasonic_config_t config;          ///< 配置副本（trig_pin, echo_pin, use_rmt），由 init 从 cfg 拷贝
    /* —— 1B —— */
    bool                    initialized;     ///< Phase 2：dal_ultrasonic_init 成功后置 true
} dal_ultrasonic_t;

/**
 * @brief 初始化超声波：校验引脚、配置 GPIO 方向（真机）、置 initialized。
 *
 * Phase 2 标准化：统一采用 config_t 模式，简化 Codegen 设备树生成。
 * 旧 API（trig_pin + echo_pin 分离参数）已迁移至此。
 *
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL；cfg->trig_pin != cfg->echo_pin。
 *   - Blocking: No.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(NULL/同 pin) / 透传 PAL 错误
 *     （真机：WINK_ERR_IO / WINK_ERR_BUSY / WINK_ERR_RESOURCE_EXHAUSTED）。
 *   - Postconditions: WINK_OK 时 dev->initialized=true；trig/echo 方向已配置（真机）；
 *                     cfg 的内容已深拷贝到 dev->config。
 *   - Sim 分支：跳过物理 GPIO 配置（旁路最低物理信号层，ADR-0003 决策2），仅置结构状态。
 *   - ESP32：自动初始化 RMT 硬件脉冲捕获；RMT 失败自动降级到 busy-wait（cfg->use_rmt 变为 false）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_init(dal_ultrasonic_t *dev, const dal_ultrasonic_config_t *cfg);

/**
 * @brief 请求一次测量（触发后立即返回；非阻塞）。
 * @note host：测量在 request 内经 pal_gpio_pulse_in 同步完成（虚拟时间下单 tick 即 READY），
 *       表现为「单 tick ready」——这是可接受的仿真保真（host 测状态机契约，非真实 wall-clock 异步）。
 *       ESP32：经 RMT 硬件测量，CPU 仅阻塞在信号量等待（不消耗 CPU，由 FreeRTOS 调度）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_ultrasonic_init() 已成功。
 *   - Blocking: Yes (≈ 测量时间 + 调度开销)，but RMT version is not busy-waiting.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK(请求已发出) / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 *   - Postconditions: 触发已发出；结果（READY/ERROR + last_status）经 get_cached_distance 读。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_request_measurement(dal_ultrasonic_t *dev);

/**
 * @brief 非阻塞读取上次测量的缓存距离/状态。
 * @note API Contract:
 *   - Preconditions: dev/distance_cm 非 NULL；dal_ultrasonic_init() 已成功。
 *   - Blocking: No.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK(READY，*distance_cm 有效) / WINK_ERR_BUSY(MEASURING/IDLE) /
 *     last_status(ERROR) / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 *   - Postconditions: WINK_OK 时 *distance_cm 为缓存距离。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_get_cached_distance(const dal_ultrasonic_t *dev, float *distance_cm);

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
 * @note API Contract:
 *   - Preconditions: dev/distance_cm 非 NULL；dal_ultrasonic_init() 已成功。
 *   - Thread-safe: No; ISR-safe: No (含阻塞 delay/polling)
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED / WINK_ERR_TIMEOUT
 *   - Postconditions: dev->last_distance 在 WINK_OK 时更新
 */
WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm);
#endif  /* WINK_STRICT_NONBLOCKING */

/**
 * @brief ADR-0008 Flash 覆写：从 16B params 反序列化并改写超声波 trig/echo 引脚。
 * @note params 布局（小端）：trig_pin:u16@0, echo_pin:u16@2（≥4B）。
 *       轻校验(trig≠echo) 与 dal_ultrasonic_init 权威校验纵深配合。
 *       非法 → 不写任何字段，返 WINK_ERR_INVALID_ARG。
 *       void* 签名适配 wink_dev_override_fn 注册表（见 wink_dev_config.h），
 *       dev 在 dal_ultrasonic_init 之前被覆写。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_apply_override(void *dev, const uint8_t *params, uint16_t len);

/**
 * @brief 反初始化超声波：停止 RMT（若已启用）、释放 trig/echo GPIO 资源、置 initialized=false。
 * @note 可在未 init 的 dev 上安全调用（直接返回 WINK_OK，no-op）。
 * @return WINK_OK
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
wink_status_t dal_ultrasonic_get_cached_distance(const dal_ultrasonic_t *dev, float *distance_cm);
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG) WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm);
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_apply_override(void *dev, const uint8_t *params, uint16_t len);
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_deinit(dal_ultrasonic_t *dev);
#endif /* !WINK_USE_ULTRASONIC */

#endif /* DAL_ULTRASONIC_H */
