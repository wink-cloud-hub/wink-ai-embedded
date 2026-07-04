# wink-micro-os 测试体系

> 本文件是 wink-micro-os host 测试的一站式入口。所有测试均在 PC 上运行，**不需要真实硬件或浏览器**。

---

## 🚀 快速命令（复制即用）

| 命令 | 用途 | 用时 | 频率 |
|------|------|------|------|
| `pwsh ./run-tests.ps1` | **日常开发门禁**：增量构建 + 跑全部 35 个 GCC 测试 | 5s - 2min | ✅ 每次提交 |
| `pwsh ./run-tests.ps1 -Clean` | 完全重建 + 跑全部测试 | 1 - 3min | 🔧 改了 CMake / 怀疑缓存污染 |
| `pwsh ./run-tests.ps1 -Detailed` | 跑测试 + 打印每个用例完整输出 | 2 - 5min | 🐛 排查失败用例 |
| **MSVC 链验证** | Visual Studio x64 Native Tools Command Prompt 中运行：<br>`cmake -G Ninja -DCMAKE_C_COMPILER=cl -DTARGET_PLATFORM=host -B build-msvc`<br>`cmake --build build-msvc`<br>`ctest --test-dir build-msvc --output-on-failure` | 2 - 5min | 📅 PR 合并前 / 重大变更 |

**当前测试规模**（2026-07-04 快照）：**35 个可执行**，覆盖 PAL 契约 / DAL 外设 / runtime / trace / 协作式调度器 / 6 个 sample 端到端。

---

## 🧩 测试体系概述

### 双链的意义

wink-micro-os 采用 **GCC + MSVC 双链验证**：

```
           ┌──────────────────────────────────────────────────┐
           │         同一套 35 个测试用例，两个编译器          │
           └───────────────────┬──────────────────────────────┘
                               │
           ┌───────────────────┴──────────────────────────────┐
           │                                                  │
     GCC 链 (MinGW)                                    MSVC 链 (cl.exe)
           │                                                  │
  日常开发快速反馈                          最严格的 C99 合规性检查
  -Wall -Wextra -Werror                           /W4 /WX /wd4100 /wd4210
  run-tests.ps1 封装                              手动跑 / CI 跑
```

- **GCC 链 = 日常门禁**：快、增量、对 Windows 开发环境友好
- **MSVC 链 = 交叉编译器兼容**：确保代码是**纯 C99**，不依赖任何 GCC 特有扩展
- 两条链跑的是**完全相同的测试用例**，只是编译器不同

### wasm target 说明

`targets/wasm/` 编译为 WebAssembly，供浏览器/Node worker 加载。其 host 端有 **Node 侧 stub 烟测**（`node targets/wasm/wink_sim_stub.js`）作**编译期契约门禁**——静态解析 wasm imports 与 `wasm_bridge.h` SSOT 交叉核验。真正的 wasm 行为验证由前端 workbench 仓的 e2e 承担；本仓 host 测试矩阵不再依赖浏览器。

---

## 📊 测试梯队详解

按**层次+价值密度**组织。**总计 35 个可执行**（29 个单测 + 6 个 sample e2e）。

---

### 🔴 Tier 1 · Core / 门禁级（失败绝不能提交）

覆盖 PAL 契约、runtime 主循环、trace、故障注册表。这些是整个 OS 的骨架，任何回归都会全局连锁。

| 测试 | 作用 |
|------|------|
| **`test_smoke`** | 编译链 smoke test（`WINK_OK`/`WINK_ERR_INVALID_ARG` 存在且语义正确）。它若失败说明构建链本身坏了，不是代码问题。 |
| **`test_pal_contract`** | PAL 契约完整性编译探针。对跨 target 使用的枚举/宏做**编译期断言**（如 `WINK_ERR_HARDWARE == -12`、`WINK_MUTEX_WAIT_FOREVER == 0xFFFFFFFF`）。任何符号缺失/漂移 → 编译失败立即拦截。 |
| **`test_pal_irq`** | ADR-0018 三级优先级 IRQ 契约（LOW/MEDIUM/HIGH）+ 单一临界区宏。 |
| **`test_pal_resource`** | 资源占用治理（GPIO/PWM/I2C 引脚计数、超出最大实例数拒绝分配、泄漏检测）。防 AI 生成代码无脑创建新实例。 |
| **`test_pal_resource_wire`** | Wire 级 pal_resource 使用规范（DAL/PAL 之间 claim/release 时序契约）。 |
| **`test_pal_pwm_router`** | PWM 通道分配状态机（分配/回收、防冲突、溢出保护、失败降级）。 |
| **`test_pal_storage`** | PAL 存储抽象（host 内存实现，ADR-0008 Flash 存储层的 host 对等）：越界读返回 0、越界写截断、erase 清零。 |
| **`test_host_pal`** | host target 端 PAL HAL/OSAL smoke（`pal_delay_ms/us`、`pal_gpio_read/write`、`pal_pwm_set`、mutex/semaphore）。 |
| **`test_runtime`** | 主循环 + 软定时器：注册回调 → 跑 N tick → fault 上报，`wink_runtime_init/run`、软定时器注册/触发/取消、回调优先级。 |
| **`test_trace`** | Golden Trace 环形缓冲（满则覆盖）+ 故障码序列化，故障诊断基础设施。 |
| **`test_wink_trace_isr_equivalence`** | ISR 上下文 vs 任务上下文写入 trace 环形缓冲的等价性（无锁竞态守卫）。 |
| **`test_actuator_registry`** | 执行器关断注册表：注册/查询、故障按优先级关断、最大安全状态回调。ADR-0008 派发基础设施。 |

---

### 🟠 Tier 2 · DAL 外设驱动

覆盖具体器件驱动。改了对应 DAL 后**首先**跑这一梯队。

| 测试 | 作用 |
|------|------|
| **`test_dal_servo`** | 舵机驱动 + ADR-0008 覆写：角度 clamp、脉宽计算、覆写 `min_pulse_ms`/`max_pulse_ms`。 |
| **`test_dal_ultrasonic`** | 超声波驱动 + 覆写：距离计算超时保护、覆写 `trig_pin`/`echo_pin`。ADR-0017 后仅暴露非阻塞 API。 |
| **`test_dal_ultrasonic_sim`** | 超声波仿真分支（`-DSIMULATION`）与真机同源换算（ADR-0003 fidelity 门禁）。 |
| **`test_dal_led`** | LED 驱动状态机。 |
| **`test_dal_button`** | 按键驱动状态机（原始状态）。 |
| **`test_button_debounce_e2e`** | 按键去抖动 e2e：runtime tick 驱动 dal_button 状态机，跨 sample_rate/debounce_ms 边界。 |
| **`test_dal_ssd1306`** | SSD1306 OLED I2C 驱动：初始化序列、画点/清屏、命令/数据区分。Phase 2 I2C 协议旁路验证。 |
| **`test_dev_config`** | ADR-0008 设备树覆写核心：CRC32 golden vector、合法/损坏 blob 解析降级、magic/version/CRC 校验、空 count、buffer 过小、未命中 id 跳过、apply 失败仅降级该项。 |
| **`test_avoidance_override`** | ADR-0008 覆写端到端：`pal_storage` 读 blob → 注册表派发 → 真实 DAL `apply_override` 改写全局 `neck_servo`/`front_radar` 字段；空/损坏 blob 静默降级。 |

---

### 🟢 Tier 3 · 协作式调度器（ADR-0013/0014）

wasm 侧协作式确定性调度器的 host 对等验证。共 8 个测试，覆盖调度器的完整 lifecycle。

| 测试 | 作用 |
|------|------|
| **`test_sim_scheduler`** | 调度器核心：任务创建/切换/优先级、单虚拟核确定性。 |
| **`test_sim_scheduler_e2e`** | 多任务合作 e2e（yield/sleep/mutex/semaphore 协同）。 |
| **`test_sim_scheduler_determinism`** | 确定性回放：相同种子/输入 → 相同调度序列。 |
| **`test_sim_scheduler_stack_clamp`** | 栈溢出保护（栈 canary、超限任务被杀）。 |
| **`test_sim_scheduler_wcet_fault`** | WCET 违约故障注入：任务超时 → 上报 trace + kill。 |
| **`test_sim_scheduler_zombie_gc`** | 已死任务的 zombie 状态 GC（防句柄泄漏）。 |
| **`test_sim_mutex_e2e`** | 协作 mutex 语义（阻塞/唤醒、优先级继承边界）。 |
| **`test_single_task_semantic_regression`** | 单任务模式与协作调度器共存不回归旧语义。 |
| **`test_sim_physical`** | 仿真物理时间推进模型（虚拟时间 vs wall clock）。 |

---

### 🔵 Tier 4 · Sample e2e（端到端）

每个 sample 都能被编译成 host 可执行并跑一段 tick，验证「PAL → DAL → runtime → App」完整链路。

| 测试 | Sample | 作用 |
|------|--------|------|
| **`app_avoidance_car_e2e`** | `avoidance_car` | 避障小车：ultrasonic + servo + runtime，注入障碍→舵机偏转。 |
| **`app_oled_dashboard_e2e`** | `oled_dashboard` | Button + LED + SSD1306 全链路（Phase 2 验收）。 |
| **`app_devkitc_smoke_e2e`** | `devkitc_smoke` | DevKitC S1-S8 真机全链路的 host 代码结构验证。 |
| **`app_dual_task_demo_e2e`** | `dual_task_demo` | 协作式双任务 demo：ADR-0013/0014 调度器端到端。 |
| **`sample_resource_conflict`** | `resource_conflict` | pal_resource 冲突检测反例：故意冲突→期望 init 失败。 |
| **`test_app_e2e`** *(共用)* | *(多样例的公共驱动)* | Unity 主入口，注入 sample 的 `app_*` 回调，跑 N tick。 |

> `unisim_smoke` sample 用于**浏览器/Node wasm 冒烟**（其 `app_callbacks.c` 打点 PAL/js_sim 桥接），host target 下不直接跑，但共享同一份 DAL/PAL 代码。

---

### ⚫ Tier 5 · 反例样例（Counter-examples）

用「构建失败/init 失败」作为验证的 sample，验证治理机制**真的会拒绝坏代码**。

| Sample | 期望行为 |
|--------|---------|
| **`resource_conflict`** | 故意让 GPIO/PWM 引脚冲突 → `pal_resource_claim` 返回负数错误码 → App init 失败 → `sample_resource_conflict` exe 检查该失败路径命中。 |

未来若引入 lint-only 反例（如「代码里不该出现的 API」），将并入本梯队。

---

## 🎯 日常开发测试策略

### 🏃 每次提交前
```powershell
pwsh ./run-tests.ps1
```
→ 增量构建 + 全 35 个测试，通常 30 秒内完成。

### 🔧 改了 DAL/外设驱动后
1. 对应 `test_dal_*` 通过
2. 再确认 `test_avoidance_override` / `test_dev_config` 未回归

### 🧠 改了核心架构（PAL/运行时/注册表）后
**必须确认**：
1. `test_pal_contract` 编译通过（契约未漂移）
2. Tier 1 全过（`test_runtime` / `test_actuator_registry` / `test_pal_storage` / `test_pal_resource`）
3. `run-tests.ps1 -Clean` 全量重建无 warning

### 🌀 改了调度器（`targets/wasm` scheduler 或 `pal_osal`）后
Tier 3 全部 8 个必须过；额外跑 `-Clean` 一次防止增量污染。

### 📋 PR 合并前 / 重大变更后
跑**双链验证**（GCC + MSVC），确保：
- 代码纯 C99，无 GCC 特有扩展
- MSVC `/W4 /WX` 下 0 warning
- 编译器差异未引入行为变化

---

## 🐛 常见问题排查

### Q: test_smoke 都失败了？
→ 构建链本身坏了。跑 `run-tests.ps1 -Clean` 清缓存重建。

### Q: test_pal_contract 编译失败？
→ **高优先级告警**：你改了 `wink_status.h` / `pal_*.h` 里的枚举/宏，但忘记同步到 ESP32 target 或 host 测试期望值。

### Q: MSVC 链报 warning 但 GCC 没事？
→ 你写了 GCC 特有扩展。常见原因：
- 裸 `__attribute__((unused))` → 用便携宏
- 空宏参数 → MSVC C4003
- 函数内 static 变量 → MSVC C4210

### Q: 改了代码但测试行为没变？
→ 增量构建的缓存问题。`run-tests.ps1 -Clean` 重建。

---

## 📌 为什么 MSVC 链重要

GCC 对 C 标准的宽容度比 MSVC 高很多——你可能无意中写了 GCC 特有扩展但自己不知道：

| GCC 允许 | MSVC 态度 | 后果 |
|----------|-----------|------|
| 空宏参数 | warning C4003 | 上游 C 代码到 ESP-IDF 可能编不过 |
| `__attribute__((unused))` | 不认识 | 同上 |
| 函数级 static 变量 | C4210 warning | 同上 |

**MSVC 链是最严格的 C99 合规性检查器**——它报的每一个 warning 都是潜在的跨编译器兼容性问题。

---

## 🔗 相关文档

- [ADR-0001 错误码符号约定](../docs/design/decisions/0001-error-code-sign-convention.md)
- [ADR-0004 静态分发 vs 运行期 ops](../docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md)
- [ADR-0008 动态设备树配置覆写](../docs/design/decisions/0008-dynamic-device-tree-config-flash.md)
- [ADR-0013 协作式确定性调度器](../docs/design/decisions/0013-sim-cooperative-scheduler.md)
- [ADR-0014 单虚拟核仿真模型](../docs/design/decisions/0014-sim-single-virtual-core.md)
- [ADR-0017 dal_ultrasonic 非阻塞化](../docs/design/decisions/0017-blocking-api-hard-isolation.md)
- [ADR-0018 PAL IRQ 收窄](../docs/design/decisions/0018-pal-irq-api-narrowing.md)
- [C 编码规范](../.claude/rules/c-code.md)
