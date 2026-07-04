/**
 * @file pal_wasm_internal.h
 * @brief Wasm target 内部接口（仅 wasm target 文件及 wink_runtime.c 的 SIMULATION 分支 include，
 *        不进入公共 PAL 接口，不由 host/esp32 target 编译）。
 *
 * 中断队列容量配置（方案 C：Wasm 主动 Poll 模型）：
 *   PAL_WASM_INTERRUPT_QUEUE_SIZE 控制 JS pending 中断队列的上限（JS 侧 MAX_PENDING 须与之一致）。
 *   超出时新中断被丢弃并在 JS 侧告警（不静默丢失）。
 *
 *   典型场景估算：超声波测距每次最多 2 个边沿事件（上升 + 下降），按 10ms tick
 *   极少同时积累超过 4 个。AI 生成业务逻辑可能产生更密集中断，默认 16 提供 4× 余量。
 *
 *   可在 CMake 或编译命令中覆盖，例如：
 *     target_compile_definitions(wink_simulator PRIVATE PAL_WASM_INTERRUPT_QUEUE_SIZE=32)
 */
#ifndef PAL_WASM_INTERNAL_H
#define PAL_WASM_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_sim_physical.h"

#ifndef PAL_WASM_INTERRUPT_QUEUE_SIZE
#define PAL_WASM_INTERRUPT_QUEUE_SIZE 16
#endif

/**
 * @brief 分发所有 JS pending 中断（tick 边界调用，由 wink_runtime.c 在 SIMULATION 宏下驱动）。
 *
 * @note 调用约束（方案 C 核心安全保证）：
 *   - 必须在 Wasm 处于正常运行态调用（非 Asyncify sleeping 窗口）。
 *   - 必须在 wink_app_delay_ms()（Asyncify 挂起点）之前调用——见 wink_runtime.c tick 循环。
 *   - 内部循环 drain 所有 pending 中断（FIFO），直到 js_pal_poll_interrupt 返回 false。
 *   - 非 ISR 上下文，与 ESP32 Bottom-Half 消费 FreeRTOS Queue 语义对称（ADR-0002）。
 *
 * 非 SIMULATION target（host/esp32）不编译此函数，零运行时代价。
 */
void pal_wasm_dispatch_pending_interrupts(void);

/**
 * @brief 分发 C 侧软中断 FIFO（pal_irq_set_pending 入队项）。
 *
 * Phase C P0-1 统一分发：由 pal_wasm_dispatch_pending_interrupts() 在 JS 队列
 * drain 完后级联调用；pal_irq_restore() 最外层 unlock 也显式补发。
 */
void pal_wasm_dispatch_pending_irqs(void);

/**
 * @brief 虚拟时钟步进接口（导出给 JS Worker）。
 *
 * SSOT 架构唯一入口：wasm 侧不主动步进时钟，时钟完全由 JS Worker 驱动。
 * 参见 pal_osal_wasm.c 注释。
 *
 * @param us 步进微秒数
 */
void pal_wasm_advance_virtual_clock(uint64_t us);

/**
 * @brief 检查时钟溢出早期警告是否已触发（Wave2 P1 Task 6）。
 *
 * SSOT：内部状态 s_clock_warning_fired，跨越 CLOCK_WARNING_THRESHOLD
 * （UINT64 中点，约 292 年微秒）后置位并保持。JS Worker 每个 tick 边界
 * 轮询本函数，首次返回 true 时输出 console.warn 提示用户重置仿真环境。
 *
 * 幂等：触发后重复读不会产生副作用，也不会自动清除——清除唯一方式是
 * 重启 wasm 实例（BSS 重新零初始化）。
 *
 * @return true 时钟已超过 50% 量程；false 未触发。
 */
bool pal_wasm_is_clock_warning_fired(void);

/**
 * @brief 获取当前虚拟时钟值（Wave2 P1 Task 6，调试/告警用途）。
 *
 * 与 pal_os_get_us() 返回同一 SSOT 状态；语义上是 Task 6 警告链路上专用
 * 的导出符号（与 pal_os_get_us 同源但命名更明确，便于 JS 侧 cwrap 时区分
 * 用途）。生产业务代码请使用 pal_os_get_us()。
 *
 * @return 当前虚拟时钟微秒数
 */
uint64_t pal_wasm_get_virtual_clock_us(void);

/* ─────────────────────────────────────────────────────────
 * 物理退化引擎内部 API（ADR-0009 Wave 2）。pal_hal_wasm.c 的 GPIO/I2C
 * 中间件层（Task 3）通过这些 helpers 读取静态故障配置 + per-pin ctx。
 *
 * 边界保证：pin >= WASM_SIM_MAX_PINS (=128) 时 get_debounce_ctx 返回
 * NULL，HAL 层须把 NULL 当作"该 pin 无退化"处理。
 *
 * WASM_SIM_MAX_PINS 在此头公开（而非藏于 pal_wasm_physical.c），便于 HAL
 * 中间件做前置边界检查 → 越界 pin 直接返回低电平/错误，避免给 JS 桥
 * 传越界值（防御深度，§3.3 plan 注释）。
 *
 * PRNG 推进协议：HAL 层调用 pal_wasm_get_prng_state() 取当前种子，
 * 传给算法库 (wink_phys_bus_drop 等)，算法返回时种子已被推进，HAL
 * 层用 pal_wasm_advance_prng_state() 写回。
 * ───────────────────────────────────────────────────────── */
#define WASM_SIM_MAX_PINS 128

uint32_t pal_wasm_get_bounce_us(void);
uint16_t pal_wasm_get_i2c_drop_permil(void);
uint32_t pal_wasm_get_prng_state(void);
void     pal_wasm_advance_prng_state(uint32_t new_state);
wink_phys_debounce_ctx_t *pal_wasm_get_debounce_ctx(uint16_t pin);

/* ─────────────────────────────────────────────────────────
 * 故障审计日志系统（ADR-0009 Wave 2 Task 8）
 * ─────────────────────────────────────────────────────────
 * 记录所有物理退化事件（GPIO 抖动、I2C 丢包等）到固定容量环形缓冲区，
 * 用于 CI 测试失败时的因果链追溯（"哪个故障在哪个时间点触发"）。
 *
 * 容量：WASM_FAULT_LOG_SIZE 条，超出后最早条目被覆盖（FIFO 环形语义）。
 * 序号：每条事件携带全局单调递增 sequence，CI 侧用作 "since last check"
 *      增量游标，不会被覆盖事件而回退（即使条目被环回覆盖）。
 * 时钟：timestamp_us 来自 pal_wasm_get_virtual_clock_us()，与同源算法
 *      golden vector 时基一致。
 *
 * 零退化默认路径：未启用故障注入时 pal_wasm_log_fault 不会被调用，热路径
 * 零开销。HAL 中间件仅在退化分支触发后埋点（见 pal_hal_wasm.c）。
 */
#define WASM_FAULT_LOG_SIZE 256

typedef enum {
    FAULT_TYPE_GPIO_BOUNCE = 1,    /* 一次抖动窗口被触发 */
    FAULT_TYPE_I2C_DROP    = 2,    /* 一次 I2C 传输被丢弃 */
    FAULT_TYPE_I2C_NOISE   = 3,    /* 预留：未来 I2C 噪声注入 */
    FAULT_TYPE_CLOCK_DRIFT = 4,    /* 预留：未来时钟漂移注入 */
} wasm_fault_type_t;

/* 故障事件记录。字段紧凑排列以减小 256 条总占用（~16B × 256 = 4KB）。 */
typedef struct {
    uint64_t timestamp_us;    /* 故障发生时的虚拟时钟（与 pal_os_get_us 同源） */
    uint8_t  fault_type;      /* wasm_fault_type_t 枚举值 */
    uint16_t pin_or_bus;      /* GPIO pin 或 I2C 总线号 */
    uint32_t sequence;        /* 全局单调递增序号（首条 = 1） */
} wasm_fault_event_t;

/** 重置故障日志（测试间隔离用）。 */
void pal_wasm_reset_fault_log(void);

/** 返回 wasm 实例是否已进入 fault 锁存状态（P0-3 Phase C）。
 *  JS 宿主可在每次跨边界调用后轮询此函数快速失败，避免 fault 后再入 C 逻辑。 */
bool pal_wasm_is_faulted(void);

/**
 * Fast-fail guards for pal_wasm_* EMSCRIPTEN_KEEPALIVE exports after a fault
 * has latched (P0-3/P1-4 Phase C).
 *
 * Place at the top of every state-mutating export (set_*, advance_*, reset_*,
 * scheduler entry points). Read-only getters may opt out since they cannot
 * corrupt state. After s_wasm_faulted is set:
 *   - Void-returning mutators become silent no-ops (safe-off already executed).
 *   - wink_status_t-returning mutators return WINK_ERR_INVALID_STATE so callers
 *     can detect the faulted state rather than getting a silent false-success.
 *   - Value-returning getters on the latch itself (pal_wasm_is_faulted) still work.
 *
 * Rationale: returning WINK_ERR_INVALID_STATE (not WINK_OK) prevents JS hosts
 * from interpreting a silent void return as success and continuing to drive
 * state. This is the primary failure mode P0-3's latch is designed to close:
 * accidental clock advancement, fault config mutation, or I/O after safe-off.
 *
 * Usage:
 *   WASM_FAULT_GUARD_VOID();     // for void funcs  (silent no-op)
 *   WASM_FAULT_GUARD_WINKERR();  // for wink_status_t funcs (returns INVALID_STATE)
 *   WASM_FAULT_GUARD_BOOL();     // for bool-returning funcs (returns false = failure)
 *
 * Return-value rationale:
 *   - VOID: caller (JS Worker post-message) doesn't await a result; silent no-op
 *     is safe because safe-off has already executed.
 *   - WINKERR: propagates a distinguishable error to C callers so they don't
 *     misinterpret a silent return as WINK_OK.
 *   - BOOL: returns false (the conventional "operation failed" sentinel for
 *     pal_wasm_gpio_read / pal_wasm_i2c_transfer JS-facing wrappers).
 *
 * NOTE: Do NOT place these guards on pal_wasm_reset_physical() itself — it is
 * the only mutating export permitted in faulted state (it calls
 * pal_wasm_clear_fault_latch() to release the latch as part of reset).
 *
 * NOTE: Read-only getters (pal_os_get_us, pal_wasm_is_faulted,
 * pal_wasm_get_*, pal_wasm_fault_event_get_*) are intentionally EXEMPT —
 * they cannot corrupt post-fault state and are needed for diagnostics.
 */
#define WASM_FAULT_GUARD_VOID() do { if (pal_wasm_is_faulted()) return; } while (0)
#define WASM_FAULT_GUARD_WINKERR() do { if (pal_wasm_is_faulted()) return WINK_ERR_INVALID_STATE; } while (0)
#define WASM_FAULT_GUARD_BOOL() do { if (pal_wasm_is_faulted()) return false; } while (0)

/** 清除 fault 锁存（供 pal_sim_scheduler_run 新周期启动时内部调用，非对外导出）。 */
void pal_wasm_clear_fault_latch(void);

/** Host→C fault 注入（P0-3 Phase C；JS 侧 wrapper 调用此 EMSCRIPTEN_KEEPALIVE 导出）。
 *  code = 8003 是 JS host plugin fault（用户 js_* override 抛异常 / reject）。
 *  msg_cstr 必须是 wasm 线性内存内的 NUL-terminated UTF-8 字符串（JS 侧 _malloc 后 stringToUTF8 写入，调用后 _free）。 */
void pal_wasm_host_fault(uint32_t code, const char* msg_cstr);

/** 重置所有物理退化状态（faults / debounce ctx / PRNG / fault log / 故障域 / fault 锁存）。
 *  测试间隔离与 JS 端 scenario reset 共用此入口。等价于把 BSS 拉回初始状态
 *  但允许在中途调用，不依赖加载器重新零初始化。 */
void pal_wasm_reset_physical(void);

/** 追加一条故障事件到环形缓冲区。HAL 中间件在退化分支调用。 */
void pal_wasm_log_fault(uint8_t fault_type, uint16_t pin_or_bus);

/** 返回当前已记录的事件数（环回后保持在 WASM_FAULT_LOG_SIZE）。 */
uint32_t pal_wasm_get_fault_log_count(void);

/** 按"从最旧到最新"的顺序读取索引 index 的事件。
 *  index >= count 时返回 false 且 *out_event 不被写入。 */
bool pal_wasm_get_fault_event(uint32_t index, wasm_fault_event_t *out_event);

/* ─────────────────────────────────────────────────────────
 * 功耗模型接口（Wave3 预埋；ADR-0009 Wave 2 Task 9）
 * ─────────────────────────────────────────────────────────
 * 接口已定义，但当前实现为 stub（无真实计算）。
 *
 * 目的：避免未来 Wave3 做功耗-时序联合仿真时需要大规模重构现有故障注入
 *      管线。提前锁定 ABI（类型 + 符号）后，Wave3 只需在 pal_wasm_physical.c
 *      内点亮真实积分逻辑，JS Worker / device-tree / Workbench 上层调用
 *      无需任何 churn。
 *
 * 字段语义（载体型字段名，Wave3 实施时直接读取）：
 *   active_current_ua    pin 处于驱动有源态时的电流（uA）
 *   leakage_current_ua   pin 静态时的漏电流（uA）
 *   transition_energy_nj 单次电平跳变消耗的等效能量（nJ）
 *
 * 边界约定：pin >= WASM_SIM_MAX_PINS 时 set_pin_power_model 返回
 *          WINK_ERR_INVALID_ARG，与 debounce ctx 越界处理对称（§3.3 plan）。
 * model == NULL 时同样返回 WINK_ERR_INVALID_ARG。
 *
 * 当前 stub 行为：set 不存储任何状态；get_total_energy_mj 始终返回 0。
 */

/** Pin 级功耗模型参数 */
typedef struct wasm_pin_power_model_t {
    uint32_t active_current_ua;     /* 有源时电流 (uA) */
    uint32_t leakage_current_ua;    /* 漏电流 (uA) */
    uint32_t transition_energy_nj;  /* 单次跳变能量 (nJ) */
} wasm_pin_power_model_t;

/** 设置指定 pin 的功耗模型（stub：返回 OK 但不存储参数） */
wink_status_t pal_wasm_set_pin_power_model(uint8_t pin,
                                           const wasm_pin_power_model_t *model);

/** 获取仿真启动以来的总能耗（stub：始终返回 0） */
uint64_t pal_wasm_get_total_energy_mj(void);

/* ─────────────────────────────────────────────────────────
 * 故障域隔离框架（Wave3 预埋；ADR-0009 Wave 2 Task 10）
 * ─────────────────────────────────────────────────────────
 * 当前所有故障注入共享单一全局配置（s_faults），爆炸半径不可控：调高
 * I2C drop 率会立刻波及到 GPIO bounce/ADC noise 等不相关通路。Wave3
 * 将把每条故障注入逻辑切换为按域查找独立配置，本任务先把 ABI 落地：
 *
 *   - 枚举锁定可寻址的域 ID 集合（GLOBAL + 几个常见外设/总线）。
 *   - get_domain_config / arm_fault_domain / get_domain_trigger_count
 *     三件套，足够 Wave3 中间件按域读配置 + 检查激活态 + 累计触发计数。
 *
 * 当前实现行为：
 *   - get_domain_config(domain_id) 对所有合法域都返回同一份全局 s_faults，
 *     这是设计意图——保证当前行为零变化，Wave3 替换为 per-domain 数组时
 *     现有调用点无需改动。
 *   - 首次调用 pal_wasm_reset_physical() 之后，所有域 armed=true（GLOBAL 域
 *     永远 armed，下游中间件可忽略该字段直到 Wave3 真正用上）。
 *   - 注意：BSS 零初始化状态下 armed=false；JS Worker 必须在 INIT 阶段
 *     调用 pal_wasm_reset_physical() 后再读取域状态。
 *   - trigger_count 永远是 0，Wave3 在故障注入分支累加。
 *
 * 边界约定（与 power_model_stub / debounce_ctx 对称）：
 *   - domain_id >= WASM_FAULT_DOMAIN_COUNT → get_config 返回 NULL；
 *     arm_fault_domain 返回 WINK_ERR_INVALID_ARG；trigger_count 返回 0。
 *
 * 故障审计日志（Task 8）与本框架的关系：审计日志记录"已发生的退化事件"，
 * 域框架决定"哪些路径会触发退化"。两者正交：未来 Wave3 多域时，单条
 * 审计事件仍只关联一个 (domain_id, pin_or_bus) 元组，日志格式无需扩展。
 */

typedef enum {
    WASM_FAULT_DOMAIN_GLOBAL = 0,    /* 全局域（当前唯一实际生效的域） */
    WASM_FAULT_DOMAIN_GPIO   = 1,    /* GPIO 域（Wave3 预留） */
    WASM_FAULT_DOMAIN_I2C0   = 2,    /* I2C 总线 0（Wave3 预留） */
    WASM_FAULT_DOMAIN_I2C1   = 3,    /* I2C 总线 1（Wave3 预留） */
    WASM_FAULT_DOMAIN_SPI0   = 4,    /* SPI 总线 0（Wave3 预留） */
    WASM_FAULT_DOMAIN_CLOCK  = 5,    /* 时钟域（Wave3 预留） */
    WASM_FAULT_DOMAIN_COUNT          /* 数组容量哨兵；也是首个非法 ID */
} wasm_fault_domain_id_t;

/** 故障域运行时状态（每域一份；BSS 零初始化后由 reset_physical 设为默认值）。
 *  config 字段当前为指针别名，Wave3 升级为每域独立的 wink_sim_faults_t 实例。 */
typedef struct {
    uint32_t domain_id;         /* 域 ID 自识别（== wasm_fault_domain_id_t） */
    bool     armed;             /* 该域是否参与故障注入；当前所有域始终 true */
    uint32_t trigger_count;     /* 已触发的退化事件累计计数（Wave3 累加） */
} wasm_fault_domain_t;

/** 获取指定故障域的配置指针（Wave3 预埋）。
 *  当前所有合法域返回同一份全局 s_faults；越界返回 NULL。 */
wink_sim_faults_t *pal_wasm_get_domain_config(uint32_t domain_id);

/** 武装/解除指定故障域（Wave3 预埋）。
 *  当前 GLOBAL 域始终 armed；其它域的 armed 标志只是占位，下游不读取。
 *  domain_id 越界返回 WINK_ERR_INVALID_ARG。 */
wink_status_t pal_wasm_arm_fault_domain(uint32_t domain_id, bool armed);

/** 获取指定故障域的累计触发计数（Wave3 预埋）。
 *  当前永远返回 0；越界亦返回 0（sentinel，不会读未初始化内存）。 */
uint32_t pal_wasm_get_domain_trigger_count(uint32_t domain_id);

#endif /* PAL_WASM_INTERNAL_H */
