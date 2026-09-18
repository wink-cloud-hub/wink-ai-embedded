# 避障小车 Phase 1 — 舵机观测闭环实施计划（已归档）

> **状态**：✅ 已完成（2026-07-11 立项并关闭；Phase 2 可选项目见全量正文 §10.4–§10.6）
> **归档说明**：本计划正文含私有仓实现路径与调试细节，已按文档密级策略整篇移入本地私有通道；
> 本文件仅保留机制概览与公开契约面。
> **归档说明**：本计划正文含内部实现路径与调试细节，已归档存档；本文件保留机制概览与公开契约面。

## 目标

在 Workbench Simulate 模式下加载避障模板后，拖动超声距离滑块 → Wasm `avoidance_car` 固件执行避障逻辑 → 前端**实时显示舵机角度**（90° ↔ 180°），形成可演示、可单测的最小闭环。

## 机制概览（公开面）

- **双层执行器观测模型**：
  - Raw 层（Worker）：读取 Wasm `pal_wasm_get_pwm_duty_percent` 与 GPIO，产出 `ActuatorOutputBatch`；
  - Semantic 层（主线程 Mapper）：结合 `actuatorSources`、外设 `actuatorObserve` 配置与 `bindings`，产出统一的 `ActuatorObservation[]`；
  - 消费层（UI / 未来 ActuatorMirror）：只读 `ActuatorObservation`，不感知底层是 PWM / FOC / VESC。
- **事件通道（W3b Spike 方案 A）**：扩展 `STATE_UPDATE.payload.actuatorOutputs`，不新增独立 `actuatorOutput` 事件。
- **Wasm bridge 最小增量**：`pal_wasm_get_pwm_duty_percent` 作为 duty SSOT；`pal_wasm_get_servo_angle` 保留为 C 侧 SG90 模型便捷读口，前端不直接依赖。
- **ID SSOT**：画布 `CircuitComponentInstance.id`（= Manifest `devices[].componentId`）。
- **范围边界**：不修改 App 层（`wink-micro-app/avoidance_car`）与 DAL 公开 API；不引入 3D / raycast / motor / ActuatorMirror（属 Phase 2）。

## 公开契约与关联

- ABI：`wasm_bridge.h`（`pal_wasm_get_pwm_duty_percent`）
- 载荷/清单：`STATE_UPDATE`、`wink-app.json` Manifest、`SimTraceSpecV2`
- 相关公开设计：[W3b 执行器](../../design/05-frontend-workbench/03-dual-viewport-phased-design/04-phase-w3b-physics-actuators.md)、[W3c 传感器桥接](../../design/05-frontend-workbench/03-dual-viewport-phased-design/05-phase-w3c-sensors-env-bridge.md)
- 后续计划：[Catalog SSOT 收敛](./2026-07-11-catalog-ssot-convergence-plan.md)
