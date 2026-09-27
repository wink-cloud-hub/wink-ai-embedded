# CMS8S78xx 官方示例仿真适配核对清单 (Checklist)

> **根目录**：`docs/vendors/Cmsemicon/CMS8S78xx_DemoCode_V2.0.2/CMS8S78xx_Example/Example/`  
> **芯片型号**：CMS8S78xx（1T 8051，增强型片内 12-bit ADC、LED/LCD 驱动、EPWM）  
> **适配目标**：将原厂 Keil C51 示例通过 WinkMicroOS MCS-51 仿真拦截层无修改接入 UniSim，实现功能级高保真仿真与 Headless 自动化测试。  
> 📖 **标准执行手册 (Playbook)**：所有 Checklist 项目的适配与实证必须严格遵循 [PLAYBOOK.md](PLAYBOOK.md)。每一项打勾 `[x]` 前必须完成：① `wink-tools` 真实编译并输出资产到 `unisim-assets/`；② `@wink-ai/unisim` Headless 场景测试 100% 绿灯通过。

---

## 一、 总体适配进度与统计

- **官方模块总数**：25 个外设类别
- **子示例总数**：43 个独立子示例工程
- **当前适配状态**：
  - [x] **已完成适配并实证**：37 个（前序 31 个外设 + ResetBySoftware 与 ResetByWDT + I2C AT24C256 与 SPI M95256 + LVD + SystemClock 已全链路数据实证通过）
  - [ ] **待补充外设仿真模型 / 真实构建测试**：1 个（`LCD` 待适配）
  - [-] **暂缓/纯芯片模拟硬件特性（不适用纯软件行为级仿真）**：5 个类别（LSE/SCM、FLASH IAP、SLEEP 低功耗、ResetByExtReset[依赖 GAP-06/NRST] 等）

---

## 二、 符号与分类说明

### 1. 状态符号
- `[x]` **已完成**：已在 `wink-micro-app/` 建立独立 App，配置 `device-tree.json`，在 UniSim / Headless 跑通断言。
- `[ ]` **待适配**：尚未创建独立 App 或尚未打通仿真端到端链路。
- `[-]` **暂缓/真机专用**：依赖芯片物理模拟特性（如停振检测、外部物理总线握手、IAP Flash），行为级仿真暂不作为重点。

### 2. 可观测性分级 (Observability Level)
- 🎯 **Level 1（界面直观可视）**：界面有数码管、LCD、LED 指示灯、蜂鸣器音频等直观 UI 控件效果。
- 📜 **Level 2（串口日志输出）**：无复杂动画，但虚拟终端/控制台有格式化打印输出（`printf` / SBUF）。
- ⚡ **Level 3（IO 打点/波形探测）**：原厂通用 `P32 = ~P32` 打点。建议在开发板上将 P32 绑定为板载 LED，使之可视化。
- ⚙️ **Level 4（纯内部静默逻辑）**：芯片内部寄存器/内存校验，界面无输出，通过断点或 CTest 变量断言验证。
- 🚫 **Blocked（未建模会死锁）**：源码轮询等待硬件标志位（如 SPI/I2C 中断标志），仿真中会直接死循环。

---

## 三、 43 个子示例逐项核对清单

### 1. LED 与显示驱动 (Display)
| 状态 | 编号 | 官方子示例路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [x] | 01 | `LED/4COM_8SEG_LED/code` | 🎯 Level 1 | P0 | `vendor_cms8s78xx_led_4com_8seg` | **标杆已落地**。Timer0 动态扫描 P3/P1，驱动 4 位 8 段数码管高保真渲染。2026-09-24 复验（PLAN-20260924-MCS51-T01-OVERFLOW-REARM-FIX）：修复 T0 溢出重入二次派发后 ISR 周期恢复模型真值 2500µs（at-anchored 标称；心跳方波 200Hz、帧率 80Hz）；场景按观测层诚实标定——原「1ms P0.0 必低」不可观测（引擎启动 burst 已推进固件 ~10ms），改为心跳速率断言 `$between [150,260]`；scanHz 插件 3 采样滑动窗在 ~10ms 批量时间戳下确定性摆动于 {67,100}，带宽由 [40,50] 改为 [60,110]（修复前实测 200）。 |
| [ ] | 02 | `LCD/4COM_7SEG_LCD/code` | 🎯 Level 1 | P1 | - | 硬件 LCD 控制器 + Timer0 扫描。需前端段码 LCD 插件配合。 |

---

### 2. 串口通信 (UART)
| 状态 | 编号 | 官方子示例路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [x] | 03 | `UART/UART0_Printf/code` | 📜 Level 2 | P0 | `vendor_cms8s78xx_uart0_printf` | **已落地**。UART0 调用 `printf` 输出格式化字符串 `CMS8S78xx Test........\n\r`。原厂源码一行不改；`REG_CMS8S78XX.H` 补齐 `FUNCCR`/`SCON0`/`SBUF0`/`BRTCON`/`PS_RXD` 及 StdDriver inline shims，并提供 Keil `printf`/`putchar` 桥接（重定向至 `SBUF0` $\to$ UARTBus 0）；`REGX52.H` 隔离 libc stdio 签名防冲突。场景：`uart0_printf.scenario.json`，`ASSERT_BUS_PAYLOAD` 报文断言 100% 绿灯（Wall-clock 98ms）。wink-tools 真实编译三件套（wasm 208KB）。架构分层门禁 0 findings。 |
| [x] | 04 | `UART/UART0_RxTx/code` | 📜 Level 2 | P0 | `vendor_cms8s78xx_uart0_rxtx` | **已落地**。UART0 中断收发回显。原厂源码一行不改（`main.c`, `demo_uart.c`, `demo_uart.h`, `isr.c`）。开机发送 `CMS8S78xx Test........\n\r`；RX 接收到字符触发 `UART0_IRQHandler`（向量 4），通过 `UART_GetReceiveIntFlag()` / `UART_SendBuff(UART_GetBuff())` / `UART_ClearReceiveIntFlag()` 实时原样回显至 TX；TX 完成触发 `UART0_IRQHandler` 经 `UART_GetSendIntFlag()` / `UART_ClearSendIntFlag()` 清除标志位。场景：`uart0_rxtx.scenario.json`，全套 5 步断言 100% 绿灯（开机 banner、单字节 `'A'` 实时回显、多字节流 `'Wink'` 流式回显，Wall-clock 179ms）。wink-tools 真实编译三件套（wasm 210KB）。架构分层门禁 0 findings。 |

---

### 3. 基础输入输出与中断 (GPIO & EXTINT)
| 状态 | 编号 | 官方子示例路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [x] | 05 | `GPIO/code` | ⚡ Level 3 | P0 | `vendor_cms8s78xx_gpio` | P12 外部引脚电平变化触发 P1EI 中断，翻转 P32 电平。已落地 Headless 断言与 CTest 实证。 |
| [x] | 06 | `EXTINT/EXTINT0/code` | ⚡ Level 3 | P0 | `vendor_cms8s78xx_extint0` | **已落地**。INT0 经 PS 复用映射至 P30（`PS_INT0=0x30`，下降沿，向量 0），ISR 执行 `P32=~P32`。场景：`vendor_cms8s78xx_extint0/unisim-scenarios/extint0.scenario.json`，13 步断言全绿（gpio:24 按键下降沿→gpio:26 LED 翻转、长按不重触发、上升沿不响应、二次按下再翻转）；wink-tools 真实编译产出 assets 三件套（wasm 173KB）。注：官方源码实际引脚为 **P30**（非旧描述 P04），已按"原厂源码一行不改"准则镜像。 |
| [x] | 07 | `EXTINT/EXTINT1/code` | ⚡ Level 3 | P0 | `vendor_cms8s78xx_extint1` | **已落地**。INT1 经 PS 复用映射至 P31（`PS_INT1=0x31`，下降沿，向量 2），ISR 执行 `P32=~P32`。场景：`vendor_cms8s78xx_extint1/unisim-scenarios/extint1.scenario.json`，13 步断言全绿（gpio:25 按键下降沿→gpio:26 LED 翻转、长按不重触发、上升沿不响应、二次按下再翻转）；wink-tools 真实编译产出 assets 三件套（wasm 173KB）。注：官方源码实际引脚为 **P31**（非旧描述 P05），已按"原厂源码一行不改"准则镜像。 |

---

### 4. 模拟与信号链 (ADC & ACMP & TS)
| 状态 | 编号 | 官方子示例路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [x] | 08 | `ADC/ADC_Ldo/code` | ⚡ Level 3 | P0 | `vendor_cms8s78xx_adc_ldo` | **已落地（ADR-0073 tier-c 收割）**。片内 12-bit ADC 持续转换 AN0（P0.0）+ 内部 LDO 基准，每次转换完成触发中断 vector 19（`ADC_IRQHandler`）翻转 P32。原厂 `main.c/isr.c/demo_adc.c/demo_adc.h` 一行不改经 `mcs51_cleanup.py` 编译；缺失的 9 个 StdDriver ADC API（`ADC_ConfigRunMode/EnableChannel/EnableLDO/ConfigADCVref/EnableInt/Start/GetADCResult/GetIntFlag/ClearIntFlag`）由框架头 `REG_CMS8S78XX.H` 新增 `static inline` shim 提供（逐字镜像 StdDriver `adc.c` 函数体，`WINK_CMS8S_VENDOR_ADC` 守卫避免与 tier-b 真实 adc.c 重定义冲突），并补 `GPIO_P00_MUX_AN0=0x01`。场景：`unisim-scenarios/adc-ldo.scenario.json`，注入 AN0（rail 32）中点电平后以 `ASSERT_WAVEFORM frequencyHz` 断言窗口 [100ms,400ms] 内 P3.2（pin 26）连续翻转落在 [1kHz, 50kHz] 活性区间（证明 EOC 中断链路闭环；实测仿真循环 ~10.7kHz、真机物理 ~3.3kHz 均在区间内；无中断则 0Hz 失败），headless 100% 绿。wink-tools 真实编译三件套（wasm 177KB）。CTest：host mcs51/cms8s 26/26 通过（含 `test_mcs51_cms8s_vendor` 真实 adc.c 编译运行）。 |
| [x] | 09 | `ADC/HardwareTrigger/code` | ⚡ Level 3 | P1 | `vendor_cms8s78xx_adc_hardware_trigger` | **已落地**。外部硬件触发采样 12-bit ADC。引脚 P0.5 经 `PS_ADET`（XSFR 0xF0CC）复用映射至 ADET 触发线，配置为下降沿触发（`ADC_TG_ADET = 0x03`, `ADC_TG_FALLING = 0x00`, `ADCON2.ADCEX = 1`）。main 处于 `while(1);` 静默等待；当 P0.5 产生下降沿时，内核触发 ADC 采样 AN0（P0.0），EOC 中断 19（`ADC_IRQHandler`）翻转 P3.2 并读取 `adc_result`。原厂源码一行不改经 `mcs51_cleanup.py` 编译；框架补充 `PS_ADET`、`GPIO_P05_MUX_ADET=0x05` 及 3 个 HardwareTrig API inline shim（`ADC_EnableHardwareTrig/DisableHardwareTrig/ConfigHardwareTrig`）；`cms8s_adc.cpp` 增加微步进 ADET 边沿检测与触发逻辑。场景：`unisim-scenarios/adc_hardware_trigger.scenario.json`，13 步断言 100% 绿（P0.5 下降沿触发、P3.2 翻转为 1、长按不重复触发、松开上升沿不触发、二次按下再次翻转回 0）。wink-tools 真实编译三件套（wasm 181KB）。CTest：`test_mcs51_cms8s_adc` 12 项测试与 `test_mcs51_cms8s_vendor` 绿灯通过；架构门禁 0 findings。 |
| [x] | 10 | `ACMP/ACMP0/code` | ⚡ Level 3 | P2 | `vendor_cms8s78xx_acmp0` | **已落地**。模拟比较器 0 阈值比较（P1.1 C0P0 vs 片内 1.2V Bandgap），正向穿越触发中断 vector 14 (`ACMP_IRQHandler`)，在 ISR 中翻转 P32 (`P32 = ~P32;`)。原厂源码一行不改（`main.c`, `demo_acmp.c`, `demo_acmp.h`, `isr.c`）。底层支持：① `REG_CMS8S78XX.H` 补齐 14 个 ACMP XSFR 声明（`C0CON0~C0CON2`, `C1CON0~C1CON2`, `CNVRCON`, `CNFBCON`, `CNIE`, `CNIF`, `C0ADJE`, `C1ADJE`, `C0HYS`, `C1HYS`，`0xF500~0xF50D`）、位掩码宏、引脚复用宏（`GPIO_P10_MUX_C0O`, `GPIO_P11_MUX_C0P0`, `GPIO_P12_MUX_C0N`）与 15 个 StdDriver inline shims；② `cms8s_xsfr_allowlist.h` 登记 14 个 XSFR 清除 GAP-23 绊线告警；③ `wink_mcs51_isr.h`/`mcs51_isr.cpp`/`cms8s_sys.cpp` 注册 `IRQ_SOURCE_ACMP` 物理中断向量 14；④ `cms8s_acmp.cpp` 实现模拟输入采样（`js_pal_adc_read_norm`）、1.2V Bandgap 阈值比较、P1.0 比较输出驱动、正向边沿检测触发中断置位 `CNIF`；⑤ 场景：`acmp0.scenario.json` 8 步 Headless 断言（P1.1 在 0.10 与 0.50/0.60 间跳变，双向验证正向穿越翻转 P3.2、反向回落保持高电平单向边沿特性、二次正向穿越再翻转回 0）。`wink-tools` 真实编译三件套（wasm 321KB），`winkcli sim run` Headless 8 步断言 100% 绿灯（Virtual 1000ms, Wall-clock 276ms），`winkcli lint` 0 findings。 |
| [x] | 11 | `ACMP/ACMP1/code` | ⚡ Level 3 | P2 | `vendor_cms8s78xx_acmp1` | **已落地**。模拟比较器 1 阈值比较（P2.1 C1P0 vs 片内 1.2V Bandgap 经 20% 分压比例 K=0.20 -> 0.24V，归一化阈值 0.048），正向穿越触发中断 vector 14 (`ACMP_IRQHandler`)，在 ISR 中翻转 P32 (`P32 = ~P32;`)。原厂源码一行不改（`main.c`, `demo_acmp.c`, `demo_acmp.h`, `isr.c`）。底层支持：① `REG_CMS8S78XX.H` 补齐 `Vref_K_T` 枚举声明（19 个基准分压系数常量）；② `cms8s_acmp.cpp` 实现 `CNVRCON` 内部基准分压比计算、P2.1 模拟采样、正向边沿中断与 P2.4 (`C1O`) 比较结果输出驱动；③ 单元测试：新增 `test_mcs51_cms8s_acmp.cpp` 完整覆盖 ACMP0/ACMP1 阈值、中断与 IO 驱动；④ 场景：`acmp1.scenario.json` 8 步 Headless 断言（P2.1 在 0.02 与 0.10/0.20 间跳变，双向验证正向穿越翻转 P3.2、反向回落保持高电平单向边沿特性、二次正向穿越再翻转回 0）。`wink-tools` 真实编译三件套（wasm 321KB），`winkcli sim run` Headless 8 步断言 100% 绿灯（Virtual 1000ms, Wall-clock 217ms），`winkcli lint` 0 findings。 |
| [x] | 12 | `TempertureSensor/code` | ⚙️ Level 4 | P2 | `vendor_cms8s78xx_temperture_sensor` | **已落地**。内部硅基固态温度传感器（AN63 子通道 4 `ADC_CH_63_TS`）微调与采样，输出至 `TsValue` 变量。原厂源码一行不改镜像。底层支持已闭环（`REG_CMS8S78XX.H` 声明 `TS_REG` 与 `ADC_ConfigAN63`，`cms8s_xsfr_allowlist.h` 登记 `0xF693`，`cms8s_adc.cpp` 支持微调档位响应，`temperture_sensor.scenario.json` 场景 100% 绿灯，Virtual 500ms / Host 88ms）。wink-tools 真实编译三件套（wasm 320KB）。架构分层门禁 0 findings。 |

---

### 5. 定时器体系 (Timer0 ~ Timer4)
| 状态 | 编号 | 官方子示例路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [x] | 13 | `Timer0/TimmingMode/code` | ⚡ Level 3 | P0 | `vendor_cms8s78xx_timer0_timming_mode` | **已落地**。定时器 0 8 位自动重载定时中断（100µs 周期溢出触发 vector 1 `Timer0_IRQHandler`），在 ISR 中翻转 P32（`P32 = ~P32;`）输出精确 5000Hz 方波。原厂源码一行不改（`main.c`, `demo_timer.c`, `demo_timer.h`, `isr.c`）。场景：`timer0_timming_mode.scenario.json`，`ASSERT_WAVEFORM` 在 pin 26 (P3.2) 断言载波频率为 5000Hz（`$near: { target: 5000, tolerance: 100 }`），100% 绿灯（Virtual 100ms, Wall-clock 89ms）。wink-tools 真实编译三件套（wasm 207KB）。架构分层门禁 0 findings。2026-09-24 复验（PLAN-20260924-MCS51-T01-OVERFLOW-REARM-FIX）：修复 T0 溢出重入二次派发后恢复 5000Hz 绿灯（修复前实测 10000Hz）；新增回归 CTest `test_mcs51_timer_overflow_rearm`（模型级精确派发数）+ `test_mcs51_vendor_timer0_rearm`（本 app 原厂源码 e2e）。 |
| [x] | 14 | `Timer0/CountMode/code` | ⚡ Level 3 | P1 | `vendor_cms8s78xx_timer0_count_mode` | **已落地**。T0 8 位自动重载外部脉冲计数模式（`TMR_MODE_COUNT`, `TMR_TIM_AUTO_8BIT`, `TMOD=0x06`）。引脚 P2.5 经 `PS_T0`（XSFR `0xF0C2`）复用映射至 T0 外部计数输入线，设定溢出周期 `256 - 5`（计满 5 个下降沿脉冲后硬件自动重载并触发 vector 1 `Timer0_IRQHandler`）。ISR 中翻转 P32（`P32 = ~P32;`）。原厂源码一行不改（`main.c`, `demo_timer.c`, `demo_timer.h`, `isr.c`）。框架补齐 `PS_T0 (0xF0C2)` / `GPIO_P25_MUX_T0 (0x25)`，`mcs51_timer.cpp` 增加 `resolve_timer_pin` 动态引脚解析与防嵌套误触发 edge latch 机制。场景：`timer0_count_mode.scenario.json`，28 步 Headless 断言全绿（前 4 脉冲保持低、第 5 脉冲翻转为高、第 9 脉冲保持高、第 10 脉冲翻转回低）。wink-tools 真实编译三件套（wasm 206KB）。CTest host 34/34 全绿。架构分层门禁 0 findings。 |
| [x] | 15 | `Timer1/TimmingMode/code` | ⚡ Level 3 | P0 | `vendor_cms8s78xx_timer1_timming_mode` | **已落地**。定时器 1 16 位定时中断（Mode 1 软件重载，100µs 周期溢出触发 vector 3 `Timer1_IRQHandler`），在 ISR 中翻转 P32（`P32 = ~P32;`）并软件重载 `TH1/TL1`，输出标称 5000Hz 方波（200 counts @ Fsys/12 = 100µs；native 后端不建模中断延迟，mode-1 软件重载按溢出时刻重基为 at-anchored 标称周期）。原厂源码一行不改（`main.c`, `demo_timer.c`, `demo_timer.h`, `isr.c`）。场景：`timer1_timming_mode.scenario.json`，`ASSERT_WAVEFORM` 在 pin 26 (P3.2) 断言载波频率为 5000Hz（`$near: { target: 5000, tolerance: 100 }`），100% 绿灯（Virtual 100ms, Wall-clock 105ms）。wink-tools 真实编译三件套（wasm 207KB）。架构分层门禁 0 findings。2026-09-24 复验（PLAN-20260924-MCS51-T01-OVERFLOW-REARM-FIX）：修复 T1 溢出重入二次派发后恢复 write-instant 重排语义，实测 5000Hz（修复前 7420Hz 为缺陷态；原 4260 目标系 ADR-0078 时代把逐次 SFR 访问代理记账误当 ISR 延迟的伪值），场景目标值按实测标定为 5000±100。 |
| [x] | 16 | `Timer1/CountMode/code` | ⚡ Level 3 | P1 | `vendor_cms8s78xx_timer1_count_mode` | **已落地**。T1 16 位外部脉冲计数模式（`TMR_MODE_COUNT`, `TMR_TIM_16BIT`, `TMOD=0x50`）。引脚 P2.4 经 `PS_T1`（XSFR `0xF0C4`）复用映射至 T1 外部计数输入线，初值与软件重载设定为 `65536 - 5`（计满 5 个下降沿脉冲后溢出产生中断 Vector 3 `Timer1_IRQHandler`）。在 ISR 中翻转 P32（`P32 = ~P32;`）并软件重载 `TH1/TL1`。原厂源码一行不改（`main.c`, `demo_timer.c`, `demo_timer.h`, `isr.c`）。框架补齐 `PS_T1 (0xF0C4)` / `GPIO_P24_MUX_T1 (0x24)`，`mcs51_timer.cpp` 动态引脚解析与 16 位外部计数溢出与软件重载全链路闭环。场景：`timer1_count_mode.scenario.json`，28 步 Headless 断言 100% 绿灯（前 4 脉冲保持低、第 5 脉冲翻转为高、第 9 脉冲保持高、第 10 脉冲翻转回低）。wink-tools 真实编译三件套（wasm 206KB）。CTest host 34/34 全绿。架构分层门禁 0 findings。 |
| [x] | 17 | `Timer2/TimingMdode/code` | ⚡ Level 3 | P0 | `vendor_cms8s78xx_timer2_timing_mode` | **已落地**。定时器 2 16 位自动重载定时中断（DIV_12 分频下 100µs 周期溢出触发 vector 5 `Timer2_IRQHandler`），在 ISR 中通过 `TMR2_GetOverflowIntFlag()` 校验、`P32 = ~P32;` 翻转输出精确 5000Hz 方波并通过 `TMR2_ClearOverflowIntFlag()` 清除标志。原厂源码一行不改（`main.c`, `demo_timer.c`, `demo_timer.h`, `isr.c`）。框架新增 `IRQ_SOURCE_TIMER2` 向量与 `REG_CMS8S78XX.H` SFR（0xC8~0xCF）及 StdDriver 8 个 `TMR2_*` inline shims，`mcs51_timer.cpp` 补齐 Timer 2 硬件级自动重载与分频时序计算。场景：`timer2_timing_mode.scenario.json`，`ASSERT_WAVEFORM` 在 pin 26 (P3.2) 断言载波频率为 5000Hz（`$near: { target: 5000, tolerance: 100 }`），100% 绿灯（Virtual 100ms, Wall-clock 103ms）。wink-tools 真实编译三件套（wasm 208KB）。架构分层门禁 0 findings。 |
| [x] | 18 | `Timer2/CountMode/code` | ⚡ Level 3 | P1 | `vendor_cms8s78xx_timer2_count_mode` | **已落地**。TMR2 16 位自动重载外部脉冲计数模式（`TMR2_ConfigRunMode(TMR2_MODE_COUNT, TMR2_AUTO_LOAD)`，即 `T2CON` 配置为计数+自动重载）。引脚 P1.6 经 `PS_T2`（XSFR `0xF0C6`）复用映射至 T2 外部时钟引脚，设定初值 `65536 - 5`（计满 5 个下降沿后溢出并自动由 `RLDH/RLDL` 重载）。溢出置位 `T2IF.T2F`，触发 vector 5 `Timer2_IRQHandler`，ISR 校验 `TMR2_GetOverflowIntFlag()`，翻转 P32（`P32 = ~P32;`）并调用 `TMR2_ClearOverflowIntFlag()` 清除标志。原厂源码一行不改。框架补齐 `PS_T2 (0xF0C6)` / `GPIO_P16_MUX_T2 (0x16)`，`mcs51_timer.cpp` 补齐 Timer 2 外部时钟引脚采样与 16 位脉冲计数及自动重载。场景：`timer2_count_mode.scenario.json`，28 步 Headless 断言 100% 绿灯（Virtual 600ms, Wall-clock 134ms）。wink-tools 真实编译三件套（wasm 206KB）。CTest host 44/44 全绿。架构分层门禁 0 findings。 |
| [x] | 19 | `Timer2/CompareMode/code` | ⚡ Level 3 | P2 | `vendor_cms8s78xx_timer2_compare_mode` | **已落地（第三批次：高级外设特性）**。TMR2 定时器 4 通道比较匹配模式（`TMR2_MODE_TIMING` + `TMR2_LOAD_DISBALE`，CC0~CC3 全部使能 Mode 0 比较）。系统时钟 12 分频（0.5µs/tick），定时周期 1ms（`65536 - 2000`），4 路比较阈值均设为 500µs（`65536 - 1000`）。使能溢出中断与 4 路通道比较中断（`T2IE |= 0x8F`）。引脚复用：P00/P01/P15/P14 分别映射为 CC0~CC3 比较波形输出。ISR 中溢出中断翻转 P32（输出 500Hz 方波）并软件重载 `65536 - 2000`；各通道比较中断触发时由 `TMR2_GetCompareIntFlag(CCn)` 校验并调用 `TMR2_ClearCompareIntFlag(CCn)` 清除标志。原厂源码一行不改。**底层改造点**：① `REG_CMS8S78XX.H` 补齐 SFR `CCL1/CCH1/CCL2/CCH2/CCL3/CCH3 (0xC2~0xC7)`，补齐比较 API inline shims（`TMR2_EnableCompare`, `TMR2_ConfigCompareValue`, `TMR2_EnableCompareInt`, `TMR2_DisableCompareInt`, `TMR2_GetCompareIntFlag`, `TMR2_ClearCompareIntFlag`, `TMR2_ConfigCompareIntMode`）及引脚宏 `GPIO_P00_MUX_CC0 (0x05)` 等；② `mcs51_timer.cpp` 引入 CC0~CC3 比较匹配事件时间推进调度，精准更新 `T2IF` 比较匹配标志；③ `mcs51_isr.cpp` 修正 `mcs51_raise_irq` 多标志外设掩码保护（避免 Timer 2 比较匹配中断误置位溢出标志 `T2F`）；④ CTest 单元测试 Test 6 验证 20ms 内 20 次 1ms 溢出、80 次比较中断与 P32 500Hz 方波翻转。**场景断言**：`timer2_compare_mode.scenario.json`，`ASSERT_WAVEFORM` 在 pin 26 (P3.2) 断言载波频率为 500Hz（`$near: { target: 500, tolerance: 20 }`），100% 绿灯（Virtual 100ms, Wall-clock 118ms）。wink-tools 真实编译三件套（wasm 210KB）。架构分层门禁 0 findings。 |
| [x] | 20 | `Timer2/CaptureMode/code` | ⚡ Level 3 | P2 | `vendor_cms8s78xx_timer2_capture_mode` | **已落地**。TMR2 4 通道输入捕获模式（`TMR2_MODE_TIMING` + `TMR2_LOAD_DISBALE`）。引脚 P00/P01/P15/P14 配置为下拉输入并经 `PS_CAP0~3` 映射至 CAP0~3 捕获输入线；时钟 12 分频，溢出周期 10ms（`65536 - 20000`）。使能上升沿捕获（`TMR2_EnableCapture(CCn, TMR2_CAP_EDGE_RISING)`）与溢出/捕获中断。ISR 中溢出中断重载 10ms；CAP0（P0.0）捕获中断触发时执行 `P32 = ~P32; TMR2_ClearCaptureIntFlag(TMR2_CC0);` 翻转指示灯。原厂源码一行不改。**底层改造点**：① `REG_CMS8S78XX.H` 补齐 `PS_CAP0~3 (0xF0C8~0xF0CB)`，补齐 `CCL1~3/CCH1~3` SFR，补齐捕获 API inline shims（`TMR2_EnableCapture`, `TMR2_EnableCaptureInt`, `TMR2_DisableCaptureInt`, `TMR2_GetCaptureIntFlag`, `TMR2_ClearCaptureIntFlag`）及引脚宏 `GPIO_P00_MUX_CAP0` 等；② `mcs51_timer.cpp` 支持 CAP0~3 上升沿检测，锁存计数器至捕获寄存器，置位 `T2IF` 捕获标志并触发 Vector 5 中断；③ 严格实现硬件级 W0C（write-0-to-clear）写入语义，彻底杜绝写清除标志时误置位其他通道标志；④ `mcs51_isr.cpp` 支持 Timer 2 多源中断标志联合使能校验（`T2IF & T2IE`）。场景：`timer2_capture_mode.scenario.json`，10 步 Headless 断言 100% 绿灯（Virtual 500ms, Wall-clock 137ms）。CTest host `test_mcs51_timer_ext_clk` 包含 Test 5 100% 绿灯。三件套 WASM 资产完备。 |
| [x] | 21 | `Timer3/TimmingMode/code` | ⚡ Level 3 | P1 | `vendor_cms8s78xx_timer3_timming_mode` | **已落地（第二批次：扩展定时器矩阵）**。扩展定时器 3 8 位自动重载定时中断（`TMR_ConfigRunMode(TMR3, TMR_MODE_TIMING, TMR_TIM_AUTO_8BIT)`），12 分频（0.5µs/tick），设定 100µs 周期溢出（`256 - 200`，10kHz 溢出），溢出触发 vector 15 `Timer3_IRQHandler`。ISR 中翻转 P32（`P32 = ~P32;`）输出精确 5000Hz 方波。原厂源码一行不改（`main.c`, `demo_timer.c`, `demo_timer.h`, `isr.c`）。底层支持：① `REG_CMS8S78XX.H` 补齐 SFR `T34MOD (0xD2)`, `TL3 (0xDA)`, `TH3 (0xDB)`, `EIE2 (0xAA)`, `EIF2 (0xB2)` 以及 `TMR_T34MOD_*` 位掩码；② `TMR_*` StdDriver inline shims 全面扩展支持 `Timern == TMR3`；③ `mcs51_timer.cpp` 实现 Timer 3 状态管理与微秒时间推进调度，防止 ISR 重入二次溢出。场景：`timer3_timming_mode.scenario.json`，`ASSERT_WAVEFORM` 在 pin 26 (P3.2) 断言载波频率为 5000Hz（`$near: { target: 5000, tolerance: 100 }`），100% 绿灯（Virtual 100ms, Wall-clock 84ms）。wink-tools 真实编译产出 assets 三件套（wasm 226KB）。 |
| [x] | 22 | `Timer4/TimmingMode/code` | ⚡ Level 3 | P1 | `vendor_cms8s78xx_timer4_timming_mode` | **已落地（第二批次：扩展定时器矩阵）**。扩展定时器 4 8 位自动重载定时中断（`TMR_ConfigRunMode(TMR4, TMR_MODE_TIMING, TMR_TIM_AUTO_8BIT)`），12 分频（0.5µs/tick），设定 100µs 周期溢出（`256 - 200`，10kHz 溢出），溢出触发 vector 16 `Timer4_IRQHandler`。ISR 中翻转 P32（`P32 = ~P32;`）输出精确 5000Hz 方波。原厂源码一行不改（`main.c`, `demo_timer.c`, `demo_timer.h`, `isr.c`）。底层支持：① `REG_CMS8S78XX.H` 补齐 SFR `TL4 (0xE2)`, `TH4 (0xE3)` 以及 `TMR_T34MOD_T4*` 位掩码；② `TMR_*` StdDriver inline shims 全面扩展支持 `Timern == TMR4`；③ `mcs51_timer.cpp` 实现 Timer 4 状态管理与微秒时间推进调度，防止 ISR 重入二次溢出。场景：`timer4_timming_mode.scenario.json`，`ASSERT_WAVEFORM` 在 pin 26 (P3.2) 断言载波频率为 5000Hz（`$near: { target: 5000, tolerance: 100 }`），100% 绿灯（Virtual 100ms, Wall-clock 82ms）。wink-tools 真实编译产出 assets 三件套（wasm 226KB）。 |

#### 定时器体系技术拆解与分批实施路径
为保障适配与实证的高保真与高效收敛，Timer0~Timer4 剩余 7 个子示例按底层共性分为三大批次实施：

1. **第一批次：外部引脚脉冲计数收割（Timer0/1 CountMode，编号 14 & 16）**
   - **核心特征**：底层 `wink_mcs51_timer_pulse` 计数逻辑已具备，重点解决原厂特有的 `PS_T0 (0xF0C2)` 和 `PS_T1 (0xF0C4)` 引脚重映射（P2.5/P2.4）。
   - **验证目标**：通过 `INPUT_PLUGIN_EVENT` 注入精准脉冲（5 个下降沿），验证计数器硬件溢出与 ISR 阶跃翻转，无缝达成 100% 绿灯。
2. **第二批次：扩展定时器对称矩阵（Timer3/4 TimmingMode，编号 21 & 22）**
   - **核心特征**：Timer 3 与 Timer 4 是完全对称的 8 位自动重载扩展定时器，共用 `T34MOD (0xD2)` 控制寄存器，通过中断向量 15 (`Timer3_IRQHandler`) 与 16 (`Timer4_IRQHandler`) 驱动。
   - **验证目标**：全面扩展 `REG_CMS8S78XX.H` 中的 `TMR_*` 基础 API（支持 `Timern == TMR3/TMR4`）与 C++ 仿真事件调度，通过 `ASSERT_WAVEFORM` 断言精准 5000Hz 方波输出。
3. **第三批次：Timer 2 高级捕获与比较（Timer2 Count/Compare/Capture，编号 18, 19, 20）**
   - **核心特征**：涉及增强型片内外设特性（`CC0~CC3` 比较匹配触发、上升沿输入捕获锁存、P1.6 外部时钟计数）。
   - **验证目标**：补齐 `CCEN (0xCE)`、`CCL1~3/CCH1~3 (0xC2~0xC7)` SFR，打通多通道比较中断标志与输入捕获边沿检测。

---

### 6. 电机与增强 PWM (EPWM) & 蜂鸣器
| 状态 | 编号 | 官方子示例路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [x] | 23 | `Buzzer/code` | 🎯 Level 1 | P1 | `vendor_cms8s78xx_buzzer` | **已落地**。片内硬件蜂鸣器分频器在 P0.3（pin 3）输出 10kHz 精确方波。原厂源码一行不改（`main.c`, `demo_buzzer.c`, `demo_buzzer.h`, `isr.c`）。框架新增 `sfr BUZDIV (0xBE)`、`sfr BUZCON (0xBF)`、`GPIO_P03_MUX_BUZZ (0x05)` 及 StdDriver 3 个 inline shims，并遵循 Task R1 描述符规范实现 `cms8s_buzzer` 硬件级分频方波生成器模型。场景：`buzzer.scenario.json`，`ASSERT_WAVEFORM` 在 pin 3 断言载波频率为 10000Hz（`$near: { target: 10000, tolerance: 200 }`），100% 绿灯（Virtual 100ms, Wall-clock 82ms）。wink-tools 真实编译三件套（wasm 205KB）。底座 CTest 34/34 全绿。架构分层门禁 0 findings。 |
| [x] | 24 | `EPWM/CoutMode/DownCountMode/code` | ⚡ Level 3 | P2 | `vendor_cms8s78xx_epwm_down_count` | **已落地**。EPWM 递减计数模式，周期 0x12C0（4800 ticks），零点匹配中断（Vector 18）翻转 P3.2。场景：`epwm_down_count.scenario.json`，`ASSERT_WAVEFORM` 断言 2500Hz 方波 100% 绿灯（Virtual 100ms, Wall-clock 107ms）。wink-tools 真实编译三件套完备（wasm 338KB）。架构分层门禁 0 findings。 |
| [x] | 25 | `EPWM/CoutMode/UpDownCountMode/code` | ⚡ Level 3 | P2 | `vendor_cms8s78xx_epwm_updown_count` | **已落地**。EPWM 增减中心对称双向计数模式，周期 0x12C0，零点匹配中断翻转 P3.2。场景：`epwm_updown_count.scenario.json`，`ASSERT_WAVEFORM` 断言 2500Hz 方波 100% 绿灯（Virtual 100ms, Wall-clock 89ms）。wink-tools 真实编译三件套完备（wasm 338KB）。架构分层门禁 0 findings。 |
| [x] | 26 | `EPWM/BrakeMode/Brake_Recover_Mode/code` | ⚡ Level 3 | P3 | `vendor_cms8s78xx_epwm_brake_recover` | **已落地**。EPWM 软件刹车与自动恢复模式。EPWM0/2 互补输出，使能零点中断与刹车中断。框架补充 6 个缺失中断向量宏（`INT2~4`, `UART1~2`, `SPI_I2C`）。场景：`epwm_brake_recover.scenario.json`，`ASSERT_WAVEFORM` 断言 2500Hz 方波 100% 绿灯（Virtual 100ms, Wall-clock 18.7s）。wink-tools 真实编译三件套完备（wasm 345KB）。架构分层门禁 0 findings。 |
| [x] | 27 | `EPWM/BrakeMode/Brake_Delay_Recover_Mode/code` | ⚡ Level 3 | P3 | `vendor_cms8s78xx_epwm_brake_delay_recover` | **已落地**。EPWM 延时刹车恢复计数模式。规范化 wink-app.json 与 CMakeLists.txt 转译标准配置。场景：`epwm_brake_delay_recover.scenario.json`，`ASSERT_WAVEFORM` 断言 2500Hz 方波 100% 绿灯（Virtual 100ms, Wall-clock 17.2s）。wink-tools 真实编译三件套完备（wasm 346KB）。架构分层门禁 0 findings。 |
| [x] | 28 | `EPWM/BrakeMode/Brake_Stop_Mode/code` | ⚡ Level 3 | P3 | `vendor_cms8s78xx_epwm_brake_stop` | **已落地**。EPWM 急停硬件锁存模式。规范化 wink-app.json 与 CMakeLists.txt 转译配置。场景：`epwm_brake_stop.scenario.json`，`ASSERT_WAVEFORM` 断言 2500Hz 方波 100% 绿灯（Virtual 100ms, Wall-clock 8.0s）。wink-tools 真实编译三件套完备（wasm 346KB）。架构分层门禁 0 findings。 |
| [x] | 29 | `EPWM/BrakeMode/Brake_Supend_Mode/code` | ⚡ Level 3 | P3 | `vendor_cms8s78xx_epwm_brake_suspend` | **已落地**。EPWM 刹车挂起立即恢复模式。规范化 wink-app.json 与 CMakeLists.txt 转译配置。场景：`epwm_brake_suspend.scenario.json`，`ASSERT_WAVEFORM` 断言 2500Hz 方波 100% 绿灯（Virtual 100ms, Wall-clock 10.7s）。wink-tools 真实编译三件套完备（wasm 346KB）。架构分层门禁 0 findings。 |
| [x] | 30 | `EPWM/BrakeMode/FBBrakeMode/code` | ⚡ Level 3 | P3 | `vendor_cms8s78xx_epwm_brake_fb` | **已落地**。EPWM 外部硬件故障引脚刹车模式（FB0 映射至 P0.6 引脚复用）。规范化 wink-app.json 与 CMakeLists.txt 转译配置。场景：`epwm_brake_fb.scenario.json`，`ASSERT_WAVEFORM` 断言 2500Hz 方波 100% 绿灯（Virtual 100ms, Wall-clock 90ms）。wink-tools 真实编译三件套完备（wasm 346KB）。架构分层门禁 0 findings。 |
| [x] | 31 | `EPWM/BrakeMode/ACMPBrakeMode/code` | ⚡ Level 3 | P3 | `vendor_cms8s78xx_epwm_brake_acmp` | **已落地**。EPWM 片内模拟比较器 ACMP0 联动故障刹车模式。支持 demo_acmp 与 demo_epwm 多源联合编译。场景：`epwm_brake_acmp.scenario.json`，INPUT_ANALOG 驱动 P1.1 (C0P0) 穿越 0.24V 阈值（0.10->0.50）触发 ACMP 输出翻转并驱动 Vector 18 故障刹车中断，ASSERT_POINT 断言 P3.3 电平跃迁 100% 绿灯（Virtual 300ms, Wall-clock 154ms）。wink-tools 真实编译三件套完备（wasm 352KB）。架构分层门禁 0 findings。 |

---

### 7. 看门狗与复位管理 (WDT & Reset & LVD)
| 状态 | 编号 | 官方子示例路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [x] | 32 | `WDT/code` | ⚡ Level 3 | P1 | `vendor_cms8s78xx_wdt` | **已完成适配**。看门狗 174.76ms 溢出周期，触发 Vector 20 中断翻转 P3.2。 |
| [x] | 33 | `Reset/ResetBySoftware/code` | ⚡ Level 3 | P2 | `vendor_cms8s78xx_reset_software` | **已落地**。软件复位控制（SWRST）仿真支持。触发软复位后内核及寄存器状态复位重入，main() 重新执行并重复 250 次 P3.2 翻转闪烁。场景：`reset_software.scenario.json`，`ASSERT_WAVEFORM` 在 [50ms, 300ms] 窗口断言方波活性 100% 绿灯（Virtual 500ms, Wall-clock 212ms）。wink-tools 真实编译三件套完备（wasm 319KB）。架构分层门禁 0 findings。 |
| [x] | 34 | `Reset/ResetByWDT/code` | ⚡ Level 3 | P2 | `vendor_cms8s78xx_reset_wdt` | **已落地**。看门狗喂狗防复位全链路闭环。主循环周期性规律喂狗并翻转 P3.2，P3.3 保持恒高验证跨周期未触发 WDT 溢出或异常复位。场景：`reset_wdt.scenario.json`，4 步断言 100% 绿灯（Virtual 500ms, Wall-clock 196ms）。wink-tools 真实编译三件套完备（wasm 326KB）。架构分层门禁 0 findings。 |
| [-] | 35 | `Reset/ResetByExtReset/code` | ⚡ Level 3 | P2 | - | **暂缓（deferred）**。依赖 GAP-06 CONFIG 选项字节建模与外部物理 NRST 复位引脚高低电平边沿模型。复位控制器软件重入能力已由底层单测与 SWRST/WDT 验证，外设模型暂留待 CONFIG 建模完成后落地。单测已提供测试缝覆盖。 |
| [x] | 36 | `LVD/code` | ⚡ Level 3 | P2 | `vendor_cms8s78xx_lvd` | **已落地**。低压检测 4.0V 阈值（LVDSEL 16 档）下降沿锁存触发 Vector 26 中断翻转 P3.2，100mV 行为级迟滞防中断风暴（PLAN-20260924-CMS8S78XX-LVD）。场景：`lvd.scenario.json`，8 步断言 100% 绿灯（Virtual 1s, Wall-clock 201ms）。Host 单测 `test_mcs51_cms8s_lvd` 6 用例全绿。wink-tools 真实编译三件套完备（wasm 367KB）。原厂四源文件零改动。架构分层门禁 0 findings。 |

---

### 8. 通信总线与片外存储 (I2C & SPI & FLASH)
| 状态 | 编号 | 官方子示例路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [x] | 37 | `I2C/I2C_Master_AT24C256/code` | ⚡ Level 3 | P3 | `vendor_cms8s78xx_i2c_master_at24c256` | **Phase 2 证据（数据一致，已正式摘牌）**：片内 I2C 主机模型 `cms8s_i2c.cpp` 按 ADR-0085/0086 将 `I2CMCR` 命令路由到会话 ABI（START\|RUN→`session_open`/`session_restart`、RUN→`session_write`/`session_read(len=1, ACK/NACK)`、STOP→`session_close`；`ADD_ACK`/`DATA_ACK` 由 `pal_i2c_result_t` 回填；host 侧提供可脚本化总线 mock，wasm 路径永不伪造数据）；`i2c_eeprom`（AT24C256，7-bit `0x50`）经 `wink-app.json` → `device-tree.json` 挂载。headless 场景 8 步断言 100% 绿灯：插件通道字节级证据 `plugin:at24c256/writeCount=6`、`readbackHex="3233343536"`（`At24c256_read_str(0x11,5)` 读回 0x32..0x36）、`readCount=6`、`addressPointer=0x16`（Virtual 2s / Wall-clock 302ms，退出码 0）；CTest `test_mcs51_cms8s_i2c` 覆盖 host mock 会话路由 + ADDR_NACKED + reset 释放。执行记录见 [PLAN-20260921](../../../docs/implementation-plans/mcs51/2026-09-21-cms8s78xx-i2c-spi-deadlock-resolution-plan.md) v2.x。 |
| [x] | 38 | `SPI/SPI_Master_95256/code` | ⚡ Level 3 | P3 | `vendor_cms8s78xx_spi_master_95256` | **Phase 2 证据（数据一致，已正式摘牌）**：片内 SPI 主机模型 `cms8s_spi.cpp` 按 ADR-0087 将 `SSCR.NSSO1` 边沿映射为 `session_open`/`session_close`、`SPDR` 写映射为 `session_transfer(len=1)`（`SPCR` 的 CPOL/CPHA/SPRn 映射为 mode/sck_hz）；`spi_eeprom`（M95256，逻辑设备号 `0`，WREN 帧 CS 上升沿锁存 WEL、WRITE 帧 CS 上升沿提交）经 `device-tree.json` 挂载。headless 场景 7 步断言 100% 绿灯：`plugin:m95256/writeCount=1`、`readbackHex="08"`（`SPI_M95256_Read_Data(0x15)` 读回 0x08）、`readCount=1`、`wel=0`（Virtual 2s / Wall-clock 226ms，退出码 0）；CTest `test_mcs51_cms8s_spi` 覆盖 host mock 会话路由 + CS 帧边界。执行记录见 [PLAN-20260921](../../../docs/implementation-plans/mcs51/2026-09-21-cms8s78xx-i2c-spi-deadlock-resolution-plan.md) v2.x。 |
| [-] | 39 | `FLASH/code` | ⚙️ Level 4 | P3 | - | 内部 Code/Data Flash IAP 擦写校验。Wasm 内存拦截需专门映射。 |

---

### 9. 低功耗与时钟系统 (SLEEP & Clock)
| 状态 | 编号 | 官方子示例路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [x] | 40 | `SystemClock/code` | ⚡ Level 3 | P2 | `vendor_cms8s78xx_systemclock` | **已落地**。P13 输出系统时钟 64 分频（CLO，Fsys=24MHz → 真 375kHz，纯整数 Bresenham 相位累加器零频偏）+ P32 软件延时闪烁指示。原厂 `main.c/isr.c` 一行不改镜像。底层支持：① `REG_CMS8S78XX.H` 补 `GPIO_P13_MUX_CLO=0x05`（`shim_audit` 0 hard mismatch）；② 新建 `cms8s_clo` 外设模型（buzzer 同构，`MCS51_PHASE_CLOCK` 注册，`MCS51_MAX_PERIPHERALS 12→16`）；③ Host CTest `test_mcs51_cms8s_clo` 7 用例全绿（mux 门控、Bresenham `[1,1,2]µs` 步进、6MHz 变频跟随 93.75kHz、跨 Family 隔离、飞态复位）；④ 场景：`clo.scenario.json`（`ASSERT_WAVEFORM` pin 11 在 [100ms,3900ms] 窗口断言 `$near 375000±2000`，4s 长跑稀释引擎 10ms 批量时间戳量化噪声，±500Hz 实测稳定）+ `p32.scenario.json`（pin 26 翻转活性 `$between [1000,300000]`，双文件隔离：headless `getPinEdges` 不分引脚，混窗会互染计数），两场景 100% 绿灯（Virtual 4s / Wall ~3.4s+5.0s）。wink-tools 真实编译三件套完备（wasm 365KB）。`test_mcs51_context_budget` 仍绿，mcs51 suite 75/79（4 项预存 MSVC 失败与本计划无关），`winkcli lint` 0 findings，许可门禁 OK。注：短窗 20ms 方案不可行——引擎按 ~10ms 主节拍批量执行固件、同节拍边沿共享同一时间戳，375kHz 在毫秒窗内不可分辨（实证发现，已记入计划 v1.3 偏离记录）；另发现 `timer0_timming_mode` 在干净树亦读 10000（期望 5000），系引擎侧预存漂移，非本计划引入。 |
| [-] | 41 | `LSE/code` | ⚡ Level 3 | P3 | - | 外部 32.768kHz 晶振停振/起振检测，翻转 P10。 |
| [-] | 42 | `SCM/code` | ⚡ Level 3 | P3 | - | 时钟安全监控（SCM），时钟切换保护。 |
| [-] | 43 | `SLEEP/` (5个子用例) | ⚙️ Level 4 | P3 | - | IDLE/STOP 模式与 GPIO/INT/WUT 唤醒。纯软件协程下通常跑在真机。 |

---

## 四、 标准适配工作流与门禁 (SOP)

### 硬性打勾准入标准 (Non-Negotiable Gates)
所有 Checklist 项目在由 `[ ]` 更新为 `[x]` 之前，**必须**完成以下两个硬性工序：

1. **真实编译输出仿真资产 (Real Build)**：
   - 执行：`python packages/wink-tools/wink.py build sim --app vendor_cms8s78xx_<feature>`
   - 验证目标目录 `<app_dir>/unisim-assets/` 下完整包含：
     - `device-tree.json`（设备树描述，由 `wink-app.json` 正确解析生成）
     - `wink_simulator.js`（Wasm 胶水层文件）
     - `wink_simulator.wasm`（固件行为级仿真二进制，大小通常在 150KB~300KB）

2. **Headless 自动化场景断言 (Headless Verification)**：
   - 编写确定性测试场景 `<app_dir>/unisim-scenarios/<feature>.scenario.json`
   - **方式 A（推荐，wink-tools 全自动）**：
     ```powershell
     cd D:\workspaces\ai-coding\wink-ai\wink-ai
     python packages/wink-tools/wink.py sim run `
       --app vendor_cms8s78xx_<feature> `
       --mode headless `
       --scenarios ../wink-ai-embedded/wink-micro-app/vendor_cms8s78xx_<feature>/unisim-scenarios/<feature>.scenario.json
     ```
   - **方式 B（在 packages/unisim 目录下通过 bun 直接执行）**：
     ```powershell
     cd D:\workspaces\ai-coding\wink-ai\wink-ai\packages\unisim
     $env:WINK_DEV="1"
     bun bin/unisim-sim.mjs run --mode=headless `
       --app ../../../wink-ai-embedded/wink-micro-app/vendor_cms8s78xx_<feature> `
       --scenarios ../../../wink-ai-embedded/wink-micro-app/vendor_cms8s78xx_<feature>/unisim-scenarios/<feature>.scenario.json
     ```
   - 验证所有 `ASSERT_POINT`（微秒级引脚电平、外设插件状态、时钟周期）均通过断言，进程退出码为 `0`。

> 详细命令参数、配置模板与排错指南详见：👉 [CMS8S78xx 官方示例仿真适配与测试标准执行手册 (Playbook)](PLAYBOOK.md)
