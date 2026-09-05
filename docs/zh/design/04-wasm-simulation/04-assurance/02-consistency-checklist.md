# 仿真一致性场景清单（状态矩阵）

| 项 | 内容 |
|---|---|
| 文档层级 | ① 设计规范（UniSim 3.0 / assurance） |
| 文档状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| SSOT 职责 | 「某场景**现在能不能验**」；✅/🟡/❌/🚫/**—** **唯一出处** |
| 上次核对 | 2026-08-02 |
| 管辖 ADR | 状态变更规程见 [`03`](./03-roadmap-and-governance.md)；场景契约见 [`01`](./01-consistency-spec.md) |
| 迁自 | `04-wasm-simulation-2.0/12-consistency-checklist.md` |

> 本文件是 [`01`](./01-consistency-spec.md) 的可读性索引：每行一个子项，只存**现状、残余缺口、验证入口、链回契约**。禁止复述五字段/算法/保障方案正文。
>
> **勿与机制落地混淆**：✅/🟡/❌/🚫/— = 场景可测性；Landed/Partial/… = 引擎机制是否交付（见根 [`00-README.md` §3.2](../00-README.md)、[`03` §1.1](./03-roadmap-and-governance.md)）。

---

## 0. 阅读约定

### 0.1 支持程度

| 标记 | 含义 |
|---|---|
| ✅ **已支持** | 可验证场景核心正确性；已知限制写在「残余缺口」 |
| 🟡 **部分支持** | 覆盖子集或仅近似；真机仍可能逃逸 |
| ❌ **不支持** | 当前基本不可验；依赖后续 Phase 或真机 |
| 🚫 **刻意不保** | 在产品边界外；仅近似或不做 |
| — **N/A** | 当前产品未暴露该 API/路径 |

**一格一符**：现状列只放**一个**主状态符。产品边界（🚫）或未暴露（—）优先；残余 stub/近似可验度写进「残余缺口」，禁止 `🚫/🟡` 这类双符。

### 0.2 验证入口列（证据指针，非方案正文）

| 填法 | 约定 |
|---|---|
| 命令 / 测试 ID | 指向可复现入口（如 `python wink-tools/wink.py test …`、lint rule、单测名）；**不写**断言逻辑 |
| `待补` | 尚无绑定入口。**新翻 ✅ 禁止**留 `待补`（见 [`03` §4](./03-roadmap-and-governance.md)）。历史 ✅ 仍为 `待补` 者视为**证据债**，不得对外声称「可审计已验」 |
| `N/A` | 主状态为 🚫 或 — |

### 0.3 解法类型（大类总览用；细则只在 `01`）

| 类型 | 含义 |
|---|---|
| **A** | 约束写法（lint / 编译期 / JSON 门禁） |
| **B** | 引擎建模 |
| **C** | 观测门禁 |
| **真机** | HIL / 真机兜底 |

### 0.4 文档指针

- 五字段模板与 C1~C25 契约正文 → [`01`](./01-consistency-spec.md)
- 生产口径（完备 ≠ 恒等）→ [`../01-overview/03-production-contract.md`](../01-overview/03-production-contract.md)
- 机制落地成熟度 → 根 [`00-README.md` §3.2](../00-README.md)、[`03` §1.1](./03-roadmap-and-governance.md)
- Phase / CI / ✅ 翻转 / golden 治理 → [`03`](./03-roadmap-and-governance.md)

---

## 1. 大类总览矩阵（C1~C25）

| ID | 一致性大类 | 现状 | 主解法 | Phase/口径 | 优先级 |
|---|---|---|---|---|---|
| [C1](#c1) | 业务因果/状态机 | ✅ | B(+A) | 基线 | 高 |
| [C2](#c2) | 虚拟微秒逻辑时序 | ✅ | B | Phase 1 | 高 |
| [C3](#c3) | 共享状态竞态 | ❌ | B+C | Phase 4 | **最高** |
| [C4](#c4) | 临界区与中断抢占 | 🟡 | B+C | Phase 4 | **最高** |
| [C5](#c5) | 阻塞/饿死/WDT | 🟡 | A+B+C | Phase 4 前置 | 高 |
| [C6](#c6) | 栈/堆/内存安全 | ✅ | A+C | Phase 2 | 高 |
| [C7](#c7) | 总线协议/CRC | 🟡 | B+A | Phase 3 | 中高 |
| [C8](#c8) | DMA/异步传输窗口 | ❌ | B | Phase 3 | 中(驱动高) |
| [C9](#c9) | 多核 SMP | ❌ | B 近似/真机 | ADR-0014 | 高 |
| [C10](#c10) | 快环 ISR/FOC | 🟡 | B 近似/HIL | ADR-0047 | 中高 |
| [C11](#c11) | 电气/模拟 | 🚫 | B 查表近似 | ADR-0003 | 视产品 |
| [C12](#c12) | CPU/ABI 指令级 | ❌ | C 双轨 | Phase 5 | 低(难查) |
| [C13](#c13) | 生命周期/复位 | 🟡 | B+C | Phase 1 补强 | **最高** |
| [C14](#c14) | 快进/Co-Sim 步进 | 🟡 | B+C | Phase 1+ | **最高** |
| [C15](#c15) | Host↔Wasm 边界 | 🟡 | A+C | 基线 | **最高** |
| [C16](#c16) | OS 同步原语语义 | 🟡 | B+A | Phase 4 前置 | **最高** |
| [C17](#c17) | 外设资源互斥/时基 | 🟡 | A+C | 持续 | 高 |
| [C18](#c18) | 总线故障态机 | ❌ | B | Phase 3 扩展 | 中高 |
| [C19](#c19) | DMA/缓冲生命周期 | ❌ | B+C | Phase 3 扩展 | 中(驱动高) |
| [C20](#c20) | 回调重入/下半部 | 🟡 | A+C | Phase 4 | 高 |
| [C21](#c21) | 时间与计数回绕 | 🟡 | A+C | 持续 | 高 |
| [C22](#c22) | 电源/低功耗/时钟域 | 🚫 | 真机 | 非目标为主 | 中 |
| [C23](#c23) | 持久化/NVS | 🟡 | B | 按需 | 中 |
| [C24](#c24) | 缓存/DMA RAM | 🚫 | 真机/C12 | 非目标为主 | 中 |
| [C25](#c25) | 浮点/数值 UB | 🟡 | C | Phase 2 | 中 |

> 主解法统一口径：C20=A+C、C21=A+C（与 [`01`](./01-consistency-spec.md) 对齐）。

---

## 2. 关键子场景（现状 · 缺口 · 验证入口 · 链到 `01`）

> 下列 **102** 子项与 [`01`](./01-consistency-spec.md) 的 `#### C*.*` 一一对应；状态变更只改本表。Wave 4C 迁入时为 98；本波增补 C1.5 / C2.5 / C13.5 / C21.4。

<a id="c1"></a>

### C1 — 业务因果/状态机

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C1.1 同源 App/BAL 状态迁移 | ✅ | — | 待补 | [01 §C1.1](./01-consistency-spec.md#c1.1) |
| C1.2 DAL Bypass / `#ifdef SIMULATION` 收窄 | 🟡 | 残留整层 `#ifdef`（超声波 deprecated 捷径），见 [08 §bypass](../02-mechanisms/08-channel-routing.md) | 待补 | [01 §C1.2](./01-consistency-spec.md#c1.2) |
| C1.3 故障/超时/断线异常路径 | ✅ | 覆盖面随器件扩展 | 待补 | [01 §C1.3](./01-consistency-spec.md#c1.3) |
| C1.4 幂等恢复与重试风暴 | 🟡 | 缺统一命令计数预言 | 待补 | [01 §C1.4](./01-consistency-spec.md#c1.4) |
| C1.5 `wink_status_t` 错误码跨 target 对齐 | 🟡 | 缺统一故障类对照表 + 每码单测 | 待补 | [01 §C1.5](./01-consistency-spec.md#c1.5) |

<a id="c2"></a>

### C2 — 虚拟微秒逻辑时序

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C2.1 sleep/定时唤醒快进 | ✅ | 非晶振漂移 | 待补 | [01 §C2.1](./01-consistency-spec.md#c2.1) |
| C2.2 脉宽测量零 Yield 环回 | ✅ | 与 C14.2 交叠（快进漏边沿） | 待补 | [01 §C2.2](./01-consistency-spec.md#c2.2) |
| C2.3 去抖/RC 低通锚定虚拟时钟 | ✅ | 噪声为注入参数 | 待补 | [01 §C2.3](./01-consistency-spec.md#c2.3) |
| C2.4 单中断友好采样周期 | 🟡 | 缺受控抖动默认开启 | 待补 | [01 §C2.4](./01-consistency-spec.md#c2.4) |
| C2.5 跨宿主整数/状态轨迹确定性 | ❌ | 缺跨浏览器/引擎 golden 矩阵 | 待补 | [01 §C2.5](./01-consistency-spec.md#c2.5) |

<a id="c3"></a>

### C3 — 共享状态竞态

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C3.1 无锁共享读写（Task↔Task） | ❌ | 待混沌调度 + TSan | 待补 | [01 §C3.1](./01-consistency-spec.md#c3.1) |
| C3.2 Task↔ISR 无锁交叉 | ❌ | 待多点插 ISR | 待补 | [01 §C3.2](./01-consistency-spec.md#c3.2) |
| C3.3 多字段结构体撕裂 | ❌ | 待影子内存字段级 | 待补 | [01 §C3.3](./01-consistency-spec.md#c3.3) |
| C3.4 发布-订阅顺序假设 | ❌ | 弱内存不全在日间轨 | 待补 | [01 §C3.4](./01-consistency-spec.md#c3.4) |

<a id="c4"></a>

### C4 — 临界区与中断抢占

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C4.1 临界区门禁（enter/exit） | 🟡 | 加深断言（已落地 nest-count 补发，见 [04 §IRQ](../02-mechanisms/04-interrupt-model.md)） | 待补 | [01 §C4.1](./01-consistency-spec.md#c4.1) |
| C4.2 调度点 ISR 投递（Poll 模型） | 🟡 | 非任意刺入 | 待补 | [01 §C4.2](./01-consistency-spec.md#c4.2) |
| C4.3 优先级嵌套 | ❌ | Phase 4+/真机 | 待补 | [01 §C4.3](./01-consistency-spec.md#c4.3) |
| C4.4 FromISR / 非 ISR-safe API 误用 | ❌ | 待上下文门禁（ISR 上下文标记已具备，Fault 化待补） | 待补 | [01 §C4.4](./01-consistency-spec.md#c4.4) |
| C4.5 Pending 中断队列溢出 | 🟡 | JS drop-newest/C drop-oldest 已实现；Fail-Loud 计数告警待强化 | 待补 | [01 §C4.5](./01-consistency-spec.md#c4.5) |

<a id="c5"></a>

### C5 — 阻塞/饿死/WDT

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C5.1 STRICT_NONBLOCKING 编译期隐藏阻塞 API | ✅ | 间接调用见 C5.4 | 待补（`-DWINK_STRICT_NONBLOCKING=1`） | [01 §C5.1](./01-consistency-spec.md#c5.1) |
| C5.2 软 WDT（虚拟时间未喂狗） | ❌ | 待实现 | 待补 | [01 §C5.2](./01-consistency-spec.md#c5.2) |
| C5.3 就绪任务饿死 | ❌ | 待实现（RR 当前过于公平） | 待补 | [01 §C5.3](./01-consistency-spec.md#c5.3) |
| C5.4 动态间接阻塞 | 🟡 | 运行时门禁不全 | 待补 | [01 §C5.4](./01-consistency-spec.md#c5.4) |
| C5.5 优先级反转 | ❌ | 继承未承诺 | 待补 | [01 §C5.5](./01-consistency-spec.md#c5.5) |

<a id="c6"></a>

### C6 — 栈/堆/内存安全

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C6.1 静态堆配额耗尽 | ✅ | ADR-0045 固定堆链接标志**当前 CMake 未检出（待落地）**；碎片几何≠真机 | 待补 | [01 §C6.1](./01-consistency-spec.md#c6.1) |
| C6.2 堆碎片化 | 🟡 | 压力模式可选 | 待补 | [01 §C6.2](./01-consistency-spec.md#c6.2) |
| C6.3 ASan/UBSan（UAF/越界/未对齐/溢出） | ✅ | Wasm 日间不全开 | `python wink-tools/wink.py test`（ASan Pass；见 ADR-0045） | [01 §C6.3](./01-consistency-spec.md#c6.3) |
| C6.4 App 禁裸 malloc | ✅ | — | `python wink-tools/wink.py lint --pack memory`（或 `memory.yaml`） | [01 §C6.4](./01-consistency-spec.md#c6.4) |
| C6.5 Per-task 栈溢出 | 🟡 | 与 FreeRTOS 栈模型有差 | 待补 | [01 §C6.5](./01-consistency-spec.md#c6.5) |
| C6.6 缓冲区在 DMA/异步传输中被复用 | ❌ | 继承大类；依赖 C8/C19 | 待补 | [01 §C6.6](./01-consistency-spec.md#c6.6) |

<a id="c7"></a>

### C7 — 总线协议/CRC

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C7.1 同源协议帧与 CRC | 🟡 | 残留 Bypass | 待补 | [01 §C7.1](./01-consistency-spec.md#c7.1) |
| C7.2 ACK 超时与重试 | 🟡 | — | 待补 | [01 §C7.2](./01-consistency-spec.md#c7.2) |
| C7.3 JSON 语义仿真门禁 | ✅ | — | 待补（ADR-0040 JSON 门禁） | [01 §C7.3](./01-consistency-spec.md#c7.3) |
| C7.4 残留 Bypass 清零审计 | 🟡 | 持续清零 | 待补 | [01 §C7.4](./01-consistency-spec.md#c7.4) |

<a id="c8"></a>

### C8 — DMA/异步传输窗口

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C8.1 粗粒度传输耗时挂起 | ❌ | 同步 API 残留（当前总线同步返回） | 待补 | [01 §C8.1](./01-consistency-spec.md#c8.1) |
| C8.2 完成中断与任务唤醒序 | ❌ | 依赖 C8.1 | 待补 | [01 §C8.2](./01-consistency-spec.md#c8.2) |
| C8.3 同步 API 残留 | 🟡 | 需标注迁移 | 待补 | [01 §C8.3](./01-consistency-spec.md#c8.3) |

<a id="c9"></a>

### C9 — 多核 SMP

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C9.1 单虚拟核产品边界 | 🚫 | 日间不验真多核（ADR-0014）；近似见 C9.2 | N/A | [01 §C9.1](./01-consistency-spec.md#c9.1) |
| C9.2 混沌交错近似多核竞态 | ❌ | Phase 4 | 待补 | [01 §C9.2](./01-consistency-spec.md#c9.2) |

<a id="c10"></a>

### C10 — 快环 ISR/FOC

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C10.1 虚拟时间软步进近似 | 🟡 | 非硬实时；ADR-0047 | 待补 | [01 §C10.1](./01-consistency-spec.md#c10.1) |
| C10.2 PWM–ADC 硬件同步降级 | 🚫 | HIL | N/A | [01 §C10.2](./01-consistency-spec.md#c10.2) |
| C10.3 DI/ISR 分层边界 | ✅ | lint 白名单已立；覆盖面随产品演进（残余近似感） | 待补（`wink lint` DI/ISR 规则） | [01 §C10.3](./01-consistency-spec.md#c10.3) |

<a id="c11"></a>

### C11 — 电气/模拟

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C11.1 SPICE/电源完整性 | 🚫 | — | N/A | [01 §C11.1](./01-consistency-spec.md#c11.1) |
| C11.2 退化引擎查表近似 | ✅ | [06](../02-mechanisms/06-physical-degradation.md) | 待补 | [01 §C11.2](./01-consistency-spec.md#c11.2) |
| C11.3 ADC 量化/参考电压/管脚电容 | 🚫 | 粗量化可选，默认不覆盖 | N/A | [01 §C11.3](./01-consistency-spec.md#c11.3) |

<a id="c12"></a>

### C12 — CPU/ABI 指令级

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C12.1 日间极速轨道（Wasm/Host 原生） | ✅ | 非指令级 | 待补 | [01 §C12.1](./01-consistency-spec.md#c12.1) |
| C12.2 夜间 RISC-V32/真机二进制双轨 | ❌ | Phase 5 | 待补 | [01 §C12.2](./01-consistency-spec.md#c12.2) |
| C12.3 结构体打包/枚举/位域 | 🟡 | `_Static_assert` 已立，位域仍编译器相关 | 待补 | [01 §C12.3](./01-consistency-spec.md#c12.3) |
| C12.4 端序与未对齐访问 | 🟡 | UBSan 在 host | 待补 | [01 §C12.4](./01-consistency-spec.md#c12.4) |

<a id="c13"></a>

### C13 — 生命周期/复位

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C13.1 冷启动：BSS/静态初值/外设默认电平 | 🟡 | `pal_wasm_reset_physical` 已实现；清单需钉死并测 | 待补 | [01 §C13.1](./01-consistency-spec.md#c13.1) |
| C13.2 热重启/软复位原因 | ❌ | 待模型 | 待补 | [01 §C13.2](./01-consistency-spec.md#c13.2) |
| C13.3 外设 deinit/再 init | 🟡 | 幂等契约待统一 | 待补 | [01 §C13.3](./01-consistency-spec.md#c13.3) |
| C13.4 任务/对象生命周期与 Zombie GC | 🟡 | GC 已实现；依赖 Sanitizer 抓 UAF | 待补 | [01 §C13.4](./01-consistency-spec.md#c13.4) |
| C13.5 长周期循环后句柄/资源计数回零 | ❌ | 缺 soak / watermark 门禁 | 待补 | [01 §C13.5](./01-consistency-spec.md#c13.5) |

<a id="c14"></a>

### C14 — 快进/Co-Sim 步进

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C14.1 时钟单一写入/禁止双重步进 | 🟡 | 单 Gate 已实现（ADR-0042）；CI 守门强化 | 待补 | [01 §C14.1](./01-consistency-spec.md#c14.1) |
| C14.2 快进跨越边沿/半窗 debounce | 🟡 | 全局最小事件时间待固化 | 待补 | [01 §C14.2](./01-consistency-spec.md#c14.2) |
| C14.3 Plant↔OS 锁步漂移 | 🟡 | 禁墙钟 plant；门禁待补 | 待补 | [01 §C14.3](./01-consistency-spec.md#c14.3) |
| C14.4 Pin Event Queue 溢出/丢失 | 🟡 | Fail-Loud 待强化 | 待补 | [01 §C14.4](./01-consistency-spec.md#c14.4) |
| C14.5 观测与注入竞态 | 🟡 | 序契约测试待补 | 待补 | [01 §C14.5](./01-consistency-spec.md#c14.5) |

<a id="c15"></a>

### C15 — Host↔Wasm 边界

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C15.1 Asyncify 挂起契约 | 🟡 | wrapper/`'auto'` 已修；错误覆盖回归要全 | 待补 | [01 §C15.1](./01-consistency-spec.md#c15.1) |
| C15.2 中断 Push→Poll/重入 | ✅ | Push 导出已永久移除 | 待补（符号/导出审计） | [01 §C15.2](./01-consistency-spec.md#c15.2) |
| C15.3 bigint/指针 ABI | ✅ | 第三方插件风险 | 待补 | [01 §C15.3](./01-consistency-spec.md#c15.3) |
| C15.4 语义 Bypass 泄露 | 🟡 | 探针可加强 | 待补 | [01 §C15.4](./01-consistency-spec.md#c15.4) |
| C15.5 Worker 隔离与主线程 starve | ✅ | — | 待补 | [01 §C15.5](./01-consistency-spec.md#c15.5) |

<a id="c16"></a>

### C16 — OS 同步原语语义

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C16.1 Mutex 锁/超时/递归 | 🟡 | 对照表+测试钉死（部分已实现） | 待补 | [01 §C16.1](./01-consistency-spec.md#c16.1) |
| C16.2 Queue/Ringbuf 满与覆盖策略 | 🟡 | 与真机逐项对齐 | 待补 | [01 §C16.2](./01-consistency-spec.md#c16.2) |
| C16.3 阻塞等待与超时唤醒序 | 🟡 | 需钉死胜者 | 待补 | [01 §C16.3](./01-consistency-spec.md#c16.3) |
| C16.4 任务通知/事件组（若暴露） | — | 未暴露则 N/A | N/A | [01 §C16.4](./01-consistency-spec.md#c16.4) |
| C16.5 死锁检测 | ❌ | 可选门禁 | 待补 | [01 §C16.5](./01-consistency-spec.md#c16.5) |

<a id="c17"></a>

### C17 — 外设资源互斥/时基

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C17.1 引脚复用冲突 | 🟡 | codegen 门禁加强 | 待补 | [01 §C17.1](./01-consistency-spec.md#c17.1) |
| C17.2 定时器/PWM 通道独占 | 🟡 | 资源表不全 | 待补 | [01 §C17.2](./01-consistency-spec.md#c17.2) |
| C17.3 APB/外设时钟变更副作用 | 🚫 | 真机 | N/A | [01 §C17.3](./01-consistency-spec.md#c17.3) |
| C17.4 PWM 占空比更新毛刺/相位连续性 | 🟡 | 可选模型 | 待补 | [01 §C17.4](./01-consistency-spec.md#c17.4) |
| C17.5 共享总线所有权（多驱动争用） | 🟡 | 重叠窗口检测 | 待补 | [01 §C17.5](./01-consistency-spec.md#c17.5) |

<a id="c18"></a>

### C18 — 总线故障态机

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C18.1 I2C NACK/clock stretch/总线挂死 | ❌ | Phase 3 扩展 | 待补 | [01 §C18.1](./01-consistency-spec.md#c18.1) |
| C18.2 UART framing/break/FIFO 溢出/idle-line | ❌ | 同上 | 待补 | [01 §C18.2](./01-consistency-spec.md#c18.2) |
| C18.3 SPI 模式/CS 时序/全双工 | ❌ | 同上（mode 参数已具备） | 待补 | [01 §C18.3](./01-consistency-spec.md#c18.3) |
| C18.4 传输中止/半包恢复 | ❌ | 继承大类 | 待补 | [01 §C18.4](./01-consistency-spec.md#c18.4) |
| C18.5 多主仲裁（若产品需要） | 🚫 | 多数产品不做 | N/A | [01 §C18.5](./01-consistency-spec.md#c18.5) |

<a id="c19"></a>

### C19 — DMA/缓冲生命周期

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C19.1 半传输/双缓冲切换 | ❌ | 依赖 C8 | 待补 | [01 §C19.1](./01-consistency-spec.md#c19.1) |
| C19.2 传输中缓冲区复用 | ❌ | 同上 | 待补 | [01 §C19.2](./01-consistency-spec.md#c19.2) |
| C19.3 Abort 后描述符/句柄残留 | ❌ | 继承大类 | 待补 | [01 §C19.3](./01-consistency-spec.md#c19.3) |
| C19.4 DMA 可访问内存区（与 C24 交叠） | 🟡 | tag/真机 | 待补 | [01 §C19.4](./01-consistency-spec.md#c19.4) |

<a id="c20"></a>

### C20 — 回调重入/下半部

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C20.1 ISR/回调内调用会 yield 的 DAL | 🟡 | 上下文门禁待 Fault 化 | 待补 | [01 §C20.1](./01-consistency-spec.md#c20.1) |
| C20.2 传感器回调嵌套写执行器 | 🟡 | 队列解耦规约 | 待补 | [01 §C20.2](./01-consistency-spec.md#c20.2) |
| C20.3 Workqueue/延迟下半部顺序 | — | 未暴露则 N/A；暴露后按 🟡 验顺序并改主符 | N/A | [01 §C20.3](./01-consistency-spec.md#c20.3) |

<a id="c21"></a>

### C21 — 时间与计数回绕

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C21.1 uint32 ms/滴答回绕 | 🟡 | 缺专项快进用例 | 待补 | [01 §C21.1](./01-consistency-spec.md#c21.1) |
| C21.2 相对超时跨越快进 | 🟡 | 绝对唤醒已实现，用例补 | 待补 | [01 §C21.2](./01-consistency-spec.md#c21.2) |
| C21.3 序列号/环形下标 wrap | 🟡 | 继承大类 | 待补 | [01 §C21.3](./01-consistency-spec.md#c21.3) |
| C21.4 时间单位换算截断/乘除溢出 | ❌ | 缺边界值单测与统一换算 API 审计 | 待补 | [01 §C21.4](./01-consistency-spec.md#c21.4) |

<a id="c22"></a>

### C22 — 电源/低功耗/时钟域

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C22.1 light/deep sleep 唤醒 | 🚫 | 极简 stub 可选；有 stub 时按 🟡 验唤醒路径 | N/A | [01 §C22.1](./01-consistency-spec.md#c22.1) |
| C22.2 外设时钟门控 | 🚫 | — | N/A | [01 §C22.2](./01-consistency-spec.md#c22.2) |
| C22.3 Brownout/欠压复位 | 🚫 | 复位原因注入见 C13.2 | N/A | [01 §C22.3](./01-consistency-spec.md#c22.3) |

<a id="c23"></a>

### C23 — 持久化/NVS

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C23.1 断电写入撕裂 | 🟡 | 按需注入 | 待补 | [01 §C23.1](./01-consistency-spec.md#c23.1) |
| C23.2 键空间耗尽/满盘 | 🟡 | 继承大类 | 待补 | [01 §C23.2](./01-consistency-spec.md#c23.2) |
| C23.3 仿真"掉电"语义 | 🟡 | Worker destroy=掉电 | 待补 | [01 §C23.3](./01-consistency-spec.md#c23.3) |

<a id="c24"></a>

### C24 — 缓存/DMA RAM

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C24.1 DMA 必须落在可 DMA 区 | 🚫 | 日间弱验见 C19.4 tag；真机/HIL 独占主承诺 | N/A | [01 §C24.1](./01-consistency-spec.md#c24.1) |
| C24.2 缓存一致性假设 | 🚫 | 真机 | N/A | [01 §C24.2](./01-consistency-spec.md#c24.2) |
| C24.3 IRAM/慢径指令位置 | 🟡 | linker 差异文档化 | 待补 | [01 §C24.3](./01-consistency-spec.md#c24.3) |

<a id="c25"></a>

### C25 — 浮点/数值 UB

| 子项 | 现状 | 残余缺口 | 验证入口 | 契约 |
|---|---|---|---|---|
| C25.1 有符号溢出/移位 UB | 🟡 | Host UBSan | 待补 | [01 §C25.1](./01-consistency-spec.md#c25.1) |
| C25.2 NaN/Inf 传播与 flush-to-zero | 🟡 | 控制律策略单测 | 待补 | [01 §C25.2](./01-consistency-spec.md#c25.2) |
| C25.3 浮点确定性 | 🟡 | 公差带比对；ADR-0055 | 待补 | [01 §C25.3](./01-consistency-spec.md#c25.3) |

---

## 3. 场景侧阅读顺序（非排期 SSOT）

> 实施排期以工程计划为准；本节是按嵌入式关注度的阅读顺序。

1. [C14](#c14) + [C15](#c15) — 快进/锁步 + 宿主边界
2. [C13](#c13) + [C16](#c16) — 冷启动 + OS 语义
3. [C5](#c5) — 软 WDT / 饿死（C5.2/C5.3）
4. [C8](#c8) + [C18](#c18)/[C19](#c19) — 异步 DMA + 总线故障 + 缓冲生命周期
5. [C3](#c3) + [C4](#c4) — 竞态 + 临界区/FromISR
6. [C17](#c17) — 引脚/定时器资源冲突
7. [C7](#c7) — Bypass 审计（C7.4）
8. [C12](#c12) — 夜间 ABI 双轨
9. [C9](#c9)/[C10](#c10)/[C11](#c11)/[C22](#c22)/[C24](#c24) — 真机/HIL/🚫 口径

---

## 4. 明确不在仿真一致性承诺内

口径唯一出处：[`../01-overview/03-production-contract.md`](../01-overview/03-production-contract.md)（ADR-0003）。
本矩阵中标 🚫 或「真机/HIL」的子项，**不得**用仿真绿灯替代真机/HIL 放行。

---

## 5. 维护规程

1. 场景支持度变更：**只改本文件**矩阵/子项「现状」「残余缺口」「验证入口」；
2. 问题定义/方案/预言变更：**只改** [`01`](./01-consistency-spec.md)；
3. 新子场景：先在 `01` 写齐五字段（或 🚫 豁免三字段），再在本文件加一行（默认 ❌/🟡，验证入口默认 `待补`）并填契约链；
4. **新翻 ✅**：验证入口必须非 `待补`/`N/A`（细则见 [`03` §4](./03-roadmap-and-governance.md)）；清证据债优先于抬升新场景；
5. 禁止双写技术方案正文；禁止用 Landed/Partial 替代本表状态符；禁止现状列双符。
