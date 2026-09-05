# ADR-0047：FOC 前后台 ISR 分层与 `pal_hwtimer` 契约

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-28 |
| 触发 | [2026-07-28 DAL/BAL 分层架构评审](../../reviews/core/2026-07-28-dal-bal-layering-architecture-review.md) P0-2 / §四；[dal-bal-followup 计划](../../implementation-plans/core/2026-07-28-dal-bal-followup-plan.md) Task B0 Owner 默认裁决表全采纳 |
| 影响范围 | SimpleFOC 本地环：BAL `control/` 数学；DAL/target `foc_isr_trampoline`；PAL `pal_hwtimer_*` + PWM–ADC 硬件触发；仿真快环执行模型；Codegen DI 静态绑定。**不含** `dal_vesc` / ODrive |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0004](0004-static-dispatch-vs-runtime-ops.md)（静态分发）；[ADR-0023](0023-bal-business-abstraction-layer.md)（BAL 禁 `pal_*`）；[ADR-0024](0024-fault-three-phase-model-and-dal-deinit-contract.md)（nFAULT 保护路径）；[ADR-0026](0026-foc-motor-dal-bal-separation.md)（积木拆分保留；本 ADR 部分 supersede ISR/DI/`pal_hwtimer`）；[ADR-0003](../unisim/0003-simulation-fidelity-boundary.md)；[ADR-0048](0048-actuator-control-semantic-naming.md)（`dal_bldc` 命名） |
| 关联计划 | [implementation-plans/2026-07-28-dal-bal-followup-plan.md](../../implementation-plans/core/2026-07-28-dal-bal-followup-plan.md)（Wave B：B0→B1→B2） |
| 关联活规范（B2 已回写） | [01-dal-device-abstraction.md](../../zh/design/02-wink-micro-os/01-dal-device-abstraction.md) §8.1；[06-bal-layer.md](../../zh/design/02-wink-micro-os/06-bal-layer.md) §4.6；[02-pal-platform-abstraction.md](../../zh/design/02-wink-micro-os/02-pal-platform-abstraction.md) §2.2 |

---

## Scope / Non-goals

| | 说明 |
|---|---|
| **Scope** | **仅**约束 **SimpleFOC 本地算法型** FOC（主控 MCU 跑 Clarke/Park/SVPWM/电流环，需 10kHz ISR / PWM–ADC 同步 / `pal_hwtimer`）。`dal_bldc` 积木命名对齐 [ADR-0048](0048-actuator-control-semantic-naming.md)。 |
| **Out of scope** | **VESC / ODrive** 外部智能驱动器：`dal_vesc` 落 **`actuator/`**（运动执行业务语义）；实现为 CAN/UART **协议组帧**，**无**主控 ISR / **无** `pal_hwtimer`；**不受本 ADR / Wave B 门禁约束**，可独立计划先行（C-005）。勿因「走总线」改放到 `comm/`。 |
| **Non-goals** | 本 ADR **不**交付 FOC 算法、ISR 实现、`pal_hwtimer` 真机代码或仿真 plant 实现；实现挂 Wave C / 独立 FOC 计划（C1/C2）。 |

---

## 背景（Context）

1. **评审 P0-2**：DAL §8.1 曾写 SimpleFOC 的 10kHz+ 硬中断计算「归入 BAL 级别」，但 BAL 公共头禁 `pal_*`（除 `pal_log.h`），且 BAL 定位为可复用纯业务逻辑。10kHz ISR + 硬件定时器触发本质平台强相关——「整包归 BAL」与既定红线冲突。
2. **评审 §四**：PAL 尚无通用硬件定时器 / ISR 注册契约，FOC 需要 `(a) pal_hwtimer_*` 公共契约 vs `(b) target 私有` 二选一；该边界此前为空。
3. **ADR-0026 vs ADR-0004 / BAL 禁 pal**：ADR-0026 正确拆了「DAL 硬件积木 + BAL 大脑」，但 §1 示例用运行期 `get_angle_fn` / `set_voltage_fn` 函数指针 DI，与 [ADR-0004](0004-static-dispatch-vs-runtime-ops.md) 静态分发冲突（R-005）；且未钉死 ISR 宿主落点、数值类型与 `pal_hwtimer`。
4. **Xtensa FPU**：ESP32(Xtensa) 中断上下文默认不保存 FPU；周期控制 ISR 内 float 会污染被抢占线程 FPU 状态——直接决定 BAL `control/` 数学 API 是定点还是 float（R-006 / C-003）。
5. **ESP-IDF IRAM / flash-cache**：`pal_hwtimer` 回调须 `IRAM_ATTR`，禁 flash 访问 / `pal_log` / malloc / 阻塞，否则 cache 失效期从 flash 取指会崩（R-008）。
6. **两类 ISR**：周期控制环（跑数学）与 nFAULT 保护（异步亚微秒关断）路径、优先级、栈与时延预算完全不同，不能混称一个 trampoline（R-007；保护路径对齐 [ADR-0024](0024-fault-three-phase-model-and-dal-deinit-contract.md)）。

---

## 方案比选（Options）

| 方案 | 结论 |
|------|------|
| A. **整包 DAL**：Clarke/Park/SVPWM + ISR + 硬件积木全进 DAL | ❌ DAL 臃肿；数学与引脚耦合；BAL 禁 pal / 同源仿真受损；与 ADR-0026 积木弹性目标冲突 |
| B. **整包 BAL**：快环数学 + ISR 宿主 + 定时器注册全进 BAL | ❌ 违反 BAL 禁 `pal_*`；ISR 平台相关代码污染公共头；与评审 P0-2 张力正相反 |
| **C. 积木 DAL + BAL 数学纯函数 + DAL/target ISR trampoline + 公共 `pal_hwtimer`** | ✅ **采纳**（B0 默认表；细化 ADR-0026 方案 B 的 ISR/DI/定时器边界） |

方案 C 将 FOC 切为三块：
- **BAL `control/`**：纯数学（Clarke/Park/SVPWM/电流环），无 `pal_*`
- **DAL 硬件积木**：PWM / 传感器 / 电流采样，无算法认知
- **DAL/target `foc_isr_trampoline`**：ISR 宿主（注册、进退中断、调用 BAL 纯函数、读写 DAL），**不进 BAL 公共头**

---

## 决策结论（Decision）

采纳 **方案 C**。以下为 B0 Owner 裁决表（2026-07-28 全采纳，无差异），**逐条锁定（与计划 B0 表原文一致）**：

| 议题 | 裁决 |
|------|------|
| FOC 数学（Clarke/Park/SVPWM/电流环） | BAL `control/` 纯函数，无 `pal_*` |
| 10kHz ISR 宿主 | DAL/target `foc_isr_trampoline`；**不进 BAL 公共头** |
| **ISR 数值类型 + FPU（R-006）** | 周期控制 ISR 优先**定点(Q15/Q31)**；若选 float，必须显式处理 Xtensa 中断 FPU 上下文（禁污染被抢占线程）。此裁决**锁定 BAL `control/` 数学 API 数值类型** |
| **两类 ISR 切分（R-007）** | **周期控制 ISR**（跑数学）与 **nFAULT 保护 ISR**（异步/亚微秒/绕软件层直接寄存器或硬件 BRK 关断，关联 ADR-0024）分列，各自定优先级/栈/时延预算 |
| 参数环（~50Hz） | BAL 非阻塞 API；共享缓冲与快环通信 |
| DI | Codegen **静态绑定**具体 `dal_*`；**禁止** ADR-0026 式运行期 fn 表作为主路径 |
| PAL 定时器 | **(a)** 公共契约 `pal_hwtimer_*` + PWM–ADC 硬件触发；禁止长期 target 野路子。**回调须 IRAM-safe / 禁 flash 访问（R-008）** |
| **仿真快环模型（R-009）** | 虚拟时间驱动的确定性步进（禁墙钟/rand）；PWM-ADC 同步在仿真端软步进降级 |
| **VESC/ODrive 外部驱动（scope，R/C-005）** | `dal_vesc`：**目录 `actuator/`**；实现为 CAN/UART 协议组帧，无主控 ISR / 无 `pal_hwtimer`；**不受 ADR-0047 / Wave B 门禁约束**，可独立计划先行 |

补充约束（不改变上表原文，写入 Consequences / C-002）：数值类型一经锁定禁止 target 分支各行其是；`pal_hwtimer` 回调另禁 `pal_log` / malloc / 阻塞（与 R-008 IRAM/禁 flash 同属施工红线）。

### C-002 补充（Owner 已确认）

ISR **允许**调用 BAL 纯快环函数，约束：

- 无阻塞、无 `pal_log`、有限栈
- 仅触碰显式共享状态（与慢环参数缓冲）
- 遵守本 ADR 锁定的数值类型（定点优先 / float 须 FPU 策略）

### 与 ADR-0026 的关系

| ADR-0026 保留 | 本 ADR supersede / 钉死 |
|---------------|-------------------------|
| DAL 硬件积木拆分（driver / sensor / current_sense） | ISR 宿主落点 = DAL/target trampoline，非「BAL 级别」模糊句 |
| BAL 纯算法控制器定位 | DI 主路径 = codegen 静态绑定；否决运行期 fn 表 |
| 设备树拓扑与 init 序 | `pal_hwtimer` 公共契约 + IRAM 回调 ABI |
| 高级硬件约束方向（PWM–ADC sync、nFAULT、扇区重构等） | 两类 ISR 分列；ISR 数值/FPU；仿真虚拟时间步进 |

---

## 后果与约束（Consequences & Constraints）

| 正面 | 负面 / 缓解 |
|------|-------------|
| 分层红线可审计：BAL 无 pal；ISR 宿主可测、可 IRAM | Wave C 须实现 `pal_hwtimer` + trampoline；B2 先回写活规范契约草案 |
| DI 与 ADR-0004 对齐；Codegen 生成静态调用，ISR 路径可内联 | 换传感器/驱动桥类型需 codegen 重绑定，不能运行期换 fn 表 |
| 定点优先降低 Xtensa FPU 事故面；API 类型一次锁定 | 若日后改 float，须全仓 BAL control + ISR FPU 策略同步变更 |
| 保护 ISR 与控制 ISR 预算分离，fail-safe 可论证 | 须在 DAL/PAL 规范分列注册入口与优先级（B2） |
| 仿真确定性步进支撑 CI 同源 | plant **不在** DAL `#ifdef SIMULATION`；收敛 `wink_sim_physical`（A3 已立规矩） |
| VESC 不被 Wave B 阻塞 | `dal_vesc` 独立计划、目录 `actuator/`；勿把 `pal_hwtimer` 强加到协议组帧实现 |

**施工红线（Wave C，本 ADR 定策）**：

- 未落地 `pal_hwtimer`（IRAM-safe 回调）前，禁止宣称 SimpleFOC 真机快环就绪
- 禁止在 BAL 公共头暴露 ISR 注册 / `pal_hwtimer` 符号
- 禁止以运行期 ops/fn 表作为 SimpleFOC DI 主路径
- 仿真快环禁止墙钟 / `rand` 驱动

---

## 遵循与后续（Compliance & Follow-up）

Accepted 后必须（由 Task B2 / Wave C）：

- [x] B2：回写 DAL §8.1（删「归入 BAL 级别」；改为算法 BAL + 积木 DAL + ISR trampoline）并链本 ADR
- [x] B2：回写 BAL——可被 ISR 调用的 `control/` 快环约束清单 + 数值类型锁定
- [x] B2：回写 PAL——`pal_hwtimer` / PWM–ADC sync **契约草案**（IRAM-safe ABI；两类 ISR 注册入口）
- [x] B1：修订 ADR-0026 状态与 §1 DI 示例（去运行期 fn 表）— 2026-07-28
- [ ] C2：`pal_hwtimer` 实现 + FOC 开工入口（触发项）
- [ ] C3：`dal_vesc` 独立计划（不受本 ADR 门禁）

---

*本 ADR 状态变更请在此记录：*
- 2026-07-28：Proposed（配合 dal-bal-followup Wave B；B0 裁决表锁定）
- 2026-07-28：Accepted（Owner 经 B0 确认默认裁决表全采纳；C-001～C-005 / C-002 已确认）
- 2026-07-28：B2 回写 DAL §8.1 / BAL §4.6 / PAL §2.2 活规范
- 2026-07-28：勘误——`dal_vesc` **目录**由 `comm/` 改为 **`actuator/`**（业务语义=运动执行；实现仍为协议组帧、无主控 ISR；与 followup 计划 v1.2.2 / C-005 对齐）

