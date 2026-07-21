#ifndef DAL_BUTTON_H
#define DAL_BUTTON_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 连续一致采样阈值：达此计数后稳定态翻转（3 × tick 间隔 ≈ 30ms @ 10ms tick） */
#define DAL_BUTTON_DEBOUNCE_THRESHOLD 3

/** @brief 默认长按判定时间（毫秒） */
#define DAL_BUTTON_DEFAULT_LONG_PRESS_MS 3000u

/**
 * @brief 按钮事件类型（语义级事件，由 dal_button_poll() 在去抖/长按状态机中派发）
 */
typedef enum {
    DAL_BUTTON_EVT_PRESS       = 0,  /**< 稳定按下（去抖完成，从释放→按下的瞬间） */
    DAL_BUTTON_EVT_RELEASE     = 1,  /**< 稳定释放（从按下→释放的瞬间） */
    DAL_BUTTON_EVT_LONG_PRESS  = 2,  /**< 长按触发（按住持续达 long_press_ms 后触发一次，不重复） */
} dal_button_event_t;

/**
 * @brief 按钮事件回调类型（在 dal_button_poll() 的 task 上下文同步调用，非 ISR）
 * @param evt  事件类型
 * @param ctx  用户在 dal_button_on_event() 注册时提供的 opaque 指针
 * @note  回调运行在调用 poll 的上下文（通常是 app_loop / 主任务），非 ISR；
 *        允许调用 WINK_BLOCKING API（如 printf）但建议保持短小。
 */
typedef void (*dal_button_event_cb)(dal_button_event_t evt, void *ctx);

typedef uint8_t dal_button_pull_t;

enum {
    DAL_BUTTON_PULL_AUTO = 0, /**< active_low → UP，否则 DOWN（默认） */
    DAL_BUTTON_PULL_UP   = 1,
    DAL_BUTTON_PULL_DOWN = 2,
    DAL_BUTTON_PULL_NONE = 3,
};

/**
 * @brief 按钮配置结构体（标准化 config_t 模式，便于 Codegen 设备树生成）
 *
 * Phase 2 标准化：所有 DAL 外设统一采用 dal_xxx_config_t + dal_xxx_init(dev, cfg) 模式。
 * 便于代码生成器（app_codegen.py）输出结构化的初始化数据。
 *
 * 成员按对齐降序排列（uint16_t → bool）：自然对齐，无填充。
 */
typedef struct {
    const char *owner;       /* 资源占用 owner 静态字符串（device_tree 实例名，静态存储） */
    uint16_t pin;            /* 逻辑 GPIO 引脚 */
    bool active_low;         /* true: 按下为低电平（常见上拉按钮）；false: 按下为高电平 */
    dal_button_pull_t pull;  /* 0 = AUTO；ADR-0034 */
} dal_button_config_t;

/**
 * @brief 按钮逻辑句柄（POD，ADR-0004 静态分发）
 *
 * 内嵌 config 副本，便于：
 *   1. Flash 动态覆写（ADR-0008）：从 Flash blob 读取 → 写入 config → dal_xxx_apply_override
 *   2. 运行时诊断：可直接打印当前生效的配置
 *
 * Wave 3 扩展字段（events + ISR counter）加在末尾，保留原有字段顺序和偏移
 * 以兼容直接访问 stable_pressed 的现有测试/代码。
 */
typedef struct {
    dal_button_config_t config; /* 配置副本（pin, active_low），由 init 从 cfg 拷贝 */
    bool stable_pressed;     /* 去抖后的稳定按下状态 */
    bool last_reported;      /* 上次 was_pressed 报告过的状态（边沿消抖） */
    bool initialized;        /* init 成功后置 true */
    uint8_t debounce_counter;/* 连续一致采样计数器 */
    uint8_t debounce_threshold; /* 稳定态翻转所需的连续一致采样数（≥1；由 dal_button_set_debounce_ms 调整，默认 DAL_BUTTON_DEBOUNCE_THRESHOLD） */

    /* ── Wave 3: event callback state ── */
    dal_button_event_cb event_cb;     /* 事件回调（NULL=不派发） */
    void *event_cb_ctx;               /* 回调 ctx */
    bool long_press_fired;            /* 本次按下周期内是否已触发过 LONG_PRESS（防止重复） */
    bool prev_pressed_for_event;      /* 上一次 poll 时的稳定态（边沿检测，与 last_reported 独立） */
    uint32_t long_press_ms;           /* 长按判定阈值（毫秒），默认 DAL_BUTTON_DEFAULT_LONG_PRESS_MS */
    uint64_t press_start_ms;          /* 当前按下周期的起始时间（pal_os_get_ms() 时间戳） */

    /* ── Wave 3: ISR edge counter ── */
    bool isr_counter_enabled;          /* dal_button_enable_isr_counter() 置 true */
    volatile uint32_t edge_count;      /* ISR 累计触发次数（volatile: ISR 写/poll 上下文读） */

    /* ── Wave 4 (S3): BAL IRQ event backend fan-out (ADR-0031) ──
     * Shared GPIO ISR dispatcher: the same underlying ISR routes to
     * (a) edge counter (isr_counter_enabled) and/or (b) BAL IRQ daemon
     * (event_backend == DAL_BUTTON_BACKEND_IRQ). Refcounted enable/disable
     * so counter and event backend can coexist on one pin. */
    uint8_t  event_backend;            /* DAL_BUTTON_BACKEND_{NONE,POLL,IRQ} (default NONE) */
    bool     gpio_isr_registered;      /* true iff the shared thunk is currently installed */
    volatile bool irq_pending;         /* set by ISR, cleared by dal_button_consume_irq_pending */
} dal_button_t;

/**
 * @brief BAL event-backend selector (S3+, ADR-0031).
 *
 * Which mechanism the BAL layer uses to detect edges before running
 * debounce and dispatching WINK_EVENT_BUTTON_*. Independent from
 * `isr_counter_enabled` — both can be true at once and share the same
 * underlying GPIO ISR thunk.
 */
typedef enum {
    DAL_BUTTON_BACKEND_NONE = 0, /**< No BAL event tracking on this button. */
    DAL_BUTTON_BACKEND_POLL = 1, /**< BAL soft-poll (dal_button_poll from a periodic tick). */
    DAL_BUTTON_BACKEND_IRQ  = 2, /**< BAL GPIO IRQ + debounce timer (ESP32 only). */
} dal_button_backend_t;

/**
 * @brief BAL-internal hook: ISR-safe notify callback invoked by the shared
 *        GPIO ISR thunk after signalling irq_pending.
 *
 * BAL registers a single process-global hook (typically "give the daemon
 * wake semaphore") via dal_button_set_irq_notify_hook(). The DAL ISR calls
 * this hook ONLY when the button's `event_backend == DAL_BUTTON_BACKEND_IRQ`.
 * DAL never depends on BAL; the hook is a function pointer supplied at BAL
 * init time so the layering stays DAL-below-BAL.
 *
 * @param ctx Opaque context provided at set_irq_notify_hook time (typically
 *            NULL — the BAL daemon uses a file-scope singleton sem).
 *
 * ISR contract: must be ISR-safe (no LOG, no blocking, use *_isr sem-give).
 */
typedef void (*dal_button_irq_notify_hook_t)(void *ctx);

/**
 * @brief 初始化按钮：校验引脚、按 active_low 配置上拉/下拉输入、置 initialized。
 *
 * Phase 2 标准化：统一采用 config_t 模式，简化 Codegen 设备树生成。
 * 旧 API（pin + active_low 分离参数）已迁移至此。
 *
 * @param dev 按钮实例句柄
 * @param cfg 配置结构体指针（内部深拷贝到 dev->config）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL。
 *   - Blocking: No。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(NULL) / 透传 PAL 错误。
 *   - Postconditions: WINK_OK 时 dev->initialized=true；GPIO 方向已配置（真机）；
 *                     cfg 的内容已深拷贝到 dev->config。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_init(dal_button_t *dev, const dal_button_config_t *cfg);

/**
 * @brief 每 tick 采样并跑计数式去抖状态机（非阻塞）。
 * @note 由 App app_loop 每周期调用一次；驱动内部维护计数器，不对外暴露 poll 接口。
 *       去抖阈值 DAL_BUTTON_DEBOUNCE_THRESHOLD（≈30ms @ 10ms tick）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_button_init() 已成功。
 *   - Blocking: No。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_poll(dal_button_t *dev);

/**
 * @brief 读取去抖后的稳定按下状态
 * @param out_pressed 输出：true=已按下；false=未按下
 * @note API Contract:
 *   - Preconditions: dev/out_pressed 非 NULL；dal_button_init() 已成功。
 *   - Blocking: No。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_is_pressed(const dal_button_t *dev, bool *out_pressed);

/**
 * @brief 检测「按下」边沿事件（按下瞬间触发一次，读后清）。
 * @param out_was_pressed 输出：true=自上次调用后发生了按下事件；false=无新按下事件
 * @note 与 is_pressed 的区别：was_pressed 只在稳定态从「未按下」→「按下」时返回 true 一次，
 *       适用于触发单次动作（如切换模式）；is_pressed 返回当前持续状态，适用于按住动作。
 * @note API Contract:
 *   - Preconditions: dev/out_was_pressed 非 NULL；dal_button_init() 已成功。
 *   - Blocking: No。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_was_pressed(dal_button_t *dev, bool *out_was_pressed);

/* ── Wave 3: Event callback (poll-driven, non-ISR) ──────── */

/**
 * @brief 注册按钮事件回调（PRESS / RELEASE / LONG_PRESS）。
 *
 * 回调在 dal_button_poll() 上下文中**同步**调用（不是 ISR！），驱动方可安全
 * 调用 printf / WINK_BLOCKING API。事件由 poll 内部的去抖和长按状态机
 * 检测：PRESS 在稳定按下边沿触发一次，RELEASE 在稳定释放边沿触发一次，
 * LONG_PRESS 在按住达到 long_press_ms 后触发一次（按住期间不重复）。
 *
 * @param dev 按钮实例
 * @param cb  事件回调（传入 NULL 注销）
 * @param ctx Opaque 指针，调用 cb 时原样转发
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED
 * @note   线程安全：只在 poll 上下文调用；enable/disable 必须在 poll 启动
 *         之前完成（或在同一 task 中串行调用）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_on_event(dal_button_t *dev, dal_button_event_cb cb, void *ctx);

/**
 * @brief 配置长按判定时间。
 * @param ms 长按毫秒阈值（必须 > 0；默认 DAL_BUTTON_DEFAULT_LONG_PRESS_MS = 3000ms）
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED
 * @note   只影响 LONG_PRESS 事件派发；不改变去抖行为。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_set_long_press_ms(dal_button_t *dev, uint32_t ms);

/**
 * @brief 配置去抖窗口（毫秒）。
 *
 * 按当前 poll 周期（wink-app.json 中 auto_poll_ms，默认 10 ms 与 runtime tick 对齐）
 * 换算为「连续一致采样次数」阈值：threshold = max(1, ms / 10)。真实换算发生在
 * BAL/App 侧对本 API 的调用点：DAL 无法感知 poll 周期，因此上层若使用非 10 ms
 * 周期，应先按自己的周期折算再传入等效 ms。
 *
 * 语义：ms=0 不合法（去抖关闭请勿调用此 API；保留 DAL 默认 30 ms ≈ 3 samples）。
 * ms<10 ms 会被 clamp 到 threshold=1（等价单次采样，最短去抖）。
 *
 * @param ms 去抖毫秒（>0；建议 ≥10ms 以匹配默认 tick）
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED
 * @note   只影响后续 poll 的稳定态判定；不影响长按阈值和事件回调。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_set_debounce_ms(dal_button_t *dev, uint32_t ms);

/* ── Wave 3: ISR edge counter (hardware-interrupt driven) ─ */

/**
 * @brief 启用 GPIO 边沿中断计数器。
 *
 * 注册 ANY_EDGE GPIO ISR，每次触发时递增 dev->edge_count（volatile）。
 * 在 host/wasm 上可通过 pal_host_trigger_gpio_interrupt(pin) 模拟触发；
 * 在无 GPIO 中断支持的 target 上返回 WINK_ERR_UNSUPPORTED（不崩，仅降级）。
 *
 * 第二次调用（重复启用）是幂等的：直接返回 WINK_OK，不重复注册。
 *
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED / WINK_ERR_UNSUPPORTED
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_enable_isr_counter(dal_button_t *dev);

/**
 * @brief 读取 ISR 边沿累计计数。
 * @param out_count 输出：自 enable/上次 reset 以来的 ISR 触发次数
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED
 * @note   未 enable 时 out_count = 0（返回 WINK_OK）；volatile 读安全，无需临界区。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_get_edge_count(const dal_button_t *dev, uint32_t *out_count);

/**
 * @brief 原子清零 ISR 边沿计数（在临界区内完成，避免丢脉冲）。
 *
 * 使用 PAL_CRITICAL_SECTION 关中断做读-改-写，保证清零瞬间的 ISR 触发
 * 不会被丢失（ISR 在临界区内被 PENDING，恢复后写入新的计数 1）。
 *
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_reset_edge_count(dal_button_t *dev);

/**
 * @brief 反初始化按钮：禁用 ISR 计数器（若已启用）、释放 GPIO 资源、置 initialized=false。
 * @note 可在未 init 的 dev 上安全调用（直接返回 WINK_OK，no-op）。
 * @return WINK_OK
 */
wink_status_t dal_button_deinit(dal_button_t *dev);

/* ── Wave 4 (S3): BAL event backend selector + shared GPIO ISR fan-out ── */

/**
 * @brief 设置 BAL 事件后端类型（BAL 调用；DAL 内部仅记录，作为 ISR 分派条件）。
 *
 * BAL 在 arm/disarm IRQ 路径时通知 DAL 当前的事件后端，DAL 共享 ISR thunk
 * 会依此决定是否要 (a) 设置 irq_pending 并 (b) 调用全局 hook。
 *
 * @param dev     Button instance (NULL-safe：NULL 直接返回).
 * @param backend DAL_BUTTON_BACKEND_{NONE, POLL, IRQ}.
 */
void dal_button_set_event_backend(dal_button_t *dev, uint8_t backend);

/**
 * @brief 启用共享 GPIO ISR（refcount 语义：counter 或 IRQ 后端任一启用时首次注册）。
 *
 * 与 dal_button_enable_isr_counter 共用同一底层 thunk；两者可并存（同一 pin
 * 上既做边沿计数又做 BAL IRQ 事件）。首次调用时向硬件注册 ANY_EDGE ISR；
 * 重复调用是幂等的。
 *
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED / WINK_ERR_UNSUPPORTED
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_enable_gpio_isr(dal_button_t *dev);

/**
 * @brief 禁用共享 GPIO ISR（refcount 语义：counter 与 IRQ 后端都 off 时才卸载硬件）。
 *
 * 若 counter 仍在运行或事件后端仍是 IRQ，则本调用只减引用不动硬件；
 * 只有两者都关闭时才真正 disable + synchronize。
 * NULL-safe / 未 init safe：任何异常输入返回 no-op。
 */
void dal_button_disable_gpio_isr(dal_button_t *dev);

/**
 * @brief 读并清 irq_pending 标志（task 上下文，临界区保护）。
 *
 * 由 BAL IRQ daemon 唤醒后调用扫描每个 slot，若 *out_was_pending == true
 * 表明自上次消费以来至少发生过一次边沿，然后立刻 arm 去抖定时器采稳定态。
 *
 * @param[out] out_was_pending  true = 曾经 pending（已清零）；false = 无 pending。
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_consume_irq_pending(dal_button_t *dev,
                                             bool *out_was_pending);

/**
 * @brief 注册进程级 IRQ 通知 hook（BAL 使用，DAL 侧仅回调）。
 *
 * 由 BAL 在启动 daemon 时调一次；hook 在 GPIO ISR 上下文中被调用（只在
 * dev->event_backend == DAL_BUTTON_BACKEND_IRQ 时），必须 ISR-safe——
 * 通常做法是 `pal_os_sem_give_isr(daemon_sem)`。
 *
 * @param fn   Hook 函数（NULL = 取消注册）。
 * @param ctx  Hook 调用时原样传入。
 */
void dal_button_set_irq_hook(dal_button_irq_notify_hook_t fn, void *ctx);

#ifdef __cplusplus
}
#endif

/* ── Compile-time pruning stubs (P2-1 2026-07-06) ──────────────────────
 * See dal_led.h header comment for rationale.
 */
#if !defined(WINK_USE_BUTTON) || !WINK_USE_BUTTON
#define WINK_BUTTON_DISABLED_MSG \
    "Button driver not enabled; add a \"button\" device to wink-app.json " \
    "(or set -DWINK_USE_BUTTON=ON)."
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_init(dal_button_t *dev, const dal_button_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_poll(dal_button_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_is_pressed(const dal_button_t *dev, bool *out_pressed);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_was_pressed(dal_button_t *dev, bool *out_was_pressed);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_on_event(dal_button_t *dev, dal_button_event_cb cb, void *ctx);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_set_long_press_ms(dal_button_t *dev, uint32_t ms);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_set_debounce_ms(dal_button_t *dev, uint32_t ms);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_enable_isr_counter(dal_button_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_get_edge_count(const dal_button_t *dev, uint32_t *out_count);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_reset_edge_count(dal_button_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG)
wink_status_t dal_button_deinit(dal_button_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG)
void dal_button_set_event_backend(dal_button_t *dev, uint8_t backend);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_enable_gpio_isr(dal_button_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG)
void dal_button_disable_gpio_isr(dal_button_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_consume_irq_pending(dal_button_t *dev,
                                             bool *out_was_pending);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG)
void dal_button_set_irq_hook(dal_button_irq_notify_hook_t fn, void *ctx);
#endif /* !WINK_USE_BUTTON */

#endif /* DAL_BUTTON_H */
