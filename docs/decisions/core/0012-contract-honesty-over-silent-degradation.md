# ADR-0012：契约诚实优于静默降级（PAL/HAL 抽象层通用原则）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-06-30（提议）／ 2026-07-02（采纳） |
| 触发 | [2026-06-30 PAL 中断子系统架构评审](../../reviews/core/2026-06-30-pal-interrupt-subsystem-architecture-review.md) §4 G1~G3 列出三处"头文件承诺与各 target 实现不一致"的契约欠债 |
| 影响范围 | `pal/include/*.h`、`pal/include/hal/*.h` 全部 PAL/HAL 公开接口；所有 `targets/{esp32,wasm,host,baremetal}/` 实现；docgen 与 codegen 对 PAL 行为的假设 |
| 决策者 | 架构委员会 & 用户 |
| 关联评审 | [2026-06-30-pal-interrupt-subsystem-architecture-review](../../reviews/core/2026-06-30-pal-interrupt-subsystem-architecture-review.md) |
| 关联实施计划 | [2026-06-30-pal-interrupt-phase1-contract-alignment-plan](../../implementation-plans/core/2026-06-30-pal-interrupt-phase1-contract-alignment-plan.md) |
| 关联设计规范 | `02-wink-micro-os/02-pal-platform-abstraction.md`（已同步本原则）<br>`docs/tech-designs/core/pal-unified-interrupt-subsystem.md` v2.1（已加 ADR-IRQ-008 局部决策映射） |

---

## 背景（Context）

WinkMicroOS 的核心架构价值是**双 target 同源编译 + 行为级保真仿真**（参考 [ADR-0002](../unisim/0002-dual-target-compilation.md)、[ADR-0003](../unisim/0003-simulation-fidelity-boundary.md)）。这要求 PAL/HAL 抽象层在所有 target 上提供**行为一致**的 API；当某 target 因硬件或 RTOS 限制无法兑现某项行为时，抽象层有两种应对方式：

| 应对方式 | 优劣 |
|---------|------|
| **A. 静默降级**（silent fallback） | 调用方不察觉行为差异；跨 target 移植时表现良好；但**掩盖跨平台 bug** —— 某些场景下"在 X 平台跑得过"的代码到 Y 平台才暴露问题，调试成本极高。 |
| **B. 显式拒接 或 头文件契约下调** | 调用方第一时间看到差异（返回 `WINK_ERR_UNSUPPORTED` 或读到 doxygen 注释）；**强迫问题前移**到 API 设计阶段而非生产环境。 |

### 当前 PAL 中断子系统积累的契约欠债（评审 §4）

2026-06-30 的 PAL 中断子系统评审列出三处实例：

1. **G1**：`pal_irq_direct_connect` 头文件承诺"直连中断完全绕过 PAL 软件分发逻辑，由硬件矢量控制器直接跳转"，但 esp32/wasm/host 三个 target 全部退化为 `pal_irq_enable((pal_isr_t)handler, NULL)`，走 dispatch wrapper。同时 `(pal_isr_t)handler` 把 `void(*)(void)` 强转 `void(*)(void*)`，是 CFI/UBSan 违例。
2. **G2**：ESP32 `pal_irq_enable(prio=REALTIME, ...)` 静默映射到 `ESP_INTR_FLAG_LEVEL3`，与 `HIGHEST` 物理优先级完全等价，但契约相反："非 RTOS 安全 vs RTOS 安全"。用户写 `REALTIME` ISR + `xQueueSendFromISR` 在 ESP32 上不会 crash（仍落在 syscall 边界内），换 STM32 真挂 NMI 后翻车。
3. **G3**：`pal_gpio_enable_interrupt_ex(prio)` 三个 target 全部 `(void)prio` 静默丢弃，但头文件称"支持指定优先级"。当 codegen 期望按钮中断高于传感器中断时，**抽象在所有 target 上都失效**。

三处的共性都是：**头文件做出了实现没有兑现的承诺**。这违反 [ADR-0003](../unisim/0003-simulation-fidelity-boundary.md) 的"仿真可信度边界声明"精神 —— 边界本身要先被诚实地标注出来，使用者才能判断风险。

---

## 方案比选（Options）

### 选项 A：维持静默降级（现状）

- ✅ 优点：无需修改任何调用方代码；diff 小。
- ❌ 缺点：跨平台 bug 隐式累积；codegen 一旦把当前形态写进数百份生成代码，再纠正契约就要写"deprecation + 迁移期"长尾路径。
- ❌ 缺点：违反 PAL 抽象的核心价值（行为级保真）。

### 选项 B：每个 target 真实现承诺的行为

- ✅ 优点：契约完全兑现，调用方无须感知差异。
- ❌ 缺点：成本巨大。G1 需要在 ESP32 走 `esp_intr_alloc(..., ESP_INTR_FLAG_HIGH | ESP_INTR_FLAG_IRAM, ...)` 真直派；G3 需要为每个 pin 单独 `esp_intr_alloc` 注册成独立 source —— 大部分 codegen 场景并不依赖此细粒度，开发成本远大于收益。
- ❌ 缺点：某些承诺**物理上无法兑现**（如 ESP32 NMI 级 C-ISR），强行实现会引入更深的 bug。

### 选项 C：显式拒接 + 头文件契约下调（推荐）

- ✅ 优点：成本最低；问题前移；保留未来用新接口（如 `pal_irq_direct_connect_unsafe()`、`pal_gpio_enable_interrupt_dedicated()`）补能力的空间。
- ✅ 优点：调用方调用某 API 时，要么得到正确行为，要么收到清晰的 `WINK_ERR_UNSUPPORTED`，永远不会拿到一个"看起来 OK 但语义偷换"的结果。
- ⚠️ 代价：少数已经依赖静默降级的调用代码需要显式处理 `WINK_ERR_UNSUPPORTED`（但通过当日 grep 验证：本项目目前没有任何此类调用方）。

---

## 决策结论（Decision）

**采纳选项 C**：将"契约诚实优于静默降级"提升为 PAL/HAL 抽象层的通用工程原则，所有后续接口设计与维护必须遵守。

### 落地规则

1. **新增 PAL/HAL 接口** 时，对每个 target 评估能否兑现承诺：
   - 全部能兑现 → 头文件正常约定行为；
   - 部分 target 不能兑现 → 写明"此 target 上返回 `WINK_ERR_UNSUPPORTED`"，**禁止静默降级**；
   - 全部 target 都不能在当前 phase 兑现 → 把承诺写成 "v1.0 阶段实现为 X，未来 v2.0 升级为 Y"，让读者立刻看到当前真相。

2. **修订已有 PAL/HAL 接口** 时，如发现头文件承诺与实现不一致：
   - 优先级 1：能用低成本真实现 → 实现之；
   - 优先级 2：成本过高 / 物理不可达 → 修订头文件 doxygen，把契约下调到实现位置，并在注释里挂未来新接口的迁移路径；
   - 优先级 3：用错误码（`WINK_ERR_UNSUPPORTED`）拒接超出能力的调用，配合头文件说明何时返回此错误码。

3. **跨 target 行为差异** 必须显式：
   - 仿真 target（host/wasm）若行为与真机 target（esp32/STM32）不一致 → 头文件 doxygen 写明"X target 上 Y / Z target 上 W"，让读者一眼看到差异。
   - 不允许在仿真 target 上"看起来通过"，但真机上 fail 的情况（除非该 fail 已在头文件契约里被列为 expected）。

4. **示例：本次 PAL 中断子系统 v2.1 修订**（参考 [2026-06-30-pal-interrupt-phase1-contract-alignment-plan](../../implementation-plans/core/2026-06-30-pal-interrupt-phase1-contract-alignment-plan.md)）
   - G1：头文件诚实下调 + trampoline 消除 cast；未来真直派走新接口。
   - G2：ESP32 显式拒接 REALTIME；host/wasm 单线程下接受，但头文件注明。
   - G3：头文件 doc-only 诚实化，明示 prio 当前被所有 target 忽略；未来 per-pin prio 走新接口。

---

## 后果与约束（Consequences & Constraints）

### 正面后果

- ✅ 跨平台 bug 强迫在 PAL 抽象层显形，而不是在业务代码里以"X 平台 Y 模型偶发崩溃"的方式出现。
- ✅ codegen 生成代码时可以直接读 PAL 头文件确定每条 API 在每个 target 上的行为，无需翻 issue 历史。
- ✅ 评审/审计时只需扫一遍头文件就能判断"哪些承诺被兑现，哪些没有"。

### 负面后果 / 约束

- ⚠️ 每次新增 PAL/HAL 接口都要做"行为差异矩阵"评估（哪 target 能兑现、哪 target 拒接），开发节奏略变慢。
- ⚠️ 已经 ship 的调用方代码若靠静默降级"碰巧能跑"，需要显式处理新错误码 —— 但本次（2026-06-30）grep 确认无此类外部调用方。

### Code Generation 指南

codegen 在生成 PAL 调用代码时：

```c
/* ✅ 推荐写法：检查返回码，遇到 UNSUPPORTED 走 fallback 或汇报 */
wink_status_t st = pal_irq_enable(irq_num, prio, isr, arg);
if (st == WINK_ERR_UNSUPPORTED) {
    /* 当前 target 不支持此 prio，降级到 HIGHEST 或上报 */
    st = pal_irq_enable(irq_num, PAL_IRQ_PRIO_HIGHEST, isr, arg);
}

/* ❌ 禁止写法：盲信调用成功，忽略 UNSUPPORTED 可能 */
pal_irq_enable(irq_num, PAL_IRQ_PRIO_REALTIME, isr, arg);  /* WINK_WARN_UNUSED_RESULT 会编译警告 */
```

---

## 遵循与后续（Compliance & Follow-up）

### Accepted 后立即执行

1. ✅ 把本原则回写至 `02-wink-micro-os/02-pal-platform-abstraction.md` 设计规范，作为 PAL 设计的基线条款。
2. ✅ 在 `.claude/skills/embedded-best-practice/` 中加入 reference 条目（"contract honesty over silent degradation"）。
3. ✅ 本 ADR 当前关联的具体落地（G1/G2/G3）已在 [2026-06-30-pal-interrupt-phase1-contract-alignment-plan](../../implementation-plans/core/2026-06-30-pal-interrupt-phase1-contract-alignment-plan.md) 中执行。

### 跨子系统应用

后续涉及 PAL/HAL 的 ADR / 实施计划应在"决策结论"或"约束"章节回引本 ADR-0012，确保新增 API 默认遵循"诚实优于降级"原则。

### 与 ADR-IRQ-008 的关系

ADR-IRQ-008（在 `docs/tech-designs/core/pal-unified-interrupt-subsystem.md` v2.1 §11 中）是本 ADR-0012 在中断子系统的局部映射。本 ADR-0012 是项目级原则，ADR-IRQ-008 是子系统级具体落地。

---

*本 ADR 状态变更请在此记录：*
- 2026-06-30：Proposed（伴随 PAL 中断子系统 Phase 1 契约对齐实施计划提出）
- 2026-07-01：Phase 1.5 落地 —— G3 首次锁定语义 + host/wasm REALTIME 一致化实现完成（见 [2026-07-01-pal-interrupt-phase1p5-gpio-prio-enforcement-plan](../../implementation-plans/core/2026-07-01-pal-interrupt-phase1p5-gpio-prio-enforcement-plan.md)）。三 target 上 `pal_gpio_enable_interrupt_ex(prio)` 与 `pal_irq_enable(REALTIME)` 契约与实现现已完全对齐；新增 ADR-IRQ-009 记录 GPIO 服务永不释放的具体设计选择。
- 2026-07-02：**Accepted（架构委员会 & 用户）**。触发采纳的证据链齐备：
  1. **G1/G2/G3 三处契约欠债已完整清偿**——Phase 1（2026-06-30）+ Phase 1.5（2026-07-01）实施完成，三 target 契约与实现对齐。
  2. **ADR-0018 已借本原则完成 PAL IRQ 公开面收窄**——诚实拒接 `direct_connect` / `shared_register` / `REALTIME/HIGHEST/LOWEST` 等无法兑现的承诺，加 `#error` 门控隔离 STRICT 变体（见 [ADR-0018](0018-pal-irq-api-narrowing.md)）。
  3. **本原则已在 5 个后续 ADR（0013/0015/0016/0018）与 3 篇设计规范中作为决策依据被引用**——已成为项目通用原则的事实基线。
  4. **回写工作已完成**：`02-pal-platform-abstraction.md` §1.1 已把 ADR-0012 纳入核心架构决策摘要；`.claude/skills/embedded-best-practice/references/index.md` 已加入 skill 引用条目。

