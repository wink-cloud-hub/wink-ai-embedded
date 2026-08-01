#ifndef DAL_BUTTON_BAL_H
#define DAL_BUTTON_BAL_H

/**
 * @file dal_button_bal.h
 * @brief BAL-internal button APIs — NOT part of the public DAL frozen surface.
 *
 * These APIs are used exclusively by the BAL layer (IRQ daemon, event backend
 * management). They are separated from dal_button.h to prevent App/AI from
 * mistakenly depending on them as stable public ABI.
 *
 * Evaluation review reference:
 *   2026-07-30-dal-type-semantic-and-function-sufficiency-review §3.1, §4 追加项 6a
 *
 * Include this header only from BAL-layer code. App code should only include
 * dal_button.h (the public frozen surface).
 */

#include "dal_button.h"  /* dal_button_t, dal_button_backend_t, etc. */

#ifdef __cplusplus
extern "C" {
#endif

/* ── BAL event-backend selector (S3+, ADR-0031) ──────────────────────
 *
 * dal_button_backend_t enum is defined in dal_button.h (it is part of
 * dal_button_t struct layout), but the APIs below that operate on it
 * are BAL-internal only.
 */

/**
 * @brief 设置 BAL 事件后端类型（BAL 调用；DAL 内部仅记录，作为 ISR 分派条件）。
 *
 * BAL 在 arm/disarm IRQ 路径时通知 DAL 当前的事件后端，DAL 共享 ISR thunk
 * 会依此决定是否要 (a) 设置 irq_pending 并 (b) 调用全局 hook。
 *
 * @param dev     Button instance (NULL-safe：NULL 直接返回).
 * @param backend DAL_BUTTON_BACKEND_{NONE, POLL, IRQ}.
 *
 * @note API Contract:
 *   - Thread-safe: No (task context only; must not race with deinit)
 *   - ISR-safe: No
 *   - Callback-context: N/A
 *   - Stability: BAL-internal; NOT part of the public DAL frozen surface.
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
 *
 * @note API Contract:
 *   - Thread-safe: No; ISR-safe: No.
 *   - Stability: BAL-internal.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_enable_gpio_isr(dal_button_t *dev);

/**
 * @brief 禁用共享 GPIO ISR（refcount 语义：counter 与 IRQ 后端都 off 时才卸载硬件）。
 *
 * 若 counter 仍在运行或事件后端仍是 IRQ，则本调用只减引用不动硬件；
 * 只有两者都关闭时才真正 disable + synchronize。
 * NULL-safe / 未 init safe：任何异常输入返回 no-op。
 *
 * @note API Contract:
 *   - Thread-safe: No; ISR-safe: No.
 *   - Stability: BAL-internal.
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
 *
 * @note API Contract:
 *   - Thread-safe: No (task context only)
 *   - ISR-safe: No
 *   - Stability: BAL-internal.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_consume_irq_pending(dal_button_t *dev,
                                             bool *out_was_pending);

/**
 * @brief BAL-internal hook: ISR-safe notify callback invoked by the shared
 *        GPIO ISR thunk after signalling irq_pending.
 *
 * BAL registers a single process-global hook (typically "give the daemon
 * wake semaphore") via dal_button_set_irq_hook(). The DAL ISR calls
 * this hook ONLY when the button's `event_backend == DAL_BUTTON_BACKEND_IRQ`.
 * DAL never depends on BAL; the hook is a function pointer supplied at BAL
 * init time so the layering stays DAL-below-BAL.
 *
 * @param ctx Opaque context provided at set_irq_hook time (typically
 *            NULL — the BAL daemon uses a file-scope singleton sem).
 *
 * ISR contract: must be ISR-safe (no LOG, no blocking, use *_isr sem-give).
 * Note: typedef dal_button_irq_notify_hook_t is defined in dal_button.h.
 */

/**
 * @brief 注册进程级 IRQ 通知 hook（BAL 使用，DAL 侧仅回调）。
 *
 * 由 BAL 在启动 daemon 时调一次；hook 在 GPIO ISR 上下文中被调用（只在
 * dev->event_backend == DAL_BUTTON_BACKEND_IRQ 时），必须 ISR-safe——
 * 通常做法是 `pal_os_sem_give_isr(daemon_sem)`。
 *
 * @param fn   Hook 函数（NULL = 取消注册）。
 * @param ctx  Hook 调用时原样传入。
 *
 * @note API Contract:
 *   - Thread-safe: No (call from task context during BAL init)
 *   - ISR-safe: No
 *   - Stability: BAL-internal.
 */
void dal_button_set_irq_hook(dal_button_irq_notify_hook_t fn, void *ctx);

#ifdef __cplusplus
}
#endif

/* ── Compile-time pruning stubs for BAL-internal APIs ──────────────────
 * These mirror the dal_button.h stub pattern: when WINK_USE_BUTTON=OFF,
 * BAL code that includes this header still compiles but gets a friendly
 * error message at link time instead of opaque "undefined reference".
 */
#if !defined(WINK_USE_BUTTON) || !WINK_USE_BUTTON
#ifndef WINK_BUTTON_DISABLED_MSG
#define WINK_BUTTON_DISABLED_MSG \
    "Button driver not enabled; add a \"button\" device to wink-app.json " \
    "(or set -DWINK_USE_BUTTON=ON)."
#endif
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

#endif /* DAL_BUTTON_BAL_H */
