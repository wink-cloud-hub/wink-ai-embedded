# wink-micro-os 测试体系

> 本文件是 wink-micro-os host 测试的一站式入口。所有测试均在 PC 上运行，**不需要真实硬件或浏览器**。

---

## 🚀 快速命令（复制即用）

| 命令 | 用途 | 用时 | 频率 |
|------|------|------|------|
| `pwsh ./run-tests.ps1` | **日常开发门禁**：增量构建 + 跑全部 20 个 GCC 测试 | 5s - 2min | ✅ 每次提交 |
| `pwsh ./run-tests.ps1 -Clean` | 完全重建 + 跑全部测试 | 1 - 3min | 🔧 改了 CMake / 怀疑缓存污染 |
| `pwsh ./run-tests.ps1 -Detailed` | 跑测试 + 打印每个用例完整输出 | 2 - 5min | 🐛 排查失败用例 |
| **MSVC 链验证** | Visual Studio x64 Native Tools Command Prompt 中运行：<br>`cmake -G Ninja -DCMAKE_C_COMPILER=cl -DTARGET_PLATFORM=host -B build-msvc`<br>`cmake --build build-msvc`<br>`ctest --test-dir build-msvc --output-on-failure` | 2 - 5min | 📅 PR 合并前 / 重大变更 |

---

## 🧩 测试体系概述

### 双链的意义

wink-micro-os 采用 **GCC + MSVC 双链验证**：

```
           ┌──────────────────────────────────────────────────┐
           │         同一套 20 个测试用例，两个编译器          │
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

---

## 📊 测试梯队详解

按**价值密度**从高到低排序。

---

### 🔴 第一梯队：门禁级（失败绝不能提交）

| 测试 | 作用 | 对应决策/目标 |
|------|------|--------------|
| **`test_pal_contract`** | **PAL 契约完整性编译探针** | 评审 P0-1 对策 |
| | 对每一个跨 target 被使用的枚举/宏做**编译期断言**（如 `WINK_ERR_HARDWARE == -12`、`WINK_MUTEX_WAIT_FOREVER == 0xFFFFFFFF`）。任何符号缺失或取值漂移 → **编译失败**，立即拦截。 | |
| | 这是 host 端对 ESP32 契约完整性能给出的最强自检。 | |
| **`test_dev_config`** | **ADR-0008 设备树覆写核心单测** | ADR-0008 Wave B 核心 |
| | CRC32 golden vector 验证、合法/损坏 blob 解析降级、magic/version/CRC 校验、空 count、buffer 过小、未命中 id 跳过、apply 失败仅降级该项。 | |
| **`test_avoidance_override`** | **ADR-0008 覆写端到端** | ADR-0008 e2e |
| | `pal_storage` 读 blob → 注册表派发 → 真实 DAL `apply_override` 改写全局 `neck_servo`/`front_radar` 字段。验证空/损坏 blob 静默降级到编译期默认。 | |

> 💡 **这三个是测试套件的核心价值**——它们保护了最容易出问题的架构契约和新特性。

---

### 🟠 第二梯队：核心子系统回归

| 测试 | 作用 |
|------|------|
| **`test_pal_storage`** | PAL 存储抽象（host 内存实现）。ADR-0008 Flash 存储层的 host 对等实现。验证 `pal_storage_read/write/erase` 语义一致：越界读返回 0、越界写截断、erase 清零。 |
| **`test_actuator_registry`** | 执行器关断注册表。注册/查询执行器、故障时按优先级关断、最大安全状态回调。是设备树覆写的派发基础设施。 |
| **`test_runtime`** | 主循环 + 软定时器。`wink_runtime_init/run`、软定时器注册/触发/取消、回调优先级。是整个 OS 的心脏。 |
| **`test_pal_pwm_router`** | PWM 通道分配状态机。DAL 外设请求 PWM 通道时的分配/回收算法，防冲突、溢出保护、分配失败降级。target 无关，host 可完全单测。 |

---

### 🟡 第三梯队：DAL 外设驱动

| 测试 | 作用 |
|------|------|
| **`test_dal_servo`** | 舵机驱动 + `apply_override`。角度 clamp、脉宽计算、ADR-0008 覆写 `min_pulse_ms`/`max_pulse_ms`。 |
| **`test_dal_ultrasonic`** | 超声波驱动 + `apply_override`。距离计算超时保护、覆写 `trig_pin`/`echo_pin`。 |
| **`test_dal_ultrasonic_sim`** | 超声波仿真分支（`-DSIMULATION`）。验证 WASM 侧的仿真实现与真机同源。ADR-0003 仿真 fidelity 门禁。 |
| **`test_dal_led`** / **`test_dal_button`** | LED/按键驱动。基础 IO 外设的状态机正确性。 |
| **`test_dal_ssd1306`** | SSD1306 OLED I2C 驱动。初始化序列、画点/清屏、命令/数据区分。Phase 2 I2C 协议旁路验证。 |

---

### 🟢 第四梯队：基础设施与契约

| 测试 | 作用 |
|------|------|
| **`test_host_pal`** | PAL HAL/OSAL host 实现 smoke。`pal_delay_ms/us`、`pal_gpio_read/write`、`pal_pwm_set`、mutex/semaphore。 |
| **`test_pal_resource`** | 资源占用治理。资源计数、超出最大实例数拒绝分配、泄漏检测。防 AI 生成代码无脑创建新实例。 |
| **`test_trace`** | 故障上报 trace。环缓冲区、写入/读取、溢出行为、故障码序列化。故障诊断的核心基础设施。 |
| **`test_smoke`** | 编译链 smoke test。最简单的 `TEST_ASSERT_TRUE(1)`。如果它失败，说明构建/链接链本身坏了（不是代码问题）。 |

---

### 🔵 第五梯队：Sample e2e

| 测试 | 作用 |
|------|------|
| **`app_avoidance_car_e2e`** | avoidance_car 样例端到端。样例 App 注入 runtime 主循环，验证完整应用链路能跑通。 |
| **`app_oled_dashboard_e2e`** | OLED Dashboard 样例端到端。Button + LED + SSD1306 全链路验证（Phase 2 验收）。 |
| **`app_devkitc_smoke_e2e`** | DevKitC 冒烟样例端到端。S1-S8 真机全链路的 host 代码结构验证。 |

---

## 🎯 日常开发测试策略

### 🏃 每次提交前
```powershell
pwsh ./run-tests.ps1
```
→ 增量构建 + 全 20 个测试，通常 30 秒内完成。

### 🔧 改了 DAL/外设驱动后
1. 对应 `test_dal_*` 通过
2. 再确认 `test_avoidance_override` / `test_dev_config` 未回归

### 🧠 改了核心架构（PAL/运行时/注册表）后
**必须确认**：
1. `test_pal_contract` 编译通过（契约未漂移）
2. `test_runtime` / `test_actuator_registry` / `test_pal_storage` 全过
3. `run-tests.ps1 -Clean` 全量重建无 warning

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
- [C 编码规范](../.claude/rules/c-code.md)
