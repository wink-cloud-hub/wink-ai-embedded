/**
 * @file pal_osal_wasm.c
 * @brief Wasm 仿真端 PAL OSAL 适配（delay/tick/mutex）。
 *
 * 虚拟时钟 SSOT 架构（ADR-0003 决策 3 + ADR-0009 §4.1）：
 *   - `s_virtual_us` 是 wasm 侧的唯一时钟源，启动时为 0；
 *   - 唯一写入入口：`pal_wasm_advance_virtual_clock()`（导出给 JS Worker）；
 *   - 读出入口：`pal_get_us()` / `pal_get_ms()`，纯内存访问、零 JS 调用；
 *   - **架构红线**：`pal_delay_ms/us()` 函数体内禁止调用 `pal_wasm_advance_virtual_clock()`，
 *     时钟推进完全由 JS Worker 在恢复 wasm 协程前驱动（避免双重步进 / 因果倒置）。
 *
 *   Asyncify 仍负责挂起 `pal_delay_ms/us` 等待 JS 端定时器；恢复时 JS 端先调
 *   `pal_wasm_advance_virtual_clock(elapsed_us)`，再返回控制权给 wasm。
 */
#include "pal_osal.h"
#include "wasm_bridge.h"
#include "pal_wasm_internal.h"
#include <emscripten.h>
#include <stdint.h>

/* ─────────────────────────────────────────────────────────
 * 虚拟时钟（ADR-0003 决策 3 / ADR-0009 §4.1）
 * ───────────────────────────────────────────────────────── */

/* wasm 侧虚拟时钟唯一状态。BSS 初始化为 0。
 * 64 位无符号自然回绕 > 580 年，仿真不可能溢出。 */
static uint64_t s_virtual_us = 0;

/* 导出给 JS Worker 的步进接口。
 * EMSCRIPTEN_KEEPALIVE 保证符号不被 -O 级优化裁掉 + 自动加入 export 表。
 * 调用者：SimWorker.ts（Wave 2 Task 5）在恢复 wasm 协程前推进时钟。 */
EMSCRIPTEN_KEEPALIVE
void pal_wasm_advance_virtual_clock(uint64_t us) {
    s_virtual_us += us;
}

uint64_t pal_get_us(void) { return s_virtual_us; }
uint64_t pal_get_ms(void) { return s_virtual_us / 1000u; }

/* ─────────────────────────────────────────────────────────
 * Delay：仅做 Asyncify 异步挂起。SSOT 红线——不主动步进时钟。
 * 时钟推进的唯一来源是 JS Worker 在恢复执行前调用
 * pal_wasm_advance_virtual_clock()。
 * ───────────────────────────────────────────────────────── */

void pal_delay_ms(uint32_t ms) {
    js_pal_delay_ms(ms);            /* Asyncify 挂起，由 JS 唤醒；JS 侧负责步进时钟 */
}

void pal_delay_us(uint32_t us) {
    js_pal_delay_us(us);
}

/* 单线程 Wasm Worker 沙箱通常无锁竞争，互斥锁退化为无竞争实现 */
pal_mutex_t pal_mutex_create(void) { return (pal_mutex_t)1; }
wink_status_t pal_mutex_lock(pal_mutex_t mutex, uint32_t timeout_ms) {
    if (mutex == NULL) return WINK_ERR_INVALID_ARG;
    (void)timeout_ms;
    return WINK_OK;
}
wink_status_t pal_mutex_unlock(pal_mutex_t mutex) {
    if (mutex == NULL) return WINK_ERR_INVALID_ARG;
    return WINK_OK;
}
void pal_mutex_destroy(pal_mutex_t mutex) { (void)mutex; }

/* Phase 5 Task 5-4：wasm 无硬件复位/WDT 语义。reset reason 恒 UNKNOWN；WDT UNSUPPORTED
 *（直至确立浏览器侧 watchdog 策略）。真挂死/CPU 卡死靠宿主（浏览器/容器）兜底，不由本层保证。 */
pal_reset_reason_t pal_get_reset_reason(void) { return PAL_RESET_REASON_UNKNOWN; }
/* ADR-0010：wasm 无持久化复位计数语义，恒 0 / no-op */
uint32_t pal_get_abnormal_boot_count(void) { return 0; }
void pal_set_abnormal_boot_count(uint32_t count) { (void)count; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_watchdog_init(uint32_t timeout_ms) { (void)timeout_ms; return WINK_ERR_UNSUPPORTED; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_watchdog_feed(void) { return WINK_ERR_UNSUPPORTED; }

uint32_t pal_critical_enter(void) {
    return 0;
}

void pal_critical_exit(uint32_t key) {
    (void)key;
}

/* ─────────────────────────────────────────────────────────
 * Task 创建（WASM 单线程仿真降级实现）
 * ───────────────────────────────────────────────────────── */

wink_status_t pal_task_create(
    void (*func)(void* arg),
    const char* name,
    uint32_t stack_depth,
    void* arg,
    int32_t priority,
    pal_core_id_t core_id,
    pal_task_handle_t* task_handle
) {
    /* Single-threaded WASM sandbox: no true concurrency.
     * We call the function immediately as a degenerate case.
     * For Asyncify micro-task scheduling, integration would happen here.
     */
    (void)name; (void)stack_depth; (void)priority;
    (void)core_id; (void)task_handle;

    func(arg);
    return WINK_OK;
}

/* ─────────────────────────────────────────────────────────
 * 环形缓冲区 (WASM 纯内存实现，单线程无并发)
 * ───────────────────────────────────────────────────────── */

struct pal_ringbuf {
    uint8_t* buffer;
    uint32_t size;
    volatile uint32_t head;
    volatile uint32_t tail;
};

pal_ringbuf_handle_t pal_ringbuf_create(uint32_t size) {
    struct pal_ringbuf* rb;

    /* Size must be power of 2 (API contract) */
    if ((size & (size - 1)) != 0) {
        return NULL;
    }

    rb = malloc(sizeof(struct pal_ringbuf));
    if (rb == NULL) {
        return NULL;
    }

    rb->buffer = malloc(size);
    if (rb->buffer == NULL) {
        free(rb);
        return NULL;
    }

    rb->size = size;
    rb->head = 0;
    rb->tail = 0;

    return rb;
}

wink_status_t pal_ringbuf_push(
    pal_ringbuf_handle_t rb,
    const void* data,
    uint32_t size
) {
    uint32_t i;
    const uint8_t* src = (const uint8_t*)data;

    if (rb == NULL || data == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    if (pal_ringbuf_used(rb) + size > rb->size) {
        return WINK_ERR_FULL;
    }

    for (i = 0; i < size; i++) {
        rb->buffer[rb->head & (rb->size - 1)] = src[i];
        rb->head++;
    }

    return WINK_OK;
}

wink_status_t pal_ringbuf_pop(
    pal_ringbuf_handle_t rb,
    void* data,
    uint32_t size
) {
    uint32_t i;
    uint8_t* dst = (uint8_t*)data;

    if (rb == NULL || data == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    if (pal_ringbuf_used(rb) < size) {
        return WINK_ERR_EMPTY;
    }

    for (i = 0; i < size; i++) {
        dst[i] = rb->buffer[rb->tail & (rb->size - 1)];
        rb->tail++;
    }

    return WINK_OK;
}

uint32_t pal_ringbuf_used(pal_ringbuf_handle_t rb) {
    if (rb == NULL) {
        return 0;
    }
    return rb->head - rb->tail;
}

void pal_ringbuf_destroy(pal_ringbuf_handle_t rb) {
    if (rb == NULL) {
        return;
    }

    free(rb->buffer);
    free(rb);
}
