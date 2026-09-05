# ADR-0068：HAL 包含路径规范与 WASM 文件命名约定

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已通过）** |
| 日期 | 2026-08-24 |
| 影响范围 | `pal/CMakeLists.txt`、全仓 C/C++ 源文件头文件包含、`targets/wasm/` 源文件 |
| 决策者 | 架构委员会 / 核心 PAL 维护组 |
| 关联 ADR | [ADR-0041](0041-hal-osal-directory-orthogonality.md)（HAL/OSAL 正交）；[ADR-0043](../tools/0043-yaml-driven-layer-lint.md)（YAML 分层 Lint）。 |

---

## 背景（Context）

1. **包含路径污染**：原 `pal/CMakeLists.txt` 将 `include/hal` 直接放入全局 `-I` 路径，导致全仓散落 51 处裸包含 `#include "pal_hal.h"` 与 `#include "pal_adc.h"`。这不仅混淆了外设所属层级，且导致头文件重命名/重构时缺乏语义前缀保护。
2. **Target 源文件命名风格分歧**：
   - ESP32 / Host 采用外设优先命名：`pal_hal_<periph>_<plat>.c`（如 `pal_hal_gpio_esp32.c`）；
   - WASM 采用轴向通道命名：`pal_wasm_ch<n>_<periph>.c`（如 `pal_wasm_ch1_gpio.c`，对应系统 4 大数据通道模型）。

---

## 决策（Decision）

1. **强制采用带子目录的包含路径（`"hal/..."`）**：
   - 所有新建与重构的源文件，包含硬件抽象接口时必须显式指定子目录前缀，例如：
     - `#include "hal/pal_pin_types.h"`
     - `#include "hal/pal_gpio.h"`
     - `#include "hal/pal_pwm.h"`
     - `#include "hal/pal_i2c.h"`
     - `#include "hal/pal_spi.h"`
   - 严禁书写无前缀的 `#include "pal_gpio.h"`。

2. **CMake `-I` 路径两步式迁移机制**：
   - **Phase 1**：CMakeLists.txt 暂时保留 `include/hal`，由 `pal_hal.h` Umbrella 聚合头提供转发，确保既有老工程零破坏编译；
   - **Phase 3**：通过批量脚本完成全仓 51 处裸包含替换后，正式从 `pal/CMakeLists.txt` 移除 `include/hal` 全局 `-I`，并由 CI 静态规则阻断裸包含。

3. **WASM 轴向命名规范正式文档化**：
   - 确认 WASM 模拟层的 `pal_wasm_ch<n>_<periph>.c` 命名属于仿真数据通道架构（Channel 1: GPIO, Channel 1b: PWM, Channel 2: Bus, Channel 3: ADC, Channel 4: Buffer），符合仿真系统设计规范，保持现有命名，无需重命名。

---

## 影响（Consequences）

- **正向收益**：彻底消灭了头文件重名冲突隐患，头文件归属层级一目了然，构建系统包含树更加清晰卫生。
- **迁移要求**：在 Phase 3 需配合自动化脚本统一清扫存量代码中的裸 include。
