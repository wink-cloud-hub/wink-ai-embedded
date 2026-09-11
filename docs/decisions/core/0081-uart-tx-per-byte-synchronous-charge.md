# ADR-0081：UART TX 按波特率整字节同步记账（`charge_us` 后置 TI）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-09-11）** |
| 日期 | 2026-09-11 |
| 触发 | GAP-02 修复项 3（deferred）、GAP-07（WDT）被其阻塞、阶段 3 Task 3 前置（`PLAN-20260911-STAGE3-MODEL-FIDELITY`） |
| 影响范围 | `wink-micro-os/frameworks/mcs51/src/mcs51_uart.cpp`（`on_sbuf_write`）、`wink_mcs51_clock.h`（`charge_us` 复用，不改语义）；Layer-① `02-wink-micro-os/07-mcs51-simulation-interception.md`（Accepted 后回写此时钟语义） |
| 决策者 | 项目 Owner（待确认） |
| **关联 ADR** | [ADR-0072](0072-dual-clock-domain-and-quota-catchup.md)（双时钟域/`charge_us`/ISR 内不 yield）、[ADR-0076](0076-mcs51-sim-backends-native-vs-iss-channel-roadmap.md)（Native 内不加异步定时器；本项 staying A 类的依据） |
| **关联实施计划** | `docs/implementation-plans/mcs51/2026-09-11-stage3-model-fidelity-plan.md`（Task 3；Task 4 依赖本 ADR 落地） |
| **关联设计规范** | `docs/design/02-wink-micro-os/07-mcs51-simulation-interception.md`（回写目标）、保真度文档 §3.1（异步定时器禁令——本决策的合规边界） |

---

## 1. 背景（Context）

`on_sbuf_write`（`mcs51_uart.cpp:218`）今日语义：A-01 就绪门控通过后，**无条件出总线 + 同一调用栈立即置 TI**。字节在虚拟时间上花费 0µs（仅 1 个微步副作用）。后果：health_pot 22 字节遥测帧在仿真中约 110µs（22 微步），硅片 9600bps 下约 23ms——差两个数量级。直接后果是 GAP-07（WDT）无法验证：不先让 TX 占据真实虚拟时间，WDT 溢出检查永远够不着"遥测阻塞 → 复位"（分类文档 A-04 条目）。

约束有三，全部来自已采纳决策：

1. **保真度 §3.1 / ADR-0076**：Native 后端禁止加异步定时器。`while(!TI);` 忙等体内即使有 `_nop_`，一个"未来某虚拟时刻才置 TI"的异步事件也构成纯内存轮询 + 异线程推进时间的死锁原型（B-01）。任何把 TI 推迟到**另一个调度点**的方案都出 A 类。
2. **ADR-0072**：虚拟时间只能经拦截点 `charge_us` 推进；ISR 上下文只记账不 yield；配额片（10ms）耗尽即协作 yield + catch-up。
3. **RX 侧已有先例**：`RX_BYTE_SPACING_US=1000` 按字节起搏递送（`rx_deliver_one`），证明"字节级时间 pacing"在现有引擎可表达——TX 应对称。

原厂波特率事实（StdDriver `uart.c:109-143` `UART_ConfigBaudRate`，`reload = N − Fsys·SMOD/(32·K·T·Baud)`，求逆即得波特率；`SMOD_Flag` 1/2 由 PCON.SMOD0 决定）：

| 源 | N | K | T | 重载寄存器 |
|---|---|---|---|---|
| TMR1 | 256 | 4 | T1M?1:3（即 Fsys/4 vs Fsys/12） | TH1 |
| TMR4 | 256 | 4 | T4M?1:3 | 8 位自动重载寄存器（实现时按手册确认地址） |
| TMR2 | 65536 | 12 | T2PS?2:1（即 Fsys/24 vs Fsys/12） | RCAP2H/L（16 位） |
| BRT | 65536 | 1<<BRTCKDIV | BRTCKDIV（BRTCON bits2:0） | BRTDH/L（`BRTDL@0xF5C1`，高字节按手册确认） |

校准锚点：health_pot（Fsys=24MHz、SMOD=2、T=1、TH1=217）→ `24M·2/(128·1·39) = 9615bps`，与 DESIGN.md 记载一致。

## 2. 方案比选（Options）

| 方案 | 描述 | 优 | 劣 | 结论 |
|---|---|---|---|---|
| A. 维持现状（0 耗时置 TI） | 不做记账 | 零风险、场景时间不变 | WDT/调度风险永久隐身；GAP-07 无前置，阶段 3 目标落空 | ❌ |
| B. 异步延迟置 TI（事件队列：字节先出总线，TI 在未来虚拟时刻由 timer 事件置位） | 真实"发送中"状态机 | SBUF 重写（GAP-25 SBUF 子项）可建模"发送中二次写损坏" | 违反 §3.1：`while(!TI);` 在 TI 到来前是纯内存轮询，异步推进即死锁原型；引入第二时钟事件源，属 B 类手段；RX 侧已证明同步 pacing 够用 | ❌（出 A 类，否决） |
| C. **同步记账后置 TI（本 ADR）** | `on_sbuf_write` 内按当前波特率算出整字节 `byte_us`，`charge_us(byte_us)` 后再出总线/置 TI/raise IRQ；TI 仍在同一调用栈同步置位 | 不加任何异步机制，`while(!TI)` 永可终止（TI 置位不依赖未来事件）；仍是 hook 内同步记账 + 同步完成，符合分类文档 §0 的 A 类定义；天然解锁 GAP-07 | 长遥测帧使场景虚拟时间膨胀（23ms 量级，预期内）；配额片内多次 yield（见 D6，属预期行为） | ✅ **采纳** |

## 3. 决策结论（Decision）

### D1. 机制：整字节同步记账，TI 仍同步置位

`on_sbuf_write` 内操作顺序固定为：① A-01 就绪门控（已落地，不动）→ ② 按 D2 算 `byte_us` 并 `charge_us` → ③ 出总线（`putchar`/capture/`js_pal_uart_write`，顺序不变）→ ④ 置 TI + `raise_irq`。**TI 置位点仍在触发写操作的同一调用栈**，只是 `virtual_us` 已前进。这是本决策 staying A 类的 load-bearing 点：忙等语义从"零时间成功"变为"耗时但成功"，永不变为"等待未来事件"。

### D2. 波特率公式（原厂公式求逆，Fsys 取 `wink_mcs51_get_clock_hz()`）

`Baud = Fsys·SMOD/(32·K·T·(N − reload))`，K/T/N/reload 按 §1 表格四源枚举；`SMOD = PCON.SMOD0 ? 2 : 1`。经典家族只有 TMR1 源且无 T1M（按 T=3 即 Fsys/12）。任一源就绪（A-01 谓词）但**速率算不出**（如重载寄存器地址手册未确认、保留 CKS）时：STRICT 中止，Release 按未就绪计——**永不假装一个编出来的速率**（A-01"不静默假装成功"规则的延伸）。

### D3. 帧位宽：模式 1 计 10 位，模式 3 计 11 位

`byte_us = frame_bits·1e6/Baud`。模式 2（SCON SM1=0）仍在 A-01 MODE-unready 桶，不在本决策覆盖（未来工作，不 silencely 计费）。

### D4. 未就绪时不记账

A-01 掩码非零时：Release 走原 warn-once + 计数路径，字节即时发出且**不 charge**（时序保持现状，错误已被计数，GAP-10 判决生效）；STRICT 在记账前中止。记账只发生在"配置正确"的字节上——WDT 验证的正是正确配置下的阻塞预算。

### D5. 与配额/yield/ISR 的交互（ADR-0072 复用，无新语义）

- 主循环长帧（如 22B≈23ms）跨约 2 个配额片：每次片耗尽协作 yield + 定时器 catch-up——**预期且正确**：硅片上 TX 忙等本就不阻塞 Timer0 ISR，中途 10ms tick 到达与真机一致。
- ISR 内写 SBUF（如 TX 链式发送）：`charge_us` 只推进 `virtual_us` 不 yield（时钟既有语义），定时器在恢复后 catch-up。嵌套记账累加，正确。
- RX 路径不动（已有 1ms 起搏，与 9600bps 字节时间同量级，自洽）。

### D6. 范围外（明确不做）

SBUF 发送中二次写损坏（GAP-25 子项，需 B 方案的状态机，留 ISS/未来）；模式 2 计费；DR/LEDSDR 强度（跨仓契约事项）；波特率误差告警（速率精度是 B-03 范畴，Native 只保证量级正确）。

## 4. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| TX 阻塞在虚拟时间上现形（health_pot 遥测 110µs → ~23ms），GAP-07 WDT 验证被解锁（Task 4 前置满足） | 所有 UART 场景虚拟时间膨胀约 `bytes·byte_us`；headless 场景超时阈值如按 wall-clock 设需复核（按虚拟时间设则无碍） |
| 计费公式与原厂 `UART_ConfigBaudRate` 同源（求逆），health_pot 9615bps 锚点可回归钉防 | 四源每源需一个锚点单测（TMR1=health_pot；BRT/TMR2/TMR4 需按手册重载寄存器补锚点，实现时若手册字段不明则该源保持 STRICT 中止） |
| 零新调度机制：无异步事件、无 §3.1 违规风险 | SBUF 重写损坏仍不可见（D6，已显式声明，不算回归） |
| AI 生成代码的 `while(!TI)` 忙等在仿真中消耗与真机同量级时间，调度类 bug 可被 WDT 抓住 | 应用 DESIGN.md 必须写入"最长阻塞段（含帧长/波特率）< WTS 间隔"硬约束（GAP-07 第二轮补充，Task 4 落） |

## 5. 遵循与后续（Compliance & Follow-up）

- [ ] 本 ADR Accepted 后：回写 Layer-① `02-wink-micro-os/07-mcs51-simulation-interception.md`（UART 时钟语义段：整字节同步记账 + 四源公式 + D5 交互）。
- [ ] 阶段 3 Task 3 按 D1–D5 实现 + 四源锚点单测 + health_pot 遥测回归（帧内容不变、虚拟时间 ~23ms）。
- [ ] Task 4（WDT）在 Task 3 合入后解锁。
- [ ] 红线手册 §4.6 追加"UART TX 按波特率占虚拟时间"条目。

---

*本 ADR 状态变更请在此记录：*
- 2026-09-11：Proposed（阶段 3 Task 3 前置，待 Owner 确认）
- 2026-09-11：Accepted（Owner 确认，执行回写 + Task 3 实现）
