# ADR-0074：8051 通道-1 外部数字 Read-Pin 缝——代理回接 `js_pal_gpio_read_state`，HiZ 回退锁存

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-08-29）** |
| **日期** | 2026-08-29 |
| **触发** | M0–M6 轨 A 验收后，UniSim 插件外设（按键/LED 等 `wink-plugin-peripherals/builtin/`）与 mcs51 之间的数据面**只通了一半**：输出方向（`js_pal_gpio_write`，sbit/整口写 diff 边沿）与模拟输入（`js_pal_adc_read_norm`）已接；**数字输入方向 C 侧从不读外部电平**——`WinkSbit/WinkSfr` 的 Read-Pin 路径只查内部 `on_read` 陷阱，无陷阱即回退锁存。结果：未修改 Keil 代码 `if(KEY==0)` 永远读到锁存值，按键插件无法驱动 8051 输入。 |
| **影响范围** | `wink-micro-os/frameworks/mcs51/include/mcs51_proxy.hpp`（Read-Pin 解析序）、`src/mcs51_uni_bridge.cpp`（host fallback）、`test/mcs51/wasm/mcs51_wasm_node_stub.js`（node 桩回调）；复用平台既有 import `js_pal_gpio_read_state`（`targets/wasm/wasm_bridge.h`、`pal_wasm_ch1_gpio.c`）。 |
| 决策者 | 项目 Owner |
| **关联 ADR** | [ADR-0071](0071-sfr-proxy-rmw-edge-data-plane.md)（SFR 代理/Read-Latch 隔离 SSOT）、[ADR-0070](0070-mcs51-zero-code-simulation-interception-layer.md)（umbrella）、[ADR-0004](0004-static-dispatch-vs-runtime-ops.md)（POD 静态分发） |
| **关联计划** | [`2026-08-27-mcs51-zero-code-simulation-plan.md`](../../implementation-plans/core/2026-08-27-mcs51-zero-code-simulation-plan.md)（M7 数字读缝） |

---

## 1. 背景（Context）

通道数据面在轴 A（平台 wasm bridge）早已双向齐备：

- **写方向（MCU→插件）**：`js_pal_gpio_write(linear_pin, level)`——mcs51 代理写路径经 diff 边沿分发已调用（ADR-0071 D3），驱动 LED 等输出插件。
- **模拟读（插件→MCU）**：`js_pal_adc_read_norm(pin)`——ADC0832/CMS8S 通道 3 已接。
- **数字读（插件→MCU）**：`js_pal_gpio_read_state(pin) -> uint8_t`——平台枚举契约 `JS_GPIO_STATE_LOW=0 / HIGH=1 / HIZ=2 / CONFLICT=3`（`wasm_bridge.h`），生产 `wink_sim_js.js` 默认返回 2（HiZ），`pal_wasm_ch1_gpio.c` 消费。**但 mcs51 代理从未调用它。**

mcs51 侧 Read-Pin 现状（ADR-0071 D2）：`WinkSbit::operator uint8_t()` 与 `WinkSfr::operator uint8_t()` 仅动态调用各 bit 的内部 `on_read` 陷阱（模型自有的位，如 ADC0832 DO 线）；无陷阱即取 `wink_mcs51_sfr_shadow` 锁存。因此一条被外部按键拉低的输入引脚，8051 代码读不到——纯输入引脚（用户代码从不写它）锁存停在复位值。

目标：让未修改 Keil 代码 `if(KEY==0) LED=0;`（`samples/gpio_in_out.c`，KEY=P3.2/线性 pin 26，LED=P1.0/pin 8）经同一 `js_pal_gpio_read_state` import 读到按键插件驱动的真实外部电平，在受限 host + Node/wasm ctest harness 内端到端打通。**不含**生产 worker 装载 mcs51 wasm、device-tree 引脚映射、Vue 画布（延后 M7 集成里程碑）。

## 2. 方案比选（Options）

| 方案 | 描述 | 优 | 劣 | 结论 |
|---|---|---|---|---|
| A. 测试继续写锁存注入 | 维持现状，数字输入靠直接写 `sfr_shadow[P3]` 模拟按键 | 零改动 | 锁存注入是 M3 hack，不经过真实数据面；无法证明插件→MCU 方向；纯输入引脚语义错误 | ❌ |
| B. 新增 mcs51 专用数字读 import | mcs51 自定义 `mcs51_read_pin()`，与轴 A 通道契约并行 | 可定制 | 违背同源/SSOT；生产 PinArbiter 需接两套；重复枚举 | ❌ |
| C. 代理 Read-Pin 回接既有 `js_pal_gpio_read_state`，HiZ/conflict 回退锁存 | 复用轴 A 枚举契约；外部驱动 0/1 胜出，无驱动（2/3）回退锁存 | 零回归（锁存注入/RMW/blinky 全保留）；与生产同一 import；RMW 红线不触碰 | 三路解析序需严守；host 需补 fallback | ✅ **采纳** |

## 3. 决策结论（Decision）

### D1. 三路 Read-Pin 解析序
`WinkSbit`/`WinkSfr` 的 Read-Pin（**非 RMW**）路径按序解析每一位：
1. **内部 `on_read` 陷阱优先**——模型拥有该位（ADC0832 DO、CMS8S 等）时陷阱值胜出，保证既有 ADC 回归不变。
2. **外部通道-1 电平**——调用 `js_pal_gpio_read_state(linear_pin)`；返回 `HIGH(1)`/`LOW(0)` 即采用该电平。
3. **锁存影子兜底**——返回 `HiZ(2)`/`CONFLICT(3)`（无外部驱动）或为控制 SFR 时，取 `wink_mcs51_sfr_shadow` 锁存位。

整端口读逐位合成：有陷阱用陷阱，否则外部 0/1 置/清该位，HiZ 则保留 `val` 中已有锁存位。

### D2. RMW 永不读外部脚（红线不变）
所有读-改-写复合赋值（sbit `^= |= &=`；整口 `|= &= ^= += -= ++ -- <<= >>=`）**仍只读锁存影子**，一字不改（ADR-0071 §2.2 准双向口 FET 锁死红线）。外部电平仅进入 Read-Pin 语义（`if(KEY)`、`uint8_t v=P1`  plain read）。

### D3. 线性引脚约定复用
`linear_pin = (port << 3) | bit`（P0.0…P3.7 → 0…31），与 ADR-0071 D3 写方向及轴 A PinArbiter 完全一致。KEY=P3.2→26，LED=P1.0→8。

### D4. 三端 import 解析
- **wasm（emscripten）**：`js_pal_gpio_read_state` 为 JS import；node 桩 `mcs51_wasm_node_stub.js` 回调测试导出的 getter `Module['_mcs51_wasm_ext_pin_state'](pin)`，未导出时优雅返回 2（HiZ）。生产由 `wink_sim_js.js` 接 PinArbiter。
- **host（MSVC/MinGW）**：无 JS 数据面，`mcs51_uni_bridge.cpp`（`#ifndef __EMSCRIPTEN__`）提供脚本化 fallback：`static uint8_t s_host_ext_pin[32]` 懒初始化为 HiZ(2)，配 `wink_mcs51_host_set_ext_pin()`/`wink_mcs51_host_ext_pins_reset()` 测试注入访问器。
- **ESP32**：mcs51 树仍 `EXCLUDE_FROM_ALL` + `if(ESP_PLATFORM) return()` 自跳过，**零增量**。

## 4. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| 未修改 Keil 数字输入代码首次读到真实外部电平，按键→LED 数据面双向打通 | Read-Pin 三路解析序须严守；新增读路径须评审不得让 RMW 误入 |
| 零回归：HiZ/conflict 回退锁存，锁存注入 gpio 测试、blinky RMW、ADC 陷阱全绿 | host fallback 数组须懒初始化 HiZ，保证不注入时仍是锁存语义 |
| 与生产 PinArbiter 同一 import 契约，M7 worker 装载无需再改 C 侧 | node 桩回调依赖导出 getter，仅该测试传 `-sEXPORTED_FUNCTIONS` |
| ESP32 零增量、无 `-fpermissive`、`#ifdef SIMULATION` 仍在最底层 | — |

**测试约束**：`test_mcs51_gpio_external_e2e.c`（host+wasm 共用驱动）经 post-init hook 注入三阶段外部电平（释放 HIGH→LED 灭 / 按下 LOW→LED 亮 / 再释放 HIGH→LED 灭），断言 `sfr_shadow[0x90]&0x01`；wasm 侧 `-sERROR_ON_UNDEFINED_SYMBOLS=1` 证明新 import 在 node 桩解析、导出 getter 链接成功。

## 5. 遵循与后续（Compliance & Follow-up）

- Accepted 后立即回写 Layer-① `07-mcs51-simulation-interception.md`（API 缝 + HiZ 回退 + 测试矩阵 host 19 / wasm 8）与 `02-virtual-clock.md` §6.5（通道-1 双向）。
- 后续 M7 集成里程碑：生产 worker 装载 mcs51 wasm 作 MCU 模型；board/device-tree 把线性 pin 0–31 映射到 PinArbiter 引脚空间；真实 button/led 插件经 PinArbiter → 同一 `js_pal_gpio_read_state`/`js_pal_gpio_write` 驱动画布。C 侧缝已就绪，M7 不再改代理。

**验收证据（2026-08-29）**：`gpio_in_out` 未修改样例 P3.2 按键经 `js_pal_gpio_read_state` 驱动 P1.0 LED，三阶段（释放→按下→再释放）锁存读回全对；MSVC host 新 exe 直跑 PASS、MinGW host mcs51 19/19、wasm/Node 8/8（含新 `wasm_mcs51_gpio_external_test`，`ERROR_ON_UNDEFINED_SYMBOLS=1` 下链接通过）；arch lint（layering+api）无发现；ESP32 零增量（mcs51 自跳过，无 esp32 target 引用 `frameworks/mcs51`）。

---

*本 ADR 状态变更请在此记录：*
- 2026-08-29：Proposed → Accepted（数字读缝在 host + Node/wasm 受限 harness 端到端验证——MinGW 19/19、wasm 8/8、arch lint 无发现、ESP32 零增量；随即回写 Layer-①）。
