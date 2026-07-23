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
 * 在 Asyncify sleeping 状态下（wasm 已 unwind、等待一个 Promise-returning
 * js_* import resolve）：
 *   - Host 不得调用任何 pal_wasm_* 导出；此时线性内存/栈状态部分位于 Asyncify
 *     备份缓冲区，重入 C 会读到不一致状态，并可能导致备份栈损坏。
 *   - Host 不得直接读写 Wasm 堆内存（HEAPU8 等视图可能因 Asyncify 临时移动而
 *     指向陈旧内容；恢复 rewind 后内容才一致）。
 *   - 允许调用纯 JS 侧逻辑（VirtualClock 推进、PinArbiter.setDriver、InterruptQueue.push
 *     等 framework-owned 组件）；这些只修改 JS 侧 state，等下一次 wasm 进入时
 *     由 Phase 0 / js_pal_poll_interrupt / js_pal_gpio_read 等 pull 路径兑现。
 *
 * P0-3 / P1-4 收窄方案（Phase C，2026-07-04）：
 *   - 不实现 isWasmYielded 状态机追踪（避免与 Asyncify 内部状态耦合）。
 *   - 由两层防线兜底：
 *     (a) safeWrap / safeWrapAsync HOF 对所有用户-overridable js_* import 做
 *         try/catch + Promise.catch，宿主抛错/reject 永远返回 resolved Promise
 *         → Emscripten 永远不会看到 throw/reject，不会 abort；错误被 marshal 到
 *         pal_wasm_host_fault(8003, msg) 走标准 fault 路径。
 *     (b) pal_wasm_host_fault 置位 s_wasm_faulted 锁存后，所有 state-mutating
 *         pal_wasm_* 导出（set_bounce_us / advance_virtual_clock / set_prng_seed
 *         等）通过 WASM_FAULT_GUARD_VOID() 宏 fast-fail 为 no-op，避免 fault 后
 *         宿主继续驱动 state 变更。pal_wasm_is_faulted() 仍可读，供宿主轮询。
 *   - pal_wasm_reset_physical() 是唯一允许在 faulted 态调用的 mutating 导出——
 *     它是测试/复位入口，内部会调用 pal_wasm_clear_fault_latch() 重置锁存。
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


/* ---- PAL OSAL 侧 JS 导入 ----
 *
 * 时间 SSOT：C 侧 pal_os_get_us/ms() 直接读 s_virtual_us 内存（零 JS 调用），
 * 虚拟时钟的唯一推进入口是 pal_wasm_advance_virtual_clock(bigint)（C→JS 导出，
 * 见下方 §WASM 退化引擎导出），不再有 JS→C 反向 get_ms/get_us 导入。
 * （P2-1：js_pal_os_get_ms/us 已删除，之前是死桩，从未被 wasm 实际调用。） */
extern void js_pal_os_sleep_ms(uint32_t ms);
extern void js_pal_os_busy_wait_us(uint32_t us);

/* ---- 分级日志桥接（P1-L1，2026-07-04）----
 * level: pal_log_level_t 数值 (ERROR=1, WARN=2, INFO=3, DEBUG=4)。
 * msg:   已在 C 侧格式化好的 NUL-terminated UTF-8 字符串（wasm 线性内存偏移）。
 * JS 默认实现分派到 console.error/warn/log/debug；宿主可覆盖转发到 UI 日志面板。
 * 契约：msg 指向的内存在 js_pal_log 返回前一直有效（同步调用，JS 侧不得持有指针）。 */
extern void js_pal_log(uint8_t level, const char *msg);

/* ---- DAL bypass 侧 JS 导入（js_sim_*）—— 签名抄 Device Registry (01-device-model-registry.md) ----
 * 仅在 #ifdef SIMULATION 下被 DAL 引用；真机分支不编译本段。
 * ADR-0003 决策2：只旁路最底层物理量来源（trigger 时序 + echo 脉宽），换算/超时两端同源。 */
extern void     js_pal_gpio_on_write(uint8_t pin, uint8_t level);
/* ---- Phase3 Plugin Channel API (2026-07-21) ----
 * 标准插件通道读取接口：从 JS 侧插件实例读取状态值。
 * instance_id: 插件实例 ID（如 "ultrasonic:0"）
 * channel_name: 通道名（如 "distanceCm"）
 * 返回: 浮点数值（插件支持的所有通道类型均以 float 形式返回）
 */
extern float    js_sim_get_plugin_channel(const char *instance_id, const char *channel_name);

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
/* [双仓联动] Phase 3 SessionRecorder diagnostic replay restore */
extern void     pal_wasm_set_prng_state(uint32_t state);
/* [双仓联动] Phase 3 L1 ABI layout lock — bump when SimFaults / snapshot ABI changes */
extern uint32_t pal_wasm_get_abi_hash(void);
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

/* ---- Host→C fault 注入（P0-3 Phase C）----
 *
 * JS 侧宿主 plugin（用户自定义 js_* override）抛同步异常或返回 rejected Promise
 * 时，由 createUnisimImports 的 safeWrap/safeWrapAsync HOF 统一捕获后调用
 * pal_wasm_host_fault(code=8003, msg_cstr) 走标准 fault 路径：
 *   1. 置位 s_wasm_faulted 锁存（pal_wasm_is_faulted() 返回 true）；
 *   2. wink_trace_fault(code) 写入审计环；
 *   3. wink_actuator_safe_off_all() 安全关断所有执行器；
 *   4. 若调度器已启动（s_app_callbacks != NULL），调 on_fault(code) 回调。
 *
 * ABI 契约：
 *   - msg_cstr 是 wasm 线性内存内 NUL-terminated UTF-8 字符串指针（JS 侧
 *     通过 _malloc + stringToUTF8 写入，调用后 _free）。允许 NULL（不传递消息）。
 *   - code 建议使用 8003 表示 JS host plugin fault；与 WCET 8002、boot-reset 8001
 *     同属 8xxx 宿主/仿真专用 code 段。
 *   - 幂等：首次 fault 走完整 safe-off 路径；后续调用仅 trace，不重复 safe-off。
 *   - 调用时机：可在任意 wasm→JS import 内部调用（包括 Asyncify sleeping 窗口），
 *     但 on_fault 回调可能延迟到下一个调度 tick。 */
extern bool   pal_wasm_is_faulted(void);
extern void   pal_wasm_host_fault(uint32_t code, const char* msg_cstr);

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

/* ---- 虚拟外设控制与状态导出 API（Scheme A + Value-injection，2026-07-05）---- */
extern void           pal_wasm_sim_reset_all_devices(void);
extern const uint8_t* pal_wasm_get_ssd1306_fb(uint32_t *width, uint32_t *height);
extern float          pal_wasm_get_servo_angle(uint8_t channel);
extern float          pal_wasm_get_pwm_duty_percent(uint8_t channel);
extern void           pal_wasm_push_pin_event(uint8_t pin, uint64_t delay_us, uint8_t level);
extern void           pal_wasm_set_ultrasonic_distance(uint8_t pin, float distance_cm);
extern void           pal_wasm_set_gpio_input(uint8_t pin, bool level);
extern bool           pal_wasm_get_gpio_output(uint8_t pin);

/* ---- 模式控制导出 API（ADR-0042） ---- */
extern void           pal_wasm_set_sim_mode(uint32_t mode);
extern uint32_t       pal_wasm_get_sim_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* WASM_BRIDGE_H */
