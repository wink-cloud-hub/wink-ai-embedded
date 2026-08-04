/**
 * @file avoidance_car_semantic_baseline.h
 * @brief avoidance_car 业务字段 baseline（fixup 计划 F5 R5）。
 *
 * 生成日期：2026-07-02（协作式调度器合入 + F1-F4 fixup 完成后）
 * 生成来源：test_app_e2e.c 双 tick 场景运行的实测业务字段值
 *
 * ⚠️ 变更此文件前请：
 *   1. 走 test_app_e2e.c 相同场景本地跑一次，人工比对 diff；
 *   2. 若差异属于合理演进（如舵机默认角度调整），更新此 baseline 并同步 code review 备注；
 *   3. 若差异不明或涉及调度器状态泄漏 —— 优先怀疑是回归 bug。
 */
#ifndef AVOIDANCE_CAR_SEMANTIC_BASELINE_H
#define AVOIDANCE_CAR_SEMANTIC_BASELINE_H

/* Case 1: 无近障（echo ≈100cm > 20cm 阈值）→ 舵机复位到 90° */
#define AVOIDANCE_CAR_BASELINE_SERVO_ANGLE_CLEAR    900

/* Case 2: 近障（echo ≈10cm < 20cm 阈值）→ 舵机扫到 180° */
#define AVOIDANCE_CAR_BASELINE_SERVO_ANGLE_NEAR     1800

/* 两个 tick 均无 fault 期望：trace_count 保持 0 */
#define AVOIDANCE_CAR_BASELINE_TRACE_COUNT          0u

#endif /* AVOIDANCE_CAR_SEMANTIC_BASELINE_H */
