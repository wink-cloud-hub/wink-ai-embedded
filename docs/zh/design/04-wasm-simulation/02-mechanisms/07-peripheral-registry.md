# 虚拟外设注册表与引脚仲裁

| 项 | 内容 |
|---|---|
| 层级 | Ⅱa 核心机制规范（Public Architecture Specification） |
| 状态 | **Active**（开源规范版；对齐 UniSim 3.0 SSOT） |
| 关联 C-ABI | [`wink-micro-os/targets/wasm/wasm_bridge.h`](../../../../../wink-micro-os/targets/wasm/wasm_bridge.h) |

> 💡 **架构契约与分工说明**：
> - 本机制规范作为公开架构标准与功能契约；
> - C 语言嵌入式运行时（WinkMicroOS）通过标准 ABI 头文件与仿真环境解耦对接。高保真仿真调度引擎与物理标定资产由 UniSim 商业引擎闭源交付。
---

## 1. 机制概述

定义外设生命周期、电源域管理与四值逻辑（0/1/Z/X）多驱动强度引脚仲裁模型。

---

## 2. 核心架构原理与边界

1. **确定性与无偏承诺**：本机制保证在微秒级虚拟时基推进下，同输入轨迹可完全复现。
2. **C/Wasm 契约解耦**：C 侧代码通过 `pal_wasm_*` 与 `wasm_bridge.h` 暴露行为，不侵入业务逻辑。
3. **分层防护**：符合 [01-overview](../01-overview/01-architecture.md) 与 [03-axes](../03-axes/00-README.md) 定义的保真度约束。

详见关联保真轴与 C 契约验收规范：[`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md)。
