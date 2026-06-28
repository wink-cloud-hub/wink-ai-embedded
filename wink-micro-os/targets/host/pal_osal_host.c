/**
 * @file pal_osal_host.c
 * @brief host 一等 target 的 PAL OSAL 实现 + 虚拟时间状态机 + host_test_ctrl 实现。
 *        虚拟时间状态在此维护（HAL 经 extern 消费）。
 */
#include "pal_osal.h"
#include "host_test_ctrl.h"
#include "wink_sim_physical.h"   /* wink_phys_debounce_ctx_t + WINK_SIM_FAULTS_IDEAL */
#include <stdlib.h>
#include <string.h>

static uint64_t s_time_us = 0;
static uint64_t s_echo_rise_us = 0;
static uint64_t s_echo_high_us = 0;
static uint16_t s_echo_pin = 0xFFFF;
static float s_pwm_duty[8];
static pal_reset_reason_t s_reset_reason = PAL_RESET_REASON_POWER_ON;   /* Phase 5：可配置复位原因（测试注入） */
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
    s_reset_reason = PAL_RESET_REASON_POWER_ON;
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
void sim_set_reset_reason(pal_reset_reason_t reason) { s_reset_reason = reason; }

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
            return true;
        }
    }
    return false;
}

/* ---- PAL OSAL ---- */
void pal_delay_ms(uint32_t ms) { s_time_us += (uint64_t)ms * 1000u; }
void pal_delay_us(uint32_t us) { s_time_us += us; }
uint64_t pal_get_ms(void) { return s_time_us / 1000u; }
uint64_t pal_get_us(void) { return s_time_us; }

pal_mutex_t pal_mutex_create(void) { return (pal_mutex_t)1; }
wink_status_t pal_mutex_lock(pal_mutex_t m, uint32_t to) {
    if (m == NULL) return WINK_ERR_INVALID_ARG;
    (void)to;
    return WINK_OK;
}
wink_status_t pal_mutex_unlock(pal_mutex_t m) {
    if (m == NULL) return WINK_ERR_INVALID_ARG;
    return WINK_OK;
}
void pal_mutex_destroy(pal_mutex_t m) { (void)m; }

/* ---- Phase 5 Task 5-4: WDT / reset reason（host：WDT 为无操作 stub；reset reason 可配置供测试） ---- */
pal_reset_reason_t pal_get_reset_reason(void) { return s_reset_reason; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_watchdog_init(uint32_t timeout_ms) { (void)timeout_ms; return WINK_OK; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_watchdog_feed(void) { return WINK_OK; }

/* ADR-0010：连续异常复位计数（host 可注入静态，供单测模拟 N 次异常复位）*/
uint32_t pal_get_abnormal_boot_count(void) { return s_abnormal_boot_count; }
void pal_set_abnormal_boot_count(uint32_t count) { s_abnormal_boot_count = count; }

uint32_t pal_critical_enter(void) {
    return 0;
}

void pal_critical_exit(uint32_t key) {
    (void)key;
}

/* ─────────────────────────────────────────────────────────
 * Task 创建（host target 降级实现，同步调用）
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
    /* Host target: single-threaded, synchronous execution for tests */
    (void)name; (void)stack_depth; (void)priority;
    (void)core_id; (void)task_handle;

    func(arg);
    return WINK_OK;
}

/* ─────────────────────────────────────────────────────────
 * 环形缓冲区 (host 纯内存实现，单线程无并发)
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
