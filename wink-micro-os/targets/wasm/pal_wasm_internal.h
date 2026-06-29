/**
 * @file pal_wasm_internal.h
 * @brief Wasm target 内部接口（仅 wasm target 文件及 wink_runtime.c 的 SIMULATION 分支 include，
 *        不进入公共 PAL 接口，不由 host/esp32 target 编译）。
 *
 * 中断队列容量配置（方案 C：Wasm 主动 Poll 模型）：
 *   PAL_WASM_INTERRUPT_QUEUE_SIZE 控制 JS pending 中断队列的上限（JS 侧 MAX_PENDING 须与之一致）。
 *   超出时新中断被丢弃并在 JS 侧告警（不静默丢失）。
 *
 *   典型场景估算：超声波测距每次最多 2 个边沿事件（上升 + 下降），按 10ms tick
 *   极少同时积累超过 4 个。AI 生成业务逻辑可能产生更密集中断，默认 16 提供 4× 余量。
 *
 *   可在 CMake 或编译命令中覆盖，例如：
 *     target_compile_definitions(wink_simulator PRIVATE PAL_WASM_INTERRUPT_QUEUE_SIZE=32)
 */
#ifndef PAL_WASM_INTERNAL_H
#define PAL_WASM_INTERNAL_H

#include <stdint.h>
#include "wink_sim_physical.h"

#ifndef PAL_WASM_INTERRUPT_QUEUE_SIZE
#define PAL_WASM_INTERRUPT_QUEUE_SIZE 16
#endif

/**
 * @brief 分发所有 JS pending 中断（tick 边界调用，由 wink_runtime.c 在 SIMULATION 宏下驱动）。
 *
 * @note 调用约束（方案 C 核心安全保证）：
 *   - 必须在 Wasm 处于正常运行态调用（非 Asyncify sleeping 窗口）。
 *   - 必须在 wink_app_delay_ms()（Asyncify 挂起点）之前调用——见 wink_runtime.c tick 循环。
 *   - 内部循环 drain 所有 pending 中断（FIFO），直到 js_pal_poll_interrupt 返回 false。
 *   - 非 ISR 上下文，与 ESP32 Bottom-Half 消费 FreeRTOS Queue 语义对称（ADR-0002）。
 *
 * 非 SIMULATION target（host/esp32）不编译此函数，零运行时代价。
 */
void pal_wasm_dispatch_pending_interrupts(void);

/**
 * @brief 虚拟时钟步进接口（导出给 JS Worker）。
 *
 * SSOT 架构唯一入口：wasm 侧不主动步进时钟，时钟完全由 JS Worker 驱动。
 * 参见 pal_osal_wasm.c 注释。
 *
 * @param us 步进微秒数
 */
void pal_wasm_advance_virtual_clock(uint64_t us);

/* ─────────────────────────────────────────────────────────
 * 物理退化引擎内部 API（ADR-0009 Wave 2）。pal_hal_wasm.c 的 GPIO/I2C
 * 中间件层（Task 3）通过这些 helpers 读取静态故障配置 + per-pin ctx。
 *
 * 边界保证：pin >= WASM_SIM_MAX_PINS (=128) 时 get_debounce_ctx 返回
 * NULL，HAL 层须把 NULL 当作"该 pin 无退化"处理。
 *
 * PRNG 推进协议：HAL 层调用 pal_wasm_get_prng_state() 取当前种子，
 * 传给算法库 (wink_phys_bus_drop 等)，算法返回时种子已被推进，HAL
 * 层用 pal_wasm_advance_prng_state() 写回。
 * ───────────────────────────────────────────────────────── */
uint32_t pal_wasm_get_bounce_us(void);
uint16_t pal_wasm_get_i2c_drop_permil(void);
uint32_t pal_wasm_get_prng_state(void);
void     pal_wasm_advance_prng_state(uint32_t new_state);
wink_phys_debounce_ctx_t *pal_wasm_get_debounce_ctx(uint16_t pin);

#endif /* PAL_WASM_INTERNAL_H */
