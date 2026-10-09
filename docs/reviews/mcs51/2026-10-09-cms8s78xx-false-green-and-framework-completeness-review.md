# CMS8S78xx 示例假绿与 MCS-51 框架完整性审查

| 项 | 内容 |
|---|---|
| 日期 | 2026-10-09 |
| 性质 | 当前代码快照审查；归档后只读 |
| 基线 | `09d002f7b34dd4b9b585e164a119642785af071a` |
| 示例范围 | `wink-micro-app/vendor/cms8s78xx/`：37 个 App、38 个场景文件 |
| 框架范围 | `wink-micro-os/frameworks/mcs51/` 及其公共 PAL 构建依赖；不对 Arduino 等其他 framework 作完整性结论 |
| 设计入口 | [MCS-51 仿真拦截规范](../../zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md) |
| 相关决策 | [ADR-0070](../../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md)、[ADR-0072](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md)、[ADR-0076](../../decisions/core/0076-mcs51-sim-backends-native-vs-iss-channel-roadmap.md)、[ADR-0082](../../decisions/core/0082-mcs51-reset-semantics-fiber-exit-and-reentry.md) |
| 修改范围 | 仅本审查记录和忽略目录下的复验产物；未修改示例、框架或 PAL 实现 |

## 1. 结论与证据边界

**存在假绿，且当前框架不能称为完善全面。** 更准确的定位是：已有较扎实的 Native 行为级仿真底座，若干外设已形成输入—处理—输出闭环，但部分“完成”条目只验证中断或指示灯活性，尚未验证该示例命名所承诺的功能。EPWM 已实测出现“计数事件正常、真实输出缺失、外部故障输入失效”的组合，现有测试仍没有覆盖这些行为。

本报告区分三种证据：

1. **动态复现的实现缺陷**：使用重新编译的 Host32 库运行独立 EPWM 探针。
2. **源码可证明的断言盲区**：场景观测量与业务变量、比较事件或复位事实没有充分因果联系。未将这些反例描述成已完成整套 Headless 变异测试。
3. **本次构建与门禁结果**：Host 单测、SDCC 容量门禁与 Wasm 构建分别统计，互不替代。

本次没有获得“38 个场景当前全部通过”的结论。通过正式 CLI 重新运行温度示例时，自动 Wasm 构建在公共 PAL 编译阶段失败，尚未进入场景执行。目录中已有的资产与历史绿灯不能替代当前源码的重新构建证据。

## 2. 本次实际复验

| 检查 | 结果 | 可以证明的范围 |
|---|---|---|
| 目录及场景盘点 | 37 App / 38 场景；每个 App 均已有资产三件套 | 文件存在，不代表资产与当前源码一致 |
| 全新 i686 GCC 16.2 Debug Host 构建与 `ctest -R '^test_mcs51_'` | **71/71 通过**，约 20 秒 | 当前配置下所注册的 MCS-51 Host 测试通过，不是 37 个官方 App 全链路通过 |
| 转译器 Python 测试 | **10/10 通过** | 既有转译用例通过，不是完整 Keil C51 语义等价证明 |
| 37 App 的 Tier-S SDCC 4.6.2 编译、链接与容量门禁 | **31/37 通过、6/37 失败** | 通过项满足该 SDCC 门禁；不等于 Keil 接受或真机烧写成功 |
| 正式 CLI 温度示例 Headless 复验 | **构建失败，场景未执行** | 当前公共 Wasm 构建链路存在实际源码错误 |
| 寄存器 shim 审计 | `hard_mismatches=[]`；SFR vendor 101 / shim 90，XSFR vendor 204 / shim 172 | 已比较声明的常量对齐；仍有缺失，不能据此宣称行为全部实现 |
| 官方镜像核对 | 148 个顶层 `.c/.h`；63 个文本不同，忽略注释及空白后 11 个仍有 token 差异 | “原厂源码一行不改”不成立；并非 63 个文件都有业务变化 |
| 独立 EPWM 探针 | 3 类缺陷均复现 | 见 F1—F3 |

本地复验产物保留在 `artifacts/cms8s78xx-audit-20261009/`，包括 `host32-tests.xml`、`transpiler-pytest.log`、`sdcc-gate-workspace.log`、`wasm-build.log`、`shim-audit.json`、`mirror-audit.json`、`scenario-inventory.json`、`epwm_probe.cpp`、`run_probe.py`、`epwm-probe.log`。该目录被 Git 忽略，不是已入库的 CI 凭据。

复验过程中另发现 x64 GCC 的 context budget 断言与 Windows manifest 链接兼容性问题；上述 71/71 来自独立的 i686 构建，不把 x64 环境失败混入外设业务结论。

## 3. 主要发现

### F1 — P1：EPWM 只实现计数、标志与部分刹车状态，没有实现 PG 波形输出

**实现证据。** [cms8s_epwm.cpp](../../../wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_epwm.cpp) 声明了 `PWMOE`、`PWMPINV`、`PWMD0L/H`、`PWMDTE`、`PWMFBKD` 等地址，但模型没有用这些寄存器生成输出，也没有 GPIO 输出调用。占空比、输出使能、反相、互补、死区和刹车输出电平没有形成可观测的 PG 引脚行为。

**动态证据。** 探针按 App 的方式配置 PG0 复用、4800 tick 周期、2400 tick 占空比并使能输出，以 10µs 步长运行 1ms：

```text
1ms_running: zero_events=5 pg_output_notifies=0 all_gpio_notifies=0
```

**为什么会绿。** 八个 EPWM 示例中，down/updown 和五个 brake 场景只断言 ISR 翻转的 P3.2（pin 26）频率。五个 brake 场景没有注入刹车条件，没有观测 PG0—PG3，也没有检查恢复时刻。ACMP brake 有阈值输入并观察 P3.3 的刹车中断，但同样没有验证 PWM 输出是否真正被切断或恢复。

例如 [epwm_brake_fb 场景](../../../wink-micro-app/vendor/cms8s78xx/epwm_brake_fb/unisim-scenarios/epwm_brake_fb.scenario.json) 全文件只有一个 pin 26 频率断言；[demo_epwm.c](../../../wink-micro-app/vendor/cms8s78xx/epwm_brake_fb/demo_epwm.c) 实际配置的是互补输出、P0.6 故障输入，以及 P2.0—P2.3 的 PG 输出。场景能证明零点中断活着，不能证明硬件刹车功能完成。[现有 EPWM 单测](../../../wink-micro-os/frameworks/mcs51/test/cms8s78xx/test_mcs51_cms8s_epwm.cpp) 也主要检查计数、标志和恢复状态，不检查实际输出波形。

**验收要求。** 至少观测 PG 输出的频率、占空比、互补关系、输出使能与刹车电平；分别注入 FB/软件/ACMP 故障，验证 stop/suspend/recover/delay recover 的不同恢复行为。关闭模型输出或移除刹车逻辑后，相关用例必须失败。

### F2 — P1：EPWM 的 FB 输入绕过外部 GPIO 仲裁，单测用内部锁存值掩盖缺陷

[cms8s_epwm.cpp](../../../wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_epwm.cpp) 的 `resolve_ps_pin_val()`（约第 76 行）直接读 `sfr_shadow`，没有使用框架已有的 `mcs51_gpio_bit_read_pin()` 外部输入路径。探针将 FB0 映射到 P0.6，端口锁存为高，通过 Host 外部输入通道拉低：

```text
FB0_external_low: resolved_pin=0 latch=1 brake_active=0
FB0_latch_low: brake_active=1
```

外部 GPIO 确实已读到低电平，EPWM 却不刹车；直接修改锁存值才触发。现有 EPWM 单测的“Hardware Pin Fault Brake”直接改 `ctx->sfr_shadow[0x90]`，因此无法发现真实输入路径失效。厂商 FB 场景又没有任何故障输入，两个测试层同时漏检。

**验收要求。** 从正式外部引脚输入通道触发，覆盖默认及重新映射 FB 引脚；禁止仅修改内部 shadow 作为硬件引脚连通性证明。

### F3 — P1：EPWM 将系统时钟写死为 24MHz

[cms8s_epwm.cpp](../../../wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_epwm.cpp) 第 276 行使用 `elapsed_us * 24 / factor`，第 411 行的下一事件计算也固定除以 24，未采用当前系统时钟。探针结果：

```text
clock_hz=24000000 zero_flag_at_200us=1 counter=4800
clock_hz=6000000 zero_flag_at_200us=1 counter=4800
```

相同周期在 6MHz 下应约 800µs 才到达零点，现在仍在 200µs 到达。现有 App 固定使用 24MHz，测试没有变频维度。框架的动态时钟能力没有覆盖到 EPWM。

### F4 — P1：温度场景没有观测温度或持续采样，稳定低电平不能证明这些业务

[温度场景](../../../wink-micro-app/vendor/cms8s78xx/temperture_sensor/unisim-scenarios/temperture_sensor.scenario.json) 只有 100ms、300ms 两次 `gpio:26 == 0`。而 [main.c](../../../wink-micro-app/vendor/cms8s78xx/temperture_sensor/main.c) 在 `TS_Adjust()` 之后把 P32 置零，之后循环只更新 `TsValue`，不再改变 P32。

因此，`TS_GetTemperature()` 返回错误常量、温度解算公式错误、后续采样失效等情况不会由这两条断言检出。场景描述中的“持续采样”“程序无死锁”超出了实际观测能力。低电平至多是校准之后某个执行点的间接证据。

[cms8s_adc.cpp](../../../wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_adc.cpp) 的 TS 子通道目前按 `1365 + (trim - 8) * 7` 生成名义值，有 trim 响应，但没有可变化的环境温度输入。这里不能用“TS_REG 已 allowlist + GPIO 低电平”替代传感器完整性证明。

**验收要求。** 提供独立观测 `TsValue`、校准档位及采样进度的通道；给至少三个温度输入，验证解算值和误差，并加入冻结采样、错误常量、校准不收敛等失败反例。不能为了观测而改写官方业务以满足断言。

### F5 — P1：Timer2 Compare 场景实际只验证 overflow，Capture 也缺数据验证

[Timer2 Compare 场景](../../../wink-micro-app/vendor/cms8s78xx/timer2_compare_mode/unisim-scenarios/timer2_compare_mode.scenario.json) 只检查 P32 约 500Hz。[isr.c](../../../wink-micro-app/vendor/cms8s78xx/timer2_compare_mode/isr.c) 第 128—150 行中，P32 仅在 overflow 分支翻转；CC0—CC3 compare 分支只清标志。即使比较事件全部缺失，overflow 仍可给出这个波形。

[cms8s_timer.cpp](../../../wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_timer.cpp) 的 `on_timer2_compare_match()` 实现了标志和 IRQ，但未驱动 App 所配置的 CC 引脚输出。场景没有验证这些输出，也没有独立验证 500µs 比较事件。

Capture 场景有 CAP0 输入与 ISR 翻转，具有事件因果证据；但没有检查锁存计数值，也没有覆盖 CAP1—CAP3。应将它标为捕获事件链路通过，而不是捕获数据和全通道全面通过。

### F6 — P1：ResetWDT 的“未复位”结论缺少独立复位观测，而且镜像已被修改

[ResetWDT 场景](../../../wink-micro-app/vendor/cms8s78xx/reset_wdt/unisim-scenarios/reset_wdt.scenario.json) 在 50/200/300ms 检查 P33 为高，再检查 P32 位于 100—200000Hz 的宽频率窗口。[main.c](../../../wink-micro-app/vendor/cms8s78xx/reset_wdt/main.c) 第 38 行每次进入都会设置 `P33=1`。

固定高电平无法区分“从未复位”和“复位重入后再次写高”；几个离散采样点也不能证明全程未复位。禁用 WDT 而仍运行主循环，同样没有对应的断言能明确检出。P32 活性证明程序仍在执行，不能单独证明喂狗正确或 WDT 没有复位。

根据各 App manifest 的 `upstream.source_dir` 对照本地官方原件，ResetWDT 原始 main 中 P33 初始化为 0；镜像改为 1，且调整了 WDT 初始化相对 GPIO 初始化的顺序。这是实际代码变化，与 [README](../../../wink-micro-app/vendor/cms8s78xx/README.md) 的“一行不改”承诺冲突。其余 token 差异包括新增或删除空 ISR、移除空语句、移动局部声明，应逐项区分，不把格式差异全部算成业务篡改。

**验收要求。** 喂狗正例明确检查 `reset_count` 不增；故意停喂负例明确检查计数增加、WDT 原因及重入，并检查复位状态。单独的 WDT 溢出中断示例已有多周期翻转断言，这不能替代 ResetWDT 的复位验证。SWRST 的跨窗口活性具有重入的间接证据，但仍需补复位原因和状态检查。

### F7 — P1：六个 EPWM 刹车示例未通过当前 SDCC 门禁

失败 App 为 `epwm_brake_acmp`、`epwm_brake_delay_recover`、`epwm_brake_fb`、`epwm_brake_recover`、`epwm_brake_stop`、`epwm_brake_suspend`，全部在 `isr.c` 报未定义 `INT2_VECTOR`、`INT3_VECTOR`、`INT4_VECTOR`、`UART1_VECTOR`、`UART2_VECTOR`、`SPI_I2C_VECTOR`，并产生重复中断号等后续错误。

镜像对照显示这些空 ISR 是后加的；Native shim 中相应宏被标注为根据向量表推断，官方设备头转译出来的 SDCC 路径没有这些宏。于是 Native 可编译与官方源硬件编译路径发生分歧。不能将 Host/Wasm 通过等同于“这批镜像仍可供硬件编译”。[容量门禁自身](../../../wink-micro-os/frameworks/mcs51/tools/gate_app_hardware_capacity.py) 明确说明，连 SDCC 通过也不等于 Keil 接受或可烧写。

### F8 — P1：当前 Wasm 公共 PAL 编译失败，历史资产无法证明当前交付链路可用

正式 CLI 运行温度示例触发重新构建，在 [pal_wasm_hwtimer.c](../../../wink-micro-os/targets/wasm/pal_wasm_hwtimer.c) 第 96/139 行使用未定义类型 `pal_hwtimer_cb_t`；[pal_hwtimer.h](../../../wink-micro-os/pal/include/hal/pal_hwtimer.h) 第 32 行实际定义的是 `pal_hwtimer_isr_t`。编译器报告 8 个错误并退出。

这是公共构建依赖的问题，不归因于温度业务或 MCS-51 模型本身；但它确实阻止本批示例按当前正式链路重新生成并执行。修复公共构建错误后，仍须重新执行全批次，不能只保留已有 `unisim-assets/` 就继续宣称当前 100% 全绿。

### F9 — P2：寄存器声明覆盖与“行为已实现”被混淆，默认回归范围也不足

[XSFR allowlist](../../../wink-micro-os/frameworks/mcs51/chips/cms8s78xx/include/cms8s_xsfr_allowlist.h) 自动来自 shim 声明；它能证明某地址被声明，不能证明每个字段被模型消费。F1 中未生效的 duty/output/dead-zone 寄存器已经在 allowlist 内，仍不会触发 unmodeled-address 检查。

[unsupported 机制](../../../wink-micro-os/frameworks/mcs51/src/mcs51_unsupported.cpp) 在 STRICT 下中止，在默认 Release 下计数并告警一次，随后 no-op。生产注入使用普通库，独立 STRICT 库用于诊断测试。38 个场景自身没有断言 unsupported/相关诊断计数；声明齐全但语义未实现的寄存器更不一定触发该机制。不能依赖 STRICT 单测证明所有生产场景都会拒绝降级。

[默认 Headless 脚本](../../../wink-micro-os/frameworks/mcs51/tools/run_mcs51_headless_evidence.ps1) 仅列出 5 个通用 MCS-51 carrier，`-App` 可指定单个 vendor App，但默认不会遍历这里的 37 个 App。其末尾的 “All mcs51 headless carriers PASSED” 不是该 vendor 目录全量证明。公共 [PR workflow](../../../.github/workflows/pr.yml) 在工具链不可用时跳过构建和测试，也不能据绿色 job 推导芯片套件已执行。

## 4. 37 个示例的断言覆盖判断

下表评价场景实际验证的性质，不表示本次重新执行这些场景均通过。

| App / 组 | 数量 | 实际覆盖与主要缺口 |
|---|---:|---|
| `gpio`, `extint0`, `extint1` | 3 | 有按下、保持、释放和重复刺激，对应输出响应；闭环较强 |
| `acmp0`, `acmp1`, `lvd` | 3 | 有模拟阈值跨越与 GPIO 响应；尚不能推出全部滤波、迟滞和输入组合完整 |
| `timer0_count_mode`, `timer1_count_mode`, `timer2_count_mode` | 3 | 检查前 4/第 5/第 9/第 10 脉冲及翻转，对计数因果约束较强 |
| `timer0_timming_mode`, `timer1_timming_mode`, `timer2_timing_mode`, `timer3_timming_mode`, `timer4_timming_mode` | 5 | 频率与中断链路验证；模式、边界与变频覆盖有限 |
| `buzzer` | 1 | 验证实际输出引脚约 10kHz，比只看 ISR 心跳更有业务意义 |
| `systemclock` | 1 | 两个场景分别看 CLO 与 P32 活性；不能据此保证全部外设变频跟随，EPWM 已有反例 |
| `uart0_printf`, `uart0_rxtx` | 2 | 有总线 payload、输入与回显数据校验；不等于全部 UART 时序、错误与流控特性完整 |
| `led_4com_8seg` | 1 | 检查显示文本、段码、位数和扫描频率，业务证据较强 |
| `i2c_master_at24c256`, `spi_master_95256` | 2 | 有插件写次数、读回字节和状态检查；尚可加强 MCU 侧接收缓冲及 NACK/异常事务验证 |
| `wdt` | 1 | 检查多个周期的溢出中断翻转；没有替代 reset 分支验证 |
| `adc_ldo` | 1 | 输入模拟值、检查 EOC/ISR 活性；不检查 ADC 数值、Vref 与对齐 |
| `adc_hardware_trigger` | 1 | 有触发边沿与响应因果；不检查转换结果准确性 |
| `timer2_capture_mode` | 1 | 检查 CAP0 事件响应；不检查锁存值与其他通道 |
| `timer2_compare_mode` | 1 | 只看 overflow 波形，对 compare 功能有直接假绿条件，见 F5 |
| `epwm_down_count`, `epwm_updown_count` | 2 | 看零点 ISR 频率，没有输出占空比/互补/死区验证 |
| 六个 `epwm_brake_*` | 6 | 五个只有零点 ISR 活性；ACMP 项有故障 IRQ 因果；均缺输出刹车闭环，六个 SDCC 均失败 |
| `temperture_sensor` | 1 | 两次固定 GPIO 低电平，对温度结果和持续采样有直接假绿条件 |
| `reset_software` | 1 | 跨窗口活性是重入间接证据；缺独立复位计数、原因与复位状态 |
| `reset_wdt` | 1 | 固定 P33 高电平加 P32 活性不足以证明未复位，见 F6 |
| 合计 | **37** | `systemclock` 有 2 个场景，因此场景总数为 38 |

宽频率窗口本身不必然是假绿：行为级仿真可以合理使用活性窗口。问题在于把活性通过扩大解释为采样数值、比较输出、刹车恢复或复位安全已经正确。

## 5. 框架成熟度判断

| 维度 | 当前判断 |
|---|---|
| 架构与隔离 | 有 core / chip / board 分离、context、trap、IRQ、GPIO 仲裁和虚拟时间机制，具有实际工程基础 |
| 已实现的外设能力 | 定时器、UART、ADC、GPIO、ACMP、WDT/LVD、CLO、蜂鸣器，以及 I2C/SPI 会话等均有实质代码和测试 |
| CMS8S78xx 全功能覆盖 | 不完整；EPWM、比较输出已发现实际缺口，LCD/IAP/外部复位/部分低功耗与外部时钟场景未闭环 |
| 语言与执行语义 | Native 将 C51 转成 C++17，仍使用宿主整数和内存模型，不能等同于 8051 指令执行 |
| 防降级门禁 | 有诊断基础，但寄存器声明不等于语义实现；现有场景没有完整的失败反例和能力计数验收 |
| 当前可交付性 | Host 指定测试通过；官方镜像 SDCC 有 6 个失败；Wasm 正式重建失败，不能宣称全链路可交付 |

[framework CMake](../../../wink-micro-os/frameworks/mcs51/CMakeLists.txt) 明确将这一层定位为 Host/Wasm 仿真专用。[REGX52.H](../../../wink-micro-os/frameworks/mcs51/include/REGX52.H) 擦除 `reentrant/data/idata/xdata/pdata`，把 `code` 映射为 `const`、`bit` 映射为字节。转译器有注释、字符串、部分预处理条件保护，但它仍不是保证 C51 类型、地址空间及 ABI 等价的完整编译器。

虚拟时间主要在 SFR/XDATA/NOP 等拦截点推进；纯内存计算循环和空 for 延时不会按真实指令耗时前进。空超级循环有 NOP 注入，也不能据此推出任意业务死循环都能被配额机制抢占。软件延时、ISR 时长、栈/寄存器组、整数溢出等真机语义需要能力边界与硬件/ISS 证据，不能用 Native 全绿代替。

全量账本自身也只列 37/43 已完成：LCD 待适配；外部复位、FLASH、LSE、SCM、SLEEP 等暂缓。Shim 审计仍列出 16 个未进入 shim 的 SFR 名称及 32 个 XSFR 名称，包括 WUT、IAP、LCD、LSE、SCM、驱动强度等。其中存在别名和已有 unsupported 捕获，不能简单按寄存器比例计算“完成百分比”，也不能把它们全部说成静默未实现。

## 6. 建议收口顺序

1. **恢复可信构建基线**：修复公共 Wasm 编译错误；恢复官方镜像可追溯性，处理六个 SDCC 失败；用当前源码全量重新构建，记录 commit、工具链与资产身份。
2. **收回超出证据的“完成”结论**：EPWM 刹车、温度解算、Timer2 比较及 ResetWDT 分别标明实际已验证范围。保留历史记录，新增复验记录，不修改已归档评审来覆盖历史。
3. **补业务闭环与负例**：优先 PG 波形/故障恢复、温度数值/采样进度、compare 事件/输出、复位计数/原因；故意禁用目标能力时，场景必须失败。
4. **能力声明与实现对齐**：将寄存器地址、字段及可用语义分别登记；未实现能力进入显式拒绝或可审计降级路径，并核对生产场景的最终判定契约。
5. **把 37 App / 38 场景纳入明确的批次回归**：工具链缺失记为未执行，不能记为通过；输出 Host、SDCC、Wasm、场景业务和硬件证据各自的结果。

这些是后续修复与验收建议，本次未实施生产代码修改，也未把尚未执行的工作记为完成。
