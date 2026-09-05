# dal_ultrasonic 分层重构实施计划

| 项 | 内容 |
|---|------|
| **计划状态** | ✅ 已完成（`7bf0f70 feat(pal): add ultrasonic HAL interface contract` + `9de6719 feat(wasm)` + `689726c feat(esp32)` + `691174c refactor(dal): switch ultrasonic driver to PAL interface` — `dal/src/sensor/dal_ultrasonic.c` 已零平台依赖；事后回填：2026-07-03） |
| 创建日期 | 2026-06-30 |
| 关联 ADR | ADR-0002 (双 target 同源编译), ADR-0007 (Protothread) |
| 关联设计规范 | 02-wink-micro-os/02-pal-platform-abstraction.md |
| 预估耗时 | 30~45 分钟 |
| 代码变动 | +120 行 / -40 行（净增 ~80 行） |
| 风险等级 | 🟢 极低（零风险渐进式重构） |

---

## 🎯 重构目标

### 问题背景

当前 `dal_ultrasonic.c` 直接包含平台特定头文件和代码：
```c
#ifdef SIMULATION
#include "wasm_bridge.h"       // WASM 仿真特定
#endif

#ifdef ESP_PLATFORM
#include "driver/rmt.h"        // ESP32 特定
#include "driver/gpio.h"
#endif
```

这种模式的问题：
1. ❌ DAL 层破坏了分层架构，直接感知平台细节
2. ❌ AI 代码生成时容易写错 `#ifdef` 边界
3. ❌ 新增平台需要修改所有 DAL 器件文件
4. ⚠️ 平台错配要等编译到对应 target 才发现

### 重构后架构

```
┌──────────────────────────────────────────────────┐
│  DAL 层 (dal_ultrasonic.c)                      │
│  ✅ 只依赖 PAL 接口，完全平台无关                 │
│  ✅ 业务逻辑：单位换算、超时处理、滤波            │
└──────────────────────┬───────────────────────────┘
                       │
┌──────────────────────▼───────────────────────────┐
│  PAL 接口契约 (pal/include/hal/pal_ultrasonic.h) │
│  ✅ 纯接口定义，零平台依赖                        │
│  ✅ 编译期静态分发                                │
└──────────────────────┬───────────────────────────┘
                       │
        ┌──────────────┴──────────────┐
        ▼                             ▼
┌──────────────────┐       ┌──────────────────┐
│ WASM 实现        │       │ ESP32 实现        │
│ targets/wasm/    │       │ targets/esp32/    │
│ 调用 js_sim_*    │       │ 操作 RMT 硬件     │
└──────────────────┘       └──────────────────┘
```

### 重构前后对比矩阵

| 评估维度 | 重构前 | 重构后 |
|---------|--------|--------|
| 分层清晰度 | ❌ DAL 直接知道平台细节 | ✅ DAL 只依赖 PAL 接口契约 |
| 错误发现时机 | ⚠️ 编译到 target 才发现 | ✅ 链接期就发现缺失实现 |
| 运行时开销 | ✅ 零开销 | ✅ 零开销，编译期静态分发 |
| 新增平台成本 | 🟡 每个 DAL 文件加 `#ifdef` | ✅ 只在 targets/ 下加实现 |
| AI 代码生成友好度 | ❌ AI 容易写错条件编译 | ✅ AI 只需要调用标准接口 |
| 单步调试友好度 | ⚠️ 要跳转到平台相关代码 | ✅ 接口边界清晰，断点好打 |

---

## 📋 前置检查（5 分钟）

### Step 0: 建立基线

**执行命令：**

```bash
# 1. 确保工作区干净
git status
git stash -u  # 有未提交的修改先暂存

# 2. 验证 WASM 构建基线（Jest 测试）
cd simulator && npm test
# 预期：78/78 测试通过

# 3. 验证 ESP32 构建基线
cd ../wink-micro-os && idf.py build
# 预期：编译成功，0 error

# 4. 标记基线 commit（关键！出问题随时回滚）
git tag -a refactor/baseline-before-ultrasonic \
    -m "Baseline before ultrasonic PAL refactor"
```

**验收标准：**
- ✅ Jest 78/78 测试全部通过
- ✅ ESP32 IDF 构建成功，0 error
- ✅ 基线 Tag 已创建

**回滚策略（任何步骤出问题都能用）：**
```bash
git reset --hard refactor/baseline-before-ultrasonic
```

---

## 🔨 重构执行（20~30 分钟）

### Step 1: 定义 PAL 接口契约（零风险，只加文件）

**目标：** 新建 PAL 接口头文件，此时没有任何代码引用它，绝对不会破坏任何东西。

**执行：**

1. 新建文件：`wink-micro-os/pal/include/hal/pal_ultrasonic.h`

```c
/**
 * @file pal_ultrasonic.h
 * @brief 超声波传感器 PAL 接口（平台无关契约）
 *
 * 所有平台（WASM / ESP32 / STM32）都必须实现这些接口。
 * DAL 层只依赖这个头文件，不感知任何平台细节。
 *
 * 设计原则：
 * 1. 零平台依赖：只使用 pal_hal.h 定义的基础类型
 * 2. 最小接口集：只暴露 DAL 层真正需要的功能
 * 3. 前向兼容：新增平台不需要修改此文件
 */

#pragma once
#include "pal_hal.h"      // 已存在，包含 pal_gpio_pin_t 等基础类型
#include "wink_status.h"  // 已存在，统一错误码

/**
 * @brief 触发超声波测量并返回回波脉宽（同步阻塞）
 *
 * @param trigger_pin 触发引脚
 * @param echo_pin 回波引脚
 * @param timeout_us 超时时间（微秒），建议 38000us (~13 米最大量程）
 * @return uint32_t 回波脉宽（微秒），0 表示超时或测量失败
 */
uint32_t pal_hal_ultrasonic_measure_pulse_us(
    pal_gpio_pin_t trigger_pin,
    pal_gpio_pin_t echo_pin,
    uint32_t timeout_us
);

/**
 * @brief 仅触发超声波发射（不等待回波，用于异步测量模式）
 *
 * @param trigger_pin 触发引脚
 * @return wink_status_t WINK_OK 表示成功
 */
wink_status_t pal_hal_ultrasonic_trigger(pal_gpio_pin_t trigger_pin);

/**
 * @brief 读取最近一次触发后的回波脉宽（异步模式）
 *
 * @param echo_pin 回波引脚
 * @param timeout_us 等待超时（微秒），0 表示立即返回
 * @return uint32_t 回波脉宽（微秒），0 表示无有效回波
 */
uint32_t pal_hal_ultrasonic_read_echo_pulse_us(
    pal_gpio_pin_t echo_pin,
    uint32_t timeout_us
);
```

2. 验证头文件能独立编译：

```bash
cd wink-micro-os
gcc -fsyntax-only -Ipal/include -Ipal/include/hal \
    pal/include/hal/pal_ultrasonic.h
```

**验收标准：**
- ✅ 头文件无语法错误
- ✅ `git status` 显示只有 1 个新增文件
- ✅ Jest 测试仍然全部通过（新增文件不会被引用）
- ✅ ESP32 构建仍然通过

**提交：**
```bash
git add pal/include/hal/pal_ultrasonic.h
git commit -m "feat(pal): add ultrasonic HAL interface contract"
```

**风险：0**

---

### Step 2: 实现 WASM 版本（零风险，只加文件）

**目标：** 在 `targets/wasm/` 下实现 WASM 版本，只在 WASM 构建中编译，ESP32 完全看不到。

**执行：**

1. 新建文件：`wink-micro-os/targets/wasm/pal_hal_ultrasonic.c`

```c
/**
 * @file pal_hal_ultrasonic.c
 * @brief WASM 仿真平台的超声波传感器实现
 *
 * 通过 wasm_bridge.h 调用 JS 侧的物理模拟函数。
 * 此文件仅在 WASM target 编译，ESP32 构建完全不可见。
 *
 * 注意：WASM 仿真模式下引脚号没有实际硬件意义，
 *       物理模拟的引脚映射由 JS 侧统一管理。
 */

#include "hal/pal_ultrasonic.h"
#include "wasm_bridge.h"  // ✅ WASM 特定头文件，只在此文件中出现

uint32_t pal_hal_ultrasonic_measure_pulse_us(
    pal_gpio_pin_t trigger_pin,
    pal_gpio_pin_t echo_pin,
    uint32_t timeout_us
) {
    // 直接委托给 JS 侧的物理模拟函数
    (void)trigger_pin;  // WASM 仿真不需要真实引脚号
    (void)echo_pin;
    (void)timeout_us;
    return (uint32_t)js_sim_ultrasonic_read_echo_pulse();
}

wink_status_t pal_hal_ultrasonic_trigger(pal_gpio_pin_t trigger_pin) {
    (void)trigger_pin;
    js_sim_ultrasonic_trigger();
    return WINK_OK;
}

uint32_t pal_hal_ultrasonic_read_echo_pulse_us(
    pal_gpio_pin_t echo_pin,
    uint32_t timeout_us
) {
    (void)echo_pin;
    (void)timeout_us;
    return (uint32_t)js_sim_ultrasonic_read_echo_pulse();
}
```

2. 修改 `targets/wasm/CMakeLists.txt`，把新源文件加入 `PAL_WASM_SOURCES`：

```cmake
set(PAL_WASM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_hal_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_osal_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_resource_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_storage_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_physical.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_hal_ultrasonic.c   # ← 新增这行
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/wink_sim_physical.c
    PARENT_SCOPE)
```

3. 验证 WASM 构建和测试：

```bash
cd simulator && npm test
```

**验收标准：**
- ✅ Jest 78/78 全部通过
- ✅ ESP32 构建仍然通过（新源文件只在 WASM CMake 里，ESP32 看不到）

**提交：**
```bash
git add targets/wasm/pal_hal_ultrasonic.c targets/wasm/CMakeLists.txt
git commit -m "feat(wasm): implement ultrasonic HAL for WASM simulation"
```

**风险：0**

---

### Step 3: 实现 ESP32 版本（零风险，只加文件）

**目标：** 在 `targets/esp32/` 下实现 ESP32 版本，只在 ESP32 构建中编译。

**执行：**

1. 新建文件：`wink-micro-os/targets/esp32/pal_hal_ultrasonic.c`

```c
/**
 * @file pal_hal_ultrasonic.c
 * @brief ESP32 平台的超声波传感器实现
 *
 * 使用 ESP-IDF RMT 外设进行高精度脉冲捕获。
 * 此文件仅在 ESP32 target 编译，WASM 构建完全不可见。
 *
 * 注意：所有代码从 dal_ultrasonic.c 的 ESP_PLATFORM 分支 1:1 拷贝，
 *       不做任何算法改动，只做函数签名适配。
 */

#include "hal/pal_ultrasonic.h"
#include "driver/rmt.h"     // ✅ ESP32 特定，只在此文件中出现
#include "driver/gpio.h"    // ✅ ESP32 特定，只在此文件中出现
#include "esp_err.h"

// ========== 从 dal_ultrasonic.c ESP_PLATFORM 分支拷贝 ==========
// 这里原封不动拷贝 ESP32 RMT 实现代码，只做函数名替换
// 例如：dal_ultrasonic_rmt_init() → 静态函数放在这里
//       dal_ultrasonic_read_pulse() → pal_hal_ultrasonic_measure_pulse_us()
// ==================================================================

// TODO: 将 dal_ultrasonic.c 中 #ifdef ESP_PLATFORM 分支的代码移到这里
```

2. 将 `dal_ultrasonic.c` 中 `#ifdef ESP_PLATFORM` 分支的代码原封不动地拷贝到这个文件里，包装成 PAL 接口函数。**不做任何算法逻辑改动！**

3. 修改 ESP32 侧的 CMake 配置，把新源文件加入编译。

4. 验证 ESP32 构建：

```bash
cd wink-micro-os && idf.py build
```

**验收标准：**
- ✅ ESP32 构建 100% 通过，0 error
- ✅ WASM Jest 测试仍然全部通过

**提交：**
```bash
git add targets/esp32/pal_hal_ultrasonic.c targets/esp32/CMakeLists.txt
git commit -m "feat(esp32): implement ultrasonic HAL for ESP32 RMT"
```

**风险：0**

---

### Step 4: 渐进式切换 DAL 层调用（唯一有风险的一步）

**目标：** 把 `dal_ultrasonic.c` 里的平台相关代码替换成 PAL 接口调用。这是唯一有可能破坏构建的一步，但也有立即回滚的能力。

**执行原则：只改调用位置，不改算法逻辑！**

**执行：**

1. 修改 `dal/src/sensor/dal_ultrasonic.c`：

   - 删除 `#ifdef SIMULATION` 里的 `#include "wasm_bridge.h"`
   - 删除 `#ifdef ESP_PLATFORM` 里的 `#include "driver/rmt.h"` 等
   - 统一改为 `#include "hal/pal_ultrasonic.h"`
   - 把所有 `js_sim_ultrasonic_*` 和 `rmt_*` 调用替换为 `pal_hal_ultrasonic_*`
   - 删除所有平台条件编译分支，只保留统一的 PAL 调用

2. **同时验证两个平台的构建（必须！）：**

```bash
# 验证 WASM
cd simulator && npm test

# 验证 ESP32
cd ../wink-micro-os && idf.py build
```

**验收标准：**
- ✅ Jest 78/78 全部通过
- ✅ ESP32 构建 100% 通过
- ✅ 运行时行为完全一致（超声波读数不变）
- ✅ `dal_ultrasonic.c` 中不再有 `SIMULATION` 或 `ESP_PLATFORM` 宏

**提交：**
```bash
git add dal/src/sensor/dal_ultrasonic.c
git commit -m "refactor(dal): switch ultrasonic driver to PAL interface"
```

**回滚策略（出问题立即执行）：**
```bash
# 这一步只改了 1 个文件，随时可以撤销
git checkout -- dal/src/sensor/dal_ultrasonic.c
```

**风险：低（所有算法逻辑都不变，只是调用位置变了）**

---

### Step 5: 清理残留代码（零风险，只删不增）

**目标：** 清理所有残留的平台特定代码，文件现在应该完全平台无关。

**执行：**

1. 检查 `dal_ultrasonic.c`，删除所有：
   - 残留的 `#ifdef SIMULATION` / `#endif`
   - 残留的 `#ifdef ESP_PLATFORM` / `#endif`
   - 任何平台特定的注释

2. 最终 `dal_ultrasonic.c` 应该只有：
   - 标准 C include (`string.h`, `stdint.h`, `math.h` 等)
   - `#include "hal/pal_ultrasonic.h"`
   - DAL 层业务逻辑（单位换算、超时处理、滤波）
   - 调用 PAL 接口

3. **最终双平台验证（必须！）：**

```bash
cd simulator && npm test
cd ../wink-micro-os && idf.py build
```

**验收标准：**
- ✅ `dal_ultrasonic.c` 中 grep 不到 `SIMULATION` 或 `ESP_PLATFORM`
- ✅ Jest 78/78 全部通过
- ✅ ESP32 构建 100% 通过
- ✅ 文件行数比重构前更少（删除了所有平台分支）

**提交：**
```bash
git add dal/src/sensor/dal_ultrasonic.c
git commit -m "cleanup(dal): remove platform conditionals from dal_ultrasonic"
```

**风险：0**

---

## ✅ 最终验收（5 分钟）

### 验收清单

| 检查项 | 通过标准 | 验证命令 |
|--------|---------|---------|
| Jest 测试 | 78/78 全部通过 | `cd simulator && npm test` |
| ESP32 构建 | 0 error 0 warning | `cd wink-micro-os && idf.py build` |
| DAL 层纯净度 | `dal_ultrasonic.c` 无任何平台特定 include 或宏 | `grep -E "SIMULATION|ESP_PLATFORM|wasm_bridge|driver\/" dal/src/sensor/dal_ultrasonic.c` → 无输出 |
| 接口一致性 | WASM 和 ESP32 实现了完全相同的 PAL 接口 | 对比两个实现文件的函数签名 |
| Git 历史 | 5 个独立原子 commit，可追溯可回滚 | `git log --oneline -5` |

### 最终 Commit 结构

```
5 commits:
1. feat(pal): add ultrasonic HAL interface contract
2. feat(wasm): implement ultrasonic HAL for WASM simulation
3. feat(esp32): implement ultrasonic HAL for ESP32 RMT
4. refactor(dal): switch ultrasonic driver to PAL interface
5. cleanup(dal): remove platform conditionals from dal_ultrasonic
```

### 完成标记

```bash
git tag -a refactor/ultrasonic-pal-done \
    -m "Ultrasonic PAL layer refactor completed successfully"
```

---

## 🚨 风险防控总览

| 可能的问题 | 发生概率 | 影响范围 | 处理方式 |
|-----------|---------|---------|---------|
| WASM 测试失败 | 低 | 仅 Step 4 | `git checkout -- dal_ultrasonic.c` 立即回滚 |
| ESP32 编译失败 | 低 | 仅 Step 3/4 | 修复对应平台的实现文件，不影响其他 |
| 运行时行为变化 | 极低 | 超声波读数 | Step 0 有基线 Tag，随时可回滚对比 |
| 算法逻辑不小心改坏 | 极低 | 超声波测量 | Step 4 明确要求"只改调用位置，不改算法逻辑" |
| 遗漏了某个平台分支的代码 | 低 | 编译错误 | grep 检查所有平台宏，确保清理干净 |

---

## 📈 重构后的长期架构收益

### 1. 新增传感器无需碰 DAL 层架构

新加温湿度、光照、气压传感器：
1. 定义 PAL 接口
2. 每个平台实现接口
3. DAL 层调用统一接口

**AI 生成新传感器驱动时，完全不需要知道平台差异。**

### 2. 新增平台成本大幅降低

未来支持 STM32 / NRF52 / 其他 MCU：
- ✅ 不需要改任何 DAL 层代码
- ✅ 只需要在 `targets/<mcu>/` 下实现所有 PAL 接口
- ✅ 零风险（改坏了只影响对应 MCU 的构建）

### 3. AI 代码生成质量提升

- ✅ AI 不需要知道 `#ifdef SIMULATION` 这种项目特定的魔法
- ✅ AI 只需要调用标准 PAL 接口
- ✅ 生成的代码天然跨平台兼容
- ✅ 错误从运行时提前到编译期

### 4. 单元测试更容易

- ✅ 可以在主机端 mock PAL 接口测试 DAL 逻辑
- ✅ 不需要真实硬件就能跑大部分传感器逻辑测试
- ✅ 测试速度从秒级降到毫秒级

---

## 📌 架构设计原则验证

本次重构完全符合项目已有的 ADR 决策：

| ADR | 符合情况 |
|-----|---------|
| **ADR-0002 双 target 同源编译** | ✅ 完美符合 - DAL 代码真正实现了同源 |
| **ADR-0004 静态分发 vs 运行期 ops** | ✅ 完美符合 - 编译期选择实现，零运行时开销 |
| **ADR-0007 Protothread** | ✅ 不影响协程调度，完全兼容 |

---

## 🎉 重构完成标准

执行完本计划后，可以说已经完成了：

> ✅ **DAL 层与 PAL 层的真正解耦**
>
> DAL 层从此以后：
> - 不知道什么是 WASM
> - 不知道什么是 ESP32
> - 不知道什么是 RMT
> - 只知道 PAL 接口契约

---

**计划制定完成，可以开始执行。**
