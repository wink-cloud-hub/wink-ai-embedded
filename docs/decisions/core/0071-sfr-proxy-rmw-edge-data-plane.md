# ADR-0071：8051 SFR 影子代理数据面——WinkSfr 代理、diff 边沿分发与 Read-Latch 隔离

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-08-29）** |
| **日期** | 2026-08-27（Accepted 2026-08-29） |
| **触发** | MCS-51 拦截层数据面设计：用户代码对位/整端口 SFR 的读写（`sbit LED = P2^0; LED=1;`、`P1 |= 0x04;`、`P1 = 0x55;`）必须在不改动源码的前提下，精确驱动 UniSim 引脚与外设陷阱；原方案 A2 存在编译断裂、Trap 漏发、RMW 语义倒挂三大缺陷（见数据面规格书 §1）。 |
| **影响范围** | `wink-micro-os/frameworks/mcs51/include/mcs51_proxy.hpp`、`src/mcs51_sfr.cpp`；`js_pal_gpio_write` 通道 1 即时通知；所有挂载 Level 2 陷阱的外设（ADC0832/CMS8S）。 |
| **决策者** | 项目 Owner（AD-12/AD-16/AD-18） |
| **关联 ADR** | [ADR-0070](0070-mcs51-zero-code-simulation-interception-layer.md)（umbrella）、[ADR-0004](0004-static-dispatch-vs-runtime-ops.md)（POD 静态分发）、[ADR-0035](0035-arduino-compat-polymorphism-sandbox.md)（C++ 沙箱） |
| **关联计划** | [`2026-08-27-mcs51-zero-code-simulation-plan.md`](../../implementation-plans/core/2026-08-27-mcs51-zero-code-simulation-plan.md)（M1 基础代理 / M4 数据面补完） |
| **关联技术设计** | `docs/tech-designs/mcs51/2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md`（数据面 SSOT） |

---

## 1. 背景（Context）

8051 准双向口的电气微结构决定了两条黄金定律：

1. **Read-Latch vs Read-Pin**：`P1 |= 0x04` 这类读-改-写（RMW）指令在真实硅片上读的是**端口锁存器**而非物理引脚；若仿真让 RMW 读物理引脚，输入引脚外部电平会被回写锁存器，导致下拉 FET 永久锁死（数据面规格书 §2 有灾难场景推演）。
2. **边沿才是事件**：整端口赋值 `P1 = 0x55` 只应对真正发生跳变的位触发外部通知/陷阱；对未变动位重复触发会造成外设状态机误动作（虚假边沿）。

原方案三缺陷：① `sbit` 在纯 C 下文件作用域非常量初始化编译断裂；② 整端口 RMW 不触发位陷阱；③ RMW 读引脚导致语义倒挂。

## 2. 方案比选（Options）

| 方案 | 描述 | 优 | 劣 | 结论 |
|---|---|---|---|---|
| A. 纯宏 + 影子数组 | `#define P1 (*(volatile uint8_t*)0x90)` 式宏 | 简单 | 无法挂位级/整端口钩子；RMW 无法区分读源；sbit 无法实现 | ❌ |
| B. C++ 代理类（WinkSfr / WinkSfrBitProxy） | 操作符重载承接全部读写语义 | 零侵入承接 `^`/`=`/`|=`/`++` 等全部语法；钩子精确 | 需 C++ 模式（AD-4 已定）；须严守 constexpr 构造/inline 规则 | ✅ **采纳** |
| C. 编译期代码生成改写 | codegen 把用户 SFR 访问改写为 API 调用 | 显式 | 破坏零侵入、正则不可靠 | ❌ |

## 3. 决策结论（Decision）

### D1. 影子内存与双代理结构（AD-12）
- 全局 `s_sfr_shadow[256]`（POD、零初始化 BSS）承载全部 SFR 状态。
- `WinkSfr`（整端口/字节）持有 `sfr_addr` 与 `port_idx`（0~3 = P0~P3，0xFF = 控制 SFR）；`WinkSfrBitProxy`（位）持有 `sfr_addr/port_idx/bit_idx/bit_mask`，由 `WinkSfr::operator^(bit)` 透传真实物理地址生成（杜绝 TCON/SCON 地址算错）。
- 两级外设同步：Level 1 普通 GPIO 跳变即时 `js_pal_gpio_write` + 每 tick `sync_in()` 批量拉取；Level 2 注册陷阱的引脚经 POD 函数指针表 `s_pin_traps[port][bit]` 在写入当周期立即跳转。

### D2. diff 边沿感知分发（Zero False-Trigger）
- 整字节写：`diff = old ^ val`；`diff == 0` 走快路径（0 触发）；仅对 `diff` 置位位调用 `on_write`。
- 位写：仅当 `old_bit != new_bit` 才通知。
- 读端口（Read-Pin）：动态调用各 bit 的 `on_read` 重构字节电平；**RMW 复合赋值（`|= &= ^= += -= ++ -- <<= >>=`）严格读锁存器影子**，严禁读引脚。

### D3. 线性引脚映射与即时通知（AD-18）
- P0~P3 共 32 引脚单调映射 `global_pin = (port << 3) | bit`（Pin 0~31）。
- 位/字节跳变瞬间立即 `js_pal_gpio_write(global_pin, new_level)` 同步 PinArbiter，前端动画零延迟；用户业务代码继续用原生 `sbit`/`P1`，零感知。

### D4. 可变 SFR 的常量初始化纪律（AD-16）
- **严禁**将 SFR 实体声明为 `constexpr`（赋予顶层只读属性，`P1 = 0x55` 编译报错）。
- 由 `WinkSfr` 的 **constexpr 构造函数**保证对象在装载期完成静态 Constant Initialization（早于任何动态 ctor、零运行时开销、零初始化顺序 fiasco），对象本身保持可写。
- C++17 用 `inline WinkSfr`（P0386R2 inline 变量消除 ODR 重复定义）；C++14 降级为 `static` 内部链接（规格书 §5.2 双方案）。
- 一律花括号初始化 `{addr, port}`，禁小括号歧义；单参数构造兼容厂商 SFR 声明。

### D5. 完整操作符代数
- 位代理：`operator=(uint8_t)`（写）、`operator uint8_t()`（读，GPIO 走 on_read、控制 SFR 先走 read_hook 推进时钟）。
- 整端口：`operator=`、`operator uint8_t()`、`|= &= ^= += -=`、前置/后置 `++ --`、`<<= >>=`、拷贝赋值全部覆盖，RMW 一律经锁存器。

## 4. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| 用户 SFR 语法零改动即获得精确边沿/陷阱语义 | 代理类必须在 C++17（或 C++14 降级）下编译；MSVC/GCC/Clang 操作符行为需 Spike-S2 验证 |
| Zero False-Trigger：未变动位 0 触发，同值写快路径 | 陷阱表为固定 32×2 POD，新增外设须按静态分发表注册（ADR-0004，无虚表） |
| Read-Latch 隔离杜绝准双向口 FET 锁死类灾难 | 所有 RMW 操作符必须走锁存器影子，新增操作符时须评审 |
| constexpr 构造 + inline 变量：零 ctor 开销、零 fiasco | 禁 constexpr 修饰对象、禁小括号初始化，须 lint/评审把关 |

**测试约束**（数据面规格书 §7）：`test_sfr_rmw_latch_integrity`（锁存完整性）、`test_sfr_edge_dispatch_accuracy`（边沿分发精度，4 套 Zero False-Trigger 向量含 ADC0832 同端口共存）、`test_sfr_operators_coverage`（操作符覆盖率）、用户态 `sfr_rmw_isolation_test.c`。

## 5. 遵循与后续（Compliance & Follow-up）

- M1 落地基础读写代理；M4 补完 diff 边沿/on_read 重构/hooks 与全部操作符。
- Accepted 后随 ADR-0070 一并回写 Layer-①。

**验收证据（2026-08-29）**：SFR 影子 + WinkSfr/WinkSbit 代理、diff 边沿分发、Read-Latch vs Read-Pin 隔离经 M4 数据面单测（RMW latch 完整性、edge dispatch 零虚假触发、操作符代数全覆盖）与 ADC0832/CMS8S e2e 在 host 18/18、wasm 7/7 全绿验证；M6 iron_ntc 闭环中继电器 P1.0 锁存读回（`sfr_shadow[0x90]&0x01`）与 ADC0832 3 线 bit-bang 的 sbit 翻转即数据面正确性的端到端证明；arch lint 无发现。

---

*本 ADR 状态变更请在此记录：*
- 2026-08-27：Proposed（随 MCS-51 拦截层方案提交；M4 数据面测试全绿后随 umbrella 转 Accepted）
- 2026-08-29：Proposed → Accepted（M4 数据面补完并经 M0–M6 三端测试矩阵验证——host 18/18、wasm 7/7、RMW 零虚假边沿单测全绿、M6 闭环端到端读回正确；随 umbrella ADR-0070 一并回写 Layer-①）。
