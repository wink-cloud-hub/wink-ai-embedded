/**
 * @file host_test_ctrl.h
 * @brief host 测试专用注入控制 API（非 PAL 契约，仅测试用）。
 *        驱动 targets/host 的虚拟时间/echo/pwm 行为，供 DAL/runtime 端到端测。
 */
#ifndef HOST_TEST_CTRL_H
#define HOST_TEST_CTRL_H

#include <stdint.h>
#include <stdbool.h>
#include "pal_irq.h"    /* pal_irq_prio_t, pal_isr_t（中断测试注入） */
#include "pal_hal.h"    /* pal_gpio_intr_t, wink_pin_t */
#include "pal_osal.h"   /* pal_reset_reason_t（sim_set_reset_reason 测试注入，Phase 5 Task 5-4） */
#include "wink_sim_physical.h"   /* wink_sim_faults_t（ADR-0009 Wave1 物理退化注入） */

void sim_reset_time(void);
void sim_set_echo_pin(uint16_t pin);
void sim_set_echo_timing(uint64_t rise_us, uint64_t high_duration_us);
float sim_last_pwm_duty(uint8_t channel);
void sim_set_reset_reason(pal_reset_reason_t reason);   /* Phase 5：注入复位原因供 boot safe-lock 测试 */

/* Phase 2：host I2C 事务捕获注入/读取 */
uint8_t  sim_last_i2c_port(void);
uint16_t sim_last_i2c_addr(void);
uint32_t sim_last_i2c_write_len(void);
uint32_t sim_i2c_transfer_count(void);

/* ADR-0009 Wave1：host GPIO 理想电平注入 + 故障配置（仅测试用）。
 * sim_set_gpio_ideal 双语义（§2.3 红线 6）：首次注册=上电态(不抖)；更新电平=跃变(触发抖动)。
 * 注入 pin 须 ≠ echo pin（§2.3 红线 7）。 */
#define SIM_GPIO_IDEAL_SLOTS 4
void sim_set_gpio_ideal(uint16_t pin, bool level);   /* 注册(上电态)/更新(跃变) pin 理想电平 */
void sim_clear_gpio_ideal(void);                      /* 清空所有注入（sim_reset_time 也会调） */
void sim_set_faults(const wink_sim_faults_t *faults); /* 设全局故障配置（退化强度） */

/* ─────────────────────────────────────────────────────────
 * Phase 1：统一中断子系统测试注入 API
 * ───────────────────────────────────────────────────────── */

/**
 * @brief 手动触发 GPIO 中断（仅 Host 平台，单元测试注入）
 * @param pin GPIO 引脚号
 *
 * ⚠️ 中断锁语义：持有中断锁期间触发的中断会被 pending，直到锁释放才执行
 */
void pal_host_trigger_gpio_interrupt(wink_pin_t pin);

/**
 * @brief 获取 ISR 被调用的次数（用于单测断言）
 * @param pin GPIO 引脚号
 * @return ISR 调用计数
 */
uint32_t pal_host_get_isr_call_count(wink_pin_t pin);

/**
 * @brief 重置 ISR 统计和 pending 队列
 */
void pal_host_reset_isr_stats(void);

/**
 * @brief 获取当前 pending 中断数量（用于单测断言中断锁语义）
 * @return pending 队列中的中断数量
 */
uint32_t pal_host_get_pending_count(void);

/**
 * @brief 获取中断锁嵌套深度（用于单测检测锁泄漏）
 * @return 当前中断锁嵌套深度
 */
int pal_host_get_irq_lock_depth(void);

/**
 * @brief 手动触发逻辑中断（仅 Host 平台，单元测试注入）
 * @param irq_num 逻辑中断号
 */
void pal_host_trigger_logical_interrupt(uint32_t irq_num);

/**
 * @brief 获取逻辑中断 ISR 调用次数
 * @param irq_num 逻辑中断号
 * @return ISR 调用计数
 */
uint32_t pal_host_get_logical_isr_call_count(uint32_t irq_num);

#endif /* HOST_TEST_CTRL_H */
