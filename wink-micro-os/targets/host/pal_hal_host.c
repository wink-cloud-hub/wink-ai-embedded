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
#include "pal_resource.h"
#include "pal_pwm_router.h"
#include "host_test_ctrl.h"

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

wink_status_t pal_gpio_init(uint16_t pin, pal_gpio_mode_t mode) {
    /* Phase 2 Task 2-3：host 资源占用治理。owner 为 PAL 层固定标识
     * （同 owner 重复 claim 幂等；不同 owner 冲突 → BUSY 由调用方透传）。 */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, pin, "pal_hal_host");
    if (wink_status_is_error(rs)) { return rs; }
    (void)mode;
    return WINK_OK;
}
void pal_gpio_write(uint16_t pin, bool level) { (void)pin; (void)level; }

bool pal_gpio_read(uint16_t pin) {
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

wink_status_t pal_gpio_enable_interrupt(uint16_t pin, pal_gpio_intr_t t, pal_gpio_isr_t cb, void *a) {
    (void)pin; (void)t; (void)cb; (void)a; return WINK_OK;
}
wink_status_t pal_gpio_disable_interrupt(uint16_t pin) { (void)pin; return WINK_OK; }

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

wink_status_t pal_gpio_pulse_in(uint16_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us) {
    /* Phase 4 Task 4-2：host 直接读 echo 脉宽（虚拟时间下同步），不经 pal_gpio_read 协作推进。
     * 这是非阻塞 DAL 的 echo 时序 SSOT（Phase 4 Task 4-6 决策：保留协作推进仅供过渡 blocking read）。 */
    if (pulse_us == NULL) { return WINK_ERR_INVALID_ARG; }
    if (pin != host_echo_pin()) { return WINK_ERR_UNSUPPORTED; }   /* 无 pin 映射（直至 virtual registry 接入） */
    uint64_t rise = host_echo_rise_us();
    if (rise > timeout_us) { return WINK_ERR_TIMEOUT; }            /* echo 起始晚于超时 */
    *pulse_us = (uint32_t)host_echo_high_us();
    (void)level;   /* host echo 即高电平脉宽 */
    return WINK_OK;
}
