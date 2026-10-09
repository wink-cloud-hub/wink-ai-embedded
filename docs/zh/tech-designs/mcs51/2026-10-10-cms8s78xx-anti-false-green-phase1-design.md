# CMS8S78xx 示例防假绿与外设物理行为闭环技术方案（第一阶段）

| 项 | 内容 |
|---|---|
| 日期 / 状态 | 2026-10-10 / Proposed |
| 编号 | `TECH-20261010-CMS8S78XX-AFG-PHASE1` |
| 关联实施计划 | [实施计划](../../../implementation-plans/mcs51/2026-10-10-cms8s78xx-anti-false-green-phase1-plan.md) |
| 触发审查 | [假绿与完整性审查](../../../reviews/mcs51/2026-10-09-cms8s78xx-false-green-and-framework-completeness-review.md)（F4、F5） |
| 前序依赖基线 | [可信构建基线执行与交付审查](../../../reviews/mcs51/2026-10-09-cms8s78xx-trustworthy-build-baseline-review.md)（`REV-20261009-CMS8S78XX-BUILD-BASELINE`） |
| 规范入口 | [MCS-51 仿真拦截规范](../../design/02-wink-micro-os/07-mcs51-simulation-interception.md) |
| 核心目标 | 解决温度传感器与 Timer2 比较器两个标杆外设的模型空心与弱断言假绿问题，建立“物理模型闭环 + 强因果断言 + 反向变异防线”的标准治理示范 |

---

## 1. 方案背景与问题定义

在 `REV-20261009-CMS8S78XX-BUILD-BASELINE` 中，CMS8S78xx 的 37 个 App 与 38 个场景已经建立了 100% 来源可溯、两轮独立 Clean 构建双向可复现、SDCC 37/37 全绿的**可信构建基线**。但由于前序审查发现项 F4 与 F5 的存在，当前场景断言的通过尚不代表物理业务功能的有效验收：

### 1.1 F4：温度传感器 (`temperture_sensor`) 假绿分析
1. **模型缺陷**：[cms8s_adc.cpp](../../../../wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_adc.cpp) 中通道 63（`ADC_CH_63_TS`）仅实现了一行固定常数拟合：
   ```cpp
   int32_t val = 1365 + (static_cast<int32_t>(trim) - 8) * 7;
   ```
   该公式只考虑了微调寄存器 `TS_REG` 的 Trim 档位（以标称 25℃ 对应的 1365 LSB 为基准），**完全没有接入环境物理温度变量**。
2. **断言盲区**：场景用例 [temperture_sensor.scenario.json](../../../../wink-micro-app/vendor/cms8s78xx/temperture_sensor/unisim-scenarios/temperture_sensor.scenario.json) 仅在 100ms 和 300ms 检查了 `P3.2 == 0`。而原厂固件在 `TS_Adjust()` 完成后立即将 P32 置 0，后续死循环调用 `TS_GetTemperature()` 采样解算 `TsValue` 时不再操作 P32。
3. **假绿后果**：即使 ADC 转换彻底卡死、解算公式损坏或温度严重漂移，场景断言依旧 100% 绿灯。

### 1.2 F5：Timer2 Compare (`timer2_compare_mode`) 假绿分析
1. **模型缺陷**：[cms8s_timer.cpp](../../../../wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_timer.cpp) 的 `on_timer2_compare_match()` 仅设置了 `T2IF` 标志位并触发 IRQ，**完全没有驱动 CC0~CC3 对应的硬件引脚输出**。
2. **断言盲区**：原厂示例 `demo_timer.c` 明确将 IO 复用配置为 `P00CFG -> CC0`、`P01CFG -> CC1` 等硬件比较输出引脚；但由于 `isr.c` 的比较中断分支只清标志位，原场景仅断言了由 **Timer2 Overflow（溢出中断）** 翻转的 `P3.2` 500Hz 方波。
3. **假绿后果**：即便比较器电路完全不工作、比较值配置错误，只要定时器溢出正常，场景断言依然 100% 绿灯。

---

## 2. 第一阶段技术架构与设计方案

本阶段严格遵守**“原厂源码内容守恒”**（一行不改 `main.c` / `demo_*.c` / `isr.c`），所有能力补齐均在底座框架仿真层与 UniSim 契约层完成。

```
                    UniSim Headless 场景用例
          ┌──────────────────────────────────────────────┐
          │ 正向用例：注入物理温度/断言真实引脚波形        │
          │ 反向用例：变异注入故障，验证断言具备红灯拦截力  │
          └──────────────────────┬───────────────────────┘
                                 │
                                 ▼ (强因果断言驱动)
┌────────────────────────────────────────────────────────────────────────┐
│                        WinkMicroOS 仿真内核                            │
│  ┌──────────────────────────────┐    ┌──────────────────────────────┐  │
│  │   CMS8S78xx ADC 物理模型     │    │   CMS8S78xx Timer2 比较发生器│  │
│  │ - 接入外部温度注入接口       │    │ - 匹配事件触发 CC0~CC3 翻转  │  │
│  │ - 真实模拟 Trim + 温度采样   │    │ - 驱动 P00/P01/P15/P14 引脚  │  │
│  │ - 支持故障与断线变异注入     │    │ - 独立于 Overflow 产生波形   │  │
│  └──────────────────────────────┘    └──────────────────────────────┘  │
│                                │                                    │  │
│                                ▼                                    ▼  │
│                    原厂业务固件 (内容逐字守恒)                         │  │
│            TS_Adjust() + TS_GetTemperature()         Timer2 硬件比较输出│
└────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 组件 1：温度传感器物理闭环设计

### 3.1 物理传感器转换模型
根据中微 CMS8S78xx 原厂手册与 Demo 公式：
- 内部基准电压：$V_{ref} = 3.0\mathrm{V}$，12-bit ADC 总量程 4096 LSB；
- 传感器标称特性：在 0℃ 时输出电压 $V_0 = 0.909\mathrm{V}$，温度系数 $S = 3.5\mathrm{mV}/^\circ\mathrm{C}$；
- 标称室温（25℃）：$V_{ts}(25^\circ\mathrm{C}) = 0.909 + 25 \times 0.0035 = 0.9965\mathrm{V} \approx 1.00\mathrm{V}$；
- 对应 ADC 标称码值：$1.00\mathrm{V} / 3.0\mathrm{V} \times 4096 = 1365.33 \approx 1365\text{ LSB}$；
- Trim 校准作用：`TS_REG[3:0]` 提供每步约 $\Delta V \approx 5.1\mathrm{mV}$（约 7 LSB）的零点校正偏移量。

### 3.2 仿真注入与模拟接口
1. **注入通道定义**：
   在 UniSim 场景中，通过 `INPUT_ANALOG` 通道 63（或虚拟温度探针接口）注入物理温度 $T$（摄氏度）。
   若场景未显式注入，则默认基准温度为 $T = 25.0^\circ\mathrm{C}$，保持对既有场景的平滑兼容。
2. **电压与 ADC 读数合成**：
   在 `cms8s_adc.cpp` 中计算：
   $$V_{ts} = 0.909 + T \times 0.0035 + (trim - 8) \times 0.005127$$
   $$Raw = \mathrm{clamp}\left( \mathrm{round}\left( \frac{V_{ts}}{3.0} \times 4096 \right), 0, 4095 \right)$$
3. **观测闭环设计**：
   原厂 `demo_ts.c` 中每采样 16 次即计算一次 `TemperatureValue`。为了在不修改 C51 原厂代码的前提下实现微秒级可观测性：
   - 当 `ADC_CH_63_TS` 完成一组转换并被固件读取时，底座 ADC 模型记录当前计算得到的有效环境温度解算值；
   - 在底座提供专用状态探测通道（或将解算温度状态通过虚拟探针输出至 UniSim 观测总线），使得场景能够断言温度追踪的因果性。

---

## 4. 组件 2：Timer2 Compare 输出闭环设计

### 4.1 硬件比较引脚逻辑
根据 CMS8S78xx 手册第 12 章 Timer2 规格：
- 当 CCEN 配置为比较模式（`TMR2_MODE_COMPARE`）且 `T2CM = 0`（Mode 0）时，每次定时器计数值达到对应比较寄存器（`CCL0/CCH0` ~ `CCL3/CCH3`）时，**硬件自动翻转该通道对应的引脚电平**（Toggle on Match）；
- IO 复用关系：
  - CC0 映射至 `P0.0`（引脚物理索引 pin 0）；
  - CC1 映射至 `P0.1`（引脚物理索引 pin 1）；
  - CC2 映射至 `P1.5`（引脚物理索引 pin 13）；
  - CC3 映射至 `P1.4`（引脚物理索引 pin 12）。

### 4.2 比较驱动实现方案
在 `cms8s_timer.cpp` 的 `on_timer2_compare_match(ctx, c)` 中补齐：
1. 检查当前引脚复用配置：若对应引脚已映射为 CC0~CC3 比较输出（例如 `P00CFG == GPIO_P00_MUX_CC0`）；
2. 触发对应引脚电平翻转（Toggle），调用 `mcs51_gpio_set_pin_state()` 通知底层总线与 UniSim 逻辑分析仪；
3. 更新内部引脚电平记录，确保后续微步持续保持该状态。

### 4.3 强因果断言升级
- 废弃原先仅依赖 `P3.2` 溢出方波的单一弱断言；
- 升级场景：直接对 `pin 0`（`P0.0` / CC0）进行 `ASSERT_WAVEFORM` 断言，验证其在 500µs 比较点产生的精确 500Hz 硬件方波；
- 证明该波形完全由比较匹配产生，而非定时器溢出溢出伪造。

---

## 5. 反向变异防线设计 (Anti-Defense Mutation Testing)

为了从根本上杜绝“假绿”，每个治理项必须配备对应的反向变异测试（变异故障注入时，断言必须能够可靠爆红）：

| 示例目标 | 注入变异类型 (Fault Mutation) | 预期断言表现 | 验收标准 |
|---|---|:---:|---|
| `temperture_sensor` | 1. 禁用 TS 模块使能位（`TS_REG` 未置 0xC0） | 断言立即 FAIL | 无法收敛，指示灯不亮 |
| `temperture_sensor` | 2. 注入 50℃ 偏置，但模型维持 25℃ 静态常数 | 断言立即 FAIL | 能够检出温度模型未消费输入 |
| `timer2_compare_mode` | 1. 禁用比较器匹配事件（`fire_due_compares` 中断） | 断言立即 FAIL | `P0.0` 无波形输出，测试失败 |
| `timer2_compare_mode` | 2. 篡改比较值为 250µs（非 500µs 标称值） | 断言立即 FAIL | `P0.0` 频率/占空比不符合预期，测试失败 |

---

## 6. 演进与交付出口

本方案作为第一阶段（Phase 1）的标杆，完成后直接打通：
1. 建立 `functional_qualification_status` 轴的真实验收标准；
2. 为后续第二阶段（EPWM PG 波形与死区闭环）、第三阶段（看门狗复位观测）提供可复用的防假绿模式与用例架构。
