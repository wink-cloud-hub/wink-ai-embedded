/**
 * @file wasm_bridge.h
 * @brief Wasm-JS 桥接契约 SSOT。
 *
 * 所有 wasm 仿真侧对 JS 的导入（js_pal_* / js_sim_*）extern 声明集中在此，
 * 杜绝散落在 pal_hal_wasm.c / pal_osal_wasm.c / dal_*.c 多处的漂移
 * （03-directory-architecture.md §9 迁移项3 / ADR-0003 SSOT 闭环）。
 *
 * 约定：js_sim_*（DAL bypass）契约以 Device Registry 为 SSOT，本头抄 Registry。
 *       Plan 4 会在此追加 js_sim_trigger_ultrasonic / js_sim_measure_echo_pulse_us。
 */
#ifndef WASM_BRIDGE_H
#define WASM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- PAL HAL 侧 JS 导入（来自旧 pal_hal_wasm.c）---- */
extern void js_pal_gpio_write(uint16_t pin, bool level);
extern bool js_pal_gpio_read(uint16_t pin);
extern void js_pal_pwm_set_duty(uint8_t channel, float duty_cycle_percent);
extern bool js_pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                                const uint8_t *write_buf, uint32_t write_len,
                                uint8_t *read_buf, uint32_t read_len);
/* ---- 中断桥 Poll 接口（方案 C：Wasm 主动拉取，取代旧 Push 模型 _trigger_wasm_interrupt）----
 *
 * 架构变更（ADR-0002 方案 C 落地，见 docs/04 01-wasm-sandbox-lifecycle.md §4）：
 *   旧 Push 模型：JS 随时调用 Wasm 导出 _trigger_wasm_interrupt
 *                → Asyncify sleeping 窗口重入崩溃（D1，确定性崩溃路径）。
 *   新 Poll 模型：JS 侧只写 pending 队列；Wasm tick 边界主动调用 js_pal_poll_interrupt 拉取。
 *                → 重入面彻底消除，无需 Asyncify state 守卫。
 *
 * js_pal_register_interrupt：C 注册 ISR 时告知 JS 侧「pin → (callback_index, arg_ptr) 映射」。
 *   arg_ptr 为 Wasm 线性内存偏移（uint32_t，wasm32 安全；wasm64 迁移见 Phase 6 Task 6-3）。
 *   JS 侧在 GPIO 事件到来时只写 pending 队列，不回调 Wasm。
 * js_pal_deregister_interrupt：注销 pin 的映射。
 * js_pal_poll_interrupt：每个 tick 边界由 C 主动调用，从 JS 侧 FIFO 队列拉取一个 pending 中断。
 *   out_callback_index / out_arg_ptr：有 pending 中断时写出，返回 true；队列空返回 false。
 *   多个 pending 须多次调用直到返回 false（JS 侧维护 FIFO，容量见 pal_wasm_internal.h）。
 *
 * 索引安全约束（Phase 6 Task 6-3 / P2-4，与旧模型相同）：
 *   - callback_index 是不透明 Wasm Table 索引，禁在此边界外做裸 cast。
 *   - JS 侧须校验 index 在已注册范围内（长期用 Emscripten addFunction 替代裸 cast）。
 *   - wasm64 下裸 (uint32_t)(uintptr_t) cast 对 >2^32 索引截断，须迁移至 addFunction。
 */
extern void js_pal_register_interrupt(uint16_t pin, uint32_t callback_index, uint32_t arg_ptr);
extern void js_pal_deregister_interrupt(uint16_t pin);
extern bool js_pal_poll_interrupt(uint32_t *out_callback_index, uint32_t *out_arg_ptr);


/* ---- PAL OSAL 侧 JS 导入 ---- */
extern void js_pal_delay_ms(uint32_t ms);
extern void js_pal_delay_us(uint32_t us);
extern uint64_t js_pal_get_ms(void);
extern uint64_t js_pal_get_us(void);

/* ---- DAL bypass 侧 JS 导入（js_sim_*）—— 签名抄 Device Registry (01-device-model-registry.md) ----
 * 仅在 #ifdef SIMULATION 下被 DAL 引用；真机分支不编译本段。
 * ADR-0003 决策2：只旁路最底层物理量来源（trigger 时序 + echo 脉宽），换算/超时两端同源。 */
extern void     js_sim_trigger_ultrasonic(uint16_t trig_pin);
extern uint32_t js_sim_measure_echo_pulse_us(uint16_t trig_pin);

/* ---- WASM 退化引擎导出（ADR-0009 Wave 2，C → JS Worker）----
 *
 * 这些符号由 pal_wasm_physical.c / pal_osal_wasm.c 定义并以
 * EMSCRIPTEN_KEEPALIVE 标注，链接器会把它们暴露到 Module exports，
 * JS Worker 通过 cwrap/ccall 调用。
 *
 * 类型契约（CMake `-s WASM_BIGINT=1`）：
 *   - uint64_t   ↔ JS bigint（pal_wasm_advance_virtual_clock 的 us 参数）
 *   - uint32_t / uint16_t ↔ JS number（≤53 位安全）
 *   - float      ↔ JS number（IEEE754 双精度兼容）
 *
 * 这里仅做声明用于跨翻译单元一致性；真实可见性来自 KEEPALIVE。 */
extern void     pal_wasm_advance_virtual_clock(uint64_t us);
extern void     pal_wasm_set_bounce_us(uint32_t us);
extern void     pal_wasm_set_warmup_us(uint32_t us);
extern void     pal_wasm_set_sample_interval_us(uint32_t us);
extern void     pal_wasm_set_adc_noise_v(float v);
extern void     pal_wasm_set_rc_tau_s(float s);
extern void     pal_wasm_set_i2c_drop_permil(uint16_t permil);
extern void     pal_wasm_set_prng_seed(uint32_t seed);
extern uint32_t pal_wasm_get_prng_state(void);
extern void     pal_wasm_reset_physical(void);

#ifdef __cplusplus
}
#endif

#endif /* WASM_BRIDGE_H */
