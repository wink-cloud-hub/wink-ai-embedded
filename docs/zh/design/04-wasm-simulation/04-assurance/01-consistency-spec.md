# 仿真一致性与高保真规范（C1~C25）

| 项 | 内容 |
|---|---|
| 文档层级 | ① 设计规范（UniSim 3.0 / assurance） |
| 文档状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| SSOT 职责 | 场景**原理 / 方案 / 预言 / 边界**；**不存**可测性状态矩阵 |
| 关联代码 | 见各子场景保障方案所链 mechanisms；测试入口 `wink-tools/wink.py` |
| 上次核对 | 2026-08-02 |
| 管辖 ADR | 0001、0002、0003、0009、0013、0014、0019、0025、0040、0042、0045、0047、0053、0054、0055（迁入场景时可按子项收窄） |
| 迁自 | `04-wasm-simulation-2.0/11-consistency-spec.md` |

> **SSOT 分工**：读本文件了解场景契约；查「现在能不能验」读 [`02-consistency-checklist.md`](./02-consistency-checklist.md)。两处不得双写技术方案正文。
>
> 下列 §0 为**已冻结结构**（字段名与 oracle 词汇）；§0.4 TOC 已于 Wave 4A 填满；§1–§3 场景正文已于 **Wave 4B** 迁入。可测状态仍只在 checklist。

---

## 0. 总则（结构冻结）

### 0.1 子场景文档模板（五字段）

每个子场景必须写齐下列五字段；历史子项若缺某一字段，在补齐前显式标注「待补」。

| 字段 | 含义 |
|---|---|
| **问题** | 真机常见什么坑；何种 App/驱动写法触发 |
| **真机 vs 仿真** | 差异机制（为何仿真会漏或会骗） |
| **保障方案** | A 约束写法 / B 引擎建模 / C 观测门禁 / 真机兜底；关键实现要点 |
| **验收预言** | 断言什么算「验到了」（可测 oracle；词汇见 §0.2） |
| **边界** | 刻意不保或只能近似的部分 |

**字段豁免（🚫 / 真机·HIL 独占）**：若该子项在 [`02`](./02-consistency-checklist.md) 主状态为 **🚫**，或验收预言明确为「真机 / HIL 独占」且产品默认不建模，则 **问题** / **真机 vs 仿真** 可省略；仍须写齐 **保障方案** / **验收预言** / **边界**。不得用豁免逃避「刻意不保」的边界声明。

### 0.2 验收预言（Oracle）标准词汇

| 词汇 | 约定含义 | 典型断言机制 |
|---|---|---|
| **Fail-Fast / Fail-Loud** | 编译期或内核初始化期直接阻断并明确报错 | `-DWINK_STRICT_NONBLOCKING=1` 链接失败、JSON 门禁卡死 |
| **Fault** | 运行时触发内核故障隔离区并捕获上下文 | 堆配额 OOM、软 WDT 超时、影子内存 TSan 断言 |
| **Bit-Identical / Golden Trace** | 运行轨迹或输出与参考 100% 逐 bit/逐 μs 一致 | 同源状态机 Trace、固定 PRNG 输出 |
| **Tolerance Band** | 在预设物理/算法公差带内验收 | RC 拟合、退化曲线比对 |
| **Error-Code Parity** | 同类故障下仿真与真机路径返回的 `wink_status_t`（ADR-0001）一致 | 故障注入对照表 + 每码单测；禁止仿/真各返回不同负码 |
| **真机 / HIL 独占** | 仿真标记不可测，交真机/HIL | 硬实时 ISR、SPICE、多核 cache 一致性 |

### 0.3 解法类型与三道防线（保障方案用词）

| 架构层 | 解法类型 | 职责 |
|---|---|---|
| 第一道：静态阻断 | **A 约束写法** | 编译/IDE 阻断危险写法 |
| 仿真核心底座 | **B 引擎建模** | Wasm/Host 复刻时间/总线/调度等 |
| 第二道：动态防护 | **C 观测门禁** | 运行时捕获并发/溢出/耗尽/超时 |
| 第三道：真机兜底 | **真机/HIL** | 刻意不建模的物理/硬实时/微架构 |

### 0.4 C 场景索引 TOC

> 标题取自 2.0 `11` §0.4。主属轴仅标一个；机制链接为 3.0 路径。checklist 锚点已由 Wave 4C 兑现。**本表不写**场景可测状态符。

| 编号 | 标题 | 主属轴 | 机制链接 | checklist 锚点 |
|---|---|---|---|---|
| C1 | 业务因果/状态机 | A | [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) | [`./02-consistency-checklist.md#c1`](./02-consistency-checklist.md#c1) |
| C2 | 虚拟微秒逻辑时序 | B | [`../02-mechanisms/02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) | [`./02-consistency-checklist.md#c2`](./02-consistency-checklist.md#c2) |
| C3 | 共享状态竞态 | E | [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) | [`./02-consistency-checklist.md#c3`](./02-consistency-checklist.md#c3) |
| C4 | 临界区与中断抢占/嵌套 | D | [`../02-mechanisms/04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md) | [`./02-consistency-checklist.md#c4`](./02-consistency-checklist.md#c4) |
| C5 | 阻塞/饿死/看门狗 | E | [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) | [`./02-consistency-checklist.md#c5`](./02-consistency-checklist.md#c5) |
| C6 | 栈/堆/内存安全 | F | [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) | [`./02-consistency-checklist.md#c6`](./02-consistency-checklist.md#c6) |
| C7 | 总线协议/CRC/错误恢复 | A | [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md)、[`../02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md) | [`./02-consistency-checklist.md#c7`](./02-consistency-checklist.md#c7) |
| C8 | DMA/总线异步传输窗口 | A | [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) | [`./02-consistency-checklist.md#c8`](./02-consistency-checklist.md#c8) |
| C9 | 多核 SMP 真实并发 | E | [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) | [`./02-consistency-checklist.md#c9`](./02-consistency-checklist.md#c9) |
| C10 | 快环 ISR（FOC/硬定时器） | C | [`../02-mechanisms/09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md) | [`./02-consistency-checklist.md#c10`](./02-consistency-checklist.md#c10) |
| C11 | 电气/模拟电路 | F | [`../02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md)、[`../01-overview/03-production-contract.md`](../01-overview/03-production-contract.md) | [`./02-consistency-checklist.md#c11`](./02-consistency-checklist.md#c11) |
| C12 | CPU/ABI 指令级 | F | [`../02-mechanisms/10-wasm-js-bridge-abi.md`](../02-mechanisms/10-wasm-js-bridge-abi.md) | [`./02-consistency-checklist.md#c12`](./02-consistency-checklist.md#c12) |
| C13 | 生命周期/复位/冷热启动 | F | [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) | [`./02-consistency-checklist.md#c13`](./02-consistency-checklist.md#c13) |
| C14 | 快进/联合仿真步进契约 | B | [`../02-mechanisms/02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) | [`./02-consistency-checklist.md#c14`](./02-consistency-checklist.md#c14) |
| C15 | Host↔Wasm 边界诚实性 | D | [`../02-mechanisms/04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md)、[`../02-mechanisms/01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md)、[`../02-mechanisms/10-wasm-js-bridge-abi.md`](../02-mechanisms/10-wasm-js-bridge-abi.md) | [`./02-consistency-checklist.md#c15`](./02-consistency-checklist.md#c15) |
| C16 | OS 同步原语语义对齐 | E | [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) | [`./02-consistency-checklist.md#c16`](./02-consistency-checklist.md#c16) |
| C17 | 外设资源互斥/时基耦合 | C | [`../02-mechanisms/09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md)、[`../02-mechanisms/07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md) | [`./02-consistency-checklist.md#c17`](./02-consistency-checklist.md#c17) |
| C18 | 总线故障态机（超越 CRC） | A | [`../02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md)、[`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) | [`./02-consistency-checklist.md#c18`](./02-consistency-checklist.md#c18) |
| C19 | DMA/缓冲生命周期 | A | [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) | [`./02-consistency-checklist.md#c19`](./02-consistency-checklist.md#c19) |
| C20 | 回调重入/延迟下半部 | D | [`../02-mechanisms/04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md) | [`./02-consistency-checklist.md#c20`](./02-consistency-checklist.md#c20) |
| C21 | 时间与计数回绕 | B | [`../02-mechanisms/02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) | [`./02-consistency-checklist.md#c21`](./02-consistency-checklist.md#c21) |
| C22 | 电源/低功耗/时钟域 | F | [`../02-mechanisms/07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md)、[`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) | [`./02-consistency-checklist.md#c22`](./02-consistency-checklist.md#c22) |
| C23 | 持久化/NVS/磨损 | F | [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) | [`./02-consistency-checklist.md#c23`](./02-consistency-checklist.md#c23) |
| C24 | 缓存/内存属性/DMA RAM | F | [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) | [`./02-consistency-checklist.md#c24`](./02-consistency-checklist.md#c24) |
| C25 | 浮点/数值与编译器 UB | F | [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md)、[`../02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md) | [`./02-consistency-checklist.md#c25`](./02-consistency-checklist.md#c25) |

---

> 生产口径（A~F 完备 ≠ 虚实恒等）唯一措辞见 [`../01-overview/03-production-contract.md`](../01-overview/03-production-contract.md)；本文件不另写一套承诺。

## 1. 仿真一致性底层原理

### 1.1 核心基线：虚拟微秒时钟 SSOT

`s_virtual_us`（uint64 单调）是唯一时钟 SSOT，放弃墙钟（机制见 [03](../02-mechanisms/02-virtual-clock.md)）：

- **零耗时快进**：HEADLESS 下所有 Fiber sleep/等待时，`pal_sim_scheduler_run` 把时钟快进到最近 `next_wakeup_us`；
- **物理逻辑绑定**：`wink_phys_debounce_step`/`wink_phys_rc_lowpass`/软定时器全部锚定虚拟时钟；
- **SSOT 红线**：`pal_delay_ms/us` 禁止主动步进；唯一赋值点是静态 `wink_vclock_advance_internal()`，合法调用者为 JS 导出 `pal_wasm_advance_virtual_clock` 与 HEADLESS 内部跳跃（ADR-0042 单 Gate）。双重步进是 C14 级逃逸。

### 1.2 控制域与物理域解耦（联合仿真）

三层：App 控制域（100% C 同源）↔ 平台仿真底座（时钟/调度/虚拟引脚/IRQ Poll/总线/配额）↔ 插件（运动学/传感器退化）。数据面导出/注入 API 见 [01 §2.1](../01-overview/01-architecture.md)。

**Step-Lock Pipe**：每步 (1) 插件读控制信号；(2) 按绑定虚拟时钟的 Δt 更新物理；(3) 注入 API 写回。锁步破坏（plant 用墙钟/不同 Δt）归 C14。内核保持零业务物理痕迹。

### 1.3 零 Yield 同步事件驱动快进

朴素 `pal_gpio_pulse_in` 上 Asyncify yield 会因 Unwind/Rewind 慢 10~50×。机制：(1) Pin Event Queue（C 侧未来引脚变化时间链表）；(2) Trig 跃变时同步回调插件写 Echo 边沿时间戳；(3) `pulse_in` 直接累加 `s_virtual_us` 返回脉宽，**Asyncify 次数为 0**。跳跃可能漏中间边沿/半窗 debounce——契约归 C14.2。

---

## 2. 场景化一致性保障（C1~C25）

<a id="c1"></a>

### C1 — 业务因果与状态机逻辑

**目标**：App/BAL 状态机、传感器语义、执行器命令、故障/超时路径在仿真与真机因果一致。

<a id="c1.1"></a>

#### C1.1 同源 App/BAL 状态迁移
- **问题**：仿真跑另一套"简化逻辑"，状态机"绿"但真机分支不同。
- **真机 vs 仿真**：真机与仿真必须共用同一 C 源（ADR-0002）。
- **保障方案**：**B** 双 target 同源编译；禁止 App 层 `#ifdef SIMULATION` 改业务分支。
- **验收预言**：同输入序列下状态迁移轨迹（或 golden trace）bit-identical / 语义等价。
- **边界**：DAL/PAL 旁路导致的细节差异归 C1.2/C7。

<a id="c1.2"></a>

#### C1.2 DAL Bypass / `#ifdef SIMULATION` 收窄
- **问题**：整驱动被 JS 替换 → CRC/超时/重试从未在仿真跑过（ADR-0003 决策 2）。
- **真机 vs 仿真**：真机跑完整驱动；仿真整层 bypass 则协议路径逃逸。
- **保障方案**：**A+B** 仅替换物理量来源，旁路落 PAL（DAL 目标零仿真宏）；ADR-0040 JSON 门禁 Fail-Loud；持续审计残留 bypass（见 [09](../02-mechanisms/08-channel-routing.md)）。
- **验收预言**：未在 `wink-app.json` 声明的语义 bypass 编译/运行期失败；声明器件的协议层仍走 C 路径。
- **边界**：物理量本身仍来自插件/注入，不宣称电气等价。

<a id="c1.3"></a>

#### C1.3 故障/超时/断线异常路径
- **问题**：只测 happy path，真机断线/超时状态机未覆盖。
- **真机 vs 仿真**：真机偶发；仿真可用确定性故障注入复现。
- **保障方案**：**B** 退化引擎 / 总线 `drop_permil` / 超时注入（见 [07](../02-mechanisms/06-physical-degradation.md)）。
- **验收预言**：注入断线/超时后进入约定故障态，恢复序列可复现。
- **边界**：电气层断线阻抗变化归 C11（电气非目标）。

<a id="c1.4"></a>

#### C1.4 幂等恢复与重试风暴
- **问题**：错误恢复路径重复入队/重复下发执行器命令。
- **真机 vs 仿真**：协作调度下时序固定，可能掩盖"重试窗口重叠"。
- **保障方案**：**B+C** 故障注入拉长 ACK 超时；观测命令计数/序列号单调性。
- **验收预言**：单次故障注入下执行器命令次数 ≤ 规约上限；无未界定双发。
- **边界**：网络层 exactly-once 不在本 OS 范围。

<a id="c1.5"></a>

#### C1.5 `wink_status_t` 错误码跨 target 对齐
- **问题**：仿真返回一个负码、真机同类故障返回另一个 → App 错误分支在仿真「绿」、板上走错路径（ADR-0001）。
- **真机 vs 仿真**：PAL/DAL 适配层易各写一套映射；OOM 等已在 C6.1 点名，缺横切契约。
- **保障方案**：**A+C** 维护「故障类 → `wink_status_t`」对照表（超时 / busy / invalid arg / NACK / NO_MEM 等）；仿真注入与 ESP-IDF 路径单测同表；禁止私自发明正数或平台特码冒充统一语义。
- **验收预言**：**Error-Code Parity** — 对照表内每码有测试；仿/真同故障类返回同一负码（或文档化的等价别名集）。
- **边界**：ESP-IDF 原生气码若未映射进 `wink_status_t`，不得出现在 App/BAL 对外契约；对照表增量随器件扩展。

---

<a id="c2"></a>

### C2 — 虚拟微秒逻辑时序（单任务/单中断友好）

**目标**：单任务或"中断不与计算重叠"场景下，sleep/脉宽/去抖/采样周期逻辑时间确定性一致。多消费者时钟与快进副作用见 C14/C21。

<a id="c2.1"></a>

#### C2.1 sleep/定时唤醒快进
- **问题**：依赖墙钟导致后台 Tab 节流、CI 抖动。
- **真机 vs 仿真**：真机墙钟近似；仿真必须虚拟时间。
- **保障方案**：**B** `s_virtual_us` + 调度器快进到 `next_wakeup_us`。
- **验收预言**：同 seed/同注册序下唤醒时刻序列可复现；墙钟耗时 ≪ 虚拟跨度。
- **边界**：当前不仿真晶振/时钟源漂移（非目标；见 [`06` §1](../02-mechanisms/06-physical-degradation.md)）。

<a id="c2.2"></a>

#### C2.2 脉宽测量零 Yield 环回
- **问题**：`pulse_in` Yield 导致性能崩溃或时序漂移。
- **真机 vs 仿真**：真机忙等/输入捕获；仿真用 Pin Event Queue 同步结算（§1.3）。
- **保障方案**：**B** 同步事件驱动快进。
- **验收预言**：已知距离/脉宽映射下返回值误差在契约容差内；过程中 Asyncify 挂起次数为 0（HEADLESS）。
- **边界**：快进跳过中间边沿见 C14.2。

<a id="c2.3"></a>

#### C2.3 去抖/RC 低通锚定虚拟时钟
- **问题**：滤波窗口跟墙钟走 → 不同机器结果不同。
- **真机 vs 仿真**：真机有模拟噪声；仿真算法必须绑 `s_virtual_us`。
- **保障方案**：**B** `wink_phys_*` 统一读虚拟时钟（[07](../02-mechanisms/06-physical-degradation.md)）。
- **验收预言**：固定输入边沿序列 + 固定参数 → 滤波输出 golden 向量一致。
- **边界**：噪声幅值是注入参数，非电路级噪声。

<a id="c2.4"></a>

#### C2.4 单中断友好采样周期
- **问题**：周期任务"以为"按固定 ms 跑，实际被调度抖动打乱。
- **真机 vs 仿真**：真机抢占抖动大；仿真协作式更稳，可能**过于理想**。
- **保障方案**：**B** 虚拟周期断言；可选 **C** 注入受控抖动。
- **验收预言**：无注入时周期误差为 0（虚拟时间）；注入抖动后业务仍满足规约或显式失败。
- **边界**：硬实时周期归 C10。

<a id="c2.5"></a>

#### C2.5 跨宿主整数/状态轨迹确定性
- **问题**：同 seed 在不同浏览器 / JS 引擎 / Wasm 运行时下轨迹漂移 → CI 绿、用户机红，或反向。
- **真机 vs 仿真**：真机无此宿主层；仿真的核心价值之一是跨宿主可复现（相对真机墙钟抖动）。
- **保障方案**：**B+C** HEADLESS golden：固定 seed、注册序、输入边沿；整数与离散状态轨迹跨宿主比对；浮点路径显式走 [ADR-0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md) 公差带，禁止默认宣称 host↔wasm / 跨引擎 byte-identical。
- **验收预言**：整数/状态轨迹 **Bit-Identical**（跨至少两种宿主或 CI 矩阵条目）；浮点项标注 `fp_mode=tolerance|bit_exact` 且符合 ADR-0055。
- **边界**：可视化插值/渲染帧率不在轨迹内；PRNG 全局消费序变更须重基 golden（见 `06` §7）。

---

<a id="c3"></a>

### C3 — 共享状态竞态（Task↔Task / Task↔ISR）

**目标**：无锁共享、多字段撕裂、Task/ISR 交叉访问能被激发并检出。

<a id="c3.1"></a>

#### C3.1 无锁共享读写（Task↔Task）
- **问题**：两任务无锁读写同一变量；协作调度固定交错"永远不错"。
- **真机 vs 仿真**：真机抢占/双核易爆；仿真 RR 掩盖。
- **保障方案**：**B** 确定性混沌调度（PRNG + `fairness_bound`）；**C** 影子内存 TSan。
- **验收预言**：混沌模式下已知竞态用例稳定 Fault；加临界区后 Fault 消失。
- **边界**：非真双核重叠写（见 C9）。

<a id="c3.2"></a>

#### C3.2 Task↔ISR 无锁交叉
- **问题**：Task 读多字节状态时被 ISR 半更新。
- **真机 vs 仿真**：真机任意指令间刺入；仿真仅在 yield/poll 点插 ISR。
- **保障方案**：**B** 多调度点插 ISR；**C** TSan 标记 ISR 上下文访问。
- **验收预言**：故意无保护的多字节共享在混沌+ISR 注入下检出。
- **边界**：指令间任意刺入需 Phase 4 加深，仍非周期精确。

<a id="c3.3"></a>

#### C3.3 多字段结构体撕裂
- **问题**：`{x,y}` 或"长度+指针"只更新一半被读到。
- **真机 vs 仿真**：同 C3.1/C3.2，结构体更易漏。
- **保障方案**：**C** 影子内存按字段/对象版本；**A** 规约要求 seqlock/临界区。
- **验收预言**：半更新用例 Fault；完整临界区或 seqlock 通过。
- **边界**：不保证检出所有无锁 ABA。

<a id="c3.4"></a>

#### C3.4 发布-订阅顺序假设
- **问题**：先写 flag 后写 payload（缺 barrier/发布序）。
- **真机 vs 仿真**：单核 Wasm 弱化乱序可见性；真机多核/编译器重排更严重。
- **保障方案**：**A** 禁止裸 flag 协议；**C** TSan/review；真机抽样。
- **验收预言**：lint/review 门禁拦截裸 flag；混沌尝试激发。
- **边界**：完整内存模型仿真不在日间轨（→C12/C24）。

---

<a id="c4"></a>

### C4 — 临界区与中断抢占/嵌套

<a id="c4.1"></a>

#### C4.1 临界区门禁（enter/exit）
- **问题**：忘 critical_exit、嵌套计数错、临界区内 sleep。
- **真机 vs 仿真**：真机可能死锁/随机崩；仿真可断言。
- **保障方案**：**B+C** `pal_os_critical_enter/exit` 状态机断言；临界区内禁止 ISR 派发（[05 §4](../02-mechanisms/04-interrupt-model.md)）。
- **验收预言**：未配对 exit → Fault；临界区内 pending IRQ 推迟到 exit 后。
- **边界**：不等价于关全局硬件中断的所有副作用。

<a id="c4.2"></a>

#### C4.2 调度点 ISR 投递（Poll 模型）
- **问题**：误以为随时可抢占；实际只在 tick/yield 边界 poll。
- **真机 vs 仿真**：真机硬件刺入；仿真 Poll。
- **保障方案**：**B** 文档化 Poll 语义；Phase 4 增加插入点近似。
- **验收预言**：在已知 yield 点注册边沿 → ISR 在下一 poll 窗口执行；纯计算无 yield 窗口内不执行（基线）。
- **边界**：任意指令间刺入不可验。

<a id="c4.3"></a>

#### C4.3 优先级嵌套
- **问题**：高优先级 ISR 抢占低优先级 ISR/Task。
- **真机 vs 仿真**：ESP-IDF/FreeRTOS 支持；仿真基线单级。
- **保障方案**：**B** Phase 4 先单级抢占 + 临界区；嵌套后置；真机兜底。
- **验收预言**：单级：临界区外可插入；嵌套用例在实现前标真机。
- **边界**：完整嵌套优先级不在日间轨承诺。

<a id="c4.4"></a>

#### C4.4 FromISR / 非 ISR-safe API 误用
- **问题**：ISR 内调用阻塞/取锁 API。
- **真机 vs 仿真**：真机偶发死锁/assert；仿真可能"碰巧能跑"。
- **保障方案**：**A** lint/API 属性（ISR-safe 白名单）；**C** ISR 上下文调非安全 API → Fault（[05 §5](../02-mechanisms/04-interrupt-model.md)）。
- **验收预言**：ISR 调 `mutex_lock` 必须 Fault。
- **边界**：第三方闭源回调需人工标注。

<a id="c4.5"></a>

#### C4.5 Pending 中断队列溢出
- **问题**：边沿风暴超过 `PAL_WASM_INTERRUPT_QUEUE_SIZE`（默认 16）→ 丢中断。
- **真机 vs 仿真**：真机可能硬件 pend；仿真队列满需策略。
- **保障方案**：**C** 队列满 Fail-Loud（计数/Fault）；**B** 可配置深度；诚实文档。
- **验收预言**：注入 > 容量边沿 → 可观测溢出计数或 Fault；禁止静默丢假装处理。
- **边界**：不建模芯片特定中断控制器全状态。

---

<a id="c5"></a>

### C5 — 阻塞/饿死/看门狗

<a id="c5.1"></a>

#### C5.1 STRICT_NONBLOCKING 编译期隐藏阻塞 API
- **问题**：App 误用阻塞读 → 真机 WDT。
- **真机 vs 仿真**：仿真默认严格模式（ADR-0025）。
- **保障方案**：**A** `-DWINK_STRICT_NONBLOCKING=1`；阻塞符号隐藏 → 链接失败（[04 §8](../02-mechanisms/03-scheduler-and-concurrency.md)）。
- **验收预言**：App 调 WINK_BLOCKING API → 仿真构建失败。
- **边界**：bringup/selftest 隔离在非严格配置。

<a id="c5.2"></a>

#### C5.2 软 WDT（虚拟时间未喂狗）
- **问题**：任务死循环不喂狗。
- **真机 vs 仿真**：真机硬件 WDT 复位；仿真用虚拟时间软 WDT。
- **保障方案**：**B+C** > N 虚拟 ms 未喂 → Fault 隔离。
- **验收预言**：停喂用例在阈值虚拟时间内 Fault。
- **边界**：不反映宿主 CPU 耗尽的**物理** WDT（见调度器 WCET 墙钟兜底，[04 §3](../02-mechanisms/03-scheduler-and-concurrency.md)）。

<a id="c5.3"></a>

#### C5.3 就绪任务饿死
- **问题**：高优先级/从不 yield 任务饿死低优先级。
- **真机 vs 仿真**：真机有时间片；仿真协作更易饿死或相反（RR 更公平）。
- **保障方案**：**C** "ready 但久未运行"计数告警；**B** 混沌 fairness bound。
- **验收预言**：构造饿死用例 → 告警/Fault。
- **边界**：不完全等价 FreeRTOS 优先级抢占（C16）。

<a id="c5.4"></a>

#### C5.4 动态间接阻塞
- **问题**：经函数指针/回调的阻塞路径，静态规约漏掉。
- **真机 vs 仿真**：同样危险。
- **保障方案**：**A** 最大化静态可见性；**C** 运行时"阻塞点"探针（阻塞 API 入口检查调用上下文）。
- **验收预言**：间接阻塞调用在可检测路径仍被运行时门禁捕获。
- **边界**：纯 asm/极端混淆不保证。

<a id="c5.5"></a>

#### C5.5 优先级反转
- **问题**：低优先级持锁，高优先级忙等/阻塞，中优先级运行。
- **真机 vs 仿真**：需优先级继承/优先级天花板；仿真基线可能无。
- **保障方案**：**B** 文档化是否实现继承；**C** 检测"高阻塞在低持锁超过阈值"；真机验证。
- **验收预言**：经典反转用例：若声明继承则通过，否则显式标弱验/真机。
- **边界**：完整继承协议可分阶段。

---

<a id="c6"></a>

### C6 — 栈/堆/内存安全

<a id="c6.1"></a>

#### C6.1 静态堆配额耗尽
- **问题**：仿真堆 >> 真实 SRAM 掩盖 OOM。
- **真机 vs 仿真**：真机 OOM 早；仿真需配额（ADR-0045）。
- **保障方案**：**B** 固定堆封顶（`-sINITIAL_MEMORY/MAXIMUM_MEMORY/ALLOW_MEMORY_GROWTH=0`，默认 256KiB 断言基线）；耗尽 → `WINK_ERR_NO_MEM`(-13)。
- **验收预言**：配额耗尽用例进入 Fault/错误处理，错误码语义与真机对齐。
- **边界**：碎片形态与真机分配器不同。
- **落地注**：固定堆链接标志在当前 CMake 尚未检出，落地状态以 CMake 为准（见 [06 §1.2](../02-mechanisms/05-memory-and-faults.md)）。

<a id="c6.2"></a>

#### C6.2 堆碎片化
- **问题**：反复 alloc/free → "总量够但分配失败"。
- **真机 vs 仿真**：分配器不同 → 碎片图不同。
- **保障方案**：**B** 可选碎片压力模式；**C** alloc 失败注入。
- **验收预言**：压力模式可触发 NO_MEM；业务有降级路径。
- **边界**：不宣称与 ESP-IDF heap 碎片几何一致。

<a id="c6.3"></a>

#### C6.3 ASan/UBSan（UAF/越界/未对齐/溢出）
- **问题**：野指针/UAF/OOB 无消毒器时静默。
- **真机 vs 仿真**：真机随机；host 测试可启消毒器。
- **保障方案**：**C** CI/Host 启 ASan+UBSan（[06 §6](../02-mechanisms/05-memory-and-faults.md)）。
- **验收预言**：已知坏用例被拦截。
- **边界**：Wasm 日间轨可能不全跑 ASan。

<a id="c6.4"></a>

#### C6.4 App 禁裸 malloc
- **问题**：App 层直接 malloc 破坏资源模型。
- **保障方案**：**A** NO-MALLOC-APP lint（`tools/lint/rules/memory.yaml`）。
- **验收预言**：违规 → lint fail。
- **边界**：平台层分配器内部除外。

<a id="c6.5"></a>

#### C6.5 Per-task 栈溢出
- **问题**：任务栈太小；Wasm 单栈/Fiber 与 FreeRTOS 多栈不同。
- **真机 vs 仿真**：真机有栈 canary；Wasm 布局可能不同。
- **保障方案**：**C** Fiber/栈 watermark 或 STACK_OVERFLOW_CHECK；**B** 尝试 per-task 栈记账。
- **验收预言**：深递归/大栈帧用例触发溢出检测。
- **边界**：与 Xtensa 栈 red zone 逐字节对齐不可验。

<a id="c6.6"></a>

#### C6.6 缓冲区在 DMA/异步传输中被复用
- **问题**：传输完成前改 buffer（与 C19 交叠）。
- **保障方案**：见 C19；本项强调内存所有权。
- **验收预言**：传输窗口内写 buffer → Fault 或数据损坏被检出。
- **边界**：依赖 C8 异步窗口先存在。

---

<a id="c7"></a>

### C7 — 总线协议帧/CRC/错误恢复

<a id="c7.1"></a>

#### C7.1 同源协议帧与 CRC
- **问题**：bypass CRC → 真机坏帧处理未测。
- **保障方案**：**B** DAL 同源；坏 CRC 注入。
- **验收预言**：坏 CRC 帧进入错误恢复，不进业务成功态。
- **边界**：电气错误位形态 → C11/C18。

<a id="c7.2"></a>

#### C7.2 ACK 超时与重试
- **问题**：超时/重试计数与真机不一致。
- **保障方案**：**B** 虚拟时间超时；`drop_permil`。
- **验收预言**：固定丢包 seed → 重试次数/最终错误码可复现。
- **边界**：线缆延迟谱不建模。

<a id="c7.3"></a>

#### C7.3 JSON 语义仿真门禁
- **问题**：未声明器件走语义捷径。
- **保障方案**：**A** ADR-0040（[08 §0](../02-mechanisms/07-peripheral-registry.md)）。
- **验收预言**：未配置外设 Fail-Loud。
- **边界**：门禁只管"是否允许 bypass"，不管 bypass 内容是否电气正确。

<a id="c7.4"></a>

#### C7.4 残留 Bypass 清零审计
- **问题**：部分驱动残留整层 `#ifdef SIMULATION`。
- **保障方案**：**A+C** 静态扫描 + CI 审计清单。
- **验收预言**：审计清单为空或每项有豁免 ADR。
- **边界**：物理量 hook 层 `#ifdef` 允许。

---

<a id="c8"></a>

### C8 — DMA/总线异步传输窗口

<a id="c8.1"></a>

#### C8.1 粗粒度传输耗时挂起
- **问题**：`pal_i2c_transfer` 0µs 返回，掩盖"传输期间并发改 buffer"。
- **真机 vs 仿真**：真机 DMA 期间 CPU 可跑其他任务。
- **保障方案**：**B** 按字节率算虚拟耗时，挂起 Task，完成 IRQ 在 deadline 唤醒。
- **验收预言**：另一任务在传输窗口写共享 buffer 可被检出为竞态（配合 C3/C19）。
- **边界**：非 cycle 精确总线时序。

<a id="c8.2"></a>

#### C8.2 完成中断与任务唤醒序
- **问题**：错误假设完成回调与等待任务唤醒顺序。
- **保障方案**：**B** 定义"先置完成标志/入队，再 resume"；混沌打乱同优先级。
- **验收预言**：用例验证官方顺序；错序在混沌下失败。
- **边界**：芯片 DMA IRQ 优先级细节可能仍有差。

<a id="c8.3"></a>

#### C8.3 同步 API 残留
- **问题**：部分 PAL 仍同步 → C8 漏洞。
- **保障方案**：**B** 总线 API 迁移清单；**A** 标注仅同步符号。
- **验收预言**：目标总线都有异步模型或显式"仅同步"标签。
- **边界**：过渡期允许带标注豁免。

---

<a id="c9"></a>

### C9 — 多核 SMP 真实并发

<a id="c9.1"></a>

#### C9.1 单虚拟核产品边界
- **问题**：用户期望浏览器里双核。
- **真机 vs 仿真**：遵循 ADR-0014——**刻意不做**真双核。
- **保障方案**：checklist 标为真机/HIL 独占或不覆盖；无"已仿真双核"主张。
- **验收预言**：无"dual-core simulated"宣称。
- **边界**：真机兜底。

<a id="c9.2"></a>

#### C9.2 混沌交错近似多核竞态
- **问题**：单核可通过高频插入近似部分竞态。
- **保障方案**：**B** Phase 4 混沌；真机双核测试加固。
- **验收预言**：已知单核可证竞态在混沌下检出；双核专属用例进真机套件。
- **边界**：cache 一致性、跨核中断路由不建模（C24）。

---

<a id="c10"></a>

### C10 — 快环 ISR（FOC/硬定时器）

<a id="c10.1"></a>

#### C10.1 虚拟时间软步进近似
- **问题**：Wasm 无 20kHz 硬 ISR。
- **真机 vs 仿真**：真机硬实时；仿真软步进。
- **保障方案**：**B** ADR-0047 软步进；plant 方程在 `wink_sim_physical`；禁墙钟/`rand`（[`09-timer`](../02-mechanisms/09-timer-and-pwm-semantics.md)）。
- **验收预言**：固定虚拟步进 → 控制状态可复现；比对 golden 轨迹。
- **边界**：不重现 50µs 硬实时。

<a id="c10.2"></a>

#### C10.2 PWM–ADC 硬件同步降级
- **问题**：真机硬件触发采样；仿真软步进相位可能漂移。
- **保障方案**：**B** 文档化降级；逻辑正确性可测，相位误差真机/HIL。
- **验收预言**：相位敏感用例进 HIL 清单；仿真只验证控制律。
- **边界**：cycle 精确同步为真机/HIL 独占。

<a id="c10.3"></a>

#### C10.3 DI/ISR 分层边界
- **问题**：业务逻辑塞进快环 → 不可仿真/不可测。
- **保障方案**：**A** ADR-0047 分层；lint 限制快环调用面（BAL `control/` 纯数学，无 `pal_*`；DAL 硬件块；`foc_isr_trampoline` 不进 BAL 公共头）。
- **验收预言**：快环只允许白名单 PAL；违规 lint fail。
- **边界**：白名单随产品演进。

---

<a id="c11"></a>

### C11 — 电气/模拟电路特性

<a id="c11.1"></a>

#### C11.1 SPICE/电源完整性
- **保障方案**：真机/HIL 独占（ADR-0003）；真机/专用工具。
- **验收预言**：对外承诺排除此项。
- **边界**：刻意不覆盖。

<a id="c11.2"></a>

#### C11.2 退化引擎查表近似
- **问题**：全理想传感器误导调参。
- **保障方案**：**B** [07](../02-mechanisms/06-physical-degradation.md) 抖动/噪声/预热/丢包。
- **验收预言**：注入参数改变读数分布；理想模式 `{0}` 可回归。
- **边界**：非 SPICE；参数为经验表。

<a id="c11.3"></a>

#### C11.3 ADC 量化/参考电压/管脚电容
- **保障方案**：真机/HIL 独占，或极粗量化可选；默认不覆盖。
- **验收预言**：若提供粗量化则有单测；否则 checklist 标为真机/HIL 独占。
- **边界**：认证级模拟精度在真机。

---

<a id="c12"></a>

### C12 — CPU/ABI 指令级一致性

<a id="c12.1"></a>

#### C12.1 日间极速轨道（Wasm/Host 原生）
- **保障方案**：**B** 快速迭代；接受 ABI 可能与真机有差。
- **验收预言**：功能测试秒级反馈。
- **边界**：非指令级。

<a id="c12.2"></a>

#### C12.2 夜间 RISC-V32/真机二进制双轨
- **问题**：padding/alignment/调用约定/枚举宽度导致"仿真过、板级崩"。
- **保障方案**：**C** Phase 5 解释器或真机 `.bin` 比对。
- **验收预言**：刻意对 padding 敏感的用例在夜间轨失败/告警。
- **边界**：慢，非日间必需。

<a id="c12.3"></a>

#### C12.3 结构体打包/枚举/位域
- **保障方案**：**A** 打包规约 + `_Static_assert`；**C** 双轨。
- **验收预言**：`_Static_assert(sizeof...)`；跨 target 尺寸一致或显式分支。
- **边界**：位域布局仍编译器相关，谨慎使用。

<a id="c12.4"></a>

#### C12.4 端序与未对齐访问
- **保障方案**：**C** UBSan；协议层显式序列化。
- **验收预言**：未对齐用例被 Host UBSan 拦截；线协议有序列化测试。
- **边界**：Wasm 与 Xtensa 对未对齐容忍度不同。

---

<a id="c13"></a>

### C13 — 生命周期/复位/冷热启动

**目标**：启动/重初始化/热重启状态对齐真机上电/复位语义，避免仿真残留污染。

<a id="c13.1"></a>

#### C13.1 冷启动：BSS/静态初值/外设默认电平
- **问题**：仿真热复用模块 → 静态变量/GPIO 默认不像上电。
- **真机 vs 仿真**：真机上电是硬件默认；Wasm 第二次 INIT 可能残留。
- **保障方案**：**B** INIT 路径强制 `pal_wasm_reset_physical` + scheduler/heap/PRNG reset；文档化冷启动清单。
- **验收预言**：两次 INIT 间状态匹配冷启动向量（无业务残留）。
- **边界**：芯片特定 strap pin 电平可能无法完全仿真。

<a id="c13.2"></a>

#### C13.2 热重启/软复位原因
- **问题**：分支依赖 `esp_reset_reason` 常量。
- **保障方案**：**B** 可注入复位原因；未实现则文档标弱验。
- **验收预言**：注入 BROWN-OUT/WATCHDOG 枚举 → 对应恢复路径。
- **边界**：真机掉电波形为非目标。

<a id="c13.3"></a>

#### C13.3 外设 deinit/再 init
- **问题**：未 deinit 再 init → 句柄泄漏、重复 IRQ 注册。
- **保障方案**：**C** 重复注册断言；资源记账。
- **验收预言**：重复 init 用例 Fail-Loud 或幂等成功（按 API 契约）。
- **边界**：契约须先声明"是否幂等"。

<a id="c13.4"></a>

#### C13.4 任务/对象生命周期与 Zombie GC
- **问题**：`task_delete` 后碰栈/队列（[04 §7](../02-mechanisms/03-scheduler-and-concurrency.md) ZOMBIE/TERMINATED）。
- **保障方案**：**B** 调度器 GC 语义；**C** UAF Sanitizer。
- **验收预言**：delete 后访问 → Fault/ASan。
- **边界**：可能与 FreeRTOS 异步 delete 细节有差，需对照表。

<a id="c13.5"></a>

#### C13.5 长周期循环后句柄/资源计数回零
- **问题**：成千上万次 create/delete 或业务循环后句柄泄漏、watermark 单调上涨；短测与单次 re-init（C13.3）抓不到。
- **真机 vs 仿真**：真机长跑后 OOM/句柄耗尽；仿真堆大时泄漏更易被掩盖。
- **保障方案**：**C** soak：固定循环 N（文档化下限）后断言 OS/DAL 句柄计数回基线、堆 watermark 不单调增长；可选与 C6.1 配额联测。
- **验收预言**：soak 结束句柄计数 = 起点；堆高水位相对起点增量 ≤ 约定阈值（理想为 0）。
- **边界**：刻意缓存/池化对象须在用例中声明为稳态持有，不计入泄漏。

---

<a id="c14"></a>

### C14 — 快进/联合仿真步进契约

**目标**：虚拟时间跳跃与 plant 锁步不破坏因果；禁止双重步进。同刻事件总序见 [ADR-0053](../../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)。

<a id="c14.1"></a>

#### C14.1 时钟单一写入/禁止双重步进
- **问题**：`delay` 与 Worker 都推进 → 时间撕裂。
- **保障方案**：**A+B** SSOT 红线（§1.1）；CI 断言唯一 Gate。
- **验收预言**：静态/运行时检测到第二写入者 → Fail。
- **边界**：HEADLESS 内部 Gate 是合法例外，必须文档化。

<a id="c14.2"></a>

#### C14.2 快进跨越边沿/半窗 debounce
- **问题**：跳到 next_wakeup 跳过本应在中途发生的 GPIO 边沿/去抖半窗。
- **保障方案**：**B** 快进前 drain 到期 pin 事件/物理步进；或快进取全局"下一事件"时间。
- **验收预言**：含中途边沿用例在快进下仍触发；去抖窗不被错误缩短/拉长。
- **边界**：队列满 → C4.5/C14.4。

<a id="c14.3"></a>

#### C14.3 Plant↔OS 锁步漂移
- **问题**：JS plant 用墙钟或不同 Δt，与 `s_virtual_us` 解耦。
- **保障方案**：**B** Step-Lock：同一 `virtual_dt` 驱动 OS 与 plant；禁 plant 读 `Date.now`。
- **验收预言**：同 seed 轨迹可复现；故意用墙钟的 plant 被门禁或测试拒绝。
- **边界**：可视化插值允许，但不得回写控制决策。

<a id="c14.4"></a>

#### C14.4 Pin Event Queue 溢出/丢失
- **问题**：同步回调写太多未来边沿 → 丢事件。
- **保障方案**：**C** 队列满 Fault；**B** 可配置深度。
- **验收预言**：溢出可观测，不静默。
- **边界**：深度与真机硬件 FIFO 无关。

<a id="c14.5"></a>

#### C14.5 观测与注入竞态
- **问题**：plant 写输入同时读 GPIO 输出，无明确屏障。
- **保障方案**：**B** 定义步内顺序"先读控制 → 算 plant → 再注入"；测试固定该序。
- **验收预言**：破坏顺序的测试夹具失败或被 API 禁止。
- **边界**：并行 Worker 多 plant 需额外屏障约定。

---

<a id="c15"></a>

### C15 — Host↔Wasm 边界诚实性

**目标**：仿真底座本身不"撒谎"；Asyncify/ABI/门禁错误 Fail-Loud。

<a id="c15.1"></a>

#### C15.1 Asyncify 挂起契约
- **问题**：`sleep` 不返回 Promise / `__async` 写错 → 静默不睡或 rewind 死循环（ADR-0019，[02 §2](../02-mechanisms/01-sandbox-and-execution.md)）。
- **保障方案**：**A** js-library wrapper 约定；**C** 非法 Asyncify 状态断言。
- **验收预言**：错误覆盖用例 CI fail；正确路径可挂起/恢复。
- **边界**：只覆盖本仓约定导入集。

<a id="c15.2"></a>

#### C15.2 中断 Push→Poll/重入
- **问题**：旧 Push 模型在 sleeping 窗口重入崩溃；必须 Poll。
- **保障方案**：**B** 禁止导出 `_trigger_wasm_interrupt`；只有 pending 队列 + dispatch（[05](../02-mechanisms/04-interrupt-model.md)）。
- **验收预言**：静态检查无 Push 导出；重入用例不崩。
- **边界**：Poll 延迟语义 → C4.2。

<a id="c15.3"></a>

#### C15.3 bigint/指针 ABI
- **问题**：`number` 截断 uint64 时钟；JS 悬垂指针。
- **保障方案**：**A** `-s WASM_BIGINT=1`；TS 全链 `bigint`；指针生命周期约定（[10](../02-mechanisms/10-wasm-js-bridge-abi.md)）。
- **验收预言**：大时钟值无损往返；违例用例 Fail。
- **边界**：第三方 JS 插件须遵守同契约。

<a id="c15.4"></a>

#### C15.4 语义 Bypass 泄露
- **问题**：Workbench/测试直接调语义捷径绕开门禁。
- **保障方案**：**A** ADR-0040；**C** 运行时探针。
- **验收预言**：未授权 bypass 失败。
- **边界**：调试器手工写内存不在范围。

<a id="c15.5"></a>

#### C15.5 Worker 隔离与主线程 starve
- **问题**：主线程跑 Wasm+Asyncify → 定时器饿死/OOM。
- **保障方案**：**A** 必须 Worker（[02 §1](../02-mechanisms/01-sandbox-and-execution.md)）。
- **验收预言**：架构测试/文档门禁；禁主线程加载路径。
- **边界**：Node host 有单独约束注记。

---

<a id="c16"></a>

### C16 — OS 同步原语语义对齐

**目标**：mutex/queue/ringbuf/超时与真机 OSAL 契约对齐，避免"返回值碰巧不同"。

<a id="c16.1"></a>

#### C16.1 Mutex 锁/超时/递归
- **问题**：超时返回码、可递归性与 FreeRTOS 不一致。
- **保障方案**：**B** 语义对照表（[04 §9](../02-mechanisms/03-scheduler-and-concurrency.md)）；`timeout_fired` 行为单测。
- **验收预言**：表每行有测试；递归策略与文档一致（禁或支持）。
- **边界**：优先级继承 → C5.5。

<a id="c16.2"></a>

#### C16.2 Queue/Ringbuf 满与覆盖策略
- **问题**：满=block/drop-oldest/reject 不一致 → 丢数。
- **保障方案**：**B** API 契约固定；**C** 满/空计数观测。
- **验收预言**：满队列用例返回码/丢弃策略匹配契约。
- **边界**：零拷贝 DMA 队列 → C19。

<a id="c16.3"></a>

#### C16.3 阻塞等待与超时唤醒序
- **问题**：事件与超时同时到期胜者未定义。
- **保障方案**：**B** 定义优先级（如事件优先）；单测钉死。
- **验收预言**：同时到期结果稳定且符合契约。
- **边界**：若真机不同，表中注"intentional diff"或改仿真。

<a id="c16.4"></a>

#### C16.4 任务通知/事件组（若暴露）
- **问题**：bit 自动清、wait-multi-bit 语义易错。
- **保障方案**：**B** 若存在则测；否则 **A** 不暴露或标 experimental。
- **验收预言**：每个暴露 API 有语义测试；未暴露则 12 标 N/A。

<a id="c16.5"></a>

#### C16.5 死锁检测
- **问题**：A 持锁等 B，B 持锁等 A。
- **保障方案**：**C** 可选 wait-for-graph 检测/超时；**A** 锁序规约。
- **验收预言**：经典死锁用例超时或 Fault。
- **边界**：不能完全证明无死锁。

---

<a id="c17"></a>

### C17 — 外设资源互斥/时基耦合

<a id="c17.1"></a>

#### C17.1 引脚复用冲突
- **问题**：两器件声明同一 GPIO。
- **保障方案**：**A** codegen/板级 JSON 冲突检测；仿真注册时断言。
- **验收预言**：冲突配置 Fail-Loud。
- **边界**：运行时动态 pin-mux 需额外 API 契约。

<a id="c17.2"></a>

#### C17.2 定时器/PWM 通道独占
- **问题**：同一硬件定时器被 PWM 与输入捕获共用。
- **保障方案**：**A** 资源分配表；**C** 二次占用 Fault。
- **验收预言**：双占用用例失败。
- **边界**：芯片特定合法共享模式需白名单。

<a id="c17.3"></a>

#### C17.3 APB/外设时钟变更副作用
- **问题**：改 CPU/APB 频率静默改 UART 波特率/PWM 频率。
- **保障方案**：仿真 **B** 粗模型或声明不保（真机/HIL）；真机测试。
- **验收预言**：若建模，改频率后波特率误差被断言；否则 checklist：真机/HIL 独占。
- **边界**：完整时钟树为非目标。

<a id="c17.4"></a>

#### C17.4 PWM 占空比更新毛刺/相位连续性
- **问题**：改 duty 产生单周期毛刺；仿真即时寄存器写无毛刺。
- **保障方案**：**B** 可选"下一周期生效"模型；或文档标弱验/近似。
- **验收预言**：若建模，更新对齐周期边界；否则标近似。
- **边界**：电气毛刺为非目标（C11）。

<a id="c17.5"></a>

#### C17.5 共享总线所有权（多驱动争用）
- **问题**：两任务无锁并发 `i2c_transfer`。
- **保障方案**：**A** 总线锁规约；**C** 重叠传输窗口检测 Fault。
- **验收预言**：无锁并发传输被检出。
- **边界**：多主仲裁 → C18。

---

<a id="c18"></a>

### C18 — 总线故障态机（超越 CRC/丢包）

<a id="c18.1"></a>

#### C18.1 I2C NACK/clock stretch/总线挂死
- **问题**：只测丢包，不测 NACK/SCL 卡死/恢复。
- **保障方案**：**B** 注入 NACK/stretch 超时；同源驱动恢复序列。
- **验收预言**：注入后错误码与恢复后成功可复现。
- **边界**：电气上升沿为非目标。

<a id="c18.2"></a>

#### C18.2 UART framing/break/FIFO 溢出/idle-line
- **问题**：坏帧、FIFO 满丢字节未测。
- **保障方案**：**B** 注入 framing error + RX 溢出；**C** 溢出计数。异步 RX/RX IRQ 模型边界见 [ADR-0054](../../../decisions/unisim/0054-sim-uart-async-rx-model-boundary.md)。
- **验收预言**：溢出后驱动进入约定错误/丢弃策略。
- **边界**：采样点相位为非目标。

<a id="c18.3"></a>

#### C18.3 SPI 模式/CS 时序/全双工
- **问题**：错 CPOL/CPHA 或 CS 边沿假设。
- **保障方案**：**B** 模型含 mode 参数；错模式致数据不匹配测试失败。
- **验收预言**：错模式用例失败；正确模式 golden 通过。
- **边界**：板级跳线延迟为非目标。

<a id="c18.4"></a>

#### C18.4 传输中止/半包恢复
- **问题**：abort 后状态机卡在半包。
- **保障方案**：**B** abort API + 驱动恢复；测半包后下一帧。
- **验收预言**：abort 后下一帧成功或显式错误。
- **边界**：DMA abort 细节与 C19 交叠。

<a id="c18.5"></a>

#### C18.5 多主仲裁（若产品需要）
- **保障方案**：多数产品为真机/HIL 独占；需要则 **B** 简化仲裁胜者模型。
- **验收预言**：需要才构建；否则 checklist：真机/HIL 独占。
- **边界**：真机完整 I2C 仲裁时序。

---

<a id="c19"></a>

### C19 — DMA/缓冲生命周期

<a id="c19.1"></a>

#### C19.1 半传输/双缓冲切换
- **问题**：只建模"全完成"；半缓冲回调逻辑未跑。
- **保障方案**：**B** 可选半完成事件；双缓冲所有权切换断言。
- **验收预言**：半传输用例回调次数与 buffer index 正确。
- **边界**：粗粒度时间，非节拍级。

<a id="c19.2"></a>

#### C19.2 传输中缓冲区复用
- **问题**：DMA 进行中 CPU 写同一 buffer。
- **保障方案**：**C** 窗口内写检测（配合 C8）；**A** API 生命周期文档。
- **验收预言**：窗口内写 → Fault。
- **边界**：依赖 C8 异步窗口存在。

<a id="c19.3"></a>

#### C19.3 Abort 后描述符/句柄残留
- **问题**：abort 后仍以为 DMA 在跑或 double-free。
- **保障方案**：**B** 状态机；**C** double-free/UAF Sanitizer。
- **验收预言**：abort 后重启成功；非法二次 abort 明确错误码。
- **边界**：芯片描述符硬件格式不仿真。

<a id="c19.4"></a>

#### C19.4 DMA 可访问内存区（与 C24 交叠）
- **问题**：buffer 落在非 DMA 区。
- **保障方案**：仿真 **A** tag 检查或弱验声明；真机必测。
- **验收预言**：若有 tag，错误区 alloc 失败；否则 12 链 C24。
- **边界**：ESP32 cache/DMA RAM 细节在真机。

---

<a id="c20"></a>

### C20 — 回调重入/延迟下半部

> 主解法统一为 **A+C**（与 [`02-consistency-checklist`](./02-consistency-checklist.md) 对齐，消除旧 05/08 冲突）。

<a id="c20.1"></a>

#### C20.1 ISR/回调内调用会 yield 的 DAL
- **问题**：回调里阻塞读传感器 → 重入/死锁。
- **保障方案**：**A** 回调上下文规则；**C** 上下文检测 Fault（与 C4.4 重叠）。
- **验收预言**：非法调用 Fault。
- **边界**：同 C4.4。

<a id="c20.2"></a>

#### C20.2 传感器回调嵌套写执行器
- **问题**：输入回调直接驱动输出，形成同步环与优先级反转。
- **保障方案**：**A** 推荐队列解耦；**C** 可选"回调深度"检测。
- **验收预言**：过深嵌套告警；推荐模式用例通过。
- **边界**：不禁止所有同步耦合，但须文档化风险。

<a id="c20.3"></a>

#### C20.3 Workqueue/延迟下半部顺序
- **问题**：假设 softirq/workqueue 顺序；仿真缺该层。
- **保障方案**：**A** 若 PAL 提供 deferred work 则定义顺序；否则不暴露。
- **验收预言**：若存在则测顺序；否则 N/A。
- **边界**：不照搬 Linux softirq 语义。

---

<a id="c21"></a>

### C21 — 时间与计数回绕

> 主解法统一为 **A+C**（与 [`02-consistency-checklist`](./02-consistency-checklist.md) 对齐）。

<a id="c21.1"></a>

#### C21.1 uint32 ms/滴答回绕
- **问题**：`now - last` 不用无符号减法 → 回绕破坏超时。
- **保障方案**：**A** 编码规约；**C** 快进跨越回绕点测试。
- **验收预言**：回绕窗两侧超时正确。
- **边界**：`s_virtual_us` 是 uint64，仍须测 App 自管 uint32。

<a id="c21.2"></a>

#### C21.2 相对超时跨越快进
- **问题**：快进后相对 deadline 算错。
- **保障方案**：**A** 一律用绝对 `wakeup_us`；单测。
- **验收预言**：快进后到期任务必唤醒。
- **边界**：与 C14 重叠。

<a id="c21.3"></a>

#### C21.3 序列号/环形下标 wrap
- **问题**：seq 差、环形下标取模错误。
- **保障方案**：**C** 专项单测；fuzz seed。
- **验收预言**：跨 wrap push/pop 正确。
- **边界**：业务自定义序列需自测。

<a id="c21.4"></a>

#### C21.4 时间单位换算截断/乘除溢出
- **问题**：`ms * 1000` → us、tick↔ms 等换算在 uint32 上溢出或静默截断（与 C21.1 回绕正交）。
- **真机 vs 仿真**：真机与仿真若换算路径不一致，一边「碰巧」用更大类型则漏测。
- **保障方案**：**A** 统一经已审计的换算 API（禁止业务手写 `* 1000` 进敏感路径）；**C** 边界值单测（接近 `UINT32_MAX/1000`、0、1）。
- **验收预言**：溢出输入 Fail-Loud 或按契约饱和/报错；合法范围往返换算可逆（在文档精度内）。
- **边界**：墙钟/晶振 ppm 非目标；App 自管换算须自测并纳入对照。

---

<a id="c22"></a>

### C22 — 电源/低功耗/时钟域

<a id="c22.1"></a>

#### C22.1 light/deep sleep 唤醒
- **保障方案**：多数路径为真机/HIL 独占 或极简"sleep=挂起任务+快进到唤醒源"；真机兜底。
- **验收预言**：若极简模型，唤醒源注入可唤醒；否则 checklist：真机/HIL 独占。
- **边界**：电流、唤醒延迟波形为非目标。

<a id="c22.2"></a>

#### C22.2 外设时钟门控
- **保障方案**：真机/HIL 独占，或显式 stub"访问未使能外设 → 错误码"。
- **验收预言**：若 stub 测错误码；否则标为真机/HIL 独占。
- **边界**：时钟树为非目标。

<a id="c22.3"></a>

#### C22.3 Brownout/欠压复位
- **保障方案**：复位原因注入 → C13.2；电源波形为非目标。
- **验收预言**：同 C13.2。
- **边界**：模拟 brownout 为非目标。

---

<a id="c23"></a>

### C23 — 持久化/NVS/磨损

<a id="c23.1"></a>

#### C23.1 断电写入撕裂
- **问题**：写中途掉电损坏镜像。
- **保障方案**：**B** 可选"写中途注入掉电"；业务 CRC/双副本同源测试。
- **验收预言**：注入撕裂后启动走恢复/默认路径。
- **边界**：Flash 物理磨损为非目标。

<a id="c23.2"></a>

#### C23.2 键空间耗尽/满盘
- **保障方案**：**B** 配额；**C** 满返回错误。
- **验收预言**：耗尽错误码对齐。
- **边界**：磨损均衡算法不仿真。

<a id="c23.3"></a>

#### C23.3 仿真"掉电"语义
- **问题**：页面刷新是否=掉电不清。
- **保障方案**：**A** 文档：Worker destroy=掉电；显式 snapshot API 另算。
- **验收预言**：无 snapshot → 重启无 NVS；有 snapshot 测往返。
- **边界**：浏览器存储配额策略在另文。

---

<a id="c24"></a>

### C24 — 缓存/内存属性/DMA RAM

<a id="c24.1"></a>

#### C24.1 DMA 必须落在可 DMA 区
- **保障方案**：**A** alloc API 分区；仿真 tag 检查或真机必测（与 C19.4 重叠）。
- **验收预言**：见 C19.4。
- **边界**：Xtensa cache 一致性完整模型为非目标。

<a id="c24.2"></a>

#### C24.2 缓存一致性假设
- **保障方案**：日间轨不覆盖（真机/HIL）；真机/Phase 5 抽样。
- **验收预言**：checklist 标为真机/HIL 独占；真机套件含 DMA+cache 用例。
- **边界**：刻意不覆盖。

<a id="c24.3"></a>

#### C24.3 IRAM/慢径指令位置
- **保障方案**：仿真标弱验；linker-script 差异文档化。
- **验收预言**：不因"仿真能链接"就宣称 IRAM 约束已验。
- **边界**：真机 link 与慢径。

---

<a id="c25"></a>

### C25 — 浮点/数值与编译器 UB

<a id="c25.1"></a>

#### C25.1 有符号溢出/移位 UB
- **问题**：有符号整数溢出与移位量 ≥ 位宽均为 C UB；控制律/协议解析中的 int16/int32 运算、移位解码易触发。
- **真机 vs 仿真**：Wasm 溢出固定为二补数 wrap，真机 Xtensa 取决于编译器（-O2 可按"无溢出"假设优化），仿真"能跑通"不代表真机不踩 UB。
- **保障方案**：**C** UBSan；**A** 编码标准。
- **验收预言**：坏用例在 Host 被拦截。
- **边界**：Wasm 与 Xtensa 溢出 wrap 行为可能仍不同 → C12。

<a id="c25.2"></a>

#### C25.2 NaN/Inf 传播与 flush-to-zero
- **问题**：传感器除零/越界产生 NaN/Inf 后在控制律里静默传播；真机 FPU 的 flush-to-zero（FTZ）/默认 NaN 模式与 Host/Wasm 不同，亚正规数被清零后行为分叉。
- **真机 vs 仿真**：Wasm 遵循 IEEE-754 默认（保留 NaN/Inf 与亚正规数），ESP32 Xtensa FPU 可配置 FTZ/rounding；同一段控制律在仿真传播 NaN、在真机被 FTZ 清零或反向，仿真结论不能直接外推。
- **保障方案**：**B** 控制律 NaN 策略单测；FPU 模式差异真机抽样。
- **验收预言**：注入 NaN → 进入安全态或显式错误。
- **边界**：硬件 FTZ 模式不精确仿真（非目标）。

<a id="c25.3"></a>

#### C25.3 浮点确定性
- **问题**：跨平台浮点归约不一致 → plant 轨迹漂移。
- **真机 vs 仿真**：Xtensa 硬件 FPU 与 Host/Wasm 的 libm、融合乘加（FMA）收缩、舍入模式不同，长时积分/归约误差累积方向不同；仿真轨迹与真机非 bit-exact（详见 [ADR-0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md)）。
- **保障方案**：**B** 固定软件 libm 或公差比对；golden 用公差带（策略见 [ADR-0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md)）。
- **验收预言**：公差带内可复现；带外失败。
- **边界**：不承诺跨所有 target bit-exact。

---

## 3. 深度诊断与演进（索引）

具体里程碑、CI 三层与维护规程见 [13 路线图与治理](./03-roadmap-and-governance.md)。核心失真维度（并发/中断时序/总线/时间推进/宿主边界/生命周期/资源拓扑/内存 ABI）的对照表也在 13。

