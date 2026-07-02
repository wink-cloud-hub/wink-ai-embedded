/**
 * @file pal_hal_wasm.c
 * @brief Wasm 仿真端 PAL HAL 适配（GPIO/PWM/I2C/中断，v2.0 SMP 安全版）。
 *        仅 HAL；OSAL 见 pal_osal_wasm.c；entry 见 wasm_entry.c；JS 契约见 wasm_bridge.h。
 *
 * 中断桥（方案 C：Poll 模型）：
 *   pal_gpio_enable_interrupt → js_pal_register_interrupt（仅写 JS 侧 pending 表映射）
 *   pal_wasm_dispatch_pending_interrupts → 由 wink_runtime.c tick 边界调用，drain JS pending 队列
 *   旧 _trigger_wasm_interrupt 导出已移除（wasm_entry.c），彻底消除 Asyncify sleeping 窗口重入面。
 *
 * 物理退化中间件（ADR-0009 Wave 2 Task 3）：
 *   pal_gpio_read  → 边界检查 + 抖动状态机（per-pin ctx，bounce_us=0 时旁路）
 *   pal_i2c_transfer → PRNG 驱动确定性丢包（drop_permil=0 时旁路）
 *   故障配置全部位于 pal_wasm_physical.c，通过 pal_wasm_get_* 内部 helper 读取；
 *   零退化时只多一次内存读，热路径开销可忽略。
 */
#include "pal_hal.h"
#include "pal_irq.h"
#include "pal_pwm_router.h"
#include "pal_osal.h"
#include "pal_debug.h"
#include "wasm_bridge.h"
#include "pal_wasm_internal.h"
#include "wink_sim_physical.h"
#include "pal_shared_chain.h"  /* target-private RCU chain algorithm (PLAN-20260701-PAL-TARGET-P1-MAINT Task 1) */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode) {
    (void)pin; (void)mode;            /* 仿真下无需硬件配置 */
    return WINK_OK;
}

wink_status_t pal_gpio_write(wink_pin_t pin, bool level) {
    if (pin < 0 || pin >= WASM_SIM_MAX_PINS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) {
        return WINK_ERR_INVALID_STATE;
    }
    js_pal_gpio_write((uint32_t)pin, level);
    return WINK_OK;
}

wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level) {
    if (out_level == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    *out_level = false; /* Defense-in-depth initialization */

    if (pin < 0 || pin >= WASM_SIM_MAX_PINS) {
        return WINK_ERR_INVALID_ARG;
    }

    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) {
        return WINK_ERR_INVALID_STATE;
    }

    /* Step 1: 从 JS 侧获取理想电平（UniSim 宏观物理状态）。 */
    bool ideal = js_pal_gpio_read(pin);

    /* Step 2: 退化中间件（仅当 bounce_us > 0 时生效）。
     * bounce_us=0 是默认零退化路径，热路径只多一次内存读 + 一次比较。 */
    uint32_t bounce_us = pal_wasm_get_bounce_us();
    if (bounce_us > 0u) {
        wink_phys_debounce_ctx_t *ctx = pal_wasm_get_debounce_ctx(pin);
        /* ctx 不可能为 NULL（pin 已过边界检查），但仍做防御式判断——
         * 假设 WASM_SIM_MAX_PINS 未来在两处不同步，至少不会崩。 */
        if (ctx != NULL) {
            /* Task 8 故障审计：在进入抖动窗口的瞬间（in_bounce false→true）
             * 记录一条审计事件。每次抖动 episode 只记录一次，避免把环形日
             * 志被采样周期内的多次同 pin 调用刷爆。CI 侧由 sequence 与
             * timestamp 区分独立的 bounce 触发。 */
            bool was_in_bounce = ctx->in_bounce;
            bool result = wink_phys_debounce_step(ctx, ideal, pal_os_get_us(), bounce_us);
            if (!was_in_bounce && ctx->in_bounce) {
                pal_wasm_log_fault(FAULT_TYPE_GPIO_BOUNCE, pin);
            }
            *out_level = result;
            return WINK_OK;
        }
    }

    /* 无退化 → 原样返回（兼容路径）。 */
    *out_level = ideal;
    return WINK_OK;
}

/* ─────────────────────────────────────────────────────────
 * WASM GPIO 中断状态表（支持中断锁语义仿真）
 * ───────────────────────────────────────────────────────── */
#define WASM_MAX_GPIO_PIN  50
static pal_gpio_isr_t s_gpio_isr[WASM_MAX_GPIO_PIN] = {NULL};
static void           *s_gpio_isr_arg[WASM_MAX_GPIO_PIN] = {NULL};
static pal_gpio_intr_t s_gpio_intr_type[WASM_MAX_GPIO_PIN] = {PAL_GPIO_INTR_DISABLE};
static bool            s_gpio_last_level[WASM_MAX_GPIO_PIN] = {false};

/* v2.2 G3（Phase 1.5，2026-07-01）：GPIO service 首次锁定的 prio。
 * WASM 是单线程执行环境（无 SharedArrayBuffer / pthread），无需 mutex；
 * 保留与 ESP32/host 对齐的双字段结构，语义一致。 */
static bool             s_gpio_service_initialized = false;
static pal_irq_prio_t   s_gpio_service_prio        = PAL_IRQ_PRIO_NORMAL;

/* 中断锁状态（支持嵌套计数） */
static uint32_t s_irq_lock_nest_count = 0;

/* Pending 中断队列（支持延迟分发 + 优先级排序） */
#define WASM_MAX_PENDING  64

typedef struct {
    uint32_t          irq_num;       /* GPIO 引脚号 */
    pal_irq_prio_t     prio;           /* 中断优先级 */
    uint32_t          target_tick;     /* 延迟到目标 tick 后分发 */
    bool              is_gpio;         /* true = GPIO 中断 */
} wasm_pending_irq_t;

static wasm_pending_irq_t s_pending_queue[WASM_MAX_PENDING];
static uint32_t s_pending_count = 0;
static uint32_t s_current_tick = 0;

/* 伪随机数生成（用于抖动模拟，Pareto 长尾分布） */
static uint32_t pseudo_rand(void)
{
    static uint32_t seed = 0x12345678;
    seed = seed * 1103515245 + 12345;
    return seed;
}

/* 计算中断分发的目标 tick（Pareto 长尾分布，v2.0 改进）
 * 80% 中断：1-2 tick 短延迟
 * 20% 中断：3-5 tick 长尾延迟（模拟真实硬件 Flash Cache Miss）
 */
static uint32_t calc_target_tick(void)
{
    uint32_t r = pseudo_rand() % 100;
    if (r < 80) {
        /* 80% 短延迟：1-2 tick */
        return s_current_tick + 1 + (pseudo_rand() % 2);
    } else {
        /* 20% 长尾延迟：3-5 tick */
        return s_current_tick + 3 + (pseudo_rand() % 3);
    }
}

/* 按优先级对 pending 队列排序（高优先级在前） */
static void sort_pending_by_priority(void)
{
    for (uint32_t i = 0; i < s_pending_count; i++) {
        for (uint32_t j = i + 1; j < s_pending_count; j++) {
            if (s_pending_queue[j].prio > s_pending_queue[i].prio) {
                wasm_pending_irq_t tmp = s_pending_queue[i];
                s_pending_queue[i] = s_pending_queue[j];
                s_pending_queue[j] = tmp;
            }
        }
    }
}

/* ─────────────────────────────────────────────────────────
 * GPIO 中断接口实现（WASM 平台，v2.0 支持中断锁语义）
 * ───────────────────────────────────────────────────────── */

wink_status_t pal_gpio_enable_interrupt_ex(wink_pin_t pin, pal_gpio_intr_t intr_type,
                                         pal_irq_prio_t prio, pal_gpio_isr_t callback, void *arg)
{
    if (pin < 0 || pin >= WASM_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
    if (callback == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (prio >= PAL_IRQ_PRIO_COUNT) {
        return WINK_ERR_INVALID_ARG;
    }

    /* v2.2 G2（Phase 1.5，2026-07-01）：与 ESP32 对齐，GPIO 路径拒接 REALTIME。
     * WASM 单线程模型无 per-pin 抢占；显式拒接以让"仿真通过 → 真机通过"关系
     * 严格成立（ADR-0012）。 */
    if (prio == PAL_IRQ_PRIO_REALTIME) {
        return WINK_ERR_UNSUPPORTED;
    }

    /* v2.2 G3：GPIO service 首次锁定 prio。WASM 单线程无 mutex 需求，
     * 直接检查即可。一旦锁定，进程生命周期内不再释放（见 pal_hal.h 契约）。 */
    if (s_gpio_service_initialized) {
        if (prio != s_gpio_service_prio) {
            return WINK_ERR_INVALID_ARG;   /* G3: prio 冲突，本次拒接 */
        }
    } else {
        s_gpio_service_prio        = prio;
        s_gpio_service_initialized = true;
    }

    s_gpio_isr[pin] = callback;
    s_gpio_isr_arg[pin] = arg;
    s_gpio_intr_type[pin] = intr_type;

    /* 注册到 JS 侧（仅写映射关系，实际中断由 JS 检测后写入 pending 队列） */
    uint32_t callback_index = (uint32_t)(uintptr_t)callback;
    uint32_t arg_ptr        = (uint32_t)(uintptr_t)arg;
    js_pal_register_interrupt((uint32_t)pin, callback_index, arg_ptr);

    return WINK_OK;
}

wink_status_t pal_gpio_disable_interrupt(wink_pin_t pin)
{
    if (pin < 0 || pin >= WASM_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }

    s_gpio_isr[pin] = NULL;
    s_gpio_intr_type[pin] = PAL_GPIO_INTR_DISABLE;
    js_pal_deregister_interrupt((uint32_t)pin);

    return WINK_OK;
}

/**
 * @brief GPIO 电平变化时检测中断条件（由仿真器内部调用）
 *
 * ⚠️ 关键仿真特性（v2.0）：
 * 此函数只标记 pending，不立即调用 ISR（模拟真实硬件的中断延迟）。
 * 中断将在 tick 边界统一分发，且遵守中断锁语义。
 */
void pal_wasm_gpio_level_changed(wink_pin_t pin, bool new_level)
{
    if (pin < 0 || pin >= WASM_MAX_GPIO_PIN) return;
    if (s_gpio_isr[pin] == NULL) return;

    bool trigger = false;
    bool old_level = s_gpio_last_level[pin];

    switch (s_gpio_intr_type[pin]) {
        case PAL_GPIO_INTR_RISING_EDGE:
            trigger = !old_level && new_level;
            break;
        case PAL_GPIO_INTR_FALLING_EDGE:
            trigger = old_level && !new_level;
            break;
        case PAL_GPIO_INTR_ANY_EDGE:
            trigger = old_level != new_level;
            break;
        case PAL_GPIO_INTR_LOW_LEVEL:
            trigger = !new_level;
            break;
        case PAL_GPIO_INTR_HIGH_LEVEL:
            trigger = new_level;
            break;
        default:
            break;
    }

    if (trigger && s_pending_count < WASM_MAX_PENDING) {
        s_pending_queue[s_pending_count].irq_num = (uint32_t)pin;
        s_pending_queue[s_pending_count].prio = PAL_IRQ_PRIO_NORMAL;
        s_pending_queue[s_pending_count].target_tick = calc_target_tick();
        s_pending_queue[s_pending_count].is_gpio = true;
        s_pending_count++;
    }

    s_gpio_last_level[pin] = new_level;
}

/**
 * @brief Tick 边界统一分发 pending 中断（由仿真主循环调用）
 *
 * 模拟真实硬件：中断只在 CPU 指令边界触发，不会在指令执行中间插入。
 *
 * ⚠️ 中断锁语义实现（v2.0 核心特性）：
 * 如果当前持有中断锁，则不分发任何中断，所有中断继续 pending。
 * 这与 ESP32 禁用中断后 ISR 延迟到中断恢复后执行的行为一致。
 */
void pal_wasm_dispatch_pending_irqs(void)
{
    s_current_tick++;

    /* ✅ 中断锁语义：持有锁时不分发任何中断 */
    if (s_irq_lock_nest_count > 0) {
        return;
    }

    /* 先按优先级排序 */
    sort_pending_by_priority();

    /* 分发所有已到期的 pending 中断 */
    uint32_t write_idx = 0;
    for (uint32_t read_idx = 0; read_idx < s_pending_count; read_idx++) {
        wasm_pending_irq_t *item = &s_pending_queue[read_idx];

        if (item->target_tick <= s_current_tick) {
            /* 已到期，执行 ISR */
            if (item->is_gpio) {
                uint32_t pin = item->irq_num;
                if (pin < WASM_MAX_GPIO_PIN && s_gpio_isr[pin] != NULL) {
                    s_gpio_isr[pin](s_gpio_isr_arg[pin]);
                }
            } else {
                /* 逻辑中断执行（Phase 2 扩展） */
            }
            /* 不写回，相当于移除 */
        } else {
            /* 未到期，保留到下一轮 */
            if (write_idx != read_idx) {
                s_pending_queue[write_idx] = s_pending_queue[read_idx];
            }
            write_idx++;
        }
    }

    s_pending_count = write_idx;
}

/* ─────────────────────────────────────────────────────────
 * 共享中断机制（WASM 仿真实现）
 * ─────────────────────────────────────────────────────────
 * PLAN-20260701-PAL-TARGET-P1-MAINT Task 1：责任链数据结构与算法层已下沉
 * 到 `targets/common/src/pal_shared_chain.c`。wasm 单线程仿真使用简化路径
 * （同步 ops 全 NULL），语义与 host 一致（且与 ESP32 SMP 版本 R-1 兼容）。 */

#define WASM_MAX_SHARED_IRQS      16

static pal_shared_chain_t *s_wasm_shared_chain[WASM_MAX_SHARED_IRQS] = {NULL};

/* 单线程：无 mux、无 synchronize；ops 传 NULL 即走算法层简化路径。 */
#define S_WASM_SHARED_SYNC_OPS  NULL

/* 共享中断 wrapper（WASM 仿真版，按注册顺序调用所有 handler） */
static void PAL_ISR wasm_shared_irq_wrapper(void *arg)
{
    uint32_t irq_num = (uint32_t)(uintptr_t)arg;
    if (irq_num >= WASM_MAX_SHARED_IRQS) {
        return;
    }

    /* ✅ v2.0 语义：始终遍历调用所有 handler，不提前终止 */
    (void)pal_shared_chain_dispatch(s_wasm_shared_chain[irq_num]);
}

/* ─────────────────────────────────────────────────────────
 * 逻辑中断核心接口（WASM 仿真实现）
 * ───────────────────────────────────────────────────────── */

#define WASM_MAX_IRQ  32
static pal_isr_t s_wasm_irq_table[WASM_MAX_IRQ] = {NULL};
static void *s_wasm_irq_arg[WASM_MAX_IRQ] = {NULL};

/* v2.1 G1：硬件直连中断的无参 trampoline（WASM 镜像 ESP32/host 实现）。
 * 旧实现 `(pal_isr_t)handler` 是 void(*)(void) → void(*)(void*) 的非法 cast；
 * 改为 trampoline 后签名清洁，WASM 单线程下 NULL 检查即足够。 */
static pal_direct_isr_t s_wasm_direct_handlers[WASM_MAX_IRQ] = {NULL};

wink_status_t pal_irq_enable(uint32_t irq_num, pal_irq_prio_t prio,
                              pal_isr_t handler, void *arg)
{
    if (irq_num >= WASM_MAX_IRQ || handler == NULL || prio >= PAL_IRQ_PRIO_COUNT) {
        return WINK_ERR_INVALID_ARG;
    }

    /* v2.2 G2（Phase 1.5，2026-07-01）：与 ESP32 对齐，默认拒接 REALTIME。
     * 让"仿真通过 → 真机通过"关系严格成立（ADR-0012 契约诚实）。
     * 静态校验类测试可编译期显式 opt-in，走**受控**放行。 */
    if (prio == PAL_IRQ_PRIO_REALTIME) {
#if defined(WINK_HOST_ALLOW_REALTIME_FOR_TESTING)
        /* opt-in：接受但一次性告警。WASM 侧走 pal_debug_printf 保持日志同源。 */
        static bool s_realtime_warn_emitted = false;
        if (!s_realtime_warn_emitted) {
            pal_debug_printf("[pal_irq WARN] wasm: REALTIME priority accepted "
                             "for testing only; ESP32 target returns "
                             "WINK_ERR_UNSUPPORTED.\n");
            s_realtime_warn_emitted = true;
        }
        /* fallthrough：落到 dispatch 路径，与 HIGHEST 等价 */
#else
        return WINK_ERR_UNSUPPORTED;
#endif
    }

    s_wasm_irq_table[irq_num] = handler;
    s_wasm_irq_arg[irq_num] = arg;
    return WINK_OK;
}

wink_status_t pal_irq_disable(uint32_t irq_num)
{
    if (irq_num >= WASM_MAX_IRQ) {
        return WINK_ERR_INVALID_ARG;
    }
    s_wasm_irq_table[irq_num] = NULL;
    s_wasm_irq_arg[irq_num] = NULL;
    /* v2.1：清理 direct-connect 槽位，对齐 ESP32/host 实现 */
    s_wasm_direct_handlers[irq_num] = NULL;
    return WINK_OK;
}

static void wasm_direct_trampoline(void *arg)
{
    uint32_t irq_num = (uint32_t)(uintptr_t)arg;
    if (irq_num >= WASM_MAX_IRQ) {
        return;
    }
    pal_direct_isr_t h = s_wasm_direct_handlers[irq_num];
    if (h != NULL) {
        h();
    }
}

wink_status_t pal_irq_direct_connect(uint32_t irq_num, pal_direct_isr_t handler)
{
    if (irq_num >= WASM_MAX_IRQ || handler == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    s_wasm_direct_handlers[irq_num] = handler;
    wink_status_t st = pal_irq_enable(irq_num, PAL_IRQ_PRIO_NORMAL,
                                       wasm_direct_trampoline,
                                       (void *)(uintptr_t)irq_num);
    if (wink_status_is_error(st)) {
        s_wasm_direct_handlers[irq_num] = NULL;
    }
    return st;
}

wink_status_t pal_irq_shared_register(uint32_t irq_num, pal_irq_prio_t prio,
                                       pal_irq_shared_handler_t handler, void *arg)
{
    if (irq_num >= WASM_MAX_SHARED_IRQS || handler == NULL || prio >= PAL_IRQ_PRIO_COUNT) {
        return WINK_ERR_INVALID_ARG;
    }

    /* ✅ 复用 targets/common 算法层；wasm 单线程 ops=NULL 走简化路径 */
    bool became_first = false;
    wink_status_t st = pal_shared_chain_append(
        &s_wasm_shared_chain[irq_num],
        S_WASM_SHARED_SYNC_OPS,
        irq_num,
        handler,
        arg,
        &became_first);
    if (wink_status_is_error(st)) {
        return st;
    }

    /* 首个 handler：注册共享 wrapper */
    if (became_first) {
        return pal_irq_enable(irq_num, prio, wasm_shared_irq_wrapper,
                              (void *)(uintptr_t)irq_num);
    }
    return WINK_OK;
}

void pal_irq_set_pending(uint32_t irq_num)
{
    if (irq_num < WASM_MAX_IRQ && s_wasm_irq_table[irq_num] != NULL) {
        if (s_pending_count < WASM_MAX_PENDING) {
            s_pending_queue[s_pending_count].irq_num = irq_num;
            s_pending_queue[s_pending_count].prio = PAL_IRQ_PRIO_NORMAL;
            s_pending_queue[s_pending_count].target_tick = calc_target_tick();
            s_pending_queue[s_pending_count].is_gpio = false;
            s_pending_count++;
        }
    }
}

void pal_irq_clear_pending(uint32_t irq_num)
{
    (void)irq_num;
    /* WASM 简化实现：不移除队列中已存在的 pending 项，
     * 分发时会检查表头指针为 NULL 则跳过 */
}

void pal_irq_synchronize(uint32_t irq_num)
{
    (void)irq_num;
    /* WASM 单线程模型，disable 后保证无 ISR 正在执行，无需同步 */
}

/* ─────────────────────────────────────────────────────────
 * 中断锁实现（WASM 仿真精确语义匹配）
 * ───────────────────────────────────────────────────────── */

uint32_t pal_irq_save(void)
{
    /* 记录锁之前的状态（支持嵌套） */
    uint32_t was_enabled = (s_irq_lock_nest_count == 0) ? 1 : 0;
    s_irq_lock_nest_count++;
    return was_enabled;
}

uint32_t pal_irq_save_rtos_safe(void)
{
    /* WASM 单线程模型下，rtos_safe 与全屏蔽行为一致
     * 真实 ESP32 上才区分优先级屏蔽，但 WASM 仿真简化处理 */
    return pal_irq_save();
}

void pal_irq_restore(uint32_t mask)
{
    if (s_irq_lock_nest_count > 0) {
        s_irq_lock_nest_count--;
        /* 只有最外层的 restore 才真正恢复中断并分发 pending
         * mask == 1 表示 save 之前是未加锁状态 */
        if (s_irq_lock_nest_count == 0 && mask) {
            /* 中断已恢复，立即分发所有 pending 的 ISR */
            pal_wasm_dispatch_pending_irqs();
        }
    }
}

/**
 * @brief 分发 JS pending 中断（方案 C：tick 边界主动拉取）。
 *
 * 循环调用 js_pal_poll_interrupt 直到队列为空（FIFO 顺序），对每个 pending 条目
 * 将 callback_index 还原为函数指针并调用 ISR。
 *
 * 调用方：wink_runtime.c 在 #ifdef SIMULATION 下、wink_app_delay_ms() 之前调用本函数。
 * 此时 Wasm 处于正常运行态（非 Asyncify sleeping），ISR 执行安全，无重入风险。
 */
void pal_wasm_dispatch_pending_interrupts(void) {
    uint32_t callback_index;
    uint32_t arg_ptr;
    /* drain 所有 pending 中断（FIFO）直到队列空 */
    while (js_pal_poll_interrupt(&callback_index, &arg_ptr)) {
        pal_gpio_isr_t isr = (pal_gpio_isr_t)(uintptr_t)callback_index;
        if (isr != NULL) {
            isr((void *)(uintptr_t)arg_ptr);
        }
    }
}

wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz) {
    uint8_t timer_num = 0;
    /* wasm 无资源表/硬件，但 router 提供通道/频率校验与槽位记账，保持与 host/esp32 一致。*/
    return pal_pwm_router_acquire(channel, frequency_hz, &timer_num);
}

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_cycle_percent) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    js_pal_pwm_set_duty(channel, duty_cycle_percent);
    return WINK_OK;
}

void pal_pwm_deinit(uint8_t channel) {
    pal_pwm_router_release(channel);   /* no-op if uninitialized */
}

wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                      const uint8_t *write_buf, uint32_t write_len,
                      uint8_t *read_buf, uint32_t read_len) {
    /* Step 1: 丢包判定（PRNG 确定性，§4.1 合规）。
     *
     * 设计说明：全局 PRNG 是有意的设计选择，保证"单种子复现全系统行为"。
     * 如果 I2C 丢包和 ADC 噪声独立 PRNG，那么改变 ADC 采样率不会影响
     * I2C 序列，但这也失去了"一个 seed = 整个系统的完整快照"的能力。
     * 当前选择：全局 PRNG，简化确定性复现（详见 pal_wasm_physical.c）。
     *
     * drop_permil=0 是零退化默认路径，热路径只多一次内存读 + 一次比较。 */
    uint16_t drop_permil = pal_wasm_get_i2c_drop_permil();
    if (drop_permil > 0u) {
        uint32_t prng_state = pal_wasm_get_prng_state();
        bool should_drop = wink_phys_bus_drop(drop_permil, &prng_state);
        pal_wasm_advance_prng_state(prng_state);  /* 回写推进后的状态 */
        if (should_drop) {
            /* Task 8 故障审计：丢包瞬间记录审计事件，pin_or_bus 字段
             * 复用为 I2C port，便于 CI 区分多总线场景。 */
            pal_wasm_log_fault(FAULT_TYPE_I2C_DROP, port);
            return WINK_ERR_IO;  /* 模拟总线故障，驱动超时退回机制触发 */
        }
    }

    /* Step 2: 正常传输（无退化路径）。 */
    return js_pal_i2c_transfer(port, dev_addr, write_buf, write_len, read_buf, read_len)
           ? WINK_OK : WINK_ERR_IO;
}

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us) {
    if (pulse_us == NULL) { return WINK_ERR_INVALID_ARG; }
    if (pin < 0 || pin >= WASM_SIM_MAX_PINS) { return WINK_ERR_INVALID_ARG; }
    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) { return WINK_ERR_INVALID_STATE; }
    (void)level; (void)timeout_us;
    *pulse_us = js_sim_measure_echo_pulse_us((uint32_t)pin);
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

