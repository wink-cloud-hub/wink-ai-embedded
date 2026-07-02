/**
 * @file pal_osal_wasm.c
 * @brief Wasm 仿真端 PAL OSAL 适配（delay/tick/mutex）。
 *
 * 虚拟时钟 SSOT 架构（ADR-0003 决策 3 + ADR-0009 §4.1）：
 *   - `s_virtual_us` 是 wasm 侧的唯一时钟源，启动时为 0；
 *   - 唯一写入入口：`pal_wasm_advance_virtual_clock()`（导出给 JS Worker）；
 *   - 读出入口：`pal_os_get_us()` / `pal_os_get_ms()`，纯内存访问、零 JS 调用；
 *   - **架构红线**：`pal_os_sleep_ms/us()` 函数体内禁止调用 `pal_wasm_advance_virtual_clock()`，
 *     时钟推进完全由 JS Worker 在恢复 wasm 协程前驱动（避免双重步进 / 因果倒置）。
 *
 *   Asyncify 仍负责挂起 `pal_os_sleep_ms/us` 等待 JS 端定时器；恢复时 JS 端先调
 *   `pal_wasm_advance_virtual_clock(elapsed_us)`，再返回控制权给 wasm。
 */
#include "pal_osal.h"
#include "wasm_bridge.h"
#include "pal_wasm_internal.h"
#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>

/* ─────────────────────────────────────────────────────────
 * 虚拟时钟（ADR-0003 决策 3 / ADR-0009 §4.1 / Wave2 P1 Task 6）
 * ───────────────────────────────────────────────────────── */

/* wasm 侧虚拟时钟唯一状态。BSS 初始化为 0。
 * 64 位无符号自然回绕 > 580 年，物理上仿真不可能在单次会话内溢出，但
 * 1000x 加速仿真 + CI 长跑（~200 天连续运行）有理论触顶风险。Task 6
 * 在 50% 量程处插入一次性早期警告（见 CLOCK_WARNING_THRESHOLD），让
 * JS 侧在真正回绕前提示用户重置仿真环境。 */
static uint64_t s_virtual_us = 0;

/* 一次性溢出预警标志。BSS 初始化为 false。
 * 跨过阈值后置 true 并保持，幂等：JS 侧只关心 false→true 边沿。 */
static bool s_clock_warning_fired = false;

/* 编译期保证时钟是 64 位（即便未来误改类型，编译即拒）。 */
_Static_assert(sizeof(s_virtual_us) == 8, "Virtual clock must be 64-bit");

/* 溢出预警阈值：UINT64 中点（约 292 年微秒），用 UINT64_C 宏避免被
 * 当成 32 位常量截断。50% 量程预留充足修复窗口。 */
#define CLOCK_WARNING_THRESHOLD (UINT64_C(0x8000000000000000))

/* 导出给 JS Worker 的步进接口。
 * EMSCRIPTEN_KEEPALIVE 保证符号不被 -O 级优化裁掉 + 自动加入 export 表。
 * 调用者：SimWorker.ts（Wave 2 Task 5）在恢复 wasm 协程前推进时钟。
 *
 * 预警逻辑：跨越 CLOCK_WARNING_THRESHOLD 时一次性置位 s_clock_warning_fired。
 * 故意不直接调用 JS 侧日志函数——避免在 Asyncify 恢复路径上引入重入风险；
 * 由 JS 侧每个 tick 边界轮询 pal_wasm_is_clock_warning_fired()。 */
EMSCRIPTEN_KEEPALIVE
void pal_wasm_advance_virtual_clock(uint64_t us) {
    s_virtual_us += us;

    if (s_virtual_us > CLOCK_WARNING_THRESHOLD && !s_clock_warning_fired) {
        s_clock_warning_fired = true;
    }
}

uint64_t pal_os_get_us(void) { return s_virtual_us; }
uint64_t pal_os_get_ms(void) { return s_virtual_us / 1000u; }

/* ─────────────────────────────────────────────────────────
 * 溢出预警 accessor（Wave2 P1 Task 6）。
 * 导出给 JS Worker：每个 tick 边界轮询，触发后 console.warn 一次。
 * KEEPALIVE 保证符号进入 Module exports；C 侧通过 pal_wasm_internal.h
 * 声明以便 wasm 单测引用。
 * ───────────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE
bool pal_wasm_is_clock_warning_fired(void) {
    return s_clock_warning_fired;
}

EMSCRIPTEN_KEEPALIVE
uint64_t pal_wasm_get_virtual_clock_us(void) {
    return s_virtual_us;
}

/* ─────────────────────────────────────────────────────────
 * Delay：仅做 Asyncify 异步挂起。SSOT 红线——不主动步进时钟。
 * 时钟推进的唯一来源是 JS Worker 在恢复执行前调用
 * pal_wasm_advance_virtual_clock()。
 * ───────────────────────────────────────────────────────── */

void pal_os_sleep_ms(uint32_t ms) {
    js_pal_os_sleep_ms(ms);            /* Asyncify 挂起，由 JS 唤醒；JS 侧负责步进时钟 */
}

void pal_os_busy_wait_us(uint32_t us) {
    js_pal_os_busy_wait_us(us);
}

/* 单线程 Wasm Worker 沙箱通常无锁竞争，互斥锁退化为无竞争实现 */
pal_os_mutex_t pal_os_mutex_create(void) { return (pal_os_mutex_t)1; }
wink_status_t pal_os_mutex_lock(pal_os_mutex_t mutex, uint32_t timeout_ms) {
    if (mutex == NULL) return WINK_ERR_INVALID_ARG;
    (void)timeout_ms;
    return WINK_OK;
}
wink_status_t pal_os_mutex_unlock(pal_os_mutex_t mutex) {
    if (mutex == NULL) return WINK_ERR_INVALID_ARG;
    return WINK_OK;
}
void pal_os_mutex_destroy(pal_os_mutex_t mutex) { (void)mutex; }

/* Phase 5 Task 5-4：wasm 无硬件复位/WDT 语义。reset reason 恒 UNKNOWN；WDT UNSUPPORTED
 *（直至确立浏览器侧 watchdog 策略）。真挂死/CPU 卡死靠宿主（浏览器/容器）兜底，不由本层保证。 */
pal_os_reset_reason_t pal_os_get_reset_reason(void) { return PAL_OS_RESET_REASON_UNKNOWN; }
/* ADR-0010：wasm 无持久化复位计数语义，恒 0 / no-op */
uint32_t pal_os_get_abnormal_boot_count(void) { return 0; }
void pal_os_set_abnormal_boot_count(uint32_t count) { (void)count; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_init(uint32_t timeout_ms) { (void)timeout_ms; return WINK_ERR_UNSUPPORTED; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_feed(void) { return WINK_ERR_UNSUPPORTED; }

/* ─────────────────────────────────────────────────────────
 * 临界区（task/ISR 双入口显式分流, ADR-0016）
 * Wasm 单线程沙箱：语义等价（都是 no-op），但通过 s_sim_in_isr 强校验
 * 调用方使用了正确入口——Debug 构建下入口误用立即命中 assert。
 * ───────────────────────────────────────────────────────── */

#include <assert.h>

static bool s_sim_in_isr = false;
static bool s_sim_in_pt = false;

void pal_os_set_sim_isr_context(bool in_isr) { s_sim_in_isr = in_isr; }
bool pal_os_in_sim_isr_context(void) { return s_sim_in_isr; }

void pal_os_set_sim_pt_context(bool in_pt) { s_sim_in_pt = in_pt; }
bool pal_os_in_sim_pt_context(void) { return s_sim_in_pt; }
bool wink_pt_in_context(void) { return s_sim_in_pt; }

uint32_t pal_os_critical_enter(void) {
    assert(!s_sim_in_isr && "pal_os_critical_enter called from ISR context; use pal_os_critical_enter_isr (ADR-0016)");
    return 0;
}

void pal_os_critical_exit(uint32_t key) {
    (void)key;
    assert(!s_sim_in_isr && "pal_os_critical_exit called from ISR context (ADR-0016)");
}

uint32_t pal_os_critical_enter_isr(void) {
    assert(s_sim_in_isr && "pal_os_critical_enter_isr called from task context; use pal_os_critical_enter (ADR-0016)");
    return 0;
}

void pal_os_critical_exit_isr(uint32_t key) {
    (void)key;
    assert(s_sim_in_isr && "pal_os_critical_exit_isr called from task context (ADR-0016)");
}

/* ─────────────────────────────────────────────────────────
 * Task 创建（WASM 单线程仿真降级实现）
 * ───────────────────────────────────────────────────────── */

wink_status_t pal_os_task_create(
    void (*func)(void* arg),
    const char* name,
    uint32_t stack_depth,
    void* arg,
    int32_t priority,
    pal_os_core_id_t core_id,
    pal_os_task_handle_t* task_handle
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

void pal_os_task_delete(pal_os_task_handle_t task_handle) {
    (void)task_handle;  /* single-threaded WASM: no-op */
}

/* ─────────────────────────────────────────────────────────
 * 环形缓冲区 (WASM 纯内存实现，单线程无并发)
 * ───────────────────────────────────────────────────────── */

struct pal_os_ringbuf {
    uint8_t* buffer;
    uint32_t size;
    volatile uint32_t head;
    volatile uint32_t tail;
};

pal_os_ringbuf_handle_t pal_os_ringbuf_create(uint32_t size) {
    struct pal_os_ringbuf* rb;

    /* Size must be power of 2 (API contract) */
    if ((size & (size - 1)) != 0) {
        return NULL;
    }

    rb = malloc(sizeof(struct pal_os_ringbuf));
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

wink_status_t pal_os_ringbuf_push(
    pal_os_ringbuf_handle_t rb,
    const void* data,
    uint32_t size
) {
    uint32_t i;
    const uint8_t* src = (const uint8_t*)data;

    if (rb == NULL || data == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    if (pal_os_ringbuf_used(rb) + size > rb->size) {
        return WINK_ERR_FULL;
    }

    for (i = 0; i < size; i++) {
        rb->buffer[rb->head & (rb->size - 1)] = src[i];
        rb->head++;
    }

    return WINK_OK;
}

wink_status_t pal_os_ringbuf_pop(
    pal_os_ringbuf_handle_t rb,
    void* data,
    uint32_t size
) {
    uint32_t i;
    uint8_t* dst = (uint8_t*)data;

    if (rb == NULL || data == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    if (pal_os_ringbuf_used(rb) < size) {
        return WINK_ERR_EMPTY;
    }

    for (i = 0; i < size; i++) {
        dst[i] = rb->buffer[rb->tail & (rb->size - 1)];
        rb->tail++;
    }

    return WINK_OK;
}

uint32_t pal_os_ringbuf_used(pal_os_ringbuf_handle_t rb) {
    if (rb == NULL) {
        return 0;
    }
    return rb->head - rb->tail;
}

void pal_os_ringbuf_destroy(pal_os_ringbuf_handle_t rb) {
    if (rb == NULL) {
        return;
    }

    free(rb->buffer);
    free(rb);
}
