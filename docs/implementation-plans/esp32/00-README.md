# ESP32 仿真与兼容实施计划库 (Implementation Plans - ESP32)

本目录为 WinkMicroOS `esp32` 仿真拦截与真机生态的实施计划库（Layer-③ 实施计划）。

## 核心计划索引

| 计划文档 | 状态 | 目标里程碑 | 说明 |
| :--- | :--- | :--- | :--- |
| **[2026-09-22-esp-idf-simulation-interception-master-plan.md](./2026-09-22-esp-idf-simulation-interception-master-plan.md)** | 📋 就绪 (v3.4) | M0~M3 | **ESP-IDF 源码级仿真拦截总纲（执行第一纲领 / SSOT）**<br>涵盖 C-ABI 驱动门面、SoC 特性矩阵解耦、ESP-IDF v5/v6 双门面兼容、FreeRTOS→`wink_sim_scheduler` 映射、语料 Tier 分级与总纲级任务 T-001~T-012。 |
| [2026-09-22-esp-idf-simulation-interception-master-plan-review.md](./2026-09-22-esp-idf-simulation-interception-master-plan-review.md) | 📎 归档 | — | v1.0 架构评审记录（其 P0/P1 已在总纲 v2.0/v3.0 闭环）。 |

### 派生子计划（模板命名，T-003 已落盘）

| 子计划 | 里程碑 | 状态 | 文档路径 |
| :--- | :--- | :--- | :--- |
| M0 骨架与最小 GPIO 闭环 | M0 | 📋 就绪执行中 (v1.1 代码事实核对修订) | [2026-09-23-esp-idf-sim-m0-gpio-plan.md](./2026-09-23-esp-idf-sim-m0-gpio-plan.md) |
| M1 FreeRTOS 调度器 Shim | M1 | 📋 待开始 (v1.1 占位+展开约束) | [2026-09-24-esp-idf-sim-m1-freertos-plan.md](./2026-09-24-esp-idf-sim-m1-freertos-plan.md) |
| M2 总线外设双版本与定点 PWM | M2 | 📋 待开始 (v1.1 占位+展开约束) | [2026-09-25-esp-idf-sim-m2-bus-plan.md](./2026-09-25-esp-idf-sim-m2-bus-plan.md) |
| M3 SoC 矩阵扩展与语料 CI | M3 | 📋 待开始 (v1.1 占位+展开约束) | [2026-09-26-esp-idf-sim-m3-soc-ci-plan.md](./2026-09-26-esp-idf-sim-m3-soc-ci-plan.md) |

## 相关架构规范与决策

- [实施计划模板](../00-IMPLEMENTATION-PLAN-TEMPLATE.md)
- [mcu-compat-plan.md](../../zh/tech-designs/mcs51/mcu-compat-plan.md)（MCU 兼容双轴模型）
- [ADR-0004 编译期静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)
- [ADR-0065 PAL 硬件 RAII 资源所有权](../../decisions/core/0065-pal-hardware-raii-resource-ownership.md)
- [ADR-0066 PWM 占空比定点规范](../../decisions/core/0066-pwm-basis-points-and-float-deprecation.md)
- [ADR-0072 双时钟域与配额片](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md)
- [ADR-0080 外部 lint pack 发现机制](../../decisions/core/0080-external-lint-pack-discovery-and-mcs51-guard-sinking.md)
