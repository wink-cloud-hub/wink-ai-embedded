# ESP-IDF 仿真拦截层实施计划 M3：SoC 矩阵扩展、自动化测试体系与收官验收

> 📋 **计划状态声明**：
> 本计划为 ESP-IDF 仿真拦截层派生子计划（Milestone 3，收官里程碑）。
> **继承总纲**：[`PLAN-20260922-ESP-IDF-SIM-MASTER`](./2026-09-22-esp-idf-simulation-interception-master-plan.md) (v3.5)
> **当前状态**：📋 待开始（v2.2 完成 ESP-IDF 官方源码真值校核与 CI 可执行性修订，前置依赖 M2 v2.4 已 100% 验收闭环）
> 🎯 **计划版本**：v2.2（2026-09-25，官方真值校核修订：S3 有效掩码/枚举回归官方事实（22~25 物理不存在）、C6 LP 外设 HP-only 有意偏离登记、"LTS" 术语与 EOL 更正、全部 CMake 配置补 `-DTARGET_PLATFORM=host`（修复默认 wasm 平台导致 ctest 零测试假绿）、winkcli CI 安装方式修正、覆盖率 link options 改 PUBLIC、`WINK_IDF_TARGET_DEFINE` 目录作用域修正、legacy_i2c corpus 硬编码宏清理、T-012 范围声明对齐）
> 📚 **关联规范**：`docs-adr.md`、`03-coding-guidelines.md`、`00-IMPLEMENTATION-PLAN-TEMPLATE.md`
> 🔍 **M2 移交基线**：M2 v2.4 已 100% 验收交付（DoD 全部通过，28/28 CTest 测试 100% 绿灯，License Map 与 Layering Lint 0 findings；闭环 4 项立即架构加固：外设全局复位链条 `esp_peripherals_reset`、I2C 链表多事务阻断与显式校验、NVS 静态池严格压缩至 3KB、UART 并发读者防护与事件长度保真）。

---

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|:---|:---|
| **计划编号** | `PLAN-20260926-ESP-IDF-SIM-M3` |
| **创建日期** | 2026-09-23（v1.0 骨架；v1.1 约束增补；v1.2 行为证据塔约束；v2.0 详设完全展开于 2026-09-25） |
| **目标平台/SoC** | `wasm32-unknown-emscripten` / `host` (x86_64, Windows/Linux)；矩阵 SoC：`esp32`, `esp32s3`, `esp32c3`, `esp32c6` |
| **工具链/SDK版本**| `ESP-IDF v5.1.3`（旧版取证基线；官方自 2020-07 起不再设 LTS 品牌，v5.1 系列支持期已于 **2025-12 EOL**）~ `v6.1+`（现行取证基线：v6.1 tag） |
| **计划状态** | 📋 待开始（已就绪，前置 M2 v2.4 100% 闭环） |
| **优先级** | 🔴 P0（收官里程碑与 CI 质量门禁） |
| **计划版本** | `v2.2` |
| **关联技术设计** | [`docs/zh/tech-designs/core/pal-i2c-v6-compatibility.md`](../../zh/tech-designs/core/pal-i2c-v6-compatibility.md) |
| **关联设计规范** | [`docs/zh/design/04-wasm-simulation/00-README.md`](../../zh/design/04-wasm-simulation/00-README.md)、[`02-wink-micro-os/`](../../zh/design/02-wink-micro-os/README.md) |
| **关联评审记录** | [`2026-09-22-esp-idf-simulation-interception-master-plan-review.md`](./2026-09-22-esp-idf-simulation-interception-master-plan-review.md) |
| **关联 ADR** | [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md)（负数错误码）、[ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)（静态分发与无虚表）、[ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)（合约诚实与降级登记）、[ADR-0014](../../decisions/unisim/0014-sim-single-virtual-core.md)（确定性调度）、[ADR-0045](../../decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md)（零 malloc 与静态池）、[ADR-0064](../../decisions/core/0064-target-capability-ssot.md)（目标平台与 SoC 能力 SSOT）、[ADR-0065](../../decisions/core/0065-pal-hardware-raii-resource-ownership.md)（禁门面 claim）、[ADR-0066](../../decisions/core/0066-pwm-basis-points-and-float-deprecation.md)（PWM 定点万分比）、[ADR-0070](../../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md)（Fiber 生命周期）、[ADR-0072](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md)（配额片强制切出）、[ADR-0080](../../decisions/core/0080-external-lint-pack-discovery-and-mcs51-guard-sinking.md)（外部 lint pack 发现）、[ADR-0082](../../decisions/core/0082-mcs51-reset-semantics-fiber-exit-and-reentry.md)（复位语义与重入）、[ADR-0083/0084](../../decisions/core/0083-adopt-gpl-3.0-only-license-policy.md)（开源许可分层）、[ADR-0085](../../decisions/core/0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md)（caps 双 SSOT 裁决） |
| **目标里程碑** | M3（SoC 矩阵扩展、Corpus CI 自动化、覆盖率与收官验收） |
| **前置依赖计划** | [`./2026-09-25-esp-idf-sim-m2-bus-plan.md`](./2026-09-25-esp-idf-sim-m2-bus-plan.md)（M2 v2.4 100% 验收闭环） |
| **继承计划** | 继承自 [`PLAN-20260922-ESP-IDF-SIM-MASTER`](./2026-09-22-esp-idf-simulation-interception-master-plan.md) (v3.5) |
| **计划负责人** | 仿真拦截专项小组 |
| **主要依赖技能** | `embedded-best-practice` |

---

## 2. 背景与目标（🔴 必选）

### 2.1 问题陈述

在 M0 ~ M2 里程碑中，ESP-IDF 仿真拦截层已成功完成骨架闭环、协作调度器（Task/Queue/Semaphore/EventGroup）、总线驱动双版本（I2C/UART/SPI/LEDC/GPTimer/NVS）以及系统级复位清洗链。
然而，在进入正式投产与开源发布之前，系统仍面临三大收官挑战：
1. **多 SoC 架构断层与越界穿透风险（ADR-0085）**：
   ESP32 家族涵盖架构与外设差异巨大的芯片变体：
   - **ESP32 经典**：40 号引脚空间（枚举 0~39；24、28~31 物理不存在，34~39 输入专用）、8 通道 LEDC（支持 High-Speed 模式）、2 个 I2C、3 个 UART；
   - **ESP32-S3**：45 个有效引脚（0~21、26~48；**22~25 物理不存在**，枚举上限 49）、8 通道 LEDC（**无 High-Speed 模式**）、2 个 I2C、3 个 UART；
   - **ESP32-C3**：RISC-V 架构，仅 22 引脚（GPIO 0~21）、6 通道 LEDC（**无 High-Speed 模式**）、**仅 1 个 I2C 控制器**、2 个 UART；
   - **ESP32-C6**：RISC-V 架构，31 引脚（GPIO 0~30）、6 通道 LEDC、I2C 2 个（HP+LP）与 UART 3 个（HP2+LP1）——**M3 门面按 HP-only 裁决暴露 1×I2C / 2×UART（见 §5.1 裁决项 4）**。
   若当前仅绑定 `chips/esp32`，用户或 AI 生成针对 C3/C6 的业务代码在仿真中请求 `GPIO_NUM_23` 或 `I2C_NUM_1` 时若不报错，烧录真实硬件将立即硬件崩盘。仿真拦截层必须在头文件闭包（编译期）与驱动门面（运行期 Fail-Loud）两个层面忠实复刻各 SoC 边界；门面头文件以全集枚举替代官方编译期 fail-loud 的有意偏离见 §5.1 裁决项 5。
2. **CI 语料与自动化质量闭环缺位（T-011；T-012 见目标 4 范围声明）**：
   当前语料和单测依赖本地手动命令行触发，缺乏 GitHub Actions 流水线自动化；未接线代码覆盖率（gcov/lcov）工具链，缺乏对门面核心路径（行覆盖率 ≥ 85%）的量化事实证明；缺乏跨平台 Headless 确定性回放证据链（T-008）。
3. **架构资产与治理收官**：
   缺乏多 SoC 编译期切换机制、完整的头文件闭包清单清单登记（`03-include-closure-inventory.md`）以及总纲各分级门禁终审签署。

### 2.2 技术/业务目标

- ✅ **目标 1：SoC 差异化硬件能力矩阵全覆盖 (ADR-0085)**：
  完整建立 `chips/esp32s3`、`chips/esp32c3`、`chips/esp32c6` 的头文件闭包（`soc_caps.h` 与 `gpio_num.h`）。在驱动门面（GPIO/LEDC/I2C/UART）中严格依据 `SOC_*` 宏校验边界。引脚越界（如 C3 下操作 GPIO 22+ 或在输入专用引脚上配置输出）及外设越界（如 C3 下使用 I2C 1、UART 2 或 High-Speed PWM）必须 100% Fail-Loud 拦截并返回 `ESP_ERR_INVALID_ARG`。
- ✅ **目标 2：Corpus 语料库接入 CI 全自动化**：
  在中央 `test/CMakeLists.txt` 中将 Tier-A 官方示例（`test/corpus/blink`、`test/corpus/ledc_basic`、`test/corpus/i2c_basic`，CTest 名 `esp_idf_corpus_blink/ledc/i2c`）与 Tier-B 语料（`test/corpus/legacy_i2c`，CTest 名 `esp_idf_corpus_legacy_i2c`）通过参数化矩阵统一接入 GitHub Actions，达成跨操作系统（Ubuntu Linux / Windows）与双编译目标（Host / Wasm）100% 自动绿灯构建。
- ✅ **目标 3：代码覆盖率接线与质量门禁（T-011）**：
  为 GCC/Clang Host 构建接线 `--coverage` 工具链，编写自动化覆盖率统计脚本 `tools/coverage.sh` 与门禁检查器 `tools/check_coverage.py`，核心门面代码行覆盖率达成 **≥ 85%** 指标。
- ✅ **目标 4：兼容性基线声明与回放证据链固化（T-008）**：
  显式声明 ESP-IDF `v5.1.3` 与 `v6.1+` 兼容性基线及全部有意偏离清单（§5.1 裁决项 4、5）；固化跨平台 Headless 仿真测试脚本 `test_esp_idf_headless_replay.py`，输出 bit-exact 确定性回放哈希。
  > 📌 **范围声明（v2.2）**：Nightly IDF 版本矩阵（T-012）**不在本计划交付**——本计划 CI 不含 `espressif/idf` 容器 job，`nightly.yml` 维持 `espressif/idf:v5.4 … || true` 现状（总纲 R-008 遗留），移交后续 Nightly 专项承接。
- ✅ **目标 5：收官验收与总纲结项**：
  完成 L0~L4 全量分级验收，更新 `01` 架构指南、`02` API 矩阵与 `03` 闭包清单，正式宣布 `frameworks/esp_idf` 源码级仿真拦截层顺利收官。

### 2.3 成功指标（验收出口）

| 指标 | 通过标准 | 验证方法 |
|:---|:---|:---|
| **SoC 差异化拦截单测** | ESP32-C3 越界引脚（GPIO 22+）、C3/S3 High-Speed LEDC 模式、C3/C6 I2C 端口 1 越界、C3 UART 2 越界 100% 拦截报 `ESP_ERR_INVALID_ARG` | `ctest -R test_esp_soc_matrix` |
| **多 SoC 交叉编译验证** | CMake 参数 `-DWINK_ESP_TARGET=esp32s3/esp32c3/esp32c6` 切换编译通过率 100% (0 error, 0 warning) | CI 矩阵构建 job |
| **CI 语料全量构建** | 全部 4 组官方语料在 CI 环境中 compile-only 100% 通过 | `ctest -R esp_idf_corpus` |
| **测试代码覆盖率** | `frameworks/esp_idf/src/` 核心行覆盖率 (Line Coverage) ≥ **85%** | `python wink-micro-os/frameworks/esp_idf/tools/check_coverage.py build_cov/coverage_filtered.info 85` |
| **确定性回放哈希** | 连续 3 次 Headless 运行记录事件哈希 bit-exact 完全一致 | `python wink-micro-os/frameworks/esp_idf/test/headless/test_esp_idf_headless_replay.py <path_to_test_binary>` |
| **Wasm 峰值内存采样** | Wasm 内存采样严格满足配额（栈深 < 768KB，全局堆段 < 16MB） | Emscripten 构建分析与单测断言 |
| **许可与架构门禁** | `check_license_map.py` 100% 绿；`winkcli lint --pack esp_idf_all` 0 findings | GitHub Actions 门禁任务 |

### 2.4 物理学与电气连续域不可逆断层声明（🔴 平台真实性边界）

> ⚠️ **SSOT 声明（对齐 ADR-0012 合约诚实原则）**：
> WinkMicroOS 仿真体系提供 **“C-ABI 源码级 100% 契约保真与行为级高保真”**。
> 本平台在物理世界与多 SoC 差异层面存在以下**不可逆断层**，开发人员与 AI 代码生成链路必须知悉：
> 1. **无底层 CPU 指令集架构微结构差异仿真**：宿主统一执行编译出的 x86_64 或 wasm32 指令，不模拟 Xtensa 双核 Windowed 寄存器溢出中断时序，亦不模拟 RISC-V 单核特权模式切换开销。
> 2. **无不同 SoC 引脚驱动强度与压摆率（Slew Rate）差异**：不模拟 ESP32 与 ESP32-C3 在不同 IO MUX 配置下的物理毫安级灌电流与上拉电阻阻值漂移。
> 3. **无射频与无线电物理介质仿真**：芯片内部 Wi-Fi/BLE 物理基带被抽象为离散数据报网络套接字，不存在物理天线阻抗匹配、多径衰落或高频噪声畸变。

---

## 3. 变更范围与影响分析（🔴 必选）

### 3.1 文件变更清单

| 文件路径 | 变更类型 | 说明 |
|:---|:---:|:---|
| `wink-micro-os/frameworks/esp_idf/chips/esp32s3/include/soc/soc_caps.h` | 🆕 新增 | ESP32-S3 原生硬件能力宏（45 有效引脚、枚举上限 49、无高速 LEDC、2×I2C、3×UART） |
| `wink-micro-os/frameworks/esp_idf/chips/esp32s3/include/soc/gpio_num.h` | 🆕 新增 | ESP32-S3 原生引脚枚举定义（`GPIO_NUM_0` ~ `GPIO_NUM_48`，**跳过物理不存在的 22~25**） |
| `wink-micro-os/frameworks/esp_idf/chips/esp32c3/include/soc/soc_caps.h` | 🆕 新增 | ESP32-C3 原生硬件能力宏（22 引脚、6 通道 LEDC、1×I2C、2×UART） |
| `wink-micro-os/frameworks/esp_idf/chips/esp32c3/include/soc/gpio_num.h` | 🆕 新增 | ESP32-C3 原生引脚枚举定义（`GPIO_NUM_0` ~ `GPIO_NUM_21`） |
| `wink-micro-os/frameworks/esp_idf/chips/esp32c6/include/soc/soc_caps.h` | 🆕 新增 | ESP32-C6 原生硬件能力宏（31 引脚、6 通道 LEDC、HP-only 1×I2C/2×UART；官方另有 LP ×1，见 §5.1 裁决项 4） |
| `wink-micro-os/frameworks/esp_idf/chips/esp32c6/include/soc/gpio_num.h` | 🆕 新增 | ESP32-C6 原生引脚枚举定义（`GPIO_NUM_0` ~ `GPIO_NUM_30`） |
| `wink-micro-os/frameworks/esp_idf/include/soc/soc_caps.h` | 🗑 删除 | ADR-0085 D3 修订/ADR-0087：数据归属 `chips/<target>`，共享树不再保留同名文件（2026-09-25 已删，禁止恢复） |
| `wink-micro-os/frameworks/esp_idf/include/soc/gpio_num.h` | 🗑 删除 | 同上 |
| `wink-micro-os/frameworks/esp_idf/channels.json` | ✏️ 修改 | 登记新增 SoC 的 `chips_handwritten`（或收割转正后移除），门禁强制（ADR-0087） |
| `wink-micro-os/frameworks/esp_idf/src/drivers/esp_ledc.c` | ✏️ 修改 | LEDC 高速模式校验改为使用 `SOC_LEDC_SUPPORT_HS_MODE` 宏 |
| `wink-micro-os/frameworks/esp_idf/src/drivers/esp_i2c_legacy.c` | ✏️ 修改 | I2C 端口上限增加 `SOC_HP_I2C_NUM` 宏校验 |
| `wink-micro-os/frameworks/esp_idf/src/drivers/esp_i2c_master.c` | ✏️ 修改 | I2C 端口上限与自动选取增加 `SOC_HP_I2C_NUM` 宏校验 |
| `wink-micro-os/frameworks/esp_idf/src/drivers/esp_uart.c` | ✏️ 修改 | UART 控制器端口增加 `SOC_UART_HP_NUM` 宏校验 |
| `wink-micro-os/frameworks/esp_idf/src/drivers/esp_spi.c` | ✏️ 修改 | SPI 控制器端口增加 `SOC_SPI_PERIPH_NUM` 宏校验（C3/C6 仅 2 个 SPI） |
| `wink-micro-os/frameworks/esp_idf/esp_idf_sources.cmake` | ✏️ 修改 | 从 `WINK_ESP_TARGET` 推导 `CONFIG_IDF_TARGET_*` 宏名（仅推导，注入位于 `frameworks/esp_idf/CMakeLists.txt`，同目录作用域） |
| `wink-micro-os/frameworks/esp_idf/CMakeLists.txt` | ✏️ 修改 | 在 `wink_framework_esp_idf` 目标上 `PUBLIC` 注入 `CONFIG_IDF_TARGET_*` 编译宏（传播至测试与语料目标） |
| `wink-micro-os/frameworks/esp_idf/test/corpus/legacy_i2c/include/sdkconfig.h` | ✏️ 修改 | 清理硬编码 `CONFIG_IDF_TARGET_ESP32` 与 `SOC_HP_I2C_NUM 2`，防止 C3/C6 矩阵 job `-Werror` 重定义冲突 |
| `wink-micro-os/frameworks/esp_idf/test/core/test_esp_soc_matrix.c` | 🆕 新增 | 多 SoC 矩阵能力与越界拦截单元测试套件（含 SPI 端口越界） |
| `wink-micro-os/frameworks/esp_idf/tools/coverage.sh` | 🆕 新增 | Linux/macOS 覆盖率数据收集与 HTML 生成脚本 |
| `wink-micro-os/frameworks/esp_idf/tools/check_coverage.py` | 🆕 新增 | 覆盖率阈值断言工具（≥ 85% 门禁检查器） |
| `wink-micro-os/frameworks/esp_idf/test/headless/test_esp_idf_headless_replay.py` | 🆕 新增 | 跨平台 Headless 仿真确定性回放验证脚本 |
| `wink-micro-os/test/CMakeLists.txt` | ✏️ 修改 | 注册 `test_esp_soc_matrix`、覆盖率开关及 CI 语料目标 |
| `wink-micro-os/frameworks/esp_idf/docs/01-architecture-and-governance-guide.md` | ✏️ 修改 | 固化收官架构指南与多 SoC 切换规则 |
| `wink-micro-os/frameworks/esp_idf/docs/02-api-coverage-matrix.md` | ✏️ 修改 | 终审更新全量 API 状态与 M3 覆盖率基线 |
| `wink-micro-os/frameworks/esp_idf/docs/03-include-closure-inventory.md` | ✏️ 修改 | 登记新增 6 个 SoC 硬件头文件条目 |
| `.github/workflows/esp_idf_ci.yml` | 🆕 新增 | ESP-IDF 仿真拦截层 GitHub Actions 自动化流水线 |

### 3.2 接口影响分析

| 接口层 | 是否有破坏性变更 | 影响范围 | 备注 |
|:---|:---:|:---|:---|
| PAL 公开 API | ❌ 否 | 无 | 严格只读依赖既有 HAL/OSAL，不修改任何 PAL 接口 |
| DAL 层 | ❌ 否 | 无 | 门面直接消费 PAL，不侵入 DAL 语义器件 |
| 现有 ESP32 应用 | ❌ 否 | 无 | 默认 `WINK_ESP_TARGET=esp32` 保持 100% 行为完全向后兼容 |
| 构建系统 | ⚠️ 是 | CMake 参数扩展 | 支持 `-DWINK_ESP_TARGET=esp32s3/c3/c6` 参数化配置 |
| 文档 | ⚠️ 是 | docs/ 体系 | 同步更新 01 架构、02 矩阵、03 闭包及总纲结项声明 |

### 3.3 架构红线继承（总纲 §8）

> 🚨 **严格执行 7 条架构红线（总纲 §8）**：
> 1. 🚨 **C-ABI 与纯 C 实现原则**：标准 C99 编写，严禁 C++ 运行时与类异常。
> 2. 🚨 **严禁侵入式修改 PAL / DAL**：只依赖 `pal/include` 既有能力。
> 3. 🚨 **严格遵守 ADR-0065**：门面层**严禁调用 `pal_resource_claim()`**。
> 4. 🚨 **零运行期堆分配（Zero Runtime Malloc）与静态预算控制**：严格锁死静态对象池，全局静态数据段严格受控于 `< 14KB` 标称线。
> 5. 🚨 **PWM 定点红线（ADR-0066）**：纯整数定点计算（0..10000 BP），严禁浮点运算。
> 6. 🚨 **合约诚实（ADR-0012）**：芯片不支持的能力（如 C3 的第二个 I2C 控制器、C3/C6/S3 的 High-Speed PWM）一律 Fail-Loud（返回 `ESP_ERR_INVALID_ARG` 或编译期报错），严禁静默成功。
> 7. 🚨 **开源许可合规（ADR-0083/0084）**：
>    - `frameworks/esp_idf/{src,include,chips}` = **`LGPL-3.0-only`**；
>    - `frameworks/esp_idf/test/**` = **`GPL-3.0-only`**；
>    - 提交前 `check_license_map.py` 必须 100% 匹配。

### 3.4 系统资源与并发约束评估

| 维度 | 预计开销 / 限制 | 风险分析 | 应对策略 |
|:---|:---|:---|:---|
| **ROM / Flash 占用** | 增量仅为几组预编译头文件与常量宏，二进制增量 < 1KB | 极低 | 头文件全为宏与类型枚举，不产生冗余数据段 |
| **RAM (静态/全局)** | 增量 **0 字节**（复用 M2 既有静态池） | 静态预算失控风险已闭环 | M2 加固已将 NVS 内存压缩至 3KB，框架总静态段严格受控在 12KB 内 |
| **栈深度 (Stack)** | 单次调用 < 256 字节 | 栈溢出风险 | 门面参数校验后直接下沉调用，无大局部数组 |
| **Wasm 峰值内存** | 调度器 Fiber 协程栈 < 768KB，Wasm 线性内存 < 16MB | 浏览器沙箱 OOM | CI 自动化脚本加入 Wasm 堆栈水位校验 |

---

## 4. 依赖与风险（🔴 必选）

### 4.1 前置依赖

| 依赖 ID | 依赖内容 | 是否阻塞 | 状态 | 备注 |
|:---|:---|:---:|:---:|:---|
| **D-001** | `targets/common/wink_sim_scheduler.h` 调度器接口 | ✅ 是 | ✅ 已就绪 | ADR-0014 既有能力 |
| **D-002** | M2 核心总线驱动双版本与定点 PWM | ✅ 是 | ✅ 100% 验收闭环 | M2 v2.4 交付闭环（28/28 CTest 全绿） |
| **D-003** | caps 双 SSOT 裁决 | ✅ 是 | ✅ 已完成 | ADR-0085 Accepted |
| **D-004** | 外部 lint pack 发现机制 | ✅ 是 | ✅ 已就绪 | ADR-0080 既有能力 |

### 4.2 外部依赖

| 依赖 ID | 依赖内容 | 提供方 | 风险等级 | 备注 |
|:---|:---|:---|:---:|:---|
| **E-001** | ESP-IDF `v5.1.3` 与 `v6.1` 官方源码树 | Espressif | 🟡 中 | S3/C3/C6 `soc_caps.h` / `gpio_types.h` 双版本取证真值源（默认以 v6.1 为准，差异登记于 §5.1 裁决项 4） |
| **E-002** | `lcov` / `genhtml` 覆盖率工具链 | Linux / MSYS2 | 🟡 中 | 产出覆盖率 HTML 报告 |
| **E-003** | GitHub Actions 运行环境 | GitHub | 🟡 中 | 执行多平台 CI 流水线 |

### 4.3 风险登记册

| 风险 ID | 风险描述 | 概率 | 影响 | 严重度 | 缓解措施 | 责任人 | 触发条件 |
|:---|:---|:---:|:---:|:---:|:---|:---|:---|
| **R-001** | 单构建树下多 SoC 引脚宏无法同时测试 | 🟠 高 | 🟠 高 | 6 | 驱动层源码与测试代码均依赖 `CONFIG_IDF_TARGET_*` 编译宏展开。采用 CI 矩阵策略（通过 `-DWINK_ESP_TARGET=xxx` 独立完整编译驱动与单测），单次构建产出对应 SoC 的 `test_esp_soc_matrix`，由 CI matrix 并行执行覆盖 4 芯片变体 | 专项小组 | 驱动与测试宏展开冲突 |
| **R-002** | MSVC 与 GCC 覆盖率生成格式不一致 | 🟡 中 | 🟡 中 | 4 | 覆盖率收集以 Ubuntu Linux CI (GCC/lcov) 为权威事实源，Windows 本地作为可选验证 | 专项小组 | Windows 本地生成覆盖率 |
| **R-003** | CI 语料构建时间过长导致流水线超时 | 🟡 中 | 🟡 中 | 4 | 语料采用 compile-only（`-c` 目标文件形态），禁止整包全链接，单用例编译控制在 2 秒内 | 专项小组 | 多语料同时接入 CI |
| **R-004** | C3/C6 单 I2C 控制器导致 Legacy 语料断言失配 | 🟡 中 | 🟠 高 | 6 | 在门面中将 `i2c_num >= SOC_HP_I2C_NUM` 纳入统一参数合法性校验，C3 下尝试初始化 Port 1 时如实抛错（C6 按 §5.1 裁决项 4 的 HP-only 裁决同样拦截） | 专项小组 | 语料在 C3 下请求 I2C 1 |

---

## 5. 优先级路线图与展开前置约束裁决（SSOT 事实源）

### 5.1 展开前置约束终审裁决

1. **裁决项 1：多 SoC 硬件能力真值表（默认对齐 ESP-IDF v6.1 官方事实源；与 v5.1.3 的差异及有意偏离显式登记于裁决项 4、5）**
   - **ESP32 经典**：40 号引脚空间（枚举 0..39；官方掩码排除 24、28~31 → 有效 35 个，34~39 输入专用 → 输出掩码 29 个）；LEDC 8 通道（**支持 High-Speed**）；I2C 2 端口；UART 3 端口；SPI 3 控制器。
   - **ESP32-S3**：45 个有效引脚（0..21、26..48；**22~25 物理不存在，官方掩码与枚举同步跳过，v5.1.3/v6.1 一致**；`GPIO_NUM_MAX = 49`；无纯输入引脚限制）；LEDC 8 通道（**不支持 High-Speed**）；I2C 2 端口；UART 3 端口；SPI 3 控制器。
   - **ESP32-C3**：22 引脚（0..21）；LEDC 6 通道（**不支持 High-Speed**）；**I2C 仅 1 端口 (`I2C_NUM_0`)**；UART 2 端口；SPI 2 控制器。
   - **ESP32-C6**：31 引脚（0..30）；LEDC 6 通道（**不支持 High-Speed**）；**I2C：HP 1 端口（`I2C_NUM_0`），硬件另有 LP I2C ×1 —— M3 按 HP-only 裁决暴露（裁决项 4）**；**UART：HP 2 端口，硬件另有 LP UART ×1 —— 同前裁决**；SPI 2 控制器。
2. **裁决项 2：多 SoC 构建与测试隔离设计（根治 R-001，CI 矩阵独立编译）**
   - 构建系统默认 `WINK_ESP_TARGET=esp32`，支持传参指定 `esp32s3/esp32c3/esp32c6`；
   - 驱动库 `wink_framework_esp_idf` 与测试目标均注入统一的 `CONFIG_IDF_TARGET_*` 编译宏，保证源码与头文件宏环境绝对一致；
   - 单次构建产出统一的 `test_esp_soc_matrix` 目标；
   - CI 流水线采用 Matrix 并行策略（`target: [esp32, esp32s3, esp32c3, esp32c6]`），完整覆盖 4 芯片变体的驱动编译与断言拦截。
3. **裁决项 3：覆盖率门禁基准线**
   - 覆盖率统计仅限定于 `frameworks/esp_idf/src/`（排除 `test/` 与第三方库）；行覆盖率目标为 **≥ 85%**。
4. **裁决项 4：C6 LP 外设 HP-only 有意偏离登记（ADR-0012 合约诚实）**
   - **官方事实（双版本）**：v6.1 C6 为 `SOC_I2C_NUM (2U)`（HP1+LP1）、`SOC_HP_I2C_NUM (1U)`、`SOC_LP_I2C_NUM (1U)`、`SOC_UART_NUM (3)`（HP2+LP1）、`SOC_UART_HP_NUM (2)`，且 `SOC_LP_I2C_SUPPORTED 1`（**硬件确有 LP I2C**）；v5.1.3 C6 为 `SOC_I2C_NUM (1U)`、`SOC_UART_NUM (2)`（尚无 HP/LP 拆分宏）。
   - **裁决**：M3 门面按 **HP-only** 暴露（`SOC_I2C_NUM = 1`、`SOC_UART_NUM = 2`、`SOC_HP_I2C_NUM = 1`、`SOC_UART_HP_NUM = 2`、`SOC_LP_I2C_NUM = 0`），运行期对 Port 1 / `UART_NUM_2` Fail-Loud。该取值**与 v5.1.3 官方一致**，相对 v6.1 为**有意偏离**（LP 外设语义不在 M3 范围），依据 ADR-0012 登记于此，禁止在任何注释/真值表中声称"C6 硬件不支持 LP I2C"。
   - **后续演进**：LP 外设支持作为独立演进项落地时，回正至 v6.1 官方值并同步更新真值表与 `TC-SOC-03`/`TC-SOC-06` 断言方向。
5. **裁决项 5：门面头文件全集枚举/宏 vs 官方条件编译（编译期 fail-loud）有意偏离登记**
   - **官方事实**：`SPI3_HOST` 仅在 `SOC_SPI_PERIPH_NUM > 2` 时定义（C3/C6 下引用即**编译期报错**）；`UART_NUM_2` 仅在 `SOC_UART_NUM > 2` 时定义；`LEDC_HIGH_SPEED_MODE` 仅在 `SOC_LEDC_SUPPORT_HS_MODE` 时定义；soc_caps 对 false 能力采取"**不定义该宏**"约定（而非 `#define x 0`）。
   - **裁决**：门面 `include/hal/{spi,uart,ledc}_types.h` 保持全集枚举/宏（保既有代码与 `TC-SOC-07` 中 `SPI3_HOST` 字面量可编译），SoC 约束一律下沉为**运行期** Fail-Loud（`soc_matrix` 单测覆盖）；`chips/{esp32s3,esp32c3,esp32c6}/soc_caps.h` 对 `SOC_LEDC_SUPPORT_HS_MODE` 显式 `#define … 0`（因 ESP32 侧既有头定义为 `1` 且驱动以 `#if !SOC_LEDC_SUPPORT_HS_MODE` 取反判断）。上述均为对官方编译期 fail-loud 的**有意替代**，依据 ADR-0012 登记。

### 5.2 任务拆分与工时矩阵

```mermaid
graph TD
    M3_1[Task M3-1: SoC 矩阵扩展 S3/C3/C6 与越界拦截单测]
    M3_2[Task M3-2: 官方语料 CI 流水线集成与覆盖率工具接线]
    M3_3[Task M3-3: 跨平台 Headless 证据链固化与收官总结]

    M3_1 --> M3_3
    M3_2 --> M3_3
```

| 任务 ID | 任务标题 | 优先级 | 预估工时 | 涉及关键文件 |
|:---|:---|:---:|:---:|:---|
| **Task M3-1** | SoC 硬件能力矩阵扩展 (S3/C3/C6) 与 Fail-Loud 校验 | 🔴 P0 | 10 h | `chips/{esp32s3,esp32c3,esp32c6}/**`, `include/soc/**`, `src/drivers/**`, `test/core/test_esp_soc_matrix.c` |
| **Task M3-2** | 官方语料 CI 全自动化与覆盖率工具接线 (T-011) | 🔴 P0 | 12 h | `test/CMakeLists.txt`, `tools/coverage.sh`, `tools/check_coverage.py`, `.github/workflows/esp_idf_ci.yml` |
| **Task M3-3** | 跨平台 Headless 证据链固化与总纲结项收官 | 🔴 P0 | 8 h | `test/headless/test_esp_idf_headless_replay.py`, `docs/*` 矩阵与指南更新 |
| **总计** | | | **30 h** | （关键路径约为 **18 h**） |

---

## 6. 详细任务拆分与代码实现设计（🔴 必选）

---

### Task M3-1：SoC 硬件能力矩阵扩展 (S3/C3/C6) 与 Fail-Loud 校验 `[ 状态: 📋 待开始 ]`

| 字段 | 内容 |
|:---|:---|
| **负责人** | 仿真拦截专项小组 |
| **预估工时** | 10 小时 |
| **优先级** | 🔴 P0 |
| **前置依赖** | M2 闭环交付 |
| **修改文件** | `chips/esp32s3/**`, `chips/esp32c3/**`, `chips/esp32c6/**`, `include/soc/**`, `src/drivers/esp_ledc.c`, `src/drivers/esp_i2c_*.c`, `src/drivers/esp_uart.c`, `test/core/test_esp_soc_matrix.c`, `test/CMakeLists.txt` |
| **接口变化** | 补全多 SoC 硬件能力头文件闭包，各驱动门面全面遵从芯片原生 `SOC_*` 宏拦截越界资源 |

#### 详细步骤与代码级设计

- [ ] **Step 1：建立 `chips/esp32s3` 头文件闭包**
  - 新建 `wink-micro-os/frameworks/esp_idf/chips/esp32s3/include/soc/soc_caps.h`：
    ```c
    /* SPDX-License-Identifier: LGPL-3.0-only */
    #ifndef SOC_CAPS_ESP32S3_H_
    #define SOC_CAPS_ESP32S3_H_

    #define SOC_GPIO_PIN_COUNT          49

    /* S3: 官方有效引脚 45 个（0~21、26~48）；GPIO 22~25 物理不存在，
     * 官方掩码显式排除（v5.1.3 / v6.1 一致），枚举同步跳过 22~25。
     * 模组级 Flash/PSRAM 占用（26~32）属模组差异，不在芯片级拦截范围。 */
    #define SOC_GPIO_VALID_GPIO_MASK        \
        (0x1FFFFFFFFFFFFULL & ~(0ULL | (1ULL<<22) | (1ULL<<23) | (1ULL<<24) | (1ULL<<25)))
    #define SOC_GPIO_VALID_OUTPUT_GPIO_MASK (SOC_GPIO_VALID_GPIO_MASK)

    #define GPIO_IS_VALID_GPIO(gpio_num) \
        ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 49) && \
         (((1ULL << (gpio_num)) & SOC_GPIO_VALID_GPIO_MASK) != 0))

    #define GPIO_IS_VALID_OUTPUT_GPIO(gpio_num) \
        ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 49) && \
         (((1ULL << (gpio_num)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))

    /* LEDC capabilities */
    #define SOC_LEDC_SUPPORTED          1
    #define SOC_LEDC_SUPPORT_HS_MODE    0 /* S3 无 High-Speed 模式；官方惯例为不定义，门面显式 =0 供取反判断（§5.1 裁决项 5） */
    #define SOC_LEDC_TIMER_NUM          4
    #define SOC_LEDC_CHANNEL_NUM        8
    #define SOC_LEDC_TIMER_BIT_WIDTH    14

    /* I2C / UART / SPI capabilities */
    #define SOC_I2C_SUPPORTED           1
    #define SOC_I2C_NUM                 2
    #define SOC_HP_I2C_NUM              2
    #define SOC_UART_SUPPORTED          1
    #define SOC_UART_NUM                3
    #define SOC_UART_HP_NUM             3
    #define SOC_GPTIMER_SUPPORTED       1
    #define SOC_SPI_PERIPH_NUM          3

    #endif /* SOC_CAPS_ESP32S3_H_ */
    ```

  - 新建 `wink-micro-os/frameworks/esp_idf/chips/esp32s3/include/soc/gpio_num.h`：
    ```c
    /* SPDX-License-Identifier: LGPL-3.0-only */
    #ifndef SOC_GPIO_NUM_ESP32S3_H_
    #define SOC_GPIO_NUM_ESP32S3_H_

    #ifdef __cplusplus
    extern "C" {
    #endif

    typedef enum {
        GPIO_NUM_NC = -1,
        GPIO_NUM_0 = 0,   GPIO_NUM_1 = 1,   GPIO_NUM_2 = 2,   GPIO_NUM_3 = 3,
        GPIO_NUM_4 = 4,   GPIO_NUM_5 = 5,   GPIO_NUM_6 = 6,   GPIO_NUM_7 = 7,
        GPIO_NUM_8 = 8,   GPIO_NUM_9 = 9,   GPIO_NUM_10 = 10, GPIO_NUM_11 = 11,
        GPIO_NUM_12 = 12, GPIO_NUM_13 = 13, GPIO_NUM_14 = 14, GPIO_NUM_15 = 15,
        GPIO_NUM_16 = 16, GPIO_NUM_17 = 17, GPIO_NUM_18 = 18, GPIO_NUM_19 = 19,
        GPIO_NUM_20 = 20, GPIO_NUM_21 = 21, /* 22~25 物理不存在，官方枚举跳过 */
        GPIO_NUM_26 = 26, GPIO_NUM_27 = 27, GPIO_NUM_28 = 28, GPIO_NUM_29 = 29,
        GPIO_NUM_30 = 30, GPIO_NUM_31 = 31, GPIO_NUM_32 = 32, GPIO_NUM_33 = 33,
        GPIO_NUM_34 = 34, GPIO_NUM_35 = 35, GPIO_NUM_36 = 36, GPIO_NUM_37 = 37,
        GPIO_NUM_38 = 38, GPIO_NUM_39 = 39, GPIO_NUM_40 = 40, GPIO_NUM_41 = 41,
        GPIO_NUM_42 = 42, GPIO_NUM_43 = 43, GPIO_NUM_44 = 44, GPIO_NUM_45 = 45,
        GPIO_NUM_46 = 46, GPIO_NUM_47 = 47, GPIO_NUM_48 = 48,
        GPIO_NUM_MAX = 49,
    } gpio_num_t;

    #ifdef __cplusplus
    }
    #endif

    #endif /* SOC_GPIO_NUM_ESP32S3_H_ */
    ```

- [ ] **Step 2：建立 `chips/esp32c3` 头文件闭包**
  - 新建 `wink-micro-os/frameworks/esp_idf/chips/esp32c3/include/soc/soc_caps.h`：
    ```c
    /* SPDX-License-Identifier: LGPL-3.0-only */
    #ifndef SOC_CAPS_ESP32C3_H_
    #define SOC_CAPS_ESP32C3_H_

    #define SOC_GPIO_PIN_COUNT          22

    /* C3: 22 个引脚 (0..21) 全部有效并可输出 */
    #define SOC_GPIO_VALID_GPIO_MASK        (0x3FFFFFULL)
    #define SOC_GPIO_VALID_OUTPUT_GPIO_MASK (SOC_GPIO_VALID_GPIO_MASK)

    #define GPIO_IS_VALID_GPIO(gpio_num) \
        ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 22) && \
         (((1ULL << (gpio_num)) & SOC_GPIO_VALID_GPIO_MASK) != 0))

    #define GPIO_IS_VALID_OUTPUT_GPIO(gpio_num) \
        ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 22) && \
         (((1ULL << (gpio_num)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))

    /* LEDC capabilities */
    #define SOC_LEDC_SUPPORTED          1
    #define SOC_LEDC_SUPPORT_HS_MODE    0 /* C3 无 High-Speed 模式；官方惯例为不定义，门面显式 =0（§5.1 裁决项 5） */
    #define SOC_LEDC_TIMER_NUM          4
    #define SOC_LEDC_CHANNEL_NUM        6 /* C3 仅 6 个通道 (0..5) */
    #define SOC_LEDC_TIMER_BIT_WIDTH    14

    /* I2C / UART / SPI capabilities */
    #define SOC_I2C_SUPPORTED           1
    #define SOC_I2C_NUM                 1 /* ⚠️ C3 仅 1 个 I2C 控制器 */
    #define SOC_HP_I2C_NUM              1
    #define SOC_UART_SUPPORTED          1
    #define SOC_UART_NUM                2 /* ⚠️ C3 仅 2 个 UART 控制器 */
    #define SOC_UART_HP_NUM             2
    #define SOC_GPTIMER_SUPPORTED       1
    #define SOC_SPI_PERIPH_NUM          2 /* ⚠️ C3 仅 2 个 SPI */

    #endif /* SOC_CAPS_ESP32C3_H_ */
    ```

  - 新建 `wink-micro-os/frameworks/esp_idf/chips/esp32c3/include/soc/gpio_num.h`：
    ```c
    /* SPDX-License-Identifier: LGPL-3.0-only */
    #ifndef SOC_GPIO_NUM_ESP32C3_H_
    #define SOC_GPIO_NUM_ESP32C3_H_

    #ifdef __cplusplus
    extern "C" {
    #endif

    typedef enum {
        GPIO_NUM_NC = -1,
        GPIO_NUM_0 = 0,   GPIO_NUM_1 = 1,   GPIO_NUM_2 = 2,   GPIO_NUM_3 = 3,
        GPIO_NUM_4 = 4,   GPIO_NUM_5 = 5,   GPIO_NUM_6 = 6,   GPIO_NUM_7 = 7,
        GPIO_NUM_8 = 8,   GPIO_NUM_9 = 9,   GPIO_NUM_10 = 10, GPIO_NUM_11 = 11,
        GPIO_NUM_12 = 12, GPIO_NUM_13 = 13, GPIO_NUM_14 = 14, GPIO_NUM_15 = 15,
        GPIO_NUM_16 = 16, GPIO_NUM_17 = 17, GPIO_NUM_18 = 18, GPIO_NUM_19 = 19,
        GPIO_NUM_20 = 20, GPIO_NUM_21 = 21,
        GPIO_NUM_MAX = 22,
    } gpio_num_t;

    #ifdef __cplusplus
    }
    #endif

    #endif /* SOC_GPIO_NUM_ESP32C3_H_ */
    ```

- [ ] **Step 3：建立 `chips/esp32c6` 头文件闭包**
  - 新建 `wink-micro-os/frameworks/esp_idf/chips/esp32c6/include/soc/soc_caps.h`：
    ```c
    /* SPDX-License-Identifier: LGPL-3.0-only */
    #ifndef SOC_CAPS_ESP32C6_H_
    #define SOC_CAPS_ESP32C6_H_

    #define SOC_GPIO_PIN_COUNT          31

    /* C6: 31 个引脚 (0..30) 全部有效并可输出 */
    #define SOC_GPIO_VALID_GPIO_MASK        (0x7FFFFFFFULL)
    #define SOC_GPIO_VALID_OUTPUT_GPIO_MASK (SOC_GPIO_VALID_GPIO_MASK)

    #define GPIO_IS_VALID_GPIO(gpio_num) \
        ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 31) && \
         (((1ULL << (gpio_num)) & SOC_GPIO_VALID_GPIO_MASK) != 0))

    #define GPIO_IS_VALID_OUTPUT_GPIO(gpio_num) \
        ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 31) && \
         (((1ULL << (gpio_num)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))

    /* LEDC capabilities */
    #define SOC_LEDC_SUPPORTED          1
    #define SOC_LEDC_SUPPORT_HS_MODE    0 /* C6 无 High-Speed 模式；官方惯例为不定义，门面显式 =0（§5.1 裁决项 5） */
    #define SOC_LEDC_TIMER_NUM          4
    #define SOC_LEDC_CHANNEL_NUM        6 /* C6 6 个通道 */
    #define SOC_LEDC_TIMER_BIT_WIDTH    20 /* C6 官方支持 20-bit 定时器计数器 */

    /* I2C / UART / SPI capabilities */
    #define SOC_I2C_SUPPORTED           1
    #define SOC_I2C_NUM                 1 /* HP-only 裁决：v5.1.3 官方=1U；v6.1 官方=2U（HP1+LP1）。见 §5.1 裁决项 4 */
    #define SOC_HP_I2C_NUM              1
    #define SOC_UART_SUPPORTED          1
    #define SOC_UART_NUM                2 /* HP-only 裁决：v5.1.3 官方=2；v6.1 官方=3（HP2+LP1）。见 §5.1 裁决项 4 */
    #define SOC_UART_HP_NUM             2
    #define SOC_GPTIMER_SUPPORTED       1
    #define SOC_SPI_PERIPH_NUM          2
    #define SOC_LP_I2C_NUM              0 /* 官方 v6.1=1U（硬件确有 LP I2C）；M3 HP-only 裁决不暴露。见 §5.1 裁决项 4 */

    #endif /* SOC_CAPS_ESP32C6_H_ */
    ```

  - 新建 `wink-micro-os/frameworks/esp_idf/chips/esp32c6/include/soc/gpio_num.h`：
    ```c
    /* SPDX-License-Identifier: LGPL-3.0-only */
    #ifndef SOC_GPIO_NUM_ESP32C6_H_
    #define SOC_GPIO_NUM_ESP32C6_H_

    #ifdef __cplusplus
    extern "C" {
    #endif

    typedef enum {
        GPIO_NUM_NC = -1,
        GPIO_NUM_0 = 0,   GPIO_NUM_1 = 1,   GPIO_NUM_2 = 2,   GPIO_NUM_3 = 3,
        GPIO_NUM_4 = 4,   GPIO_NUM_5 = 5,   GPIO_NUM_6 = 6,   GPIO_NUM_7 = 7,
        GPIO_NUM_8 = 8,   GPIO_NUM_9 = 9,   GPIO_NUM_10 = 10, GPIO_NUM_11 = 11,
        GPIO_NUM_12 = 12, GPIO_NUM_13 = 13, GPIO_NUM_14 = 14, GPIO_NUM_15 = 15,
        GPIO_NUM_16 = 16, GPIO_NUM_17 = 17, GPIO_NUM_18 = 18, GPIO_NUM_19 = 19,
        GPIO_NUM_20 = 20, GPIO_NUM_21 = 21, GPIO_NUM_22 = 22, GPIO_NUM_23 = 23,
        GPIO_NUM_24 = 24, GPIO_NUM_25 = 25, GPIO_NUM_26 = 26, GPIO_NUM_27 = 27,
        GPIO_NUM_28 = 28, GPIO_NUM_29 = 29, GPIO_NUM_30 = 30,
        GPIO_NUM_MAX = 31,
    } gpio_num_t;

    #ifdef __cplusplus
    }
    #endif

    #endif /* SOC_GPIO_NUM_ESP32C6_H_ */
    ```

- [ ] **Step 4：落地 ADR-0087 —— per-SoC 数据归属 `chips/<target>`（无分发层）**

  > ⚠️ **2026-09-25 修订（ADR-0085 D3 修订 / ADR-0087）**：数据物理归属 `chips/<target>/include/soc/`，
  > 共享 `include/soc/` 不得再有同名文件；选片由 CMake include 顺序（`esp_idf_target.cmake` 单源）完成，
  > **禁止 `#include_next`**（MSVC 不可移植）。esp32 数据已按字节迁移（sha256 与 manifest 同值）；
  > 本步将其余 SoC 落地，原"手写 `#if CONFIG_IDF_TARGET_*` 分发体"方案作废。

  1. 新建 `chips/{esp32s3,esp32c3,esp32c6}/include/soc/{soc_caps.h,gpio_num.h}`，内容按本计划 §5.1
     裁决与正文取证（有效引脚掩码、端口数、HS 模式显式 `0` 等）；首版为手写，逐项对照官方 v6.1 源，
     **必须登记 `channels.json: chips_handwritten`**，否则门禁 fail。
  2. **禁止**创建或恢复 `include/soc/{soc_caps,gpio_num}.h`；共享树出现同名文件即门禁 fail
     （`check_harvested_headers.py` 的 `relocated must not exist` 规则）。
  3. 配置验证：`cmake -DWINK_ESP_TARGET=esp32s3|c3|c6` 必须配置通过；未提供数据的 SoC 由
     `esp_idf_target.cmake` 在 configure 期 `FATAL_ERROR`（不得静默回退 esp32）。
  4. 后续收割器支持按 SoC 发射后，将上述头转为生成物并从 `channels.json` 移出（闭源改动清单见
     `2026-09-25-esp-idf-asset-channels-refactor-plan.md`）。

- [x] **Step 4.5（已提前落地，2026-09-25，PLAN-20260925-ESP-IDF-ASSET-CHANNELS）：目标宏单源与 PUBLIC 注入**
  > **现状**：推导收敛于 `frameworks/esp_idf/esp_idf_target.cmake`（`WINK_ESP_TARGET` 单源 →
  > `WINK_ESP_TARGET_INCLUDE_DIR` / `WINK_IDF_TARGET_DEFINE` / `WINK_IDF_TARGET_STRING_DEFINE` +
  > 缺数据 `FATAL_ERROR`）；`CMakeLists.txt` 已 PUBLIC 注入数值与字符串双宏；Tier-B 语料硬编码已清理；
  > `test/wasm/esp_idf_wasm_compile.cmake` 已复用单源。M3 只需在 SoC 矩阵执行时复验（下方为原始留存）。

  > ⚠️ **评审修复**：现有 `esp_idf_sources.cmake` 仅设置了头文件搜索路径，但中转头文件和驱动源码中的
  > `#if defined(CONFIG_IDF_TARGET_ESP32S3)` 等条件编译**完全依赖此宏的存在**。若不注入，所有 SoC
  > 切换将跌入 fallback 分支回到 ESP32，导致多 SoC 拦截失效。
  >
  > ⚠️ **目录作用域约束（v2.2 修复）**：`esp_idf_sources.cmake` 由 `frameworks/esp_idf/CMakeLists.txt`
  > 以 `include()` 引入，变量**仅在该目录作用域可见**；`test/CMakeLists.txt` 属另一目录，
  > **读取不到 `WINK_IDF_TARGET_DEFINE`**（跨目录引用会得到空串）。因此注入必须紧随
  > `wink_framework_esp_idf` 目标创建之后、在**同一** `frameworks/esp_idf/CMakeLists.txt` 内完成，
  > 并以 `PUBLIC` 传播给测试可执行文件与 corpus OBJECT 目标。

  在 `wink-micro-os/frameworks/esp_idf/esp_idf_sources.cmake` 的 `WINK_ESP_TARGET` 设置之后追加：
  ```cmake
  # 从 WINK_ESP_TARGET 推导 CONFIG_IDF_TARGET_* 宏名（仅推导；注入见 frameworks/esp_idf/CMakeLists.txt）
  string(TOUPPER "${WINK_ESP_TARGET}" _WINK_ESP_TARGET_UPPER)
  set(WINK_IDF_TARGET_DEFINE "CONFIG_IDF_TARGET_${_WINK_ESP_TARGET_UPPER}")
  ```
  在 `wink-micro-os/frameworks/esp_idf/CMakeLists.txt` 中 `add_library(wink_framework_esp_idf …)` 之后注入：
  ```cmake
  target_compile_definitions(wink_framework_esp_idf PUBLIC
      ${WINK_IDF_TARGET_DEFINE}=1
  )
  ```

  同步**清理 Tier-B 语料硬编码宏**（否则 C3/C6 矩阵 job 下 `sdkconfig.h` 的
  `CONFIG_IDF_TARGET_ESP32` / `SOC_HP_I2C_NUM 2` 将与命令行注入宏及 `chips/*/soc_caps.h`
  在 `-Werror` 下触发重定义冲突）：修改
  `wink-micro-os/frameworks/esp_idf/test/corpus/legacy_i2c/include/sdkconfig.h`，删除
  `#define CONFIG_IDF_TARGET_ESP32 1` 与 `#define SOC_HP_I2C_NUM 2` 两行
  （目标宏由命令行 `target_compile_definitions` 供给，能力宏由芯片头供给），
  仅保留 `#include "sdkconfig_base.h"` 与语料自有配置 `#define SOC_I2C_SUPPORT_SLAVE 1`。

- [ ] **Step 5：强化驱动层门面 Fail-Loud 校验**
  - 在 `src/drivers/esp_ledc.c` 中：
    **替换**（非追加）现有硬编码 `#if defined(CONFIG_IDF_TARGET_ESP32C3) || ...` 分支，改为遵从 `SOC_LEDC_SUPPORT_HS_MODE` 宏：
    ```c
    /* 替换原来的 #if defined(CONFIG_IDF_TARGET_ESP32C3) || ... 硬编码分支 */
    #if !SOC_LEDC_SUPPORT_HS_MODE
        if (timer_conf->speed_mode == LEDC_HIGH_SPEED_MODE) {
            ESP_LOGE(TAG, "High speed mode not supported on current SoC");
            return ESP_ERR_INVALID_ARG;
        }
    #endif
    ```
  - 在 `src/drivers/esp_i2c_legacy.c` 与 `src/drivers/esp_i2c_master.c` 中：
    增加控制器端口上限校验。**⚠️ 注意**：`SOC_HP_I2C_NUM` 校验必须在函数入口处、所有以 `SOC_HP_I2C_NUM` 为维度的静态数组（`s_i2c_high_period[SOC_HP_I2C_NUM]` 等）访问**之前**完成，否则 C3 下传入 `i2c_num=1` 将导致越界写入：
    ```c
    /* ADR-0085 D1：门面仅依据 SOC_* 判定合法性，严禁引用 PAL_I2C_PORT_MAX 等 PAL 宏 */
    if (i2c_num >= SOC_HP_I2C_NUM) {
        return ESP_ERR_INVALID_ARG;
    }
    ```
    **同步存量整改（D1 合规）**：`esp_i2c_master.c` 的 `#define MAX_MASTER_BUSES PAL_I2C_PORT_MAX`
    与 `esp_i2c_legacy.c` 中 4 处 `PAL_I2C_PORT_MAX` 校验均为 ADR-0085 D1 违规存量，
    一并替换为 `SOC_HP_I2C_NUM`（静态数组维度同步替换），消除门面与 PAL caps 的交叉依赖。
  - 在 `src/drivers/esp_uart.c` 中：
    增加控制器端口上限校验。**裁决**：UART 枚举 `UART_NUM_0/1/2` + `UART_NUM_MAX` 保持全集（保证编译兼容性），SoC 约束仅在运行时 Fail-Loud 拦截（官方为 `#if SOC_UART_NUM > 2` 才定义 `UART_NUM_2` 的**编译期** fail-loud，门面以运行期替代，见 §5.1 裁决项 5）：
    ```c
    _Static_assert(UART_NUM_MAX >= SOC_UART_HP_NUM,
                   "UART_NUM_MAX must cover all SoC UART ports");
    if (uart_num >= SOC_UART_HP_NUM || uart_num >= UART_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    ```
  - 在 `src/drivers/esp_spi.c` 中（**评审新增**）：
    增加 SPI 控制器端口上限校验（C3/C6 仅 2 个 SPI，ESP32/S3 有 3 个）：
    ```c
    if (host_id >= SOC_SPI_PERIPH_NUM) {
        ESP_LOGE(TAG, "SPI host %d not available on current SoC (max %d)",
                 (int)host_id, (int)SOC_SPI_PERIPH_NUM);
        return ESP_ERR_INVALID_ARG;
    }
    ```
    > 📌 **有意偏离登记（§5.1 裁决项 5）**：官方在 `SOC_SPI_PERIPH_NUM <= 2` 的芯片上**不定义** `SPI3_HOST`
    > （C3/C6 下引用 `SPI3_HOST` 即编译期报错）；门面 `include/hal/spi_types.h` 保持全集定义，
    > 以运行期 Fail-Loud 替代编译期 fail-loud，保障 `TC-SOC-07` 可编译可执行。

- [ ] **Step 6：编写多 SoC 单元测试套件 `test/core/test_esp_soc_matrix.c`**
  ```c
  /* SPDX-License-Identifier: GPL-3.0-only */
  #include "unity.h"
  #include "driver/gpio.h"
  #include "driver/ledc.h"
  #include "driver/i2c.h"
  #include "driver/uart.h"
  #include "driver/spi_master.h"
  #include "soc/soc_caps.h"

  void setUp(void) {}
  void tearDown(void) {}

  void test_soc_gpio_boundary(void) {
  #if defined(CONFIG_IDF_TARGET_ESP32)
      // ESP32: GPIO 34 是输入专用，配置输出必须失败
      gpio_config_t cfg_in_only = {
          .pin_bit_mask = (1ULL << 34),
          .mode = GPIO_MODE_OUTPUT,
      };
      TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, gpio_config(&cfg_in_only));
  #elif defined(CONFIG_IDF_TARGET_ESP32C3)
      // ESP32-C3: 仅 22 个引脚，操作 GPIO 22 必须越界失败
      gpio_config_t cfg_oob = {
          .pin_bit_mask = (1ULL << 22),
          .mode = GPIO_MODE_OUTPUT,
      };
      TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, gpio_config(&cfg_oob));
  #elif defined(CONFIG_IDF_TARGET_ESP32S3)
      // ESP32-S3: GPIO 48 是合法输出引脚
      gpio_config_t cfg_s3 = {
          .pin_bit_mask = (1ULL << 48),
          .mode = GPIO_MODE_OUTPUT,
      };
      TEST_ASSERT_EQUAL(ESP_OK, gpio_config(&cfg_s3));
  #endif
  }

  void test_soc_ledc_hs_mode_restriction(void) {
      ledc_timer_config_t t_cfg = {
          .speed_mode = LEDC_HIGH_SPEED_MODE,
          .timer_num = LEDC_TIMER_0,
          .duty_resolution = LEDC_TIMER_10_BIT,
          .freq_hz = 5000,
      };
  #if !SOC_LEDC_SUPPORT_HS_MODE
      TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ledc_timer_config(&t_cfg));
  #else
      TEST_ASSERT_EQUAL(ESP_OK, ledc_timer_config(&t_cfg));
  #endif
  }

  void test_soc_i2c_port_restriction(void) {
  #if SOC_HP_I2C_NUM < 2
      // C3 官方仅 1 个 I2C；C6 官方为 HP1+LP1，M3 按 HP-only 裁决拦截（§5.1 裁决项 4）
      // 使用 (i2c_port_t)1 字面量绕过 C3 下 I2C_NUM_1 可能未定义的问题
      TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, i2c_driver_install((i2c_port_t)1, I2C_MODE_MASTER, 0, 0, 0));
  #else
      TEST_ASSERT_EQUAL(ESP_OK, i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0));
      i2c_driver_delete(I2C_NUM_1);
  #endif
  }

  void test_soc_uart_port_restriction(void) {
  #if SOC_UART_HP_NUM < 3
      // C3 官方仅 2 个 UART；C6 官方为 HP2+LP1，M3 按 HP-only 裁决拦截（§5.1 裁决项 4）
      TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, uart_driver_install(UART_NUM_2, 256, 256, 0, NULL, 0));
  #else
      TEST_ASSERT_EQUAL(ESP_OK, uart_driver_install(UART_NUM_2, 256, 256, 0, NULL, 0));
      uart_driver_delete(UART_NUM_2);
  #endif
  }

  void test_soc_spi_host_restriction(void) {
  #if SOC_SPI_PERIPH_NUM < 3
      // C3 / C6 仅有 2 个 SPI 控制器，SPI3_HOST 越界必须报错
      spi_bus_config_t bus_cfg = { .mosi_io_num = 1, .sclk_io_num = 2, .miso_io_num = -1 };
      TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                        spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_DISABLED));
  #endif
  }

  int main(void) {
      UNITY_BEGIN();
      RUN_TEST(test_soc_gpio_boundary);
      RUN_TEST(test_soc_ledc_hs_mode_restriction);
      RUN_TEST(test_soc_i2c_port_restriction);
      RUN_TEST(test_soc_uart_port_restriction);
      RUN_TEST(test_soc_spi_host_restriction);
      return UNITY_END();
  }
  ```

- [ ] **Step 7：在 CMakeLists.txt 中注册 SoC 矩阵测试（🔴 评审裁决：采用 CI 矩阵策略）**

  > ⚠️ **评审裁决**：驱动源码与测试源码必须在同一个 `CONFIG_IDF_TARGET_*` 宏下编译。
  > 因此"单构建树同时注册 4 个 SoC 测试目标"在共享驱动库的架构下不可行（驱动中的
  > `#if !SOC_LEDC_SUPPORT_HS_MODE` 只会按当前唯一的 SoC 宏展开）。
  >
  > **最终裁决**：保持 CI 矩阵策略（每个 SoC 独立完整编译），单次构建仅产出一个
  > `test_esp_soc_matrix` 测试目标（不带 SoC 后缀），该测试在对应 SoC 宏下运行。

  在 `test/CMakeLists.txt` 中注册 `test_esp_soc_matrix`，它将自动继承 `wink_framework_esp_idf`
  目标上的 `CONFIG_IDF_TARGET_*` 宏定义。CI 通过 `-DWINK_ESP_TARGET=xxx` 参数矩阵化执行。

#### 验证步骤

1. **验证命令**（每个 SoC 独立构建，对应 CI 矩阵策略）：
   ```powershell
   # 以 ESP32-C3 为例，其余 SoC 同理替换 WINK_ESP_TARGET 值
   # 🔴 必须显式指定 -DTARGET_PLATFORM=host：默认平台为 wasm，
   #    wasm 分支不会注册 test/ 目录，ctest 将以"0 tests"空跑假绿
   cmake -B build_c3 -S wink-micro-os -DENABLE_ESP_IDF_FRAMEWORK=ON -DTARGET_PLATFORM=host -DWINK_ESP_TARGET=esp32c3
   cmake --build build_c3 --config Debug
   ctest --test-dir build_c3 -C Debug -R "test_esp_soc_matrix" --output-on-failure
   ```
2. **预期输出**：
   ```text
   100% tests passed, 0 tests failed out of 1
   ```
3. **机器合规检查**：
   ```powershell
   winkcli lint --pack esp_idf_all
   python .github/scripts/check_license_map.py
   ```

---

### Task M3-2：官方语料 CI 全自动化与覆盖率工具接线 `[ 状态: 📋 待开始 ]`

| 字段 | 内容 |
|:---|:---|
| **负责人** | 仿真拦截专项小组 |
| **预估工时** | 12 小时 |
| **优先级** | 🔴 P0 |
| **前置依赖** | Task M3-1 |
| **修改文件** | `wink-micro-os/test/CMakeLists.txt`, `tools/coverage.sh`, `tools/check_coverage.py`, `.github/workflows/esp_idf_ci.yml` |
| **接口变化** | 接入 CI 流水线与覆盖率生成通道，无代码接口变化 |

#### 详细步骤与代码级设计

- [ ] **Step 1：中央 `test/CMakeLists.txt` 覆盖率编译选项接线 (T-011)**
  ```cmake
  option(WINK_ENABLE_COVERAGE "Enable code coverage flags for GCC/Clang" OFF)
  if(WINK_ENABLE_COVERAGE AND NOT MSVC)
      set(ESP_IDF_COVERAGE_FLAGS "-fprofile-arcs -ftest-coverage")
      # 编译仅插桩框架库源码（PRIVATE）；覆盖率仅统计 frameworks/esp_idf/src/
      target_compile_options(wink_framework_esp_idf PRIVATE ${ESP_IDF_COVERAGE_FLAGS})
      # 🔴 链接必须 PUBLIC：wink_framework_esp_idf 是 STATIC 库，
      #    PRIVATE link options 不会传播到测试可执行文件，最终链接将缺少
      #    __gcov_* 符号而失败；PUBLIC 使所有链接该库的目标继承 gcov 链接选项
      target_link_options(wink_framework_esp_idf PUBLIC ${ESP_IDF_COVERAGE_FLAGS})
  endif()
  ```

- [ ] **Step 2：编写覆盖率收集与生成脚本 `tools/coverage.sh`**
  ```bash
  #!/usr/bin/env bash
  set -euo pipefail

  BUILD_DIR="build_cov"
  rm -rf "${BUILD_DIR}"
  # 🔴 -DTARGET_PLATFORM=host 必不可少：默认 wasm 平台不注册 test/，
  #    ctest 会以"0 tests found"（exit 0）空跑，覆盖率采集为空且门禁失真
  cmake -B "${BUILD_DIR}" -S wink-micro-os \
      -DENABLE_ESP_IDF_FRAMEWORK=ON \
      -DTARGET_PLATFORM=host \
      -DWINK_ENABLE_COVERAGE=ON \
      -DCMAKE_BUILD_TYPE=Debug

  cmake --build "${BUILD_DIR}" -j"$(nproc)"
  ctest --test-dir "${BUILD_DIR}" -L esp_idf --output-on-failure

  # 收集并过滤只保留 frameworks/esp_idf/src
  lcov --capture --directory "${BUILD_DIR}" --output-file "${BUILD_DIR}/coverage_all.info"
  lcov --extract "${BUILD_DIR}/coverage_all.info" "*/frameworks/esp_idf/src/*" --output-file "${BUILD_DIR}/coverage_filtered.info"
  genhtml "${BUILD_DIR}/coverage_filtered.info" --output-directory "${BUILD_DIR}/coverage_html"

  echo "HTML report generated at: ${BUILD_DIR}/coverage_html/index.html"
  ```

- [ ] **Step 3：编写覆盖率门禁断言工具 `tools/check_coverage.py`**
  ```python
  #!/usr/bin/env python3
  # SPDX-License-Identifier: GPL-3.0-only
  import sys
  import re

  def main():
      if len(sys.argv) < 3:
          print("Usage: check_coverage.py <coverage_file> <min_line_percent>")
          sys.exit(1)

      info_file = sys.argv[1]
      threshold = float(sys.argv[2])

      lines_found = 0
      lines_hit = 0
      branches_found = 0
      branches_hit = 0

      with open(info_file, "r", encoding="utf-8") as f:
          for line in f:
              if line.startswith("LF:"):
                  lines_found += int(line.strip().split(":")[1])
              elif line.startswith("LH:"):
                  lines_hit += int(line.strip().split(":")[1])
              elif line.startswith("BRF:"):
                  branches_found += int(line.strip().split(":")[1])
              elif line.startswith("BRH:"):
                  branches_hit += int(line.strip().split(":")[1])

      if lines_found == 0:
          print("Error: No line data found in coverage file!")
          sys.exit(1)

      percentage = (lines_hit / lines_found) * 100.0
      print(f"ESP-IDF Core Facade Line Coverage: {percentage:.2f}% (Hit {lines_hit}/{lines_found})")
      print(f"Required Threshold: {threshold:.2f}%")

      if branches_found > 0:
          br_percentage = (branches_hit / branches_found) * 100.0
          print(f"ESP-IDF Core Facade Branch Coverage: {br_percentage:.2f}% (Hit {branches_hit}/{branches_found}) [Info Only]")

      if percentage < threshold:
          print(f"FAILED: Coverage {percentage:.2f}% is below threshold {threshold:.2f}%!")
          sys.exit(1)
      
      print("SUCCESS: Coverage threshold gate PASSED!")

  if __name__ == "__main__":
      main()
  ```

  > 💡 **覆盖率现实性与豁免策略（评审补充）**：85% 行覆盖率门禁主要考量正常业务逻辑与可触发的边界校验分支。对于永不可达的底层防御性分支（如 `_Static_assert` 或特定硬编码桩函数），若首次测试由于防御性代码较多达到 80%~84%，可通过调整 lcov 抽取规则排除无业务逻辑的纯桩代码文件，**禁止直接下调门禁数值**，确保高质量交付。


- [ ] **Step 4：创建 GitHub Actions 工作流 `.github/workflows/esp_idf_ci.yml`**
  ```yaml
  name: ESP-IDF Simulation Interception CI

  on:
    push:
      branches: [ master ]
      paths:
        - 'wink-micro-os/frameworks/esp_idf/**'
        - '.github/workflows/esp_idf_ci.yml'
    pull_request:
      branches: [ master ]
      paths:
        - 'wink-micro-os/frameworks/esp_idf/**'
        - '.github/workflows/esp_idf_ci.yml'
    workflow_dispatch:

  jobs:
    lint-and-governance:
      runs-on: ubuntu-latest
      steps:
        - uses: actions/checkout@v4
        - name: Set up Python
          uses: actions/setup-python@v5
          with:
            python-version: '3.11'
        - name: Check License Map
          run: python .github/scripts/check_license_map.py
        # winkcli 经 GitHub Releases 分发（与 pr.yml 同策略）。
        # 🔴 禁止 `pip install ./wink-tools`：wink-tools 无 pyproject.toml/setup.py，
        #    该命令必然失败；且 pr.yml 已注明 winkcli 未上 PyPI（名称抢注风险）。
        - name: Run Layering / API / ESP-IDF Lint
          run: |
            if ! command -v winkcli >/dev/null 2>&1; then
              echo "::warning::winkcli not on PATH; install from GitHub Releases (see wink-tools/README.md). Lint packs skipped."
              exit 0
            fi
            winkcli lint --pack layering --pack api
            winkcli lint --pack esp_idf_all

    host-matrix-tests:
      needs: lint-and-governance
      strategy:
        fail-fast: false
        matrix:
          os: [ubuntu-latest, windows-latest]
          target: [esp32, esp32s3, esp32c3, esp32c6]
      runs-on: ${{ matrix.os }}
      steps:
        - uses: actions/checkout@v4
        - name: Configure CMake
          run: |
            cmake -B build -S wink-micro-os -DENABLE_ESP_IDF_FRAMEWORK=ON -DTARGET_PLATFORM=host -DWINK_ESP_TARGET=${{ matrix.target }}
        - name: Build and Test
          run: |
            cmake --build build --config Debug -j2
            ctest --test-dir build -C Debug -L esp_idf --output-on-failure

    coverage-gate:
      needs: lint-and-governance
      runs-on: ubuntu-latest
      steps:
        - uses: actions/checkout@v4
        - name: Install lcov
          run: sudo apt-get update && sudo apt-get install -y lcov
        - name: Run Coverage Pipeline
          run: |
            chmod +x wink-micro-os/frameworks/esp_idf/tools/coverage.sh
            ./wink-micro-os/frameworks/esp_idf/tools/coverage.sh
        - name: Check 85% Line Coverage Threshold
          run: |
            python wink-micro-os/frameworks/esp_idf/tools/check_coverage.py build_cov/coverage_filtered.info 85
        - name: Upload Coverage HTML Artifact
          uses: actions/upload-artifact@v4
          with:
            name: esp_idf_coverage_html
            path: build_cov/coverage_html/
  ```

---

### Task M3-3：跨平台 Headless 证据链固化与收官总结 `[ 状态: 📋 待开始 ]`

| 字段 | 内容 |
|:---|:---|
| **负责人** | 仿真拦截专项小组 |
| **预估工时** | 8 小时 |
| **优先级** | 🔴 P0 |
| **前置依赖** | Task M3-1, Task M3-2 |
| **修改文件** | `test/headless/test_esp_idf_headless_replay.py`, `docs/01-architecture-and-governance-guide.md`, `docs/02-api-coverage-matrix.md`, `docs/03-include-closure-inventory.md` |
| **接口变化** | 产出确定性仿真证据链，完成全文档结项收归 |

#### 详细步骤与代码级设计

- [ ] **Step 1：固化 Headless 确定性回放验证脚本 `test_esp_idf_headless_replay.py`**

  > ⚠️ **评审加固**：原版本三处问题已修正：① 无超时保护（死循环将永久挂起）；② 未过滤 `ESP_LOGx` 中的时间戳/指针地址等非确定性内容；③ 仅跑 2 次统计上不充分。

  ```python
  #!/usr/bin/env python3
  # SPDX-License-Identifier: GPL-3.0-only
  """
  ESP-IDF Headless Deterministic Replay Verifier
  Verifies that running the same simulation multiple times produces bit-exact
  deterministic event hashes (zero scheduling jitter, identical virtual timelines).
  """
  import subprocess
  import hashlib
  import sys
  import re

  # 过滤非确定性行：时间戳、指针地址、进程 ID 等
  _NON_DETERMINISTIC_PATTERN = re.compile(
      r'(\([0-9]+\)|0x[0-9a-fA-F]+|\d{4}-\d{2}-\d{2}|pid=\d+)',
      re.IGNORECASE
  )

  def run_simulation_and_hash(binary_path, timeout_sec=30):
      try:
          res = subprocess.run(
              [binary_path],
              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
              text=True, timeout=timeout_sec
          )
      except subprocess.TimeoutExpired:
          print(f"Error: binary timed out after {timeout_sec}s — possible infinite loop")
          return None
      if res.returncode != 0:
          print(f"Error: binary exited with {res.returncode}\n{res.stderr}")
          return None
      # 过滤非确定性内容后计算 SHA-256
      lines = res.stdout.replace("\r\n", "\n").splitlines(keepends=True)
      filtered = [_NON_DETERMINISTIC_PATTERN.sub('<FILTERED>', l) for l in lines]
      sha = hashlib.sha256("".join(filtered).encode("utf-8")).hexdigest()
      return sha

  def main():
      if len(sys.argv) < 2:
          print("Usage: test_esp_idf_headless_replay.py <path_to_test_binary>")
          sys.exit(1)
      binary = sys.argv[1]
      runs = 3  # 至少 3 次以排除偶然因素
      print(f"Testing deterministic replay on: {binary} ({runs} runs)")

      hashes = [run_simulation_and_hash(binary) for _ in range(runs)]
      if any(h is None for h in hashes):
          print("Failed to obtain all run hashes")
          sys.exit(1)

      for i, h in enumerate(hashes, 1):
          print(f"Run {i} Hash: {h}")

      if len(set(hashes)) == 1:
          print("SUCCESS: Deterministic replay verified! All runs bit-exact match.")
          sys.exit(0)
      else:
          print("FAILURE: Scheduling jitter detected! Hash mismatch between runs.")
          sys.exit(1)

  if __name__ == "__main__":
      main()
  ```

- [ ] **Step 2：三层证据塔 L2 Vendor 行为用例核查**
  - 运行 `test_esp_idf_blink_run`，验证周期调度与 GPIO 输出事件；
  - 运行官方示例语料 CTest：`esp_idf_corpus_blink`、`esp_idf_corpus_ledc`、`esp_idf_corpus_i2c`、`esp_idf_corpus_legacy_i2c`（可批量：`ctest -R esp_idf_corpus`）。

- [ ] **Step 3：文档矩阵与闭包清单终审回写**
  - 更新 `docs/01-architecture-and-governance-guide.md`：增加多 SoC 矩阵配置与 CI 流水线使用指南；
  - 更新 `docs/02-api-coverage-matrix.md`：记录最终覆盖率百分比与全部已闭环外设状态；
  - 更新 `docs/03-include-closure-inventory.md`：归档 6 个 SoC 头文件（S3/C3/C6 的 `soc_caps.h` 与 `gpio_num.h`）。

- [ ] **Step 4：组织总纲全量验收评审与结项**
  - 核查 L0~L4 全部门禁出口；
  - 将实施总纲 [`PLAN-20260922-ESP-IDF-SIM-MASTER`](./2026-09-22-esp-idf-simulation-interception-master-plan.md) 状态签署为 `✅ 已验收结项`。

---

## 7. 测试策略与分级验收出口（L0 ~ L4）

### L0 编译门禁（必须 100% 通过）
- [ ] **Host 目标编译**：GCC / Clang `-Wall -Wextra -Werror` 0 error 0 warning。
- [ ] **Wasm 目标编译**：Emscripten 0 error 0 warning。
- [ ] **多 SoC 目标编译**：`-DWINK_ESP_TARGET=esp32s3/esp32c3/esp32c6` 全部无告警编译。
- [ ] **Tier-A / Tier-B 语料编译**：4 组官方语料源文件（`*.c`）原文零修改 100% 编译通过（harness 侧 `legacy_i2c/include/sdkconfig.h` 按 Step 4.5 清理硬编码宏，不属语料源文件）。
- [ ] **代码规范与机器红线**：`winkcli lint --pack esp_idf_all` 与 `winkcli lint --pack layering --pack api` 全绿。
- [ ] **开源许可门禁**：`python .github/scripts/check_license_map.py` 100% 匹配。

### L1 单元测试门禁（必须 100% 通过）
- [ ] 全部单元测试（`test_esp_err`、`test_esp_gpio`、`test_esp_idf_freertos`、`test_esp_ledc`、`test_esp_i2c`、`test_esp_uart`、`test_esp_gptimer`、`test_esp_spi`、`test_esp_nvs`）全部绿灯。
- [ ] `test_esp_soc_matrix`：各 SoC 芯片引脚边界与专用控制器越界拦截 100% 断言成功。

#### L1 边界与异常分支测试用例矩阵（🔴 必测）

| 测试用例 ID | 芯片/外设场景 | 注入异常参数 | 预期断言与防护行为 |
|:---|:---|:---|:---|
| **TC-SOC-01** | ESP32-C3 GPIO | 请求配置 `GPIO_NUM_22` ~ `40` | 返回 `ESP_ERR_INVALID_ARG`（超出 C3 芯片 22 引脚物理极限） |
| **TC-SOC-02** | ESP32-C3 LEDC | `speed_mode = LEDC_HIGH_SPEED_MODE` | 返回 `ESP_ERR_INVALID_ARG`（C3 无硬件高速模式生成器） |
| **TC-SOC-03** | ESP32-C3 I2C | `i2c_port = I2C_NUM_1` | 返回 `ESP_ERR_INVALID_ARG`（C3 仅有单一 I2C 控制器） |
| **TC-SOC-04** | ESP32 经典 GPIO | 在 `GPIO_NUM_34` 上设置 `GPIO_MODE_OUTPUT` | 返回 `ESP_ERR_INVALID_ARG`（34~39 仅限输入专用） |
| **TC-SOC-05** | ESP32-S3 GPIO | 配置 `GPIO_NUM_48` 为输出电平 | 成功返回 `ESP_OK`（S3 支持高位 48 号引脚输出） |
| **TC-SOC-06** | ESP32-C6 UART | 请求初始化 `UART_NUM_2` | 返回 `ESP_ERR_INVALID_ARG`（C6 门面仅暴露 2 个 HP UART；硬件另有 1 个 LP UART，按 §5.1 裁决项 4 HP-only 拦截） |
| **TC-SOC-07** | ESP32-C3/C6 SPI | 请求初始化 `SPI3_HOST` | 返回 `ESP_ERR_INVALID_ARG`（C3/C6 仅有 2 个 SPI 控制器，SPI3_HOST 越界） |

### L2 行为仿真与回放门禁
- [ ] `test_esp_idf_headless_replay.py`：3 次运行轨迹哈希 bit-exact 全部一致（含非确定性内容过滤）。
- [ ] Vendor app 行走测试：无运行时未捕获异常，`esp_restart` 后状态清洗完整。

### L3 文档与覆盖率门禁
- [ ] 行覆盖率报告：`frameworks/esp_idf/src/` Line Coverage **≥ 85%**。
- [ ] `01`、`02`、`03` 三份治理文档齐备且无断链。

### L4 治理与发布门禁
- [ ] 0 动态堆分配（外部 lint 扫描零 `malloc`）。
- [ ] GitHub Actions CI 工作流在 master 分支全绿通过。

---

## 8. 回滚与降级方案（🔴 必选）

### 方案 1：CMake 开关全局回退
- **触发条件**：新增 SoC 宏或 CI 接入导致整体构建严重受阻。
- **操作步骤**：命令行指定 `-DENABLE_ESP_IDF_FRAMEWORK=OFF`，立即将所有 ESP-IDF 模块剔除构建树。
- **预期恢复时间**：< 1 分钟。

### 方案 2：SoC Target 回退至经典 ESP32 基线
- **触发条件**：C3/S3/C6 芯片宏定义与现有业务代码产生非预期冲突。
- **操作步骤**：恢复 `-DWINK_ESP_TARGET=esp32`，屏蔽试验性 SoC 分支。
- **预期恢复时间**：< 2 分钟。

### 方案 3：Git 分支原子回退
- **操作步骤**：`git revert <commit-hash>`。M3 变更不修改底座 PAL/DAL，回退安全无侵入。

---

## 9. 参考资料与变更记录（🔴 必选）

### 9.1 参考资料
- [`PLAN-20260922-ESP-IDF-SIM-MASTER`](./2026-09-22-esp-idf-simulation-interception-master-plan.md) (v3.5)
- [ADR-0085：ESP-IDF 门面 SoC 能力与 PAL Caps 双 SSOT 裁决](../../decisions/core/0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md)
- [ADR-0080：框架外部 lint pack 自动发现机制](../../decisions/core/0080-external-lint-pack-discovery-and-mcs51-guard-sinking.md)
- ESP-IDF `v5.1.3` 与 `v6.1` 官方芯片树（`components/soc/esp32s3`, `components/soc/esp32c3`, `components/soc/esp32c6`；v5.1 系列已于 2025-12 EOL，仅作旧版取证基线）

### 9.2 计划版本变更记录

| 版本 | 日期 | 变更内容 | 变更人 |
|:---:|:---:|:---|:---:|
| **v1.0** | 2026-09-23 | 建立 M3 SoC 矩阵扩展与自动化测试骨架文档 | 仿真拦截专项小组 |
| **v1.1** | 2026-09-24 | 增加构建隔离、T-011/T-012 提前 spike 与收官两项检查约束 | 仿真拦截专项小组 |
| **v1.2** | 2026-09-24 | 吸收 vendor 行为证据套件要求与三层证据塔规范 | 仿真拦截专项小组 |
| **v2.0** | 2026-09-25 | **完全详设展开版（收官战役）**：<br>① 消费 M2 v2.4 交付基线，补齐全部前置约束；<br>② 给出 S3/C3/C6 三芯片 `soc_caps.h` 与 `gpio_num.h` 完整官方真值表与代码设计；<br>③ 给出驱动层门面 Fail-Loud 宏校验改造；<br>④ 给出多 SoC 差异化拦截单测 `test_esp_soc_matrix` 完整代码；<br>⑤ 设计覆盖率工具链 `coverage.sh` 与 `check_coverage.py`（≥ 85% 门禁）；<br>⑥ 给出 GitHub Actions CI 全流水线 YAML 配置；<br>⑦ 给出 Headless 确定性回放脚本 `test_esp_idf_headless_replay.py`；<br>⑧ 规范化 L0~L4 收官验收准则与回滚策略。 | 仿真拦截专项小组 |
| **v2.1** | 2026-09-25 | **专家评审加固版**：<br>① 补全 S3 `gpio_num.h` 缺失的 GPIO 22~25 枚举并显式声明 Flash/PSRAM 简化策略；<br>② Step 4.5 新增 CMake `CONFIG_IDF_TARGET_*` 编译宏自动推导与注入；<br>③ LEDC/I2C/UART 门面校验加固（明确替换旧宏、前置端口校验防止越界、`_Static_assert` 保障 UART 枚举覆盖）；<br>④ 补充 SPI 门面 SoC 校验（C3/C6 限制 2 控制器，新增 `TC-SOC-07`）；<br>⑤ 裁决采用 CI 矩阵独立编译策略，解决驱动与单测宏展开冲突；<br>⑥ 加固 Headless 确定性回放脚本（3 轮测试、超时保护、时间戳/指针地址过滤）；<br>⑦ 覆盖率工具链补充分支覆盖率解析与豁免策略，修正附录 A.2 脚本执行路径；更新 SSOT 追溯矩阵。 | 仿真拦截专项小组 |
| **v2.2** | 2026-09-25 | **官方真值校核与可执行性修订版（v5.1.3/v6.1 源码逐项比对 + 本地 cmake/ctest 实测）**：<br>① S3 有效掩码与枚举回归官方事实：`GPIO 22~25` 物理不存在（v5.1.3/v6.1 掩码与枚举一致排除），掩码改为 `0x1FFFFFFFFFFFFULL & ~(22~25)`、枚举跳过 22~25，真值表/文件清单/§2.1 同步修正（v2.1 的"22~25 枚举补全"系误判，本版撤销）；<br>② ESP32/C6 真值表精确化（掩码排除 24、28~31；C6 官方双版本值 v5.1.3=1I2C/2UART、v6.1=2I2C/3UART 含 LP），新增 **裁决项 4**：C6 LP 外设 HP-only 有意偏离登记（撤销"C6 硬件不支持 LP I2C"错误注释）；<br>③ 新增 **裁决项 5**：门面全集枚举/宏（`SPI3_HOST`/`UART_NUM_2`/`LEDC_HIGH_SPEED_MODE`/`SOC_LEDC_SUPPORT_HS_MODE=0`）对官方编译期 fail-loud 的有意替代登记；<br>④ 全部 CMake 配置补 `-DTARGET_PLATFORM=host`（实测默认 wasm 平台下 `test/` 不注册、`ctest -L esp_idf` 零测试 exit 0 假绿）；<br>⑤ CI 删除必失败的 `pip install ./wink-tools`（无打包元数据，pr.yml 明令禁止），改为 pr.yml 同款 winkcli 获取与缺省跳过策略；<br>⑥ 覆盖率 `target_link_options` 由 PRIVATE 改 PUBLIC（STATIC 库 PRIVATE 不传播，测试可执行文件缺 `__gcov_*` 必然链接失败）；<br>⑦ Step 4.5 注入动作移至 `frameworks/esp_idf/CMakeLists.txt`（修 `WINK_IDF_TARGET_DEFINE` 跨目录作用域不可见），并新增 legacy_i2c corpus `sdkconfig.h` 硬编码宏清理（防 C3/C6 矩阵 `-Werror` 重定义）；<br>⑧ Step 5 I2C 校验移除 `PAL_I2C_PORT_MAX`（ADR-0085 D1）并登记存量整改（`esp_i2c_master.c`/`esp_i2c_legacy.c`）；<br>⑨ 撤销"v5.1.3 LTS"术语（官方 2020-07 起无 LTS 品牌，v5.1 已于 2025-12 EOL，v5.1.3 亦非最后 patch）；<br>⑩ 目标 4 与 T-012 范围声明对齐（Nightly IDF 版本矩阵不在本计划交付）；Headless 成功指标补路径与 `<path_to_test_binary>` 参数；DoD/Step 语料名更正为实际 CTest 名 `esp_idf_corpus_*`。 | 仿真拦截专项小组 |

---

## 附录 A：验证操作手册

### A.1 本地测试执行步骤（Windows PowerShell）

```powershell
# 1. 切换至仓库根目录
cd d:\workspaces\ai-coding\wink-ai\wink-ai-embedded

# 2. 默认 ESP32 目标配置与编译（🔴 必须显式 -DTARGET_PLATFORM=host，否则 test/ 不注册、ctest 空跑假绿）
cmake -B build -S wink-micro-os -DENABLE_ESP_IDF_FRAMEWORK=ON -DTARGET_PLATFORM=host
cmake --build build --config Debug

# 3. 运行多 SoC 矩阵测试套件与核心单测
ctest --test-dir build -C Debug -L esp_idf --output-on-failure

# 4. 执行代码规范与机器红线检查
winkcli lint --pack esp_idf_all
winkcli lint --pack layering --pack api

# 5. 开源许可合规检查
python .github/scripts/check_license_map.py
```

### A.2 Linux 覆盖率生成与门禁检查步骤（Bash）

> ⚠️ **评审修正**：脚本必须在**仓库根目录**（包含 `wink-micro-os/` 的那一层）执行，不要先 `cd wink-micro-os`，否则 `coverage.sh` 内部的 `-S wink-micro-os` 路径将变为错误的二重嵌套。

```bash
# 1. 确认当前在仓库根目录（包含 wink-micro-os/ 子目录）
# cd /path/to/wink-ai-embedded

# 2. 赋予脚本执行权限并生成覆盖率报告
chmod +x wink-micro-os/frameworks/esp_idf/tools/coverage.sh
./wink-micro-os/frameworks/esp_idf/tools/coverage.sh

# 3. 验证行覆盖率是否达到 85% 门禁基线
python wink-micro-os/frameworks/esp_idf/tools/check_coverage.py \
    build_cov/coverage_filtered.info 85

# 4. 查看 HTML 可视化报告
# 报告位于 build_cov/coverage_html/index.html
```

---

## 附录 B：SSOT 追溯矩阵（Master Plan & ADRs 映射）

| 实施项 / 交付物 | 对应总纲条目 | 对应 ADR | 验证出口 |
|:---|:---|:---|:---|
| S3/C3/C6 `soc_caps.h` & `gpio_num.h` | 实施总纲 Task 3-1 | [ADR-0085](../../decisions/core/0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md) | `test_esp_soc_matrix` |
| LEDC/I2C/UART/SPI 越界 Fail-Loud 校验 | 实施总纲 Task 3-1 | [ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md) | `TC-SOC-01` ~ `TC-SOC-07` |
| SPI 门面 SoC 端口校验（C3/C6 限 2 个） | 实施总纲 Task 3-1 | [ADR-0085](../../decisions/core/0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md) | `TC-SOC-07` / `test_soc_spi_host_restriction` |
| CI 覆盖率收集与 85% 门禁 | 实施总纲 T-011 | [ADR-0080](../../decisions/core/0080-external-lint-pack-discovery-and-mcs51-guard-sinking.md) | `check_coverage.py` 退出码 0 |
| GitHub Actions 自动化工作流 | 实施总纲 Task 3-2 | [ADR-0083/0084](../../decisions/core/0083-adopt-gpl-3.0-only-license-policy.md) | Actions 流水线全绿 |
| Headless 确定性回放脚本（3 次运行+过滤） | 实施总纲 T-008 | [ADR-0014](../../decisions/unisim/0014-sim-single-virtual-core.md) | `test_esp_idf_headless_replay.py` |
| 收官结项文档与闭包清单登记 | 实施总纲 Task 3-3 | [ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md) | `01`、`02`、`03` 文档终审合入 |
