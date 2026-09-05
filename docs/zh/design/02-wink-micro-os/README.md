# WinkMicroOS 运行时引擎架构设计 (DAL & PAL)

本目录包含 **WinkMicroOS** 运行时内核的详细设计规范。WinkMicroOS 是整个低代码平台的基石，它通过向业务层屏蔽底层芯片总线的电气时序，实现应用业务代码的同源开发与多平台部署。

---

## 📂 模块设计文档

*   **[00-wink-micro-os-market-analysis.md](./00-wink-micro-os-market-analysis.md) - 商业价值、竞品分析与市场前景调研报告**
    *   以严苛的商业和技术标准，对 `wink-micro-os` 的定位、市场机会、核心受众及开源竞品进行深度剖析，论证其范式创新价值与独特生态位。
*   **[01-dal-device-abstraction.md](./01-dal-device-abstraction.md) - 器件抽象层 (DAL) 设计规范与设备树静态生成**
    *   详细说明如何提供传感器和执行器的语义级封装，如何通过编译宏控制真机与仿真两种执行分支，以及如何通过前端拓扑在编译时生成 C 代码设备树。
*   **[02-pal-platform-abstraction.md](./02-pal-platform-abstraction.md) - 平台抽象层 (PAL) API 规范**
    *   细化定义跨平台的硬件抽象总线接口（GPIO、PWM、I2C、SPI、ADC）与操作系统包装层接口（OSAL 任务、信号量、时钟周期、延时），规范 Targets 对接规范。
*   **[03-directory-architecture.md](./03-directory-architecture.md) - 内核目录架构设计（A*）**
    *   Ports & Adapters 内核骨架（pal INTERFACE / dal / runtime / trace 一等 peer / targets），公共 API 面与 App/BAL 禁入规则，CMake 库依赖图。
*   **[04-runtime-and-trace.md](./04-runtime-and-trace.md) - 运行时生命周期与 Golden Trace 契约**
    *   回调注入主循环（`wink_app_callbacks_t`）、tick 调度、fault 上报 trace、target entry 接线流程。
*   **[05-hardware-and-fidelity-testing-guide.md](./05-hardware-and-fidelity-testing-guide.md) - 硬件与保真测试指南**
*   **[06-bal-layer.md](./06-bal-layer.md) - BAL 业务抽象层（域划分 / 命名 / 依赖 / CI）★ SSOT**
    *   物理增强 · math · control 三域；文件与 API 命名；actuator vs control；硬切割目标态与评审清单。决策：[ADR-0037](../../decisions/core/0037-bal-domain-partition-and-closed-loop-motor.md)、[ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md)。

---

## 📐 分层与数据透传设计

```text
       [ App (AI 生成) / BAL (内核静态库) ]
                   │ (调器件语义 API / 注册回调)
                   ▼
     ┌───────────────────────────────┐
     │  runtime (主循环) + trace ◄── peer 一等层
     └───────────────┬───────────────┘
                   │ (调 DAL)
                   ▼
     ┌───────────────────────────────┐
     │     器件抽象层 (DAL)           │ ◄── SIMULATION 旁路直通 ──► [ Web 虚拟外设 UI ]
     └───────────────┬───────────────┘
                   │ (调总线与 OS API)
                   ▼
     ┌───────────────────────────────┐
     │     平台抽象层 (PAL)           │   ← INTERFACE 契约
     └───────┬───────────────┬───────┘
             ▼               ▼ (CMake 静态装配路由 - ADR-0041)
       [ targets/ (HAL适配) ]  [ osal/ (OS适配) ]   (targets/<plat> × osal/<variant>)
```

WinkMicroOS 的设计哲学是：**把复杂细节留在底层实现（通过严苛测试和条件编译进行双轨分流），向顶层应用暴露最符合人脑自然感知的物理世界语义。**

