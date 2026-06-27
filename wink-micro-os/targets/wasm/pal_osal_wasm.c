/**
 * @file pal_osal_wasm.c
 * @brief Wasm 仿真端 PAL OSAL 适配（delay/tick/mutex）。
 *        Asyncify 挂起在 js_pal_delay_ms；虚拟时钟为 ADR-0003 决策3 路标（暂用 JS 墙钟）。
 */
#include "pal_osal.h"
#include "wasm_bridge.h"

void pal_delay_ms(uint32_t ms) {
    js_pal_delay_ms(ms);            /* Asyncify 挂起，由 JS 唤醒 */
}

void pal_delay_us(uint32_t us) {
    js_pal_delay_us(us);
}

uint64_t pal_get_ms(void) { return js_pal_get_ms(); }
uint64_t pal_get_us(void) { return js_pal_get_us(); }

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
