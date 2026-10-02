<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 4 P0 标杆 `peripherals/adc/oneshot_read` 官方示例仿真治理闭环

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261002-LANE4-P0-ADC-ONESHOT-READ-v1.0 |
| 状态 | 📋 **Draft (Pending User Approval)** |
| 日期 | 2026-10-02 |
| 目标平台/SoC | `wasm32-unknown-emscripten` / `esp32 xtensa` 双 Target |
| 工具链/SDK版本| `ESP-IDF v6.1@fff9895c` / `Emscripten 3.1.x` / Node.js 24+ / Python 3.11+ |
| 优先级 | **P0（Lane 4 模拟电学核心筑基标杆）** |
| 治理依据 | [governance-sop-esp](../../../.agents/skills/governance-sop-esp/SKILL.md)<br>[ADR-0001：负数错误码标准](../../decisions/core/0001-error-code-sign-convention.md)<br>[ADR-0002：双 Target 同源编译](../../decisions/core/0002-dual-target-compilation.md)<br>[ADR-0004：编译期静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0012：Fail-Loud 原则](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0083 / ADR-0084：开源许可分层地图](../../decisions/core/0083-open-source-license-boundary.md)<br>[00.5-pal-adc-subsystem-plan.md](../wokwi-dal-type-coverage-type/00.5-pal-adc-subsystem-plan.md) |
| 管辖数据源 | `checklist.data.json`（Display ID: 4, `esp.peripherals.adc.oneshot_read`） |
| 实施目标文件 | `wink-micro-os/frameworks/esp_idf/include/esp_adc/adc_oneshot.h`（新增）<br>`wink-micro-os/frameworks/esp_idf/include/esp_adc/adc_cali.h`（新增）<br>`wink-micro-os/frameworks/esp_idf/include/esp_adc/adc_cali_scheme.h`（新增）<br>`wink-micro-os/frameworks/esp_idf/src/drivers/esp_adc.c`（新增）<br>`wink-micro-os/frameworks/esp_idf/src/core/esp_sim_handle.h`（扩充句柄类别）<br>`wink-micro-os/frameworks/esp_idf/esp_idf_sources.cmake`（登记源文件）<br>`wink-micro-app/vendor/esp_idfv61/peripherals/adc_oneshot_read/`（新建示例应用脚手架）<br>`wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1`（登记 carrier） |
| 验收门禁 | `python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --gate 1`<br>`python .github/scripts/check_license_map.py`<br>Headless 正向基线通过 + Canary 变异 100% 击杀<br>`evidence_verifier.py --verify-all` 凭据验证全绿 |

---

## 一、 战略目标与现状分析

### 1.1 背景与破局意义
在 ESP-IDF 官方示例治理清单中，**Lane 4: 模拟量转换与电学传感** 目前实证完成率为 0%（0/12）。
- **`peripherals/adc/oneshot_read`（Display ID: 4）** 是 Lane 4 中唯一的 **P0 核心筑基标杆**；
- 它是模拟电位器采样、NTC 测温、环境光传感器采集等后续 11 个高级进阶示例与外设驱动的底层先决依赖；
- 攻克该标杆将一举打开 Lane 4 的治理通道，使 6 大并发泳道中已有 4 条泳道（Lane 1, 2, 3, 4）完全攻克 P0 标杆。

### 1.2 底座就绪情况与缺口分析
1. **底层 PAL 就绪状态（已完成）**：
   - 跨平台抽象头文件 `wink-micro-os/pal/include/hal/pal_adc.h` 已定义且合规；
   - Wasm 仿真驱动 `pal_wasm_ch3_adc.c` 已实现（通过 `js_pal_adc_read_norm` 连接前端 PinArbiter 模拟轨，支持 RC 低通滤波与噪声模拟）；
   - ESP32 物理驱动 `pal_hal_adc_esp32.c` 与 Host 驱动均已就绪。
2. **待补齐缺口（Layer A 框架门面层）**：
   - 缺少 ESP-IDF v6.1 的 ADC Oneshot 与 Calibration C-ABI 头文件：
     - `esp_adc/adc_oneshot.h`
     - `esp_adc/adc_cali.h`
     - `esp_adc/adc_cali_scheme.h`
   - 缺少框架门面驱动 `src/drivers/esp_adc.c`：
     - 需将 ESP-IDF 的 `adc_oneshot_unit_handle_t` 映射并下沉到 `pal_adc_acquire()` 与 `pal_adc_read_raw()`；
     - 需提供校准门面 `adc_cali_raw_to_voltage()`，依据 12-bit 分辨率与衰减档位将 raw 数据映射为真实 mV 电压。
3. **应用镜像与治理脚手架缺口**：
   - `wink-micro-app/vendor/esp_idfv61/peripherals/adc_oneshot_read` 尚未在磁盘落盘；
   - 需从本地权威原厂目录导入 `oneshot_read_main.c`，并固化 SHA-256 哈希防篡改；
   - 需编撰符合 `governance-sop-esp` 契约的高保真业务断言场景。

---

## 二、 架构规格与接口设计

### 2.1 C-ABI 头文件契约（Zero-Modification 上游完全兼容）

#### 1. `include/esp_adc/adc_oneshot.h`
- 结构体定义：
  - `adc_oneshot_unit_init_cfg_t`（`unit_id`, `clk_src`, `ulp_mode`）
  - `adc_oneshot_chan_cfg_t`（`atten`, `bitwidth`）
- 句柄定义：`typedef void *adc_oneshot_unit_handle_t;`
- 函数契约：
  ```c
  esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *init_config, adc_oneshot_unit_handle_t *ret_unit);
  esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t handle, adc_channel_t channel, const adc_oneshot_chan_cfg_t *config);
  esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t handle, adc_channel_t channel, int *out_raw);
  esp_err_t adc_oneshot_del_unit(adc_oneshot_unit_handle_t handle);
  ```

#### 2. `include/esp_adc/adc_cali.h` 与 `include/esp_adc/adc_cali_scheme.h`
- 宏定义：
  ```c
  #define ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED  0
  #define ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED   1
  ```
- 结构体与函数：
  - `adc_cali_line_fitting_config_t`
  - `adc_cali_handle_t`
  - `adc_cali_create_scheme_line_fitting()`
  - `adc_cali_delete_scheme_line_fitting()`
  - `adc_cali_raw_to_voltage(adc_cali_handle_t handle, int raw, int *voltage)`

### 2.2 句柄安全与静态池设计（零堆分配）
依据 ADR-0045（零运行时堆）与 ADR-0004（静态分发）：
- 在 `esp_sim_handle.h` 增加：
  - `ESP_SIM_HANDLE_ADC_ONESHOT = 11`
  - `ESP_SIM_HANDLE_ADC_CALI = 12`
- `esp_adc.c` 内部维护静态槽位池：
  - `s_adc_units[2]`（ESP32 仅支持 ADC_UNIT_1 与 ADC_UNIT_2）；
  - `s_cali_schemes[4]`；
  - 严禁调用 `malloc`/`free`。

### 2.3 物理引脚拓扑映射
依据 ESP32 芯片硬件规格及 `include/soc/adc_channel.h`：
- `ADC_UNIT_1` / `ADC_CHANNEL_4` $\rightarrow$ **GPIO 32**
- `ADC_UNIT_1` / `ADC_CHANNEL_5` $\rightarrow$ **GPIO 33**
- `ADC_UNIT_2` / `ADC_CHANNEL_0` $\rightarrow$ **GPIO 4**
在 `wink-app.json` 中明确声明引脚拓扑。

---

## 三、 场景因果与防假绿断言设计 (Per SOP A-1~A-4)

### 3.1 真实业务契约
原厂 `oneshot_read_main.c` 运行逻辑：
1. 初始化 ADC1（配置 Channel 4, Channel 5）与 ADC2（配置 Channel 0）；
2. 初始化 Line Fitting 校准；
3. 进入 `while (1)` 循环：
   - 读 ADC1 Channel 4 原始 Raw 值及校准后的 mV 电压；
   - `vTaskDelay(1000ms)`；
   - 读 ADC1 Channel 5 原始 Raw 值及校准后的 mV 电压；
   - `vTaskDelay(1000ms)`；
   - 读 ADC2 Channel 0 原始 Raw 值及校准后的 mV 电压；
   - `vTaskDelay(1000ms)`。

### 3.2 断言设计（拒绝纯电源假绿 P-1）
场景文件：`unisim-scenarios/adc_oneshot_read.scenario.json`
- **步骤 1**：500ms 检查系统启动与核心电源 3.3V 稳定（辅助检查）；
- **步骤 2**：1200ms 断言控制台/日志输出 ADC1 Channel 4 的真实采样值；
- **步骤 3**：2200ms 断言控制台/日志输出 ADC1 Channel 5 的真实采样值；
- **步骤 4**：3200ms 断言控制台/日志输出 ADC2 Channel 0 的真实采样值；
- **变异击杀设计（Canary）**：
  变异器篡改电压期望值或偏移采样匹配器，基线仿真必须被硬阻断击杀，证明断言对真实模拟采样具备 100% 缺陷敏感性。

---

## 四、 实施任务拆解与执行工序

### Phase 1：框架层门面实现（Layer A）
- [ ] **Task 1.1**：在 `wink-micro-os/frameworks/esp_idf/src/core/esp_sim_handle.h` 登记 `ESP_SIM_HANDLE_ADC_ONESHOT` 与 `ESP_SIM_HANDLE_ADC_CALI`。
- [ ] **Task 1.2**：在 `wink-micro-os/frameworks/esp_idf/include/esp_adc/` 新增 `adc_oneshot.h`、`adc_cali.h`、`adc_cali_scheme.h`。
- [ ] **Task 1.3**：在 `wink-micro-os/frameworks/esp_idf/src/drivers/esp_adc.c` 实现门面驱动，连接底座 `pal_adc`。
- [ ] **Task 1.4**：在 `wink-micro-os/frameworks/esp_idf/esp_idf_sources.cmake` 登记 `esp_adc.c`。

### Phase 2：示例工程创建与原厂镜像固化
- [ ] **Task 2.1**：在 `wink-micro-app/vendor/esp_idfv61/peripherals/adc_oneshot_read/` 建立应用骨架。
- [ ] **Task 2.2**：从本地原厂路径原封不动镜像 `oneshot_read_main.c`，固化哈希 `0bdc7226...`。
- [ ] **Task 2.3**：编写 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [ ] **Task 2.4**：在 `run_esp32_headless_evidence.ps1` 登记 `adc_oneshot_read` carrier 项。

### Phase 3：场景编撰与自动化闭环验证
- [ ] **Task 3.1**：编撰 `unisim-scenarios/adc_oneshot_read.scenario.json` 核心业务断言。
- [ ] **Task 3.2**：运行 Gate 1 静态语义门禁 `run_gates.py --gate 1`。
- [ ] **Task 3.3**：运行正向 Headless 仿真，确认基线通过并生成 `run-report.json`。
- [ ] **Task 3.4**：执行 Canary 变异击杀验证，确保缺陷敏感性 100% 达标。
- [ ] **Task 3.5**：复核许可证门禁 `check_license_map.py`。

### Phase 4：清单同步与交付归档（Delivery）
- [ ] **Task 4.1**：更新 `checklist.data.json` 条目 `esp.peripherals.adc.oneshot_read`，登记 verified 状态与 assets/scenario SHA-256。
- [ ] **Task 4.2**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。
- [ ] **Task 4.3**：运行 `evidence_verifier.py --verify-all` 进行全量一致性复核。
- [ ] **Task 4.4**：执行独立原子 Git 提交。

---

## 五、 风险矩阵与回滚预案

| 风险项 | 等级 | 表现形式 | 应对与回滚策略 |
|---|:---:|---|---|
| **R-1: ADC2 通道冲突** | 低 | 物理 ESP32 上 Wi-Fi 与 ADC2 冲突 | 仿真环境无物理 RF 干扰；若未配置 Wi-Fi，ADC2 独立正常工作。 |
| **R-2: 门面许可越权** | 中 | 头文件引入了不合规的 SPDX 标识 | 框架门面严格遵循 LGPL-3.0-only，运行 `check_license_map.py` 自动化拦截。 |
| **R-3: 基线仿真假绿** | 高 | 仅断言电源轨通过 Gate 1 却未断言 ADC 数据 | 严格执行 A-1~A-4 契约，强校验日志中 `Raw Data` 与 `Cali Voltage`。 |
