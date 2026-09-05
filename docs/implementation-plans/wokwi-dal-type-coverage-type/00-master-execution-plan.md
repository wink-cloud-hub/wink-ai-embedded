# Wokwi-Elements ↔ DAL `type` 全量外设覆盖大型执行计划总纲

| 项 | 内容 |
|---|---|
| **计划名称** | Wokwi-Elements 组件库全量 50 元件 ↔ DAL 控制语义族落地主计划 |
| **创建日期** | 2026-08-05 |
| **基线文档** | [`2026-07-29-wokwi-elements-dal-type-coverage-review.md`](../../reviews/frontend/2026-07-29-wokwi-elements-dal-type-coverage-review.md) |
| **核心约束规范** | [`dal-role-architecture-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-role-architecture-spec.md) (Role 面板与动词契约 SSOT)、[`dal-api-consistency-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md) (v3.4.3 驱动硬规范) |
| **目标版本** | Wink OS DAL Driver Spec v2.0 |
| **执行策略** | 100% 无遗漏覆盖、防破坏性变更架构冻结、分批次 (P0/P1/P2) 推进、一组件一子计划 |

---

## 1. 执行总纲目标与防破坏性变更原则

### 1.1 总体目标
基于 `2026-07-29-wokwi-elements-dal-type-coverage-review.md` 的评审结论，对 `wokwi-elements` 组件库中全部 **50 个 `*-element.ts`** 进行标准化 DAL (Device Abstraction Layer) 控制语义驱动落地：
1. **7 个非外设组件**：6 个开发板 + 1 个无源电阻，明确排除。
2. **43 个外设组件**：归一化映射至 **28 个 DAL `type` 语义** 及 **2 个 Codegen 自动拆解复合元件**。
3. **9 个已落地 `type`**：保持向后兼容，补齐 5 大运行时防线。
4. **19 个待落地 `type`（1 🟡 Roadmap + 18 🆕 新增）**：按照 P0 $\rightarrow$ P1 $\rightarrow$ P2 三阶段逐一编写具体外设执行计划并推进编码落地。

### 1.2 三维抽象设计心法（DAL 架构护城河）
在各外设落地过程中，必须严格遵循 DAL 三维抽象原则，杜绝事后 API 破坏性重构：
- **`type` = 驱动护城河**：只要底层的通信协议、控制物理量单位（角度 / 步数 / 占空比 / 原始电压）和 C 驱动代码改变，必须建立新的 `type`。
- **`variant` = 同族避风港**：驱动代码框架不变，仅硬件接线、引脚排列、接口协议（I2C/SPI）或算法机制变了，在 `type` 内部用细分 `variant` 枚举消化，绝不向 App 暴露新的 API。全仓统一淘汰 `drive_mode` / `decode_mode` 等历史旧词。
- **`role` = 应用变形金刚**：底层驱动与采集由 DAL 固化，上层 App 以何种角色称呼它（如 `hmi_dial` vs `pulse_counter`），由 `role` 进行能力平面映射（严格遵循 [`dal-role-architecture-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-role-architecture-spec.md) 5 大能力域与 19 个标准 Role 动词契约定义）。

### 1.3 5 大资深架构底线 Guard（运行时健壮性防线）
所有新增/重构外设驱动必须强制通过以下 5 道运行时质量防线：
- **🛡️ Guard A: 低功耗预留 (`enable_pin`)**：所有 `sensor/*` 与 `actuator/*` POD `config_t` 必须统一预留 `int16_t enable_pin` (默认 `-1` 为未绑定)，支持休眠自动关断。
- **🛡️ Guard B: I2C Bus-Owner 总线共享**：所有 I2C DAL 驱动 `init()` 必须遵守“若 PAL I2C 总线已打开则复用”的共享契约。
- **🛡️ Guard C: 零值即默认 (Zero-as-Default)**：波特率/时钟频率等配置若为 `0`，自动推导为平台最佳默认值。
- **🛡️ Guard D: 高频事件底层中断托底 (Anti-Polling-Loss)**：对 `encoder`、`ir_receiver`、`ultrasonic`，底层 C 实现必须依赖 PAL EXTI 或 Timer Capture，严禁纯软件轮询。
- **🛡️ Guard E: 高带宽显示强制 DMA 异步化 (Anti-CPU-Blocking)**：对 `tft` 和 `led_matrix`，刷屏接口必须为真正异步契约 (`request_flush`/`is_flush_done`)，底层依赖 SPI-DMA 或 RMT-DMA。

### 1.4 外设 Variant 拓扑分析与芯片型号映射通用方法论 (依据 [`dal-best-practices.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-best-practices.md) §3)
在编写后续 20 个外设分计划时，**必须统一采用以下 4 步拓扑变体分析法**，确保“业务语义冻结 + 拓扑枚举消化”：

1. **市面主流芯片/模块全量检索统计**：
   - 检索并盘点该 `type` 外设在嵌入式/开源硬件领域的全部主流具体芯片、传感器型号与外围模块（如继电器的 `SRD-05VDC`、`G3MB-202P`、`HFE10`；电机驱动的 `TB6612`、`L298N`、`DRV8833` 等）。
2. **细分变体优先原则与确定性断言 (Subdivided Variant First & Deterministic Pin Map Invariant)**：
   - 描述硬件型号、接口协议（I2C/SPI）、电路拓扑（IN-IN/Phase-Enable）、算法机制的字符串选型，开发者一律统一收拢为细分 `variant` 枚举（如 `ssd1306_i2c` / `ssd1306_spi`；`in_in` / `phase_enable`；`matrix_4x4` / `adc_resistor_ladder`；`standard` / `logarithmic`）。
   - 每一个 `variant` 枚举值对应的物理引脚映射（Pin Map）必须**绝对唯一且确定**，严禁同一个 `variant` 值内部存在拓扑歧义。
   - 细分变体在 C 驱动中仅占用 1 字节整数枚举（`uint8_t`），配合 Codegen DCE 机制，**运行期 CPU 性能零开销、Flash 体积零冗余**。
3. **`affects_pins` 元数据显示标注**：
   - 在 Codegen YAML 中明确标注 `fields.variant.affects_pins: true`（变体改变引脚数量/功能，如电机/键盘）或 `false`（变体仅改变算法/屏驱，如旋钮/OLED），指引前端与 UniSim 执行物理引脚重排或静默无感切换。
4. **Codegen 芯片别名映射表 (`aliases` / `ic_to_variant_map`)**：
   - 具体芯片型号名仅存活于 Codegen 驱动描述中作为别名，由 `app_codegen` 自动将芯片型号映射为确切的 `variant` 枚举值 + 默认引脚/极性配置。

---


## 2. 全量外设执行进度 Checklist

### 2.1 统计概览
- **总 `type` 数量**：28 个主 `type` + 2 个 Codegen 拆解复合元件目标
- **已完成 (✅ Completed)**：9 个
- **开发中/已规划 (🟡 In Progress)**：1 个 (`stepper`)
- **待开发 (🆕 Planned)**：18 个（P0: 8 个 | P1: 6 个 | P2: 6 个，涉及 20 个独立外设分计划文档）

---

### 2.2 P0 级外设（覆盖 ~80% 入门应用，优先落地）

> **⚠️ 执行序修正（2026-08-05 核对）**：`analog_knob`(#1) 与 `analog_sensor`(#5) 均依赖 **PAL ADC 子系统**，而该子系统**当前完全不存在**（`pal_hal.h` 仅有 GPIO/PWM/I2C；`pal_resource_type_t` 无 ADC 通道；`wasm_bridge.h` 无 ADC 读通道）。
> 故：
> 1. 新增前置计划 **`00.5-pal-adc-subsystem-plan.md`**，由 #1 与 #5 共享；
> 2. **实际执行序改为 `relay`(#4) → `buzzer`(#3) 先行**（纯 GPIO/PWM，零 PAL 欠账），先跑通 "new-dal → YAML → lint → 单测 → 双 target" 全链路，再回到 ADC 家族。
> 3. 同时需前置定稿共享 role **`analog_input`**（现有 7 个 role 中无连续模拟输入 role，而 `user_surface` 的 `DEVICE-REQUIRES-ROLE` 为硬门禁）。

| # | 父目录 (Category) | type | 对应 Wokwi 组件 | 关键架构约束 | 进度状态 | 子计划文档路径 |
|---|---|---|---|---|---|---|
| 0.5 | pal | *(前置)* | — | **PAL ADC 子系统**：`pal_adc.h` + 三 target 实现 + `PAL_RESOURCE_ADC_CHANNEL` + wasm bridge + role `analog_input` | ✅ Complete | [`00.5-pal-adc-subsystem-plan.md`](./00.5-pal-adc-subsystem-plan.md) |
| 0.6 | infrastructure | *(前置/拓扑)* | — | **基础设施外设与拓扑选通**：`PCF8574` + `74HC138` + `TCA9548A` + PAL GPIO 多态句柄 + Codegen DAG 排序 | 🆕 Planned | [`00.6-infrastructure-devices-plan.md`](./00.6-infrastructure-devices-plan.md) |
| 1 | input | `analog_knob` | `potentiometer`, `slide-potentiometer` | HMI 调参，API 返回 `knob_promille` `[0,1000]`（**非 float 0~1.0**，ADR-0056 封闭后缀） | ✅ Complete | [`01-p0-input-analog-knob-plan.md`](./01-p0-input-analog-knob-plan.md) |
| 2 | input | `keypad` | `membrane-keypad` | 矩阵键盘，非阻塞行列扫描 `get_key` API | ✅ Completed | [`02-p0-input-keypad-plan.md`](./02-p0-input-keypad-plan.md) |
| 3 | output | `buzzer` | `buzzer` | 无源/有源蜂鸣器（PWM 调音 / GPIO 开关） | 🆕 Planned | [`03-p0-output-buzzer-plan.md`](./03-p0-output-buzzer-plan.md) |
| 4 | output | `relay` | `ks2e-m-dc5` | 继电器开关控制（高/低电平触发） | ✅ Completed | [`04-p0-output-relay-plan.md`](./04-p0-output-relay-plan.md) |
| 5 | sensor | `analog_sensor` | `photoresistor`, `gas`(AO), `flame`(AO), `sound`(AO) | 物理量模拟测量，返回 `_raw`/`_mv`，计算移至 BAL | 🚧 Blocked (待 0.5) | `05-p0-sensor-analog-sensor-plan.md` |
| 5.1 | sensor | `ntc` | `wokwi-ntc-temperature-sensor` | **电热小家电专属 NTC 驱动**：开路/短路安规原语，浮点摄氏度 `temp_degc` + 51 单片机零浮点定点十分之一摄氏度 `temp_ddegc` 双模 API | 🚧 In Progress | [`18-p0-sensor-ntc-temperature-plan.md`](./18-p0-sensor-ntc-temperature-plan.md) |
| 6 | sensor | `digital_sensor` | `gas`(DO), `flame`(DO), `sound`(DO) | 通用二值阈值触发（DO 比较器），保持语义纯粹 | 🆕 Planned | `06-p0-sensor-digital-sensor-plan.md` |
| 7 | sensor | `temp_humidity` | `dht22` | 单线专用数字时序，非阻塞状态机测量 | 🆕 Planned | `07-p0-sensor-temp-humidity-plan.md` |
| 8 | display | `lcd_char` | `lcd1602`, `lcd2004` | I2C/并行字符屏，零 Framebuffer 指令刷屏 | 🆕 Planned | `08-p0-display-lcd-char-plan.md` |

---

### 2.3 P1 级外设（运动、高频交互与高带宽显示）

| # | 父目录 (Category) | type | 对应 Wokwi 组件 | 关键架构约束 | 进度状态 | 子计划文档路径 |
|---|---|---|---|---|---|---|
| 9 | input | `ir_receiver` | `ir-receiver`, `ir-remote` | NEC 红外解码，**强制依赖底层中断/定时器捕获 (Guard D)** | 🆕 Planned | `09-p1-input-ir-receiver-plan.md` |
| 10 | output | `led_bar` | `led-bar-graph` | 多路 GPIO / 移位寄存器条形指示灯 | 🆕 Planned | `10-p1-output-led-bar-plan.md` |
| 11 | actuator | `stepper` | `stepper-motor`, `biaxial-stepper` | STEP/DIR 与 4 线相序双驱动 `drive_mode` | 🟡 In Progress | `11-p1-actuator-stepper-plan.md` |
| 12 | display | `tft` | `ili9341` | SPI 彩屏，**强制 Windowed API + SPI-DMA 异步刷屏 (Guard E)** | 🆕 Planned | `12-p1-display-tft-plan.md` |
| 13 | display | `led_matrix` | `neopixel`, `neopixel-matrix`, `led-ring` | WS2812 阵列，**强制 RMT/SPI-DMA 异步刷屏 (Guard E)** | 🆕 Planned | `13-p1-display-led-matrix-plan.md` |
| 14 | display | `seg_display` | `7segment` | 数码管，支持直接 GPIO 或 TM1637 驱动 | 🆕 Planned | `14-p1-display-seg-display-plan.md` |

---

### 2.4 P2 级外设（IoT/进阶传感与存储总线）

| # | 父目录 (Category) | type | 对应 Wokwi 组件 | 关键架构约束 | 进度状态 | 子计划文档路径 |
|---|---|---|---|---|---|---|
| 15 | sensor | `motion` | `pir-motion-sensor` | 仅限 PIR 人体红外移动侦测（不与数字阈值混用） | 🆕 Planned | `15-p2-sensor-motion-plan.md` |
| 16 | sensor | `imu` | `mpu6050` | I2C 6 轴加速度/陀螺仪芯片 | 🆕 Planned | `16-p2-sensor-imu-plan.md` |
| 17 | sensor | `load_cell` | `hx711` | 24-bit 专用 AFE 称重芯片，双线脉冲串行协议 | 🆕 Planned | `17-p2-sensor-load-cell-plan.md` |
| 18 | sensor | `heart_rate` | `heart-beat-sensor` | 模拟脉搏心率传感器 | 🆕 Planned | `18-p2-sensor-heart-rate-plan.md` |
| 19 | storage | `sdcard` | `microsd-card` | SPI 块设备接口，供 FAT/LittleFS 文件系统挂载 | 🆕 Planned | `19-p2-storage-sdcard-plan.md` |
| 20 | storage | `rtc` | `ds1307` | I2C 掉电保活 RTC，`dal_rtc_time_t` 防 2038 溢出 | 🆕 Planned | `20-p2-storage-rtc-plan.md` |

---

### 2.5 已落地外设基线（✅ Completed 巡检与 Guard 补齐）

| # | 父目录 | type | 对应 Wokwi 组件 | 当前状态 | Guard 补齐与维护要求 |
|---|---|---|---|---|---|
| 21 | input | `button` | `pushbutton`, `pushbutton-6mm`, `tilt-switch`, `slide-switch` | ✅ Complete | 确认支持消抖与低功耗/极性配置 |
| 22 | output | `led` | `led`, `rgb-led` | ✅ Complete | 支持单色与 RGB 多通道，WS2812 单灯支持 |
| 23 | actuator | `dc_motor` | — (H 桥) | ✅ Complete | 规范 `safe_off()` 为刹车/高阻断电 |
| 24 | actuator | `rc_servo` | `servo` | ✅ Complete | 50Hz PWM 角度控制，补齐 `enable_pin` |
| 25 | sensor | `encoder` | `ky-040`, `rotary-dialer` | ✅ Complete | 确认 PAL EXTI 托底（Guard D） |
| 26 | sensor | `ultrasonic` | `hc-sr04` | ✅ Complete | 确认非阻塞状态机与 Capture 托底（Guard D） |
| 27 | comm | `gps` | — (NMEA) | ✅ Complete | UART 异步环形缓冲区接收 |
| 28 | display | `mono_oled` | `ssd1306` | ✅ Complete | I2C/SPI 1KB 局部 Framebuffer |
| 29 | storage | `eeprom` | — | ✅ Complete | I2C Bus-Owner 共享 (Guard B) |

---

### 2.6 Composite 复合元件 Codegen 拆解 Checklist

| # | 复合元件名 | Wokwi 组件名 | Codegen 拆解策略 | 状态 | 拆解产物验证 |
|---|---|---|---|---|---|
| 30 | `dip-switch-8` | `dip-switch-8` | Codegen 设备树转换为 8× `input/button` 节点 | ⚙️ Codegen | 验证生成的 `wink-app.json` 正确扩展为 8 路独立开关 |
| 31 | `analog-joystick` | `analog-joystick` | Codegen 设备树转换为 2× `input/analog_knob` + 1× `input/button` 节点 | ⚙️ Codegen | 验证 X/Y 轴映射为 `analog_knob`，按键映射为 `button` |

---

## 3. 标准外设落地工作流与质量卡点 (Quality Gates)

每个新增/重构外设在执行子计划时，必须严格执行以下 6 步交付流程：

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────────────┐
│ 1. 脚手架生成   │───>│ 2. C 驱动实现   │───>│ 3. Codegen 驱动描述     │
│ (wink.py new-dal│    │ (dal_<type>.h/c)│    │ (codegen/drivers/*.yaml)│
└─────────────────┘    └─────────────────┘    └─────────────────────────┘
                                                        │
┌─────────────────┐    ┌─────────────────┐             ▼
│ 6. 静态 Lint    │<───│ 5. 仿真/单体测试│<───┌─────────────────┐
│ (6 个 pack 全跑)│    │ (Host/WASM Unit)│    │ 4. Role 契约集成│
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

> **⚠️ 路径修正（ADR-0051）**：驱动描述的 SSOT 是 **`wink-micro-os/codegen/drivers/<type>.yaml`**，**不是** `wink-tools/tools/codegen/drivers/<type>.py`。
> ADR-0051 已把 SSOT 路径从 tools 的 `*.py` 迁为可扫描扩展根，MVP 阶段**禁止 Python hooks**；`wink-tools` 下的 `*.py` 仅作兼容层/golden 对照（`register` 开关）。
> 另：基类名为 **`DriverBase`**（`drivers/base.py:30`），非 `DriverPlugin`。各分计划若写 Python 插件，均须按此更正。

### 质量卡点 (Checklist Guard Window)
- [ ] **Point 1: 脚手架规范**：使用 `python wink-tools/wink.py new-dal <type> --category <cat> --role <role>` 生成代码骨架。
- [ ] **Point 2: Zero Malloc**：C 驱动只使用 POD `config_t` 与 `instance_t` 静态分配，**严禁使用 `malloc/free`**。
- [ ] **Point 3: 非阻塞规范**：全量 API 遵守非阻塞契约（ADR-0017），禁用 `delay_ms()` 或死等 loop。
- [ ] **Point 4: 硬件托底**：高频/高带宽外设必须实现中断 ISR 或 DMA (Guard D / Guard E)。
- [ ] **Point 5: 关断安全**：Actuator 必须实现 `safe_off()` 且 YAML 声明 `config.safe_off_fn`；非 Actuator **必须留空**（`DAL-L-020`，两个方向都是 error）。Sensor 预留 `enable_pin` (Guard A)。
- [ ] **Point 6: 结构体 ABI**：首成员 MUST 内嵌 `dal_<type>_config_t config;` **值副本**（`DAL-S-011`，**不是指针**）+ `_Static_assert(offsetof(...) == 0)`（`DAL-S-014`）；`_Static_assert` 的 sizeof/offsetof 数值须由 `lint --pack abi` 实测回填，**禁止手工估算**。
- [ ] **Point 7: 量纲封闭后缀**：所有物理量字段/参数后缀必须落在 `tools/lint/dal/quantity_suffixes.py` 的封闭表内。**`_norm` / `_val` / `_threshold` 均不在表内**，会触发 `dal.quantity.suffix_closed` **error**；归一化比例请用 `_promille`。
- [ ] **Point 8: DAL 头不得漏 HAL**：DAL 公共头**禁止** `#include "pal_hal.h"`（`DAL-HDR-NO-HAL` error）。因此不能在公共头用 `wink_pin_t`（其定义在 `pal_hal.h`），可选引脚请用裸 `int16_t`。`dal_dc_motor.h` 是带 `until: 2026-12-31` 的历史豁免，**新驱动不得沿用**。
- [ ] **Point 9: 不改 `dal/CMakeLists.txt`**：该文件顶部注释明确 "do not edit this file's driver list"；驱动列表由 `list_drivers.py` 从 YAML 自动生成并含 `WINK_USE_<TYPE>` 剪枝。
- [ ] **Point 10: 单测路径**：单测放 `wink-micro-os/test/unit/dal/test_dal_<type>.c`（**实际路径是 `test/unit/dal/`，非 `test/dal/`**）。
- [ ] **Point 11: 自动化静态检查（全 pack）**：
      `python wink-tools/wink.py lint --pack layering --pack api --pack drivers --pack dal --pack abi --pack user_surface`
      **仅跑 `--pack drivers` 会漏掉 `dal.quantity` / `DAL-HDR-NO-HAL` / `DEVICE-REQUIRES-ROLE` / ABI 断言等致命规则。**

### ⚠️ 全仓已存在的技术债（各分计划需知悉，勿被误导）
实测 `wink.py lint --pack dal` 当前报 **114 个 error**，`--pack drivers` 亦有多条 warning，说明现有 9 个驱动并非 lint 干净基线：
- `dal.yaml.quantity_class_required`（DAL-U-021）对 `fields` 中**每个字段**（含 `gpio_pin` / `role` / `active_low` 这类非物理量）强制 `quantity_class`，9 个驱动全部违规 → **该规则疑似过宽，需与 lint owner 裁定**，否则新驱动只能写一堆语义无意义的声明。
- `dal.quantity.suffix_closed` 对 `_pin` / `_port` / `_addr` / `_channel` / `_valid` 等**非物理量后缀**同样报 error → 同上，需裁定豁免名单。
- `drivers.config_field_order`：`dc_motor` / `eeprom` 的 YAML `fields` 顺序与 C 结构体成员顺序不一致（已告警）。**新驱动应从第一天对齐**。
- 建议：在 P0 启动前用 `--baseline` 固化现有债务基线，使新驱动实现"零新增 error"可被机械验证。

---

## 4. 分计划目录规划与子计划模版规范

### 4.1 目录结构划分
所有分计划文档统一下发存放在本总纲所在目录下：
[`docs/implementation-plans/2026-08-05-wokwi-dal-type-coverage-plan/`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/implementation-plans/2026-08-05-wokwi-dal-type-coverage-plan/)

```
docs/implementation-plans/2026-08-05-wokwi-dal-type-coverage-plan/
├── 00-master-execution-plan.md                      # [本文件] 计划总纲与全量 Checklist
├── 00.1-category-type-variant-wokwi-ssot.md         # [基线] 外设 Category-Type-Variant ↔ Wokwi 映射 SSOT
├── 00.5-pal-adc-subsystem-plan.md                   # [前置] PAL ADC 子系统 + role analog_input（✅ Complete）
├── 00.6-infrastructure-devices-plan.md              # [拓扑] 基础设施外设 (PCF8574/74HC138/TCA9548A)
├── 01-p0-input-analog-knob-plan.md                  # P0: HMI 调参旋钮/滑杆（✅ Complete）
├── 02-p0-input-keypad-plan.md                       # P0: 矩阵键盘（✅ Complete）
├── 03-p0-output-buzzer-plan.md                      # P0: 蜂鸣器
├── 04-p0-output-relay-plan.md                       # P0: 继电器
├── 05-p0-sensor-analog-sensor-plan.md               # P0: 通用模拟量传感器 (NTC/光敏/AO)
├── 06-p0-sensor-digital-sensor-plan.md              # P0: 通用数字开关传感器 (DO)
├── 07-p0-sensor-temp-humidity-plan.md               # P0: 温湿度传感器 (DHT22)
├── 08-p0-display-lcd-char-plan.md                   # P0: 字符点阵屏 (LCD1602/2004)
├── 09-p1-input-ir-receiver-plan.md                  # P1: NEC 红外接收
├── 10-p1-output-led-bar-plan.md                     # P1: LED 条形图
├── 11-p1-actuator-stepper-plan.md                   # P1: 步进电机
├── 12-p1-display-tft-plan.md                        # P1: SPI 彩屏 (ILI9341)
├── 13-p1-display-led-matrix-plan.md                 # P1: WS2812 灯阵/灯环
├── 14-p1-display-seg-display-plan.md                # P1: 数码管
├── 15-p2-sensor-motion-plan.md                      # P2: PIR 人体红外移动侦测
├── 16-p2-sensor-imu-plan.md                         # P2: 6 轴 IMU (MPU6050)
├── 17-p2-sensor-load-cell-plan.md                   # P2: 称重传感器 (HX711)
├── 18-p2-sensor-heart-rate-plan.md                  # P2: 脉搏心率传感器
├── 19-p2-storage-sdcard-plan.md                     # P2: MicroSD 卡块设备
└── 20-p2-storage-rtc-plan.md                        # P2: RTC 实时时钟 (DS1307)
```

### 4.2 具体外设计划文档（分计划）模版规范 (SSOT)
后续每个外设计划文档需严格遵守以下标准模版格式：

```markdown
# DAL 外设执行计划：[category]/[type]

## 1. 需求与硬件映射
- 映射 Wokwi 组件：`[wokwi-component-name]`
- 控制语义描述：...
- 量纲与 A/B 分类（ADR-0056）：actuator_command 还是 sensor_measurement？选用哪个**封闭后缀**？

## 2. 硬件拓扑分类与 Variant 架构分析 (依据 dal-best-practices.md §3)
- **细分变体优先原则（Subdivided Variant First）**：描述硬件型号/接口协议/接线拓扑/算法机制的字符串选型，统一收拢为确切的细分 `variant` 枚举（如 `ssd1306_i2c` / `ssd1306_spi`），对外 API 绝对冻结，C 头文件无具体芯片型号。
- **一变体一确定拓扑断言（Deterministic Pin Map Table）**：
  - 声明 `affects_pins`: `true`（变体改变管脚数量/功能）或 `false`（变体仅改变算法/屏驱）。
  - **变体引脚契约表**：
    | Variant 枚举值 | 引脚数量 | 物理引脚列表 (Pin Name & simRole) |
    |---|---|---|
    | `variant_a` | 3 Pin | `pwm` (pwm), `dir_a` (dir_a), `dir_b` (dir_b) |
    | `variant_b` | 2 Pin | `pwm` (pwm), `dir` (dir) |
- **市面主流芯片/模块全量盘点**：统计并罗列该 type 在市面上的所有主流具体型号/产品。
- **Codegen 别名映射表 (`aliases` / `ic_to_variant_map`)**：将型号别名映射为 `variant` + 默认引脚配置。

## 3. 数据结构设计 (dal_[type].h)
- POD Config (`dal_[type]_config_t`)：首成员 `const char *owner`；`dal_[type]_variant_t variant;` 细分变体枚举；必填引脚 `uint16_t`，可选引脚 `int16_t`(-1 哨兵)。
- Instance (`dal_[type]_t`)：首成员 MUST 为**内嵌值副本** `dal_[type]_config_t config;` + `_Static_assert(offsetof == 0)`。
- 公共头**禁止** include `pal_hal.h`。
- ABI `_Static_assert` 数值由 `lint --pack abi` 实测回填。

## 4. C 驱动核心逻辑 (dal_[type].c)
- `dal_[type]_init/deinit/...`，全部返回 **`wink_status_t`**（无 `wink_err_t`）。
- 错误码用实际枚举名：`WINK_ERR_NOT_INITIALIZED` / `WINK_ERR_ALREADY_INITIALIZED` / `WINK_ERR_INVALID_ARG` ...
- 多拓扑 `switch(config.variant)` 分支处理（结合 `#if WINK_[TYPE]_HAS_[VARIANT]` DCE 优化）。
- 中断/DMA 硬件托底方案 (Guard D / Guard E)。
- 低功耗与 `safe_off()` 实现（仅 actuator）。
- 乘法中间值显式提升 `uint32_t`/`int32_t`（DAL-U-029）。

## 5. Codegen 驱动描述 (wink-micro-os/codegen/drivers/[type].yaml)
- Schema 1.1 `fields:`（顺序须与 C 结构体成员顺序一致，包含 `variant` 字段及 `affects_pins: true/false` 属性）。
- `quantity` / `quantity_class`（DAL-U-021 / ADR-0056：`actuator_command` vs `sensor_measurement`）。
- `config:` 的 `init_fn`/`deinit_fn`/`safe_off_fn`（非 actuator 须留空）。
- `default_role` + `role_bindings`：必须依据 [`dal-role-architecture-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-role-architecture-spec.md) §3 与 §4 正确绑定 Role 与 Verb，严格遵守 `error_class` 导出的 C 函数签名（如 `fire_and_forget` $\rightarrow$ `void` + `WINK_IGNORE_RESULT`）；若 role 为新开规范，须一并新建 `codegen/roles/<role>.yaml`。

## 6. Wasm / Wokwi 仿真映射契约
- Wokwi 前端元件属性绑定与 WASM 物理桥接。

## 7. 验证与测试计划 (三仓闭环验证 Checkpoint)
- C 单测路径 `wink-micro-os/test/unit/dal/test_dal_[type].c`。
- 6 个 lint pack 全跑：`python wink-tools/wink.py lint --pack layering --pack api --pack drivers --pack dal --pack abi --pack user_surface`。
- UniSim 仿真绑定无 Drift 校验：`bun tools/gen-frontend-binders.ts --check`。
- 前端外设插件 `variant-registry.ts` 拓扑等价表 (`equivalence`) 注册校验。
- host + wasm + ESP32 三向编译（ADR-0002）。
```


---

## 5. 下一步行动 (Next Actions)

1. **审阅总纲**：与架构团队确认总纲中 P0/P1/P2 的划分与 28 个 `type` 映射表。
2. **裁定两个 lint 开放问题**（阻塞全部 20 个分计划的验收标准）：
   - `dal.yaml.quantity_class_required`（DAL-U-021）是否应仅作用于真实物理量字段？现状 9 个驱动全违规。
   - `dal.quantity.suffix_closed` 是否应豁免 `_pin`/`_port`/`_addr`/`_channel`/`_valid` 等非物理量后缀？
   - 结论若需变更规范 → 走 ADR 并回写 `dal-api-consistency-spec.md`。
3. **固化 lint 债务基线**：`wink.py lint arch --pack ... --format json --output baseline.json`，使后续"零新增 error"可机械验证。
4. **编写前置计划 `00.5-pal-adc-subsystem-plan.md`**（PAL ADC + `PAL_RESOURCE_ADC_CHANNEL` + wasm bridge + board ADC 能力元数据 + role `analog_input`）。
5. **按修正后的执行序推进 P0**：`relay`(#4) → `buzzer`(#3) 先跑通全链路 → 再 `analog_knob`(#1) / `analog_sensor`(#5) → 其余。

