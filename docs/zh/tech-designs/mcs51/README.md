# MCS-51/8051 零侵入仿真拦截层技术方案与 RFC (MCS-51 Tech-Designs)

本目录归档 8051/Keil C51 仿真拦截层（`frameworks/mcs51/`）的详细设计、时钟域契约、数据面规范与 Spike 实验资产。

---

## 📂 技术设计索引

| 文档名称 | 核心主题与设计目标 | 管辖 ADR / 状态 |
| :--- | :--- | :--- |
| **[2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md](./2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md)** | MCS-51 零侵入仿真拦截层总纲与代理总架构 | [ADR-0070](../../../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md) (Accepted) |
| **[2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md](./2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md)** | SFR 代理、影子内存、RMW 红线与引脚边沿检测数据面 | [ADR-0071](../../../decisions/core/0071-sfr-proxy-rmw-edge-data-plane.md) (Accepted) |
| **[2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md](./2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md)** | 双时钟域映射、配额强制切出与 Catch-Up 补账时序方案 | [ADR-0072](../../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md) (Accepted) |
| **[2026-08-27-mcs51-user-code-compatibility-and-limitations-guide.md](./2026-08-27-mcs51-user-code-compatibility-and-limitations-guide.md)** | 用户 Keil C51 源码兼容性矩阵、方言擦除与不支持清单 | 设计指南 (Active) |
| **[mcu-compat-plan.md](./mcu-compat-plan.md)** | 多架构 MCU 兼容路线与生态扩展综合规划 | 路线规划 (Accepted) |

---

## 🔬 原型实验 (Spikes & Assets)

- **[spikes/S1-yield-api.md](./spikes/S1-yield-api.md)**：Spike S1 - 协作式 Fiber 让出与配额切出原型
- **[spikes/S2-compiler-dialect-chain.md](./spikes/S2-compiler-dialect-chain.md)**：Spike S2 - 编译器方言清洗链（Keil `.c` ➔ C++17 `.cpp`）
- **[spikes/S3-codegen-thermal-contract.md](./spikes/S3-codegen-thermal-contract.md)**：Spike S3 - 板级 Codegen 与 NTC 热闭环契约
