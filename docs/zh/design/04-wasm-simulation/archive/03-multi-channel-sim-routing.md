# 4.3 嵌入式仿真多通道路由与外设选型架构 (Multi-Channel Sim Routing & Selection Architecture)

| 项 | 内容 |
|----|------|
| 状态 | Living Spec（活文档） |
| 关联 ADR | [ADR-0003 仿真可信度边界](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)、[ADR-0002 双 target 同源](../../../decisions/unisim/0002-dual-target-compilation.md)、[ADR-0040 Arduino 语义仿真门禁](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md) |
| 关联实现 | `wink-ai/packages/unisim`、`wink-ai/packages/embedded-frontend`、`wink-micro-os/targets/wasm` |
| 保真专规 | [05-simulation-consistency-and-fidelity-spec.md](./05-simulation-consistency-and-fidelity-spec.md) |

在嵌入式 WebAssembly 仿真中，核心矛盾是**高真机一致性（Hardware-Firmware Fidelity）**与**浏览器运行性能（Web Performance）**的平衡。

本规范定义 UniSim 的**四通道 + PWM 子类平台旁路路由架构**，并给出外设选型决策指南。目标：在浏览器中保持流畅交互的同时，让 App / BAL / DAL **尽可能跑与真机同源的驱动逻辑**。

> **范围**：本篇只覆盖仿真多轴中的 **A. 外设物理源**。时间基、定时器、中断、调度、故障门禁见目录 [README 多轴总览](./README.md) 与 [05](./05-simulation-consistency-and-fidelity-spec.md) / [08](./08-simulation-consistency-checklist.md)。

---

## 0. 仿真可信度边界（先于通道选型）

本规范遵循 [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)：**行为级（causal）高保真**，不承诺 cycle / 电气级保真。

| 保真层级 | 保证内容 | 不保证内容 | 典型验证场景 |
|---|---|---|---|
| **L1 逻辑/因果** | App/BAL/DAL 状态机、错误码、超时语义、协议打包与解析 | — | 避障状态机、OLED 刷新逻辑 |
| **L2 协议/信号语义** | I2C/UART/SPI **事务 payload**、GPIO 因果沿、PWM **duty 语义**、ADC raw 语义 | bit 级波形、线阻抗、参考电压漂移 | SSD1306 帧缓冲、寄存器读写从机 |
| **L3 时序/电气近似** | 仅在 `timing` Accuracy Mode 下，对有限场景做虚拟时钟脉宽/沿近似 | cycle 级、抢占中断嵌套、模拟前端非线性 | HC-SR04 echo 脉宽捕获（近似） |

> **措辞约束**：下文「同源」均指 **L1/L2 驱动逻辑同源**，不是「仿真可替代真机时序与电气验证」。时序与电气级仍须真机。

---

## 1. 核心架构设计原则

传统模拟器若在微秒/纳秒级逐 bit 翻转电平（如 115200 波特率 UART、400kHz I2C），每秒可达数十万次 JS↔Wasm 跨边界调用，浏览器主线程会卡死。

早期曾在 DAL 内用 `#ifdef SIMULATION` 做**业务直通旁路**（直接返回厘米、摄氏度等）。工程实践表明：这会撕裂仿真与真机驱动路径，**协议换算 / 超时 / 错误恢复无法在仿真中被测到**。

### 1.1 分层同源契约（Homology Boundary）

```text
┌ App / BAL / DAL（API + 实现） ─── 禁止仿真业务特化；目标：零 `#ifdef SIMULATION`
├ PAL / HAL API（双 target 同签名）─ 接口形态稳定，真机与 Wasm 共用声明
├ PAL Wasm 实现 / `wasm_dev_*` ──── 唯一合法的平台旁路落点（可仿真特化）
└ JS Plugin / ProductWorld ──────── 只产生「物理量来源」，不得替代 DAL 逻辑
```

### 1.2 三条铁律

1. **只替换物理量来源（Physical Source Substitution）**  
   仿真可替换：引脚电平、脉宽沿、总线从机响应字节、ADC raw、缓冲区内容。  
   仿真不可替换：DAL 内的单位换算、CRC/校验、超时判定、重试与错误恢复。

2. **Platform 层旁路（PAL Convergence）**  
   所有拦截与路由下沉到 **PAL / HAL**（及 Wasm target 实现），禁止在 DAL 内嵌入业务级捷径。

3. **Fail-Loud 选型（ADR-0040）**  
   新外设必须映射到下文某一通道；无法映射时禁止私自 DAL `#ifdef` 捷径，应先扩展 PAL 抽象或新增通道契约。

### 1.3 Accuracy Mode 与保真门禁

| 模式 | 可支撑的保真主张 | 禁止用作证据的场景 |
|---|---|---|
| `behavioral` | L1 状态机、L2 payload / StateChannel 语义 | 沿触发 IRQ、脉宽捕获、去抖时序 |
| `timing` | L2 沿因果 + 有限 L3 虚拟时钟脉宽近似 | cycle / 电气级宣称 |

**门禁**：凡宣称「脉冲器件（如超声波）高一致」的用例，必须在 **`timing`** 下验证；`behavioral` 结果不得作为脉宽 / 中断一致性证据。

---

## 2. 四通道 Platform 仿真路由机制

基于 PAL 接口形态分流（旁路的是**传输成本**，不是驱动逻辑）：

```text
                             [ Wasm 固件：App / BAL / DAL 逻辑同源 ]
                                                     │
        ┌──────────────────────┬─────────────────────┼─────────────────────┬──────────────────────┐
        ▼                      ▼                     ▼                     ▼                      ▼
 [1. Pin-Level]        [2. Bus Protocol]     [2b. PWM Duty]      [3. Analog Signal]    [4. Buffer Payload]
 pal_gpio_*            pal_i2c/spi/uart_*    pal_pwm_set_duty    pal_adc_read          pal_ws2812_write /
 PinArbiter            I2CBus/SPIBus/UARTBus notifyDutyChange    (raw / voltage)       pal_camera_* / SAB
 按键·LED·超声波脉冲     OLED·总线传感器·UART   舵机·电机 duty       NTC·光敏·摇杆           WS2812·摄像头帧
        │                      │                     │                     │                      │
        └──────────────────────┴─────────────────────┴─────────────────────┴──────────────────────┘
                                                     ▼
                                      [ SimWorker + SimulationPluginHost ]
                                                     │
                                                     ▼
                         [ embedded-frontend：ControlHub / World UI / ProductWorld(3D) ]
```

> **说明**：PWM Duty 在实现上常与总线导入同文件装配，但语义上是**调制/定时量**，不是字节事务；选型时按 **通道 2b** 处理，避免与 I2C/SPI 混淆。

### 2.1 通道 1：电平级通道 (Pin-Level)

| | |
|---|---|
| **同源保留** | DAL 触发时序、`pulse_in`/捕获、超时与错误处理 |
| **旁路替换** | 引脚电气源：`PinArbiter` 上的驱动电平与沿时刻 |
| **PAL 锚点** | `pal_gpio_read` / `pal_gpio_write` / `pal_gpio_pulse_in`（或等价 capture） |

* **运行机制**：固件 GPIO 读写映射到 **`PinArbiter`**（多源驱动仲裁与阻抗/悬空语义）。插件通过 `writePin` 注入；UI 可通过 ideal 驱动注入。
* **脉冲器件终态（超声波）**：

```text
ProductWorld / ControlHub
  → UltrasonicPlugin（持有 distanceCm）
  → 换算 echoUs，按 VirtualClock 在 PinArbiter 注入 ECHO 高低沿
  → C：pal_gpio_write(TRIG) + pal_gpio_pulse_in(ECHO)   ← 测量路径同源
  → DAL：脉宽→距离换算 / 超时 / 错误码                 ← 业务路径同源
```

### 2.2 通道 2：协议总线通道 (Bus Protocol)

| | |
|---|---|
| **同源保留** | DAL 寄存器序列、命令打包、应答解析、错误恢复 |
| **旁路替换** | 电气 bit 时序；以 **事务级 payload** 交给虚拟从机 |
| **PAL 锚点** | `pal_i2c_transfer` / `pal_spi_transfer` / `pal_uart_write|read` |

* **数据传输**：同 Worker 内对 Wasm Heap 做同步 `Uint8Array` 切片，经 `I2CBus` / `SPIBus` / `UARTBus` 分发给插件解析器（非跨线程 MessageChannel 零拷贝）。
* **示例**：SSD1306 — C 侧完整走 I2C 写缓冲；`MonoOledPlugin` 解析命令/数据并发布 framebuffer。

### 2.3 通道 2b：PWM Duty 子类 (Modulation Semantic)

| | |
|---|---|
| **同源保留** | DAL 角度/速度→duty 换算（若在 DAL）、使能与限幅逻辑 |
| **旁路替换** | 微秒级 PWM 波形边沿 |
| **PAL 锚点** | `pal_pwm_set_duty` → `notifyDutyChange` → 插件状态 / 3D 关节 |

保真层级默认 **L2（duty 语义）**；不宣称载波周期级 L3，除非在 `timing` 下另有明确契约。

### 2.4 通道 3：模拟量信号通道 (Analog Signal)

| | |
|---|---|
| **同源保留** | DAL 对 raw 的标定、滤波、阈值与错误码 |
| **旁路替换** | ADC 通道上的电压/raw 来源 |
| **PAL 锚点** | `pal_adc_read(channel)`（及对称 DAC，若有） |

Plugin Channel / ControlHub 注入的是 **绑定到 ADC 通道的物理源**，不是 DAL 业务返回值（禁止直接 `return temperature_c`）。

### 2.5 通道 4：缓冲区旁路通道 (Buffer Payload)

| | |
|---|---|
| **同源保留** | 应用/DAL 对帧缓冲或 RGB 数组的填充与消费算法 |
| **旁路替换** | 非标极高频 bit 时序（如 WS2812 0.4μs 归零码）或海量帧的逐字节 GPIO 翻转 |
| **PAL 锚点** | `pal_ws2812_write(buf,len)` / `pal_camera_capture` / `SharedArrayBuffer` 直流 |

仍必须走 **具名 PAL 缓冲区 API**，不得退化为在 DAL 里 `#ifdef` 直接画 UI。

---

## 3. 外设仿真选型决策指南

### 3.1 选型决策表

「落地状态」：`Landed` 已端到端可用 · `Partial` 机制有、路径未收敛 · `Planned` 架构预留。

| 外设类别 | 代表器件 | 通道 | PAL 锚点 | 落地 | 推荐 Accuracy | 同源保留 / 旁路替换 |
|---|---|---|---|---|---|---|
| 开关量/指示 | 按键、LED、继电器 | **1 Pin** | `pal_gpio_*` / PinArbiter | Landed | behavioral 即可；测 IRQ 用 timing | 保留读写与中断订阅；替换引脚电平源 |
| 脉冲时序传感器 | HC-SR04 | **1 Pin** | `gpio` + `pulse_in`/capture | **Partial** | **timing（强制）** | 保留捕获与换算；替换 ECHO 沿源（见 §5 缺口） |
| 总线显示屏 | SSD1306 等 | **2 Bus** | `pal_i2c/spi_transfer` | Landed | behavioral | 保留协议打包；替换 bit 时序为 payload |
| 总线传感器 | MPU6050、AHT20 等 | **2 Bus** | 同上 | Partial～Planned | behavioral | 保留寄存器逻辑；插件做虚拟从机 |
| 串口模块 | GPS NMEA、AT 模组 | **2 Bus** | `pal_uart_*` | Partial（总线有，UI 少） | behavioral | 保留帧解析；替换电气波形 |
| 舵机/电机 PWM | SG90、H 桥 | **2b PWM** | `pal_pwm_set_duty` | Landed（duty） | behavioral；测沿用 timing | 保留 duty 语义路径；不仿真载波边沿 |
| 模拟量传感器 | NTC、LDR、摇杆 | **3 Analog** | `pal_adc_read` | Planned | behavioral | 保留标定/阈值；替换 raw 源 |
| 非标高频灯珠 | WS2812B | **4 Buffer** | `pal_ws2812_write` | Planned | behavioral | 保留 RGB 缓冲语义；不仿真归零码 |
| 高吞吐媒体 | Camera / I2S | **4 Buffer** | capture / SAB | Planned | behavioral | 保留算法消费；替换帧注入方式 |

### 3.2 选型决策树

```text
                              [ 拿到一个新的外设 ]
                                       │
                    ┌──────────────────┴──────────────────┐
                    ▼ (是)                                ▼ (否)
     [ 1. 标准数字总线字节事务? ]              [ 2. 是否 PWM duty / 电机调制? ]
           (I2C / SPI / UART)                              │
                    │                         ┌────────────┴────────────┐
                    ▼                         ▼ (是)                    ▼ (否)
            【通道 2：Bus】              【通道 2b：PWM】     [ 3. 纯模拟量 ADC/DAC? ]
                                                                  │
                                                     ┌────────────┴────────────┐
                                                     ▼ (是)                    ▼ (否)
                                              【通道 3：Analog】   [ 4. GPIO / 脉冲捕获? ]
                                                                        │
                                                           ┌────────────┴────────────┐
                                                           ▼ (是)                    ▼ (否)
                                                    【通道 1：Pin】   [ 5. 高吞吐/非标极高频? ]
                                                                              │
                                                                 ┌────────────┴────────────┐
                                                                 ▼ (是)                    ▼ (否)
                                                          【通道 4：Buffer】     【Fail-Loud】
                                                                              扩展 PAL 或立 ADR
                                                                              禁止 DAL 业务 #ifdef
```

---

## 4. Plugin Channel 保真红线

`js_sim_get_plugin_channel` / ControlHub / `stateChannels` 是 **插件与宿主之间的物理语义 SSOT**，不是 DAL 业务旁路 API。

| 允许 | 禁止 |
|---|---|
| UI/3D → Plugin 注入 `distanceCm`、电压、寄存器镜像 | DAL **直接**读业务语义 channel 并 `return` 给应用 |
| Plugin 内用 channel 计算后，写入 **Pin / Bus 从机 / ADC 源** | 用 channel 跳过 `pulse_in` / 总线事务 / ADC 采样路径且不标注过渡 |
| 观测面、Trace、UI 绑定读 channel | 在 DAL 内 `#ifdef SIMULATION` 调用 channel 等价物 |

**超声波收敛方向**：channel 只喂给 Plugin；测量路径必须回到通道 1 的沿注入（见 §2.1）。C 侧 `wasm_dev_*` 内「读 cm 再换算 μs」仅可作为 **Deprecated shortcut**，不得写入新器件样板。

---

## 5. 架构现状与保真收敛状态 (SSOT Alignment)

对照 `packages/unisim`、`packages/embedded-frontend`、`wink-micro-os/targets/wasm`：

1. **PinArbiter**：GPIO 电气 SSOT（取代早期文档中的 `PinManager`）。
2. **总线传输**：同 Worker 同步 Heap slice → `I2CBus` / `SPIBus` / `UARTBus`；非 MessageChannel 跨线程零拷贝。
3. **OLED**：Scheme-A 地址短路由已退役；统一 `js_pal_i2c_transfer` → `MonoOledPlugin`。
4. **旧专用 import**：`js_sim_trigger_ultrasonic` / `js_sim_measure_echo_pulse_us` 已淘汰；勿再写入新设计。
5. **Trace**：DAL/PAL 不直接打 trace；`pal.transfer` 类摘要由 Worker 在 `js_pal_*` 返回时记录。
6. **ProductWorld / Raycaster**：3D 碰撞属表现层；距离注入 Plugin，**严禁**作为 C DAL 返回值。

### 5.1 已知保真缺口（须收敛）

| 缺口 | 现状 | 目标 | 优先级 |
|---|---|---|---|
| 超声波测量捷径 | `wasm_dev_ultrasonic_get_pulse_us` 优先 `js_sim_get_plugin_channel(..., "distanceCm")` 并在 C 内 cm→μs | Plugin 注入 ECHO 沿 + 同源 `pulse_in`；删除/降级 C 内换算捷径 | P0 |
| DAL 过时注释 | `dal_ultrasonic.c` 仍提及已淘汰 `js_sim_trigger/measure` | 注释与 ADR-0003 演进后的 PAL 路径对齐 | P1 |
| 通道 3/4 | 多为架构预留 | 落地时补 PAL API + 插件 + 选型表状态升级为 Landed | P2 |
| UART/SPI UI | 总线在引擎侧存在，前端渲染消费者少 | 按器件补 World/Hub 绑定，不改通道模型 | P2 |

---

## 6. 保真验收清单（新增外设 / 改旁路时自检）

- [ ] DAL / App **无**仿真业务分支（无返回物理语义捷径的 `#ifdef SIMULATION`）。
- [ ] 旁路锚点落在 **PAL API 或 Wasm PAL 实现**，且能指出通道 1 / 2 / 2b / 3 / 4。
- [ ] 表格已填写：同源保留项、旁路替换项、落地状态、Accuracy Mode。
- [ ] 脉冲 / 沿 / 超时相关用例可在 **`timing`** 下复现；未用 `behavioral` 冒充时序证据。
- [ ] Plugin Channel 仅作物理源或观测；测量类路径可被 Trace 到对应 `js_pal_*`。
- [ ] 无法归类时已 Fail-Loud（扩展 PAL 或 ADR），未私加 DAL 捷径。

---

## 7. 旧方案淘汰与迁移说明 (Deprecations)

| 状态 | 项 | 说明 |
|---|---|---|
| **已淘汰** | DAL 业务直通（整段驱动 `#ifdef SIMULATION` 返回业务量） | 撕裂同源路径，测试覆盖假象 |
| **已淘汰** | 驱动内嵌 3D Raycaster / 直接 `js_sim_get_distance` | 表现层不得穿透 DAL |
| **已淘汰** | 每器件专用 `js_sim_trigger_*` / `js_sim_measure_*` 作为长期 ABI | 统一为 Pin/Bus/ADC/Buffer + Plugin Channel |
| **过渡中** | C `wasm_dev_*` 读 `distanceCm` 并本地换算脉宽 | Deprecated shortcut；收敛到 §2.1 沿注入 |
| **演进说明** | 相对 ADR-0003 决策 2 原文 | 「只换物理量来源」仍然有效；落点从「DAL 内最底层 `#ifdef`」进一步下沉为 **PAL Wasm 实现 + Plugin**，DAL 目标零仿真宏 |

---

## 8. 相关文档

* [02-virtual-peripheral-registry.md](./02-virtual-peripheral-registry.md) — 虚拟外设与 DeviceTree
* [05-simulation-consistency-and-fidelity-spec.md](./05-simulation-consistency-and-fidelity-spec.md) — 一致性与保真专规
* [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md) — 可信度边界
* UniSim 实现侧：`packages/unisim/docs/ARCHITECTURE.md`、`sim-observation-layers.md`、`BUS_MODELS.md`

