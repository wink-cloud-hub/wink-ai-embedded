# ADR-0070：场景 t=0 初始条件与供电循环契约（通用物理环境预置）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-09-15：unisim Q6/T7 评审签发；D-005a 实现余项在本仓 `PLAN-20260915` 跟踪）** |
| 日期 | 2026-09-13（提议） |
| 触发 | `PLAN-20260915-APPLIANCE-SAFETY-AND-GB4706` D-005a/b：固件可能在首个 tick 前采样物理量（上电热态检测）或读取引脚（POST 卡键抑制），运行中注入与之存在竞态；安规场景需要通用的"物理环境预置 + 供电循环"能力 |
| 影响范围 | unisim 场景 Schema（可选字段与阶段语义）、scenario session 与 headless runner、场景作者与 CI、跨应用/跨品类复用 |
| 决策者 | 嵌入式与仿真架构联合组 |
| 关联 | [ADR-0067](./0067-appliance-plant-profile-architecture.md)（激励源隔离）、[ADR-0069](./0069-appliance-cross-category-safety-extension.md)（跨品类安规）、一致性规范 C14.5 |

---

## 背景（Context）

`winksim` 现行场景执行模型：

- `scenario-session.start(startClockUs, resetToZero)` 只在**场景启动时**提供一次复位；
- 所有 step（含 `INPUT_ANALOG` / `INPUT_PLUGIN_EVENT` / `CONFIG_PIN`）都按虚拟时间轴在**固件运行中**应用；
- `SYSTEM_CONTROL → HARD_RESET / SOFT_RESET` 在 headless dispatcher 中为空实现（no-op）；
- 没有"固件启动前"的输入应用阶段。

因此，任何在 `main()` 早期就采样物理环境的固件（不止养生壶：温控器开机读探头、电机类开机读限位/按键、POST 自检类应用）都无法被场景表达"开机瞬间的物理初值"；若用 30 ms 的 `INPUT_ANALOG` 近似，会与 boot 采样产生竞态（`PLAN-20260915` 场景 2/3 即为此设计性缓落）。

本 ADR 固化**通用能力契约**。养生壶（`mcs51_health_pot`）只是首个消费者；引擎侧不得出现任何应用/设备业务知识，否则将破坏可扩展与可维护性（延续 ADR-0067 §2 的物理环境隔离原则）。

## 决策结论（Decision）

1. **通用阶段模型（Step-Lock 扩展）**：
   ```text
   校验场景 → 应用初始条件到物理环境 → 复位/启动 MCU → 时间轴 t=0 起跑 → 时间轴 step（可覆盖初值）
   ```
   作为**可选**字段/阶段实现（建议名 `header.initialConditions`，最终由 unisim schema 评审定名）。缺省行为与现状完全一致，存量场景零改动。不新增 step 类型：初始条件复用既有输入面（模拟通道值、引脚电气/电平、插件输入态）。
2. **可寻址空间与禁止项**：初始条件只能写通用输入面（通道 / 引脚 / 插件输入态），**不得**出现设备或业务名（如 NTC、磁控管、heater）、固件内部变量或安全常数（延续 ADR-0067 §2 与 C14.5）。
3. **覆盖与可观测**：初始条件在 boot 前生效并持续，直到时间轴 step 覆盖（后写者赢，与现有输入语义一致）；初值应用必须进入 trace/报告，可被断言与审计。
4. **供电循环语义（可选，独立于 1–3）**：实现 `SYSTEM_CONTROL → HARD_RESET / SOFT_RESET`：
   - `HARD_RESET`：MCU / 外设 / 固件内存按 ABI 复位；**物理环境（插件值、引脚外部驱动、虚拟时钟）保留**；场景时间轴继续；要求幂等、可重复执行；
   - `SOFT_RESET`：仅复位程序运行状态（具体边界由 unisim 定义并测试）；
   - 定位：为"运行 → 掉电重上电 → 继续断言"的单场景序列提供语义；**不替代**真实电源插件建模，后者如需要另行演进。
5. **通用性与反特化门禁（强制）**：
   - unisim 侧新增**与应用无关的 runner 单测**：阶段顺序、初值覆盖规则、reset 前后不变量；
   - ADR 验收要求**第二个无关应用**使用同一能力编写并跑通一个场景（跨应用复用证明），防止为单一应用特化；
   - 禁止项：应用专属 step/字段（如 `PRIME_NTC`、`healthPotHotBoot`）、runner 内业务知识、固件 `#ifdef SIMULATION` 测试钩子。
6. **兼容性**：全部为向后兼容的可选新增；`SimTraceSpecV2` 输出协议不变（本次仅输入侧与执行阶段）。

## 后果与约束（Consequences & Constraints）

- 实现主体在 **unisim**（schema / dispatcher / session 与 runner 单测）；**wink-tools 无需改动**，除非未来新增 CLI 覆盖参数（如命令行强制初值），那将另走工具域契约。
- 本仓（`wink-ai-embedded`）不实现 runner，仅消费契约：`PLAN-20260915` 场景 2/3 待 **D-005a** 落地后补齐；落地前严禁硬写 JSON（避免假阳性）。
- 养生壶的上电热态锁定是首个消费者；不得因此让引擎认识任何养生壶语义。

## 实施拆分（D-005a / D-005b）

| 编号 | 范围 | 优先级 |
|---|---|---|
| **D-005a** | 初始条件阶段：Schema 可选字段 + session/runner 阶段时序 + 覆盖规则 + runner 单测；验收 = 两个 `safety-*` 场景在 CI 可断言（热初值续锁被拒 / t=0 卡键抑制） | 必需 |
| **D-005b** | `HARD_RESET/SOFT_RESET` headless 实现与语义测试；验收 = "运行→复位→重上电"单场景序列 | 可选（按需） |

## 遵循与后续（Compliance & Follow-up）

- 已回写（2026-09-15）：[01-consistency-spec.md](../../zh/design/04-wasm-simulation/04-assurance/01-consistency-spec.md) §C14.6「t=0 预置与复位语义」（五字段条款）与 checklist C14.6 行。
- **unisim 评审结论（2026-09-15，Plant 侧）**：
  - 已落地：`header.initialConditions.plants.<id>.{parameters,inputs}`（Plant 侧映射 `model.parameters`/端口初值，unisim design §5.6）；`HARD_RESET` 保留 Plant 物理状态与参数（不重建 Plant 实例）；初值应用进运行报告；契约测试与热态起步断言已在 unisim 侧取证，`health-pot-fast-boil` 双跑一致性保持 12/12；
  - **应用面补充（2026-09-15 复审）**：决策 2 所列"通用输入面（通道 / 引脚 / 插件输入态）"在 Plant 语境下即第四应用面 `header.initialConditions.plants.<id>.{parameters,inputs}`，属决策 1 阶段模型的同阶段映射（见 C14.6），非设备/业务专属字段；D-005a 余项仍指**固件侧**通用输入面之 boot 前预置；
  - 应用面清单：unisim 评审产物 `2026-09-15-adr-0070-application-surface`（决策：安规场景 1/4/5 保持静态注入/HIL；场景 2/3 需通用输入面之 boot 前预置，或 Plant boot 前初值输出发布——二选一）；
  - 仍待补（D-005a 实现余项，不阻塞本 ADR 契约）：通用输入面（模拟通道/引脚/插件输入态）之 boot 前预置；`safety-power-cycle-hot-reboot`、`safety-post-jammed` 两场景 CI 断言；§5 第二无关应用复用证明。
- 场景侧：`PLAN-20260915` 场景 2/3 在 D-005a 交付后补齐；验收必须包含跨应用复用场景与 runner 单测。
- 与 ADR-0067/0069 的关系：本 ADR 只定义**输入侧通用能力**，激励源互斥与安全硬常数隔离条款不变。

---

*本 ADR 状态变更请在此记录：*
- 2026-09-13：Proposed（随 `PLAN-20260915` D-005a/b 拆分提出）
- 2026-09-15：Accepted（unisim Q6/T7 评审签发；Plant 侧语义已实现并取证，通用输入面与场景验收在 `PLAN-20260915` D-005a 跟踪；C14.6 与 checklist 回写完成）
- 2026-09-15：复审补充（不改决策语义）：回写链接改为可解析 Markdown 链接；应用面澄清 `plants.<id>.{parameters,inputs}` 属决策 1 同阶段映射（见 §遵循与后续）
