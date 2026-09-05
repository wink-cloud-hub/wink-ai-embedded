# 实施计划：通道-1 GPIO 写增加「驱动强度」轴（ADR-0077）

| 项 | 内容 |
|---|---|
| 创建日期 | 2026-09-01 |
| 关联 ADR | [ADR-0077](../../decisions/core/0077-gpio-write-drive-strength-axis.md)（Accepted） |
| 关联设计规范 | `docs/design/02-wink-micro-os/07-mcs51-simulation-interception.md`（准双向口强度模型，Accepted 后回写） |
| 关联实施计划 | `2026-08-30-mcs51-stage2-live-channels-plan.md`（Stage-2 四通道，本计划为 Stage-3 首项） |
| 变更性质 | **跨仓 ABI 破坏性变更**（两仓原子改齐；wasm 由 CLI 从源码重建，无长期存活预编译产物） |

## 1. 目标

把数字输出的「驱动强度」纳入通用 Wasm ABI：`js_pal_gpio_write(pin, level, strength)`。
- esp32（push-pull）显式传 `SUPPLY`，电气结果逐位不变。
- 8051（准双向口）proxy 边沿上报 `锁存1→WEAK 弱上拉 / 锁存0→SUPPLY 强低`，并在上电时对 P0–P3 种 WEAK-HIGH + shadow latch=0xFF。
- host PinArbiter 已有三级强度模型（WEAK=1/PULL=2/SUPPLY=3），数值恒等映射；`strength ?? SUPPLY` 兜底版本偏斜。

落地后移除 `mcs51_thermos` 的「不写输入口 P3」workaround，恢复标准 `P3 = 0xFF` 初始化（zero-intrusion 惯例验证）。

## 2. 触点清单

### 仓 A：wink-ai-embedded

| 文件 | 改动 |
|---|---|
| `wink-micro-os/targets/wasm/wasm_bridge.h` | CH1 加 `wink_drive_t` 枚举（WEAK=1/PULL=2/SUPPLY=3）；`js_pal_gpio_write` 加第三参 `uint8_t strength` |
| `wink-micro-os/targets/wasm/pal_wasm_ch1_gpio.c` | L85（init_output 初始电平）、L124（运行写）两处传 `WINK_DRIVE_SUPPLY` |
| `wink-micro-os/targets/wasm/pal_wasm_degradation.c` | `PAL_WASM_ABI_HASH` 由 `tools/update_wasm_abi_hash.py` 重算 |
| `wink-micro-os/targets/wasm/wink_sim_js.js` | L76 import 签名加 `strength`，转发 `Module['js_pal_gpio_write'](pin, level, strength)` |
| `wink-micro-os/frameworks/mcs51/include/mcs51_proxy.hpp` | L49 声明加第三参；L112（WinkSbit 边沿）、L203（WinkSfr 边沿）按 `new_bit` 传 WEAK/SUPPLY |
| `wink-micro-os/frameworks/mcs51/src/mcs51_uni_bridge.cpp` | host fallback `js_pal_gpio_write` 加第三参（记录 strength，供测试观测） |
| `wink-micro-os/frameworks/mcs51/src/mcs51_bridge.cpp` | `mcs51_framework_init()` 在 `mcs51_trap_reset()` 后：P0–P3 shadow latch=0xFF + 对 32 脚登记 WEAK-HIGH |
| `wink-micro-os/test/wasm/test_pal_gpio_read_wasm_semantics.c` | L100 EM_JS 签名加第三参 |
| `wink-micro-os/test/mcs51/wasm/mcs51_wasm_node_stub.js` | L31 stub 加第三参 |
| `wink-micro-os/test/mcs51/unit/test_sfr_edge_dispatch_accuracy.cpp` | 新增强度断言：上升沿 WEAK、下降沿 SUPPLY（经 host fallback 观测） |
| `wink-micro-app/mcs51_thermos/thermos.c` | 恢复标准 `P3 = 0xFF`（删 workaround 注释） |
| `wink-micro-app/mcs51_thermos/DESIGN.md` | §8 第 5 项标记落地、移除「不写输入口」规避说明 |

### 仓 B：wink-ai

| 文件 | 改动 |
|---|---|
| `packages/unisim/src/types/wasm/imports.ts` | L6 `js_pal_gpio_write(pin, level, strength: number)` |
| `packages/unisim/src/core/bridge/unisim-bridge-factory.ts` | L208 setDriver 用 `strength ?? DriveStrength.SUPPLY` 映射（1/2/3 恒等） |
| `packages/unisim/scripts/abi-catalog/abi-catalog.yaml` | `js_pal_gpio_write` signature + strength 参数 desc（枚举编码入 desc）+ `adr: ["0077"]` + 行号 |
| `packages/unisim/docs/architecture/hardware-channel-abi-catalog.md` | 由 `bun run gen:abi-catalog` 重生成（`check:abi-catalog` gate） |

## 3. 执行顺序（两仓原子，本地一次性改齐后再分别提交）

1. **embedded ABI 头 + esp32 PAL**：wasm_bridge.h 枚举+签名；pal_wasm_ch1_gpio.c 两处 SUPPLY；wink_sim_js.js 转发。
2. **embedded mcs51**：mcs51_proxy.hpp 边沿强度；mcs51_uni_bridge.cpp host fallback 签名+strength 记录；mcs51_bridge.cpp 上电种子。
3. **embedded 测试**：EM_JS / node stub 签名；test_sfr_edge_dispatch_accuracy 加强度断言。
4. **重算 ABI hash**：`python wink-micro-os/tools/update_wasm_abi_hash.py`。
5. **wink-ai host**：imports.ts + bridge factory 强度映射。
6. **wink-ai abi-catalog**：改 yaml → `bun run gen:abi-catalog` → `check:abi-catalog` 转绿。
7. **thermos 去 workaround**：恢复 `P3 = 0xFF`、更新 DESIGN。
8. **全量回归**（见 §4）。
9. **ADR-0077 置 Accepted + 回写**设计规范 `07-mcs51-simulation-interception.md`。
10. **两仓分别原子提交**（embedded 先 / wink-ai 后，或同 PR）。

## 4. 验收 / 回归门（全绿方可收工）

- **embedded 固件侧**：mcs51 ctest host 23 + wasm/Node 10（含新强度断言）；`wink lint arch` 干净。
- **mcs51 headless（6 carrier）**：uart_hello / uart_echo / analog_threshold / button_led / button_led_int + **thermos 两场景**（恢复 `P3=0xFF` 后按键必须仍可读、heater/LED/遥测全对）。
- **esp32 headless 回归**：gpio 类应用（avoidance_car、dual_task_demo、devkitc_smoke 等）SUPPLY 路径逐位不变。
- **wink-ai**：unisim TS 测试套件无新增失败（对照既有基线 manifest-SSOT/pdk 环境失败）；`check:abi-catalog` 绿；改动文件 tsc 零错误。

## 5. 风险与约束

- **跨仓原子窗口**：签名不匹配会导致 emscripten 链接/实例化报错。wasm 从源码重建，窗口仅在本地两仓工作树之间；改齐前勿单独提交一仓。
- **版本偏斜**：host 保留 `strength ?? SUPPLY`（旧 wasm→esp32 行为不变）；新 wasm 配旧 host 时 JS 忽略多余实参，强度丢失但不崩（mcs51 退回强驱动、旧 bug 复现）——只降保真不炸。
- **上电种子顺序**：WEAK-HIGH 登记必须与 shadow latch=0xFF 同步，否则固件 `Pn=0xFF` 再产生 diff 边沿。种子置于 `mcs51_trap_reset()` 之后、内部 hook 注册之后。
- **高边 active-high 执行器**（继电器 P1.0）：8051 上报 WEAK-HIGH，依赖「无竞争读高」——与真实硬件一致（高边弱驱动需外部三极管）；断言/插件需知晓弱高语义。
