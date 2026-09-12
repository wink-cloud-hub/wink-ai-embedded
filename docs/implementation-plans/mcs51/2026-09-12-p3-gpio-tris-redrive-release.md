# P3：GPIO TRIS 方向切换重驱/释放与自驱回读消歧实施计划

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260912-MCS51-P3-TRIS` |
| **创建日期** | `2026-09-12` |
| **目标平台/SoC** | `host` / `wasm`（mcs51 仿真；`frameworks/mcs51` 在 ESP_PLATFORM 直接 return） |
| **工具链/SDK版本** | host `GCC/MinGW C++17` / `Emscripten`；真机对照 `Keil C51`（仅文档） |
| **计划状态** | `P3-A 完成（2026-09-12）；P3-B 条件项未触发（暂不排期）` |
| **优先级** | 🟡 P2（不阻塞主链路；解锁 vendor EOC 两场景 + 模型保真收尾） |
| **计划版本** | `v1.0` |
| **关联技术设计** | 无（本计划即 Layer-③；Phase B2 若实施需修订 ADR-0077） |
| **关联设计规范** | `docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md` §2.6（ADR-0077 落地节） |
| **关联评审记录** | Stage7 附录 E（`docs/implementation-plans/mcs51/2026-09-11-mcs51-decoupling/stage7-test-contract-close.md:116-122`） |
| **关联 ADR** | ADR-0077（准双向口驱动强度轴）、ADR-0074（Read-Pin 外部缝）、ADR-0071（SFR 代理数据面）；Phase B2 需修订 ADR-0077 |
| **目标里程碑** | 关闭 Stage7 附录 E 受控遗留；`vendor_cms8s78xx_v202/adc_ldo`、`adc_hardware_trigger` 的 EOC 波形断言复绿 |
| **前置依赖计划** | `PLAN-20260911-MCS51-S7`（链接期自注册与测试契约收尾，已完成） |
| **替代/废弃** | 无 |
| **计划负责人** | （待定） |
| **所需子代理技能** | `embedded-best-practice` |

---

## 2. 背景与目标（🔴 必选）

### 2.1 问题陈述

Stage7 附录 E 归档：A-05 方向模型（stage3 `0f3e426` 引入）下，`GPIO_ENABLE_OUTPUT(P3TRIS,2)` 之前对 P3.2 的锁存写因输入方向被抑制；方向切换到输出时模型不按锁存重驱，ISR `P32=~P32` 的回读命中 bridge 上电 WEAK-HIGH 种子，`~1=0` 与锁存同值 → `mcs51_gpio_bit_write` 短路 → 无输出边沿，EOC 波形频率为 0。

代码核对后确认两个降本事实：

1. **PxTRIS 是 SFR（0x9A/0xA1/0xA2/0xA3，`REG_CMS8S78XX.H:50-53`）**，整字节/复合写经 `WinkSfr::operator=` → `wink_mcs51_on_sfr_write` → `sfr_write_hooks[addr]`（`mcs51_proxy.hpp:195-206`、`mcs51_bridge.cpp:105-120`）。可直接复用现有 `mcs51_trap_register_sfr_write`（`mcs51_sfr.cpp:39-42`），**无需新增 core hook、不动 `Mcu51Context` 布局/预算、无 ABI 变更、无需新 ADR**（Phase A）。
2. **释放语义已有现成 C-ABI**：`js_pal_gpio_release_mcu(pin)`（`targets/wasm/wasm_bridge.h:84`，生产 `wink_sim_js.js:97` 已接）；缺 host fallback（`mcs51_uni_bridge.cpp`）与 node 测试桩（`mcs51_wasm_node_stub.js`）。

### 2.2 两个约束

1. **底层收窄（AGENTS Bypass 规则）**：修复落在 chip package 的 per-context SFR hook 上，通用 GPIO 核心路径零改动、classic 家族零行为变化。
2. **仿真隔离语义**：host 无 PinArbiter，`release_mcu` 只能记录日志供测试断言；真实释放只在 wasm 生产 + 跨仓 headless 验证。

### 2.3 技术目标

- **P3-A（必做）**：TRIS 方向切换按硅片语义重驱/释放（含附录未提的对称缺口：输出→输入释放、OD/AN 交互），解锁 EOC 场景。
- **P3-B（条件项）**：自驱回读消歧。仅当 A 验收后仍存在"bridge 上电 WEAK-HIGH 种子被当外部电平"的真实失真时排期；B2 改种子语义需修订 ADR-0077。

### 2.4 成功指标（验收出口）

| 指标 | 通过标准 | 验证方法 |
|------|----------|----------|
| 主机单元测试 | mcs51 host 全量绿（相对既有基线无新增失败），含 T8~T11 新用例 | `ctest -R mcs51` |
| wasm/Node | `wasm_mcs51_gpio_test`、`wasm_mcs51_cms8s_adc_test` 重建后绿 | `ctest -R wasm_mcs51` |
| 跨仓 headless | `vendor_cms8s78xx_v202/adc_ldo`、`adc_hardware_trigger` 的 `ASSERT_WAVEFORM` 频率落 [1k,50k]；五载体 + health_pot + analog_threshold 无回归 | `wink build wasm` + headless runner |
| 文档 | Stage7 附录 E 关闭；GAP-08 状态更新；设计规范 §2.6 回写 | 文档 diff |

---

## 3. 变更范围与影响分析（🔴 必选）

### 3.1 文件变更清单

| 文件路径 | 变更类型 | 说明 |
|----------|----------|------|
| `frameworks/mcs51/chips/cms8s78xx/src/cms8s_gpio.cpp` | ✏️ 修改 | TRIS 写 hook 注册 + 重驱/释放/OD/AN 判定；reset 释放全引脚 |
| `frameworks/mcs51/include/wink_mcs51_gpio.h` | ✏️ 修改 | 声明 `js_pal_gpio_release_mcu`（数据面缝） |
| `frameworks/mcs51/src/mcs51_uni_bridge.cpp` | ✏️ 修改 | host fallback `js_pal_gpio_release_mcu` + release 观测 + reset |
| `frameworks/mcs51/test/wasm/mcs51_wasm_node_stub.js` | ✏️ 修改 | node 测试桩补 `js_pal_gpio_release_mcu` no-op |
| `frameworks/mcs51/test/cms8s78xx/test_mcs51_gpio_dir.cpp` | ✏️ 修改 | T2 改走代理缝 + T8~T11 新用例 |
| `docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md` | ✏️ 修改 | §2.6 补方向切换语义 |
| `docs/todolist/2026-09-10-mcs51-sim-vs-silicon-gap-todolist.md` | ✏️ 修改 | GAP-08 状态收尾 |
| `docs/implementation-plans/mcs51/2026-09-11-mcs51-decoupling/stage7-test-contract-close.md` | ✏️ 修改 | 附录 E 标记关闭（保留根因记录） |

### 3.2 影响与兼容性

- **classic 家族**：不注册 TRIS hook、不释放，行为零变化（S4-1 家族绝缘契约）。
- **CMS8S 场景初始状态**：`cms8s_gpio_reset` 释放全 32 脚（硅片复位=输入/HiZ），但 bridge 上电种子在 `mcs51_context_reset` 之后执行（`mcs51_bridge.cpp:44-50`），framework init 淨结果不变（仍 WEAK-HIGH）；仅影响运行中复位。
- **上下文 RAM**：无结构体变化，`test_mcs51_context_budget` 不受影响。
- **现有 host 计数断言**：TRIS 经代理的旧路径不再直接 poke shadow 的用例（T2）需同步预期；其余直接 poke 用例（T3/T5）不变。

---

## 4. 任务拆分

### Task 1：TRIS 写 hook 与重驱/释放（P3-A）`[ 状态: ✅ 完成 ]`

- [x] **Step 1**：`cms8s_gpio.cpp` 新增 `port_from_tris_addr`、`drive_latch`、`release_pin`、`on_tris_write`；按 `old^new` 逐位：
  - 0→1 且非 AN：latch=0 → `write(pin,false,SUPPLY)`；latch=1 → OD 则 `release_mcu`，否则 `write(pin,true,WEAK)`；CFG=AN → 不驱动。
  - 1→0：`release_mcu`。
- [x] **Step 2**：init/reset 安装 4 个 TRIS hook（幂等，S4-H2 契约）；reset 先释放全 32 脚再装 hook。
- [x] **Step 3**：host fallback 与 node 桩补齐 release 缝 + 观测 API（`wink_mcs51_host_gpio_release_count/pin/reset`）。

### Task 2：测试（P3-A）

- [x] **Step 1**：T2 改走 `wink_mcs51_on_sfr_write(0xA2, old, new)`（含 shadow 预写），预期加入重驱 notify。
- [x] **Step 2**：T8 输入态 latch=0 抑制 → TRIS 0→1 重驱（pin/level/strength 断言）。
- [x] **Step 3**：T9 TRIS 1→0 释放（release 计数 + 引脚）。
- [x] **Step 4**：T10 OD=1 且 latch=1 → 输出使能走 release；AN 脚使能不驱动。
- [x] **Step 5**：T11 classic 经同一代理路径零通知/零释放。

### Task 3：验证与跨仓收尾

- [x] **Step 1**：host `ctest -R mcs51 -E wasm`，对比基线（仅 3 项既有失败）。
- [x] **Step 2**：wasm `gpio/cms8s_adc/test/timer0` 重建后跑 Node 测试 4/4。
- [x] **Step 3**：重建 `vendor_cms8s78xx_v202/{adc_ldo,adc_hardware_trigger}` unisim-assets 并跑 headless（均 PASS）；五载体 5/5 + `mcs51_health_pot` 15/15 无回归。
- [x] **Step 4**：文档回写（设计 §2.6、GAP-08、附录 E 关闭）。

### Task 4：P3-B 评估（条件项，A 验收后触发）

- 触发条件：A 后仍存在种子被当外部电平的失真（输入脚读 1 掩盖外部 HiZ；extint/capture 把自驱切换当外部边沿）。
- B1（模型层，无 ABI）：read-pin 输出态优先返回模型驱动值、输入态忽略自驱种子；评估 `Mcs51GpioHooks` 扩展与 context 预算。
- B2（bridge 层）：CMS8S 复位种子由 `write(WEAK)` 改 `release_mcu`（classic 保留）；需修订 ADR-0077 + 全场景回归。

---

## 5. 风险与回退

| 风险 | 等级 | 缓解 |
|------|------|------|
| SFR hook 仅在代理路径生效，测试直接 poke shadow 绕过 | 中 | 新用例统一走 `wink_mcs51_on_sfr_write`/WinkSfr 代理 |
| `mcs51_trap_reset()` 清 hook，模型 reset 未重装 | 中 | 遵循 cms8s_sys `install_dispatch` 的 reset-rebuilds-registration 契约 |
| host 无 arbiter，释放语义无法端到端验证 | 中 | 观测计数 + 跨仓 headless 真值 |
| 重驱新增 notify 影响既有波形/计数断言 | 低 | 只影响配置时刻，功能稳态不变；跑全场景回归 |
| 运行中复位释放影响他实例 | 低 | 仿真单 MCU；release 为幂等 no-op（无驱动时） |
| 回退 | — | 删除 TRIS hook 注册即回退到 A-05 现状，无数据/ABI 残留 |

---

## 6. 文档回写

- `07-mcs51-simulation-interception.md` §2.6：补"方向切换重驱/释放"语义与 release 缝。
- `2026-09-10-mcs51-sim-vs-silicon-gap-todolist.md` GAP-08：状态从"部分"更新为方向切换子项完成。
- Stage7 附录 E：标记关闭，保留根因与修复证据。

---

## 7. 变更记录

| 版本 | 日期 | 变更 | 作者 |
|------|------|------|------|
| v1.0 | 2026-09-12 | 初始版本：P3-A 任务拆分与验收；P3-B 条件项 | — |
| v1.1 | 2026-09-12 | P3-A 关闭：实现、host/wasm/headless 证据与文档回写完成 | — |

---

## 8. 执行证据（P3-A，2026-09-12）

| 项目 | 结果 |
|------|------|
| host 单测 | `test_mcs51_gpio_dir` T1-T11 全 PASS；`ctest -R mcs51 -E wasm` 58/61（3 项失败在净化 HEAD 同样复现，为既有问题，与 P3 无关） |
| wasm/Node | `wasm_mcs51_test/timer0/gpio/cms8s_adc` **4/4 PASS**（重建后） |
| 跨仓 headless 五载体 | `mcs51_uart_hello`/`uart_echo`/`analog_threshold`/`button_led_int`/`button_led` **5/5 PASS** |
| `mcs51_health_pot` | **15/15 PASS** |
| vendor EOC（附录 E 遗留） | `adc_ldo` `ASSERT_WAVEFORM` **PASS**（修复前频率 0）；`adc_hardware_trigger` 13 步 **PASS** |
| 资产刷新 | 五载体 + `mcs51_analog_threshold` + `mcs51_health_pot` + vendor 两 app 的 `unisim-assets` 随生产 wasm 重建 |
| 架构门禁 | ctest `test_mcs51_layering_gate` PASS；外部 `wink lint --changed` 无发现 |

**P3-B 裁决**：A 后未再观察到"上电种子被当外部电平"的真实失真（EOC 波形与全部场景断言均绿），自驱回读消歧暂不排期；若后续出现输入脚读到自驱弱高掩盖外部 HiZ 的场景，再按 B1（模型层）评估。
