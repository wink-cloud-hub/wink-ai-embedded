# MCS-51 仿真层「仿真通过 / 真机出问题」缝隙审计与整改任务清单

| 元数据项 | 说明 |
| :--- | :--- |
| **文档编号** | MCS51-GAP-2026-09-10 |
| **创建日期** | 2026-09-10 |
| **所属模块** | `wink-micro-os/frameworks/mcs51/`、`wink-micro-app/mcs51_*`、UniSim 无头执行链（sister repo `wink-ai/packages/unisim`） |
| **状态** | **Draft / 待评审**（todolist，执行前需迁移为 Layer-② 技术设计或 Layer-③ 实施计划，重大项补 ADR）。2026-09-10 已合并第二轮外部评审（见 §8）与第三轮自查（见 §9）。**阶段 1 热修已落地（2026-09-10，未提交）：GAP-01/22/04/13 + §9.5 审计脚本，31 个 mcs51 host 测试全绿 + 8 应用 22 个无头场景全绿（生产 wasm 重建），见 §10** |
| **审计基线** | master @ e473f35；对照 `docs/vendors/Cmsemicon/CMS8S78xx_DemoCode_V2.0.2`（原厂头文件/StdDriver）、CMS8S78xx 数据手册 V1.0.7、参考手册 V1.1.1 |
| **关联决策** | ADR-0012（契约诚实）、ADR-0070（C++ 拦截层）、ADR-0071（数据面代理）、ADR-0072（双时钟域）、ADR-0073（CMS8S ADC 真实寄存器图）、ADR-0076（Native/ISS 双后端）、ADR-0077（准双向口） |
| **关联文档** | [仿真与硅片保真度及测试方法论](../zh/tech-designs/mcs51/2026-09-08-mcs51-simulation-vs-silicon-fidelity-and-test-limits.md)、[用户代码限制手册](../zh/tech-designs/mcs51/2026-08-27-mcs51-user-code-compatibility-and-limitations-guide.md)、[后端责任划分（C++ Proxy / ISS / 永解不了）](./2026-09-10-mcs51-sim-backend-responsibility-classification.md) |
| **目标受众** | mcs51 框架维护者、仿真引擎开发者、CI/HIL 工程师、AI 代码生成 Agent 维护者 |

---

## 0. 背景与审计范围

对 `frameworks/mcs51`（21 个模型源文件 + 代理头 + 清理脚本 + 构建链路）、`mcs51_health_pot` 全代码与场景、UniSim 无头执行链做了一次全量白盒审计，并与**原厂器件资料逐条核对**，目的是回答一个问题：

> 是否存在「Wasm 仿真场景全绿，但 Keil/SDCC 编译烧录真机后行为错误」的设计缝隙？

结论：**存在**。缝隙分两类：

1. **已显式承认并文档化的差异**（0 周期外设、同步栈内 ISR、5µs 微步粗账、`volatile` 反向失真、Keil L15 静态覆盖、多字节撕裂、RAM/ROM 预算、无波特率波形）——属于 Native 后端的刻意取舍（ADR-0076），本清单不重复立项，仅在 §4 记录补强项。
2. **本次新发现、文档未覆盖的缝隙**——首轮 12 项（P0×3、P1×5、P2×4，GAP-01~12），第二轮外部评审合并后新增 7 项（GAP-13/14/15/17'/19/20/21，其中 GAP-16 并入 GAP-13、GAP-18 并入 GAP-12），第三轮自查新增 4 项（GAP-22~25），合并裁定见 §8、§9。累计 23 项。

> 审计同时确认了一批**做对了、无需改动**的机制：RMW 读锁存器语义、TA 保护窗口回滚、EIF2/T2IF 的 W0C 语义、中断 IE/EA/标志三重仲裁、PS_xx 选择寄存器 0x7F 复位种子、蜂鸣器复用校验与频率公式、原厂 StdDriver 未修改源码的 tier-b 对编译测试。

---

## 1. 问题总表

| 编号 | 优先级 | 后端归属（定义见[后端责任划分](./2026-09-10-mcs51-sim-backend-responsibility-classification.md)） | 标题 | 真机后果 | 影响 health_pot？ |
| :--- | :--- | :--- | :--- | :--- | :--- |
| [GAP-01](#gap-01p0shim-常量错误gpiop13_mux_rxd-0x02--原厂-0x03) | **P0** | A-07 | shim 常量错误：`GPIO_P13_MUX_RXD` 0x02 ≠ 原厂 0x03 | 用该宏的 RXD 应用收不到串口 | 否（硬编码 0x03，仅 TX） |
| [GAP-02](#gap-02p0uart-tx-链路零校验) | **P0** | A-01 + A-03 | UART TX 链路零校验（TR1/波特率/TXD mux/SCON 全不看） | 波特率错/无 TXD 复用 → 无输出或乱码，仿真照发 | 是（前提性风险，见 §5） |
| [GAP-03](#gap-03p0缺少-8051-工具链编译门禁) | **P0** | A-07 | 缺少 8051 工具链编译门禁（用户源码从未被 C51 编译器编译） | C90 方言/容量超限真机编译失败，仿真无感 | 是（未验证） |
| [GAP-04](#gap-04p1ckcon-复位种子错误0x07--0x003-倍定时器偏差) | **P1** | A-06 | CKCON 复位种子错误（硅片 0x07，模型 0x00，3 倍偏差） | 不显式清 T0M/T1M 的应用时序/波特率全错 | 否（应用显式配置） |
| [GAP-05](#gap-05p1adc-参考电压链模拟复用完全不建模) | **P1** | A-02 | ADC 参考电压链 / 模拟 mux / LDO 不参与码值 | NTC 上拉轨≠3.0V 时全温区系统性测温偏差 | **是（前提性风险）** |
| [GAP-06](#gap-06p1config-选项字节不在仿真世界fosc-硬编码-24mhz) | **P1** | A-06 + C-03 | CONFIG 选项字节不建模，Fosc 硬编码 24MHz | 芯片 CONFIG 非 24MHz 路径时 tick 与波特率同比错 | **是（前提性风险）** |
| [GAP-07](#gap-07p1wdt-只验证-ta-序列不验证超时复位ta-窗口过宽容) | **P1** | A-04（依赖 A-03 先行） | WDT 不模拟超时复位；TA 窗口无超时/不被打断 | 真机喂狗不及时复位循环；错误 TA 用法虚假通过 | 低（喂狗周期 10ms，余量充足） |
| [GAP-08](#gap-08p1gpio-方向上下拉驱动强度寄存器不参与行为) | **P1** | A-05 | TRIS/UP/OD/DR/LEDSDR 不参与引脚行为 | 忘配输出方向/上拉 → 继电器不吸合、按键乱触发 | 否（应用配置完整） |
| [GAP-09](#gap-09p2xram-合法窗口-8kb--硅片-1kb) | P2 | A-06 | XRAM 合法窗口 8KB ≠ CMS8S78xx 实际 1KB | 0x0400~0x1FFF 访问仿真合法、真机落入 XSFR | 否（仅用 0x10~0x15） |
| [GAP-10](#gap-10p2生产-wasm-非-strict无头 runner-不按-warning-判失败) | P2 | A-07 | 生产 wasm 非 STRICT，无头 runner 不消费 warning/OOB 计数 | 场景绿色掩盖越界访问与未建模特性调用 | 间接 |
| [GAP-11](#gap-11p2c51-16-位-int--unsigned-char-语义差异未入红线手册) | P2 | A-07（文档）+ B-04（语义根治） | C51 16 位 int / unsigned char 语义差异未文档化 | 依赖回绕/符号/移位的代码两端分叉 | 否（已人工核对） |
| [GAP-12](#gap-12p2杂项-t234-时钟公式重复向量静默覆盖cleanup-注入面过宽w0c-缺口) | P2 | A-08 | T2/3/4 周期不跟 clock_hz；重复向量静默覆盖；cleanup 注入面过宽；P0EXTIF W0C 缺口 | 多类边角行为分叉（详见正文） | 否（仅用 T0） |
| [GAP-13](#gap-13p1第二轮评审复位后硬件时钟未种子化12mhz-兜底与-ckcon-叠加最多-6-倍) | **P1** | A-06 | 复位后硬件时钟未种子化（12MHz 兜底，与 CKCON 叠加最多 6 倍）；buzzer 同源问题并入 | 不写 CLKDIV 就起定时器的固件时序错 | 否（main 首句即配时钟） |
| [GAP-14](#gap-14p2第二轮评审timer0-mode1-软件重载延迟不建模) | P2 | A-08（粗补/文档）+ B-03（精确根治） | Timer0 Mode1 软件重载延迟（每 tick 数 µs 漂移）不建模 | 高精度时间戳/频率测量应用系统性漂移 | 可忽略（最细 100ms） |
| [GAP-15](#gap-15p2第二轮评审整端口写的逐位通知非原子) | P2 | A-08 | 整端口写被拆成 8 个逐位 gpio_write 通知（当前同步不可观测，属隐式假设） | 未来异步插件/总线型外设可能读到中间态 | 否（同步执行） |
| [GAP-17'](#gap-17p2第二轮评审修正stoppd-唤醒源不完整) | P2 | A-08 | STOP 唤醒源模型不完整（仅 INT0/1；缺 GPIO 端口中断/WUT/LSE/LVD/SWE） | 低功耗代码仿真不醒/真机行为不一致 | 否（未用低功耗） |
| [GAP-19](#gap-19p1第二轮评审结构性风险-a场景绿--可烧录的物理前提未显式呈现) | **P1** | A-07 | 结构：场景报告/AI 提示词不呈现"绿色不保证的物理前提" | 用户/AI 误把仿真绿当可烧录背书 | 间接 |
| [GAP-20](#gap-20p2第二轮评审结构性风险-ccleanup-副本溯源与门禁状态不可见) | P2 | A-07 | 结构：被测的是 cleanup 副本，C51 编译状态不在报告中 | 改写器缺陷导致"测的不是真机跑的" | 间接 |
| [GAP-21](#gap-21p2第二轮评审结构性风险-bwasm-每场景重建的生命周期假设无回归钉防) | P2 | A-07 | 结构：跨场景污染当前靠"每场景新 wasm 实例"兜住，无测试钉防 | 未来 runner 复用实例即成 P0 | 否 |
| [GAP-22](#gap-22p1第三轮自查中断语义映射表的休眠错误种子) | **P1** | A-06（含家族门控） | 中断语义映射表 3 处向量错（UART1/I2C/SPI 撞车）+ ADC 优先级 EIP 寄存器错 | 当前无模型 raise 故零信号，将来一做就跳错向量 | 否 |
| [GAP-23](#gap-23p1第三轮自查未建模-sfrxsfr-静默影子无-tripwire实测覆盖-77101-sfr93204-xsfr) | **P1** | A-07 | 未建模 SFR/XSFR 静默落影子、无 unsupported 计数（SFR 77/101、XSFR 93/204） | 用 EPWM/I2C/SPI/ACMP/WUT/Flash 的固件仿真"正常"真机无功能 | 否（health_pot 只用已建模寄存器） |
| [GAP-24](#gap-24p2第三轮自查经典-51-movx-外部总线与-iap-非易失区未建模) | P2 | A-08 | 经典 51 的 MOVX 外部总线占用 P0/P2/P3.6/7；IAP 非易失区不持久 | 把 XBYTE 搬到 at89 carrier 会与 GPIO 冲突假通过 | 否（CMS8S 内部 XRAM） |
| [GAP-25](#gap-25p2第三轮自查合集模拟脚数字读sbuf-重写递归c51-库与栈深) | P2 | A-05 + A-08 + B-04 | 模拟脚仍可数字读；SBUF 发送中重写不报错；递归仿真安全真机踩 overlay；Keil 库/栈面未验证 | 多类 AI 生成代码假通过 | 否（已人工核对） |

> 后端标签定义见[后端责任划分](./2026-09-10-mcs51-sim-backend-responsibility-classification.md)：A=C++ Proxy 可解（同步记账+校验+门禁），B=必须 ISS，C=仿真永解不了（HIL/量产兜底）。各条修复方案首行标签以本表为准，正文不重复展开。 |

---

## 2. P0 问题详项

### GAP-01（P0）shim 常量错误：`GPIO_P13_MUX_RXD` 0x02 ≠ 原厂 0x03

**证据**

- 框架 shim：`wink-micro-os/frameworks/mcs51/include/REG_CMS8S78XX.H:423`
  ```c
  #define GPIO_P13_MUX_RXD                     (0x02)
  ```
- 原厂头文件：`StdDriver/inc/gpio.h:127`
  ```c
  #define  GPIO_P13_MUX_RXD	(0x03)
  ```
- 其余同类宏（P00_MUX_AN0=0x01、P03_MUX_BUZZ=0x05、P14_MUX_TXD=0x03、P22_MUX_TXD=0x03、P21_MUX_RXD=0x03、INT 沿配置 0x01/0x02/0x03）逐一 diff **均一致，仅此一处错误**。
- 受影响的未修改原厂例程（仿真 carrier）：`wink-micro-app/vendor_cms8s78xx_v202/uart0_printf/demo_uart.c:106,140,173,208`、`uart0_rxtx/demo_uart.c:98` 均调用 `GPIO_SET_MUX_MODE(P13CFG, GPIO_P13_MUX_RXD)`。

**真机后果（仿真为何发现不了）**

仿真 UART RX 由 FIFO 直接灌 SBUF（`mcs51_uart.cpp:116-127` 的 `wink_mcs51_uart_rx_push` → `rx_deliver_one`），**不读 RXD 引脚、不校验 P13CFG/PS_RXD**。所以原厂例程在仿真里收发全绿，实际给 P13CFG 写入的 0x02 在硅片上是另一种复用功能，RXD 从未连到引脚 → 真机收不到任何字节。这是一份「仿真验证过的引脚配置 ≠ Keil 烧录后执行的引脚配置」的实锤。

**修复方案**

1. 立即修正宏值为 `(0x03)`。
2. 新增防漂移机制：在 tier-b vendor 对编译测试中增加「shim 宏值 vs 原厂 `gpio.h` 宏值」全量比对（脚本解析两侧 `#define GPIO_*_MUX_*` 做白名单 diff），纳入 CI。
3. 反向增强（与 GAP-02 合并实施）：UART 模型在使能接收（REN=1）时校验 RXD mux + PS_RXD 选择寄存器；TX 校验 TXD mux 与波特率源就绪，未就绪时 STRICT 断言 / Release 单次告警。

**验收标准**

- [ ] `GPIO_P13_MUX_RXD == 0x03`，`vendor_cms8s78xx_v202/uart0_*` 仿真中 P13CFG 影子值为 0x03。
- [ ] 新增的宏值比对测试在 CI 运行，人为篡改任一 mux 宏能使测试失败。

---

### GAP-02（P0）UART TX 链路零校验

**证据**

`wink-micro-os/frameworks/mcs51/src/mcs51_uart.cpp:43-59`：

```cpp
void on_sbuf_write(void) {
    uint8_t b = ...sfr_shadow[SFR_SBUF];
    putchar(...); js_pal_uart_write(0, &b, 1);   // 无条件出总线
    sfr_set_bit(SFR_SCON, SCON_TI);              // 同一调用栈内立即置 TI
    mcs51_raise_irq(IRQ_SOURCE_UART0);
}
```

写 SBUF 路径完全不看：TR1 是否启动、TH1 重载/BRT 配置、T1M/SMOD0/FUNCCR 时钟源、TXD 引脚 mux（P14/P22 CFG=0x03）、SCON 模式（模式 0 是同步移位寄存器，不是 UART）。

> **第二轮评审补充（BRT/TMR2/TMR4 波特率源）**：CMS8S 的 BRT 独立波特率定时器（XSFR `BRTCON@0xF5C0`，自带 BRTCKDIV 分频，**不在 CKCON 中**）在框架内没有任何时间模型——`FUNCCR` 选择 BRT(3)/TMR4(1)/TMR2(2) 后照样即时置 TI，与 Timer1 路径一样不产生任何「波特率源未就绪/配置错误」信号。修复项 1 的就绪检查必须枚举全部 4 种源；未建模源被选中时 STRICT 必须直接报错（当前属于静默假装成功）。

**真机后果**

经典 `SBUF=c; while(!TI); TI=0;` 忙等在真机上依赖波特率发生器逐位发送后置 TI。以下任何一类错误，仿真都照常发出时序正确的字节流、场景全绿，真机分别表现为：

| 错误类型 | 真机表现 |
| :--- | :--- |
| TR1 未启动 / TH1 错 / T1M、SMOD 错 | 永久卡死在 `while(!TI)` 或乱码（波特率偏差） |
| TXD mux 未配置 | 字节发出但引脚无波形，对外无输出 |
| SCON 写成模式 0 | 同步移位寄存器波形，异步接收端全错 |
| FUNCCR 时钟源选错 | 波特率完全偏离 |

health_pot 的遥测场景描述文字已经意识到此风险（`health-pot-uart-telemetry.scenario.json` 中 "without it uart_send() busy-waits on TI forever and the main loop wedges"），但框架层没有任何技术拦截。

**修复方案**

1. 在 `on_sbuf_write` 前增加「TX 就绪」检查函数，按 FUNCCR 选定的波特率源校验：Timer1（TR1=1 且 mode 2 + TH1）/ BRT（BRTCON.BRTEN + BRTDL/H）/ TMR2 / TMR4；校验 SCON 模式 ∈ {1,3}（异步）；校验当前映射 TXD 引脚（P14 默认或 PS/CFG 重映射）的 CFG=0x03。
2. 未就绪：STRICT 断言；Release 保留发送但 `pal_log_w` 单次告警，并通过计数器暴露（配合 GAP-10 进入场景判决）。
3. （可选增强，Native 可承受的保真度）按虚拟波特率延迟置 TI（以 µs 计的整字节发送时间通过 charge_us 记账），取代「即时 TI」，使 `while(!TI)` 类忙等在仿真中也消耗真实发送时间——注意遵守保真度文档 §3.1：延迟必须经过可推进虚拟时间的拦截点，不能引入纯内存轮询死锁；本路径忙等体内有 `_nop_()`（health_pot 即如此），但需在 ADR 层面确认后再做。
4. 红线手册 §4.6 补「UART 仿真不验证波特率/引脚配置」的显式条目与真机 checklist。

**验收标准**

- [x] 新增 STRICT/Release 测试（`test_mcs51_uart_tx_ready` + `_strict`，2026-09-10 落地）：TR1=0 写 SBUF 触发断言/BAUD 计数；模式 0 触发断言/MODE 计数；REN+`PS_RXD`=0x13 指向未复用引脚触发断言/RXD 计数；BRT/TMR2/TMR4 运行位与保留 CKS 全覆盖；health_pot 等价配置零触发（35/35 host 全绿）。
  注：原计划的"TXD mux 缺失触发断言"用例在实施中被修正——P3.1 为硬连线默认脚（手册 §21.2 + 原厂 gpio.h 核实），TXD 在功能层恒就绪，该原因位保留供 GAP-08（TRIS）细化；覆盖改用 RXD 选择器失配用例，见计划 v1.2。
- [ ] health_pot 现有遥测场景在开启校验后仍通过（证明其配置完整）。（待 sister repo 重建生产 wasm 后验证，host 侧等价序列已零触发）

---

### GAP-03（P0）缺少 8051 工具链编译门禁

**证据**

- `wink-micro-app/mcs51_health_pot/CMakeLists.txt:40-62`：非 Emscripten 直接 `set(WINK_APP_SOURCES "")`，不出任何 target；`frameworks/mcs51/CMakeLists.txt:15-20` 在 ESP_PLATFORM 直接 return。
- `mcs51_cleanup.py --target=sdcc` 的代码路径存在（SDCC 方言改写），但全仓库无任何构建/CI 消费其产物（grep 仅命中 wasm 目标的 `--target wasm`，与 mcs51 无关）。
- 结论：`health_pot.c`（及全部 carrier 用户源码）**有史以来只被 C++17 编译过，从未被任何 8051 C 编译器编译过**。

**真机后果**

1. **C 方言**：C++17 接受、Keil C51（C90 子集）拒绝的写法会在真机编译期才暴露，例如：语句中/块后声明（health_pot 已人工核对为「声明置顶」风格，但 AI 生成代码不保证）、`for(uint8_t i;…)`、`//` 与新式注释组合、严格的别名与类型转换差异等。
2. **容量**：FLASH 16KB / DATA 256B / XRAM 1KB（数据手册 §1.3/§2.2.3）超限无任何自动信号；红线手册 §3.5 目前要求人工看 Keil `.map`。
3. cleanup 正则对源码的改写（ISR、main、delay、loop 注入）只保证 C++ 副本可跑，不证明原文件 C51 可编译。

**修复方案（建议单独立 Layer-③ 实施计划）**

1. **Tier-S（SDCC 编译门禁，先行，低成本）**：CI 中对每个 mcs51 carrier 执行 `mcs51_cleanup.py --target=sdcc` → `sdcc -mmcs51 --std-c89 -c`（只编译不链接，或使用 CMS8S 器件本地头占位）。目标是兜住 C 方言、明显的类型/声明问题；SDCC 与 Keil 方言差异（`__interrupt`/`__code`/`__at`）已在 cleanup 中处理。
2. **Tier-K（Keil 门禁，条件具备时）**：Windows CI/本地安装 C51 工具链时，对原厂 Keil 工程与 carrier 执行 `C51.exe + BL51.exe`，解析 `.m51`/`.map` 输出，自动断言 DATA/IDATA/XDATA/CODE 预算并产出报告。
3. 将工具链门禁结果挂到 `wink.py sim run` 的前置检查或独立 `wink.py mcs51 gate` 子命令，场景测试报告同时展示「编译门禁」与「行为场景」两列结果。
4. SDCC 路径下发现的 cleanup 改写缺陷（参见 GAP-12 的 delay 劫持等）一并修复。

**验收标准**

- [ ] 5 个现有 carrier + health_pot + vendor_cms8s78xx_v202 全部通过 SDCC `-std-c89` 编译（或每个失败项有显式 issue 编号）。
- [ ] CI 产物含每个应用的 DATA/XDATA/CODE 用量（SDCC `.mem`/map）与器件预算的对比。
- [ ] 故意引入 C99 中位声明的样例应用能被门禁拦下。

---

## 3. P1 问题详项

### GAP-04（P1）CKCON 复位种子错误（0x07 → 0x00，3 倍定时器偏差）

**证据**

- 参考手册 §8.2.2（PDF 提取核对）：CKCON（0x8E）复位值 = `0000 0111`，即 WTS=000、**T1M=1、T0M=1**，Timer0/1 默认时钟 Fsys/4（1T 模式）。
- 模型侧：`mcs51_context.cpp:38-46` 只播种 P0~P3=0xFF、SP=0x07、PCON=0x00 和 PS_xx 选择寄存器，CKCON 随全零清零 = T0M/T1M=0（Fsys/12）。
- 模型公式本身支持两档：`mcs51_timer.cpp:126-134`（`divider = TnM ? 4 : 12`）——错的只是上电种子。

**真机后果**

不显式配置 T0M/T1M 的应用：Timer0/1 周期与 UART 波特率在仿真与真机之间相差 3 倍。health_pot 因显式执行 `CKCON &= ~0x08`（T0M=0，10ms tick）与 `CKCON |= 0x10`（T1M=1，波特率）而免疫；但这恰恰是「老手写的代码安全、AI 生成的普通代码会踩」的缝隙类型，且与硅片复位语义不一致属于模型硬伤。

**修复方案**

1. `mcs51_context_reset` 增加 CMS8S 种子：`sfr_shadow[0x8E] = 0x07`（注意 health_pot 用 WTS bits[7:5]=010 写 CKCON，种子的 WTS=000 不影响其后续 RMW）。
2. 同步核对其余增强 SFR 的复位值（建议依据参考手册附录的寄存器复位值表全量过一遍，至少：CKCON=0x07、TA=0x00、T34MOD、ADCON1、BUZCON）。
3. 模型默认时钟自洽性：在未写 CLKDIV 前，timer 与 buzzer 的默认 Fsys 应一致——当前 timer 侧兜底 12MHz（`wink_mcs51_clock.h:37`），buzzer 侧兜底 24MHz（`cms8s_buzzer.cpp:44`），应统一为「按 MCU 型号种子 Fosc」（CMS8S78xx=24MHz），与 GAP-06/GAP-13 合并设计。
4. **（结构性建议，第二轮评审后补充）从「逐颗修种子」升级为 per-MCU 复位种子描述符**：CKCON=0x07、Fosc=24MHz、XRAM=1KB（GAP-09）、PCON、BRTCON、ADCON1 等硅片复位事实目前散落在各模型中且已连续错两处（GAP-04、GAP-13）。建议建一张按 `wink-app.json` 的 mcu 字段索引的 seed 表（AT89C52=12MHz/无 XSFR、CMS8S78xx=24MHz/1KB/CKCON=0x07…），由 `mcs51_context_reset` 统一装载，替代全局默认常量——这是两类 MCU 共存且不互相污染的唯一干净方案。

**验收标准**

- [ ] 新增单元测试：reset 后 CKCON==0x07；不配置 CKCON 的 Timer0 16 位定时在 Fsys/4 下周期正确。
- [ ] health_pot 全部既有场景复测无回归（它显式配置 CKCON，不应受影响）。

---

### GAP-05（P1）ADC 参考电压链 / 模拟 mux / LDO 完全不建模

**证据**

- `mcs51_adc.cpp:51-61`：码值 = `norm × 4095`，归一化电压直接当满量程。
- `REG_CMS8S78XX.H` 的 `ADC_EnableLDO / ADC_ConfigADCVref / ADC_EnableLDOOutput`（行 1114-1129）只把 ADCLDO 写入 XSFR 影子；`cms8s_adc.cpp` 读码值时**不检查 LDOEN、不解析 VSEL、不检查 OUTEN**。
- 通道选择（ADCCHS）直接映射注入/拉取轨（`cms8s_adc.cpp:84-91`），不校验对应引脚的模拟 mux（如 AN0 需 P00CFG=0x01）。
- UniSim NTC 插件：`wink-plugin-peripherals/builtin/ntc/1.0.0/src/simulation.ts:111`，`ratio = Rntc/(Rntc+Rpull)`，即归一化电压的参考是**上拉电阻所接的电源轨**，与 ADC 基准无关。

**真机后果**

health_pot 的整张 NTC LUT（`ntc_lut_raw[]`）与 OPEN/SHORT/OVERTEMP 阈值只在「**160k 上拉轨电压 == ADC 内部 3.0V 基准（ADC_VREF_3V + LDO OUTEN 对外供出）**」时成立。若实际板子把上拉接到 VDD（3.3V/5V 小家电主板极常见）：

- 码值按 Vrail/3.0 整体缩放（25°C 锚点 241 → 约 265@3.3V），温度系统性偏差，沸点/保温/干烧判据全部平移；
- NTC 开路钳位电压不再是满量程，`NTC_OPEN_RAW=3900` 可能永远不触发（探针开路失去保护）；
- 忘写 P00CFG=0x01、忘开 LDO 时真机码值为 0/飘移，仿真照样返回精确码值。

仿真中 LUT 与插件物理自洽，所以全场景绿——这是典型的「测试闭环正确、物理前提未验证」。

**修复方案**

1. 码值换算引入基准电压模型：`raw = norm × 4095 × (Vrail/Vref)`，Vref 由 ADCLDO.VSEL + LDOEN 决定（1.2/2.0/2.4/3.0V 枚举已在 shim 中），Vrail 由 device-tree/wink-app.json 新增 ADC 外电路字段声明（如上拉轨=VREF_OUT|VDD|固定电压）。
2. ADEN/LDOEN 未使能、通道引脚 mux ≠ ANx 时：返回 0 或上一结果 + STRICT 断言（至少 Release 告警）。
3. health_pot 板级资产中显式声明「NTC 上拉接 ADC_LDO OUT（3.0V）」，使该前提从隐性变为被断言的配置；真机 BOM/原理图核对项写入应用 DESIGN.md。
4. 红线手册 §4.4 补充 ADC 电气前提（基准、外电路电源轨、模拟 mux、LDO 建立时间）。
5. **（第二轮评审补充）中值滤波在仿真中不可证伪**：`adc_read_filtered()` 的 3 次采样在 0 周期模型下是同一瞬时值，median-of-3 的毛刺剔除能力无法被场景验证；真机 3 次转换间隔约 150µs（DIV_256），但温漂位移量极小（1°C/s 升温率 ×450µs ≈ **0.00045°C**，不构成独立误差项，无需建模时间展宽）。应在保真度文档中明确「数字/模拟滤波的抗扰效果不属于 Native 仿真可验证范围」，滤波类逻辑的验证依赖注入毛刺的单元测试（对 `mcs51_adc_set_value` 做三次不同注入）或 HIL。

**验收标准**

- [ ] 新增单元测试：VSEL=3V 与 Vrail=VDD(3.3V) 配置下同一 ratio 产生不同 raw，且与手算一致。
- [ ] LDO 未使能时启动转换触发 STRICT 断言。
- [ ] health_pot device-tree 含外电路声明字段，缺字段时构建告警。

---

### GAP-06（P1）CONFIG 选项字节不建模，Fosc 硬编码 24MHz

**证据**

- `cms8s_sys.cpp:31`：`constexpr uint32_t CMS8S_FOSC_HZ = 24000000u;`，Fsys 只随 CLKDIV 变化（§4.1 的 Fsys=Fosc/(2·div) 模型正确）。
- 参考手册 §3.6/§4.1：Fosc 还由 **CONFIG 选项字节**（地址 A569H，烧录时写入）决定——HSI 48MHz 的 /1/2/3/6/8 分频、Fixed_Clock8MHz、HSE 等；CONFIG 同时控制 LVR、调试脚等。
- CONFIG 既不在 SFR 也不在 XSFR 空间，运行时代码完全不可见——仿真无从得知烧录配置。

**真机后果**

health_pot 的 10ms tick（T0 重载 0xB1E0）与 9600bps（TH1=217）都按 24MHz 计算。若烧录时 CONFIG 选了其他 Fosc 路径（如 48MHz/2 名义 24MHz 但偏差不同、或 Fixed 8MHz），**节拍与波特率同比错误**，仿真与场景全部绿色。

**修复方案**

1. 在 `wink-app.json` / board 描述中引入 `optionBytes`（或 `foscHz`）显式字段，由构建系统注入模型（`wink_mcs51_set_hardware_clock_hz` 已存在，只需接线 + 复位种子）。
2. 缺省值与器件手册推荐量产 CONFIG 一致（CMS8S78xx 24MHz 路径），缺字段时构建期 INFO 日志明示当前假设。
3. 真机烧录流程（burn-firmware 类 skill/脚本）增加「读取/校验芯片 CONFIG == 工程声明」步骤；health_pot DESIGN.md 写明要求的 CONFIG 字面值。
4. 文档化：Fosc 公差（HSI ±1%）对波特率/计时的影响分析（9600bps 在 ±1% 内可接受，10ms tick 累计误差对 25s/60s 看门狗判据的影响可忽略，但应显式声明）。

**验收标准**

- [ ] wink-app.json 可声明 foscHz，模型 timer/buzzer/baud 全部跟随；声明 8MHz 时遥测场景按 8MHz 重新标定后通过。
- [ ] 烧录 checklist 含 CONFIG 校验项。

---

### GAP-07（P1）WDT 只验证 TA 序列，不验证超时复位；TA 窗口过宽容

**证据**

- `cms8s_sys.cpp:81-88`：WDCON 写仅做 TA 解锁校验，注释自述 "watchdog reset timing is not modelled"。
- 参考手册 §3.4/§8.3：WDT 溢出间隔由 CKCON.WTS<2:0> 选择（2^17~2^26 个 Tsys），WDTRE=1 时溢出产生**芯片复位**；WDTCLR 喂狗。
- TA 窗口模型（`cms8s_sys.cpp:52-64`）：只有「再写 TA」会中止序列；**没有时序过期、也不会被中间的其他 SFR 访问中止**。硅片上 TA 解锁有严格的几条指令窗口，窗口外或被打断即失效。

**真机后果**

- 主循环任何 >0.7s（health_pot WTS=010 → 2^24/24MHz≈0.70s）的阻塞段都会引发真机复位重启；仿真中不存在该后果，WDT 相关逻辑（喂狗节奏、WTS 档位选择）零覆盖。
- 不规范但「碰巧」的 TA 序列（两条 TA 之间插入大量操作）仿真通过，真机被忽略（CLKDIV/WDCON 写不进去）——对 CLKDIV 而言会表现为时钟与预期不符。

**修复方案**

1. WDT 粗粒度模型：记录最近一次 WDTCLR 的虚拟时间与 WTS 档位，在 microstep/catch-up 路径检查溢出 → 触发仿真复位（调用 `mcs51_context_reset` 并重新从 main 进入，或至少 STRICT 断言 + 计数器暴露）。即便不模拟精确的 Tsys 计数，也能验证「最长阻塞段 < WDT 间隔」这一核心安全属性。
2. TA 窗口加上限：记录 0xAA 的虚拟时间戳，超过固定窗口（如手册规定周期数折算）后相位归零；解锁写之前若发生过特定 SFR 写，相位归零（按手册精确语义实现，至少做到「两 TA 之间存在其他 SFR 写即失效」）。
3. 场景层增加「长遥测帧/长忙等不触发 WDT 复位」的活性断言。

> **第二轮评审补充（两条）**：
> 1. **任务依赖耦合**：WDT 模型要能在仿真中抓住「遥测阻塞 → 复位」，前提是 GAP-02 修复项 3（UART TX 按波特率消费虚拟时间）先落地——当前 22 字节遥测在仿真中仅消耗约 22 个微步（~110µs 虚拟，真机 9600bps 下约 23ms），不先给 TX 建模时间，WDT 永远无法因串口忙等触发。实施计划中 GAP-07 必须排在 GAP-02 之后。
> 2. **health_pot 的 WDT 余量是「有条件低」而非无条件低**：23ms ≪ 700ms 在 9600bps/当前帧长下成立，但改 2400bps（~92ms）或加长遥测帧会吃掉余量；「最长阻塞段（含 UART 帧长/波特率）< WTS 间隔」应写入应用 DESIGN.md 的硬约束项，而不是仅靠代码评审记忆。

**验收标准**

- [ ] 喂狗间隔超过 WTS 间隔的测试固件在仿真中被复位/断言；health_pot（10ms 喂狗、最长阻塞 ~23ms 遥测）不触发。
- [ ] 两条 TA 之间插入无关 SFR 写时，受保护寄存器写入被回滚。

---

### GAP-08（P1）GPIO 方向/上下拉/驱动强度寄存器不参与行为

**证据**

- P0TRIS~P3TRIS（0x9A/0xA1-0xA3）、P0UP/P0OD/P0RD/PxDR、LEDSDRP* 在框架内没有任何 hook 注册（grep `mcs51_trap_register_sfr_(read|write)` 列表无这些地址），只作为普通影子字节存在。
- 任何锁存器写都以固定强度通知 PinArbiter：`mcs51_gpio.cpp:99-101,123-125`（写 1 → WEAK，写 0 → SUPPLY），与 TRIS/DR 配置无关。
- 读引脚直接取插件外部电平（`mcs51_gpio.cpp:41-58`），不看输入使能与内部上拉配置。

**真机后果**

- 继电器/LED 引脚忘配 TRIS 输出（硅片复位为准双向、高电平驱动能力弱）：真机不吸合/亮度异常，仿真引脚照常翻转。
- 按键引脚忘开 P0UP：真机浮空随机触发，仿真由 button 插件强驱动永远有效。
- LED 大电流段选的 P3DR/LEDSDR 配置缺失不会在仿真中暴露。

**修复方案**

1. 为 TRIS/UP/OD/DR 建立模型并参与 js 通知：输出方向门控写通知、内部上拉参与 Read-Pin 的缺省电平（HiZ 输入 + 上拉使能时缺省读 1，而不是依赖插件恰好驱动）、开漏模式下写 1 不产生强驱通知。
2. 强度码携带 DR/LEDSDR 配置到 PinArbiter（扩展 `js_pal_gpio_write` strength 语义或新增字段）。
3. STRICT 子模式「引脚配置审计」：扫描被当输出用但从未配置 TRIS 的引脚（可在复位后一次性统计），输出告警清单。

**验收标准**

- [ ] TRIS=输入时写锁存器不产生对外电平通知；UP=1 的 HiZ 输入读回 1。
- [ ] 删掉 health_pot main 中的 P0UP 配置后，仿真出现显式告警（演示防线有效）后恢复。

---

## 4. P2 问题详项

### GAP-09（P2）XRAM 合法窗口 8KB ≠ 硅片 1KB

- **证据**：`frameworks/mcs51/CMakeLists.txt:53` 默认 `WINK_MCS51_XDATA_SIZE=8192`；数据手册 §2.2.3：CMS8S78xx XRAM = **1KB（0x0000~0x03FF）**，0x0400 以上与 XSFR 窗口的关系需按 §2.2.4 地址图处理。
- **后果**：应用 XBYTE 访问 0x0400~0xEFFF 在仿真合法（静默落影子），真机落在未映射区/XSFR 行为未定义。红线手册 §3.5 只给了 4~16KB 通用口径，未按器件收窄。
- **修复**：aperture 改为按 MCU 型号配置（CMS8S78xx=1024，AT89C52 无内部 XRAM 则按外接声明），从 board/wink-app.json 注入 CMake；XRAM 与 XSFR 窗口间的空洞访问应判 OOB。
- **验收**：CMS8S 配置下访问 XBYTE[0x0400] 触发 STRICT 断言。

### GAP-10（P2）生产 wasm 非 STRICT，无头 runner 不按 warning 判失败

- **证据**：`WINK_MCS51_STRICT` 默认 OFF（`CMakeLists.txt:61-66`）；OOB 与未建模特性在 Release 仅 warn-once（`mcs51_xdata.cpp:62-75`、`mcs51_unsupported.cpp:49-63`）；计数器 `wink_mcs51_unsupported_warning_count()` / `wink_mcs51_xdata_oob_count()` 存在但 sister repo `unisim` 无头 runner 不消费（grep `headless-sim-runner.ts` 仅 UART 路由引用）。
- **后果**：场景绿色可能掩盖「应用用了未建模外设/越界访问」——对 AI 生成代码流水线尤其危险。
- **修复**（两条任选其一或并行）：
  1. CI 场景矩阵增加一份 STRICT wasm 构建（同一批场景跑两遍：Release + STRICT）；
  2. runner 在场景结束时 ccall 读取两个计数器，>0 即判 FAIL（header 可加 `strictWarnings: true` 选择项），未建模特性白名单按应用声明。
- **验收**：在测试固件中故意写一次非法 XBYTE，场景运行失败并给出地址。

### GAP-11（P2）C51 16 位 int / unsigned char 语义差异未入红线手册

- **证据**：红线手册 §2/§3 没有整数语义章节。Keil C51：`int`=16 位、plain `char` 默认 **unsigned**；宿主 C++17：int=32 位、char=signed。
- **后果**：三类代码两端分叉（health_pot 已人工核对未使用这些写法）：
  1. 依赖 16 位回绕/溢出的状态与计时；
  2. `char c; if (c & 0x80)` / `c > 0` 的符号判断；
  3. `1 << 15`（51 上 int 16 位为负）、位移超宽行为。
- **修复**：红线手册新增一节，要求跨端代码一律使用 `stdint.h` 定宽类型、禁止依赖裸 `int/char` 宽度与符号；可在 cleanup 阶段加 lint 正则对裸 `char`/`int` 给出 WARNING（噪音较大，先文档后工具）。
- **验收**：手册章节合入；cleanup --lint 模式（新增）能列出裸 char/int 使用点。

### GAP-12（P2）杂项集合

| 子项 | 证据 | 后果 / 修复 |
| :--- | :--- | :--- |
| **T2/3/4 周期公式不跟 clock_hz** | `mcs51_timer.cpp:216-242,409-459,522-572`：us 折算写死 `/2`、`/6`，不乘 1e6/Fsys；T0/T1 公式（126-134 行）则正确使用 Fsys。**第二轮评审后经参考手册复核（T2CON/T34MOD 章），分频语义本身正确**：T2PS `0=Fsys/12、1=Fsys/24`，24MHz 下即每计数 0.5µs（counts/2）/1µs（counts）；T3M/T4M `0=Fsys/12、1=Fsys/4`，24MHz 下即 counts/2 与 **counts/6（=Fsys/4=6MHz 的倒数，并非错误分频）**。缺陷性质明确为「24MHz 硬编码倒数」，随 GAP-13 一并参数化 | 执行 SYS_SET_SYSTEM_CLK 分频后 T2/3/4 时序错误。修复：统一走 counts_to_us 形式，补 T2PS/T3M/T4M 四组合单测 |
| **重复中断向量静默覆盖** | `mcs51_isr.cpp:52-56` 后注册者直接覆盖表项，无告警 | Keil/SDCC 链接期即报重复向量；仿真静默。修复：重复注册时 STRICT 断言/警告并计数 |
| **cleanup 空 superloop 注入面过宽** | `mcs51_cleanup.py:89-91,357-361` 对任意位置（含 ISR 体内）的 `while(1);` 注入 `_nop_()` | 改变的是仿真副本语义（真机 ISR 内死等直接卡死整个系统）；修复：注入仅限函数顶层 main 循环，ISR 内空死循环改为告警 |
| **cleanup delay 本地定义守卫可漏判** | `mcs51_cleanup.py:98-100,364-382`：定义正则只认单行开括号与固定返回类型表，跨行 K&R 花括号、`static void delay_ms(void)` 以外签名会漏判，随后调用点被劫持为 `wink_mcs51_delay_ms` | 仿真跑的不是用户延时实现。修复：改用括号配平的函数体扫描，识别全部合法 C 定义形式 |
| **P0~P3EXTIF W0C 语义缺口** | W0C hook 只注册了 T2IF/EIF2（`mcs51_timer.cpp:883-891`）；端口中断标志寄存器 0xB4~0xB7 走普通存储，`GPIO_ClearIntFlag` 的 `P0EXTIF = 0xFF & ~bit` 在模型中是直接赋值（恰好语义对），但 ISR 用 `|=` 写其他位会误清标志 | 多引脚同时挂起时清标志行为与硅片不一致。修复：0xB4~0xB7 注册同款 W0C hook |
| **TA 窗口内 `_nop_` 数量不校验** | 同 GAP-07 | 与 GAP-07 合并处理 |

---

## 5. health_pot 烧录前人工 Checklist（框架短期无法证明的前提）

以下三项是审计后确认的、**仿真全绿也不能替代**的真机前提，建议进入应用 `docs/DESIGN.md` 与烧录 SOP：

- [ ] **CONFIG 选项字节**：烧录芯片的 CONFIG 必须配置为 HSI 24MHz 路径（Fsys=24MHz），否则 10ms tick 与 9600bps 同比错误（对应 GAP-06）。
- [ ] **NTC 外电路电源轨**：160k 上拉必须接到 ADC LDO 3.0V 基准输出（ADCLDO OUTEN，P 口供电脚），**不能接 VDD**；否则需按 Vrail/3.0 重新标定 LUT 与 `NTC_OPEN_RAW=3900` 钳位阈值（对应 GAP-05）。
- [ ] **Keil 实编译 + map 预算 + WDT 连跑**：Keil C51 编译 0 error；CODE/DATA/XDATA 用量在 16KB/256B/1KB 内；真机长时运行（建议 ≥ 干烧/超时保护最长周期 60s 的数倍）不发生 WDT 复位，遥测持续输出（对应 GAP-03、GAP-07、GAP-02）。

已核对通过、可放心的项：TH1=217/T1M/SMOD→9615bps（0.16%）、T0 0xB1E0→10ms、BUZDIV 频率公式、PS_RXD=0x21、P22/P21 mux=0x03、XRAM 仅用 0x10~0x15、共享变量均 `volatile uint8_t`、ISR 与主循环无共享非可重入函数（无 L15 风险）、继电器 3s dwell 全路径覆盖。

---

## 6. 整改路线建议

| 阶段 | 内容 | 关联条目 | 后端映射（定义见[后端责任划分](./2026-09-10-mcs51-sim-backend-responsibility-classification.md)） | 建议产出 |
| :--- | :--- | :--- | :--- | :--- |
| **阶段 1（热修，1~2 天）** | 修 RXD 宏；**修 IRQ 语义映射表（GAP-22）**；CKCON 复位种子 + **per-MCU reset seed 描述符（含 Fosc 种子，一并消除 GAP-13/16）**；T2/3/4 公式参数化；重复向量告警；P0EXTIF W0C | GAP-01/22/04/13/12 | A-07、A-06、A-08 | 直接 PR + 单测，无需 ADR |
| **阶段 1.5（脚本化门禁，2~3 天）** | **`mcs51_shim_audit.py`：SFR/XSFR 地址、向量表、mux/掩码宏对原厂头文件全量 diff；未建模寄存器白名单生成（GAP-23 tripwire 的输入）；复位值 YAML 比对框架** | §9.5/GAP-23 | A-07 | CI 脚本 + 首份 diff 基线报告 |
| **阶段 2（门禁，3~5 天）** | SDCC `--target=sdcc` 接入 CI 编译门禁（注意核实 SDCC 对扩展向量 19/15/16 的 `__interrupt` 改写覆盖）；STRICT 场景矩阵或 runner 计数判决（**含未建模 XSFR tripwire 计数**）；XRAM aperture 按型号收窄；**每场景实例独立性的回归钉防** | GAP-03/10/09/21/23 | A-07、A-06 | Layer-③ 实施计划 + CI 改动 |
| **阶段 3（模型保真，1~2 周）** | UART 配置就绪校验（4 种波特率源全枚举）+ 可选波特率记账（**必须先于 GAP-07**）；ADC 基准/外电路/mux 模型（**含模拟脚数字读屏蔽**）；GPIO 方向与上下拉；WDT 超时与 TA 窗口收窄；Fosc/CONFIG 声明接线；**STOP 唤醒源补全**；经典 51 MOVX 总线占用；SBUF 重写/递归等 lint | GAP-02/05/08/07/06/17'/24/25 | A-01、A-02、A-03、A-04、A-05、A-06、A-08 | Layer-② 技术设计，涉及时钟语义的补 ADR 并回写设计规范 |
| **阶段 4（文档/lint/呈现，持续）** | 红线手册补 int/char 语义、CONFIG、基准链、UART 前提、Mode1 重载漂移、滤波不可证伪；cleanup lint/注入面收窄与 manifest；**场景报告"物理前提"固定区块 + AI 提示词注入 + 门禁状态上报告** | GAP-11/12/14/19/20 | A-07、A-08、B-03/B-04（文档声明部分） | 文档 PR + cleanup 测试 + sister repo runner 改动 |
| **阶段 5（HIL 兜底，视硬件条件）** | 廉价真机冒烟：利用 health_pot 既有 UART 遥测，真机上电校验帧节奏/按键/加热时序，作为 GAP-02/05/06 类物理前提的最终背书 | GAP-02/05/06 | C-01、C-02、C-03 | HIL 场景规范（同源 scenario 在真机台运行） |

> 流程提醒：按仓库文档流转规则，本 todolist 中的阶段 2/3 执行前应迁移为 `docs/implementation-plans/mcs51/` 下的正式实施计划；涉及时钟/中断语义变更的决定先写 ADR，Accepted 后回写 Layer-① 设计规范与现行技术规格 §保真度边界。

---

## 7. 附：审计方法与参照资料

- 全量通读：`frameworks/mcs51/include|src|tools` 全部文件、`mcs51_health_pot` 源码与 14 个场景、UniSim 生成壳与 NTC 插件源码。
- 交叉核对：
  - 原厂设备头：`docs/vendors/Cmsemicon/CMS8S78xx_DemoCode_V2.0.2/CMS8S78xx_Demo/Libary/Device/CMS8S78xx/Include/cms8s78xx.h`（SFR 地址、向量号、ADCLDO 位定义、CKCON 位定义）；
  - 原厂 StdDriver：`Libary/StdDriver/inc/gpio.h`（mux 宏值）、`src/adc.c`、`src/uart.c`；
  - 数据手册 V1.0.7（XRAM 1KB、CONFIG、时钟树）；参考手册 V1.1.1（CKCON 复位值、WDT 时序与档位、BUZZER 公式 `Fbuz=Fsys/(2·BUZDIV·BUZCKS)`、TA 保护）。
- 已文档化、本清单不重复立项的差异：0 周期外设、同步栈内 ISR、微步粗账、`volatile` 反向失真、L15 静态覆盖、多字节撕裂、RAM/ROM 人工 map 把关、无纳秒级波特率波形——见保真度文档 §2~§4 与红线手册 §5、§8。

---

## 8. 第二轮外部评审合并记录（2026-09-10）

外部评审基于同一基线独立通读后提出 GAP-13~GAP-18 与 3 条结构性风险。经逐条回到代码与参考手册复核，裁定如下（**评审原案 3 处被修正**）：

### 8.1 裁定总表

| 评审提案 | 裁定 | 去向 |
| :--- | :--- | :--- |
| GAP-13 默认时钟 12MHz 与 CMS8S 24MHz 不一致 | ✅ 采纳（事实成立），优先级定 P1；评审"一行零风险热修"建议**否决**（理由见 GAP-13） | GAP-13 |
| GAP-14 Timer0 Mode1 软件重载延迟 | ✅ 采纳，降为 P2（ppm 级漂移，文档+选型建议） | GAP-14 |
| GAP-15 整端口写逐位通知非原子 | ✅ 采纳为 P2 硬化项（当前同步执行、不可观测） | GAP-15 |
| GAP-16 蜂鸣器 24MHz 兜底 | ⚠️ 重复，并入 GAP-13（GAP-04 修复项 3 已含） | GAP-13 |
| GAP-17 "PCON/IDLE/PD 没有建模" | ❌ **原案事实错误**：hook 在 `mcs51_bridge.cpp:59` 注册、`mcs51_pcon.cpp` 完整实现；但复核手册 §5.4.1 发现真缺口——**STOP 唤醒源不完整** | GAP-17' |
| GAP-18 sbit 写 hook 的 old_val 语义 | ⚠️ 非缝隙（实现等价），落为 hook 契约注释 | 并入 GAP-12 |
| §II-A BRT 种子 | ⚠️ 前提修正（BRT 无 CKCON 位，时钟在 BRTCON），真问题并入：BRT/TMR2/TMR4 波特率源全无时间模型 | GAP-02 |
| §II-B median-of-3 时间展宽 | ⚠️ 算术修正（0.00045°C 而非 0.07°C）；保留的真问题是"滤波抗扰不可在 Native 仿真验证" | GAP-05 |
| §II-C WDT 余量依赖波特率/帧长 | ✅ 采纳，并补任务依赖：WDT 建模必须排在 UART TX 记账之后 | GAP-07 + §5 checklist |
| §II-D T3M `/6` 可能是错误分频 | ❌ **误判**：手册 §11.2.1 `T3M/T4M 0=Fsys/12、1=Fsys/4`，24MHz 倒数即 counts/2 与 counts/6；T2PS `0=Fsys/12、1=Fsys/24` 同样吻合。语义正确，仅是硬编码 | GAP-12 行内修正 |
| 风险 A "场景绿=可烧录"隐性承诺 | ✅ **强烈采纳，P1**（本轮最有价值的结构性意见） | GAP-19 |
| 风险 B 跨场景 ISR/状态污染 | ⚠️ 现状安全（每场景独立实例，已核执行链），加回归钉防 | GAP-21 |
| 风险 C cleanup 副本溯源不可见 | ✅ 采纳 P2 | GAP-20 |

### GAP-13（P1，第二轮评审）复位后硬件时钟未种子化（12MHz 兜底，与 CKCON 叠加最多 6 倍）

**证据**

- `wink_mcs51_clock.h:37` 全局默认 `WINK_MCS51_DEFAULT_CLOCK_HZ = 12000000`；生产路径中唯一设置硬件时钟的地方是 `cms8s_sys.cpp:76`（CLKDIV 写 hook）。`mcs51_context_reset` 不种子时钟，`ctx->clock_hz=0` → `wink_mcs51_get_clock_hz()` 回落到 12MHz。
- 时序窗口：reset → 用户 main 首句 `SYS_SET_SYSTEM_CLK` 之间，T0/T1 的 us 换算以 12MHz 计；与 GAP-04（CKCON 影子 0x00，T0M=T1M=0）叠加时，有效计数频率 = 12MHz/12 = 1MHz，而硅片复位态 = 24MHz/4 = **6MHz，偏差 6 倍**（CLKDIV 已写、CKCON 未写窗口为 3 倍）。
- 同源不一致：buzzer 兜底写死 24MHz（`cms8s_buzzer.cpp:44`），同窗口内 timer 按 12MHz、buzzer 按 24MHz（评审 GAP-16，并入本条）。
- 现有 carrier 免疫原因：health_pot 与 vendor 例程都在启动任何定时器前先写 CLKDIV。

**为什么不接受评审的"一行热修"**：该宏是**全局**默认，AT89C52 carrier（`mcs51_uart_hello`/`uart_echo`/`button_led` 的 wink-app.json 均为 `at89c52`，经典 12MHz 器件）依赖它；直接改成 24MHz 会让经典 51 家族时序反错，不是零风险。

**修复方案**

1. 在 GAP-04 修复项 4 的 per-MCU seed 描述符中一并解决：`mcs51_context_reset` 尾部按 MCU 型号调用 `wink_mcs51_set_hardware_clock_hz(fosc_seed)`（CMS8S78xx=24MHz，AT89C52=12MHz）。
2. buzzer 等所有外设统一经同一取值接口，删除分散硬编码。
3. 注意保留 `set_hardware_clock_hz` 不重标定 microstep 计费量子的现有语义（拦截点计数与 Fsys 无关，见 `mcs51_clock.cpp:121-130` 注释）；生产路径从不调用 `set_clock_hz`（F3 动态校准实际休眠），在 seed 工作中顺带核实是否需要激活或显式标注。

**验收**：reset 后未写 CLKDIV 时，CMS8S 配置下 T0 周期按 24MHz 计算；AT89C52 carrier 全部场景无回归。

### GAP-14（P2，第二轮评审）Timer0 Mode1 软件重载延迟不建模

硅片 Mode1（16 位非自动重载）溢出后，从溢出到 ISR 写回 TH0/TL0 之间定时器继续从 0000 计数，每 tick 周期 = 重载周期 + 响应/前导指令延迟（无 `using` 的 C ISR 含 ACC/PSW 等压栈前导，约 5~10µs @24MHz/12T）。模型 `on_overflow` 从计划溢出时刻 `at_us` 零延迟重排（`mcs51_timer.cpp:193`），每 tick 精确。health_pot 最细分辨率 100ms，60s 累计偏差 <0.1%，无实际影响。**处置**：保真度文档补一条；红线手册建议时间精度应用选 Mode2 或 Timer2 自动重载；不为 Native 后端建模该抖动（ISS 后端天然覆盖）。

### GAP-15（P2，第二轮评审）整端口写的逐位通知非原子

`mcs51_gpio_sfr_write`（`mcs51_gpio.cpp:118-132`）把 `P3 = val` 拆成 8 次 `js_pal_gpio_write`。当前所有通知在同一 C 调用栈、同一 `virtual_us` 内同步完成（微步计费在全部通知之后），插件无 await 点，中间态不可观测，4COM 消影序列在现有引擎下安全。**处置**：①在 hook/数据面文档写明"逐位通知顺序 bit0→7、同刻、禁止插件在 writePin 中重入读其他引脚做组合判断"；②中期为并口类外设（seg_display、并行总线）增加批量 byte-strobe 通知 API（一次调用携带整字节 + diff），新老路径并存。

### GAP-17'（P2，第二轮评审修正）STOP（Power-Down）唤醒源不完整

评审原案"PCON 未建模"不成立：`mcs51_bridge.cpp:59` 注册 0x87 写 hook，`mcs51_pcon.cpp` 实现了 IDLE（步进推进到下一事件 / 事件等待）与 PD（事件等待），`mcs51_isr.cpp:205-215` 在中断到达时 post 唤醒事件。**但对照参考手册 §5.4.1，STOP 的合法唤醒源为：① INT0/INT1；② GPIO 端口中断（PxnEICFG，向量 7~10）；③ WUT（LSI 唤醒定时器）；④ LSE；另 §5.1 SWE 位置位时 UART0 RXD 可唤醒、LVD 可唤醒。** 模型只放行了 INT0/INT1（且要求 EA=1），其余源在 PD 中唤不醒 fiber（PD 阻塞于 `wink_event_pend` 期间微步停摆，端口轮询也不运行）。**处置**：P2 补 GPIO 端口中断的 PD 唤醒路径（port_ints 挂起即 post），WUT/LSE/SWE/LVD 在模型实现对应外设前于红线手册标注"不支持 STOP 唤醒"；不允许静默假装能醒。

### GAP-19（P1，第二轮评审结构风险 A）"场景绿 = 可烧录"的物理前提必须主动呈现

**问题**：平台对用户/AI 的实际心智是"场景绿即可烧录"，但本清单证明绿只代表"功能逻辑在仿真物理假设下正确"；GAP-02/05/06 等前提性风险藏在红线手册深处。

**落地三件套**：

1. **场景报告固定区块**：runner 在每个场景摘要下输出「🔇 本结果不覆盖的真机前提」，条目由应用 `wink-app.json` 使用的外设自动生成（如：UART 波特率/引脚配置、ADC 基准与外电路电源轨、CONFIG 时钟、WDT 节奏、RAM/CODE 预算），对应 GAP 编号可追溯。
2. **AI 生成链路注入同一份清单**：代码生成 Agent 的系统提示词/脚手架输出必须包含该应用类型的真机前提 checklist（sister repo frontend/codegen 改动）。
3. **应用模板 README**：mcs51 carrier 模板自带「烧录前确认」段（CONFIG、电源轨、工具链编译、map 预算）。

### GAP-20（P2，第二轮评审结构风险 C）cleanup 副本溯源与门禁状态上报告

被测物是 `mcs51_cleanup.py` 的改写副本而非原文件；改写器一旦漏判（见 GAP-12 的跨行 delay 定义），仿真与真机执行的就是不同代码。**处置**：① cleanup 每次产出 transform manifest（ISR/header/main/loop/delay 改写计数与片段定位），写入构建产物；② 场景报告头部标注「测试基于 cleanup 副本；C51 工具链编译：通过 / 未执行（见 GAP-03）」；③ ③ tier-b vendor 对编译测试与本门禁的职责边界在红线手册 §6 写明。

### GAP-21（P2，第二轮评审结构风险 B）wasm 每场景重建的生命周期假设需回归钉防

经核实当前链路**安全**：CLI 对每个 spec 重新调用 `runSimulationScenario`（`run.command.ts:223`），内部重新 `loadWasmModuleInNode`/instantiate（`headless-sim-runner.ts:276-330`），每场景独立实例 → 静态构造重跑、线性内存全新，ISR 表/BSS 不跨场景继承；`mcs51_context_reset` 保留 ISR 表的设计在该生命周期下正确。**处置**：加一条 runner 级回归测试（连续两场：场 A 注册并派发向量 N，场 B 断言 `isr_dispatch_count==0` 且无场 A 残留引脚电平），把"禁止复用 wasm 实例跑多场景"写成 runner 契约注释；未来若做实例池化，该测试强制先解决 reset 语义。

### 8.2 评审未覆盖、审计方追加的两条

1. **per-MCU 复位种子描述符**（已写入 GAP-04 修复项 4 / GAP-13）：从逐颗打补丁升级为按型号装载硅片复位事实，防止下一颗 MCU 再出种子漂移。
2. **HIL 阶段位**（§6 阶段 5）：利用 health_pot 既有遥测做真机同源冒烟，是物理前提类风险（GAP-02/05/06）唯一不可替代的终检。

---

## 9. 第三轮自查（审计完备性追问后的定向复查，2026-09-10）

在被问及"是否已全面"后，对未逐行文件与未排查类别做定向复查，并实际运行了 shim↔原厂头文件全量 diff。结论：**发现 4 类新缝隙（GAP-22~25），同时证明"全面"不可由人工通读达成，必须脚本化（见 §9.5）**。

### 9.1 全量 diff 的量化结果（证据，而非估计）

对 `cms8s78xx.h`（原厂）与 shim（`REG_CMS8S78XX.H` + `REGX52.H`）解析全部 `sfr`/`xsfr` 声明：

| 面 | 原厂数量 | shim 声明 | 地址不一致 | 未声明（访问路径见 GAP-23） |
| :--- | ---: | ---: | ---: | ---: |
| 直接 SFR | 101 | 77（重叠名） | **0** | 29：I2C(0xF1~0xF7)、SPI SPCR/SPSR/SPDR/SSCR(0xEC~0xEF)、IAP/Flash MCTRL/MDATA/MADR/MLOCK/PCRCD(0xF9~0xFF)、WUT(0xBC/BD)、双 DPTR(DPS/DPL1/DPH1)、乘法器 MADR/MDATA |
| XSFR（MOVX） | 204 | 93 | **0** | 111：EPWM 全套 55、ACMP 14、UID 12、LCD 7、LVD/LSE/TS/SPI 引脚共享 13、引脚压摆率/施密特 PxSR/PxDS 8、BOOTCON、PS_FB0/1 |

关键解读：**凡是声明了的寄存器，地址全部正确**（GAP-01 的 RXD 是宏值错，不在此列）；风险形态是"未声明/未建模寄存器的访问静默"，不是"地址写错"。

### GAP-22（P1，第三轮自查）中断语义映射表的休眠错误种子

**证据**：`mcs51_isr.cpp:22-36` 默认映射表 vs 原厂 `cms8s78xx.h:1178-1198` 向量表：

| 框架语义源 | 框架 vector | 使能位 | 硅片事实 | 问题 |
| :--- | ---: | ---: | :--- | :--- |
| `IRQ_SOURCE_UART1` | 16 | EIE2.1 | CMS8S78xx **无 UART1**（17 号保留） | 与 **TMR4 向量(16) 撞车**，且使能位正是 ET4IE |
| `IRQ_SOURCE_I2C` | 20 | EIE2.5 | I2C 向量 = **21** | 与 **WDT 向量(20)** 撞车 |
| `IRQ_SOURCE_SPI` | 21 | EIE2.6 | SPI 向量 = **22** | 与 **I2C 向量(21)** 撞车 |
| ADC prio_sfr/prio_bit | EIP1(0xB9).4 | — | EIE2(0xAA) 的配对优先级寄存器是 **EIP2(0xBA)**（EIP1 配 EIE1） | 写 EIP2.ADCIP 仲裁读不到，ADC 高优先级永不生效 |

当前无任何模型 raise UART1/I2C/SPI/PWM 源（grep 确认），所以仿真零信号、现有测试全绿；属于"将来谁建模型谁踩错向量"的休眠种子，与 GAP-01 同类。

**修复**：
1. 按原厂表重排语义枚举（`I2C→21、SPI→22、WDT→20`，删除无器件的 UART1 或改按型号注入）；② ADC prio 改 0xBA/4，T3/T4 的 EIP2(0xBA)/T2 的 IP.5 一并复核；③ 用 §9.5 的向量 diff 脚本钉防。
2. **外设模型表按 MCU 门控（第三轮复查追加）**：`g_mcs51_peripherals` 当前对所有型号无条件注册 CMS8S 专属模型（cms8s_adc/buzzer/sys 的 TA/CLKDIV/WDCON hook），at89c52 carrier 运行时 0xDF/0xBE/0xBF/0x8F/0x97 这些在经典 51 上不存在的 SFR 也被拦截。现有 carrier 不触碰故无现象，但属跨型号污染，修复时应随 per-MCU seed 描述符一并按型号选择模型集合（AT89C52 只装 timer/uart/extint + ADC0832 外部模型）。
**验收**：shim/映射表与原厂向量逐源一致；单测：raise I2C 派发 vector 21、EIP2.4 置位时 ADC 先于同级源。

### GAP-23（P1，第三轮自查）未建模 SFR/XSFR 静默影子，无 tripwire

**证据**：GAP-12 只覆盖 Timer mode3/外部计数两个 unsupported id（`wink_mcs51_strict.h` 枚举止于 10）。实测未声明寄存器 29+111 个（§9.1）。访问路径有三种，危险度不同：

1. 用户代码写**未声明名**（`PWMCON = …`）：C++ 编译直接报错——**诚实失败，可接受**。
2. 用户经 `XBYTE[0xF120]` 等绝对地址访问：落入 64KB 影子静默成功（在 XSFR 窗口 [0xF000,0x10000) 内，OOB 不触发）——**危险假通过**：EPWM 应用仿真里"配置成功"但无波形。
3. 厂商源码自带的 `#define PWMCON *(volatile unsigned char xdata *)0xF120`：cleanup 抹掉 `xdata` 后是宿主野指针，崩在编译/运行（不优雅但不假通过）。

**修复**：
1. 在 XDATA 影子写路径增加 **XSFR 地址 tripwire**：0xF000~0xFFFF 中未被任何模型注册的地址，首次写/读计入新计数器 `wink_mcs51_unmodeled_xsfr_*`（STRICT 断言，Release warn-once），取代静默；按地址段给出外设名提示（EPWM/I2C/SPI/ACMP/IAP/WUT…）。
2. 用 §9.5 diff 自动产出"未建模寄存器清单"作为 STRICT 白名单输入：已声明模型拥有的地址放行，其余报警。
3. 红线手册 §4.7 的"未建模清单"从手写 3 条替换为脚本生成的完整列表。

**验收**：写 `XBYTE[0xF120]`（PWMCON）触发 STRICT 断言/Release 计数 +1；health_pot 全部既有 XSFR 访问（PxxCFG/LEDSDR/ADCLDO/PS_RXD）不产生计数。

### GAP-24（P2，第三轮自查）经典 51 MOVX 外部总线与 IAP 非易失区未建模

**证据与机理**：

- AT89C52 无内部 XRAM，任何 `XBYTE[]` 在真机驱动 P0（数据/地址低 8 位复用）、P2（地址高 8 位）、**P3.6=/WR、P3.7=/RD**；仿真中是影子 RAM，不占用也不通知这些引脚。把 health_pot 的 TLM slot 写法移植到 at89 carrier，仿真正常而真机 P2/P3.6/P3.7 作为 GPIO 的功能全部失效（假通过）。CMS8S 有内部 XRAM，health_pot 不受影响。
- 双 DPTR（DPS=0x86、DPL1=0x84、DPH1=0x85）未声明：使用它的块搬运代码编译期报错（诚实失败）。
- 1KB 非易失数据区经 IAP 寄存器（MCTRL@0xFF/MDATA@0xFE/MADR@0xFC-FD/MLOCK@0xFB/PCRCD@0xF9-FA）访问，仿真无持久化模型；IAP 代码在影子上"读写成功"，掉电丢失无感知。

**修复**：①at89 类型号下 XBYTE 访问对 P0/P2/P3.6/7 产生总线占用通知（至少 STRICT 告警 + 与这些引脚 GPIO 写冲突时报错）；②IAP 写序列做粗粒度持久化模型（会话内保持）或明确 unsupported；③红线手册补"经典 51 外部总线占用"章节。
**验收**：at89 carrier 中 `XBYTE[0x1234]=v` 后再把 P3.7 当 GPIO 用，STRICT 报错。

### GAP-25（P2，第三轮自查）合集：模拟脚数字读、SBUF 重写、递归、Keil 库/栈面

| 子项 | 证据 | 真机后果 / 处置 |
| :--- | :--- | :--- |
| 模拟通道脚仍可数字读 | `P00CFG=0x01`（AN0）后无 hook 屏蔽数字路径，`mcs51_gpio_read_pin` 照常读仲裁器 | 硅片上模拟使能脚数字输入无效；误把 AN 脚当按键的代码仿真可用、真机恒定。修复：记录 AN mux 状态，数字读返回锁存/告警（随 GAP-08 一并） |
| SBUF 发送中二次写 | `on_sbuf_write` 无"上一字节未发送完"状态 | 真机损坏移位中的帧；仿真允许。修复：即时 TI 模型下至少 STRICT 对"未清 TI 再写"计数 |
| 递归函数 | C51 非 reentrant + overlay 模型下递归必然自踩局部；宿主原生栈让递归正常 | 红线手册 L15 章节补"递归绝对禁止"；SDCC 门禁（GAP-03）加 `-Wstack-offset`/人工审查项 |
| Keil 库与栈深 | 仿真链接宿主 libc（printf 浮点/%bd 语义不同）；IDATA 栈上限 248B 无界；16/32 位乘除库函数非重入 | GAP-03 的 Keil map 解析中加入 STACK 余量断言；红线手册补库函数重入清单 |

### 9.5 新增强制任务：shim↔原厂资料全量 diff 脚本（CI 门禁）

本轮复查的方法论结论：人工通读无法保证完备（两轮审计后仍找出 GAP-22~25）。把一次性脚本固化为 CI：

1. **SFR/XSFR 地址 diff**（本轮已验证可行，约 40 行 Python）：解析原厂 `cms8s78xx.h` 与 shim 声明，输出：地址不一致（应为 0）、shim-only、vendor-only 三张表；vendor-only 必须出现在 GAP-23 的"已知未建模"白名单里，否则 CI 失败。
2. **中断向量/使能/优先级 diff**：原厂向量表 ↔ `s_default_irq_map` 全字段比对（本轮已人工证明 4 处错）。
3. **mux/位掩码宏 diff**：GAP-01 机制的泛化（`#define GPIO_*`、`*_Msk/_Pos` 全量比对，已发现 1 处）。
4. **复位值 diff（人工辅助）**：以参考手册附录寄存器表为输入建 YAML 种子表，与 `mcs51_context_reset` 装载值比对；首批必须含 CKCON=0x07、PCON=0x00、PS_xx=0x7F、端口 0xFF。
5. 脚本位置建议：`wink-micro-os/frameworks/mcs51/tools/mcs51_shim_audit.py`，输出纳入 GAP-10 的场景判决（差异未白名单即 FAIL）。

### 9.6 审计覆盖度的诚实声明（残余盲区）

即使完成上述脚本，以下面仍不在自动化覆盖内，属于不可穷尽的物理/工程面：CONFIG 烧录与时钟公差、LVR 门限、VDD 与 ADC LDO 压差约束、继电器反灌/感性负载、灌电流与实际 COM 接法、BOM/原理图；Keil 启动文件（startup.A51 的 DATA/XRAM 初始化）、未逐行的 ADC0832 后半与其余 4 个 carrier/vendor 16 例程的行为审计。**结论：本清单是"持续收敛的开放集合"，GAP-03（SDCC 门禁）、GAP-10（STRICT 判决）、§9.5（diff 门禁）与 HIL 阶段（§6 阶段 5）共同构成剩余风险的主要压制手段；任何单项场景绿色都不应被解读为真机背书。**

---

## 10. 执行记录

### 10.1 阶段 1 + 1.5 热修（2026-09-10，已完成，已提交）

| 条目 | 改动 | 验证 |
| :--- | :--- | :--- |
| GAP-01 | `REG_CMS8S78XX.H`：`GPIO_P13_MUX_RXD` 0x02→0x03 | 审计脚本硬比对（见下） |
| GAP-22 | `mcs51_isr.cpp` 默认映射表：I2C→21、SPI→22、PWM EIF2.3/EIP2.2、ADC 优先级 EIP2.**3**（=vector−16）、T3→EIP1.7、UART1 置 0xFF 未映射 | `test_mcs51_silicon_seeds` 全字段断言 |
| GAP-04/13 | `mcs51_context.h/.cpp`：新增 `mcs51_context_set_family()` 与编译期家族默认；reset 末尾按家族种子化（CMS8S：CKCON=0x07 + Fosc=24MHz；经典 51：CKCON=0 + 12MHz 回退不变） | `test_mcs51_silicon_seeds`：两家族种子 + 端口/PS 种子保持 |
| §9.5 | 新增 `frameworks/mcs51/tools/mcs51_shim_audit.py`：SFR/XSFR 地址、GPIO mux 宏、IRQ 向量四面对原厂硬比对；输出未建模寄存器清单（GAP-23 白名单输入，当前 SFR 29、XSFR 111） | 脚本退出码 0："No hard mismatches" |

回归：31 个 mcs51/cms8s host 测试全部 rc=0（含 irq_arbitration、cms8s_adc、cms8s_buzzer、vendor StdDriver、low_power、timer_ext_clk、uart 全套）。全量 host 构建中唯一失败目标 `app_oled_dashboard_e2e` 为预存环境问题（缺 sister repo 的 app_codegen.py），与本次改动无关。

提交记录（2026-09-10 当日已落库，不再是"未提交"）：
1. `a1f2afd fix(mcs51): correct IRQ semantic map vectors/priority SFRs (GAP-22)`；`6bcbf79 fix(mcs51): per-family silicon reset seeds for CKCON and power-on Fosc (GAP-04/13)`；`b9df7b3 test(mcs51): add shim-vs-vendor audit script and silicon seed/IRQ map test`；`4a55404 docs(mcs51): sim-vs-silicon gap audit todolist (GAP-01..25)`；`877da61 chore(mcs51): rebuild wasm simulator assets after GAP-01/22/04/13 fixes`。
2. **wasm 无头回归已完成（2026-09-10，sister repo wink.py 自动重建生产 wasm）**：health_pot **15/15** 场景 PASS（含干烧/超温/继电器 dwell/遥测，验证 GAP-13 生产路径：CMS8S 种子 CKCON=0x07+24MHz 与应用显式重配置共存）；5 个经典 carrier 各 1/1（uart_hello/uart_echo/analog_threshold/button_led/button_led_int，at89c52 家族路径无回归）；GAP-01 活体验证——厂商未修改例程 **uart0_printf、uart0_rxtx 各 1/1 PASS**（两者都写 `P13CFG=GPIO_P13_MUX_RXD`，现在落 0x03）。合计 8 应用 22 场景全绿。
3. 工作区有 1 个非内容改动：`mcs51_health_pot/unisim-assets/device-tree.json` 与厂商 2 例的 device-tree.json 仅 CRLF 规范化差异（configure/资产提取触发），提交时排除或还原。无头运行重建了 7 个应用的 `wink_simulator.{js,wasm}` 资产——属于 tracked 构建产物（参照历史 commit `rebuild wasm simulator asset`），随框架改动一并重新生成，提交时确认 diff 仅为重建内容。
4. GAP-22 的家族门控（at89 构建不应注册 CMS8S 专属模型 hook）只完成了种子层，外设表门控待阶段 3。

### 10.2 第二轮执行：GAP-22 修正 + GAP-03 Task 0（2026-09-10，已提交）

- **GAP-22 优先级位修正（commit 7a8479e）**：SDCC 试点用原厂头机械转译时发现 v1 修复的 EIP 位公式错误——原厂 `IRQ_SET_PRIORITY` 按**优先级模块编号**（扩展模块=向量+1）而非 vector−16。正确映射：T3→EIP2.0、T4→EIP2.1、PWM→EIP2.3、ADC→EIP2.4。审计脚本升级为 vector+priority 全字段硬比对并通过变异测试；31 host 测试全绿。
- **GAP-03 Task 0 落地（SDCC Tier-S 门禁工具链）**：`mcs51_sdcc_devhdr.py`（原厂 Keil 头→SDCC 机械转译，杜绝手写占位漂移）、`sdcc_gate/` 头树（家族 wink_mcu.h、intrins/absacc/经典名别名）、`mcs51_sdcc_gate.py`（按应用 cleanup→编译→**链接**→`--code/iram/xram-size` 预算判决→`.mem` 报告；CMS8S 自动链接全套厂商 StdDriver）；cleanup 新增用户 `sbit` 声明 SDCC 改写与 GB18030 回退。
- **门禁实测**：**8/8 应用通过**（6 官方 carrier + 厂商 uart0_printf/uart0_rxtx 多 TU 例程）；未定义符号负向样例正确红灯；health_pot CODE=10923B/16KB、栈余 184B（CMS8S CODE 含整套 StdDriver 偏保守，Tier-K 为最终准）。
- 评审修正已并入两份实施计划：GAP-02 v1.1（默认引脚 P3.1/P3.0 非必需 CFG、家族门控、位掩码计数器、STRICT 独立构建目标）；GAP-03 v1.1（弃用 `--std-c89`、预算需链接+容量参数、转译器替代占位头、Task 0 状态）。
