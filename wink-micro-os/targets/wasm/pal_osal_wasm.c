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
#include "wink_sim_scheduler.h"
#include "wink_trace.h"
#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/* Fault 锁存 / host_fault / 审计日志 已迁至 pal_wasm_fault.c（2026-07 wasm target 拆分）。
 * scheduler_run 通过 pal_wasm_fault_set_callbacks / pal_wasm_invoke_fault 走内部接口，
 * 不再直接 touch fault.c 的静态状态。 */

/* ─────────────────────────────────────────────────────────
 * 临界区/ISR/PT 上下文标志（ADR-0016 分流）
 * 前向声明在文件顶部，使 mutex_lock 等早期函数也能 assert。
 * 定义与 setter/getter 位于文件下方临界区段落。
 * ───────────────────────────────────────────────────────── */
static bool s_sim_in_isr = false;
static bool s_sim_in_pt = false;

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
    WASM_FAULT_GUARD_VOID();
    s_virtual_us += us;

    if (s_virtual_us > CLOCK_WARNING_THRESHOLD && !s_clock_warning_fired) {
        s_clock_warning_fired = true;
    }
}

EMSCRIPTEN_KEEPALIVE
uint64_t pal_os_get_us(void) { return s_virtual_us; }
EMSCRIPTEN_KEEPALIVE
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

static sim_ctx_t* s_main_ctx = NULL;

void pal_os_sleep_ms(uint32_t ms) {
    if (s_main_ctx == NULL) {
        /* Wasm legacy fallback (e.g. before scheduler starts or in legacy tests) */
        js_pal_os_sleep_ms(ms);
        return;
    }
    uint32_t cur = sim_scheduler_current_id();
    assert(cur != SIM_SCHED_NO_READY &&
           "pal_os_sleep_ms called from main thread while scheduler is active; "
           "tasks must sleep inside their fiber context.");
    sim_ctx_t* cur_ctx = sim_scheduler_current_ctx();
    assert(cur_ctx != NULL && "sim_scheduler_current_ctx returned NULL in task context");
    sim_scheduler_yield_timed(cur, pal_os_get_us(), (uint64_t)ms * 1000);
    /* 红线 15 反面契约：task 让出不改 s_current_task_id。 */
    sim_ctx_switch(cur_ctx, s_main_ctx);
}

void pal_os_busy_wait_us(uint32_t us) {
    js_pal_os_busy_wait_us(us);
}

/* ─────────────────────────────────────────────────────────
 * Mutex：协作调度下真正的 BLOCKED 路径实现（P0 E5-part2）
 *
 * 设计（ADR-0017 契约诚实 + 协作式确定性调度）：
 *   - 用静态 POD 池（与 WINK_SIM_MAX_TASKS 一致 8 槽）避免 malloc 失败路径
 *   - owner = WINK_SIM_MAX_TASKS 表示未持有；否则持锁 task 的 scheduler id
 *   - waiter 用内嵌环形 FIFO（WINK_SIM_MAX_TASKS 槽足够：task 总数上限）
 *   - lock：未持 → 拿锁；已持 → sim_scheduler_block + ctx_switch 回 main，
 *     被 resume 回来后查 timeout_fired（返 TIMEOUT）或 owner==self（返 OK）
 *   - unlock：FIFO 弹出一个 waiter resume；否则清 owner
 *   - destroy：必须未被持有（否则 INVALID_STATE，避免删除有等者的锁 UB）
 *
 * 注：wasm 单线程协作 + ADR-0014 单 vcore，无需 ISR 安全、递归锁、优先级反转
 * 处理。所有 mutx 操作均在 task 上下文，不会被异步打断。
 * ───────────────────────────────────────────────────────── */

#define WASM_MUTEX_POOL_SIZE  WINK_SIM_MAX_TASKS
#define WASM_MUTEX_NO_OWNER   (WINK_SIM_MAX_TASKS + 1)  /* task id 0..WINK_SIM_MAX_TASKS-1；SIM_SCHED_NO_READY=UINT32_MAX 为 main */

typedef struct {
    bool     used;                                /* 槽位分配标志 */
    uint32_t owner;                               /* WINK_SIM_MAX_TASKS (>=) 表未持 */
    uint32_t waiters[WINK_SIM_MAX_TASKS];         /* FIFO：等待队列 */
    uint8_t  w_head;
    uint8_t  w_tail;
    uint8_t  w_count;
} wasm_mutex_t;

static wasm_mutex_t s_mtx_pool[WASM_MUTEX_POOL_SIZE];

#define WASM_SEM_POOL_SIZE  WINK_SIM_MAX_TASKS

typedef struct {
    bool     used;
    uint32_t count;
    uint32_t waiters[WINK_SIM_MAX_TASKS];
    uint8_t  w_head;
    uint8_t  w_tail;
    uint8_t  w_count;
} wasm_sem_t;

static wasm_sem_t s_sem_pool[WASM_SEM_POOL_SIZE];

static wasm_sem_t* wasm_sem_from_handle(pal_os_sem_t h) {
    if (h == NULL) return NULL;
    uintptr_t idx = (uintptr_t)h - 1;
    if (idx >= WASM_SEM_POOL_SIZE) return NULL;
    wasm_sem_t* s = &s_sem_pool[idx];
    if (!s->used) return NULL;
    return s;
}

static void sq_push(wasm_sem_t* s, uint32_t tid) {
    if (s->w_count >= WINK_SIM_MAX_TASKS) return;
    s->waiters[s->w_tail] = tid;
    s->w_tail = (uint8_t)((s->w_tail + 1u) % WINK_SIM_MAX_TASKS);
    s->w_count++;
}

static bool sq_pop(wasm_sem_t* s, uint32_t* out) {
    if (s->w_count == 0) return false;
    *out = s->waiters[s->w_head];
    s->w_head = (uint8_t)((s->w_head + 1u) % WINK_SIM_MAX_TASKS);
    s->w_count--;
    return true;
}

static void sq_remove(wasm_sem_t* s, uint32_t tid) {
    if (s->w_count == 0) return;
    uint8_t w = 0, r = 0;
    uint8_t cnt = s->w_count;
    for (uint8_t i = 0; i < cnt; ++i) {
        uint32_t t = s->waiters[(s->w_head + i) % WINK_SIM_MAX_TASKS];
        if (t != tid) {
            s->waiters[(s->w_head + w) % WINK_SIM_MAX_TASKS] = t;
            w++;
        } else { r++; }
    }
    s->w_tail = (uint8_t)((s->w_head + w) % WINK_SIM_MAX_TASKS);
    s->w_count = (uint8_t)(s->w_count - r);
}

static wasm_mutex_t* wasm_mtx_from_handle(pal_os_mutex_t h) {
    if (h == NULL) return NULL;
    uintptr_t idx = (uintptr_t)h - 1;
    if (idx >= WASM_MUTEX_POOL_SIZE) return NULL;
    wasm_mutex_t* m = &s_mtx_pool[idx];
    if (!m->used) return NULL;
    return m;
}

static void wq_push(wasm_mutex_t* m, uint32_t tid) {
    if (m->w_count >= WINK_SIM_MAX_TASKS) return; /* 满——防御，不会发生因为 task 上限 */
    m->waiters[m->w_tail] = tid;
    m->w_tail = (uint8_t)((m->w_tail + 1u) % WINK_SIM_MAX_TASKS);
    m->w_count++;
}

static bool wq_pop(wasm_mutex_t* m, uint32_t* out) {
    if (m->w_count == 0) return false;
    *out = m->waiters[m->w_head];
    m->w_head = (uint8_t)((m->w_head + 1u) % WINK_SIM_MAX_TASKS);
    m->w_count--;
    return true;
}

static void wq_remove(wasm_mutex_t* m, uint32_t tid) {
    /* 从 FIFO 中线性查找并移除 tid（队列最多 WINK_SIM_MAX_TASKS=8，线性扫描可接受）。
     * 用于 BLOCKED 因超时唤醒时把自己从等待队列摘除，避免后续 unlock 错误 resume。 */
    if (m->w_count == 0) return;
    uint8_t r = 0, w = 0;
    uint8_t cnt = m->w_count;
    for (uint8_t i = 0; i < cnt; ++i) {
        uint32_t t = m->waiters[(m->w_head + i) % WINK_SIM_MAX_TASKS];
        if (t != tid) {
            m->waiters[(m->w_head + w) % WINK_SIM_MAX_TASKS] = t;
            w++;
        } else {
            r++;
        }
    }
    m->w_tail = (uint8_t)((m->w_head + w) % WINK_SIM_MAX_TASKS);
    m->w_count = (uint8_t)(m->w_count - r);
}

pal_os_mutex_t pal_os_mutex_create(void) {
    for (uint32_t i = 0; i < WASM_MUTEX_POOL_SIZE; ++i) {
        if (!s_mtx_pool[i].used) {
            wasm_mutex_t* m = &s_mtx_pool[i];
            m->used = true;
            m->owner = WASM_MUTEX_NO_OWNER;
            m->w_head = 0;
            m->w_tail = 0;
            m->w_count = 0;
            return (pal_os_mutex_t)(uintptr_t)(i + 1);
        }
    }
    return NULL; /* 池耗尽 */
}

wink_status_t pal_os_mutex_lock(pal_os_mutex_t mutex, uint32_t timeout_ms) {
    wasm_mutex_t* m = wasm_mtx_from_handle(mutex);
    if (m == NULL) return WINK_ERR_INVALID_ARG;

    /* ISR/pt 上下文调 mutex_lock 是编程错误——wasm 下 assert 拦截
     * （host 下 fiber 也不会在 ISR 里调锁，保持契约）。*/
    assert(!s_sim_in_isr && "pal_os_mutex_lock called from ISR context");

    uint32_t self = sim_scheduler_current_id();
    if (self == SIM_SCHED_NO_READY) {
        /* 在 main 上下文（scheduler 未启动或 main fiber）：直接尝试非阻塞拿锁，
         * 失败返 TIMEOUT（main 上下文不能 block——否则会阻塞调度器本身）。*/
        if (m->owner == WASM_MUTEX_NO_OWNER) {
            m->owner = self; /* 主上下文用 SIM_SCHED_NO_READY 作为 owner id——unlock 需识别 */
            return WINK_OK;
        }
        return WINK_ERR_TIMEOUT;
    }

    if (m->owner == WASM_MUTEX_NO_OWNER) {
        m->owner = self;
        return WINK_OK;
    }

    /* 递归锁（同 task 重入）：wasm 协作下可检测到，但 POSIX 递归锁需要
     * RECURSIVE 属性才允许。这里选择拒绝——返 BUSY 帮助发现 bug。*/
    if (m->owner == self) {
        return WINK_ERR_BUSY;
    }

    /* 不可用 → block 并让出 */
    uint64_t timeout_us = (timeout_ms == WINK_MUTEX_WAIT_FOREVER)
                           ? 0ULL
                           : (uint64_t)timeout_ms * 1000ULL;
    /* resource_id 用池索引 +1（≥1，避免 blocked_on=0 表"未阻塞"） */
    uint32_t resource_id = (uint32_t)((uintptr_t)mutex);
    wq_push(m, self);
    sim_scheduler_block(self, resource_id, pal_os_get_us(), timeout_us);
    sim_ctx_t* cur_ctx = sim_scheduler_current_ctx();
    assert(cur_ctx != NULL);
    sim_ctx_switch(cur_ctx, s_main_ctx);

    /* 恢复路径：被 unlock 唤醒 → owner 已转交给我；或超时 → timeout_fired=true */
    sim_task_t const* t = sim_scheduler_get(self);
    if (t->timeout_fired) {
        /* 超时：把自己从 waiter 队列摘出来，避免 unlock 误 resume 已超时的 task */
        wq_remove(m, self);
        return WINK_ERR_TIMEOUT;
    }
    /* resume 路径：unlock 把 owner 设为被唤醒 task（见下），这里直接验证 */
    if (m->owner != self) {
        /* 理论上不应发生——防御：返 IO 错误 */
        return WINK_ERR_IO;
    }
    return WINK_OK;
}

wink_status_t pal_os_mutex_unlock(pal_os_mutex_t mutex) {
    wasm_mutex_t* m = wasm_mtx_from_handle(mutex);
    if (m == NULL) return WINK_ERR_INVALID_ARG;

    uint32_t self = sim_scheduler_current_id();
    if (m->owner == WASM_MUTEX_NO_OWNER) {
        return WINK_ERR_INVALID_STATE; /* 未持有 */
    }
    /* 允许主上下文（SIM_SCHED_NO_READY）释放自己持的锁——对称性 */
    if (m->owner != self && !(m->owner == SIM_SCHED_NO_READY && self == SIM_SCHED_NO_READY)) {
        return WINK_ERR_PERMISSION; /* 非 owner 释放 */
    }

    /* FIFO 取第一个 waiter 转交 owner 并 resume */
    uint32_t waiter;
    if (wq_pop(m, &waiter)) {
        m->owner = waiter;
        sim_scheduler_resume(waiter);
    } else {
        m->owner = WASM_MUTEX_NO_OWNER;
    }
    return WINK_OK;
}

void pal_os_mutex_destroy(pal_os_mutex_t mutex) {
    wasm_mutex_t* m = wasm_mtx_from_handle(mutex);
    if (m == NULL) return;
    /* 有持锁者或有等待者时不允许 destroy——契约诚实，返回前未释放的指针将变成 wild */
    /* 注意：此接口无返回值（与 ESP32/host vSemaphoreDelete 对齐），非法调用直接
     * assert，避免静默 UB。*/
    assert(m->owner == WASM_MUTEX_NO_OWNER && m->w_count == 0 &&
           "pal_os_mutex_destroy: mutex still held or has waiters");
    m->used = false;
}

/* ─────────────────────────────────────────────────────────
 * 线程同步二值信号量（Semaphore）
 * ───────────────────────────────────────────────────────── */

pal_os_sem_t pal_os_sem_create(void) {
    for (uint32_t i = 0; i < WASM_SEM_POOL_SIZE; ++i) {
        if (!s_sem_pool[i].used) {
            wasm_sem_t* s = &s_sem_pool[i];
            s->used = true;
            s->count = 0;
            s->w_head = 0; s->w_tail = 0; s->w_count = 0;
            return (pal_os_sem_t)(uintptr_t)(i + 1);
        }
    }
    return NULL;
}

wink_status_t pal_os_sem_take(pal_os_sem_t sem, uint32_t timeout_ms) {
    wasm_sem_t* s = wasm_sem_from_handle(sem);
    if (s == NULL) return WINK_ERR_INVALID_ARG;
    assert(!pal_os_in_sim_isr_context() && "pal_os_sem_take called from ISR context");

    uint32_t self = sim_scheduler_current_id();
    uint64_t now = sim_scheduler_get_time();

    if (s->count > 0) {
        s->count = 0;
        return WINK_OK;
    }
    if (self == SIM_SCHED_NO_READY) {
        return WINK_ERR_TIMEOUT;
    }

    uint64_t timeout_us = (timeout_ms == WINK_MUTEX_WAIT_FOREVER)
                           ? 0ULL : (uint64_t)timeout_ms * 1000ULL;
    uint32_t resource_id = (uint32_t)((uintptr_t)sem + 0x10000);
    sq_push(s, self);
    sim_scheduler_block(self, resource_id, now, timeout_us);
    sim_ctx_t* cur_ctx = sim_scheduler_current_ctx();
    assert(cur_ctx != NULL);
    sim_ctx_switch(cur_ctx, s_main_ctx);

    const sim_task_t* t = sim_scheduler_get(self);
    if (t->timeout_fired) {
        sq_remove(s, self);
        return WINK_ERR_TIMEOUT;
    }
    return WINK_OK;
}

wink_status_t pal_os_sem_give(pal_os_sem_t sem) {
    wasm_sem_t* s = wasm_sem_from_handle(sem);
    if (s == NULL) return WINK_ERR_INVALID_ARG;

    uint32_t next_task = 0;
    if (sq_pop(s, &next_task)) {
        sim_scheduler_resume(next_task);
        return WINK_OK;
    }
    s->count = 1;
    return WINK_OK;
}

wink_status_t pal_os_sem_give_isr(pal_os_sem_t sem) {
    return pal_os_sem_give(sem);
}

void pal_os_sem_destroy(pal_os_sem_t sem) {
    wasm_sem_t* s = wasm_sem_from_handle(sem);
    if (s != NULL) {
        s->used = false;
    }
}

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

// assert.h included at top

/* s_sim_in_isr / s_sim_in_pt 已在文件顶部定义；setter/getter 在此实现。 */

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
    void (*func)(void*), const char* name, uint32_t stack_depth,
    void* arg, int32_t priority, pal_os_core_id_t core_id,
    pal_os_task_handle_t* task_handle)
{
    uint32_t id;
    wink_status_t st = sim_scheduler_register(
        func, arg, name, priority, (int32_t)core_id, stack_depth, &id);
    if (st != WINK_OK) return st;
    if (task_handle) *task_handle = (pal_os_task_handle_t)(uintptr_t)(id + 1);
    return WINK_OK;
}

void pal_os_task_delete(pal_os_task_handle_t handle) {
    if (handle == NULL) {
        uint32_t cur = sim_scheduler_current_id();
        sim_ctx_t* cur_ctx = sim_scheduler_current_ctx();
        assert(cur_ctx != NULL && "self-delete outside task fiber context");
        sim_scheduler_mark_zombie(cur);
        /* 红线 15 反面契约：task 让出不改 s_current_task_id —— 主 loop 切回后清零。 */
        sim_ctx_switch(cur_ctx, s_main_ctx);
        /* Unreachable */
    } else {
        uint32_t id = (uint32_t)(uintptr_t)handle - 1;
        sim_scheduler_mark_zombie(id);
    }
}

/* ── Runtime introspection (wasm: unsupported → 0) ─────── */
uint32_t pal_os_get_free_heap_size(void) { return 0u; }
uint32_t pal_os_get_min_free_heap_size(void) { return 0u; }
uint32_t pal_os_get_current_task_stack_free(void) { return 0u; }

/* 物理墙钟（微秒），用于 pal_sim_scheduler_run WCET 兜底判定（红线 11）。
 * 走 emscripten_get_now()（浏览器 performance.now()，毫秒精度） × 1000。
 * 严格与虚拟时钟 pal_os_get_us()/s_virtual_us 分离：虚拟时钟服务业务语义；
 * 物理墙钟只服务"CPU 死循环是否卡死宿主线程"这一物理事实。 */
static inline uint64_t wasm_wall_clock_us(void) {
    return (uint64_t)(emscripten_get_now() * 1000.0);
}

wink_status_t pal_sim_scheduler_run(const struct wink_app_callbacks* callbacks,
                                    uint32_t main_task_id, uint32_t max_ticks) {
    /* Re-entrancy guard: the cooperative scheduler is not re-entrant. Under the
     * wasm single-threaded + Asyncify model, re-entry could theoretically occur
     * if a JS callback invoked during promise resolution (e.g., sleep completion)
     * somehow called back into pal_sim_scheduler_run. This assert catches that
     * class of bug loudly at development time rather than silently corrupting
     * s_app_callbacks / s_main_ctx. In release builds (NDEBUG) we still guard
     * with a runtime fault rather than corrupting state. */
    static bool s_scheduler_running = false;
    assert(!s_scheduler_running && "pal_sim_scheduler_run is not re-entrant");
    if (s_scheduler_running) {
        wink_trace_fault(WINK_ERR_PANIC);
        return WINK_ERR_INVALID_STATE;
    }
    s_scheduler_running = true;

    if (callbacks == NULL) {
        s_scheduler_running = false;
        return WINK_ERR_INVALID_ARG;
    }

    s_main_ctx = sim_ctx_from_current();
    /* 新 run 周期先清 fault 锁存（同时清空 fault.c 内 App callbacks 缓存），
     * 再把本次 run 的 callbacks 注册进去，供 pal_wasm_host_fault / WCET 兜底
     * 走 wink_runtime_fault 路径时定位 on_fault 回调。 */
    pal_wasm_clear_fault_latch();
    pal_wasm_fault_set_callbacks(callbacks);
    uint32_t ticks_run = 0;

    /* --- WCET config cache（fixup 计划 R9 / P1-5 契约诚实）---
     *
     * 注意（P1-5）：wasm 侧没有与 host 侧 IsDebuggerPresent() 对等的 API —— 浏览器
     * devtools 断点不会暴露给 wasm 代码，`emscripten_get_now()` 在断点 resume 后会
     * 跳变，立即触发 8002 WCET fault。这是已知保真度边界：浏览器下断点调试时请通过
     *   WINK_SIM_BYPASS_WCET=1
     * 环境变量或 `WINK_SIM_WCET_THRESHOLD_US=<大值>` 手动放宽。详见
     * 04-wasm-simulation/07-scheduler-model.md §5 WCET 章节。 */
    uint64_t wcet_threshold_us = WINK_SIM_TASK_WCET_THRESHOLD_US;
    const char* env_thr = getenv("WINK_SIM_WCET_THRESHOLD_US");
    if (env_thr) {
        wcet_threshold_us = strtoull(env_thr, NULL, 10);
    } else if (getenv("CI") != NULL) {
        wcet_threshold_us *= 10ULL;
    }
    bool bypass_wcet = (getenv("WINK_SIM_BYPASS_WCET") != NULL);

    /* 红线 15：进入主调度 loop 前清空 current_id */
    sim_scheduler_set_current(SIM_SCHED_NO_READY);

    while (1) {
        /* Phase 0：每轮 tick 首先 drain 所有 pending 中断。
         * - JS 侧 Poll 队列（GPIO 边沿等外部事件，InterruptQueue FIFO）；
         * - C 侧软中断 FIFO（pal_irq_set_pending 路径，在 JS drain 完后级联派发）。
         * 恢复 ADR-0013 §"已知保真度边界" 第 3 条承诺的"O(scheduler tick)" 唤醒延迟——
         * 无论哪个 task 正在跑，任何 sleep 期间到达的 ISR 都在下一次调度决策前被 dispatch。
         * 持锁时（s_irq_lock_nest_count > 0）drain 直接返回，锁释放由
         * pal_irq_restore() 最外层补发（P0-1 修复：两条路径统一尊重 IRQ 临界区）。 */
        pal_wasm_dispatch_pending_interrupts();

        /* Phase 1: GC —— 释放已 ZOMBIE 的 fiber（此时它们都不在运行） */
        sim_scheduler_gc_zombies();

        /* 终结机制检查：若 app_main 任务已被删除 (TERMINATED) 或 max_ticks 达到，跳出调度 loop */
        if (main_task_id != SIM_SCHED_NO_READY) {
            const sim_task_t* main_task = sim_scheduler_get(main_task_id);
            if (main_task->state == SIM_TASK_STATE_TERMINATED) {
                break;
            }
        }
        if (max_ticks > 0 && ticks_run >= max_ticks) {
            break;
        }

        /* Phase 2: 唤醒到期的 WAITING/BLOCKED */
        uint64_t now = pal_os_get_us();
        sim_scheduler_wakeup_by_time(now);

        /* Phase 3: 选下一个 READY */
        uint32_t next = sim_scheduler_pick_next();
        if (next == SIM_SCHED_NO_READY) {
            uint64_t wake = sim_scheduler_next_wakeup_us();
            if (wake == UINT64_MAX) break;   /* 全部 TERMINATED */

            now = pal_os_get_us();
            if (wake > now) {
                uint32_t sleep_ms = (uint32_t)((wake - now + 999) / 1000);
                js_pal_os_sleep_ms(sleep_ms);  /* Asyncify 挂起，由 JS 唤醒并步进时钟 */
            }
            continue;
        }

        /* Phase 4: 切到 task (带 WCET 运行监控 —— 物理墙钟) */
        sim_scheduler_set_current(next);
        const sim_task_t* t = sim_scheduler_get(next);
        uint64_t wall_start_us = wasm_wall_clock_us();
        sim_ctx_switch(s_main_ctx, t->ctx);
        /* 红线 15：task 让出后清空 current_id */
        sim_scheduler_set_current(SIM_SCHED_NO_READY);
        uint64_t duration_us = wasm_wall_clock_us() - wall_start_us;

        if (!bypass_wcet && duration_us > wcet_threshold_us) {
            /* 红线 16：走 fault.c 内联合入口，置锁存 + 调 wink_runtime_fault。
             * fault.c 内缓存的 s_app_callbacks 由本函数入口的
             * pal_wasm_fault_set_callbacks(callbacks) 注册。 */
            pal_wasm_invoke_fault(8002);
        }

        if (next == main_task_id) {
            ticks_run++;
        }
    }

    /* 清理残余 fiber */
    sim_scheduler_gc_zombies();
    sim_scheduler_set_current(SIM_SCHED_NO_READY);
    s_scheduler_running = false;
    return WINK_OK;
}

/* pal_os_ringbuf_* 环形缓冲区实现已上移至 targets/common/src/pal_osal_ringbuf.c，
 * 与 host target 共享（字节级一致的纯内存单线程实现）。 */
