#ifndef WINK_SIM_SCHEDULER_H
#define WINK_SIM_SCHEDULER_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include "wink_status.h"
#include "sim_ctx.h"

#define WINK_SIM_MAX_TASKS 8
#define WINK_SIM_TASK_WCET_THRESHOLD_US (5000u) /* 5ms WCET limit in simulation */

/* 平台安全栈下限（对齐 §3.4 §"栈深度" 表） */
#if defined(__EMSCRIPTEN__)
    #define WINK_SIM_STACK_MIN     (16u * 1024u)
    #define WINK_SIM_ASYNCIFY_MIN  (2u  * 1024u)
#elif defined(_WIN32)
    #define WINK_SIM_STACK_MIN     (32u * 1024u)
    #define WINK_SIM_ASYNCIFY_MIN  0u   /* 不适用 */
#else
    #define WINK_SIM_STACK_MIN     (32u * 1024u)
    #define WINK_SIM_ASYNCIFY_MIN  0u
#endif

typedef enum {
    SIM_TASK_STATE_INVALID = 0,
    SIM_TASK_STATE_READY,       /* 可运行 */
    SIM_TASK_STATE_WAITING,     /* sleep_ms 时间等待 */
    SIM_TASK_STATE_BLOCKED,     /* 等外部事件（mutex/queue/sem）；wakeup_us>0 表示带超时 */
    SIM_TASK_STATE_ZOMBIE,      /* 自删已让出，fiber 未释放，等主调度器 GC */
    SIM_TASK_STATE_TERMINATED,  /* 已释放，slot 可被 register 复用 */
} sim_task_state_t;

typedef struct {
    void   (*func)(void*);
    void*    arg;
    int32_t  priority;
    int32_t  core_id;           /* 记录但不用于调度（ADR-0014） */
    uint64_t wakeup_us;         /* 0 = 无时间唤醒；>0 = 到期强制 READY（WAITING/BLOCKED 共用） */
    uint32_t blocked_on;        /* 0 = 未 BLOCKED；>0 = 等待的资源 id（mutex/queue handle） */
    bool     timeout_fired;     /* 供 mutex_lock 返回 TIMEOUT 判断；resume 时清零 */
    sim_task_state_t state;
    uint32_t id;                /* 单调分配 */
    char     name[16];
    sim_ctx_t* ctx;             /* target 相关协程句柄；由 sim_ctx_create 分配 */
} sim_task_t;
_Static_assert(sizeof(sim_task_t) <= 96, "sim_task_t must stay compact");

/* 生命周期 */
void          sim_scheduler_reset(uint32_t prng_seed);
wink_status_t sim_scheduler_register(void (*func)(void*), void* arg,
                                     const char* name, int32_t priority,
                                     int32_t core_id, uint32_t stack_depth,
                                     uint32_t* out_id);
void          sim_scheduler_mark_zombie(uint32_t task_id);   /* 自删标记 */
void          sim_scheduler_gc_zombies(void);                /* 主 loop 调用：ZOMBIE → TERMINATED，释放 fiber */

/* pal_sim_scheduler_run 主调度入口。
 *
 * `callbacks` —— App 回调集合（用于 WCET 触发时透传给 wink_runtime_fault，落实
 * ADR-0012 契约诚实 + fixup 计划红线 16）。允许为 NULL（测试用无 App 场景）。
 *
 * 头依赖处理（fixup 计划 RF-007 纪律）：pal 头只做前向声明，禁止 include
 * `wink_app.h` / `wink_runtime.h`，严守 pal < runtime < app 分层。 */
struct wink_app_callbacks;
wink_status_t pal_sim_scheduler_run(const struct wink_app_callbacks* callbacks,
                                    uint32_t main_task_id, uint32_t max_ticks);

/* 调度决策（Step 3 拆两步，副作用透明） */
uint32_t      sim_scheduler_wakeup_by_time(uint64_t now_us); /* WAITING/BLOCKED 到期 → READY，返回唤醒数量 */
#define SIM_SCHED_NO_READY UINT32_MAX
uint32_t      sim_scheduler_pick_next(void);                 /* 纯函数：从 READY 中挑一个 */

/* 让出与阻塞 */
void          sim_scheduler_yield_timed(uint32_t task_id, uint64_t now_us, uint64_t duration_us);
void          sim_scheduler_block(uint32_t task_id, uint32_t resource_id,
                                uint64_t now_us, uint64_t timeout_us /* 0 = 无限等 */);
void          sim_scheduler_resume(uint32_t task_id);        /* 事件唤醒：BLOCKED → READY，清 timeout_fired */

/* 最近唤醒时间（供 wasm/host 侧推进虚拟时钟） */
uint64_t      sim_scheduler_next_wakeup_us(void);

/* Introspection（供单测 / trace 用） */
uint32_t      sim_scheduler_task_count(void);
const sim_task_t* sim_scheduler_get(uint32_t task_id);
uint32_t      sim_scheduler_current_id(void);
void          sim_scheduler_set_current(uint32_t task_id);

/* 当前运行 fiber 的 ctx（供 pal_os_sleep_ms / pal_os_task_delete 内让出前定位自身 ctx）。
 * 契约：只有在 task fiber 上下文（`s_current_task_id != SIM_SCHED_NO_READY`）返回非 NULL。
 * 见 fixup 计划红线 12（sim_ctx_switch 契约 v2：from 必须非空）。 */
sim_ctx_t*    sim_scheduler_current_ctx(void);

#endif
