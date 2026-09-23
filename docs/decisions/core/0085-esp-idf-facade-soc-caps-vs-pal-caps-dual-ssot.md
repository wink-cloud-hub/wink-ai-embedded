# ADR-0085：ESP-IDF 门面 SoC 能力判据与 PAL 平台上限双 SSOT 架构

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-09-23）** |
| 日期 | 2026-09-23 |
| 触发 | `PLAN-20260922-ESP-IDF-SIM-MASTER` §3.3.2：ESP-IDF 门面 `chips/${WINK_ESP_TARGET}` 的 `SOC_*` 宏与 `pal_target_caps.h` 仿真硬编码分支产生双 SSOT 分叉，需在开工前裁决职责划界 |
| 影响范围 | `wink-micro-os/frameworks/esp_idf/` 门面层、`pal/include/hal/pal_target_caps.h`（本 ADR 不修改，仅界定职责）、`frameworks/esp_idf/chips/` SoC 矩阵 |
| 决策者 | 项目架构团队 / 仿真拦截专项小组 |
| **关联 ADR** | [ADR-0064](0064-target-capability-ssot.md)（PAL Target Caps SSOT 头文件架构）、[ADR-0012](0012-contract-honesty-over-silent-degradation.md)（合约诚实）、[ADR-0014](../unisim/0014-sim-single-virtual-core.md)（单虚拟核协作调度）|
| **关联计划** | [PLAN-20260922-ESP-IDF-SIM-MASTER](../../implementation-plans/esp32/2026-09-22-esp-idf-simulation-interception-master-plan.md) §3.3.2 |

---

## 1. 背景（Context）

### 1.1 冲突事实

ADR-0064 建立了 `pal/include/hal/pal_target_caps.h` 作为平台能力 SSOT。在真机 ESP32 构建中，该头文件通过 `#if defined(ESP_PLATFORM)` 分支直接映射自 ESP-IDF 的 `soc/soc_caps.h`，能力值精确。

但在仿真构建（`__wasm__` 或 host）中，编译器永远命中 wasm/host 硬编码 fallback 分支：

```c
// pal_target_caps.h — wasm/host fallback (ADR-0064)
#define PAL_PWM_CHANNEL_MAX     8
#define PAL_I2C_PORT_MAX        2
#define PAL_GPIO_PIN_MAX        50
```

ESP-IDF 门面引入 `chips/${WINK_ESP_TARGET}/include/soc/soc_caps.h`，按单芯片能力严格校验（如 ESP32-C3: `SOC_GPIO_PIN_COUNT=22`, `SOC_I2C_NUM=1`）。这产生了 **双 SSOT 分叉**：

| 层 | ESP32 | ESP32-C3 | 冲突方向 |
|---|---|---|---|
| 门面 `SOC_*` | GPIO=40, I2C=2 | GPIO=22, I2C=1 | 门面严于 PAL |
| PAL caps | GPIO=50, I2C=2 | GPIO=50, I2C=2 | PAL 始终宽松 |

风险方向为「门面严于 PAL」：门面判定越界的操作，PAL 不会额外拦截——**不会产生运行时错误**，但双规则漂移违反 SSOT 原则，且未来如果 PAL 值被错误缩小会导致本应合法的操作被底层拦截而门面不知情。

### 1.2 为什么不直接修改 `pal_target_caps.h`

1. `pal_target_caps.h` 服务全部仿真 target（Wink-Native、Arduino、MCS-51），不能因 ESP-IDF 门面的芯片选择而缩小全局上限。
2. ADR-0064 明确将 wasm/host 分支定义为「仿真宿主基线能力」，非特定芯片能力。
3. 修改 PAL caps 将影响所有框架的既有测试和 `pal_resource` 池大小。

---

## 2. 方案比选（Options）

| 方案 | 描述 | 结论 |
|---|---|---|
| A. 双层职责划界 | 门面以 `SOC_*` 为唯一合法性判据；PAL caps 为仿真宿主绝对上限，不因芯片缩小 | **采纳** |
| B. PAL caps 动态注入 | 按 `WINK_ESP_TARGET` 在构建期自动替换 `pal_target_caps.h` 宏值 | 否决：影响其他框架；需大幅重构 PAL caps 头文件架构 |
| C. 统一取两者最小值 | 门面和 PAL 均取 `min(SOC_*, PAL_*)` | 否决：引入隐式耦合；PAL 值变化会无预期地影响门面行为 |

---

## 3. 决策结论（Decision）

### D1 门面以 `SOC_*` 宏为唯一合法性判据

ESP-IDF 门面层（`frameworks/esp_idf/src/drivers/*.c`）进行参数合法性校验时，**仅依据** `chips/${WINK_ESP_TARGET}/include/soc/soc_caps.h` 中的 `SOC_*` 宏与 `GPIO_IS_VALID_GPIO()` 等芯片级判断函数。越界操作直接返回 `ESP_ERR_INVALID_ARG`。

门面**不读取**、**不依赖** `PAL_GPIO_PIN_MAX` / `PAL_I2C_PORT_MAX` 等 PAL 层宏。

### D2 PAL caps 为仿真宿主绝对上限，不因芯片缩小

`pal_target_caps.h` 的 wasm/host 硬编码值（GPIO=50, I2C=2, PWM=8）代表 **仿真宿主最多能支持的硬件资源池大小**。该值只增不减（除非宿主本身资源缩减），不因 `WINK_ESP_TARGET` 的选择而变化。

### D3 构建时 CMake 芯片路径注入

- 构建传入 `-DWINK_ESP_TARGET=esp32c3`（默认 `esp32`）；
- CMake 将 `frameworks/esp_idf/chips/${WINK_ESP_TARGET}/include` 置于包含路径 **首位**（`BEFORE PUBLIC`）；
- `soc/soc_caps.h` 透传至芯片专属头，门面编译时即绑定目标芯片能力。

### D4 职责划界文字声明

| 层 | 职责 | 判据来源 | 变动频率 |
|---|---|---|---|
| ESP-IDF 门面 | 「芯片能做什么」 | `SOC_*` / `GPIO_IS_VALID_*` | 随 `WINK_ESP_TARGET` 切换 |
| PAL caps | 「仿真宿主最多能开多少」 | `pal_target_caps.h` 硬编码 | 极少变动，仅宿主资源扩展时 |

### D5 长期演进方向（不阻塞当前计划）

对齐 `mcu-compat-plan.md §4.1` 提出的「caps 自注入重构」：未来在 `targets/*/` 下分发 `pal_target_caps_*.h`，由 CMake 按 target + 芯片组合自动选择，彻底消除 `#if defined(ESP_PLATFORM)` / `#if defined(__wasm__)` 的二分 fallback。该方向作为后续 ADR 演进，不影响本 ADR 的即时执行。

---

## 4. 影响（Consequences）

### 正向收益

- **职责清晰**：门面与 PAL 各自有唯一判据来源，不存在交叉依赖或隐式耦合。
- **零侵入**：不修改 `pal_target_caps.h`，不影响既有框架（Arduino/MCS-51/Wink-Native）的测试和行为。
- **芯片扩展便利**：新增 SoC 只需在 `chips/` 下创建目录并填写 `soc_caps.h`，无需触碰 PAL 层。

### 治理要求

1. ESP-IDF 门面代码中**严禁** `#include "hal/pal_target_caps.h"` 或使用 `PAL_GPIO_PIN_MAX` 等宏。
2. `frameworks/esp_idf/tools/lint/` 外部 pack 应包含对此约束的机器检查规则。
3. 任何未来对 `pal_target_caps.h` wasm/host 分支值的修改，须评估对所有已注册框架的影响。

### 回写要求

本 ADR Accepted 后，须将 D1~D4 核心内容回写至 `docs/zh/design/02-wink-micro-os/` 设计规范中 PAL 能力管理相关章节，并在 `PLAN-20260922-ESP-IDF-SIM-MASTER` 中将 D-003 标记为闭环。
