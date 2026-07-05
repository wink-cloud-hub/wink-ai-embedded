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
#include "pal_osal.h"
#define WINK_ALLOW_ADVANCED_IRQ_APIS
#include "pal_irq_advanced.h"
#include "pal_resource.h"
#include "pal_pwm_router.h"
#include "hal/pal_rmt.h"

#include "host_test_ctrl.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>   /* v2.2 G3：并发首次注册竞态保护 */

/* PAL 实现自身合法调用 WINK_BLOCKING API（pal_gpio_pulse_in / pal_rmt_pulse_capture_wait_armed）：
 * 抑制 -Wdeprecated-declarations 使 -Werror 下仍能编译。ADR-0017 层 2 严格模式生效时，
 * 相关阻塞声明会从 header 中消失，本 TU 也无法命中——那是预期。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

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
#define HOST_MAX_GPIO_PIN  50

#define HOST_MAX_LOOPBACKS 8
static struct {
    wink_pin_t pin_out;
    wink_pin_t pin_in;
    bool active;
} s_host_loopbacks[HOST_MAX_LOOPBACKS] = {0};

static wink_pin_t s_host_pwm_pins[PAL_PWM_CHANNELS] = {
    -1, 4, 5, -1, -1, -1, -1, -1  // Channel 1 -> GPIO4, Channel 2 -> GPIO5
};

wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode) {
    /* Track A（M1）：DAL 是资源占用 SSOT，PAL 层不再自 claim（否则与 DAL 语义 owner
     * 二次抢占同 pin，恒返 BUSY）。DAL init 已保证 claim 在此之前完成。 */
    (void)pin;
    (void)mode;
    return WINK_OK;
}

wink_status_t pal_gpio_set_direction(wink_pin_t pin, pal_gpio_mode_t mode) {
    /* Host simulation: all pins are always readable + writable regardless
     * of mode; direction change is a no-op that always succeeds. */
    (void)pin;
    (void)mode;
    return WINK_OK;
}

extern void sim_set_gpio_ideal(uint16_t pin, bool level);

/* Host RMT arm/wait_armed 状态机（软件触发脉冲捕获路径）:
 *   arm() 记录 arm 时刻并清空 last_rise/last_pulse；
 *   pal_gpio_write() 在 armed 且写目标经 loopback 到 s_host_rmt_pin 时，
 *     rise 时记 s_host_rmt_last_rise_us；fall 时记 pulse_us = now - rise。
 *   wait_armed() 优先返回记到的 pulse_us；未记到则按 timeout 语义返回。
 * 逻辑与真机 RMT 语义等价（"起始沿到反向沿"高电平段），且完全依赖 host sim
 * 时间推进（pal_os_busy_wait_us / 显式 advance），无需真实并发。
 *
 * 定义放在 pal_gpio_write 之前以便其查询 armed/pin 状态；实体 API 位于文件下方。 */
static wink_pin_t s_host_rmt_pin         = -1;
static bool       s_host_rmt_armed       = false;
static uint64_t   s_host_rmt_last_rise_us  = 0;
static uint32_t   s_host_rmt_last_pulse_us = 0;

static void host_rmt_note_edge(bool level) {
    if (!s_host_rmt_armed) { return; }
    uint64_t now = host_sim_time_us();
    if (level) {
        /* rising edge：记录起始时刻 */
        s_host_rmt_last_rise_us = now;
    } else {
        /* falling edge：仅在此前 rising 已被记录时计算脉宽 */
        if (s_host_rmt_last_rise_us > 0 && now > s_host_rmt_last_rise_us) {
            uint64_t diff = now - s_host_rmt_last_rise_us;
            /* 兼容 wait_armed 后端的 uint32_t 输出，clamp 到该范围 */
            s_host_rmt_last_pulse_us = (diff > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)diff;
        }
    }
}

wink_status_t pal_gpio_write(wink_pin_t pin, bool level) {
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) {
        return WINK_ERR_INVALID_STATE;
    }
    for (int i = 0; i < HOST_MAX_LOOPBACKS; i++) {
        if (s_host_loopbacks[i].active && s_host_loopbacks[i].pin_out == pin) {
            sim_set_gpio_ideal((uint16_t)s_host_loopbacks[i].pin_in, level);
            /* RMT arm/wait_armed 路径：pin_out 经 loopback 输出到 pin_in，
             * 若 pin_in 即当前 RMT 绑定引脚且已 arm，则通知捕获状态机记录时刻。 */
            if (s_host_rmt_armed && s_host_loopbacks[i].pin_in == s_host_rmt_pin) {
                host_rmt_note_edge(level);
            }
        }
    }
    (void)level;
    return WINK_OK;
}

wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level) {
    if (out_level == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    *out_level = false; /* Defense-in-depth initialization */

    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }

    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) {
        return WINK_ERR_INVALID_STATE;
    }

    /* ADR-0009 Wave1：注入了理想电平的 pin → 走抖动退化（§3.1）；否则走原 echo 协作推进逻辑 */
    bool debounced;
    extern bool host_gpio_read_debounced(uint16_t pin, bool *out_level);
    if (host_gpio_read_debounced(pin, &debounced)) {
        *out_level = debounced;
        return WINK_OK;
    }

    if (pin != host_echo_pin()) {
        *out_level = false;
        return WINK_OK;
    }

    uint64_t t = host_sim_time_us();
    uint64_t rise = host_echo_rise_us();
    uint64_t high = host_echo_high_us();
    /* 向下一个 echo 边沿推进，但单次最多推进 ECHO_POLL_WINDOW_US，
     * 使驱动 polling 循环的 30ms 超时判定可达（远期 rise 不会被瞬间跳过）。 */
    if (t < rise) {
        uint64_t target = rise;
        if (rise - t > ECHO_POLL_WINDOW_US) target = t + ECHO_POLL_WINDOW_US;
        host_sim_advance_to(target);
        *out_level = (target >= rise);
        return WINK_OK;
    }
    if (t < rise + high) {
        host_sim_advance_to(rise + high);
        *out_level = false;
        return WINK_OK;
    }
    *out_level = false;
    return WINK_OK;
}

/* ─────────────────────────────────────────────────────────
 * Host GPIO 中断实现（支持中断锁语义 + pending 队列，用于单元测试）
 * ───────────────────────────────────────────────────────── */
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
            pal_os_set_sim_isr_context(true);
            s_gpio_isr[pin](s_gpio_isr_arg[pin]);
            pal_os_set_sim_isr_context(false);
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
    if (prio < PAL_IRQ_PRIO_LOW || prio > PAL_IRQ_PRIO_HIGH) {
        return WINK_ERR_INVALID_ARG;
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

/* P1-P5-10: host 单线程模型，disable 返回时保证无 ISR 正在执行，
 * synchronize_interrupt 语义等价于 no-op（仅做参数合法性校验）。 */
wink_status_t pal_gpio_synchronize_interrupt(wink_pin_t pin)
{
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
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
        pal_os_set_sim_isr_context(true);
        s_gpio_isr[pin](s_gpio_isr_arg[pin]);
        pal_os_set_sim_isr_context(false);
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
 * 逻辑中断核心接口（Host 单元测试支持）
 * ───────────────────────────────────────────────────────── */
#define HOST_MAX_IRQ  32
static pal_isr_t s_host_irq_table[HOST_MAX_IRQ] = {NULL};
static void *s_host_irq_arg[HOST_MAX_IRQ] = {NULL};
static uint32_t s_host_irq_call_count[HOST_MAX_IRQ] = {0};

wink_status_t pal_irq_enable(uint32_t irq_num, pal_irq_prio_t prio,
                              pal_isr_t handler, void *arg)
{
    /* P1-P5-9: prio 使用命名常量边界（PAL_IRQ_PRIO_LOW..HIGH）而非 magic 0，
     * 与 ESP32 / wasm target 保持一致。 */
    if (irq_num >= HOST_MAX_IRQ || handler == NULL ||
        prio < PAL_IRQ_PRIO_LOW || prio > PAL_IRQ_PRIO_HIGH) {
        return WINK_ERR_INVALID_ARG;
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
    return WINK_OK;
}

void pal_irq_set_pending(uint32_t irq_num)
{
    if (irq_num < HOST_MAX_IRQ && s_host_irq_table[irq_num] != NULL) {
        if (s_irq_lock_depth > 0) {
            /* 持有锁时不立即执行（单测可检查 pending 状态） */
        } else {
            s_host_irq_call_count[irq_num]++;
            pal_os_set_sim_isr_context(true);
            s_host_irq_table[irq_num](s_host_irq_arg[irq_num]);
            pal_os_set_sim_isr_context(false);
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
    /* Track A（M1）：DAL 是资源占用 SSOT，PAL 层不再自 claim PWM 通道。 */
    return WINK_OK;
}
wink_status_t pal_pwm_set_duty(uint8_t channel, float duty) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    host_record_pwm(channel, duty);
    return WINK_OK;
}

void pal_pwm_deinit(uint8_t channel) {
    if (!pal_pwm_router_channel_ready(channel)) { return; }   /* no-op if uninitialized */
    /* Track A（M1）：PAL 层不再持 claim；release 由 DAL layer 拥有（未来 dal_xxx_deinit）。 */
    pal_pwm_router_release(channel);
}

wink_status_t pal_test_enable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in) {
    if (pin_out < 0 || pin_out >= HOST_MAX_GPIO_PIN || pin_in < 0 || pin_in >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
    // Update existing slot or find empty slot
    int first_empty = -1;
    for (int i = 0; i < HOST_MAX_LOOPBACKS; i++) {
        if (s_host_loopbacks[i].active && s_host_loopbacks[i].pin_out == pin_out && s_host_loopbacks[i].pin_in == pin_in) {
            return WINK_OK; // Already exists
        }
        if (!s_host_loopbacks[i].active && first_empty < 0) {
            first_empty = i;
        }
    }
    if (first_empty < 0) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }
    s_host_loopbacks[first_empty].pin_out = pin_out;
    s_host_loopbacks[first_empty].pin_in = pin_in;
    s_host_loopbacks[first_empty].active = true;
    return WINK_OK;
}

wink_status_t pal_test_disable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in) {
    if (pin_out < 0 || pin_out >= HOST_MAX_GPIO_PIN || pin_in < 0 || pin_in >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
    for (int i = 0; i < HOST_MAX_LOOPBACKS; i++) {
        if (s_host_loopbacks[i].active && s_host_loopbacks[i].pin_out == pin_out && s_host_loopbacks[i].pin_in == pin_in) {
            s_host_loopbacks[i].active = false;
            return WINK_OK;
        }
    }
    return WINK_OK;
}

/* P1-P4 (2026-07-04)：pin_map getter。host 是虚拟 target，部分引脚有默认路由以支持自环；
 * 未映射引脚返回 WINK_ERR_UNSUPPORTED 让调用方明确感知语义。参数越界仍返
 * INVALID_ARG。*/
wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin) {
    if (out_pin == NULL) { return WINK_ERR_INVALID_ARG; }
    if (channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    if (s_host_pwm_pins[channel] < 0) {
        return WINK_ERR_UNSUPPORTED;
    }
    *out_pin = s_host_pwm_pins[channel];
    return WINK_OK;
}

wink_status_t pal_i2c_port_pins(uint8_t port, wink_pin_t *out_sda, wink_pin_t *out_scl) {
    if (out_sda == NULL && out_scl == NULL) { return WINK_ERR_INVALID_ARG; }
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    return WINK_ERR_UNSUPPORTED;
}

/* Phase 2：host I2C 事务捕获（供 ssd1306 单测验证 flush 发出正确事务） */
extern void host_record_i2c(uint8_t port, uint16_t addr, uint32_t write_len);

wink_status_t pal_i2c_transfer(uint8_t port, uint16_t addr,
                      const uint8_t *w, uint32_t wl, uint8_t *r, uint32_t rl) {
    (void)w; (void)r; (void)rl;
    host_record_i2c(port, addr, wl);
    return WINK_OK;
}

wink_status_t pal_i2c_scan(uint8_t port, uint8_t *out_found_bitmap, size_t bitmap_bytes) {
    if (out_found_bitmap == NULL || bitmap_bytes < 16) {
        return WINK_ERR_INVALID_ARG;
    }
    if (port >= PAL_I2C_PORTS) {
        return WINK_ERR_INVALID_ARG;
    }
    /* Host simulation has no physical I2C bus; report no devices present
     * rather than UNSUPPORTED, so selftest can report "empty bus" cleanly. */
    memset(out_found_bitmap, 0, 16);
    return WINK_OK;
}

static uint32_t (*s_sim_measure_fn)(uint16_t) = NULL;

void host_register_sim_ultrasonic(void (*trigger_fn)(uint16_t), uint32_t (*measure_fn)(uint16_t)) {
    (void)trigger_fn;
    s_sim_measure_fn = measure_fn;
}

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us) {
    if (pulse_us == NULL) { return WINK_ERR_INVALID_ARG; }
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) { return WINK_ERR_INVALID_ARG; }
    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) { return WINK_ERR_INVALID_STATE; }
    *pulse_us = 0;

    // Check active loopbacks first
    for (int i = 0; i < HOST_MAX_LOOPBACKS; i++) {
        if (s_host_loopbacks[i].active && s_host_loopbacks[i].pin_in == pin) {
            wink_pin_t pin_out = s_host_loopbacks[i].pin_out;
            for (int ch = 0; ch < PAL_PWM_CHANNELS; ch++) {
                if (s_host_pwm_pins[ch] == pin_out) {
                    float duty = sim_last_pwm_duty(ch);
                    uint32_t p = (uint32_t)((duty / 100.0f) * (1000000.0f / 50.0f));
                    if (p >= timeout_us) {
                        return WINK_ERR_TIMEOUT;
                    }
                    *pulse_us = p;
                    (void)level;
                    return WINK_OK;
                }
            }
        }
    }

    if (s_sim_measure_fn) {
        uint32_t p = s_sim_measure_fn((uint16_t)pin);
        if (p > 0) {
            if (p >= timeout_us) {
                return WINK_ERR_TIMEOUT;
            }
            *pulse_us = p;
            (void)level;
            return WINK_OK;
        }
    }

    if (pin != (wink_pin_t)host_echo_pin()) { return WINK_ERR_UNSUPPORTED; }   /* 无 pin 映射 */
    uint64_t rise = host_echo_rise_us();
    if (rise > timeout_us) { return WINK_ERR_TIMEOUT; }            /* echo 起始晚于超时 */
    *pulse_us = (uint32_t)host_echo_high_us();
    (void)level;   /* host echo 即高电平脉宽 */
    return WINK_OK;
}


wink_status_t pal_rmt_pulse_capture_init(wink_pin_t pin, pal_rmt_edge_t start_edge) {
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) { return WINK_ERR_INVALID_ARG; }
    s_host_rmt_pin = pin;
    s_host_rmt_armed = false;
    s_host_rmt_last_rise_us = 0;
    s_host_rmt_last_pulse_us = 0;
    (void)start_edge;
    return WINK_OK;
}

wink_status_t pal_rmt_pulse_capture_arm(void) {
    if (s_host_rmt_pin < 0) { return WINK_ERR_INVALID_ARG; }
    /* 清空上一轮采样，进入监听态。任何后续 pal_gpio_write(s_host_rmt_pin, level)
     * 若同时有 loopback(pin,pin) 生效，会经 host_rmt_note_edge 记录时刻。 */
    s_host_rmt_last_rise_us = 0;
    s_host_rmt_last_pulse_us = 0;
    s_host_rmt_armed = true;
    return WINK_OK;
}

wink_status_t pal_rmt_pulse_capture_wait_armed(uint32_t timeout_us, uint32_t *pulse_us_out) {
    if (pulse_us_out == NULL) { return WINK_ERR_INVALID_ARG; }
    if (s_host_rmt_pin < 0) { return WINK_ERR_INVALID_STATE; }
    *pulse_us_out = 0;

    /* arm→drive→wait 路径：软件驱动的自环脉冲已在 arm 与本调用之间通过
     * pal_gpio_write 记录到 s_host_rmt_last_pulse_us。返回并清理 armed 态。 */
    if (s_host_rmt_armed && s_host_rmt_last_pulse_us > 0) {
        uint32_t p = s_host_rmt_last_pulse_us;
        s_host_rmt_armed = false;
        s_host_rmt_last_pulse_us = 0;
        s_host_rmt_last_rise_us = 0;
        if (p >= timeout_us) {
            return WINK_ERR_TIMEOUT;
        }
        *pulse_us_out = p;
        return WINK_OK;
    }

    /* 未记到 pulse：走原有 pulse_in 兜底（HC-SR04 sim / echo pin 逻辑）。
     * 若都无匹配，返 TIMEOUT 保持与 ESP32 语义一致。 */
    s_host_rmt_armed = false;
    wink_status_t st = pal_gpio_pulse_in(s_host_rmt_pin, true, timeout_us, pulse_us_out);
    if (st == WINK_ERR_UNSUPPORTED) {
        /* 无 pin 映射时按 timeout 处理，与 ESP32 wait_armed 超时语义一致 */
        return WINK_ERR_TIMEOUT;
    }
    return st;
}

wink_status_t pal_rmt_pulse_capture_wait(uint32_t timeout_us, uint32_t *pulse_us_out) {
    if (pulse_us_out == NULL) { return WINK_ERR_INVALID_ARG; }
    if (s_host_rmt_pin < 0) { return WINK_ERR_INVALID_STATE; }
    *pulse_us_out = 0;
    wink_status_t s = pal_rmt_pulse_capture_arm();
    if (wink_status_is_error(s)) { return s; }
    return pal_rmt_pulse_capture_wait_armed(timeout_us, pulse_us_out);
}

void pal_rmt_pulse_capture_deinit(void) {
    s_host_rmt_pin = -1;
    s_host_rmt_armed = false;
    s_host_rmt_last_rise_us = 0;
    s_host_rmt_last_pulse_us = 0;
}

bool pal_rmt_pulse_capture_is_active(void) {
    return s_host_rmt_pin >= 0;
}

