/**
 * @file pal_hal_host.c
 * @brief host 一等 target 的 PAL HAL 实现。
 *
 * 设计要点（协作式时间推进，迁移自 ADR-0003 计划 Task 2 pal_host_stub.c）：
 *   ultrasonic 真机分支用 while(!pal_gpio_read(echo)){...} 空等 echo 变高。
 *   host 无真实时间流逝，故让 pal_gpio_read 在被调用时把虚拟时间推进到下一个
 *   echo 边沿，驱动 while 循环前进。
 *
 * ⚠ Phase 4 决策（Task 4-6，方案 B）：App 已迁移到非阻塞 DAL
 *   （dal_ultrasonic_request_measurement + get_cached_distance），其 echo 时序 SSOT 是
 *   pal_gpio_pulse_in（直接读 host_echo_high_us，不经 pal_gpio_read 协作推进）。
 *   pal_gpio_read 的协作推进**保留**，仅供过渡期 @deprecated 的 blocking dal_ultrasonic_read
 *   及其 host 单测驱动——二者（pulse_in vs 协作 read）服务于不同 API（新非阻塞 vs 过渡阻塞），
 *   非冗余。App 完全迁移、sim 旁路下沉到 PAL capture 后，blocking read + 协作推进一并移除
 *   （Phase 4 follow-up，不在本阶段强删）。
 *
 * 注：虚拟时间状态机在 pal_osal_host.c 维护（sim_* API 经 extern 访问）。
 */
#include "pal_hal.h"
#include "pal_irq.h"
#include "pal_resource.h"
#include "pal_pwm_router.h"
#include "pal_debug.h"
#include "hal/pal_ultrasonic.h"
#include "host_test_ctrl.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>   /* v2.2 G3：并发首次注册竞态保护 */

/* 虚拟时间状态（OSAL 侧推进，HAL 侧消费）—— 跨文件共享，故 extern */
extern uint64_t host_sim_time_us(void);
extern void host_sim_advance_to(uint64_t us);
extern uint64_t host_echo_rise_us(void);
extern uint64_t host_echo_high_us(void);
extern uint16_t host_echo_pin(void);
extern void host_record_pwm(uint8_t channel, float duty);

/* 协作式 echo 轮询窗口：真机驱动用 while(!read(echo)){ 超时判定 } 空等 echo。
 * host 无真实时间流逝，故 pal_gpio_read 在被调用时把虚拟时间向 echo 边沿推进，
 * 但每次最多推进本窗口——若 echo 在窗口外（远超 30ms 才变高），驱动循环自身的
 * 30ms 超时判定自然触发（模拟「echo 久不响应」）。窗口值对齐器件超时 (30000us)。 */
#define ECHO_POLL_WINDOW_US 30000u

wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode) {
    /* Phase 2 Task 2-3：host 资源占用治理。owner 为 PAL 层固定标识
     * （同 owner 重复 claim 幂等；不同 owner 冲突 → BUSY 由调用方透传）。 */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin, "pal_hal_host");
    if (wink_status_is_error(rs)) { return rs; }
    (void)mode;
    return WINK_OK;
}
void pal_gpio_write(wink_pin_t pin, bool level) { (void)pin; (void)level; }

bool pal_gpio_read(wink_pin_t pin) {
    /* ADR-0009 Wave1：注入了理想电平的 pin → 走抖动退化（§3.1）；否则走原 echo 协作推进逻辑 */
    bool debounced;
    extern bool host_gpio_read_debounced(uint16_t pin, bool *out_level);
    if (host_gpio_read_debounced(pin, &debounced)) { return debounced; }

    if (pin != host_echo_pin()) return false;
    uint64_t t = host_sim_time_us();
    uint64_t rise = host_echo_rise_us();
    uint64_t high = host_echo_high_us();
    /* 向下一个 echo 边沿推进，但单次最多推进 ECHO_POLL_WINDOW_US，
     * 使驱动 polling 循环的 30ms 超时判定可达（远期 rise 不会被瞬间跳过）。 */
    if (t < rise) {
        uint64_t target = rise;
        if (rise - t > ECHO_POLL_WINDOW_US) target = t + ECHO_POLL_WINDOW_US;
        host_sim_advance_to(target);
        return target >= rise;                /* 推进到变高时刻返回高；窗口内未达返回低 */
    }
    if (t < rise + high) {
        host_sim_advance_to(rise + high);
        return false;                         /* 推进到变低时刻，echo 为低 */
    }
    return false;
}

/* ─────────────────────────────────────────────────────────
 * Host GPIO 中断实现（支持中断锁语义 + pending 队列，用于单元测试）
 * ───────────────────────────────────────────────────────── */
#define HOST_MAX_GPIO_PIN  50
#define HOST_MAX_PENDING   64

static pal_gpio_isr_t  s_gpio_isr[HOST_MAX_GPIO_PIN] = {NULL};
static void            *s_gpio_isr_arg[HOST_MAX_GPIO_PIN] = {NULL};
static uint32_t         s_isr_call_count[HOST_MAX_GPIO_PIN] = {0};

/* v2.2 G3（Phase 1.5，2026-07-01）：GPIO service 首次锁定的 prio。
 * 由 s_gpio_service_mux 同步。host 支持多线程，需真实 mutex。 */
static bool             s_gpio_service_initialized = false;
static pal_irq_prio_t   s_gpio_service_prio        = PAL_IRQ_PRIO_NORMAL;
static pthread_mutex_t  s_gpio_service_mux         = PTHREAD_MUTEX_INITIALIZER;

/* Pending 中断队列（中断锁语义仿真） */
static uint32_t s_pending_gpio[HOST_MAX_PENDING];
static uint32_t s_pending_count = 0;

/* 中断锁状态（嵌套计数，用于单测断言） */
static int s_irq_lock_depth = 0;

/* 刷新所有 pending 的中断（当中断锁释放时调用） */
static void flush_pending_interrupts(void)
{
    while (s_pending_count > 0) {
        s_pending_count--;
        uint32_t pin = s_pending_gpio[s_pending_count];

        if (pin < HOST_MAX_GPIO_PIN && s_gpio_isr[pin] != NULL) {
            s_isr_call_count[pin]++;
            s_gpio_isr[pin](s_gpio_isr_arg[pin]);
        }
    }
}

wink_status_t pal_gpio_enable_interrupt_ex(wink_pin_t pin, pal_gpio_intr_t intr_type,
                                         pal_irq_prio_t prio, pal_gpio_isr_t callback, void *arg)
{
    (void)intr_type;

    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
    if (callback == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (prio >= PAL_IRQ_PRIO_COUNT) {
        return WINK_ERR_INVALID_ARG;
    }

    /* v2.2 G2（Phase 1.5，2026-07-01）：与 ESP32 对齐，GPIO 路径拒接 REALTIME。
     * host 单线程模型下无 per-pin 抢占，REALTIME 本无处映射；显式拒接以让
     * "仿真通过 → 真机通过"关系严格成立（ADR-0012）。 */
    if (prio == PAL_IRQ_PRIO_REALTIME) {
        return WINK_ERR_UNSUPPORTED;
    }

    /* v2.2 G3：GPIO service 首次锁定 prio。host 支持并发，需 mutex 保护。
     * 一旦锁定，进程生命周期内不再释放（见 pal_hal.h 契约）。 */
    pthread_mutex_lock(&s_gpio_service_mux);
    if (s_gpio_service_initialized) {
        if (prio != s_gpio_service_prio) {
            pthread_mutex_unlock(&s_gpio_service_mux);
            return WINK_ERR_INVALID_ARG;   /* G3: prio 冲突，本次拒接 */
        }
    } else {
        s_gpio_service_prio        = prio;
        s_gpio_service_initialized = true;
    }
    pthread_mutex_unlock(&s_gpio_service_mux);

    s_gpio_isr[pin] = callback;
    s_gpio_isr_arg[pin] = arg;
    s_isr_call_count[pin] = 0;

    return WINK_OK;
}

wink_status_t pal_gpio_disable_interrupt(wink_pin_t pin)
{
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }

    s_gpio_isr[pin] = NULL;
    return WINK_OK;
}

/**
 * @brief 手动触发 GPIO 中断（仅 Host 平台可用，用于单测）
 *
 * ⚠️ 中断锁语义实现（v2.0 核心特性）：
 * 如果当前持有中断锁，则只记录到 pending 队列，不实际调用 ISR。
 * ISR 将在中断锁释放（最外层 restore）时统一执行。
 *
 * 单测用法：
 *   pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE, my_isr, NULL);
 *
 *   uint32_t mask = pal_irq_save();
 *   pal_host_trigger_gpio_interrupt(TEST_PIN);
 *   TEST_ASSERT_EQUAL(0, pal_host_get_isr_call_count(TEST_PIN));  // ✅ 未执行
 *   pal_irq_restore(mask);
 *   TEST_ASSERT_EQUAL(1, pal_host_get_isr_call_count(TEST_PIN));  // ✅ 已执行
 */
void pal_host_trigger_gpio_interrupt(wink_pin_t pin)
{
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) return;
    if (s_gpio_isr[pin] == NULL) return;

    if (s_irq_lock_depth > 0) {
        /* 持有中断锁 → 加入 pending 队列，延迟执行 */
        if (s_pending_count < HOST_MAX_PENDING) {
            s_pending_gpio[s_pending_count++] = (uint32_t)pin;
        }
    } else {
        /* 无锁 → 立即执行 */
        s_isr_call_count[pin]++;
        s_gpio_isr[pin](s_gpio_isr_arg[pin]);
    }
}

uint32_t pal_host_get_isr_call_count(wink_pin_t pin)
{
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) return 0;
    return s_isr_call_count[pin];
}

/**
 * @brief 获取当前 pending 中断数量（用于单测断言）
 */
uint32_t pal_host_get_pending_count(void)
{
    return s_pending_count;
}

/**
 * @brief 获取中断锁嵌套深度（用于单测断言，检测锁泄漏）
 */
int pal_host_get_irq_lock_depth(void)
{
    return s_irq_lock_depth;
}

/* ─────────────────────────────────────────────────────────
 * 共享中断机制（Host 单元测试支持）
 * ───────────────────────────────────────────────────────── */

#define HOST_MAX_SHARED_HANDLERS  4
#define HOST_MAX_SHARED_IRQS      16

typedef struct {
    pal_irq_shared_handler_t handler;
    void                     *arg;
} host_shared_entry_t;

typedef struct {
    host_shared_entry_t entries[HOST_MAX_SHARED_HANDLERS];
    uint8_t count;
} host_shared_chain_t;

static host_shared_chain_t *s_host_shared_chain[HOST_MAX_SHARED_IRQS] = {NULL};

/* 共享中断 wrapper（Host 仿真版） */
static void PAL_ISR host_shared_irq_wrapper(void *arg)
{
    uint32_t irq_num = (uint32_t)(uintptr_t)arg;
    if (irq_num >= HOST_MAX_SHARED_IRQS) {
        return;
    }

    host_shared_chain_t *chain = s_host_shared_chain[irq_num];
    if (chain == NULL) {
        return;
    }

    /* ✅ v2.0 语义：始终遍历调用所有 handler，不提前终止 */
    for (uint8_t i = 0; i < chain->count; i++) {
        if (chain->entries[i].handler != NULL) {
            (void)chain->entries[i].handler(chain->entries[i].arg);
        }
    }
}

/* ─────────────────────────────────────────────────────────
 * 逻辑中断核心接口（Host 单元测试支持）
 * ───────────────────────────────────────────────────────── */
#define HOST_MAX_IRQ  32
static pal_isr_t s_host_irq_table[HOST_MAX_IRQ] = {NULL};
static void *s_host_irq_arg[HOST_MAX_IRQ] = {NULL};
static uint32_t s_host_irq_call_count[HOST_MAX_IRQ] = {0};

/* v2.1 G1：硬件直连中断的无参 trampoline（host 镜像 ESP32 实现）。
 * 旧实现 `(pal_isr_t)handler` 是 void(*)(void) → void(*)(void*) 的非法 cast；
 * 改为 trampoline 后签名清洁，host 单线程下 NULL 检查即足够。 */
static pal_direct_isr_t s_host_direct_handlers[HOST_MAX_IRQ] = {NULL};

wink_status_t pal_irq_enable(uint32_t irq_num, pal_irq_prio_t prio,
                              pal_isr_t handler, void *arg)
{
    if (irq_num >= HOST_MAX_IRQ || handler == NULL || prio >= PAL_IRQ_PRIO_COUNT) {
        return WINK_ERR_INVALID_ARG;
    }

    /* v2.2 G2（Phase 1.5，2026-07-01）：与 ESP32 对齐，默认拒接 REALTIME。
     * 让"仿真通过 → 真机通过"关系严格成立（ADR-0012 契约诚实）。
     * 静态校验类测试可编译期显式 opt-in，走**受控**放行。 */
    if (prio == PAL_IRQ_PRIO_REALTIME) {
#if defined(WINK_HOST_ALLOW_REALTIME_FOR_TESTING)
        /* opt-in：接受但一次性告警，避免循环中刷屏 */
        static bool s_realtime_warn_emitted = false;
        if (!s_realtime_warn_emitted) {
            fprintf(stderr,
                    "[pal_irq WARN] host: REALTIME priority accepted for "
                    "testing only; ESP32 target returns WINK_ERR_UNSUPPORTED.\n");
            s_realtime_warn_emitted = true;
        }
        /* fallthrough：落到 dispatch 路径，与 HIGHEST 等价 */
#else
        return WINK_ERR_UNSUPPORTED;
#endif
    }

    s_host_irq_table[irq_num] = handler;
    s_host_irq_arg[irq_num] = arg;
    s_host_irq_call_count[irq_num] = 0;
    return WINK_OK;
}

wink_status_t pal_irq_disable(uint32_t irq_num)
{
    if (irq_num >= HOST_MAX_IRQ) {
        return WINK_ERR_INVALID_ARG;
    }
    s_host_irq_table[irq_num] = NULL;
    s_host_irq_arg[irq_num] = NULL;
    /* v2.1：清理 direct-connect 槽位，对齐 ESP32 实现 */
    s_host_direct_handlers[irq_num] = NULL;
    return WINK_OK;
}

static void host_direct_trampoline(void *arg)
{
    uint32_t irq_num = (uint32_t)(uintptr_t)arg;
    if (irq_num >= HOST_MAX_IRQ) {
        return;
    }
    pal_direct_isr_t h = s_host_direct_handlers[irq_num];
    if (h != NULL) {
        h();
    }
}

wink_status_t pal_irq_direct_connect(uint32_t irq_num, pal_direct_isr_t handler)
{
    if (irq_num >= HOST_MAX_IRQ || handler == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    s_host_direct_handlers[irq_num] = handler;
    wink_status_t st = pal_irq_enable(irq_num, PAL_IRQ_PRIO_NORMAL,
                                       host_direct_trampoline,
                                       (void *)(uintptr_t)irq_num);
    if (wink_status_is_error(st)) {
        s_host_direct_handlers[irq_num] = NULL;
    }
    return st;
}

wink_status_t pal_irq_shared_register(uint32_t irq_num, pal_irq_prio_t prio,
                                       pal_irq_shared_handler_t handler, void *arg)
{
    if (irq_num >= HOST_MAX_SHARED_IRQS || handler == NULL || prio >= PAL_IRQ_PRIO_COUNT) {
        return WINK_ERR_INVALID_ARG;
    }

    host_shared_chain_t *old_chain = s_host_shared_chain[irq_num];
    host_shared_chain_t *new_chain = NULL;

    if (old_chain == NULL) {
        /* 第一个 handler：创建新链 */
        new_chain = malloc(sizeof(host_shared_chain_t));
        if (new_chain == NULL) {
            return WINK_ERR_NO_MEM;
        }
        memset(new_chain, 0, sizeof(host_shared_chain_t));
    } else {
        /* 已有 handler：复制旧链，添加新 handler */
        if (old_chain->count >= HOST_MAX_SHARED_HANDLERS) {
            return WINK_ERR_NO_MEM;
        }
        new_chain = malloc(sizeof(host_shared_chain_t));
        if (new_chain == NULL) {
            return WINK_ERR_NO_MEM;
        }
        memcpy(new_chain, old_chain, sizeof(host_shared_chain_t));
    }

    /* 追加新 handler */
    uint8_t idx = new_chain->count;
    new_chain->entries[idx].handler = handler;
    new_chain->entries[idx].arg = arg;
    new_chain->count++;

    /* 原子替换指针 */
    s_host_shared_chain[irq_num] = new_chain;
    free(old_chain);

    /* 如果是第一个 handler，注册共享 wrapper */
    if (new_chain->count == 1) {
        return pal_irq_enable(irq_num, prio, host_shared_irq_wrapper,
                              (void *)(uintptr_t)irq_num);
    }

    return WINK_OK;
}

void pal_irq_set_pending(uint32_t irq_num)
{
    if (irq_num < HOST_MAX_IRQ && s_host_irq_table[irq_num] != NULL) {
        if (s_irq_lock_depth > 0) {
            /* 持有锁时不立即执行（单测可检查 pending 状态） */
        } else {
            s_host_irq_call_count[irq_num]++;
            s_host_irq_table[irq_num](s_host_irq_arg[irq_num]);
        }
    }
}

void pal_irq_clear_pending(uint32_t irq_num)
{
    (void)irq_num;
}

void pal_irq_synchronize(uint32_t irq_num)
{
    (void)irq_num;
    /* Host 单线程模型，无需同步 */
}

void pal_host_trigger_logical_interrupt(uint32_t irq_num)
{
    pal_irq_set_pending(irq_num);
}

uint32_t pal_host_get_logical_isr_call_count(uint32_t irq_num)
{
    if (irq_num >= HOST_MAX_IRQ) return 0;
    return s_host_irq_call_count[irq_num];
}

void pal_host_reset_isr_stats(void)
{
    memset(s_isr_call_count, 0, sizeof(s_isr_call_count));
    memset(s_host_irq_call_count, 0, sizeof(s_host_irq_call_count));
    s_pending_count = 0;
    s_irq_lock_depth = 0;

    /* v2.2 G3：单测隔离——重置 GPIO service 锁定状态。
     * 生产 API 不提供解锁（见 pal_hal.h 契约）；这里只在 host 测试钩子里放行，
     * 以便每个用例从干净的 uninitialized 状态开始。 */
    pthread_mutex_lock(&s_gpio_service_mux);
    s_gpio_service_initialized = false;
    s_gpio_service_prio        = PAL_IRQ_PRIO_NORMAL;
    pthread_mutex_unlock(&s_gpio_service_mux);

    /* 一并清 GPIO handler 表 —— 单测每个 case 应从零状态开始 */
    memset(s_gpio_isr, 0, sizeof(s_gpio_isr));
    memset(s_gpio_isr_arg, 0, sizeof(s_gpio_isr_arg));
}

/* ─────────────────────────────────────────────────────────
 * 中断锁实现（双等级语义，Host 平台支持精确检测）
 * ───────────────────────────────────────────────────────── */

uint32_t pal_irq_save(void)
{
    /* 返回旧的锁深度（用于 restore 时判断是否是最外层） */
    uint32_t old_depth = s_irq_lock_depth;
    s_irq_lock_depth++;
    return old_depth;
}

uint32_t pal_irq_save_rtos_safe(void)
{
    /* Host 单线程模型下，rtos_safe 与全屏蔽行为一致 */
    return pal_irq_save();
}

void pal_irq_restore(uint32_t mask)
{
    if (s_irq_lock_depth <= 0) {
        /* 不匹配的 restore（没有对应 save），单测可检测到
         * 打印警告但不崩溃，便于测试定位 */
        fprintf(stderr, "WARNING: pal_irq_restore() called without matching save()!\n");
        return;
    }

    s_irq_lock_depth--;

    /* 只有当锁深度变为 0（真正释放），且 save 之前也是未锁状态（mask == 0），
     * 才刷新 pending 中断。这样嵌套锁内层不会误触发 flush。 */
    if (s_irq_lock_depth == 0 && mask == 0) {
        flush_pending_interrupts();
    }
}

wink_status_t pal_pwm_init(uint8_t channel, uint32_t freq) {
    uint8_t timer_num = 0;
    wink_status_t rs = pal_pwm_router_acquire(channel, freq, &timer_num);
    if (wink_status_is_error(rs)) { return rs; }
    rs = pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL, channel, "pal_hal_host");
    if (wink_status_is_error(rs)) {
        pal_pwm_router_release(channel);   /* roll back router reservation */
        return rs;
    }
    return WINK_OK;
}
wink_status_t pal_pwm_set_duty(uint8_t channel, float duty) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    host_record_pwm(channel, duty);
    return WINK_OK;
}

void pal_pwm_deinit(uint8_t channel) {
    if (!pal_pwm_router_channel_ready(channel)) { return; }   /* no-op if uninitialized */
    /* gcc16 不因 (void) 抑制 warn_unused_result：先赋值再丢弃，释放/deinit best-effort 不失败。*/
    wink_status_t _rel = pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, channel, "pal_hal_host");
    (void)_rel;
    pal_pwm_router_release(channel);
}

/* Phase 2：host I2C 事务捕获（供 ssd1306 单测验证 flush 发出正确事务） */
extern void host_record_i2c(uint8_t port, uint16_t addr, uint32_t write_len);

wink_status_t pal_i2c_transfer(uint8_t port, uint16_t addr,
                      const uint8_t *w, uint32_t wl, uint8_t *r, uint32_t rl) {
    (void)w; (void)r; (void)rl;
    host_record_i2c(port, addr, wl);
    return WINK_OK;
}

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us) {
    /* Phase 4 Task 4-2：host 直接读 echo 脉宽（虚拟时间下同步），不经 pal_gpio_read 协作推进。
     * 这是非阻塞 DAL 的 echo 时序 SSOT（Phase 4 Task 4-6 决策：保留协作推进仅供过渡 blocking read）。 */
    if (pulse_us == NULL) { return WINK_ERR_INVALID_ARG; }
    if (pin != (wink_pin_t)host_echo_pin()) { return WINK_ERR_UNSUPPORTED; }   /* 无 pin 映射（直至 virtual registry 接入） */
    uint64_t rise = host_echo_rise_us();
    if (rise > timeout_us) { return WINK_ERR_TIMEOUT; }            /* echo 起始晚于超时 */
    *pulse_us = (uint32_t)host_echo_high_us();
    (void)level;   /* host echo 即高电平脉宽 */
    return WINK_OK;
}

/* ─────────────────────────────────────────────────────────
 * Debug Output（PAL 统一接口）
 * ───────────────────────────────────────────────────────── */

void pal_debug_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

/* ─────────────────────────────────────────────────────────
 * 超声波传感器 PAL 实现 (Host 平台)
 * ───────────────────────────────────────────────────────── */

static void (*s_sim_trigger_fn)(uint16_t) = NULL;
static uint32_t (*s_sim_measure_fn)(uint16_t) = NULL;

void host_register_sim_ultrasonic(void (*trigger_fn)(uint16_t), uint32_t (*measure_fn)(uint16_t)) {
    s_sim_trigger_fn = trigger_fn;
    s_sim_measure_fn = measure_fn;
}

wink_status_t pal_hal_ultrasonic_init(uint16_t echo_pin) {
    (void)echo_pin;
    return WINK_OK;
}

wink_status_t pal_hal_ultrasonic_trigger(uint16_t trigger_pin) {
    if (s_sim_trigger_fn) {
        s_sim_trigger_fn(trigger_pin);
    }
    return WINK_OK;
}

wink_status_t pal_hal_ultrasonic_measure_pulse_us(
    uint16_t echo_pin,
    uint32_t timeout_us,
    uint32_t *pulse_us
) {
    if (pulse_us == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (s_sim_measure_fn) {
        uint32_t p = s_sim_measure_fn(echo_pin);
        if (p >= timeout_us) {
            return WINK_ERR_TIMEOUT;
        }
        *pulse_us = p;
        return WINK_OK;
    }
    return pal_gpio_pulse_in((wink_pin_t)echo_pin, true, timeout_us, pulse_us);
}

void pal_hal_ultrasonic_deinit(void) {
}

