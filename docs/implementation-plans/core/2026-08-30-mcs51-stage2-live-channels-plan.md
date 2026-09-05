# MCS-51 仿真 Stage 2：活通道收口（UART TX/RX、INT0/1、模拟绑定）

| 项 | 内容 |
|----|------|
| 创建日期 | 2026-08-30 |
| 关联 ADR | [ADR-0076](../../decisions/core/0076-mcs51-sim-backends-native-vs-iss-channel-roadmap.md)（Accepted 2026-08-30，D2 A 类）、[ADR-0074](../../decisions/core/0074-mcs51-channel1-external-read-pin.md)、[ADR-0075](../../decisions/core/0075-mcs51-production-wasm-target-headless.md) |
| 关联设计规范 | [`02-wink-micro-os/07-mcs51-simulation-interception.md`](../../design/02-wink-micro-os/07-mcs51-simulation-interception.md) §0 看板②、§2.4 矩阵、§2.5（A/B 分类）、[`04-wasm-simulation/02-mechanisms/08-channel-routing.md`](../../design/04-wasm-simulation/02-mechanisms/08-channel-routing.md) |
| 范围 | `wink-micro-os/frameworks/mcs51/`（C++ 模型 + host fallback）、`targets/wasm/`（如需导出）、`test/mcs51/`（ctest）、`wink-micro-app/`（headless 载体 app）；ch3 模拟绑定的宿主侧接线在姊妹仓 unisim（跨仓任务，见 T4） |

## 1. 背景与目标

阶段 0（ADR-0075）已证通道-1 数字 GPIO 双向 headless 7/7。看板② 剩余 🔶/❌ 项经 ADR-0076 全部分类为 **A 类（模型缺失，native 可补，零改用户 Keil 码）**。本计划收口其中四个"便宜项"——均不依赖 Stage 3 的定时边沿注入队列：

1. **UART TX** 上活总线（现仅 putchar/console）；
2. **UART RX** 字节注入（SBUF+RI+向量 4，现完全未建模）；
3. **外部中断 INT0/1**（P3.2/P3.3 外部边沿 → IE0/IE1 → 向量 0/2，现未接）；
4. **ch3 模拟绑定**（`js_pal_adc_read_norm` 宿主侧 stub 0.0 → `arbiter.readAnalog` 活闭环）。

完成后看板② 活通道从 1 条（数字 GPIO）变 4 条（数字 + UART 双向 + 模拟）；UART/I2C 类 OLED、串口调试、电位器/NTC 类模拟插件进入可演示状态。

**不在本计划**（Stage 3，另立计划）：定时边沿注入队列（DHT/红外/超声波/软 UART RX）、外部计数 C/T、`_nop_` 重标定、PWM 占空量测、bit-bang I2C/SPI 从机模型。

## 2. 任务拆分

### T1 — UART TX 路由 `js_pal_uart_write`（通道-2 写方向上活）

- **现状**：`frameworks/mcs51/src/mcs51_uart.cpp::on_sbuf_write()` 仅 `putchar` console + 内存 capture；`js_pal_uart_write(uint8_t port, const uint8_t* buf, uint32_t len)` 桥符号已存在（轴 A `targets/wasm/pal_wasm_ch2_uart.c:161` 在用，生产 JS `wink_sim_js.js:124` 有 `Module['js_pal_uart_write']` override 钩子）。
- **改动**：
  1. `mcs51_uart.cpp`：SBUF 写钩子内在 console/capture 之后追加 `js_pal_uart_write(0, &b, 1)`（port 0 = 8051 唯一硬件 UART；声明经 `wasm_bridge.h` 同等 extern，mcs51 侧自建最小 extern 声明，不 include 轴 A PAL 头，守分层门禁）。
  2. host fallback：`mcs51_uni_bridge.cpp` 增 `js_pal_uart_write` 弱实现——环形记录 (port, byte)，提供 `wink_mcs51_host_uart_tx_*` 计数/取字节/reset 观测 API（镜像现有 gpio notify 日志模式）。
  3. wasm Node 桩：`test/mcs51/wasm/mcs51_wasm_node_stub.js` 增 `js_pal_uart_write` 导出（记录字节供 Node 断言）；生产 `wink_sim_js.js` 已有钩子，**零改动**。
- **验收**：
  - 新增 host ctest `test_mcs51_uart_tx_route`（或扩 `uart_printf` e2e）：固件 `SBUF='A'` 等标准写法后，host fallback 日志含精确字节序；console capture 行为不变（回归）。
  - wasm/Node：Node 桩收到同一字节序。
  - headless：见 T5。

### T2 — UART RX 模型（字节注入 → SBUF 影子 + RI + 向量 4）

- **现状**：`wink_mcs51_uart_on_read` 空实现；SBUF 读返影子（末个发送字节）；RI 仅影子存储。向量 4 派发路径已被 TX 使用（EA+ES 门控同槽）。
- **模型**（守 trap 四红线：注入是纯状态机、零延时、不推钟）：
  1. 新增 C-ABI 入口 `void wink_mcs51_uart_rx_push(uint8_t byte)`：
     - 门控：`SCON.REN`（bit4）= 0 时丢弃（硬件不收）；可选 `RI` 已置位时覆盖/丢弃策略——**采用覆盖 + 计数丢字节**（硬件 8051 无 FIFO，溢出行为依实现；模型记 `s_rx_dropped` 供观测，STRICT 下不 assert）。
     - 写 `wink_mcs51_sfr_shadow[SFR_SBUF] = byte`（注意：SBUF 读/写在硬件上是两个寄存器，影子只有一个地址——TX 写钩子在调用时已先取 shadow 值，RX 覆盖影子不影响 TX 路径；读 SBUF 的固件代码读到的即最新注入字节）。
     - 锁 `SCON.RI`（bit0）；EA+ES 均置位时 `wink_mcs51_dispatch_vector(4)`（与 TX 同向量，ISR 内软件分 TI/RI——标准 8051 写法，零改用户码）。
  2. host：`wink_mcs51_uart_rx_push` 即测试注入 API（post-init hook 或片边界驱动均可，无时间语义→可直接注入）。
  3. wasm/生产：`EMSCRIPTEN_KEEPALIVE` 导出 `wink_mcs51_uart_rx_push`（经 mcs51 bridge 层导出，不与轴 A `pal_wasm_ch2_uart.c` 的 ringbuf 入口混用——mcs51 DAL 全关、模型是 SFR 级，不走 pal_uart）；Node 桩测试经 `Module._wink_mcs51_uart_rx_push(b)` 驱动。unisim 侧 UARTBus 插件 → 该导出的接线属跨仓活，列入 T4/T5 跟踪。
  4. `wink_mcs51_uart_reset` 清 RI 与丢字节计数。
- **验收**：
  - host ctest `test_mcs51_uart_rx`：标准 Keil 风格 ISR（`interrupt 4` 内判 RI、读 SBUF、清 RI）收到注入字节序列并写回 capture（回声固件）；EA/ES/REN 门控各一条负向用例（未开 REN 丢弃、未开 ES 不进 ISR 但 RI 置位可轮询）。
  - wasm/Node 同场景。

### T3 — 外部中断 INT0/1（P3.2/P3.3 → IE0/IE1 → 向量 0/2）

- **现状**：向量 0/2 槽与用户 `interrupt 0/2` 注册机制都在（28 向量表）；缺外部边沿→TCON.IE0/IE1 锁存。外部电平现仅在固件 plain-read 时经 `js_pal_gpio_read_state` 拉取（瞬时采样），中断需要"世界变化即感知"。
- **模型**：
  1. 采样点：**配额片边界**（10ms yield 点，外部世界本来就只在片边界更新）+ 微步让出点（`wink_mcs51_microstep`）调一个 `mcs51_extint_poll()`：对线性 pin 26（P3.2/INT0）、27（P3.3/INT1）调 `js_pal_gpio_read_state`，与上次采样电平比较得边沿。
     - 注：片内多次翻转不可见（外部世界片内冻结，与现 Read-Pin 语义一致）；NEC 红外等片内协议靠 Stage 3 定时边沿队列，不靠本模型。
  2. 判据（标准 8051）：
     - `ITx=1`（TCON bit0/2，边沿模式）：**下降沿**（INT 低有效，1→0）锁 `IEx`（TCON bit1/3）；硬件边沿检测在真实芯片上片内也可触发，模型退化到片边界采样，文档注明。
     - `ITx=0`（电平模式）：外部脚为低期间 IEx 保持/置位（模型：采样为低则锁 IEx，向量派发后**不**自动清——电平模式硬件响应中断期间保持低即反复请求；固件通常边沿模式。模型按"低电平且 EXx+EA 则每片派发一次"实现，限频天然 = 片率 100Hz，文档注明退化语义）。
     - IEx 由固件 RETI 后硬件自动清（边沿模式）——模型在 `dispatch_vector(0/2)` 返回后清 TCON.IEx（镜像定时器向量的标志处理；先核实现有 timer 派发对 TFx 的清理位置保持一致）。
  3. 门控：`IE.EA` + `IE.EX0/EX1`（bit2/bit6）；向量号 0/2。
  4. host：经现有 `wink_mcs51_host_set_ext_pin(26, 0/1)` 造边沿；wasm：Node 桩 `_mcs51_wasm_ext_pin_state` 已导出 getter，直接复用。
- **验收**：
  - host ctest `test_mcs51_extint`：Keil 风格 `interrupt 0` ISR 翻转某 SFR/变量；注入 1→0 边沿后跑若干 tick，断言 ISR 执行；IT0=0 电平模式一条用例；EX0=0 负向用例。
  - wasm/Node 同场景。
  - **红线**：RMW/锁存语义不变；poll 只读外部态、不写脚。

### T4 — ch3 模拟绑定（`js_pal_adc_read_norm` → `arbiter.readAnalog`）

- **现状**：C 侧链路通（mcs51 12-bit 注入轨 `js_pal_adc_read_norm(32+ch)` → ADC0832 消费 `&0xFF`、CMS8S 片内 12-bit）；host fallback 返 0.0（`mcs51_uni_bridge.cpp:33`），生产 JS 有 `Module['js_pal_adc_read_norm']` override 钩子（`wink_sim_js.js:137`）但 unisim 侧未接 `arbiter.readAnalog`；ADC0832/iron_ntc 现靠测试注入闭环。
- **拆分**：
  1. **本仓**：host ctest 侧无需改（注入轨已有 `mcs51_adc_set_value`）；确认生产 wasm 链接导出面无缺口（`js_pal_adc_read_norm` 已是 import，无新导出）。
  2. **跨仓（unisim）**：headless runner / worker 的 `createUnisimImports` 提供 `Module['js_pal_adc_read_norm']`（或等价 import 注入）→ `arbiter.readAnalog(pin)` 归一化返 0..1；映射规则 pin = `32+ch`（mcs51 注入轨约定）与 device-tree 模拟通道声明对齐。**此任务在姊妹仓执行，本计划只跟踪验收**。
  3. **闭环实证**：iron_ntc 场景（或 `thermal_heater_plate`/analog_knob 插件）经 `winkcli sim run --mode headless` 跑活模拟——NTC 插件温度 → arbiter → `js_pal_adc_read_norm` → ADC0832 位翻转回注 → 固件 bang-bang → 继电器脚写回插件，**全程无测试注入 API**。
- **验收**：headless 场景断言继电器在设定点翻转（复用 iron_ntc e2e 的冷热两态断言，数据源从注入改活插件）；若跨仓接线本周期不可得，本任务降级为"本仓侧就绪 + 跨仓 issue/计划挂起"，不阻塞 T1–T3 合入。

### T5 — headless 在线实证（生产 wasm + 真实插件）

- **载体**：
  - TX：新增微型生产 app `mcs51_uart_hello`（或把现有 `uart_printf` sample 包装为 app：`wink-app.json` + scenario，固件经 SBUF 周期性发 `"HELLO\n"`），headless UART 总线 spy 插件断言收到字节序。
  - RX：同一 app 回声逻辑（收字节 → ISR/轮询 → 回发），场景经 UARTBus 推字节，断言回声；若 unisim 侧 UART RX→导出接线（T2.3）未就绪，RX headless 降级为 ctest 证据，文档看板如实标注 🔶。
  - INT0：新增 `mcs51_button_led_int`（或在 app 目录加变体）：按键接 P3.2 用 `interrupt 0` 翻转 LED（与轮询版 `mcs51_button_led` 对照），headless 7 步场景同形断言 LED on/off。
  - 模拟：T4.3 iron_ntc 活闭环。
- **资产入库约定**同 ADR-0075（device-tree.json + wink_simulator.js 入库，.wasm 不入库）。

## 3. 验收标准（总）

| 门 | 标准 |
|---|---|
| host ctest（MSVC + MinGW） | 现有 mcs51 19/19 不回归；新增 uart_tx_route / uart_rx / extint 全绿（计数 19→22） |
| wasm/Node | 现有 8/8 不回归；新增三个 wasm 用例全绿（`-sERROR_ON_UNDEFINED_SYMBOLS=1`） |
| 生产 headless | TX spy 断言通过（`mcs51_uart_hello` 2/2）；INT0 按键→LED 通过（`mcs51_button_led_int` 10/10，重构前树）；模拟活闭环通过（`mcs51_analog_threshold` 8/8，跨仓 `afc54d68`+`81b94565`）；UART RX live 回声通过（`mcs51_uart_echo` 4/4，跨仓 `cf19d412`） |
| 分层门禁 | `python wink-tools/wink.py lint arch --pack layering --pack api` 无发现 |
| 零增量 | 默认 esp32 app（avoidance_car）wasm 配置+构建通过、mcs51 gate 静默；ESP_PLATFORM 树零 mcs51 符号 |
| 文档 | 看板② 五行状态更新（TX/RX/INT0/模拟 → ✅ 或如实 🔶）；§3.1 目录树/§3.2 API 面/§3.4 测试矩阵回写；ADR-0076 §5 Stage 2 项勾销 |

## 4. 风险与约束

- **SBUF 单影子**：8051 硬件 SBUF 收/发是两个物理寄存器、同一地址。模型单影子下 RX 覆盖影子不影响 TX（TX 钩子在写语句内即时取值）；需在 ctest 加"TX 后紧接 RX 注入"交错用例防回归。
- **INT 电平模式语义退化**：片边界采样（100Hz）替代硬件异步检测；电平模式重复派发限频 100Hz。小家电按键中断几乎全用边沿模式（ITx=1），退化可接受，须在 §2.4/看板注明。
- **跨仓依赖（已解除）**：T4.2（模拟）与 T2.3（UART RX 上 UARTBus）的 unisim 侧接线原不在本仓；已于 2026-08-30 在 sister `wink-ai` 已提交树落地（`afc54d68`/`81b94565`/`cf19d412`）并 headless 实证，headless 看板由 🔶 转 ✅。数字引脚输入回归亦已闭环：sister `8d06a4e8` 修复 `PluginContext.writePin/analogWrite` behavioral 闸位（见 §6），两个数字输入载体当前树全绿。
- **红线不变**：trap 四红线（RX 注入/INT poll 均为纯状态机、零延时、不推虚拟钟）；RMW 永不读外部脚；`#ifdef SIMULATION` 在最底层；无 `-fpermissive`；vendor 夹具只读不入库。

## 5. 时间线（建议）

1. T1 TX 路由（0.5 天，最小闭环先通）
2. T2 RX 模型（1 天）
3. T3 INT0/1（1–1.5 天，含 poll 挂接点与 timer 标志清理对齐核实）
4. T5 headless 载体 app + 场景（1 天，与 T1/T3 并行收尾）
5. T4 跨仓接线跟踪 + iron_ntc 活闭环（0.5 天本仓侧 + 跨仓）
6. 回归 + lint + 文档回写（0.5 天）

## 6. 完成状态（2026-08-30 执行记录）

| 任务 | 状态 | 证据 |
|---|---|---|
| T1 UART TX → `js_pal_uart_write` | ✅ 完成 | SBUF 写钩子上 UARTBus TX 时间线；host/wasm ctest + headless `mcs51_uart_hello`（`ASSERT_BUS_PAYLOAD` 2/2） |
| T2 UART RX 模型 | ✅ **完成（活通道 live-proven）** | fiber drain 队列→SBUF 影子+RI+向量 4；host `uart_echo` + wasm/Node ctest 闭环。**活喂字节已跨仓接通**：sister `wink-ai` `cf19d412` —— `UartBus.sendToFirmware` 优先解析 `wink_mcs51_uart_rx_push`（容忍 emscripten 下划线前缀），回落 `pal_wasm_push_uart_rx_byte(port,b)`；`BusDomainHandler.setWasmExportsFn` 解决 headless 上下文先于 wasm 实例化的导出晚绑定。headless 载体 `mcs51_uart_echo`（embedded `8d3e66f`）`INPUT_BUS` 推 "A"/"BC" → 固件经向量-4 ISR 收回 → polled TX 回声，`ASSERT_BUS_PAYLOAD` 4/4 |
| T3 INT0/1 外部中断 | ✅ 完成 | `mcs51_extint.cpp`：边沿/电平、10ms 节流、reset 基线保留、重入保护、HiZ→idle-HIGH；host 模型直测 7 例 + Keil e2e（host+wasm/Node）+ headless `mcs51_button_led_int` 10/10 |
| T4 ch3 模拟绑定 | ✅ **完成（活闭环 live-proven）** | C 侧 `js_pal_adc_read_norm(32+ch)`→12-bit 通；跨仓两缝已落地 sister `wink-ai`：`afc54d68`（`unisim-bridge-factory.ts` `js_pal_adc_read_norm` 从硬编 `return 0.0` 改接 `arbiter.readAnalog(pin)`，无驱动返 0 零回归）+ `81b94565`（`headless-domain-context.ts` 把共享 `PinArbiter` 传入 `AdcDomainHandler`——此前 headless ADC 写进断线 store）。headless 载体 `mcs51_analog_threshold`（embedded `5cf1c6d`）：`INPUT_ANALOG adcChannel:32` valueNorm 0.8→0.2→0.8 → CMS8S 片内 12-bit ADC → 固件阈值翻转 P1.0 LED，`ASSERT_POINT` 8/8。iron_ntc 插件路径同源（同 arbiter 模拟轨），未单独跑 |
| T5 headless 载体 | ✅ 完成 | `mcs51_uart_hello`（TX 信标）+ `mcs51_button_led_int`（/INT0 ISR）+ `mcs51_uart_echo`（RX→TX 回声，embedded `8d3e66f`）+ `mcs51_analog_threshold`（模拟阈值，embedded `5cf1c6d`），均未修改 Keil 源、生产链接、资产按 ADR-0075（device-tree.json + wink_simulator.js 入库，.wasm gitignore）。四条活通道（数字 GPIO / UART TX / UART RX live / 模拟）全部 headless 实证 |

**回归**：MSVC host mcs51 ctest **33/33**（23 host + 10 wasm/Node）全绿；arch lint（layering+api）无发现；ESP32 mcs51 gate 静默零增量。

**提交（embedded）**：`bae6c9f`（T3 框架+测试）、`a77b117`（extint HiZ→idle-HIGH 精化）、`41434dc`（mcs51_button_led_int app）、`c7b804a`（mcs51_uart_hello app）、`5cf1c6d`（mcs51_analog_threshold app，T4）、`8d3e66f`（mcs51_uart_echo app，T2.3）。
**提交（sister `wink-ai` unisim）**：`afc54d68`（T4 `js_pal_adc_read_norm`→arbiter 模拟轨）、`81b94565`（T4 headless AdcDomainHandler 绑定共享 PinArbiter）、`cf19d412`（T2.3 `UartBus.sendToFirmware` 活喂 mcs51/esp32 + `BusDomainHandler.setWasmExportsFn` 晚绑定）。

**跨仓已收口（原前置告警解除）**：T4 模拟活桥与 T2.3 UART RX 活喂字节均已在 sister `wink-ai` **已提交**树落地并 headless 实证（模拟 8/8、UART 回声 4/4），不再挂起。

**数字输入回归已修复（sister `8d06a4e8`，2026-08-30，本计划外跟进已闭环）**：sister `0c3a7609`（multi-architecture headless scenario engine）首次把 `accuracyMode:'behavioral'` 传进 `PluginContext`（此前 ctor 默认 `'timing'`），暴露 legacy `PluginContext.writePin/analogWrite` 的 behavioral 早退闸位于 `arbiter.setDriver` 之前——source 插件（按键）按下电平永不落 PinArbiter，固件 `js_pal_gpio_read_state`→`arbiter.readPin` 恒读 idle HIGH。修复把闸位移到 arbiter 驱动之后（功能电平恒驱，仅 timing 波形边队列 behavioral 跳过），对齐 `GpioDomainHandler.write`/`AdcDomainHandler.writeNorm`。当前树 headless：`mcs51_button_led` 7/7、`mcs51_button_led_int` 10/10 全绿，五载体全 PASS（UART/模拟本就不受影响）。

**遗留（Stage 3）**：定时边沿注入队列（DHT/红外/超声波/软 UART RX 共用）、外部计数 C/T、`_nop_` 重标定 + 量子细化、I2C/SPI 从机模型（ADR-0076 A 类，另立计划）。
