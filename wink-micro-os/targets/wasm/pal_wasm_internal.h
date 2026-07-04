/**
 * @file pal_wasm_internal.h
 * @brief Wasm target 内部接口（仅 wasm target 与 wink_runtime.c 的 SIMULATION 分支
 *        include，不进入公共 PAL，也不由 host/esp32 编译）。
 *
 * 头文件只暴露契约（容量宏、类型、原型、WASM_FAULT_GUARD_* 宏）；
 * 实现细节 / 设计动机放在对应 .c 文件的 docblock。
 */
#ifndef PAL_WASM_INTERNAL_H
#define PAL_WASM_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_sim_physical.h"

/* JS pending 中断队列容量（JS 侧 MAX_PENDING 须一致；超出丢弃 + 告警）。 */
#ifndef PAL_WASM_INTERRUPT_QUEUE_SIZE
#define PAL_WASM_INTERRUPT_QUEUE_SIZE 16
#endif

/* per-pin 状态上限（debounce ctx / 未来功耗模型）。pin >= 此值 → sentinel。 */
#define WASM_SIM_MAX_PINS 128

/* 故障审计日志容量（环形；条目自带全局单调 sequence，覆盖后仍可增量溯源）。 */
#define WASM_FAULT_LOG_SIZE 256

/* ── 中断分发（pal_irq_wasm.c）─────────────────────────────── */
/* 分发所有 JS pending 中断（tick 边界；非 Asyncify sleeping 窗口调用）。 */
void pal_wasm_dispatch_pending_interrupts(void);
/* 分发 C 侧软中断 FIFO；由上一函数级联调用，pal_irq_restore 最外层 unlock 也补发。 */
void pal_wasm_dispatch_pending_irqs(void);

/* ── 虚拟时钟（pal_osal_wasm.c，导出到 JS Worker）──────────── */
/* SSOT：wasm 侧不主动步进，由 JS Worker 驱动。 */
void     pal_wasm_advance_virtual_clock(uint64_t us);
/* 时钟溢出早期警告（跨 UINT64 中点后置位并保持；重启 wasm 实例才清零）。 */
bool     pal_wasm_is_clock_warning_fired(void);
/* 当前虚拟时钟微秒数（与 pal_os_get_us() 同源，命名区别仅便于 JS 桥 cwrap）。 */
uint64_t pal_wasm_get_virtual_clock_us(void);

/* ── 物理退化引擎（pal_wasm_physical.c）────────────────────── */
/* HAL 中间件读全局故障配置 + per-pin ctx 后调 wink_sim_physical 算法库。
 * get_debounce_ctx 对越界 pin 返回 NULL（HAL 须视为"该 pin 无退化"）。
 * PRNG 推进：取 state → 传算法（算法内部推进）→ advance_prng_state 写回。 */
uint32_t pal_wasm_get_bounce_us(void);
uint16_t pal_wasm_get_i2c_drop_permil(void);
uint32_t pal_wasm_get_prng_state(void);
void     pal_wasm_advance_prng_state(uint32_t new_state);
wink_phys_debounce_ctx_t *pal_wasm_get_debounce_ctx(uint16_t pin);

/* Wave3 interim: pal_wasm_fault_domain.c 用它把所有域别名回落到全局 s_faults；
 * Wave3 引入 per-domain 配置存储后与该 helper 一起删除。 */
wink_sim_faults_t *pal_wasm_get_faults_ref(void);

/* ── Fault 锁存 / 审计日志（pal_wasm_fault.c）──────────────── */
typedef enum {
    FAULT_TYPE_GPIO_BOUNCE = 1,    /* 一次抖动窗口被触发 */
    FAULT_TYPE_I2C_DROP    = 2,    /* 一次 I2C 传输被丢弃 */
    FAULT_TYPE_I2C_NOISE   = 3,    /* 预留：未来 I2C 噪声注入 */
    FAULT_TYPE_CLOCK_DRIFT = 4,    /* 预留：未来时钟漂移注入 */
} wasm_fault_type_t;

/* 故障事件记录，~16B × WASM_FAULT_LOG_SIZE = 4 KB。 */
typedef struct {
    uint64_t timestamp_us;    /* 与 pal_os_get_us 同源 */
    uint8_t  fault_type;      /* wasm_fault_type_t */
    uint16_t pin_or_bus;      /* GPIO pin 或 I2C 总线号 */
    uint32_t sequence;        /* 全局单调递增（首条 = 1；环形覆盖不回退） */
} wasm_fault_event_t;

/* Fault 锁存查询；mutator 应通过 WASM_FAULT_GUARD_* 宏 fast-fail。 */
bool pal_wasm_is_faulted(void);

/* Fast-fail guards for pal_wasm_* EMSCRIPTEN_KEEPALIVE mutators once s_wasm_faulted
 * has latched. VOID → 静默 no-op；WINKERR → WINK_ERR_INVALID_STATE（避免 C 侧
 * caller 把 void silent return 误解为 OK）；BOOL → false。
 * 豁免：pal_wasm_reset_physical（唯一 latched 期允许运行的 mutator，自己解锁）；
 *       只读 getter（不腐化状态且诊断必需）。 */
#define WASM_FAULT_GUARD_VOID()    do { if (pal_wasm_is_faulted()) return; } while (0)
#define WASM_FAULT_GUARD_WINKERR() do { if (pal_wasm_is_faulted()) return WINK_ERR_INVALID_STATE; } while (0)
#define WASM_FAULT_GUARD_BOOL()    do { if (pal_wasm_is_faulted()) return false; } while (0)

/* 清 fault 锁存并清空 App callbacks 引用（scheduler_run 新周期启动时内部调用）。 */
void pal_wasm_clear_fault_latch(void);

/* Scheduler ↔ fault 显式 helper：pal_osal_wasm.c 只走这两个接口，不直连
 * pal_wasm_fault.c 的 static 状态。set_callbacks 须在每一轮 scheduler_run
 * 入口重新注册（clear_fault_latch 会同时清空引用）。 */
struct wink_app_callbacks;
void pal_wasm_fault_set_callbacks(const struct wink_app_callbacks *cb);

/* 内部 fault 注入（当前唯一调用者：scheduler WCET 兜底 code=8002）。
 * 幂等：已 latch 后仅 trace 不重复 safe-off。 */
void pal_wasm_invoke_fault(uint32_t code);

/* Host→C fault 注入（JS 侧 safeWrap 捕获宿主 plugin 异常后调用，code=8003）。
 * msg_cstr 必须是 wasm 线性内存内 NUL 结尾的 UTF-8 字符串（JS 侧 _malloc +
 * stringToUTF8 写入，调用后 _free）。 */
void pal_wasm_host_fault(uint32_t code, const char *msg_cstr);

/* 重置全部物理退化状态（faults / debounce ctx / PRNG / fault log / 故障域 /
 * fault 锁存）；等效 BSS 零初始化，允许运行期调用。 */
void pal_wasm_reset_physical(void);

/* 故障日志：HAL 中间件在退化分支调用 log_fault（未启用故障注入时零开销）。
 * get_event 按"从最旧到最新"逻辑索引；越界时 *out_event 不被写入，
 * 且 get_count 返回值为环回后的实际条目数（最多 WASM_FAULT_LOG_SIZE）。 */
void     pal_wasm_log_fault(uint8_t fault_type, uint16_t pin_or_bus);
uint32_t pal_wasm_get_fault_log_count(void);
bool     pal_wasm_get_fault_event(uint32_t index, wasm_fault_event_t *out_event);
void     pal_wasm_reset_fault_log(void);

/* ── 功耗模型 Wave3 stub（pal_wasm_fault_domain.c）─────────── */
/* ABI 冻结、当前 stub：set 不存储，get_total_energy_mj 返回 0。 */
typedef struct wasm_pin_power_model_t {
    uint32_t active_current_ua;     /* 有源电流 (uA) */
    uint32_t leakage_current_ua;    /* 漏电流 (uA) */
    uint32_t transition_energy_nj;  /* 单次跳变能量 (nJ) */
} wasm_pin_power_model_t;

/* pin >= WASM_SIM_MAX_PINS 或 model == NULL → WINK_ERR_INVALID_ARG。 */
wink_status_t pal_wasm_set_pin_power_model(uint8_t pin,
                                           const wasm_pin_power_model_t *model);
uint64_t      pal_wasm_get_total_energy_mj(void);

/* ── 故障域隔离 Wave3 stub（pal_wasm_fault_domain.c）──────── */
/* ABI 冻结：当前所有合法域都别名到全局 s_faults；trigger_count 恒 0。
 * 首次访问契约：BSS 零初始化下 armed=false —— JS Worker 必须在 INIT 阶段
 * 调用 pal_wasm_reset_physical() 后再读取域状态（reset 会把 armed 置 true）。 */
typedef enum {
    WASM_FAULT_DOMAIN_GLOBAL = 0,    /* 全局域（当前唯一实际生效的域） */
    WASM_FAULT_DOMAIN_GPIO   = 1,    /* GPIO 域（Wave3 预留） */
    WASM_FAULT_DOMAIN_I2C0   = 2,    /* I2C 总线 0（Wave3 预留） */
    WASM_FAULT_DOMAIN_I2C1   = 3,    /* I2C 总线 1（Wave3 预留） */
    WASM_FAULT_DOMAIN_SPI0   = 4,    /* SPI 总线 0（Wave3 预留） */
    WASM_FAULT_DOMAIN_CLOCK  = 5,    /* 时钟域（Wave3 预留） */
    WASM_FAULT_DOMAIN_COUNT          /* 容量哨兵；也是首个非法 ID */
} wasm_fault_domain_id_t;

typedef struct {
    uint32_t domain_id;         /* == wasm_fault_domain_id_t */
    bool     armed;             /* 是否参与故障注入；reset 后默认 true */
    uint32_t trigger_count;     /* 累计触发计数（Wave3 累加） */
} wasm_fault_domain_t;

/* domain_id >= WASM_FAULT_DOMAIN_COUNT → get_config NULL / arm INVALID_ARG /
 * trigger_count 0（sentinel，不读未初始化内存）。 */
wink_sim_faults_t *pal_wasm_get_domain_config(uint32_t domain_id);
wink_status_t      pal_wasm_arm_fault_domain(uint32_t domain_id, bool armed);
uint32_t           pal_wasm_get_domain_trigger_count(uint32_t domain_id);

/* 重置所有故障域到 armed=true / trigger_count=0（reset_physical 内调用）。 */
void pal_wasm_reset_fault_domains(void);

#endif /* PAL_WASM_INTERNAL_H */
