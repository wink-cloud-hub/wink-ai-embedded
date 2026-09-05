# ADR-0077：通道-1 GPIO 写增加「驱动强度」轴——准双向口（8051）弱上拉/强低建模

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-09-01 落地并全绿）** |
| 日期 | 2026-09-01 |
| 触发 | `mcs51_thermos` 闭环应用接入时暴露：8051 准双向口固件按惯例写 `P3 = 0xFF` 使能输入，拦截层 SFR shadow 为 BSS（初值 0），该写对每一引脚产生 0→1 diff 边沿 → `js_pal_gpio_write(pin,1)` → host PinArbiter 把 MCU 登记为 **SUPPLY 强高驱动者**；它与 button 插件的强低驱动同强度异态 → 仲裁为 CONFLICT → 8051 Read-Pin 回退锁存值 → **按键永远读高（失效）**。根因：GPIO 写 ABI 只有 `(pin, level)`，**无驱动强度维度**，host 把一切 MCU 写当 push-pull 强驱动（esp32 语义），无法表达 8051「锁存 1=弱上拉、锁存 0=强灌低」。 |
| 影响范围 | **跨仓 ABI 变更**。embedded：`wink-micro-os/targets/wasm/wasm_bridge.h`、`pal_wasm_ch1_gpio.c`、`wink_sim_js.js`、`wink_sim_stub.js`；`frameworks/mcs51/include/mcs51_proxy.hpp`、`src/mcs51_uni_bridge.cpp`、`src/mcs51_bridge.cpp`；测试 `test/wasm/test_pal_gpio_read_wasm_semantics.c`、`test/mcs51/unit/test_sfr_edge_dispatch_accuracy.cpp`。wink-ai：`packages/unisim/src/types/wasm/imports.ts`（手写契约，非 codegen）、`src/core/bridge/unisim-bridge-factory.ts`、**ABI SSOT `packages/unisim/scripts/abi-catalog/abi-catalog.yaml` + 重生成 `hardware-channel-abi-catalog.md`**（见 §5.4）。 |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0074](0074-mcs51-channel1-external-read-pin.md)（Read-Pin 三路解析/HiZ 回退锁存）、[ADR-0071](0071-sfr-proxy-rmw-edge-data-plane.md)（准双向口 RMW 红线）、[ADR-0070](0070-mcs51-zero-code-simulation-interception-layer.md)（umbrella）、[ADR-0076](0076-mcs51-sim-backends-native-vs-iss-channel-roadmap.md)（通道分类路线图） |
| 关联计划 | [`docs/implementation-plans/core/2026-09-01-gpio-drive-strength-axis-plan.md`](../../implementation-plans/core/2026-09-01-gpio-drive-strength-axis-plan.md) |

---

## 1. 背景（Context）

通道-1 写方向现状：固件写脚 → `js_pal_gpio_write(uint16_t pin, bool level)` → host `unisim-bridge-factory.ts` 内

```ts
js_pal_gpio_write(pin, level) {
  arbiter.setDriver(pin, { id: `mcu:gpio${pin}`, state: level ? HIGH : LOW,
                           strength: DriveStrength.SUPPLY });
}
```

host PinArbiter **本已具备强度仲裁机制**（`DriveStrength { SUPPLY=3, PULL=2, WEAK=1 }`，规则：最强非 HiZ 驱动者胜；仅同强度异态才 CONFLICT；弱上拉 vs 强低不冲突、强胜弱）。缺的是 ABI 把「这次写的驱动强度」从 wasm 传到 host——强度被硬编码为 SUPPLY。

这对 esp32（push-pull 输出，高/低皆强驱动）正确，对 8051 准双向口错误。8051 端口电气事实：

| 固件写锁存器 | 8051 物理 | 应有强度 |
|---|---|---|
| `Px.n = 1` | 内部弱上拉 FET 导通（输入释放态 / 高电平输出，高边驱动能力弱） | `WEAK` |
| `Px.n = 0` | NMOS 强导通灌低 | `SUPPLY` |

该规则对输入脚与输出脚**同时成立**（8051 高边输出物理上即弱驱动——LED 常接低边点亮、P0 口需外部上拉即因此）。当前 `mcs51_thermos` 靠固件侧规避（**不写输入口 P3**，只初始化输出口 P1/P2），但这是 workaround：标准 Keil 例程普遍 `Pn = 0xFF` 初始化端口，未修改用户代码（zero-intrusion 目标，ADR-0070）撞上即失效。

## 2. 方案比选（Options）

| 方案 | 描述 | 优 | 劣 | 结论 |
|---|---|---|---|---|
| A. 维持固件侧规避 | 约定「输入口不写 `Pn=0xFF`」，写进文档 | 零 ABI 改动 | 违背 zero-intrusion：未修改 Keil 例程（惯写 `Pn=0xFF`）必失效；高边 active-high 输出（继电器）报 SUPPLY-HIGH 电气语义仍错；隐患留给每个新应用 | ❌ |
| B. host 探测 wasm 导出处分支 | host 见 `wink_mcs51_*` 导出则把 level=1 降为 WEAK | 不动 ABI | 把 8051 架构知识漏进 host 通用桥接层，违背「目标物理模型在固件拦截层」分层；每加一种内核（PDK 开漏等）host 加一个特判；level=1→WEAK 对 esp32 高边 push-pull 是误降级，无法区分 | ❌ |
| C. 新增并列 `js_pal_gpio_write_drive(pin,level,strength)`，保留旧函数 | 纯增量 | 旧符号一行不动，可灰度 | 两个语义近乎重复的 import 永久并存，旧的在 host 永远写死 SUPPLY；esp32 迁不迁都留下双路径/foot-gun；「默认强度=push-pull」的 esp32 偏见永久焊在通用 ABI | ❌ |
| D. **原地扩展** `js_pal_gpio_write(pin, level, strength)` | 强度是数字输出固有属性，进 ABI 本体；旧语义 = 新函数 `strength=SUPPLY` | 严格超集、可平替；改签名后所有调用点编译期穷尽检查，零静默遗漏；单符号 + 强度词表对齐 host 已有三级模型；esp32 显式传 SUPPLY 电气零变化；版本偏斜可 host 兜底降级 | 跨仓 ABI 破坏性变更，需两仓原子改齐（wasm 由 CLI 从源码重建，无长期存活预编译产物，风险可控） | ✅ **采纳** |

**为何不新增并列函数（否 C）**：任何引脚驱动都有强度，「无强度 write」实为「默认 SUPPLY=push-pull」这一 esp32 专属假设。留它等于把该假设固化进通用 ABI，新内核反复踩坑（本次 8051 即是）。强度枚举与 host `DriveStrength` 一一对应，无认知负担。

## 3. 决策结论（Decision）

### D1. ABI 原地加第三参 `strength`
```c
/* targets/wasm/wasm_bridge.h —— 中性枚举，数值与 host DriveStrength 对齐（恒等映射），
 * 不把 host TS 类型泄漏进固件。 */
typedef enum {
    WINK_DRIVE_WEAK   = 1,  /* 弱上拉/开漏释放：8051 锁存 1、I2C SDA 释放等 */
    WINK_DRIVE_PULL   = 2,  /* 电阻上/下拉（外部 4.7k 等） */
    WINK_DRIVE_SUPPLY = 3,  /* VCC/GND 直连或 push-pull 强驱动：esp32 输出、8051 锁存 0 */
} wink_drive_t;

extern void js_pal_gpio_write(uint16_t pin, bool level, uint8_t strength);
```
旧 `js_pal_gpio_write(pin, level)` 语义 ≡ 新函数 `(pin, level, WINK_DRIVE_SUPPLY)`——**严格超集平替**，不保留旧符号。

### D2. 各调用方传参
- **esp32 PAL**（`pal_wasm_ch1_gpio.c` 初始电平 + 运行写两处）：恒传 `WINK_DRIVE_SUPPLY`。push-pull 高/低皆强驱动，**电气结果与现状逐位一致**（仅意图显式化）。
- **8051 proxy 边沿分发**（`mcs51_proxy.hpp` 的 `WinkSbit::operator=` 与 `WinkSfr::operator=` 两处 diff 边沿）：`new_bit=1 → WINK_DRIVE_WEAK`，`new_bit=0 → WINK_DRIVE_SUPPLY`。proxy 无需区分输入/输出方向（准双向口规则对两者一致）。
- **8051 上电种子**（`mcs51_bridge.cpp mcs51_framework_init()`，`trap_reset()` 之后）：对 P0–P3 各引脚登记 **WEAK-HIGH**，**同时**把 `wink_mcs51_sfr_shadow[0x80/0x90/0xA0/0xB0] = 0xFF`（锁存器上电态）。此后固件标准 `Pn = 0xFF` 与影子同值 → diff=0、无边沿、不重复注册；输入脚处于 WEAK-HIGH 上拉态。

### D3. host 侧映射 + 版本偏斜兜底
- `imports.ts`：签名加 `strength: number`。
- `unisim-bridge-factory.ts`：`arbiter.setDriver(pin, { id:'mcu:gpio'+pin, state, strength: strength ?? DriveStrength.SUPPLY })`。`strength` 缺省（旧 wasm 配新 host）兜底 SUPPLY → esp32 行为不变；新 wasm 配旧 host 时 JS 忽略多余实参，强度丢失（mcs51 退回强驱动、旧 bug 复现）但**不崩**——偏斜只降保真不炸。

### D4. 仲裁自洽性核验（thermos 全场景）
- 按键 P3.2：锁存 1 → WEAK-HIGH 上拉；按下 button 插件 SUPPLY-LOW → SUPPLY 胜 → Read-Pin 读 LOW；释放 → 仅剩 WEAK-HIGH → HIGH。**标准 `P3=0xFF` 初始化即工作。**
- 继电器 P1.0 active-high：`heater=1` → WEAK-HIGH，无竞争 → 插件读 on=true（与真实 8051 高边弱驱动需外部三极管一致）；`heater=0` → SUPPLY-LOW。
- 低边 LED（亮=拉低）：SUPPLY-LOW，不变。
- WEAK vs SUPPLY 异态**不**触发 CONFLICT（不同强度），消除假仲裁失败。

## 4. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| 8051 准双向口电气语义首次正确建模；未修改 Keil 例程的标准 `Pn=0xFF` 初始化可工作，移除 thermos 「不写输入口」workaround | 跨仓 ABI 破坏性变更：embedded（C/JS glue/stub/测试）与 wink-ai（imports.ts/bridge）须**同一次原子改齐**，否则 emscripten 链接/实例化报错（wasm 从源码重建，无长期存活二进制，窗口小） |
| 强度进通用 ABI，未来 PDK 开漏、I2C 释放、外部上拉等复用同一词表，host 零特判 | host 须保留 `strength ?? SUPPLY` 兜底以容忍版本偏斜 |
| esp32 push-pull 路径电气零变化（显式 SUPPLY） | 高边 active-high 执行器在 8051 上报 WEAK-HIGH，依赖「无竞争读高」——与真实硬件一致，但断言/插件需知晓弱高语义 |
| Read-Pin 三路解析序（ADR-0074）与 RMW 只读锁存红线（ADR-0071）**不变**；本决策仅改「写边沿上报的强度」 | 上电 WEAK-HIGH 种子须与 shadow latch=0xFF 一同设置，避免固件初始化再产生 diff 边沿 |

## 5. 遵循与后续（Compliance & Follow-up）

1. 立实施计划（core，mcs51 Stage-3）：按 D1–D3 清单两仓原子改齐。
2. 回归门（全绿方可 Accepted）：
   - mcs51：5 个既有 headless carrier（uart_hello/uart_echo/analog_threshold/button_led/button_led_int）+ `mcs51_thermos` 两场景；mcs51 ctest host 23 + wasm/Node 10。
   - esp32：gpio 类应用（avoidance_car、dual_task_demo、devkitc_smoke 等）headless 重验——SUPPLY 路径应逐位不变。
   - wink-ai：unisim TS 测试套件（pin-arbiter / bridge / gpio domain）。
3. Accepted 后回写：`docs/design/02-wink-micro-os/07-mcs51-simulation-interception.md`（准双向口强度模型）；`mcs51_thermos/DESIGN.md` §8 第 5 项标记落地、移除「不写输入口」规避说明。
4. **ABI catalog SSOT 同步（wink-ai 仓，CI 门禁 `check:abi-catalog`，挂 `test:gates`）**：实施改签名后必须——
   - 编辑 `packages/unisim/scripts/abi-catalog/abi-catalog.yaml` 的 `js_pal_gpio_write` 条目：`signature` 改为 `void js_pal_gpio_write(uint16_t pin, bool level, uint8_t strength)`；`params` 增 `- { name: strength, type: uint8_t, desc: "1=WEAK 弱上拉/开漏释放, 2=PULL 电阻上下拉, 3=SUPPLY push-pull/强驱动（数值对齐 host DriveStrength；缺省=3）" }`（schema 无 enum 字段，枚举编码惯例写在参数 `desc`，同 `js_pal_gpio_read_state` 返回值先例）；`adr: ["0077"]`；更新 `c_header_line`/`ts_decl_line` 行号。
   - 运行 `bun run gen:abi-catalog`（=`python scripts/abi-catalog/generate-abi-catalog-md.py`）重生成 `hardware-channel-abi-catalog.md`，使 `--check` gate 转绿。
   - 注意：catalog 描述**已实现** ABI，**不得在代码落地前**先改 yaml（否则文档化不存在的签名）；此步与代码同 PR。
5. thermos 固件可在落地后恢复标准 `P3 = 0xFF` 初始化（验证 zero-intrusion 惯例可用）。

> 对照 [ADR-0065](../unisim/0065-uart-burst-idle-gap-framing.md)（UART 空闲分帧）：纯 host 内部分析器改动，**不变更任何 wasm import/export 符号签名**（`js_pal_uart_write` 保持 `(port,buf,len)`），故**无需**改 ABI catalog。

---

*本 ADR 状态变更请在此记录：*
- 2026-09-01：Proposed（由 `mcs51_thermos` 准双向口按键失效事件提炼；待评审）
- 2026-09-01：**Accepted**（D1–D3 两仓原子落地，实施计划 `2026-09-01-gpio-drive-strength-axis-plan.md`；Owner 批准开工）。落地内容与回归证据：
  - **embedded**：`wasm_bridge.h` 增 `wink_drive_t`（WEAK=1/PULL=2/SUPPLY=3）+ `js_pal_gpio_write(pin,level,strength)`；`pal_wasm_ch1_gpio.c` 两处（init 初始电平 + 运行写）显式传 `WINK_DRIVE_SUPPLY`；`wink_sim_js.js` 转发第三参；`mcs51_proxy.hpp` 声明 + `MCS51_DRIVE_WEAK/SUPPLY` 常量，sbit 与 whole-port 两处 diff 边沿按 `new_bit?WEAK:SUPPLY` 上报；`mcs51_uni_bridge.cpp` host fallback 记录强度（0→3 兜底）并新增测试观测口 `wink_mcs51_host_gpio_notify_strength`；`mcs51_bridge.cpp mcs51_framework_init()` 在 `trap_reset()` 之后**同时**置 P0–P3 shadow=0xFF 并对 32 脚登记 WEAK-HIGH 驱动（两者缺一不可：仅 shadow 则 host 无 MCU 驱动注册）。ABI hash 重算 → `PAL_WASM_ABI_HASH = 0x20149EFCu`。
  - **测试修复**：M3 遗留 `test_mcs51_gpio_host` 依赖「shadow 跨 framework-init 不重置」做按键注入，上电种子现在正确把 P3 复位为 0xFF——改为经 post-init hook（在复位种子之后）注入锁存电平，与 M4 外部注入测试同模式；`test_sfr_edge_dispatch_accuracy.cpp` 增 WEAK/SUPPLY 强度断言（上升沿=1、下降沿=3，whole-port 混合沿按位序）；emcc/Node 独立脚本补 `js_pal_gpio_write` 第三参及两个链接 stub（ultrasonic trigger、waveform edge）。
  - **wink-ai**：`imports.ts` 签名加 `strength?: number`；`unisim-bridge-factory.ts` 强度恒等映射 + 非 1/2/3 兜底 SUPPLY；abi-catalog SSOT `js_pal_gpio_write` 条目签名/strength `desc`/`adr:[ADR-0077]`/行号同步并重生成 catalog MD，`check:abi-catalog` 绿。
  - **thermos**：固件恢复标准 `P3 = 0xFF` 初始化（删除「不写输入口」workaround），两场景全绿——zero-intrusion 惯例验证通过。
  - **回归（全绿）**：mcs51 ctest **33/33**（host 23 + wasm/Node 10）；headless **5/5 既有 carrier + thermos 2/2**；esp32 SUPPLY 路径经 emcc/Node `test_pal_gpio_read_wasm_semantics` **9/9**（esp32 恒传 3 → host 映射 SUPPLY，电气逐位不变）；unisim `bun test src/` **164 pass / 32 fail（全为既有 manifest/sg90 SSOT 基线，与本次无关）**；`wink lint arch --pack layering --pack api` 无发现；`tsc --noEmit` 触碰文件零错误（151 行为既有基线）。
  - 回写：`docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md` 准双向口强度模型；`mcs51_thermos/DESIGN.md` §8 第 5 项标记落地。
