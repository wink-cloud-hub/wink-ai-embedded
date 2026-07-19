/**
 * @file pal_osal_host.c
 * @brief host 一等 target 的 PAL OSAL 实现 + 虚拟时间状态机 + host_test_ctrl 实现。
 *        虚拟时间状态在此维护（HAL 经 extern 消费）。
 */
#include "pal_osal.h"
#include "host_test_ctrl.h"
#include "wink_sim_physical.h"   /* wink_phys_debounce_ctx_t + WINK_SIM_FAULTS_IDEAL */
#include "wink_sim_scheduler.h"
#include "host_wall_clock.h"     /* F2 R6：WCET 物理墙钟量测共享 helper */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
static inline bool IsDebuggerPresent(void) { return false; }
#endif

struct wink_app_callbacks;
__attribute__((weak)) void wink_runtime_fault(const struct wink_app_callbacks* callbacks, uint32_t fault_code) {
    (void)callbacks;
    (void)fault_code;
    fprintf(stderr, "[STUB] wink_runtime_fault called with code %u\n", (unsigned int)fault_code);
}

void pal_wasm_dispatch_pending_interrupts(void) {
    /* No-op on host simulation target */
}


static uint64_t s_time_us = 0;
static uint64_t s_echo_rise_us = 0;
static uint64_t s_echo_high_us = 0;
static uint16_t s_echo_pin = 0xFFFF;
static float s_pwm_duty[8];
static pal_os_reset_reason_t s_reset_reason = PAL_OS_RESET_REASON_POWER_ON;   /* Phase 5：可配置复位原因（测试注入） */
static uint32_t s_abnormal_boot_count = 0;   /* ADR-0010：连续异常复位计数（host 可注入，供单测）*/

/* Phase 2：host I2C 事务捕获状态 */
static uint8_t  s_last_i2c_port = 0;
static uint16_t s_last_i2c_addr = 0;
static uint32_t s_last_i2c_write_len = 0;
static uint32_t s_i2c_transfer_count = 0;

/* ADR-0009 Wave1：GPIO 理想电平注入 + per-pin 抖动 ctx + 全局 faults。
 * 语义契约（§2.3 红线 6/7）：
 *   - sim_set_gpio_ideal 双语义：首次注册 pin = 上电态（stable=level，不抖）；
 *     更新已注册 pin 电平 = 用户操作跃变（仅改 ideal，不碰 ctx → 下次采样 target≠stable 触发抖动）。
 *   - 注入 pin 须 ≠ host_echo_pin()（默认 0xFFFF），否则短路 echo 协作推进（§2.3 红线 7）。 */
static struct {
    bool     set;
    uint16_t pin;
    bool     ideal;
    wink_phys_debounce_ctx_t ctx;
} s_gpio_ideal[SIM_GPIO_IDEAL_SLOTS];
static wink_sim_faults_t s_faults = { 0 };

/* ---- HAL 侧 extern 的访问器 ---- */
uint64_t host_sim_time_us(void) { return s_time_us; }
void host_sim_advance_to(uint64_t us) { if (us > s_time_us) s_time_us = us; }
uint64_t host_echo_rise_us(void) { return s_echo_rise_us; }
uint64_t host_echo_high_us(void) { return s_echo_high_us; }
uint16_t host_echo_pin(void) { return s_echo_pin; }
void host_record_pwm(uint8_t channel, float duty) {
    if (channel < 8) s_pwm_duty[channel] = duty;
}

/* ---- Phase 2：I2C 事务捕获（供 host HAL + test ctrl） ---- */
void host_record_i2c(uint8_t port, uint16_t addr, uint32_t write_len) {
    s_last_i2c_port       = port;
    s_last_i2c_addr       = addr;
    s_last_i2c_write_len  = write_len;
    s_i2c_transfer_count++;
}

uint8_t  host_last_i2c_port(void)      { return s_last_i2c_port; }
uint16_t host_last_i2c_addr(void)      { return s_last_i2c_addr; }
uint32_t host_last_i2c_write_len(void) { return s_last_i2c_write_len; }
uint32_t host_i2c_transfer_count(void) { return s_i2c_transfer_count; }

/* host_test_ctrl.h 命名包装（与 sim_last_pwm_duty 同范式） */
uint8_t  sim_last_i2c_port(void)      { return host_last_i2c_port(); }
uint16_t sim_last_i2c_addr(void)      { return host_last_i2c_addr(); }
uint32_t sim_last_i2c_write_len(void) { return host_last_i2c_write_len(); }
uint32_t sim_i2c_transfer_count(void) { return host_i2c_transfer_count(); }

/* ---- host_test_ctrl 实现 ---- */
void sim_reset_time(void) {
    s_time_us = 0; s_echo_rise_us = 0; s_echo_high_us = 0; s_echo_pin = 0xFFFF;
    memset(s_pwm_duty, 0, sizeof(s_pwm_duty));
    s_reset_reason = PAL_OS_RESET_REASON_POWER_ON;
    s_abnormal_boot_count = 0;
    s_last_i2c_port = 0; s_last_i2c_addr = 0;
    s_last_i2c_write_len = 0; s_i2c_transfer_count = 0;
    sim_clear_gpio_ideal();   /* ADR-0009 Wave1：重置时清空 GPIO 注入表，保证测试隔离 */
}
void sim_set_echo_pin(uint16_t pin) { s_echo_pin = pin; }
void sim_set_echo_timing(uint64_t rise_us, uint64_t high_duration_us) {
    s_echo_rise_us = rise_us; s_echo_high_us = high_duration_us;
}
float sim_last_pwm_duty(uint8_t channel) {
    if (channel >= 8) return -1.0f;
    return s_pwm_duty[channel];
}
void sim_set_reset_reason(pal_os_reset_reason_t reason) { s_reset_reason = reason; }

/* ---- ADR-0009 Wave1：GPIO 理想注入 API */
void sim_set_gpio_ideal(uint16_t pin, bool level) {
    /* 跃变分支：pin 已注册 → 仅更新理想电平，不碰 ctx（保留旧 stable → 下次采样 target≠stable 触发抖动 */
    for (int i = 0; i < SIM_GPIO_IDEAL_SLOTS; i++) {
        if (s_gpio_ideal[i].set && s_gpio_ideal[i].pin == pin) {
            s_gpio_ideal[i].ideal = level;
            return;
        }
    }
    /* 注册分支：首次占用空槽 = 上电态（stable=level，无跃变不抖；flip=false） */
    for (int i = 0; i < SIM_GPIO_IDEAL_SLOTS; i++) {
        if (!s_gpio_ideal[i].set) {
            s_gpio_ideal[i].set = true;
            s_gpio_ideal[i].pin = pin;
            s_gpio_ideal[i].ideal = level;
            s_gpio_ideal[i].ctx.stable_level    = level;   /* 上电态 */
            s_gpio_ideal[i].ctx.in_bounce       = false;
            s_gpio_ideal[i].ctx.bounce_start_us = 0;
            s_gpio_ideal[i].ctx.bounce_flip     = false;
            return;
        }
    }
    assert(false && "GPIO ideal slots exceeded SIM_GPIO_IDEAL_SLOTS!");
}
void sim_clear_gpio_ideal(void) {
    for (int i = 0; i < SIM_GPIO_IDEAL_SLOTS; i++) { s_gpio_ideal[i].set = false; }
}
void sim_set_faults(const wink_sim_faults_t *faults) {
    s_faults = (faults != NULL) ? *faults : WINK_SIM_FAULTS_IDEAL;
}
/* HAL 侧访问器：命中注入 pin → 走抖动退化；返回 true 表示命中 */
bool host_gpio_read_debounced(uint16_t pin, bool *out_level) {
    for (int i = 0; i < SIM_GPIO_IDEAL_SLOTS; i++) {
        if (s_gpio_ideal[i].set && s_gpio_ideal[i].pin == pin) {
            *out_level = wink_phys_debounce_step(&s_gpio_ideal[i].ctx, s_gpio_ideal[i].ideal,
                                                 s_time_us, s_faults.bounce_us);
            /* SIM_TRACE：host 层补充 pin 信息（算法层 pin=-1） */
#if defined(SIM_TRACE_DEBOUNCE) && SIM_TRACE_DEBOUNCE
            printf("[SIM_HOST] pin=%d: ideal=%d debounced=%d stable=%d in_bounce=%d\n",
                   pin, s_gpio_ideal[i].ideal, *out_level,
                   s_gpio_ideal[i].ctx.stable_level, s_gpio_ideal[i].ctx.in_bounce);
#endif
            return true;
        }
    }
    return false;
}

/* ---- PAL OSAL ---- */
static sim_ctx_t* s_main_ctx = NULL;

void pal_os_sleep_ms(uint32_t ms) {
    /* fixup 计划 H2 修订：
     * host target 的 pal_os_sleep_ms 编译时不看 SIMULATION 宏（pal_host OBJECT 库
     * 由 top-level 编译，未定义 SIMULATION）——只依据"调度器是否已启动"分流。
     *
     *   - s_main_ctx == NULL：调度器未启动。合法用途包括：
     *       (a) non-SIM 单任务 e2e（avoidance_car / oled_dashboard / devkitc_smoke /
     *           test_runtime）：wink_runtime_run 走 non-SIMULATION 主 loop 分支，
     *           每 tick 通过 pal_os_sleep_ms 推进虚拟时钟；
     *       (b) legacy 单测直接调 pal_os_sleep_ms 推进时钟（现推荐改用 host_sim_advance_to）。
     *
     *   - s_main_ctx != NULL：SIM 调度器运行中，sleep 必须在任务协程上下文中调用，
     *     并通过 sim_ctx_switch 让出到主调度 loop。 */
    if (s_main_ctx == NULL) {
        s_time_us += (uint64_t)ms * 1000u;
        return;
    }
    uint32_t cur = sim_scheduler_current_id();
    /* T5 契约：在调度器运行期间，sleep 必须在任务协程上下文中调用 */
    assert(cur != SIM_SCHED_NO_READY &&
           "pal_os_sleep_ms called from main thread while scheduler is active; "
           "did you call sleep outside task fiber context?");
    sim_ctx_t* cur_ctx = sim_scheduler_current_ctx();
    assert(cur_ctx != NULL && "sim_scheduler_current_ctx returned NULL in task context");
    sim_scheduler_yield_timed(cur, host_sim_time_us(), (uint64_t)ms * 1000);
    /* 红线 15 反面契约：task 让出时不动 s_current_task_id —— 恢复后 sleep 后续
     * 逻辑（若有）仍需 sim_scheduler_current_ctx() 定位自己的 ctx。 */
    sim_ctx_switch(cur_ctx, s_main_ctx);
    /* 主 loop 会推进虚拟时钟并 wakeup_by_time 把我们转 READY，再切回来 */
}
void pal_os_busy_wait_us(uint32_t us) { s_time_us += us; }
uint64_t pal_os_get_ms(void) { return s_time_us / 1000u; }
uint64_t pal_os_get_us(void) { return s_time_us; }

/* ---- Phase 5 Task 5-4: WDT / reset reason（host：WDT 为无操作 stub；reset reason 可配置供测试） ---- */
pal_os_reset_reason_t pal_os_get_reset_reason(void) { return s_reset_reason; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_init(uint32_t timeout_ms) { (void)timeout_ms; return WINK_OK; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_feed(void) { return WINK_OK; }

/* ADR-0010：连续异常复位计数（host 可注入静态，供单测模拟 N 次异常复位）*/
uint32_t pal_os_get_abnormal_boot_count(void) { return s_abnormal_boot_count; }
void pal_os_set_abnormal_boot_count(uint32_t count) { s_abnormal_boot_count = count; }

/* ─────────────────────────────────────────────────────────
 * 临界区（task/ISR 双入口显式分流, ADR-0016）
 * Host 单线程仿真：语义等价（都是 no-op），但通过 s_sim_in_isr 强校验
 * 调用方是否使用了正确的入口——Debug 构建下入口误用立即命中 assert。
 * ───────────────────────────────────────────────────────── */

static bool s_sim_in_isr = false;
static bool s_sim_in_pt = false;

void pal_os_set_sim_isr_context(bool in_isr) { s_sim_in_isr = in_isr; }
bool pal_os_in_sim_isr_context(void) { return s_sim_in_isr; }
bool pal_os_in_isr(void) { return pal_os_in_sim_isr_context(); }

void pal_os_set_sim_pt_context(bool in_pt) { s_sim_in_pt = in_pt; }
bool pal_os_in_sim_pt_context(void) { return s_sim_in_pt; }
bool wink_pt_in_context(void) { return s_sim_in_pt; }

uint32_t pal_os_critical_enter(void) {
    /* task 版被 ISR 上下文调用 → 契约违反（在真机 ESP32 上会触发 portENTER_CRITICAL assert） */
    assert(!s_sim_in_isr && "pal_os_critical_enter called from ISR context; use pal_os_critical_enter_isr (ADR-0016)");
    return 0;
}

void pal_os_critical_exit(uint32_t key) {
    (void)key;
    assert(!s_sim_in_isr && "pal_os_critical_exit called from ISR context (ADR-0016)");
}

uint32_t pal_os_critical_enter_isr(void) {
    /* ISR 版被 task 上下文调用 → 契约违反（保护范围不匹配） */
    assert(s_sim_in_isr && "pal_os_critical_enter_isr called from task context; use pal_os_critical_enter (ADR-0016)");
    return 0;
}

void pal_os_critical_exit_isr(uint32_t key) {
    (void)key;
    assert(s_sim_in_isr && "pal_os_critical_exit_isr called from task context (ADR-0016)");
}

/* ─────────────────────────────────────────────────────────
 * Mutex：协作调度下真正的 BLOCKED 路径实现（P0 E5-part2）
 *
 * 与 wasm 端同构（共享 wink_sim_scheduler 协作调度模型）：
 *   - 静态 POD 池（WINK_SIM_MAX_TASKS 槽）避免 malloc 失败
 *   - owner=HOST_MUTEX_NO_OWNER 表未持；waiter FIFO 队列
 *   - lock 争用时 sim_scheduler_block + ctx_switch，resume 后检查 timeout_fired
 *   - unlock FIFO 取一个 waiter 转交 owner 并 resume；支持超时自摘除
 *   - destroy 断言未持锁且无 waiter（契约诚实，避免 UB）
 * ───────────────────────────────────────────────────────── */

#define HOST_MUTEX_POOL_SIZE  WINK_SIM_MAX_TASKS
#define HOST_MUTEX_NO_OWNER   (WINK_SIM_MAX_TASKS + 1)  /* task id 0..WINK_SIM_MAX_TASKS-1 */

typedef struct {
    bool     used;
    uint32_t owner;
    uint32_t waiters[WINK_SIM_MAX_TASKS];
    uint8_t  w_head;
    uint8_t  w_tail;
    uint8_t  w_count;
} host_mutex_t;

static host_mutex_t s_mtx_pool[HOST_MUTEX_POOL_SIZE];

#define HOST_SEM_POOL_SIZE  WINK_SIM_MAX_TASKS

typedef struct {
    bool     used;
    uint32_t count;
    uint32_t waiters[WINK_SIM_MAX_TASKS];
    uint8_t  w_head;
    uint8_t  w_tail;
    uint8_t  w_count;
} host_sem_t;

static host_sem_t s_sem_pool[HOST_SEM_POOL_SIZE];

static host_sem_t* host_sem_from_handle(pal_os_sem_t h) {
    if (h == NULL) return NULL;
    uintptr_t idx = (uintptr_t)h - 1;
    if (idx >= HOST_SEM_POOL_SIZE) return NULL;
    host_sem_t* s = &s_sem_pool[idx];
    if (!s->used) return NULL;
    return s;
}

static void hsq_push(host_sem_t* s, uint32_t tid) {
    if (s->w_count >= WINK_SIM_MAX_TASKS) return;
    s->waiters[s->w_tail] = tid;
    s->w_tail = (uint8_t)((s->w_tail + 1u) % WINK_SIM_MAX_TASKS);
    s->w_count++;
}

static bool hsq_pop(host_sem_t* s, uint32_t* out) {
    if (s->w_count == 0) return false;
    *out = s->waiters[s->w_head];
    s->w_head = (uint8_t)((s->w_head + 1u) % WINK_SIM_MAX_TASKS);
    s->w_count--;
    return true;
}

static void hsq_remove(host_sem_t* s, uint32_t tid) {
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

static host_mutex_t* host_mtx_from_handle(pal_os_mutex_t h) {
    if (h == NULL) return NULL;
    uintptr_t idx = (uintptr_t)h - 1;
    if (idx >= HOST_MUTEX_POOL_SIZE) return NULL;
    host_mutex_t* m = &s_mtx_pool[idx];
    if (!m->used) return NULL;
    return m;
}

static void hwq_push(host_mutex_t* m, uint32_t tid) {
    if (m->w_count >= WINK_SIM_MAX_TASKS) return;
    m->waiters[m->w_tail] = tid;
    m->w_tail = (uint8_t)((m->w_tail + 1u) % WINK_SIM_MAX_TASKS);
    m->w_count++;
}

static bool hwq_pop(host_mutex_t* m, uint32_t* out) {
    if (m->w_count == 0) return false;
    *out = m->waiters[m->w_head];
    m->w_head = (uint8_t)((m->w_head + 1u) % WINK_SIM_MAX_TASKS);
    m->w_count--;
    return true;
}

static void hwq_remove(host_mutex_t* m, uint32_t tid) {
    if (m->w_count == 0) return;
    uint8_t w = 0, r = 0;
    uint8_t cnt = m->w_count;
    for (uint8_t i = 0; i < cnt; ++i) {
        uint32_t t = m->waiters[(m->w_head + i) % WINK_SIM_MAX_TASKS];
        if (t != tid) {
            m->waiters[(m->w_head + w) % WINK_SIM_MAX_TASKS] = t;
            w++;
        } else { r++; }
    }
    m->w_tail = (uint8_t)((m->w_head + w) % WINK_SIM_MAX_TASKS);
    m->w_count = (uint8_t)(m->w_count - r);
}

pal_os_mutex_t pal_os_mutex_create(void) {
    for (uint32_t i = 0; i < HOST_MUTEX_POOL_SIZE; ++i) {
        if (!s_mtx_pool[i].used) {
            host_mutex_t* m = &s_mtx_pool[i];
            m->used = true;
            m->owner = HOST_MUTEX_NO_OWNER;
            m->w_head = 0; m->w_tail = 0; m->w_count = 0;
            return (pal_os_mutex_t)(uintptr_t)(i + 1);
        }
    }
    return NULL;
}

wink_status_t pal_os_mutex_lock(pal_os_mutex_t mutex, uint32_t timeout_ms) {
    host_mutex_t* m = host_mtx_from_handle(mutex);
    if (m == NULL) return WINK_ERR_INVALID_ARG;
    assert(!s_sim_in_isr && "pal_os_mutex_lock called from ISR context");

    uint32_t self = sim_scheduler_current_id();
    uint64_t now = host_sim_time_us();

    if (m->owner == HOST_MUTEX_NO_OWNER) {
        m->owner = self;
        return WINK_OK;
    }
    if (self == SIM_SCHED_NO_READY) {
        /* 主上下文（scheduler 外）争用已持锁——不能 block 否则死锁调度器 */
        return WINK_ERR_TIMEOUT;
    }
    if (m->owner == self) {
        return WINK_ERR_BUSY; /* 非递归锁，同 task 重入是 bug */
    }

    uint64_t timeout_us = (timeout_ms == WINK_MUTEX_WAIT_FOREVER)
                           ? 0ULL : (uint64_t)timeout_ms * 1000ULL;
    uint32_t resource_id = (uint32_t)((uintptr_t)mutex);
    hwq_push(m, self);
    sim_scheduler_block(self, resource_id, now, timeout_us);
    sim_ctx_t* cur_ctx = sim_scheduler_current_ctx();
    assert(cur_ctx != NULL);
    sim_ctx_switch(cur_ctx, s_main_ctx);

    const sim_task_t* t = sim_scheduler_get(self);
    if (t->timeout_fired) {
        hwq_remove(m, self);
        return WINK_ERR_TIMEOUT;
    }
    if (m->owner != self) return WINK_ERR_IO;
    return WINK_OK;
}

wink_status_t pal_os_mutex_unlock(pal_os_mutex_t mutex) {
    host_mutex_t* m = host_mtx_from_handle(mutex);
    if (m == NULL) return WINK_ERR_INVALID_ARG;
    uint32_t self = sim_scheduler_current_id();
    if (m->owner == HOST_MUTEX_NO_OWNER) return WINK_ERR_INVALID_STATE;
    if (m->owner != self) return WINK_ERR_PERMISSION;

    uint32_t waiter;
    if (hwq_pop(m, &waiter)) {
        m->owner = waiter;
        sim_scheduler_resume(waiter);
    } else {
        m->owner = HOST_MUTEX_NO_OWNER;
    }
    return WINK_OK;
}

void pal_os_mutex_destroy(pal_os_mutex_t mutex) {
    host_mutex_t* m = host_mtx_from_handle(mutex);
    if (m == NULL) return;
    assert(m->owner == HOST_MUTEX_NO_OWNER && m->w_count == 0 &&
           "pal_os_mutex_destroy: mutex still held or has waiters");
    m->used = false;
}

/* ─────────────────────────────────────────────────────────
 * 线程同步二值信号量（Semaphore）
 * ───────────────────────────────────────────────────────── */

pal_os_sem_t pal_os_sem_create(void) {
    for (uint32_t i = 0; i < HOST_SEM_POOL_SIZE; ++i) {
        if (!s_sem_pool[i].used) {
            host_sem_t* s = &s_sem_pool[i];
            s->used = true;
            s->count = 0;
            s->w_head = 0; s->w_tail = 0; s->w_count = 0;
            return (pal_os_sem_t)(uintptr_t)(i + 1);
        }
    }
    return NULL;
}

wink_status_t pal_os_sem_take(pal_os_sem_t sem, uint32_t timeout_ms) {
    host_sem_t* s = host_sem_from_handle(sem);
    if (s == NULL) return WINK_ERR_INVALID_ARG;
    assert(!s_sim_in_isr && "pal_os_sem_take called from ISR context");

    uint32_t self = sim_scheduler_current_id();
    uint64_t now = host_sim_time_us();

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
    hsq_push(s, self);
    sim_scheduler_block(self, resource_id, now, timeout_us);
    sim_ctx_t* cur_ctx = sim_scheduler_current_ctx();
    assert(cur_ctx != NULL);
    sim_ctx_switch(cur_ctx, s_main_ctx);

    const sim_task_t* t = sim_scheduler_get(self);
    if (t->timeout_fired) {
        hsq_remove(s, self);
        return WINK_ERR_TIMEOUT;
    }
    return WINK_OK;
}

wink_status_t pal_os_sem_give(pal_os_sem_t sem) {
    host_sem_t* s = host_sem_from_handle(sem);
    if (s == NULL) return WINK_ERR_INVALID_ARG;

    uint32_t next_task = 0;
    if (hsq_pop(s, &next_task)) {
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
    host_sem_t* s = host_sem_from_handle(sem);
    if (s != NULL) {
        s->used = false;
    }
}

/* ─────────────────────────────────────────────────────────
 * Task 创建（host target 降级实现，同步调用）
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
        /* 自删三段式（对齐 R-009）：
         *   ① mark_zombie —— 只改状态，不删 fiber
         *   ② SwitchToFiber(main) —— 让出；当前 fiber 挂起
         *   ③ 主 loop 下轮 gc_zombies → sim_ctx_destroy → DeleteFiber
         *     （此时 fiber 不再是自己，安全） */
        uint32_t cur = sim_scheduler_current_id();
        sim_ctx_t* cur_ctx = sim_scheduler_current_ctx();
        assert(cur_ctx != NULL && "self-delete outside task fiber context");
        sim_scheduler_mark_zombie(cur);
        /* 红线 15 反面契约：task 让出前不动 s_current_task_id —— 主 loop 切回后
         * 会立即调用 sim_scheduler_set_current(SIM_SCHED_NO_READY)。 */
        sim_ctx_switch(cur_ctx, s_main_ctx);
        /* Unreachable */
    } else {
        uint32_t id = (uint32_t)(uintptr_t)handle - 1;
        sim_scheduler_mark_zombie(id);
    }
}

/* ── Runtime introspection (host/posix: unsupported → 0) ── */
uint32_t pal_os_get_free_heap_size(void) { return 0u; }
uint32_t pal_os_get_min_free_heap_size(void) { return 0u; }
uint32_t pal_os_get_current_task_stack_free(void) { return 0u; }

wink_status_t pal_sim_scheduler_run(const struct wink_app_callbacks* callbacks,
                                    uint32_t main_task_id, uint32_t max_ticks) {
    s_main_ctx = sim_ctx_from_current();
    uint32_t ticks_run = 0;

    /* --- WCET config cache（fixup 计划 R9）---
     * env var 在入口读一次，主 loop 每 tick 只读 static，避免 getenv 系统调用开销。
     * 阈值优先级：$WINK_SIM_WCET_THRESHOLD_US > $CI 放宽 10x > 默认 WINK_SIM_TASK_WCET_THRESHOLD_US。
     * bypass 优先级：$WINK_SIM_BYPASS_WCET 或 IsDebuggerPresent()。 */
    uint64_t wcet_threshold_us = WINK_SIM_TASK_WCET_THRESHOLD_US;
    const char* env_thr = getenv("WINK_SIM_WCET_THRESHOLD_US");
    if (env_thr) {
        wcet_threshold_us = strtoull(env_thr, NULL, 10);
    } else if (getenv("CI") != NULL) {
        /* CI 环境放宽 10 倍以吸收虚拟化 CPU 抖动（fixup 计划 R9 / RF-002）。 */
        wcet_threshold_us *= 10ULL;
    }
    bool bypass_wcet = (getenv("WINK_SIM_BYPASS_WCET") != NULL) || IsDebuggerPresent();

    /* 红线 15：进入主调度 loop 前显式清空 current_id */
    sim_scheduler_set_current(SIM_SCHED_NO_READY);

    while (1) {
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
        uint64_t now = host_sim_time_us();
        sim_scheduler_wakeup_by_time(now);

        /* Phase 3: 选下一个 READY */
        uint32_t next = sim_scheduler_pick_next();
        if (next == SIM_SCHED_NO_READY) {
            uint64_t wake = sim_scheduler_next_wakeup_us();
            if (wake == UINT64_MAX) break;   /* 全部 TERMINATED */
            host_sim_advance_to(wake);
            continue;
        }

        /* Phase 4: 切到 task (带 WCET 运行监控) —— 用物理墙钟量测 slice 耗时（fixup 红线 11），
         * 虚拟时钟 pal_os_get_us() 只服务业务语义，不用于 WCET 判定。 */
        sim_scheduler_set_current(next);
        const sim_task_t* t = sim_scheduler_get(next);
        uint64_t wall_start_us = host_wall_clock_us();
        sim_ctx_switch(s_main_ctx, t->ctx);
        /* 红线 15：task 让出后，主 loop 立即清空 current_id */
        sim_scheduler_set_current(SIM_SCHED_NO_READY);
        uint64_t duration_us = host_wall_clock_us() - wall_start_us;

        if (!bypass_wcet && duration_us > wcet_threshold_us) {
            fprintf(stderr,
                "[ERROR] Task [%s] WCET violated: executed for %llu us, "
                "threshold is %llu us. Triggering 8002!\n",
                t->name, (unsigned long long)duration_us,
                (unsigned long long)wcet_threshold_us);
            /* 红线 16：透传 callbacks，让 App on_fault(8002) 被调，与真机 fault 路径一致。 */
            wink_runtime_fault(callbacks, 8002);
        }

        if (next == main_task_id) {
            ticks_run++;
        }
    }

    /* 清理残余 fiber */
    sim_scheduler_gc_zombies();
    /* 退出前对齐红线 15：清空 current_id */
    sim_scheduler_set_current(SIM_SCHED_NO_READY);
    return WINK_OK;
}

/* pal_os_ringbuf_* 环形缓冲区实现已上移至 osal/common/pal_osal_ringbuf.c，
 * 与 wasm OSAL 共享（字节级一致的纯内存单线程实现）。 */
