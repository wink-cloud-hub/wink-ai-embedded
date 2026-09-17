# ADR-0071：Plant 模型终态架构：双供给链与单一解析面（Plant Model Provider）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-09-17：Phase 4.3 W0 决策门拍板，承接 Q43-2/Q43-4 及 P1~P3）** |
| 日期 | 2026-09-17 |
| 触发 | S3 物理插件生态（`wink-plugin-plants`）与 FMI 工业制品共存演化；存量 `PlantDriverRegistry` 缺乏优先级且重复注册崩溃（审查 X-1）；场景 `kind` 静态封死无法支持第三方模型扩展（审查 X-3） |
| 影响范围 | `@wink-ai/unisim` 内核、场景 DSL Schema、`wink-plugin-plants` 独立仓、FMI Wasm 加载管道、浏览器/Worker 运行时 |
| 决策者 | 仿真架构治理组、内核组、生态扩展组 |
| 关联 | [ADR-0055](../unisim/0055-sim-fp-determinism-and-golden-policy.md)（浮点确定性分级）、[ADR-0067](../unisim/0067-appliance-plant-profile-architecture.md)（激励互斥与安规隔离）、[ADR-0083/0084](../core/0083-adopt-gpl-3.0-only-license-policy.md)（分层开源许可）、[Plant 模型终态架构讨论稿](../../../../wink-ai/packages/unisim/docs/architecture/plant-model-provider-end-state.md) |

---

## 背景（Context）

在 WinkMicroOS 与 UniSim 的闭环仿真体系中，被控物理对象（Plant）是固件控制闭环的关键右翼（如加热盘、电热丝、直流电机、液位传感器等）。

随着系统向 Phase 4.3 演进，仿真系统同时面临两类异构物理模型的供给需求：
1. **轻量、敏捷的开源/企业私有 TypeScript 物理插件**（S3 形态：以 npm 包发布，包含微分公式与机理 3D/UI 可视化）；
2. **重度、高精的工业级 FMI/FMU Co-Simulation 制品**（S2 形态：由 Simulink/Modelica 导出，编译为 `.fmu.wasm`，自带高阶数值积分器）。

**代码现状的致命缺陷**：
1. **注册机制脆弱**：`PlantDriverRegistry.register()` 在遇到同名 kind 时直接抛出致命错误，不支持外置模型对内置默认实现的**安全覆盖（Override）与优先级仲裁**（代码审查 X-1）；
2. **场景 Schema 静态死锁**：`scenario.schema.ts` 的 `PortPlantModelSchema` 使用了编译期闭合的静态可辨识联合（`first_order_thermal | dc_motor_kinematics | fmu`），导致任何第三方物理模型在不修改并重新发布 unisim 内核源码的前提下，**根本无法在场景中声明与加载**（代码审查 X-3）；
3. **分叉维护风险**：若不在此刻收敛，场景语法、加载机制、诊断码和 V&V 对标体系将按照“插件”和“FMU”分化为两套独立的特化分支，系统技术债务呈指数级膨胀。

---

## 决策结论（Decision）

确立 **“双供给链，单一解析面，编排器零分支”** 的工业级终态架构：

```text
场景 DSL（开放 Kind Seam，通过 PlantModelRef 统一声明）
  model: { kind: string, provider?: 'builtin'|'plugin'|'fmu', id?: string, version?: string, sha256?: string }
        │
        ▼
PlantModelResolver（唯一引用解析面；宿主无关，装载期 Fail-Loud）
  ├── BuiltinProvider → 默认基线 TS 驱动（first_order_thermal / dc_motor_kinematics）
  ├── PluginProvider  → wink-plugin-plants npm 包（三阶优先级 dev > user > builtin，SemVer 仲裁）
  └── FmuProvider     → .fmu.wasm 工业制品（FmuArtifactResolver + sha256 lockfile 三方一致校验）
        │
        ▼ 产物 100% 统一收敛
IPlantModelDriver V1（不变式契约）
        │
        ▼
PlantOrchestrator + PlantHostBinding（Invariant I4：编排器彻底消除 kind 分支）
```

### 1. 核心架构三原则

1. **供给链该分（Dual Supply Chains）**：
   * TS 插件（源码可审、npm 语义化版本分发、离散 bit-exact 确定性）；
   * FMU 制品（二进制黑盒、sha256/lockfile 信任锚、连续 tolerance 容差标定、自带 Co-Simulation 求解器）；
   * 严禁将两类异构产物强行合并为同一种文件格式。
2. **解析面必须合（Single Resolution Surface）**：
   * 引入统一的 `PlantModelResolver` 作为模型装载的单一事实入口；
   * 统一定位装载期错误，对外抛出带步骤定位的 `PLANT_MODEL_*` 诊断码；
   * 编排层、执行层与宿主层完全不感知底层模型的来源通道。
3. **编排器零分支（Invariant I4）**：
   * 运行时核心 `PlantOrchestrator` 只认微步调度（Substep Budget）、端口依赖拓扑图（Graph）与仿射换算（Affine），绝对禁止出现 `if (provider === 'fmu')` 分支。

### 2. 拍板决议细则（承接 Phase 4.3 W0 决策门）

* **决议 1：Resolver 归属模式（P1 决议）**
  `PlantModelResolver` 及其装载器实现完整内置在 `@wink-ai/unisim` 引擎内部；对外暴露的 SDK 仅导出 `PlantModelRef`、`IPlantModelDriver` 与 `PlantModelProvider` 纯类型契约。外部插件作者只依赖类型，无需引入庞大的引擎内核。
* **决议 2：场景 Schema 开缝机制（P2 决议）**
  彻底打破 `model.kind` 的静态枚举。`kind` 开放为 `z.string().min(1)`，由装载期 Manifest 与 Resolver 进行语义与端口强校验。彻底抹去 `fmu` 或特定厂商的硬编码特权分支。
* **决议 3：插件制品严格隔离（P3 决议）**
  禁止在 `wink-plugin-plants` 插件源码包内直接硬打包 `.fmu.wasm` 二进制。所有 FMU 工业制品必须走 S2 独立制品库并经 `fmu.lock.json` 进行 SHA256 校验，严防开源许可证（LGPL/GPL）污染与安全合规风险。
* **决议 4：三阶优先级仲裁规则**
  当不同来源提供相同 `kind` 的物理模型时，解析器必须严格遵循以下确定性仲裁规则：
  $$\text{本地开发覆盖 (dev)} > \text{项目安装指定 (user)} > \text{引擎内置基线 (builtin)}$$
  在同一优先级内，最高兼容语义化版本（SemVer）胜出。外置插件可按优先级覆盖内置实现；停用外置插件时，系统可瞬间单行回滚到内置默认模型，保证零破坏。

---

## 后果与约束（Consequences & Constraints）

### 积极影响
1. **解耦生态**：第三方与算法工程师无需接触 UniSim 核心源码，即可通过独立的 npm 包或 FMU 文件发布受控物理模型；
2. **消除重复债务**：场景 Schema、错误诊断体系、可视化 UI 挂载和 V&V 对拍验证不再按来源分叉；
3. **平滑向前兼容**：存量场景无需修改任何 DSL，缺省 `provider` 时自动映射至内置参考实现，`test:golden:m3` 基线 100% 保持。

### 约束与破坏性防线
1. **安规与激励互斥红线（ADR-0067）**：无论模型来自 TS 插件还是工业 FMU，被控对象（Plant）只计算物理规律，严禁参与固件控制决策；
2. **Worker 域加载安全**：外置插件在浏览器或 Worker 中加载时，必须通过 Blob URL / 独立沙箱载入，严禁引入 Node.js 原生模块（`fs`, `child_process`），违者装载期 Fail-Loud。

---

## 实施映射（Phase 4.3 任务）

| 任务卡 | 实施内容 | 验收门禁 |
|---|---|---|
| **P43-T5** | 核心重构：`PlantModelResolver` + `scenario.schema.ts` Kind Seam 开缝 | `bun test resolver.test.ts`；三阶优先级单测通过 |
| **P43-T6** | 建立 `wink-plugin-plants` 独立多包仓库骨架 | 仓库通过许可证门禁，支持 Worker-safe 加载 |
| **P43-T7** | 外置 `first_order_thermal` 试点（零破坏证明） | `test:golden:m3` 在插件覆盖下 100% 零漂移通过 |
| **P43-T8** | 第二外置模型（`dc_motor_kinematics`）与开发者 SOP 移交 | 跑通端到端浏览器仿真；发布作者开发指南 |
