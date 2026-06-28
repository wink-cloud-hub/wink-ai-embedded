/**
 * @file host_test_ctrl.h
 * @brief host 测试专用注入控制 API（非 PAL 契约，仅测试用）。
 *        驱动 targets/host 的虚拟时间/echo/pwm 行为，供 DAL/runtime 端到端测。
 */
#ifndef HOST_TEST_CTRL_H
#define HOST_TEST_CTRL_H

#include <stdint.h>
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

#endif /* HOST_TEST_CTRL_H */
