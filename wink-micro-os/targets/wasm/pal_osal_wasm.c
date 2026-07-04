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

// assert.h included at top

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
