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

#endif /* PAL_WASM_INTERNAL_H */
