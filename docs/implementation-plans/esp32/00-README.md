# ESP32 仿真与兼容实施计划库 (Implementation Plans - ESP32)

本目录为 WinkMicroOS `esp32` 仿真拦截与真机生态的实施计划库（Layer-③ 实施计划）。

## 核心计划索引

| 计划文档 | 状态 | 目标里程碑 | 说明 |
| :--- | :--- | :--- | :--- |
| **[2026-09-22-esp-idf-simulation-interception-master-plan.md](./2026-09-22-esp-idf-simulation-interception-master-plan.md)** | 📋 就绪 (v3.5) | M0~M3 | **ESP-IDF 源码级仿真拦截总纲（执行第一纲领 / SSOT）**<br>涵盖 C-ABI 驱动门面、SoC 特性矩阵解耦、ESP-IDF v5/v6 双门面兼容、FreeRTOS→`wink_sim_scheduler` 映射、语料 Tier 分级与总纲级任务 T-001~T-012。 |
| [2026-09-22-esp-idf-simulation-interception-master-plan-review.md](./2026-09-22-esp-idf-simulation-interception-master-plan-review.md) | 📎 归档 | — | v1.0 架构评审记录（其 P0/P1 已在总纲 v2.0/v3.0 闭环）。 |
| [2026-09-24-esp-idf-sim-m1-freertos-plan-review.md](./2026-09-24-esp-idf-sim-m1-freertos-plan-review.md) | 📎 归档 | M1 | v1.0 架构评审记录（5 项核心事实与架构缺陷，已在 M1 计划 v1.1 闭环）。 |

### 派生子计划（模板命名，T-003 已落盘）

| 子计划 | 里程碑 | 状态 | 文档路径 |
| :--- | :--- | :--- | :--- |
| M0 骨架与最小 GPIO 闭环 | M0 | ✅ 已完成 (v1.4 闭环交付) | [2026-09-23-esp-idf-sim-m0-gpio-plan.md](./2026-09-23-esp-idf-sim-m0-gpio-plan.md) |
| M1 FreeRTOS 调度器 Shim | M1 | ✅ 已完成 (v1.4 全量验收闭环) | [2026-09-24-esp-idf-sim-m1-freertos-plan.md](./2026-09-24-esp-idf-sim-m1-freertos-plan.md) |
| M2 总线外设双版本与定点 PWM | M2 | ✅ 已完成 (v2.4 28/28 测试与架构加固闭环) | [2026-09-25-esp-idf-sim-m2-bus-plan.md](./2026-09-25-esp-idf-sim-m2-bus-plan.md) |
| M3 SoC 矩阵扩展与语料 CI | M3 | ✅ 已完成 (v2.3 M3-1~M3-3 全量交付验收) | [2026-09-26-esp-idf-sim-m3-soc-ci-plan.md](./2026-09-26-esp-idf-sim-m3-soc-ci-plan.md) |
| Phase 2 并发安全、忙等自愈与静态构造 | P2 | ✅ 已完成 (v1.0 45/45 测试与架构加固闭环) | [2026-09-26-esp-idf-sim-phase2-plan.md](./2026-09-26-esp-idf-sim-phase2-plan.md) |
| Phase 3 虚拟中断、NVS沙箱与堆能力 | P3 | ✅ 已完成 (v1.0 48/48 测试全绿与架构治理闭环) | [2026-09-27-esp-idf-sim-phase3-plan.md](./2026-09-27-esp-idf-sim-phase3-plan.md) |
| M4 Wi-Fi/BLE 连接性（演进路线图） | M4 | 🔄 执行中 (v2.0 M4-1~M4-3 已交付，M4-4 推进中) | [2026-09-27-esp-idf-sim-m4-wifi-ble-connectivity-roadmap.md](./2026-09-27-esp-idf-sim-m4-wifi-ble-connectivity-roadmap.md) |
| M4-1 Wi-Fi 基础连接状态机与 esp_event 事件循环 | M4-1 | ✅ 已完成 (v1.2 50/50 测试全绿与语料验收闭环) | [2026-09-27-esp-idf-sim-m4-1-wifi-event-plan.md](./2026-09-27-esp-idf-sim-m4-1-wifi-event-plan.md) |
| M4-2 MQTT 与 HTTP 通信代理与自闭环 | M4-2 | ✅ 已完成 (v1.2 60/60 测试全绿与语料验收闭环) | [2026-09-27-esp-idf-sim-m4-2-mqtt-http-plan.md](./2026-09-27-esp-idf-sim-m4-2-mqtt-http-plan.md) |
| M4-3 NimBLE 虚拟 GATT 服务与特征值抽象 | M4-3 | ✅ 已完成 (v1.2 65/65 测试全绿与语料验收闭环) | [2026-09-27-esp-idf-sim-m4-3-nimble-gatt-plan.md](./2026-09-27-esp-idf-sim-m4-3-nimble-gatt-plan.md) |
| M4-4 UniSim Web 交互式虚拟蓝牙调试面板 | M4-4 | 🔄 推进中 (v1.0 计划草案编制就绪) | [2026-09-27-esp-idf-sim-m4-4-unisim-ble-inspector-plan.md](./2026-09-27-esp-idf-sim-m4-4-unisim-ble-inspector-plan.md) |
| ESP-IDF 仿真基建加固 | H0～H8 | ✅ 已完成 (86/86 CTest 闭环交付) | [2026-09-28-esp-idf-simulation-hardening-plan.md](./2026-09-28-esp-idf-simulation-hardening-plan.md) |
| ESP-IDF 深度架构评估与官方示例迁移总纲 | M5~M8 | 📋 就绪 (战略路线已发布) | [2026-09-29-esp-idf-simulation-deep-architecture-analysis-and-migration-strategy.md](./2026-09-29-esp-idf-simulation-deep-architecture-analysis-and-migration-strategy.md) |
| 自治自愈 Loop 底座主计划 | M9 | 📋 就绪 (v2.3 双 Agent 对抗自愈) | [2026-10-02-autonomous-self-healing-loop-plan.md](./2026-10-02-autonomous-self-healing-loop-plan.md) |
| Loop 外置插件外设自主闭环与规范约束计划 | M10 | 📋 就绪 (v1.0 外设总纲融入与防腐) | [2026-10-02-loop-peripheral-plugin-governance-plan.md](./2026-10-02-loop-peripheral-plugin-governance-plan.md) |
| **高 ROI 核心功能实施规划与执行路线图** | M11 | 📋 就绪 (四维 ROI 模型与三波执行梯队) | [2026-10-07-esp-idf-high-roi-checklist-execution-plan.md](./2026-10-07-esp-idf-high-roi-checklist-execution-plan.md) |

## 相关架构规范与决策

- [实施计划模板](../00-IMPLEMENTATION-PLAN-TEMPLATE.md)
- [mcu-compat-plan.md](../../zh/tech-designs/mcs51/mcu-compat-plan.md)（MCU 兼容双轴模型）
- [ADR-0004 编译期静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)
- [ADR-0065 PAL 硬件 RAII 资源所有权](../../decisions/core/0065-pal-hardware-raii-resource-ownership.md)
- [ADR-0066 PWM 占空比定点规范](../../decisions/core/0066-pwm-basis-points-and-float-deprecation.md)
- [ADR-0072 双时钟域与配额片](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md)
- [ADR-0080 外部 lint pack 发现机制](../../decisions/core/0080-external-lint-pack-discovery-and-mcs51-guard-sinking.md)
