# 4.6 物理退化引擎与故障注入（WASM Wave 2 设计回写）

> **状态**：Accepted（活文档）  ·  **更新日期**：2026-06-29
>
> 本文档是 [ADR-0009](../../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md)
> 「物理特性模拟与故障注入架构」在 wasm 仿真目标（`targets/wasm`）的设计回写，
> 同时承接 [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md) 决策 3
> 「虚拟时钟」的 wasm 端落地。回写之后，本文件即为 wasm 端物理退化与虚拟时钟的
> 单一事实来源（SSOT）。
>
> **编号说明**：本文件按目录现有可用序号占用 `06-`。ADR-0009 Wave 2 计划中
> 原标注 `02-physical-degradation-engine.md` 的槽位已被 `02-virtual-peripheral-registry.md`
> 占用（属早期文档命名），故顺延至 `06-`，避免破坏现有交叉引用。

---

## 0. 适用范围与不在范围

**适用范围**：
- wasm 仿真目标的 OSAL/HAL 适配层（`wink-micro-os/targets/wasm/`）。
- target-无关算法库（`wink-micro-os/targets/common/src/wink_sim_physical.c`），由
  host PoC（Wave 1）与 wasm 沙箱共同复用。
- 浏览器侧 UniSim Worker 桥接（`@wink-ai/unisim`）。

**不在范围**：
- esp32 / baremetal 真机目标——**零编译污染**（ADR-0009 §4.3）：上述算法源文件
  与符号不得出现在真机 CMake 编译树或最终 ELF 中。检查方式：
  `grep -r "wink_sim_physical\|pal_wasm_physical" wink-micro-os/targets/esp32 wink-micro-os/targets/baremetal`
  必须无输出。
- 电气级 SPICE 仿真——仍由真机覆盖（见 ADR-0003 §决策 1 边界声明）。
- 多任务抢占调度（FreeRTOS 仿真）——仍属 ADR-0003 决策 3 路线项，转入
  [`05-simulation-consistency-and-fidelity-spec.md`](./05-simulation-consistency-and-fidelity-spec.md)。

---

## 1. 总体架构（双域混合 / Hybrid Double-Domain）

```text
 ┌─────────────────────────── UI 主线程 (Vue 3) ─────────────────────────┐
 │  画布交互、3D 场景、故障调参滑块                                       │
 └────────────────────────────────▲─────────────────────────────────────┘
                                  │ postMessage (UI ↔ Worker)
                                  ▼
 ┌────────────────────── UniSim Worker (TS) ─────────────────────────────┐
 │  VirtualClock(bigint)  ──────►  WasmPhysicalBridge  ─────► SimWorker  │
 │     │                                │                          │     │
 │     │  唯一时钟控制权                │ cwrap setters             │     │
 │     │                                ▼                          ▼     │
 │     └─────►  pal_wasm_advance_virtual_clock(us:bigint)   …      …     │
 └────────────────────────────────▲─────────────────────────────────────┘
                                  │ wasm exports / imports (bigint ABI)
                                  ▼
 ┌──────────────────── wasm 沙箱 (C, Emscripten) ────────────────────────┐
 │                                                                       │
 │  ┌─ pal_osal_wasm.c ───────────────────────────────────────────────┐  │
 │  │  s_virtual_us  ←—— pal_wasm_advance_virtual_clock(us)（唯一写入）│  │
 │  │  pal_get_us / pal_get_ms（纯读，零 JS 调用）                     │  │
 │  │  pal_delay_ms/us：仅 Asyncify 挂起，**禁止主动步进时钟**         │  │
 │  └───────────────────────────────────────────────────────────────────┘  │
 │                                                                       │
 │  ┌─ pal_wasm_physical.c ──────────────────────────────────────────┐    │
 │  │  faults POD（{0} == ideal）  +  PRNG state  +  per-pin ctx[128]│    │
 │  │  导出 setters：pal_wasm_set_{bounce_us,warmup_us,...}          │    │
 │  │  导出 reset：pal_wasm_reset_physical()                         │    │
 │  └────────────────────────────────────────────────────────────────┘    │
 │                                                                       │
 │  ┌─ pal_hal_wasm.c（GPIO/I2C 中间件层）───────────────────────────┐    │
 │  │  pal_gpio_read：抖动状态机透明叠加（DAL 不感知）              │    │
 │  │  pal_i2c_transfer：PRNG 命中阈值则返回 WINK_ERR_TIMEOUT       │    │
 │  └────────────────────────────────────────────────────────────────┘    │
 │                                                                       │
 │  ┌─ targets/common/src/wink_sim_physical.c（算法库 SSOT）────────┐    │
 │  │  抖动状态机 / RC 低通 + 高斯噪声 / 总线丢包判定 / 预热+采样限制│    │
 │  │  host PoC 与 wasm 共用此源，golden 向量字节级对齐             │    │
 │  └────────────────────────────────────────────────────────────────┘    │
 └───────────────────────────────────────────────────────────────────────┘
```

**双域职责切分**：
- **JS 域**：理想物理状态（按钮按下、距离 32cm、温度 25°C）+ 故障配置 + 时钟控制；
  通过消息协议下发，不进行任何微观时序模拟。
- **C/wasm 域**：信号退化、抖动、丢包、噪声等**就地（local）**算法处理；时钟读出
  纯本地内存，零跨边界开销。

---

## 2. 虚拟时钟 SSOT 架构（ADR-0003 决策 3 落地）

### 2.1 设计原则

| 原则 | 实现 |
|------|------|
| **单一真相** | `targets/wasm/pal_osal_wasm.c::s_virtual_us` (uint64_t) 是 wasm 侧时钟的唯一持有者，BSS 零初始化。 |
| **唯一写入入口** | `pal_wasm_advance_virtual_clock(uint64_t us)`，`EMSCRIPTEN_KEEPALIVE` 导出。 |
| **写入者唯一** | JS Worker（`SimWorker.STEP_CLOCK`）。任何其它路径（含 `pal_delay_ms/us`）**禁止**调用此函数。 |
| **读取入口** | `pal_get_us()` / `pal_get_ms()` —— 纯内存访问，零 JS round-trip。 |
| **类型契约** | uint64_t ↔ JS `bigint`，CMake `-s WASM_BIGINT=1`；TS 全链路 `bigint`，禁 `number` 隐式转换。 |
| **溢出保护** | uint64_t 自然回绕 > 580 年；仿真场景下不可达。 |

### 2.2 架构红线（静态可检）

> **不可违反**：`pal_delay_ms()` / `pal_delay_us()` 函数体**禁止**调用
> `pal_wasm_advance_virtual_clock()`。时钟推进的因果链是「JS Worker 在恢复 wasm
> 协程前先步进时钟 → wasm 恢复执行后看到新时间」，单向且唯一。如果 C 侧 delay
> 自己也步进，就出现「双重步进 / 因果倒置」，破坏可重放性。

**Grep 验证脚本**（推荐纳入 CI）：

```bash
# 提取 pal_delay_ms 与 pal_delay_us 函数体，断言无 advance 调用
awk '/^void pal_delay_(ms|us)/,/^}/' \
    wink-micro-os/targets/wasm/pal_osal_wasm.c \
  | grep "pal_wasm_advance_virtual_clock" \
  && { echo "SSOT violation"; exit 1; } || echo "SSOT clean"
```

### 2.3 JS 镜像（VirtualClock.ts）

JS 侧维护一份等价的 `bigint` 计数器，用于：
1. 调度（决定 `STEP_CLOCK` 的 `us` 量）。
2. 时间线回放 UI（避免高频跨 wasm 边界读时钟）。

JS 与 wasm 两个计数器**不直接对账**（wasm 侧仍是仲裁者），但因为所有写入都由
Worker 发起，自然保持一致。任意时刻强制同步可通过 `pal_wasm_reset_physical()` +
`VirtualClock.reset()` 完成。

---

## 3. 退化引擎（pal_wasm_physical.c）

### 3.1 全局状态布局（BSS）

```c
/* targets/wasm/pal_wasm_physical.c */
#define WASM_SIM_MAX_PINS 128        /* 覆盖 ESP32-S3(49) / Cortex-M(<100) */

static wink_sim_faults_t        s_faults;      /* {0} == ideal direct-pass */
static uint32_t                 s_prng;        /* 全局 PRNG（§4.2） */
static wink_sim_bounce_ctx_t    s_pin_ctx[WASM_SIM_MAX_PINS];
```

- **零动态内存**：所有状态走 BSS，无 `malloc`；初值依赖 C11 §6.7.9 p10 的零初始化保证。
- **`faults = {0}` 等价 ideal**：直通行为，即「不开启故障注入」的零开销默认状态，
  也是 §8 的内置降级方案。

### 3.2 导出 API（C → JS）

所有 setter 都通过 `EMSCRIPTEN_KEEPALIVE` 标注；JS 通过 `cwrap` 调用。

| 符号 | 参数 → JS 类型 | 用途 |
|------|---------------|------|
| `pal_wasm_advance_virtual_clock` | `uint64_t` ↔ `bigint` | 唯一时钟写入（§2） |
| `pal_wasm_set_bounce_us` | `uint32_t` ↔ `number` | GPIO 抖动持续时间 |
| `pal_wasm_set_warmup_us` | `uint32_t` ↔ `number` | 传感器预热延迟 |
| `pal_wasm_set_sample_interval_us` | `uint32_t` ↔ `number` | 最小采样间隔约束 |
| `pal_wasm_set_adc_noise_v` | `float` ↔ `number` | ADC 高斯噪声幅值 |
| `pal_wasm_set_rc_tau_s` | `float` ↔ `number` | RC 一阶滤波时常数 |
| `pal_wasm_set_i2c_drop_permil` | `uint16_t` ↔ `number` | I2C 丢包阈值（千分比） |
| `pal_wasm_set_prng_seed` | `uint32_t` ↔ `number` | PRNG 种子（确定性入口） |
| `pal_wasm_get_prng_state` | 返回 `uint32_t` | 当前 PRNG 状态（回归断言用） |
| `pal_wasm_reset_physical` | — | 重置 faults / PRNG / per-pin ctx |

### 3.3 内存安全：WASM_SIM_MAX_PINS 边界检查

所有 per-pin 访问入口（HAL 中间件 `pal_gpio_read` 与 setter）必须：

```c
if ((unsigned)pin >= WASM_SIM_MAX_PINS) {
    /* 越界：HAL 视为「该 pin 无退化」（透传）；setter 视为空操作 */
    return /* NULL or default */;
}
```

意图：恶意或调试期 JS 传入越界 pin 时，**不发生 BSS OOB 写**，且仿真行为可观测
（透传等价于「故障未生效」，比静默崩溃更利于排查）。

---

## 4. 故障注入分层（强制纪律，复述 ADR-0009 §3.0）

| 层级 | 处理位置 | 故障类型 | 对上层透明？ |
|------|---------|---------|-------------|
| **L1 PinManager 中间件** | `pal_hal_wasm.c` | GPIO 断线、抖动、上下拉失效、高阻 | ✅ |
| **L2 总线控制器中间件** | `pal_hal_wasm.c::pal_i2c_transfer` 等 | I2C ACK 丢失、SPI 位翻转、总线超时 | ✅ |
| **L3 外设驱动桩（业务层）** | `dal/*_sim.c` | 传感器超量程、电机堵转、EEPROM 坏块 | ❌（显式） |

**禁止反模式**：
- ❌ DAL 驱动 `attachEvents/read/write` 内手动模拟断线。
- ❌ 每个外设各自实现抖动 / 噪声 / 丢包。
- ❌ DAL 调用 `pinManager.setDriverLevel()` 模拟故障。

### 4.1 抖动模型（采样强制翻转）

> **设计修订（Wave 1 验证）**：ADR-0009 §3.1 骨架的 `(now/1000)%2` 模型在
> `WINK_RUNTIME_TICK_MS=10` 默认采样周期下会静默失效（商每 tick 增 10，偶数恒定）。
> 已正式改为「每次采样强制翻转」(`bounce_flip ^= 1`)：采样周期无关、最严苛抖动、
> 100% 确定。RC 噪声与总线丢包仍走 PRNG。

### 4.2 PRNG 全局设计（有意为之）

PRNG 单全局实例 `s_prng` 是**架构选择而非缺陷**：
- ADR-0009 §4.1 「单 seed 100% 复现」契约要求：种子→完整轨迹是一对一映射。
- 若每个外设独立 PRNG，则种子空间爆炸、复现成本陡增。
- 未来若有外设级独立需求，再演化为「派生子流（seed = hash(global_seed, peripheral_id)）」。

---

## 5. 跨语言契约（JS ↔ wasm）

### 5.1 BigInt ABI

CMake 链接必须开启 `-s WASM_BIGINT=1`，否则：
- C 侧 `uint64_t` 参数 / 返回值会被截断为两个 `i32`。
- JS 侧若误传 `number` 给 `bigint` 导出函数，Emscripten 抛 `TypeError`，
  作为类型不匹配的**运行期防线**（与 TS 编译期类型检查互为双重保险）。

### 5.2 Worker 消息协议（SimWorker.ts）

| Request | 字段 | wasm 调用 |
|---------|------|----------|
| `INIT` | — | 绑定 Module + 重置 `VirtualClock` + `pal_wasm_reset_physical` |
| `SET_FAULTS` | `faults: SimFaultsConfig` | 批量调用所有 `pal_wasm_set_*` setter |
| `STEP_CLOCK` | `us: bigint` | `pal_wasm_advance_virtual_clock(us)` |
| `SET_GPIO_IDEAL` | `pin, level` | 写 wasm 侧 ideal pin level（HAL 读时叠加退化） |
| `READ_GPIO_DEGRADED` | `pin` | `pal_gpio_read(pin)`（已含抖动） |
| `TEST_I2C_TRANSFER` | `port, devAddr, writeBuf, readLen` | `pal_i2c_transfer()`（带丢包） |

每条消息携带 `id: number` 用于关联响应（前端 `await` 单条 round-trip）。

---

## 6. 测试矩阵与 SSOT 静态断言

| 层 | 用例 | 工具 |
|----|------|------|
| L0 编译 | wasm / host / esp32 / baremetal 四目标编译通过，TS `tsc --noEmit` 零警告 | CMake + Ninja + tsc |
| L0.5 静态架构 | **`pal_delay_ms` 体内不调 `pal_wasm_advance_virtual_clock`**（§2.2 grep）；`-s WASM_BIGINT=1` 出现在链接选项；`WASM_SIM_MAX_PINS` 边界检查存在 | grep 脚本 |
| L1 单测（C） | 算法库 golden 向量（`test/common/test_physical_golden.h`） host 与 wasm 双端字节一致；虚拟时钟单调性；故障 setter 回环；pin 边界 0 / 127 / 128 / UINT16_MAX | Unity（host + wasm Node 运行时） |
| L1 单测（TS） | `VirtualClock` bigint 边界与负值拒绝；`WasmPhysicalBridge` setter 调用次序；`SimWorker` 消息分派 | Jest |
| L2 集成 | 按键消抖端到端（`test_button_debounce_e2e_wasm.c` ↔ host `test_button_debounce_e2e.c` 字节级一致） | Unity |
| L3 确定性 | 同 seed 同输入 → 字节级输出；连续 1000 次零偏差 | 自定义脚本 |

---

## 7. 零编译污染验证（持续）

```bash
# 真机目录不得引用退化引擎
grep -r "wink_sim_physical\|pal_wasm_physical" \
    wink-micro-os/targets/esp32 \
    wink-micro-os/targets/baremetal
# 期望：无输出
```

算法库 `targets/common/src/wink_sim_physical.c` 仅被 `pal_host` 与 `pal_wasm` 两个
CMake OBJECT 库枚举编译；esp32 / baremetal 的 CMakeLists 显式列出源文件（非 glob），
不可能误入。

---

## 8. 降级与回滚

1. **运行期降级**：`SET_FAULTS` 下发全零配置（`SimFaultsConfig` 等价 `{0}`）→ 所有
   退化算法在阈值判断后旁路，等同直通。无需重编。
2. **Git 回退**：`git revert` Wave 2 全部 commit → 回 baseline；算法库源文件保留
   （host 试点已采纳），但 wasm 端 setter / 中间件消失，行为回到 Wave 1 之前。
3. **编译期裁剪**：从 `pal_wasm` CMake 移除 `pal_wasm_physical.c`，并还原
   `pal_gpio_read` / `pal_i2c_transfer` 中的退化叠加 → 退化路径完全消失。

---

## 9. 参考链接

- [ADR-0009](../../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md)（双域混合架构、§4.1 确定性守卫、§4.3 零编译污染）
- [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)（决策 3：虚拟时钟落地）
- [ADR-0002](../../../decisions/unisim/0002-dual-target-compilation.md)（双 target 同源编译，wasm32 / xtensa 兼容）
- [Wave 1 计划](../../../implementation-plans/unisim/2026-06-28-adr-0009-host-pilot-physical-sim-wave1-plan.md)
- [Wave 2 计划](../../../implementation-plans/unisim/2026-06-29-adr-0009-wasm-physical-sim-wave2-plan.md)
- 源代码入口：
  - C：`targets/wasm/pal_osal_wasm.c`、`pal_wasm_physical.c`、`pal_hal_wasm.c`、`wasm_bridge.h`
  - 算法库 SSOT：`targets/common/src/wink_sim_physical.c`
  - TS：`@wink-ai/unisim` (VirtualClock, WasmPhysicalBridge, SimWorker)

