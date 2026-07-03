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

// ============================================================================
//                          EMSCRIPTEN ABI 契约附录
//
// 以下是 C ↔ JS 跨语言调用的所有隐性前提假设。
// 修改任何桥接代码时，必须确保不违反这些契约。
// 违反任何一条都会导致难以调试的运行时崩溃或静默数据损坏。
// ============================================================================

/*
 * ABI 契约 #1: WASM 栈增长方向
 * -------------------------------
 * Emscripten WASM 栈是**向下增长**的（从高地址向低地址）。
 * Asyncify 展开/重绕时依赖此行为。
 * 验证：_Static_assert 检查不可行，因为栈方向是运行时属性。
 * 风险：栈溢出时会静默覆盖堆内存，无边界检查。
 * 防护：编译时指定 -s ASYNCIFY_STACK_SIZE=65536，留足余量。
 */

/*
 * ABI 契约 #2: 浮点数与 NaN 装箱
 * -----------------------------
 * - C float/double ↔ JS number: 符合 IEEE 754，安全互转
 * - C long double: 不要在桥接接口使用！Emscripten 将其降级为 double
 * - JS NaN/Infinity: 传到 C 侧是合法的 IEEE 值，但 C 侧逻辑可能没处理
 * 防护：所有桥接函数禁止使用 long double；JS 侧传入前做 isFinite 检查
 */

/*
 * ABI 契约 #3: 指针对齐要求
 * -------------------------
 * Emscripten malloc 保证 8 字节对齐。
 * - uint64_t/double 访问需要 8 字节对齐
 * - 未对齐访问在 WASM 中是**未定义行为**（实际可能静默读错值）
 * 防护：所有跨边界传递的结构体使用 __attribute__((aligned(8)))
 *       或使用 packed 结构体配合 memcpy 访问
 */

/*
 * ABI 契约 #4: EM_JS 宏展开时机
 * -----------------------------
 * EM_JS 定义的 JS 代码在**编译期**嵌入 WASM 二进制。
 * - 运行时无法动态修改
 * - 无法访问 JS 侧闭包变量，只能访问全局作用域
 * - 参数传递有开销，避免在热路径调用
 */

/*
 * ABI 契约 #5: WASM_BIGINT ABI
 * ---------------------------
 * 启用 -s WASM_BIGINT=1 后：
 * - C uint64_t/int64_t ↔ JS bigint: 精确传递，无精度损失
 * - 但如果 TS 侧误用 number 传入，Emscripten 会抛出 TypeError
 * 防护：TS 侧所有时钟/时间相关字段强制为 bigint 类型
 *       SimWorker 消息反序列化后做 runtime typeof 校验
 */

/*
 * ABI 契约 #6: Asyncify 重入限制
 * ------------------------------
 * 在 Asyncify sleeping 状态下：
 * - 不能调用任何 WASM 导出函数
 * - 不能访问 WASM 堆内存（堆内容在重绕前是不一致的）
 * - 只能调用纯 JS 侧逻辑
 * 防护：所有 JS → WASM 调用必须在 WASM 处于 running 状态
 *       使用状态机跟踪 WASM 执行状态
 */

// ============================================================================
// 契约结束，以下为正式符号声明
// ============================================================================

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
extern void js_pal_os_sleep_ms(uint32_t ms);
extern void js_pal_os_busy_wait_us(uint32_t us);
extern uint64_t js_pal_os_get_ms(void);
extern uint64_t js_pal_os_get_us(void);

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

/* ---- 虚拟时钟溢出预警导出（Wave2 P1 Task 6）----
 *
 * JS Worker 每个 tick 边界轮询 pal_wasm_is_clock_warning_fired()，首次
 * 返回 true 时 console.warn 输出剩余量程提示。语义详见
 * pal_wasm_internal.h 注释。 */
extern bool     pal_wasm_is_clock_warning_fired(void);
extern uint64_t pal_wasm_get_virtual_clock_us(void);

/* ---- OSAL 时钟读取（EMSCRIPTEN_KEEPALIVE 导出供 JS 直接读取虚拟时钟）---- */
extern uint64_t pal_os_get_us(void);
extern uint64_t pal_os_get_ms(void);

/* ---- JS 简化 HAL 读取导出（bool 返回避免 out-pointer 编组）----
 *
 * 内部 PAL API pal_gpio_read / pal_i2c_transfer 返回 wink_status_t 并用 out-pointer。
 * 以下 EMSCRIPTEN_KEEPALIVE 包装提供 JS 友好签名（直接 bool 返回），
 * 供 WasmPhysicalBridge.readGpioDegraded / i2cTransfer 使用。 */
extern bool     pal_wasm_gpio_read(uint16_t pin);
extern bool     pal_wasm_i2c_transfer(uint8_t port, uint16_t dev_addr,
                                      const uint8_t *write_buf, uint32_t write_len,
                                      uint8_t *read_buf, uint32_t read_len);

/* ---- 故障审计日志导出（Wave2 Task 8）----
 *
 * pal_wasm_physical.c 维护一个 256 条环形缓冲区，记录所有物理退化事件
 * （GPIO 抖动、I2C 丢包等）。CI 测试失败后由 JS Worker 通过 cwrap 这些
 * 导出符号读回日志，重建 "哪个故障在哪个时间点触发" 的因果链。
 *
 * 字段级访问器避免 struct 跨语言传递的 alignment/padding 风险：JS 先调
 * pal_wasm_get_fault_log_count() 取总数，再对每个 index ∈ [0, count) 逐
 * 字段读出（timestamp 用 BigInt，其余 number）。
 *
 * 越界 index 时所有字段 getter 返回 0；调用方必须先用 count 做边界判断。
 * 详细语义见 pal_wasm_internal.h。 */
extern uint32_t pal_wasm_get_fault_log_count(void);
extern void     pal_wasm_reset_fault_log(void);
extern uint64_t pal_wasm_fault_event_get_timestamp(uint32_t index);
extern uint8_t  pal_wasm_fault_event_get_type(uint32_t index);
extern uint16_t pal_wasm_fault_event_get_pin_or_bus(uint32_t index);
extern uint32_t pal_wasm_fault_event_get_sequence(uint32_t index);

/* ---- 功耗模型接口（Wave3 stub；ADR-0009 Wave 2 Task 9）----
 *
 * 当前为预埋占位：set_pin_power_model 校验参数后返回 WINK_OK 但不存储；
 * get_total_energy_mj 始终返回 0。语义详见 pal_wasm_internal.h。
 *
 * 类型契约：
 *   - wink_status_t 是 int 枚举 → JS number。
 *   - uint64_t 总能耗（mJ） ↔ JS bigint（WASM_BIGINT=1）。
 *   - wasm_pin_power_model_t 是仅含 3 个 uint32 的 POD struct，跨语言传递
 *     由 JS 在 wasm 堆 malloc 后逐字段写入再传指针偏移（标准 wasm/JS
 *     struct passing 模式）。这里前向声明即可，JS 永远不直接看到完整定义。
 *
 * 加在此处而非 #include pal_wasm_internal.h 是为了保持 wasm_bridge.h 作为
 * "JS 看到的所有 C 符号 SSOT" 的边界：内部头只被 wasm target *.c 引用，
 * 桥头独立地把对外契约暴露给非 wasm-internal 的代码（测试、文档生成）。
 *
 * 声明形式：仅用纯 struct 前向声明（不带 typedef）。C99 下同名 typedef
 * 出现两次是硬错（本头 + pal_wasm_internal.h 的完整定义会冲突），C11 才允许；
 * 前向 struct 声明多次共存则从 C89 起就合法，跨标准更稳。 */
struct wasm_pin_power_model_t;   /* 完整定义见 pal_wasm_internal.h */
#include "wink_status.h"   /* wink_status_t */
extern wink_status_t pal_wasm_set_pin_power_model(uint8_t pin,
                                                  const struct wasm_pin_power_model_t *model);
extern uint64_t      pal_wasm_get_total_energy_mj(void);

#ifdef __cplusplus
}
#endif

#endif /* WASM_BRIDGE_H */
