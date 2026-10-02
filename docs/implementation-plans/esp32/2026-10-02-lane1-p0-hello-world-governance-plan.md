<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 1 P0 标杆 `get-started/hello_world` 官方示例仿真治理闭环

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261002-LANE1-P0-HELLO-WORLD-v1.0 |
| 状态 | ✅ **Complete (Fully Verified & Signed)** |
| 日期 | 2026-10-02 |
| 目标平台/SoC | `wasm32-unknown-emscripten` / `esp32 xtensa` 双 Target |
| 工具链/SDK版本| `ESP-IDF v6.1@fff9895c` / `Emscripten 3.1.x` / Node.js 24+ / Python 3.11+ |
| 优先级 | **P0（Lane 1 入门启动与系统软复位生命周期基石标杆）** |
| 治理依据 | [governance-sop-esp](../../../.agents/skills/governance-sop-esp/SKILL.md)<br>[ADR-0001：负数错误码标准](../../decisions/core/0001-error-code-sign-convention.md)<br>[ADR-0002：双 Target 同源编译](../../decisions/core/0002-dual-target-compilation.md)<br>[ADR-0004：编译期静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0012：Fail-Loud 原则](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0083 / ADR-0084：开源许可分层地图](../../decisions/core/0083-open-source-license-boundary.md) |
| 管辖数据源 | `checklist.data.json`（Display ID: 2, `esp.get_started.hello_world`） |
| 实施目标文件 | `wink-micro-os/frameworks/esp_idf/include/esp_flash.h`（新增 Flash 自省 C-ABI）<br>`wink-micro-os/frameworks/esp_idf/src/core/esp_system.c`（实现 Flash 尺寸查询与最小堆自省）<br>`wink-micro-app/vendor/esp_idfv61/get-started/hello_world/`（新建示例应用脚手架）<br>`wink-micro-app/vendor/esp_idfv61/get-started/hello_world/hello_world_main.c`（原厂零修改镜像，固化 SHA-256）<br>`wink-micro-app/vendor/esp_idfv61/get-started/hello_world/unisim-scenarios/hello_world.scenario.json`（业务因果断言场景）<br>`wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1`（登记 carrier） |
| 验收门禁 | `python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --gate 1`<br>`python .github/scripts/check_license_map.py`<br>Headless 正向基线通过 + Canary 变异 100% 击杀<br>`evidence_verifier.py --verify-all` 凭据验证全绿（达成 13/13 黄金基准） |

---

## 一、 战略目标与现状分析

### 1.1 背景与破局意义
在 ESP-IDF 官方示例治理清单中：
- **`get-started/hello_world`（Display ID: 2）** 是 ESP-IDF 官方最权威的起步示例；
- 与已治理完成的 `001: blink` 相比，`hello_world` 覆盖了芯片自省（`esp_chip_info`）、闪存自省（`esp_flash_get_size`）、堆内存统计（`esp_get_minimum_free_heap_size`）、控制台非阻塞/阻塞格式化输出以及由 `vTaskDelay` + `esp_restart()` 构成的系统软复位生命周期；
- 攻克 `hello_world` 将消除 001~004 连续编号断档，为 Lane 1（系统生命周期）建立完整的参考基准。

### 1.2 原厂代码与底座缺口分析
权威原厂路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\get-started\hello_world\main\hello_world_main.c`
固化 SHA-256：`b2d1c8573307e010d276d2d0df537f1fe752416f8422e267aa8b1661c1744595`

1. **Layer A 框架就绪项**：
   - `esp_chip_info()` 已在 `esp_system.c` 中实现（返回 `CHIP_ESP32`, 2 cores, WiFi/BT/BLE）；
   - `esp_restart()` 已在 `esp_idf_bridge.c` 中实现（noreturn 软复位契约）；
   - FreeRTOS `vTaskDelay` 与时间基准已完全具备。
2. **Layer A 框架缺口项**：
   - 缺少标准头文件 `esp_flash.h` 及 `esp_flash_t` 结构体声明；
   - 缺少 `esp_flash_get_size(esp_flash_t *chip, uint32_t *out_size)` 函数实现（在仿真环境中返回标准 4MB Flash 即 0x400000 字节）；
   - 缺少 `esp_get_minimum_free_heap_size()` 函数实现（与 `esp_get_free_heap_size()` 协同返回确定性堆容量）。
3. **Layer C 应用载体缺口**：
   - `wink-micro-app/vendor/esp_idfv61/get-started/hello_world` 尚未落盘；
   - 需零篡改镜像 `hello_world_main.c`，配置 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json`；
   - 在 `run_esp32_headless_evidence.ps1` 中登记 `hello_world` carrier。
4. **Layer S 场景断言编撰**：
   - 编撰 `unisim-scenarios/hello_world.scenario.json`；
   - 采用因果断言，匹配控制台关键日志序列与系统重启契约，坚决杜绝假绿。

---

## 二、 架构规格与接口设计

### 2.1 C-ABI 头文件契约

#### 1. `include/esp_flash.h`（新增标准 Flash 自省抽象）
```c
/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef WINK_ESP_FLASH_H
#define WINK_ESP_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_flash_t esp_flash_t;

/**
 * @brief Get flash chip size in bytes.
 * @param chip Pointer to main flash chip (NULL for default).
 * @param out_size Pointer to store flash size in bytes.
 * @return ESP_OK on success.
 */
esp_err_t esp_flash_get_size(esp_flash_t *chip, uint32_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* WINK_ESP_FLASH_H */
```

#### 2. `esp_system.c` 补充实现
```c
esp_err_t esp_flash_get_size(esp_flash_t *chip, uint32_t *out_size) {
    (void)chip;
    if (!out_size) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Standard ESP32 devkit 4MB external SPI flash */
    *out_size = 4 * 1024 * 1024;
    return ESP_OK;
}

uint32_t esp_get_minimum_free_heap_size(void) {
    return esp_get_free_heap_size();
}
```

---

## 三、 执行计划与任务拆解

### Phase 1：Layer A 底座框架 C-ABI 补齐
- [x] **Task 1.1**：创建 `wink-micro-os/frameworks/esp_idf/include/esp_flash.h`（遵循 LGPL-3.0-only 许可）。
- [x] **Task 1.2**：在 `wink-micro-os/frameworks/esp_idf/src/core/esp_system.c` 实现 `esp_flash_get_size()` 与 `esp_get_minimum_free_heap_size()`。

### Phase 2：Layer C 应用载体与原厂镜像落盘
- [x] **Task 2.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/get-started/hello_world/`。
- [x] **Task 2.2**：镜像 `hello_world_main.c`，固化 SHA-256（`b2d1c8573307e010d276d2d0df537f1fe752416f8422e267aa8b1661c1744595`）。
- [x] **Task 2.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 2.4**：在 `run_esp32_headless_evidence.ps1` 的 `$allCarriers` 数组登记 `hello_world`。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/hello_world.scenario.json` 业务因果断言：
  - 监听并断言控制台 `"Hello world!"`；
  - 断言芯片型号、CPU 核心数与特性；
  - 断言 4MB Flash 与堆内存输出；
  - 断言倒计时日志序列与重启准备。

### Phase 4：Layer G 闭环验证与 Canary 变异击杀
- [x] **Task 4.1**：运行 Gate 1 静态语义门禁 `run_gates.py --gate 1`。
- [x] **Task 4.2**：运行正向 Headless 仿真，确认基线通过并生成 `run-report.json`。
- [x] **Task 4.3**：执行 Canary 变异击杀验证，确保缺陷敏感性 100% 达标。
- [x] **Task 4.4**：运行开源许可分层门禁 `check_license_map.py`。

### Phase 5：Layer G 凭据归档与 13/13 黄金基准交付
- [x] **Task 5.1**：带 `-WriteEvidence` 运行生成官方凭据，更新 `checklist.data.json` 晋升为 `verified` 并生成机器审计签署。
- [x] **Task 5.2**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。
- [x] **Task 5.3**：运行 `evidence_verifier.py --verify-all` 进行全量一致性复核（达成 13/13 黄金基准）。
- [x] **Task 5.4**：执行规范 Git 原子提交并汇报成果。
