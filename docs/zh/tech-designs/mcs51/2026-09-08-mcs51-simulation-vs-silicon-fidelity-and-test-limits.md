# 8051 仿真与物理硅片一致性差异、时序边界与场景测试方法论 (Simulation vs. Silicon Fidelity, Timing Divergence & Test Methodology)

| 属性 | 内容 |
| :--- | :--- |
| **文档状态** | Formal Standard - 现行技术设计规格 (Layer-②) |
| **基线版本与 ADR** | [ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md) (契约诚实), [ADR-0070](../../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md) (C++拦截), [ADR-0071](../../decisions/core/0071-sfr-proxy-rmw-edge-data-plane.md) (数据面代理), [ADR-0072](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md) (双时钟域与 Trap 红线), [ADR-0073](../../decisions/core/0073-cms8s-adc-real-register-map-supersedes-ssot.md) (CMS8S ADC 真实图与 0 周期即时), [ADR-0076](../../decisions/core/0076-mcs51-sim-backends-native-vs-iss-channel-roadmap.md) (双后端路线与 A/B 类缺口) |
| **创建日期** | 2026-09-08 |
| **适用对象** | 仿真引擎架构师、系统设计者、CI/HIL 自动化测试工程师、嵌入式固件高级开发者 |
| **所属模块** | `wink-micro-os` / `frameworks/mcs51/` / `UniSim` / `unisim-scenarios` |
| **关联文档** | [用户代码限制手册](2026-08-27-mcs51-user-code-compatibility-and-limitations-guide.md)<br>[时序面：时钟域与时序一致性规格书](2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md)<br>[UniSim 仿真一致性与高保真规范](../../design/04-wasm-simulation/04-assurance/01-consistency-spec.md) |

---

## 1. 为什么需要本文档（定位与职责分工）

在 WinkMicroOS 的 MCS-51 仿真生态中，存在两类**性质完全不同、受众各异的限制与规范**：

```
                    ┌──────────────────────────────────────────────────────────┐
                    │                      8051 兼容性体系                      │
                    └─────────────────────────────┬────────────────────────────┘
                                                  │
                  ┌───────────────────────────────┴───────────────────────────────┐
                  ▼                                                               ▼
  ┌───────────────────────────────┐                               ┌───────────────────────────────┐
  │ ① 用户代码限制 (User-Facing)   │                               │ ② 仿真与真机一致性及测试限制   │
  │ (Coding Restrictions)         │                               │ (Fidelity & Test Limits)      │
  ├───────────────────────────────┤                               ├───────────────────────────────┤
  │ 关注点：C 代码怎么写能通过编译 │                               │ 关注点：代码完全同源正确，但  │
  │ 受众：业务固件工程师、AI Agent│                               │ 仿真与真机在时序/并发表现不同 │
  │ 载体：user-code-compatibility-│                               │ 受众：架构师、仿真开发者、    │
  │      and-limitations-guide.md │                               │       CI/HIL 测试工程师       │
  │ 违规后果：编译报错、语法不兼容│                               │ 载体：本规格书                │
  └───────────────────────────────┘                               └───────────────────────────────┘
```

- **《用户代码限制手册》**关注的是**代码本身的合规性**（如严禁 K&R C 语法、严禁读 `PSW.CY` 进位标志、严禁裸指针解引用等）。
- **本文档**关注的是：**当用户代码 100% 合规、且原厂代码“一行不改”直接编译运行时，虚拟仿真世界与物理硅片在微架构、时钟流逝、外设状态机推进上的客观差异，以及基于此差异构建自动化场景测试断言的正确方法论。**

---

## 2. 仿真与物理硅片的三大本质机制差异

### 2.1 外设模型：0 周期即时转换 vs. 硬件物理转换耗时

以中微 **CMS8S78xx 片内 12-bit ADC**（[ADR-0073](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/decisions/core/0073-cms8s-adc-real-register-map-supersedes-ssot.md)）为例：

```c
// 原厂标准连续采样主循环
while (1) {
    ADC_GO();                   // 写入 ADCON0.1 启动转换
    while (ADC_IS_BUSY);        // 轮询等待硬件自清零
    adc_result = ADC_GetADCResult();
}
```

| 维度 | 物理硅片 (CMS8S78xx Silicon) | Native 仿真模型 (UniSim cms8s_adc.cpp) |
| :--- | :--- | :--- |
| **时钟源** | $F_{sys} / 256$（例如 24MHz 分频后为 93.75kHz） | 虚拟时钟从时钟（无模拟电路时钟发生器） |
| **转换耗时** | 约 14~16 个 ADC 周期，实测 **$150 \sim 170\mu\text{s}$** | **$0\mu\text{s}$ 即时穿透**（[ADR-0072 D1](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/decisions/core/0072-dual-clock-domain-and-quota-catchup.md) / [ADR-0073 D2](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/decisions/core/0073-cms8s-adc-real-register-map-supersedes-ssot.md)） |
| **CPU 表现** | 在 `while (ADC_IS_BUSY)` 中空转数百个机器周期 | 写 `ADCON0` 钩子内同步完成转换并自清 `ADGO`，`while` 首轮即假退出 |
| **连续采样频率** | 方波周期 $\approx 2 \times 150\mu\text{s} = 300\mu\text{s} \implies$ **~3.3kHz** | 循环周期由 SFR 读写微步耗时累计（~93.4µs） $\implies$ **~10.7kHz** |

> **为什么不能在 Native 仿真下简单增加延时定时器？**
> 详见后文 §3.1。给 Native 即时外设强行增加定时器延时，会引发纯内存轮询死锁、离散事件排序紊乱和浏览器端 Wasm 性能崩塌。

---

### 2.2 中断机制：调用栈内同步嵌套派发 vs. 物理硬件异步抢占

- **物理硅片（硬件异步抢占）**：
  ADC 模拟电路是芯片内的独立硬件模块。当 `ADC_GO()` 触发后，硬件转换在后台独立运行。
  转换结束时，硬件中断逻辑在**任意汇编机器指令边界**强制将当前 PC 压入硬件栈，打断主程序，跳转执行 `ADC_IRQHandler`，执行完毕通过 `RETI` 弹栈返回。
- **Native 仿真（同步借机派发）**：
  在 Native C++ 编译下，中断派发函数 `wink_mcs51_dispatch_vector(VECTOR_ADC)` 是由写寄存器钩子 `on_adcon0_write` **直接同步调用的 C 语言普通函数**。
  这意味着：**`ADC_IRQHandler` 是在 `ADC_GO()` 写操作的当前调用栈内嵌套执行的**。主程序甚至尚未执行到下一行语句，中断函数就已经全部运行完毕。

---

### 2.3 时间推进：微步粗账（5µs/拦截点） vs. 物理指令机器周期自流逝

- **物理硅片**：
  每条 8051 机器指令自然消耗 12-T（1~4 个机器周期）。时间是连续且伴随 CPU 取指执行自动流逝的，即使代码执行纯算术运算（如 `a = b + c;`），物理时间也在准确前进。
- **Native 仿真（ADR-0072 粗账机制）**：
  用户 C 代码被编译为宿主原生指令（x86 或 Wasm），底层**没有 8051 指令流水线**。
  从时钟 `s_virtual_us` 仅在**明确的拦截点**进行离散记账（每访问一次 SFR 或调用一次 `_nop_()` 充 `WINK_MCS51_MICROSTEP_US = 5µs`）。
  纯寄存器/内存运算（如 `int i; i++;`）在虚拟世界中**消耗 0µs 时间**。

---

## 3. 典型时序陷阱与竞态场景深剖 (Deep Dive)

### 3.1 纯内存忙等导致的“虚拟时间冻结”死锁（最致命隐患）

小家电固件中常见的另一类等待方式是通过全局变量标志位：
```c
volatile uint8_t g_adc_done = 0;

void ADC_IRQHandler(void) interrupt 19 {
    g_adc_done = 1;
    ADC_ClearIntFlag();
}

int main(void) {
    ADC_Config();
    g_adc_done = 0;
    ADC_GO();
    while (!g_adc_done); // 等待 ISR 置位
    // ... 后续处理
}
```

* **真机运行**：`while(!g_adc_done)` 在硬件中执行死循环，耗时 $150\mu\text{s}$ 后硬件中断打断 CPU，置位 `g_adc_done = 1`，循环正常跳出。
* **仿真陷阱**：
  - 若保持当前 **0 周期即时转换**：`ADC_GO()` 内同步调用 ISR，`g_adc_done` 在写语句内就已置 1，随后的 `while(!g_adc_done)` 首轮即跳出，**测试通过**。
  - **若错误地为 ADC 引入了异步延时定时器**：由于 `while(!g_adc_done)` 访问的是纯内存变量（非 SFR、无 `_nop_`），**虚拟时间推进量恒为 0µs**！定时器永远无法等到超时，主 Fiber 陷入无限死循环，最终触发宿主 WCET 超时假死。

---

### 3.2 启动后清理标志位的时间先后陷阱

某些固件工程师可能会写出存在逻辑缺陷的代码：
```c
ADC_GO();
g_adc_done = 0;          // ❌ 错误习惯：在启动后才清零标志位
while (!g_adc_done);
```
- **真机表现**：硬件 ADC 转换需要 $150\mu\text{s}$，而执行 `g_adc_done = 0` 仅需 $0.5\mu\text{s}$。中断在 $150\mu\text{s}$ 之后才到来，因此该 Bug 在真机上被“物理延迟”侥幸掩盖，程序看似正常工作。
- **仿真表现**：`ADC_GO()` 瞬间同步执行 ISR，将 `g_adc_done` 置为 1；随后紧接着的 `g_adc_done = 0` 将其清零；程序在 `while(!g_adc_done)` 处**永久死锁**。

> **架构启示**：仿真环境中的同步调用反而暴露了用户代码原有的时序竞态漏洞。

---

### 3.3 采样率相关的控制算法失真（PID / 数字滤波）

若连续采样的 ADC 数据被送入数字信号处理（DSP）、IIR 滤波或 PID 算法：
- 离散时间算法的系数高度依赖采样周期 $T_s$（如积分项 $K_i \cdot T_s$、微分项 $K_d / T_s$）。
- 物理真机上由于分频等待，采样频率约为 $f_s \approx 3.3\text{ kHz}$；
- 仿真环境下由于 0 周期穿透加微步累计，采样频率约为 $f_s \approx 10.7\text{ kHz}$；
- **影响**：控制回路在仿真中的等效增益会发生畸变，在仿真中调优的滤波截止频率或 PID 参数在真机上可能表现出响应迟钝或过冲发散。

---

### 3.4 硬件分频寄存器（ADC_CLK_DIV_*）改动不敏感

CMS8S78xx 的 `ADCON1` 允许配置 `ADC_CLK_DIV_2` 到 `ADC_CLK_DIV_256`（相差 128 倍速度）。
- **真机表现**：改为 `DIV_2` 后，物理转换率提升至近百 kHz；
- **仿真表现**：Native 模型 [cms8s_adc.cpp](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/cms8s_adc.cpp) 专注于 0 周期状态机，未模拟分频计时，连续采样的循环节拍完全由代码本身的微步数量决定，频率不会因分频配置改变而改变。

---

## 4. 自动化场景测试（Scenario）编写与断言方法论

在编写自动化无头场景测试（如 `unisim-scenarios/*.scenario.json`）时，必须遵守以下测试设计准则：

### 4.1 核心原则：断言链路活性与数据正确性，严禁强校验微步频率

#### ❌ 错误示范：耦合仿真微步耗时的魔法值断言
```json
{
  "type": "ASSERT_WAVEFORM",
  "pin": 26,
  "expected": {
    "kind": "frequencyHz",
    "value": { "$near": { "target": 10000, "tolerance": 4000 } }
  },
  "description": "断言翻转频率接近 10kHz"
}
```
* **缺陷**：把仿真内部基于微步累计出的 ~10.7kHz 当成了物理标准。如果同一套测试运行在真机或 HIL 平台（频率为 3.3kHz），该用例将直接误报失败；若未来框架微步粒度调整，测试也会脆弱断裂。

#### ✅ 正确示范：宽容活性区间断言（Liveness Band）
```json
{
  "type": "ASSERT_WAVEFORM",
  "pin": 26,
  "expected": {
    "kind": "frequencyHz",
    "value": { "$between": [1000, 50000] }
  },
  "description": "[核心断言] ADC 持续转换链路验证：证明 EOC 中断 vector 19 派发 → ISR 翻转 P32 链路闭环。频率落在 [1kHz, 50kHz] 活性区间（真机物理 ~3.3kHz，仿真模型微步节拍 ~10.7kHz 均在区间内；若无中断则频率为 0 断言失败）"
}
```
* **收益**：
  1. 准确表达了测试意图：验证“ADC 持续采样、EOC 中断派发、ISR 翻转引脚”全流程是否活跃运转；
  2. 若中断丢失或死锁，引脚频率为 0Hz，测试精准失败；
  3. 兼容真机物理分频（3.3kHz~6.6kHz）与仿真执行节拍（~10.7kHz），用例可在仿真与真机测试台完全同源运行。

---

### 4.2 场景测试的三维正交断言矩阵

一个完备的即时外设场景测试，应当建立多维正交断言，而非仅依赖单一的频率指标：

```
                           ┌───────────────────────────┐
                           │   ADC 场景三维断言矩阵     │
                           └─────────────┬─────────────┘
                                         │
            ┌────────────────────────────┼────────────────────────────┐
            ▼                            ▼                            ▼
  ┌───────────────────┐        ┌───────────────────┐        ┌───────────────────┐
  │ ① 引脚活性断言    │        │ ② 模拟码值精度断言 │        │ ③ 中断状态闭环断言│
  │ (Waveform Active) │        │ (Data Accuracy)   │        │ (Interrupt State) │
  ├───────────────────┤        ├───────────────────┤        ├───────────────────┤
  │ 断言 P32 频率处于 │        │ 注入 AN0 = 0.5V， │        │ 验证 EIF2.ADCIF   │
  │ [1kHz, 50kHz] 范围│        │ 断言 adc_result   │        │ 被 ISR 正常清零， │
  │ 证明无死锁、持续翻转│        │ 落在 [2000, 2100] │        │ 无中断重入/标志挂死│
  └───────────────────┘        └───────────────────┘        └───────────────────┘
```

---

## 5. 保真度边界与双后端演进路线（ADR-0076 对齐）

根据 [ADR-0076](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/decisions/core/0076-mcs51-sim-backends-native-vs-iss-channel-roadmap.md)（mcs51 仿真双后端策略），系统在架构上明确划定了两套后端的分工：

| 评估维度 | Native 功能级后端 (当前默认 / 主线) | ISS 指令级后端 (按需触发 / 规划中) |
| :--- | :--- | :--- |
| **执行机制** | Keil C 作为 C++17 native 编译 (x86/Wasm) | SDCC 编译为 8051 机器码，轻量解释器执行 |
| **外设模型** | **0 周期即时状态机 (Trap 四红线)** | 周期精确离散事件模型 |
| **时钟保真度** | 粗账微步推进 (5µs 粒度) | **12-T / 机器周期 1:1 绝对保真** |
| **中断响应** | 同步借机派发 (C 函数调用) | **硬件级指令边界异步压栈抢占** |
| **纯内存轮询** | 不推进时间，存在死锁风险 | **天然推进机器周期，完美支持** |
| **性能表现** | **极高 (近原生速度，Wasm 60fps 跑多 MCU)** | 较重 (解释执行，速度慢 1~2 个数量级) |
| **适用业务** | **小家电温控、按键交互、UI 显示、AI 低代码** | **WS2812 亚微秒脉冲、手写汇编、闭环电机 FOC** |

### 架构结论
在当前 Native 功能级主后端下，**维持 0 周期即时模型并解除断言的硬编码耦合是唯一正确的工程选择**。只有当业务明确要求亚微秒级精确时序时，才应当依据 ADR-0076 切换至 ISS 后端，绝不可在 Native 框架内打脆弱的硬件延时补丁。
