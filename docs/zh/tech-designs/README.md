# Wink-AI 核心技术方案与 RFC 中心 (Tech-Designs RFC)

本目录归档 Wink-AI 平台 4 大正交领域（`core`, `tools`, `frontend`, `unisim`）的技术方案提案（RFC）与详细设计。

---

## 📂 领域分类索引

| 领域 (Domain) | 职责与方案范围 | 目录入口 |
| :--- | :--- | :--- |
| **`core`** | C 内核架构 (PAL/DAL/BAL)、驱动重构、中断子系统、I2C/SPI 兼容性 | [core/](core/README.md) |
| **`mcs51`** | MCS-51/8051 零侵入仿真拦截层、SFR 代理、双时钟域与 Spike 资产 | [mcs51/](mcs51/README.md) |
| **`tools`** | CLI 工具链架构、SDK 打包发布、Scannable Codegen、Layer Lint 规则 | [tools/](tools/README.md) |
| **`frontend`** | 前端工作台 HCTR 走线算法、拓扑元器件、Phase B 仿真体验 | [frontend/](frontend/README.md) |
| **`unisim`** | Wasm 仿真观察层、并发压力测试、联合仿真插件契约、Arduino 仿真门控 | [unisim/](unisim/README.md) |

---

## 🔄 与 SSOT / ADR 的联动契约
1. **方案评审通过后**：重大技术选型提炼为 `docs/decisions/` 中的 ADR 决策。
2. **结项归档闭环**：稳定的架构结论必须回写至 `docs/zh/design/` (与 `docs/en/design/`) 对应章节。
