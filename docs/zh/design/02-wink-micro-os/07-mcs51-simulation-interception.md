# 2.7 MCS-51/8051 仿真拦截层（frameworks/mcs51）

> **状态：Active（M0–M6 轨 A 已验收；**生产 wasm 链接 + headless 在线实证阶段 0 已验收，2026-08-29，ADR-0075 Accepted**）**
> 本文件为 Layer-① 活规范。设计真相以下列文档为准：
> - ADR：[ADR-0070](../../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md)（umbrella，Accepted）、[ADR-0071](../../decisions/core/0071-sfr-proxy-rmw-edge-data-plane.md)（SFR 数据面，Accepted）、[ADR-0072](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md)（双时钟域）、[ADR-0073](../../decisions/core/0073-cms8s-adc-real-register-map-supersedes-ssot.md)（CMS8S78xx ADC 真实寄存器图，Accepted）、[ADR-0074](../../decisions/core/0074-mcs51-channel1-external-read-pin.md)（通道-1 外部数字 Read-Pin 缝，Accepted）、[ADR-0075](../../decisions/core/0075-mcs51-production-wasm-target-headless.md)（**生产 wasm 链接 + headless 阶段 0，Accepted**）
> - 技术设计：[`docs/tech-designs/mcs51/`](../../tech-designs/mcs51/)（总纲 + 数据面 + 时序面 + 用户手册 + 总方案）
> - 实施计划：[`2026-08-27-mcs51-zero-code-simulation-plan.md`](../../implementation-plans/core/2026-08-27-mcs51-zero-code-simulation-plan.md)
> - 评审记录：[`2026-08-29-mcs51-simulation-layer-review.md`](../../reviews/core/2026-08-29-mcs51-simulation-layer-review.md)（Layer-④）
> - 路线决策：[ADR-0076](../../decisions/core/0076-mcs51-sim-backends-native-vs-iss-channel-roadmap.md)（双后端 native vs ISS + 通道对接路线 + 小家电域频率，**Accepted 2026-08-30**；见 §2.5）

## 0. 进度看板（SSOT 索引）

> 本表是 mcs51 仿真进度的**单一总览**，汇总散落在 §2.x / §3.4 / 各 ADR / 实施计划中的状态。关键区分：**① 框架内部 8051 外设模型**（host/wasm ctest 闭环验证）≠ **② 接到 unisim 活通道**（`js_pal_*` → PinArbiter/真实插件，生产 headless 实证）。模型建好不等于上了活总线。
>
> 图例：✅ 已证（生产/ctest 实证）　🔶 缝在但未接活（stub/console/测试注入）　❌ 未建（路线见 ADR-0076）

### ① 框架内部外设/机制模型（host 23/23、wasm 10/10 ctest 闭环）

| 能力 | 状态 | 证据 / ADR |
|---|---|---|
| SFR/sbit 代理 + diff 边沿 + Read-Latch/Pin + RMW 红线 | ✅ | ADR-0071/0074，M4 ctest |
| 虚拟钟 + fiber 协程 + 配额 catch-up（功能 µs） | ✅ | ADR-0072，M2 |
| Timer0/Timer1 功能模型（lazy 溢出、ISR 向量 1/3、mode 0/1/2） | ✅ | blinky_timer0 |
| UART **TX**（SBUF→putchar/console、置 TI、向量 4） | ✅（出 console + 活通道见 ②） | uart_printf、mcs51_uart_hello |
| **外部中断 INT0/1 模型**（P3.2/P3.3 外部边沿→ITx 边沿/电平→锁 IE0/IE1→派向量 0/2、10ms 采样节流、fiber rendezvous、未驱动线 idle-HIGH、重入保护） | ✅ | ADR-0076 T3，test_mcs51_extint（模型直测 7 例）/int0 e2e |
| **UART RX 模型**（fiber 上下文 drain 注入字节→锁 SBUF 影子+置 RI+派向量 4） | ✅（host+wasm ctest 闭环；活通道见 ② ✅） | ADR-0076 T2/T2.3，uart_echo |
| **ADC0832** 外置 bit-bang 从机（3 线 FSM、`on_read` 回注 DO） | ✅ | adc0832_read、iron_ntc |
| **CMS8S78xx 片内 ADC** 真实寄存器图（0 周期、向量 19、原厂 adc.c 未修改编译） | ✅ | ADR-0073，tier-b |
| XDATA/XSFR 窗口、ABSACC、方言擦除（REGX52/REG_CMS8S）、28 向量表 | ✅ | §3.1 ctest |
| **iron_ntc 热闭环**（ADC0832 测温 + NTC LUT + 继电器 bang-bang + 开/短路安全态） | ✅ | M6 e2e |
| 板级 codegen `mcs51_board_config.h` + 生产 wasm 自动链接 | ✅ | ADR-0075 |
| Timer 外部计数 C/T、Timer0 mode3 | ❌（现 idle + STRICT 告警） | ADR-0076 D2（A 类） |

### ② 接到 unisim 活通道（`js_pal_*` → PinArbiter/真实插件）

| 通道 | 状态 | 证据 / 缺口 |
|---|---|---|
| ch1 数字**写**（MCU→插件，LED/继电器） | ✅ | headless 7/7（`js_pal_gpio_write`） |
| ch1 写**驱动强度轴**（8051 准双向口弱上拉/强低） | ✅ | ADR-0077，`js_pal_gpio_write(pin,level,strength)`，上电 WEAK-HIGH 种子；health_pot 恢复 `P3=0xFF` |
| ch1 数字**读**（插件→MCU，按键 Read-Pin 三路） | ✅ | headless 7/7（`js_pal_gpio_read_state`） |
| ch3 模拟 **ADC** | ✅ C 侧缝（`js_pal_adc_read_norm(32+ch)`→12-bit）+ 跨仓活桥已接通：sister `wink-ai` `afc54d68`（桥 `js_pal_adc_read_norm` 改接 `arbiter.readAnalog(pin)`，无驱动返 0 零回归）+ `81b94565`（headless `AdcDomainHandler` 绑定共享 PinArbiter，此前写进断线 store）。headless `mcs51_analog_threshold`：`INPUT_ANALOG adcChannel:32` 0.8→0.2→0.8 → CMS8S 片内 12-bit ADC → 阈值翻 P1.0 LED，`ASSERT_POINT` **8/8**（T4） | ADR-0076 A 类（跨仓已落地） |
| ch2 UART **TX** | ✅ SBUF 写→`js_pal_uart_write`→UARTBus TX 时间线（置 TI、向量 4 同步）；headless `mcs51_uart_hello` `ASSERT_BUS_PAYLOAD` PASS（T1/T5） | ADR-0076 A 类 |
| ch2 UART **RX** | ✅ 框架模型（fiber 上下文 drain 队列→锁 SBUF+置 RI+派向量 4，host+wasm ctest 闭环，T2）+ **活通道已跨仓接通**：sister `wink-ai` `cf19d412`——`UartBus.sendToFirmware` 优先解析 `wink_mcs51_uart_rx_push`（容忍 emscripten `_` 前缀），回落 `pal_wasm_push_uart_rx_byte(port,b)`；`BusDomainHandler.setWasmExportsFn` 解 headless 上下文先于 wasm 实例化的导出晚绑定。headless `mcs51_uart_echo`：`INPUT_BUS` 推 "A"/"BC" → 向量-4 ISR 收回 → polled TX 回声，`ASSERT_BUS_PAYLOAD` **4/4**（T2.3） | ADR-0076 A 类（跨仓已落地） |
| ch2 bit-bang **I2C / SPI 从机** | ❌ 未建（ADC0832 = 现成 SPI 从机模板） | ADR-0076 A 类 |
| ch1 外部中断 **INT0/1**（向量 0/2） | ✅ 外部边沿→按 IT0/IT1 锁 IE0/IE1→派向量 0/2（边沿/电平、10ms 采样节流、未驱动线 idle-HIGH）；host 模型直测 + Keil e2e（host+wasm/Node）+ headless `mcs51_button_led_int` **10/10** PASS（T3/T5） | ADR-0076 A 类 |
| ch1b **PWM** 占空（8051 无硬件 PWM） | ❌ 边沿能出，占空靠量边沿反推（⚠️） | §2.4，ADR-0076 |
| 定时边沿注入队列（DHT 单总线/NEC 红外/超声波 ECHO/软 UART RX 共用） | ❌ 未建（外部世界现仅 10ms 片边界更新） | ADR-0076 D2 最高杠杆 |
| ch4 WS2812/摄像头（亚 µs 周期编码） | ❌ native 功能钟不可行 | ADR-0076 B 类 → ISS 后端 |

**一句话进度**：内部模型 ~70% 就绪（定时器/双 ADC/UART TX+RX 模型/外部中断 INT0/1/时钟/代理/热闭环已 ctest 证，host 23/wasm 10）；**Stage 2 活通道已接通五项**——通道-1 数字 GPIO 双向（阶段 0）、通道-2 UART **TX** 上 UARTBus（`mcs51_uart_hello` headless）、通道-1 **外部中断 INT0/1**（`mcs51_button_led_int` headless 10/10）、**通道-3 模拟 ADC 活桥**（`mcs51_analog_threshold` headless 8/8，跨仓 `afc54d68`+`81b94565`）、**通道-2 UART RX 活喂字节**（`mcs51_uart_echo` headless 4/4，跨仓 `cf19d412`）。仍 ❌：I2C/SPI 从机、定时边沿注入队列——属 Stage 3 收尾（A 类，native 可补、零改用户码）；亚 µs 时序（WS2812 等）归可选 ISS cycle 后端（B 类，小家电域低频，按需触发）。

> **Stage 2 headless 载体（ADR-0076，2026-08-30）**：`mcs51_uart_hello`（UART TX 信标，`ASSERT_BUS_PAYLOAD` 总线间谍）、`mcs51_button_led_int`（/INT0 边沿 ISR 按键→LED，10 步同形断言 plugin 状态 + 原始 `gpio:8` 驱动电平）、`mcs51_analog_threshold`（ch3 模拟阈值→P1.0 LED，`INPUT_ANALOG` 0.8/0.2/0.8 追踪 8/8）、`mcs51_uart_echo`（UART RX live `INPUT_BUS` "A"/"BC" 4/4）。四个 app 均为**未修改 Keil 源码**，走生产 wasm 链接（ADR-0075 资产约定：device-tree.json + wink_simulator.js 入库，.wasm gitignore）。
>
> **跨仓依赖已解除（2026-08-30）**：ch3 模拟活桥（sister `afc54d68`+`81b94565`）与 ch2 UART RX 活喂字节（sister `cf19d412`）均已在 sister `wink-ai` **已提交**树落地并 headless 实证。
>
> ✅ **数字引脚输入回归已修复（sister `8d06a4e8`，2026-08-30）**：sister 多架构 headless 引擎把 `accuracyMode:'behavioral'` 传播进 `PluginContext` 后，legacy `PluginContext.writePin/analogWrite` 的 behavioral 早退闸位于 `arbiter.setDriver` **之前**，导致 source 插件（按键）的按下电平永不落 PinArbiter，固件经 `js_pal_gpio_read_state`→`arbiter.readPin` 恒读 idle 电平（UART/模拟不受影响：分别走独立 bus 面与 `AdcDomainHandler` 不设闸）。修复把闸位移到 `arbiter.setDriver/setAnalogDriver` **之后**——功能电平恒驱 arbiter（与 `GpioDomainHandler.write`/`AdcDomainHandler.writeNorm` 一致），仅 timing 波形边队列 `pushOrEnqueueWasmEvent` 在 behavioral 跳过。`mcs51_button_led`（7/7）与 `mcs51_button_led_int`（10/10）headless 转绿，五载体全 PASS。

## 1. 定位（双轴模型）

MCU 兼容分两条正交轴：
- **轴 A 真机 port**（`targets/<mcu>/`）：wink 本体经原厂工具链真跑在芯片上（esp32/host/wasm，STM32 待做）。
- **轴 B 仿真拦截层**（`frameworks/<eco>/`）：外国生态用户代码在 host/wasm 上经 wink 运行。现有 `frameworks/arduino/`；**8051 支持 = 新建 `frameworks/mcs51/`，与 arduino 层同构**。

核心结论：**wink 本体不跑 8051 真机**。用户 Keil C51 业务源码零改动，经 C++17 沙箱（`-x c++`）+ SFR 代理 + Fiber 协程在 UniSim 中功能级高保真运行。ESP_PLATFORM 构建下整树不编入（真机固件零增量）。

## 2. 架构要点（待 M6 填实）

- 四大 extern "C" 语言边界：main 重映射、WINK_ISR 静态注册、mcs51_trap C-ABI POD 表、wink_app_get_callbacks/mcs51_adc 注入契约。
- 数据面：SFR 影子内存 + WinkSfr/WinkSfrBitProxy 代理、diff 边沿分发、Read-Latch vs Read-Pin、线性引脚映射 `(port<<3)|bit` → PinArbiter 即时通知（写 `js_pal_gpio_write`）；**通道-1 外部数字 Read-Pin 缝（ADR-0074）**：plain-read 经 `js_pal_gpio_read_state` 回读外部电平，内部陷阱→外部 0/1→HiZ 回退锁存，RMW 永不读外部脚。
- 时序面：宿主 100Hz 主时钟 / 虚拟 µs 从时钟 1:1 映射、配额强制切出 + Catch-Up 补账、Trap 四红线（零延时/禁 yield/纯状态机/时钟解耦）。
- 外设模型：ADC0832（3 线 DIO 阶段隔离状态机）、CMS8S78xx 片内 12-bit ADC（真实寄存器图 0 周期即时穿透，ADR-0073）、UART 双落点（stdout / JS Console）。
- 板级 SSOT：`wink-app.json` 经 wink-tools codegen 生成 `mcs51_board_config.h` + `device-tree.json`。
- 不支持清单（12-T 指令周期、PSW 标志、RC 测温、内联汇编等）：`WINK_SIM_STRICT` 下 assert，release 下降级 `pal_log_w`。

### 2.1 CMS8S78xx 片内 ADC 模型（M5，ADR-0073 Accepted）

模型对齐原厂 CMS8S78xx 真实寄存器图（参考手册 Ch.22 + 设备头 `cms8s78xx.h` + StdDriver `adc.c` 三方核对）；无夹具期 SSOT 的理想化图（`ADCON@0xE1`/START bit6/EOC bit5/`ADCFG/ADCH/ADCL`）**已废弃**：

| SFR | 地址 | 模型语义 |
|---|---|---|
| ADCON0 | 0xDF | bit1 **ADGO**：写 1 启动、转换完成后影子自清（该位即忙标志，`while(ADCON0&0x02)` 首轮即假）；bit6 **ADFM**：0=左对齐/1=右对齐；bit5:2 ANACH |
| ADCON1 | 0xDE | bit7 **ADEN** 模块使能（门控：未使能不转换）；bit6:4 ADCKS |
| ADCCHS | 0xD9 | bit5:0 通道（0~25 = AN0~AN25；0x3F = AN63 内部） |
| ADRESH / ADRESL | 0xDD / 0xDC | 结果高/低字节（只读，模型装载） |
| EIE2 / EIF2 | 0xAA / 0xB2 | bit4 ADCIE 使能 / bit4 ADCIF 完成标志（锁存、软件清零，同 UART TI 先例） |
| IE（EA） | 0xA8 | bit7；EA+ADCIE 均置位时派发 Keil **中断向量 19** |
| ADCLDO / PxxCFG | XSFR 0xF692 / 0xF000.. | MOVX/xdata 空间，经 XSFR 窗口 + `WinkXsfr` 代理受检访问 |

- **0 周期即时穿透**（ADR-0072 即时外设语义）：ADCON0 写钩子（`mcs51_trap_register_sfr_write(0xDF, …)`，`frameworks/mcs51/src/cms8s_adc.cpp`）在写语句内同步完成：门控 ADGO+ADEN → 取通道 → 从 12-bit 注入轨 `mcs51_adc_get_value()` 拉码值 → 按 ADFM 装载 ADRESH/ADRESL → 影子自清 ADGO → 按 ADCIE 锁存 ADCIF、按 EA 派发向量 19。
- **码值装载**（与原厂 `ADC_GetADCResult` 互逆）：右对齐 `ADRESH=(raw>>8)&0x0F, ADRESL=raw&0xFF`（读取 `0x0FFF&((ADRESH<<8)|ADRESL)`）；左对齐 `ADRESH=(raw>>4)&0xFF, ADRESL=(raw&0x0F)<<4`（读取 `0x0FFF&((ADRESH<<4)|(ADRESL>>4))`）。
- **基础设施**：ISR 向量表 `WINK_MCS51_NUM_VECTORS` 8→28（核心 0~7、CMS8S 扩展 8~27）；xdata 合法孔径 = XRAM `[0, WINK_MCS51_XDATA_SIZE)` ∪ XSFR 窗口 `[0xF000,0x10000)`（STRICT assert+abort / release 告警丢弃双态）；模拟注入轨统一 12-bit（0~4095，32 通道），ADC0832 消费点 `&0xFF` 掩码不受影响。
- **原厂 StdDriver 未修改编译（tier-b，2026-08-29 收割，ADR-0073 D6）**：原厂 StdDriver `adc.c` 经 committed shim `frameworks/mcs51/include/cms8s78xx.h`（置于 include 首位遮蔽原厂 Keil 设备头——其重定义 stdint/sfr、野指针 `ADCLDO`，仅 `#include "REG_CMS8S.H"`）+ GBK→UTF-8 transcode（`mcs51_cleanup.py` `read_source`/`--transcode`，构建树规范化、源只读不入库）+ C++17 `inline WinkSfr/WinkXsfr` ODR 安全多 TU 共享，在 host 编译运行（`test_mcs51_cms8s_vendor`）。REG_CMS8S.H 与原厂重名枚举宏采用原厂逐字 token 间距（GCC 无 `-Wmacro-redefined`，仅逐字一致静默；vendor 头目录标 SYSTEM include、MSVC `/wd4005`）；夹具缺失 CMake 优雅跳过。原厂夹具（`docs/vendors/`）参考只读、永不入库（E-003/license）。
- **v1 收窄**：AN63 内部通道（BGR/温度/VDD）返回 0；ADCLDO VSEL 不影响满量程；完整 ADC_Ldo 例程（tier-c，需 system.h/gpio.h shim + 19 个 ISR 桩）延后 M6。
- 证据：M5 host（MSVC/MinGW）mcs51 ctest 16/16、wasm/Node 6/6、STRICT 抽测、arch lint 无发现；tier-b 收割后 MSVC 23/23（17 host 含 `test_mcs51_cms8s_vendor` + 6 wasm）、MinGW host 17/17、arch lint 无发现。

### 2.2 板级 codegen 缝与 NTC 闭环（M6 轨 A，验收 #4）

板级静态描述经 wink-tools codegen 从 `wink-app.json` + `tools/codegen/boards/<board>.json` 生成固件期头文件 `mcs51_board_config.h`（生成器 `tools/codegen/generators/mcs51_board_config.py`、模板 `templates/mcs51_board_config.h.j2`）。**只**固化固件必须静态知晓的常量（引脚 port/bit、ADC 通道、设定点）；热动力学参数（tau/watts/beta/R25）属运行期 device-tree.properties，**不**编入固件（spike-S3 C4）。

- 引脚约定（与 board headers 一致）：线性 index = `port*8 + bit`（P0.0…P3.7 → 0…31）；SFR 口地址 = `0x80 + port*0x10`（P0=0x80、P1=0x90、P2=0xA0、P3=0xB0）。
- 桥接缝：`mcs51_bridge.cpp` 在编译期 `#if __has_include("mcs51_board_config.h")` 引入；framework init 内 `#ifdef MCS51_HAS_ADC0832` 以头文件引脚常量调 `mcs51_adc0832_init(...)` **自动绑定**外部 ADC0832——零运行期 JSON 依赖。头文件目录在 CMake 中加入 **`wink_mcs51_compat` 库**（而非测试 exe）的 include，因 `__has_include` 在编译 bridge.cpp 时求值。
- CMake 以 `EXISTS <generator>` 夹具门控：生成器/样例缺失则 STATUS 跳过，既有 mcs51/wasm 测试照常（同 tier-b 夹具门）。host（`test/CMakeLists.txt`）与 wasm（`test/mcs51/wasm/add_wink_wasm_mcs51_test.cmake`）各有一条生成 `mcs51_board_config.h` 的 custom command。
- **闭环载体 iron_ntc**：未修改式 Keil C51 温控样例（`test/mcs51/samples/iron_ntc.c`）——3 线 ADC0832 bit-bang、自带 8-bit code→温度 LUT（NTC 上拉：码高=冷）、P1.0 继电器/加热 bang-bang（180 °C 设定点）、开路（码≥250）/短路（码≤8）安全态强制 `HEATER=0` 并锁存故障码。e2e 驱动（host+wasm 共用）经 post-init hook 注入冷(200)/热(20)/开路(255)/短路(0) 四码跨四次 run，断言继电器 P1.0 锁存在设定点翻转、开路/短路进入安全态。**驱动不调 `mcs51_adc0832_init`**——读到注入码即证明桥接自动绑定。

### 2.3 生产 wasm 链接与 headless 在线实证（阶段 0，ADR-0075 Accepted）

M0–M6 在**受限 ctest harness**（host fallback + node 桩）内验证；ADR-0075 把 mcs51 接入**生产** `wink_simulator` 链接，使其与 esp32 风格 app 同形、可被 unisim 真实桥驱动。阶段 0 纯本仓 `wink-micro-os`/`wink-micro-app` + 姊妹仓 unisim headless runner，**零 unisim 改动、零前端改动**。

- **第二个自动链接框架**：根 `CMakeLists.txt` 在 arduino 自动链接块之后新增镜像块，gate 在 app 经 `PARENT_SCOPE` 导出的显式布尔 `WINK_APP_MCS51`：`target_link_libraries(wink_simulator PRIVATE wink_mcs51_compat)`，并 `add_dependencies(wink_mcs51_compat generate_config generate_mcs51_board_config)` 保序（STATIC 库不继承 exe 的 codegen 构建序）。默认 esp32 app 不置位该布尔，gate 静默、不链 mcs51。
- **框架拥有回调**：mcs51 app **不**含 `app_callbacks.c`/`device_tree.c`、**不**跑 `app_codegen.py`——`mcs51_bridge.cpp` 已强定义 `wink_app_get_callbacks()`（init/loop→Keil `main` fiber）。app 唯一源是 cleaned Keil `.cpp`（C++17）。生产链接**不得**带入 `mcs51_wasm_link_stubs.c`（通道 `*_reset` 与真实 `pal_wasm_ch*.c` 重复）。
- **真实通道 PAL + 生产 js 库**：生产 wasm 链全部 `pal_wasm_ch*.c` + `wink_sim_js.js`（MODULARIZE `WasmSandbox`、ASYNCIFY、WASM_BIGINT）。mcs51 代理的 `js_pal_gpio_write`/`js_pal_gpio_read_state`/`js_pal_adc_read_norm` 与 esp32 app 走**同一 PinArbiter 桥**（`createUnisimImports`），故 headless 与 worker 装载同形。
- **生产板级 codegen**：根 CMake 新增 `generate_mcs51_board_config` target，跑 `mcs51_board_config.py` 生成进 `${WINK_CONFIG_DIR}/mcs51_board_config.h`；gate 含"`wink-app.json` 的 `board` 字段匹配 mcs51"（`_wink_app_is_mcs51`，板名 regex 一次检出，兼作 DAL 绕开判据）。
- **DAL 全关 + 空 stub 兜底**：mcs51 app 用零 DAL 驱动（button/led 是裸 pin + 前端插件）。检出 mcs51 板后不调 `wink_dal_apply_pruning()`/`app_codegen.py`，改为内联 `list_drivers.py` 把每个 `WINK_USE_<driver>` CACHE FORCE OFF，`dal`/`bal` 只编无条件 stub TU。`dal` 新增 `dal/src/wink_dal_stub.c`（空 TU 占位，镜像 `bal/src/wink_bal_stub.c`），保证全裁剪后 STATIC 目标仍有源。
- **双消费者 SSOT**：mcs51 app 的 `wink-app.json` 同时供 ① C 构建（板名检出绕开 DAL 裁剪）与 ② 前端 device-tree（`winkcli sim run` 经 `runtime_device_tree.py` 生成 `unisim-assets/device-tree.json`）。button/led 用 `gpio_pin`（两 manifest 皆别名）+ `active_low`/`active_high`。`device-tree.json` 为生成物，不手写。
- **headless 证据规范（统一约定）**：所有证据 scenario 必须放在 `<app>/unisim-scenarios/*.scenario.json`（与 `pdk_button_led`/`oled_dashboard` 一致；前端 workspace-scanner 只发现 `unisim-scenarios/` 或 `scenarios/` 目录，散在 app 根目录的 scenario 发现不到），且统一经跨仓 CLI headless 目录形式调用（`--scenarios` 传**目录**则自动跑该目录下全部 `*.scenario.json`；自动构建 WASM + 生成 device-tree + 抽资产，再用真实 PinArbiter + 真实插件）：
  ```
  cd <sister>/wink-ai/packages/wink-tools
  WINK_DEV=1 python wink.py sim run --app "<abs app dir>" --mode headless \
            --scenarios "<abs app dir>/unisim-scenarios" --reporter spec
  ```
  mcs51 证据载体：`mcs51_button_led`（未修改 Keil 按键→LED，`sbit KEY=P3^2(线性26,active-low); sbit LED=P1^0(pin8)`）、`mcs51_button_led_int`、`mcs51_uart_hello`、`mcs51_uart_echo`、`mcs51_analog_threshold`，各带 `unisim-scenarios/`。
  **一键聚合证据**：`wink-micro-os/frameworks/mcs51/tools/run_mcs51_headless_evidence.ps1`（自动定位 sister CLI、按通道顺序跑全部载体、聚合 PASS/FAIL 与退出码；`-App <name>` 单跑、`$env:WINK_AI_ROOT` 覆盖 sister 路径）。五载体（TX / RX-live / 模拟 / INT0 中断 / 轮询按键）**全 PASS**（数字输入回归经 sister `8d06a4e8` 修复，见看板 ② 注）。
- **资产入库约定**：`unisim-assets/device-tree.json` 与 `wink_simulator.js`（胶水+设备树，可评审）入库；`wink_simulator.wasm` 为构建产物，`.gitignore` 的 `*.wasm` 排除（同 avoidance_car/oled_dashboard）。

> **延后（不在阶段 0）**：Stage 1 前端（Vue mcs51 画板、device-tree→manifest translator/resolver 去 esp32 硬编码、P1.0 等 pin 标签、DIP40 artwork、worker 装载 mcs51 wasm）；Stage 2 模拟（host 桥 `js_pal_adc_read_norm` 由 stub 0.0 接 `arbiter.readAnalog`、adc0832/`thermal_heater_plate` 插件闭环）。C 侧生产缝已就绪，Stage 1 不再改 C/链接。

### 2.4 通道覆盖 × 零修改可行性矩阵（落地边界）

判定问题：**在完全不修改用户 Keil 源码的前提下**，某类外设/通道能否功能级仿真。结论的关键前提是拦截层已具备的**五个原语**（全部就绪，其中"从机回驱读脚"已由 ADC0832 生产验证）：

| 原语 | 机制 | 已用于 |
|---|---|---|
| 固件写脚 → 外 | sbit/整口写 diff 边沿 → `on_write` 陷阱 + `js_pal_gpio_write` | LED、ADC0832 CLK/DI |
| 外电平 → 固件读脚 | Read-Pin 三路（`on_read` 陷阱 → `js_pal_gpio_read_state` → 锁存回退） | 按键、ADC0832 DO |
| **从机往固件读脚注数据位** | **`mcs51_trap_register_read(port,bit,on_read,…)` 读陷阱** | **ADC0832 DO（`mcs51_adc0832.cpp:165`，证明硬方向可行）** |
| 拦截点读虚拟时间 | `wink_mcs51_virtual_us()`（`mcs51_clock.cpp:90`） | 定时器 catch-up |
| 派中断向量 | `wink_mcs51_dispatch_vector(n)` + SFR 读写钩子（28 向量表） | 定时器 1/3、UART 4、ADC 19 |

**ADC0832 即通用模板**：一个 bit-bang 从机模型，监视主机 CLK/DI 写边沿（写陷阱重建事务）、在 DO 上用读陷阱回送数据位、0 虚拟时间完成。I2C 从机、SPI 从机、UART RX、外部中断全是同一套原语的不同状态机——**工作量在框架补模型，不需改用户 Keil 代码**。

逐通道 verdict（✅ 已证 / 🔧 框架补模型即零修改落地 / ⚠️ 零改码可行但语义靠边沿时间推断 / ❌ 当前保真度下不可行）。**缺口分类（ADR-0076 D2）**：所有 🔧/⚠️ 项均为 **A 类——模型缺失**，同 native 后端补模型即零改用户码落地；唯一 ❌（WS2812/摄像头亚 µs 单脚周期编码）为 **B 类——信息在 native 编译时已销毁**，归可选 ISS cycle 后端（见 §2.5）。

> **🔧 vs ⚠️ 的分界 = 信息载体**。🔧 协议**自时钟/电平/寄存器**（I2C 的 SCL、SPI 的 CLK 边沿即事务时钟，字节/ACK 是离散逻辑符号）——代理状态机精确重建每个符号，不靠墙钟。⚠️ 协议**时间编码**（PWM 占空、脉冲宽度 = 两个边沿的时间差）——代理只看到一串翻转，必须用 `wink_mcs51_virtual_us()` 量间隔反推语义，故：① 需 device-tree/板级配置**声明引脚角色**（裸翻转无法区分 PWM/舵机/闪灯，声明来自配置非改用户 C 码）；② 至少 1 个完整周期后才报得出，仅对**稳态周期波**有效；③ 须在 **timing 精度模式**下验证，不得用 behavioral 冒充。符号宽度在百~千 µs（舵机 1–2ms、PWM 调光、超声波 ECHO）时功能钟足够；亚 µs 才撞墙（见 ❌）。**⚠️ 不是不可实现，是"靠测量推断 + 受 timing 保真度约束 + 需补基础设施"，零改用户码仍成立。**

| 通道 | 典型 8051 用法 | 零修改能仿？ | 依据 / 缺口 |
|---|---|---|---|
| 1 GPIO 输出 | LED/继电器 sbit 写 | ✅ **已证** | 写边沿→PinArbiter→插件（headless 7/7） |
| 1 GPIO 轮询输入 | `if(KEY==0)` 按键 | ✅ **已证** | Read-Pin 三路，`mcs51_button_led` |
| 1 外部中断 INT0/1 | `interrupt 0` 按键/计数 | ✅ **已证**（Stage 2 T3） | P3.2/P3.3 外部电平经 `js_pal_gpio_read_state` 采样（10ms 节流），按 IT0/IT1 边沿/电平锁 IE0/IE1→派向量 0/2；边沿派发后硬件自清、未驱动线 idle-HIGH。host 模型直测 7 例 + Keil e2e（host+wasm/Node）+ headless `mcs51_button_led_int` 10/10。用户 `IT0/EX0/EA=1` 零改 |
| 1 脉冲时序（超声波/输入捕获） | bit-bang TRIG + 轮询 ECHO 计时 | ⚠️ **零改码可行，边沿时间推断**【A 类：补定时边沿注入队列】 | 定时器+虚拟钟+外部读都在；**缺定时边沿注入队列**（轴 A esp32 有 `push_pin_event`，mcs51 读路径现拉瞬时电平、无"未来时刻调度边沿"）。补框架侧定时边沿模型（按 `virtual_us` 求值此刻脚电平），零改用户码。ECHO 宽几百~几千 µs 功能钟够分；但 08-channel-routing 红线要求脉冲用例在 **timing 模式**可复现（废 cm→µs 捷径、走真边沿注入 + 同源 `pulse_in`） |
| 1b PWM | 8051 **无硬件 PWM**，定时器 ISR 软翻转 | ⚠️ **零改码可行，占空靠量边沿**【A 类：边沿量测 + 引脚角色声明】 | 软 PWM **边沿**走通道 1 零改码能出；占空无 `pal_pwm_set_duty` 语义调用（轴 A esp32 白拿 duty），需框架/插件**量**边沿间隔反推：`virtual_us` 记上升→下降差=高电平时间、两上升沿差=周期，duty=高/周期。需 ① device-tree 声明该脚为 PWM/舵机（非改 C 码）② 稳态周期波、≥1 周期延迟 ③ timing 模式验证。舵机 1–2ms / LED 调光 kHz 周期远长指令周期，功能微步噪声可忽略→够用；硬件 PWM N/A |
| 2 I2C（bit-bang） | 软件 I2C → OLED/EEPROM | 🔧 **可行（框架补从机模型）** | 同 ADC0832 模式：监视 SCL/SDA 写陷阱重建 START/ADDR/ACK，开漏 SDA 用读陷阱回 ACK/数据，接 I2CBus 寄存器镜像。零用户改码 |
| 2 SPI（bit-bang） | 软件 SPI → 传感器/屏 | 🔧 **可行** | **ADC0832 本质即 3 线 SPI 从机**（CLK/DI/DO/CS），MISO 回读已证 |
| 2 UART TX | `SBUF=c; while(!TI)` printf | ✅ **已证**（Stage 2 T1） | SBUF 写钩子→`js_pal_uart_write`→UARTBus TX 时间线（置 TI、向量 4 同步）；固件只写 SBUF。headless `mcs51_uart_hello` `ASSERT_BUS_PAYLOAD` PASS |
| 2 UART RX | 等 RI、读 SBUF、`interrupt 4` | ✅ **已证**（Stage 2 T2/T2.3） | 框架侧 fiber drain 队列→锁 SBUF 影子+置 RI+派向量 4，host+wasm ctest 闭环；**活通道跨仓已接通**（sister `cf19d412`：`UartBus.sendToFirmware` 优先 `wink_mcs51_uart_rx_push`、回落 `pal_wasm_push_uart_rx_byte`，`setWasmExportsFn` 晚绑定）。headless `mcs51_uart_echo` `INPUT_BUS` "A"/"BC" → 回声 `ASSERT_BUS_PAYLOAD` 4/4。用户标准 SBUF/RI/ISR 不变 |
| 3 模拟 ADC | ADC0832 bit-bang / CMS8S 片内 | ✅ **已证**（Stage 2 T4） | C 侧链路通（`js_pal_adc_read_norm(32+ch)`→12-bit）；跨仓活桥已接（sister `afc54d68` 桥→`arbiter.readAnalog`、`81b94565` headless AdcDomainHandler 绑定共享 PinArbiter）。headless `mcs51_analog_threshold` `INPUT_ANALOG adcChannel:32` 0.8/0.2/0.8 → CMS8S 片内 ADC → 阈值翻 P1.0 LED，`ASSERT_POINT` 8/8。固件零改 |
| 4 WS2812/摄像头 | 单脚 bit-bang，0.4µs NRZ 编码 | ❌ **当前保真度下不可行**【B 类→ISS cycle 后端，ADR-0076 D1；native 实验路径见 D4，非通用】 | 见下"周期时间编码墙"与 §2.5 |

**唯一真正的墙——单脚周期时间编码协议**：WS2812 数据全编码在单脚脉冲宽度（T0H≈0.4µs / T1H≈0.8µs）。bare 8051 无帧缓冲/DMA，唯一信息源是**逐指令周期的翻转时序**；而功能级代理刻意不建模 12-T 指令周期（见 §2 不支持清单），微步只按 SFR 访问充"功能 µs"、无周期精度 → bitstream 无法还原。这不是通道没接，是**保真度天花板**：要仿 WS2812/摄像头需 `cycle` 精度模式（wasm 仿真轴标 Planned）。软 UART 波特率同理受限于功能时钟（硬件 UART 字节级不受影响）。

**与通道无关的核心限制**（影响零修改）：PSW 标志位（CY/OV/AC）不建模 → 依赖进位标志的多字节手工汇编会错（C 编译器生成的正常算术一般不依赖，手写 asm 会）；内联汇编、12-T 周期在 `WINK_SIM_STRICT` 下 assert。

**落地结论**：低代码/AI 生成场景常见外设，**零改用户码全部可落地**——按键/LED/数码管/外部中断/串口收发/OLED[I2C]/温湿度/ADC 电位器·NTC 属 ✅/🔧（逻辑符号精确重建）；舵机调光[软 PWM]、超声波[脉冲宽度]属 ⚠️（零改码可行，占空/脉宽靠量边沿反推 + 引脚角色声明 + timing 模式验证）。唯一 ❌ 是 WS2812/摄像头这类**亚 µs 单脚周期时间编码**协议（B 类，归可选 ISS cycle 后端，ADR-0076 D1；native 非通用实验路径见 D4）。路线已由 [ADR-0076](../../decisions/core/0076-mcs51-sim-backends-native-vs-iss-channel-roadmap.md)（Accepted 2026-08-30）拍板：A 类 Stage 2 活通道（UART TX/RX、INT0/1、模拟绑定）→ Stage 3 基础设施（定时边沿注入队列、外部计数 C/T、`_nop_` 重标定、I2C/SPI 从机）；ISS 按需触发，不阻塞主线。后端边界与时钟保真模型见 **§2.5**。

### 2.5 时钟保真模型与后端边界（ADR-0076 Accepted）

native 功能级后端的虚拟钟（ADR-0072）：`s_virtual_us` 只在拦截点推进、每拦截点固定充 `WINK_MCS51_MICROSTEP_US=5µs`；时间 ∝ 被拦截事件数而非真实指令周期；纯寄存器算术充 ~0 时间；外部世界只在配额片边界（10ms）yield 时推进；定时器溢出时刻按 period 精确计算（µs 级、事件驱动），但固件 bit-bang 翻转的边沿落在 5µs 粗账上。

**缺口分两类（根因不同，决定能否在 native 补）：**

- **A 类——模型缺失**：五个原语（§2.4）都在，某外设/通道的框架侧模型还没写。同 native 后端补模型即落地，零改用户码。含：定时边沿注入队列（最高杠杆，解锁 DHT/红外/超声波/软 UART RX）、外部脉冲计数 C/T、`_nop_()` 重标定 5µs→~1µs + 量子细化、INT0/1、硬件 UART TX/RX、bit-bang I2C/SPI 从机、模拟量 host 桥接。
- **B 类——信息在 native 编译时已销毁**：Keil C 编成 x86/wasm 指令后运行时不存在 8051 指令流，三样东西编译即丢失：① 每条 C 语句真实 12-T 机器周期数；② PSW 标志位（CY/OV/AC）；③ 内联汇编 8051 助记符。凡语义依赖这三者——亚 µs 单脚周期编码（WS2812 T0H≈0.4µs、摄像头）、高速软 UART（115200，8.7µs/bit 近量子极限）、精确指令周期/中断响应延迟、PSW 进位依赖的多字节手写汇编——调时钟参数补不回来。

**双后端策略（D1）**：

| 后端 | 状态 | 机制 | 覆盖 |
|---|---|---|---|
| native C++ 功能级代理 | **默认/主**（现状） | Keil C 当 C++17 编，SFR/sbit 代理 + trap + 功能虚拟钟 | 电平逻辑/字节事务/百 µs~ms 定时；A 类补全后覆盖小家电压倒性多数外设 |
| ISS cycle 级 | **可选第二后端，Planned（本阶段不实现）** | 同一未修改 `.c` 经 **SDCC** 编成真实 8051 opcode 后解释执行；每指令周期数已知、SFR 落 ISS 内存图可 trap、GPIO 翻转带精确周期时间戳 | 一次解决全部 B 类；更慢更重，8051 ISS 核小、有成熟公开实现可集成，属有限工程量非研究风险 |

两后端**共用同一 `js_pal_*` PinArbiter 数据面与插件**，仅时钟源与拦截机制不同；后端选择按 app/协议（device-tree/板级配置声明），对插件与前端透明。

**域频率结论（D3，小家电域）**：B 类低频——主流加热控制类显示用数码管+指示灯/TM1650 低速 2 线驱动，WS2812 集中在照明/玩具/氛围灯偏消费电子；摄像头 8051 本体 ≈0；小家电串口几乎全 9600（115200 走硬件 UART，字节级属 A 类）；真实实时时序很粗（可控硅过零 10ms、数码管扫描百 µs~ms、NEC 560/1690µs、DHT11 最小 26µs，全在几十 µs~ms）；PSW/内联汇编极低。两个"看似 B 实为 A"的高频点：**38kHz 红外载波**（半周期 13µs ≫ 量子，`_nop_` 重标定后可做）与 **DHT11/22 单总线**（26µs ≫ 量子）。对低代码/AI 生成场景再打折：AI 生成纯 C + 标准抽象，不写亚 µs bit-bang；WS2812 类需求应在平台侧做成声明式灯带组件。**故先把 A 类做扎实（Stage 2/3）；ISS 等出现具体 WS2812 灯效产品/摄像头/内联汇编厂商驱动需求再投，不为假想需求阻塞主线。**

**红线不变**：ESP32 零 mcs51 增量；纯算术空转延时（无 nop/SFR）native 仍充 ~0，归"不支持/建议用定时器"，不因此上 ISS；A 类模型守 trap 四红线（边沿时间线是"读时按虚拟钟求值"，不主动推进钟）。

### 2.6 准双向口驱动强度模型（ADR-0077 Accepted）

8051 端口是准双向口：**锁存 1 = 内部弱上拉 FET 导通（输入释放态/高边弱驱动），锁存 0 = NMOS 强导通灌低**；该规则对输入脚与输出脚同时成立（LED 常接低边、P0 口需外部上拉皆因此）。此前 GPIO 写 ABI 只有 `(pin, level)`，host 把一切 MCU 写当 esp32 push-pull 强驱动（SUPPLY）——标准 Keil 初始化 `Pn = 0xFF`（对 BSS 影子初值 0 产生 0→1 边沿）把 MCU 登记为 **SUPPLY-HIGH 强驱动者**，与按键插件的 SUPPLY-LOW 同强度异态仲裁成 CONFLICT，Read-Pin 回退锁存 → 按键永远读高（失效）。修复把驱动强度提为数字输出的固有 ABI 维度（跨仓原地扩展，严格超集平替旧符号）：

- **ABI**（`targets/wasm/wasm_bridge.h`）：中性枚举 `wink_drive_t { WEAK=1, PULL=2, SUPPLY=3 }`（数值恒等映射 host PinArbiter `DriveStrength`，不泄漏 host TS 类型）；`js_pal_gpio_write(uint16_t pin, bool level, uint8_t strength)`。旧 `(pin,level)` ≡ 新函数传 SUPPLY。ABI hash 重算 `PAL_WASM_ABI_HASH = 0x20149EFCu`。
- **esp32 PAL**（`pal_wasm_ch1_gpio.c` init 初始电平 + 运行写两处）：恒传 `WINK_DRIVE_SUPPLY`——push-pull 高/低皆强驱动，**电气结果逐位不变**（仅意图显式化）。
- **8051 proxy 边沿分发**（`mcs51_proxy.hpp` sbit 与 whole-port 两处 diff 边沿）：上升锁存沿 → `MCS51_DRIVE_WEAK`（1），下降锁存沿 → `MCS51_DRIVE_SUPPLY`（3）；proxy 不区分输入/输出方向（规则对两者一致）。
- **8051 上电种子**（`mcs51_bridge.cpp mcs51_framework_init()`，`trap_reset()` 之后）：**同时** ① 置 P0–P3 `wink_mcs51_sfr_shadow[0x80/0x90/0xA0/0xB0] = 0xFF`（锁存器真实上电态），② 对 32 脚（P0.0–P3.7）调 `js_pal_gpio_write(pin, true, WEAK)` 登记弱高驱动。两者缺一不可：仅置影子则固件首条 `LED=1` 无 diff 边沿、host 端无任何 MCU 驱动注册（曾使 `mcs51_analog_threshold` 启动 LED-off 断言回归）；仅登记驱动不置影子，则固件 `Pn=0xFF` 仍产生 0→1 伪边沿。种子后固件标准 `Pn = 0xFF` 与影子同值 → diff=0、无边沿、不重复注册，**精确镜像硅片（上电从不跳变）**。
- **host 桥**（wink-ai `unisim-bridge-factory.ts`）：强度恒等映射进 `arbiter.setDriver`，非 1/2/3/缺省兜底 SUPPLY（`strength ?? SUPPLY`）——旧 wasm 配新 host 退回 esp32 行为不炸；新 wasm 配旧 host 时 JS 忽略多余实参、强度丢失（mcs51 退回强驱动、旧 bug 复现）但不崩。ABI SSOT 见 abi-catalog `js_pal_gpio_write` 条目（strength 参数 `desc` 编码枚举，挂 ADR-0077）。
- **仲裁自洽**：按键 P3.2 锁存 1 = WEAK-HIGH 上拉，按下插件 SUPPLY-LOW → SUPPLY 胜、读 LOW，释放仅剩 WEAK-HIGH → HIGH；WEAK vs SUPPLY 异态**不**触发 CONFLICT（不同强度）。未修改 Keil 例程的标准 `Pn=0xFF` 初始化即可工作——health_pot 已删除「不写输入口」workaround、恢复 `P3 = 0xFF`。
- Read-Pin 三路解析序（ADR-0074）与 RMW 只读锁存红线（ADR-0071）**不变**；本决策仅改「写边沿上报的强度」。证据：mcs51 ctest host 23 + wasm/Node 10 全绿（含新增 WEAK/SUPPLY 强度断言）、5 carrier + health_pot 2/2 headless、esp32 emcc/Node GPIO 语义 9/9（SUPPLY 路径不变）。

## 3. 目录树、API 面、构建与测试矩阵（活规范）

### 3.1 目录树（`wink-micro-os/`）

```
frameworks/mcs51/
  include/   REGX52.H（方言擦除 + WinkSfr/WinkSbit + main/ISR 重映射边界）
             REG_CMS8S.H（CMS8S78xx SFR/XSFR 代理 + 原厂逐字掩码宏）
             cms8s78xx.h（tier-b shim，遮蔽原厂 Keil 设备头）
             mcs51_adc.h / ADC0832.H / mcs51_trap.h / mcs51_isr.h
             absacc.h（XBYTE/XWORD WinkXByteProxy）、mcs51_xsfr.hpp
  src/       mcs51_bridge.cpp（init/缝）、mcs51_sfr.cpp（影子+代理+边沿分发）
             mcs51_adc.cpp（12-bit 注入轨）、mcs51_adc0832.cpp（3 线 FSM）
             cms8s_adc.cpp（片内 ADC 0 周期模型）、mcs51_isr.cpp（28 向量表）
             mcs51_clock.cpp / mcs51_timer.cpp / mcs51_uart.cpp
             mcs51_xdata.cpp（XRAM + XSFR 窗口）、mcs51_unsupported.cpp（STRICT 双态）
  tools/mcs51_cleanup.py（Keil .c → .cpp：ISR 重写、UTF-8/GBK 解码、--transcode）
test/mcs51/
  samples/   blinky、blinky_timer0、uart_printf、gpio_in_out、adc0832_read、
             cms8s_adc_test、iron_ntc（未修改式 Keil 用户源码）
  apps/iron_ntc/wink-app.json（板级 codegen SSOT 输入）
  unit/      数据面/时钟/静态初始化/STRICT/CMS8S ADC 等框架单测
  wasm/      add_wink_wasm_mcs51_test.cmake + node 桩 + wasm 专用驱动
  test_mcs51_*_e2e.c（host+wasm 共用闭环驱动）
```

### 3.2 API 面（C-ABI 契约）

- 运行入口：Keil `main` → `wink_mcs51_user_main`（不返回，跑在协作 fiber 上，`SIMULATION=1`）；`_nop_()` → `wink_mcs51_microstep()` 让出（裸 `while(1){}` 冻结）。
- 注入/观测：`mcs51_adc_set_value/get_value/reset`（12-bit、32 通道；ADC0832 消费点 `&0xFF`）；`mcs51_adc0832_set_value(ch,val)`、`mcs51_adc0832_init(...)`；`mcs51_framework_set_post_init_hook(fn)`（每次 `wink_runtime_run()` 的 framework init 末尾、`trap_reset/adc_reset` 之后运行，注入存活）。
- 影子观测：`wink_mcs51_sfr_shadow[256]`（C 链接 BSS，跨 run **不**复位）、`wink_mcs51_xdata_shadow[65536]`（每 run 复位）。
- 外设模型注册：`mcs51_trap_register_sfr_read/write(addr,hook)`、`mcs51_trap_register_read/write(port,bit,fn,ctx)`。
- **通道数据面（UniSim channel，ADR-0074）**：
  - 写方向（MCU→插件）：sbit/整口写 diff 边沿 → `js_pal_gpio_write(linear_pin, level)`（channel-1）；模拟读 → `js_pal_adc_read_norm(pin)`（channel-3）。
  - **数字读方向（插件→MCU，Read-Pin）**：代理 plain-read 经 `uint8_t js_pal_gpio_read_state(uint16_t pin)` 回读外部电平，复用轴 A 枚举契约 `JS_GPIO_STATE_LOW=0 / HIGH=1 / HIZ=2 / CONFLICT=3`。**Read-Pin 三路解析序**：① 内部 `on_read` 陷阱（模型自有位，如 ADC0832 DO）优先 → ② 外部 0/1 驱动电平胜出 → ③ `HiZ(2)/CONFLICT(3)`（无外部驱动）回退锁存影子。**RMW 复合赋值永不读外部脚**（准双向口红线，仍只读锁存）。
  - host fallback（`mcs51_uni_bridge.cpp`，`#ifndef __EMSCRIPTEN__`）：`s_host_ext_pin[32]` 懒初始化 HiZ，测试注入 `wink_mcs51_host_set_ext_pin(pin,state)` / `wink_mcs51_host_ext_pins_reset()`；wasm node 桩回调导出 getter `Module['_mcs51_wasm_ext_pin_state'](pin)`，未导出返 HiZ。生产由 `wink_sim_js.js` 接 PinArbiter（M7 集成）。
- **Stage 2 活通道模型（ADR-0076）**：
  - 外部中断（T3，`mcs51_extint.cpp`）：`wink_mcs51_extint_poll()` 在每个 microstep（fiber rendezvous）采样 INT0=P3.2(线性 26)/INT1=P3.3(27) 的 `js_pal_gpio_read_state`，10ms 虚拟片节流（`SAMPLE_PERIOD_US`，reset 后强制首采）；按 `IT0/IT1` 边沿/电平锁 `IE0/IE1`（TCON 0x88），`EA+EX0/EX1`（IE 0xA8）门控后 `wink_mcs51_dispatch_vector(0/2)`，边沿派发后硬件自清 IEx；HiZ/冲突按内部弱上拉解析为 idle-HIGH；`s_in_poll` 重入保护防 ISR 内 SFR 访问递归派发。`wink_mcs51_extint_reset()` 清节流/IE 标志但**保留**边沿基线（外部电平是 WORLD 态，跨 framework init 持续）。
  - UART TX（T1，`mcs51_uart.cpp`）：SBUF 写钩子→`js_pal_uart_write(port,buf,len)` 上 UARTBus（同步置 TI、向量 4）。
  - UART RX（T2 模型 + T2.3 活通道）：fiber 上下文 drain 注入字节队列→锁 SBUF 影子+置 RI+派向量 4；host 注入/观测 API + wasm node 桩闭环。**活喂字节跨仓已接通**（sister `cf19d412`）：emscripten 导出 `wink_mcs51_uart_rx_push(byte)`（单一 8051 UART，无 port 参数）喂 FIFO，microstep 每步按 `RX_BYTE_SPACING_US` 节奏 drain 一字节；`UartBus.sendToFirmware` 优先该导出（mcs51 wasm 同时链 esp32 PAL 对象，故不能走 `pal_wasm_push_uart_rx_byte`——其 ring 8051 固件不 drain）。见看板 ② ✅。
  - 模拟 ADC 活桥（T4，跨仓）：固件经 `js_pal_adc_read_norm(32+ch)` 读（CMS8S on-chip 12-bit：norm×4095→raw）；sister 桥 `afc54d68`→`arbiter.readAnalog`、headless `81b94565` AdcDomainHandler 绑共享 PinArbiter；headless `INPUT_ANALOG adcChannel:32` → setAnalogDriver(32) → 桥读 arbiter → 固件 ADC 转换。见看板 ② ✅。

### 3.3 构建集成与三端

- 未修改 Keil `.c` 经 `mcs51_cleanup.py` 正则清洗为构建树 `.cpp`（源只读不入库），以 C++17 编入 host（MSVC/MinGW）与 emcc/wasm（ASYNCIFY fiber）；**无 `-fpermissive`、无硬编码 GBK 输入字符集**（源 UTF-8；GBK 仅用于读 vendor 夹具到 UTF-8 构建副本）。
- ESP32：`frameworks/mcs51/CMakeLists.txt` 顶部 `if(ESP_PLATFORM) return()`，真机固件**零** mcs51 符号/增量。
- 运行：`wink_runtime_run(cb, max_ticks)`，tick=10ms；宿主 100Hz 主时钟 / 虚拟 µs 从时钟 1:1。

### 3.4 测试矩阵（2026-08-30 证据）

| 端 | 计数 | 覆盖 |
|---|---|---|
| MSVC host | mcs51 **23/23** | M1 blinky、M2 Timer0 ISR/时钟/静态初始化、M3 UART/GPIO/shims、M4 ADC0832 + RMW 数据面/边沿/操作符、M5 CMS8S 片内 ADC 单元+e2e、**M6 iron_ntc 闭环**、tier-b 原厂 adc.c、**M7 外部数字读缝 `test_mcs51_gpio_external`（ADR-0074）**、**Stage2 `test_mcs51_extint`（外部中断模型直测 7 例：边沿锁存/自清、禁能挂起、电平每片重请、HiZ→idle-HIGH、reset 基线语义）+ `test_mcs51_int0`（Keil INT0 ISR e2e）+ UART RX echo** |
| MinGW host | **23/23** | 同上（GCC 方言链） |
| emcc/wasm + Node | **10/10** | blinky、timer0、uart、uart_echo（RX）、gpio、adc0832、cms8s_adc、iron_ntc、gpio_external（外部 Read-Pin 缝）、**int0（/INT0 边沿 ISR e2e，导出 getter 回调）**（`-sERROR_ON_UNDEFINED_SYMBOLS=1`，ctest label `wasm`） |
| **生产 wasm + unisim headless（ADR-0075 阶段 0）** | **7/7 步 PASSED** | `mcs51_button_led` 轮询按键→LED（阶段 0） |
| **生产 wasm + unisim headless（ADR-0076 Stage 2）** | **10/10 + 2/2 + 8/8 + 4/4 步 PASSED** | `mcs51_button_led_int`：/INT0 边沿 ISR 按键→LED，10 步同形断言（按下边沿→on+`gpio:8==0`、按住不重触发、释放上升沿 no-op、再按→off）；`mcs51_uart_hello`：UART TX 信标 `ASSERT_BUS_PAYLOAD`（"HELLO" / 重复 "HELLO\nHELLO"）；`mcs51_analog_threshold`：ch3 模拟活桥 `INPUT_ANALOG adcChannel:32` 0.8/0.2/0.8 → CMS8S ADC → 阈值翻 P1.0 LED，8 步 `ASSERT_POINT`；`mcs51_uart_echo`：UART RX live `INPUT_BUS` "A"/"BC" → 向量-4 ISR → polled TX 回声，4 步 `ASSERT_BUS_PAYLOAD`。均未修改 Keil 源、生产链接（`winkcli sim run --mode headless`）。数字输入两载体在 sister `8d06a4e8` 修复后于**当前树**全绿 |

- **回归（2026-08-30）**：MSVC host mcs51 ctest **33/33**（23 host + 10 wasm/Node）全绿、arch lint（layering+api）无发现、ESP32 mcs51 gate 静默零增量。
- ✅ **跨仓活桥已收口（2026-08-30）**：ch3 模拟（sister `afc54d68`+`81b94565`）与 ch2 UART RX 活喂字节（sister `cf19d412`）均在 sister `wink-ai` **已提交**树落地并 headless 实证。
- ✅ **数字引脚输入回归已修复（sister `8d06a4e8`，2026-08-30）**：根因非 mcs51 固件、亦非 `0c3a7609` 直接改坏，而是该重构首次把 `accuracyMode:'behavioral'` 传进 `PluginContext`（此前 ctor 默认 `'timing'`）。legacy `PluginContext.writePin/analogWrite` 的 behavioral 早退闸位于 `arbiter.setDriver` 之前，source 插件（按键）的按下电平永不落 arbiter，固件 `js_pal_gpio_read_state`→`arbiter.readPin` 恒读 idle HIGH。修复把闸位移到 arbiter 驱动**之后**（功能电平恒驱，仅 timing 波形边队列 behavioral 跳过），与 `GpioDomainHandler.write`/`AdcDomainHandler.writeNorm` 对齐。当前树 headless：`mcs51_button_led` 7/7、`mcs51_button_led_int` 10/10 全绿，五载体 PASS。
- `wink lint --pack layering --pack api`：无发现。PR CI：host 矩阵（ubuntu+windows）+ 新增 wasm job（setup-emsdk + Node，`ctest -L wasm`）。

### 3.5 回写清单（M6 轨 A 已完成项）

- [x] 本文节扩写为活规范（目录树、API 面、构建集成、测试矩阵）
- [x] `03-directory-architecture.md` 目录树补 `frameworks/mcs51/`
- [x] `03-app-codegen/03-ai-dsl-and-codegen-pipeline.md` 补 mcs51 板级 codegen 小节
- [x] `04-wasm-simulation/` 通道/引脚映射补 Pin 0~31 线性映射与 mcs51 时钟域
- [x] **M7（ADR-0074）** §3.2 API 面补通道-1 外部数字 Read-Pin 缝（`js_pal_gpio_read_state` + HiZ 回退锁存 + RMW 红线）；§3.4 测试矩阵 host 19 / wasm 8；`02-virtual-clock.md` §6.5 标注通道-1 双向
- [x] **阶段 0（ADR-0075）** 新增 §2.3 生产 wasm 链接与 headless 在线实证（第二框架自动链接、框架拥有回调、真实通道 PAL + 生产 js 库、生产板级 codegen、DAL 全关 + 空 stub、双消费者 SSOT、headless 载体 `mcs51_button_led`、资产入库约定）；§3.4 测试矩阵补生产 headless 7/7 行
- [x] **通道可行性** 新增 §2.4 通道覆盖 × 零修改可行性矩阵（五原语、ADC0832 为 bit-bang 从机通用模板、逐通道 ✅/🔧/⚠️/❌ verdict、WS2812 单脚周期时间编码墙、PSW/内联汇编边界）
- [x] **ADR-0076（Accepted 2026-08-30）** §2.4 矩阵 ⚠️/❌ 项标注 A/B 分类；落地结论尾注改为 Stage 2/3 路线；新增 §2.5 时钟保真模型与后端边界（功能 µs 粗账、A/B 两类缺口、双后端策略、ISS 按需触发、小家电域频率结论）

> 轨 B（延后）：`thermal_heater_plate` unisim 插件连续热平衡（`step(dtUs)`，analog_knob 为结构模板）、wasm 热平衡验证、夜间长跑；tier-c（完整 ADC_Ldo 原厂例程：system.h/gpio.h shim + 19 ISR 桩）、AN63 内部通道、ADCLDO VSEL 效应、故障上电锁存。
