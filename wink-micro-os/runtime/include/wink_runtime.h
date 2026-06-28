/**
 * @file wink_runtime.h
 * @brief OS 主循环入口（target-agnostic）。
 *
 * 各 target 的 *_entry.c 实例化 wink_app_callbacks_t 后调用 wink_runtime_run。
 * 调度器仅用 PAL OSAL 做 tick，挂起语义由 target 实现（ADR-0002 双 target 对齐落点）。
 *
 * Tick 周期 SSOT：优先从 wink_config.h 获取（由 wink_app.json codegen 生成），
 * 若未定义则退化为 10ms 兼容默认值。
 */
#ifndef WINK_RUNTIME_H
#define WINK_RUNTIME_H

#include "wink_app.h"
#include "wink_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 运行 OS 主循环
 * @param callbacks App 生命周期回调（NULL 返回 WINK_ERR_INVALID_ARG）
 * @param max_ticks 最多跑多少个 loop tick；传 0 表示无限循环（真机/wasm）
 * @return WINK_OK（max_ticks>0 跑完后）或 WINK_ERR_INVALID_ARG
 * @note host/测试传有限 max_ticks 避免 while(1) 卡死
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_runtime_run(const wink_app_callbacks_t *callbacks, uint32_t max_ticks);

/** @brief runtime 内部故障码：boot 时检测到 WDT/PANIC 复位（Phase 5 Task 5-5） */
#define WINK_FAULT_BOOT_AFTER_RESET 8001u
/** @brief runtime 内部故障码：单个 callback 执行时间超过限制 (细粒度 WCET 警告) */
#define WINK_WARN_WCET_EXCEEDED     8002u
/** @brief runtime 内部故障码：全局 tick 总时间超过限制 (全局 WCET 警告) */
#define WINK_WARN_TICK_OVERRUN      8003u

/* ---- Boot safe-lock 恢复策略常量（ADR-0010，修订 ADR-0007）---- */
/** @brief 连续异常复位锁死阈值：达此值才锁死；单次/偶发复位自动恢复 */
#define WINK_BOOT_LOCK_THRESHOLD    3u
/** @brief 健康里程碑 tick 数（≈2s @默认 10ms tick）：init 成功且跑满则清零异常复位计数 */
#define WINK_BOOT_HEALTHY_TICKS     200u

/**
 * @brief 显式故障路径（Phase 5 Task 5-3）。
 * @note 顺序：wink_trace_fault → wink_actuator_safe_off_all → callbacks->on_fault。
 *       先关断所有执行器，再通知 App。fail-safe 回调不得阻塞。
 *       当前 void 回调无法自动捕获 App 错误，故暴露显式 fault API（App/驱动在检测到不可恢复
 *       状态时主动调用）；回调返回值迁移到 status 为 follow-up。真挂死/CPU 卡死靠硬件 WDT 兜底。
 */
void wink_runtime_fault(const wink_app_callbacks_t *callbacks, uint32_t fault_code);

/*
 * Phase 5 Task 6（预留，Future Work）：分频调度 / 优先级时间轮（Task Prescaling）。
 * 打破单一 app_loop 10ms 强制同步轮询：允许组件注册不同频率的 tick 回调（如 1ms 电机 PID、
 * 20ms 超声波状态机、500ms 日志心跳）。本阶段仅预留模型——后续在 wink_app_callbacks_t 扩展
 * 或设计文档中定义 multi-rate 回调接口，并铺垫 CPU 负载隔离（保障 fail-safe 监控实时性）。
 */

#ifdef __cplusplus
}
#endif

#endif /* WINK_RUNTIME_H */
