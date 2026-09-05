# ADR-0076：mcs51 仿真双后端（native 功能级代理 vs ISS cycle 级）与通道对接路线——按小家电域频率定优先级

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-08-30）** |
| **日期** | 2026-08-30 |
| **触发** | ADR-0075 阶段 0 把 mcs51 接入生产 wasm 并 headless 实证通道-1（GPIO 双向）后，需回答落地关键问题：**在完全不修改用户 Keil 源码的前提下，其余通道/外设能否仿真、边界在哪**。逐通道分析（Layer-① §2.4）暴露两类性质不同的缺口：一类是"模型未写"（同后端可补），一类是"信息在 native 编译时已销毁"（调时钟参数无解）。本 ADR 据此定后端策略与通道路线优先级。 |
| **影响范围** | `wink-micro-os/frameworks/mcs51/`（native 代理：边沿时间线、外部计数、`_nop_` 重标定、软/硬 UART、INT0/1、I2C/SPI 从机、模拟绑定——Stage 2/3）；可选新增 ISS cycle 后端（**本 ADR 不实现，仅定策略与触发条件**）；Layer-① `02-wink-micro-os/07-mcs51-simulation-interception.md`（回写后端边界与路线）。 |
| 决策者 | 项目 Owner |
| **关联 ADR** | [ADR-0070](0070-mcs51-zero-code-simulation-interception-layer.md)（umbrella）、[ADR-0072](0072-dual-clock-domain-and-quota-catchup.md)（双时钟域/功能 µs）、[ADR-0073](0073-cms8s-adc-real-register-map-supersedes-ssot.md)（0 周期即时外设）、[ADR-0074](0074-mcs51-channel1-external-read-pin.md)（通道-1 双向数字面）、[ADR-0075](0075-mcs51-production-wasm-target-headless.md)（生产 wasm + headless 阶段 0） |
| **关联设计规范** | [`02-wink-micro-os/07-mcs51-simulation-interception.md`](../../zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md) §2.4（通道覆盖 × 零修改可行性矩阵）、[`04-wasm-simulation/02-mechanisms/08-channel-routing.md`](../../zh/design/04-wasm-simulation/02-mechanisms/08-channel-routing.md)（四通道分类法，MCU 无关） |

---

## 1. 背景（Context）

mcs51 拦截层把**未修改 Keil C51 源码当 C++17 native 编译**（x86/wasm），经 SFR/sbit 代理 + trap 表 + 虚拟钟在 UniSim 中功能级运行。阶段 0 已证通道-1 GPIO 双向（`js_pal_gpio_write` / `js_pal_gpio_read_state`）。逐通道"零修改能否仿真"分析得出：仿真所需的**五个原语已全部就绪**，其中"从机往固件读脚注数据位"已由 ADC0832 的 `on_read` 陷阱（`mcs51_adc0832.cpp:165`）生产验证——I2C/SPI 从机、UART RX、外部中断都是同一 bit-bang 从机模板的不同状态机。

但缺口分两类，根因不同：

- **A 类——模型缺失**：机制原语都在，只是某个外设/通道的框架侧模型还没写。同 native 后端补模型即可，零改用户码。
- **B 类——信息在编译时销毁**：native 编译把 Keil C 编成 x86/wasm 指令，运行时**不存在 8051 指令流**，因此三样东西在编译那一刻即丢失：① 每条 C 语句真实耗几个 12-T 机器周期；② PSW 标志位（CY/OV/AC，native 算术设的是 x86 flags）；③ 内联汇编里的 8051 助记符（native 编译器不认识，现 STRICT assert）。凡语义依赖这三者的，**调时钟参数（微步大小、量子、`_nop_` 计费）补不回来**——信息已不存在。

当前时钟模型（ADR-0072）佐证该边界：虚拟钟 `s_virtual_us` 只在拦截点推进、每拦截点固定充 `WINK_MCS51_MICROSTEP_US=5µs`（`wink_mcs51_clock.h:39`），时间 ∝ 被拦截事件数而非真实指令周期；纯寄存器算术（无 SFR/`_nop_`/XDATA）充 ~0 时间；外部世界只在配额片边界（`QUOTA_US=10ms`）yield 时推进。定时器溢出时刻按 period 精确计算（µs 级、事件驱动），但固件 bit-bang 翻转的边沿时间落在 5µs 粗账上。

## 2. 方案比选（Options）

| 方案 | 描述 | 优 | 劣 | 结论 |
|---|---|---|---|---|
| A. 只做 native 代理，B 类声明不支持 | 把 A 类模型补全；WS2812/摄像头/汇编驱动等 B 类永久判 ❌ | 零新后端、成本最低 | 遇真实 RGB 灯效/汇编厂商驱动即卡死，无升级路径 | ❌（无兜底） |
| B. 立即上 ISS cycle 后端替换 native | 不做 native 增强，直接上 8051 指令集仿真器 | 精度最高，B 类全解 | ISS 慢/重、工程量大；为低频需求阻塞高频主线；多数功能级场景不需要周期精度 | ❌（过度投入） |
| C. **双后端共存：native 功能级为主（补 A 类），ISS cycle 级为可选第二后端（按需触发）** | native 覆盖压倒性多数外设；ISS 作为时序关键场景的高精度兜底，两后端共用同一 `js_pal_*` PinArbiter 数据面与插件 | 主线快、零改用户码；B 类有明确升级路径而非死路；按域频分配投入 | 需长期维护两套时钟/拦截机制；后端选择需策略 | ✅ **采纳** |

## 3. 决策结论（Decision）

### D1. 双后端：native 功能级代理为主，ISS cycle 级为可选第二后端（Planned）

- **native C++ 代理 = 默认/主后端**（现状，ADR-0070/0071/0072/0074/0075）。未修改 Keil C 当 C++17 编，SFR/sbit 代理 + trap + 功能虚拟钟。快、轻、零改用户码，覆盖电平逻辑/字节事务/百 µs~ms 级定时。
- **ISS cycle 后端 = 可选高精度后端，本 ADR 不实现**。把同一未修改 `.c` 用 **SDCC**（开源 8051 工具链，复用现有 cleanup/shim，符合"不改源码"；不依赖 Keil 在环）编成真实 8051 opcode 后解释执行：每条指令周期数已知（12T，1–4 机器周期），SFR 落在 ISS 内存图上可 trap，GPIO 翻转带精确周期时间戳。一次解决全部 B 类（亚 µs NRZ、高速软 UART、精确指令周期/中断响应延迟、PSW、内联汇编）。8051 ISS 核小、有公开成熟实现可集成——**是有限工程量，非研究风险**；但属并行后端，更慢更重。
- **两后端共用同一数据面**：都经 `js_pal_gpio_*` / `js_pal_adc_read_norm` /（未来）`js_pal_i2c/spi/uart_*` 接同一 PinArbiter 与插件，仅时钟源与拦截机制不同。后端选择**按 app / 按协议**（device-tree/板级配置声明），对插件与前端透明。

### D2. 缺口分两类：A 类（native 可补，零改用户码）vs B 类（需 ISS）

- **A 类 = 模型缺失**，同 native 后端补模型即落地：
  - **定时边沿注入队列**（最高杠杆）：插件把带虚拟时间戳的边沿时间线压给某脚，`on_read` 陷阱按 `wink_mcs51_virtual_us()` 求值"此刻该脚电平"（ADC0832 已证 `on_read` 能片内回驱，扩展为按虚拟钟调度）。解：外部世界片内冻结（现仅 10ms 片边界更新）→ 支撑 DHT11/22 单总线、NEC 红外解码、超声波 ECHO、软 UART RX。
  - **外部脉冲计数 C/T**：`TMOD` C/T=1（T0/P3.4、T1/P3.5）现 idle + STRICT 告警；改为注入边沿每上升沿 +1 TLx/THx，溢出路径已存在。
  - **`_nop_()` 重标定 + 量子细化**：`_nop_` 从充 5µs 改为 ~1µs（真实 1 机器周期 @12MHz），让 bit-bang 延时循环真耗时间，把可分辨时序从 5µs 推向 ~1µs 级。
  - 外加 Layer-① §2.4 已列的 🔧 项：INT0/1 外部边沿→IE0/IE1→派向量 0/2、硬件 UART TX 路由 `js_pal_uart_write` / RX 字节注入 SBUF+RI+向量 4、bit-bang I2C/SPI 从机模型（ADC0832 模板）、模拟量 host 桥接 `arbiter.readAnalog`（Stage 2）。
- **B 类 = 信息编译时已销毁，native 调参无解，归 ISS**：亚 µs 单脚周期时间编码（WS2812 T0H≈0.4µs/T1H≈0.8µs、摄像头）、高速软 UART（115200，8.7µs/bit 接近量子极限）、精确指令周期/中断响应延迟、PSW 进位标志依赖（多字节手写汇编 ADDC/DAA）、内联汇编/12T 周期。

### D3. 域频率判断（小家电域）：B 类低频，A 类覆盖压倒性多数 → 优先做 A 类，ISS 按需触发

针对真实小家电固件（电饭煲/电磁炉/风扇/热水器/空炸/豆浆机/破壁机；主控多为 8051 核 CMS8S/N76E003/STC8/SH79F 或廉价 M0）逐项判断 B 类出现频率：

| B 类项 | 小家电频率 | 依据 |
|---|---|---|
| WS2812 亚 µs RGB 灯带 | **低（主流加热控制类）/ 中（个护装饰·智能小件氛围灯）** | 主流显示用数码管 + 指示灯（直驱扫描或 TM1650/TM1638/SM1668 低速 2 线驱动），不用可寻址 RGB；WS2812 集中在照明/玩具/部分智能风扇·净化器灯效，偏消费电子 |
| 摄像头 | **≈0（8051 本体）** | 带宽/RAM 不可能；带摄像头的智能产品摄像头在独立 Linux/RTOS 模组 |
| 高速软 UART（115200+） | **极低** | 小家电串口几乎全 9600（接 ESP WiFi 模组/显示板/调试）；115200 一般走硬件 UART（字节级，属 🔧/A 类非 B） |
| 精确指令周期/亚 µs bit-bang | **低** | 真实实时时序很粗：可控硅过零触发 = 半周期 10ms@50Hz；数码管扫描 = 百 µs~ms；NEC 红外解码 = 560/1690µs；DHT11 = 26~70µs——全在几十 µs~ms，A 类够得到 |
| PSW 进位依赖 | **极低** | 仅手写多字节汇编算术靠 CY；固件几乎全 C，多字节运算由编译器生成、不读 PSW SFR |
| 内联汇编 | **低，集中在时序紧驱动** | 多为 `_nop_()` 精确延时（已拦截）；真 `#pragma asm` 块仅极少数软 UART/flash 时序/WS2812 驱动；原厂 StdDriver 是 C（CMS8S adc.c 已证未修改编译） |

两个"看似 B、实为 A"的高频点：**38kHz 红外载波**（半周期 13µs ≫ 量子，`_nop_` 重标定后 bit-bang 可做，载波容忍度高；NEC 解码走边沿时间线）与 **DHT11/22 单总线**（最小 26µs ≫ 量子）——均归 A 类边缘。

**对低代码/AI 生成场景再打折**：AI 生成 app 是纯 C + 标准外设/驱动库，不写内联汇编、不手搓周期级延时，走 GPIO/ADC/定时器/标准协议抽象（正 ✅/🔧 区）；WS2812 等即使需要也应在平台侧做成"声明式灯带组件"（device-tree 声明 N 灯 + 专用解码），而非让 AI 生成亚 µs bit-bang。

**结论**：小家电物理世界（加热/电机/按键/测温/50Hz 市电）天生 ms 级，B 类不是落地拦路项。**优先级：先把 A 类做扎实（Stage 2/3）；ISS cycle 后端作为战略保险/小众兜底，等出现具体 WS2812 灯效产品 / 摄像头 / 内联汇编厂商驱动需求再投，不为假想需求阻塞主线。**

### D4. WS2812 的 native 实验路径（非通用，过渡）

native 下 `_nop_()` 每步计费使 T1H（多 nop）相对 T0H（少 nop）虚拟高电平时间成比例变长（单调、放大）。配合 device-tree 声明"X 脚 = WS2812、N 灯"，解码器**不靠绝对 µs、只分短/长脉冲**，对**纯 C、nop 填充、padding 余量足**的驱动可能解码。局限：余量依赖具体驱动 nop 数差（差 1 条 nop = 1 量子，不稳）；大量真实 8051 WS2812 驱动恰恰用内联汇编卡周期（native 直接 assert 跑不起来）。故 native 仅覆盖"声明过的纯 C 子集"，**不通用、不保证**；通用解留 ISS（D1）。

## 4. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| 落地边界清晰：A 类零改用户码 native 可补，B 类有 ISS 升级路径而非死路 | 长期维护两套时钟/拦截机制（native 功能钟 vs ISS 周期钟）；后端选择需 device-tree/板级配置声明 |
| 按域频分配投入：小家电高频 A 类先行，ISS 不阻塞主线 | ISS 落地需引 SDCC 工具链 + 8051 解释器核 + SFR trap 内存图对接，工程量在触发时单估 |
| A 类"定时边沿注入队列"一项同时解锁 DHT/红外/超声波/软 UART RX，杠杆高 | `_nop_` 重标定到 1µs 会改变全局时间推进速率，需回归现有定时器/延时测试（delay_ms 经定时器/`wink_mcs51_delay_ms` 不受影响，纯 nop 延时变长） |
| 两后端共用 `js_pal_*` 数据面，插件/前端/ PinArbiter 不感知后端 | 纯算术空转延时（无 nop/SFR）在 native 仍充 ~0，无法建模；此类写法归"不支持/建议用定时器"，不因此上 ISS |

**红线不变**：ESP32 零 mcs51 增量（mcs51 树仍 `if(ESP_PLATFORM) return()`）；无 `-fpermissive`；无硬编码 GBK 输入字符集；RMW 复合赋值永不读外部脚；`#ifdef SIMULATION` 在最底层；vendor 夹具 `docs/vendors/` 只读不提交；A 类模型遵守 trap 四红线（零延时/禁 yield/纯状态机/永不推进虚拟时间——边沿时间线是"读时按虚拟钟求值"，不主动推进钟）。

## 5. 遵循与后续（Compliance & Follow-up）

- Accepted 后立即回写 Layer-① `07-mcs51-simulation-interception.md`：§2.4 矩阵的 ⚠️/❌ 项标注 A/B 分类与本 ADR；新增 §2.5「时钟保真模型与后端边界」（功能 µs 粗账的 9 项缺陷、A/B 分类、双后端策略、域频率结论）。
- Stage 2/3 路线（A 类，另开实施计划）：① 定时边沿注入队列（DHT/红外/超声波/软 UART RX 共用）② 外部计数 C/T ③ `_nop_` 重标定 + 量子细化 ④ INT0/1、硬件 UART TX/RX、I2C/SPI 从机、模拟绑定（部分已在 §2.4 🔧）。
- ISS cycle 后端：**不在近期路线**；触发条件 = 出现具体 WS2812/RGB 灯效产品需求、摄像头、或必须未修改编译的内联汇编厂商驱动。触发后另写 ADR 定 SDCC 工具链与 ISS 核选型。

---

*本 ADR 状态变更请在此记录：*
- 2026-08-30：Proposed（逐通道零修改可行性分析 → A/B 两类缺口根因 → 双后端策略；小家电域频率判断 B 类低频、A 类优先、ISS 按需兜底；待 Owner Accepted 后回写 Layer-① §2.4/新增 §2.5）。
- 2026-08-30：Accepted（Owner 拍板：双后端策略 + A 类 Stage 2/3 路线 + ISS 按需触发条件成立；已回写 Layer-① §2.4 A/B 标注与新增 §2.5 时钟保真模型与后端边界；Stage 2 活通道实施计划另立 Layer ③）。
