<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 4 P1 标杆 `peripherals/dac/dac_cosine_wave` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 4（模拟量转换与电学传感）P1 标杆治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261003-LANE4-P1-DAC-COSINE-WAVE-GOVERNANCE-v1.0 |
| 任务目标 | 达成 `peripherals/dac/dac_cosine_wave`（Display ID: 13）零修改镜像治理与确定性仿真认证 |
| 泳道与优先级 | **Lane 4（模拟电学与信号发生） / P1 标杆** |
| 状态 | 🟢 **Complete** |
| 日期 | 2026-10-03 |
| 上游路径 | `examples/peripherals/dac/dac_cosine_wave` |
| 目标载体目录 | `wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_cosine_wave/` |
| 场景路径 | `peripherals/dac_dac_cosine_wave/unisim-scenarios/peripherals_dac_dac_cosine_wave.scenario.json` |
| 依赖能力 | `cap.analog.adc_oneshot`, `cap.analog.dac_out`, `cap.core.fiber_task`, `cap.core.sync_tokens` |
| 交付工件 | 原厂零修改代码镜像、UniSim 确定性场景、仿真执行报告、Gate 1 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在相继攻克 `adc_oneshot_read` (#004)、`dac_dac_oneshot` (#014) 以及 `adc_continuous_read` (#003) 之后，系统模拟外设（ADC/DAC）的主干能力已全面确立。
`peripherals/dac/dac_cosine_wave`（Display ID: 13）是 ESP-IDF 官方演示硬件余弦波发生器（Cosine Wave Generator / CWG）特性的代表性示例：
- 演示通过 `driver/dac_cosine.h` 配置 DAC0 (GPIO 25) 和 DAC1 (GPIO 26) 双通道硬件余弦发生器；
- 演示独立频率（1kHz/8kHz）、相位（0°/180°）与衰减幅度（0dB/-6dB）配置；
- 演示多通道同步启动（`dac_cosine_start`）；
- 演示内部 ADC2 单元（`adc_oneshot_read`）周期采样回读 DAC 输出引脚电压并打印：
  `"DAC channel 0 value: %4d\tDAC channel 1 value: %4d\n"`。

### 1.2 战略收益
1. **闭环 DAC 硬件信号发生器驱动体系**：在已有 `dac_oneshot` 基础上补充 `dac_cosine` 门面驱动，消除模拟波形输出能力断档。
2. **打通 DAC 余弦波与 ADC2 周期回读因果闭环**：验证多任务协作下模拟通道注入/回读的确定性调度与控制台输出。
3. **消除 Lane 4 关键 P1 缺口**：使全量清单按 Display ID 升序的第一顺位未交付项（Display ID: 13）达成正式交付。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 上游路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\peripherals\dac\dac_cosine_wave`；
- 镜像文件：`main/dac_cosine_example_main.c` 落地至 `wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_cosine_wave/main.c`；
- 校验并锁定 SHA-256：
  - `main.c`: `f4eb206bc2430b50255051b8e2350581c277f5979b1a0bcbbcf68ce2b6cfe8c`；
- 严禁对官方源码进行任何修改，所有适配统一通过 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json` 及底座框架门面完成。

### 2.2 DAC 余弦波发生器门面模型 (Layer A)
- 头文件 `wink-micro-os/frameworks/esp_idf/include/driver/dac_cosine.h`：
  - 导出 `dac_cosine_config_t`, `dac_cosine_handle_t`；
  - 导出 `dac_cosine_new_channel`、`dac_cosine_start`、`dac_cosine_stop`、`dac_cosine_del_channel`；
- 句柄代际令牌：
  - 在 `esp_sim_handle.h` 登记 `ESP_SIM_HANDLE_DAC_COSINE = 15`；
- 驱动实现 `src/drivers/esp_dac.c`：
  - 静态数组维护 DAC Cosine 通道配置与状态（活跃标志、频率、相位、衰减）；
  - 支持 Wasm/Host 跨平台同源编译；
- 拓扑与回读闭环：
  - DAC 通道 0 绑定 GPIO 25，DAC 通道 1 绑定 GPIO 26；
  - 在场景中通过 `INPUT_ANALOG` 向 GPIO 25 与 GPIO 26 注入对应波形电平，由 `adc_oneshot_read` 采集并打印。

### 2.3 确定性因果时序与断言设计 (Layer S)
- 业务因果链路：
  - 供电断言：3.3V 稳定供电；
  - 模拟激励：周期注入模拟电压信号至 GPIO 25 与 GPIO 26；
  - 业务出口断言：断言控制台持续输出匹配正则表达式 `"DAC channel 0 value:\\s+\\d+\\s+DAC channel 1 value:\\s+\\d+"`；
  - 变异敏感性（Canary）：变异断言匹配预期（如篡改格式或预期匹配失败值），确保 100% 击杀。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_cosine_wave/`。
- [x] **Task 1.2**：镜像 `dac_cosine_example_main.c` 为 `main.c`，固化 SHA-256。
- [x] **Task 1.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 1.4**：在 `run_esp32_headless_evidence.ps1` 登记 `dac_dac_cosine_wave`。

### Phase 2：Layer A 底座对齐与 DAC Cosine Wave 驱动实现
- [x] **Task 2.1**：新建 `driver/dac_cosine.h`，导出标准配置与 API。
- [x] **Task 2.2**：在 `esp_sim_handle.h` 登记 `ESP_SIM_HANDLE_DAC_COSINE = 15`。
- [x] **Task 2.3**：在 `src/drivers/esp_dac.c` 实现余弦波通道生命周期与启停。
- [x] **Task 2.4**：编译验证底座框架。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/peripherals_dac_dac_cosine_wave.scenario.json` 业务因果断言集合。
- [x] **Task 3.2**：核验 `checklist.data.json` 中该项元数据。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App dac_dac_cosine_wave`，确保正向仿真 100% 通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：注入非等价业务变异，验证指定断言在窗口期内精准失败并被捕获（Canary Kill 100%）。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 `run_gates.py --gate 1` 确保 Gate 1 静态语义门禁 100% 通过。
- [x] **Task 7.2**：运行 `evidence_verifier.py --verify-all` 达成全量全绿通过（21/21）。
- [x] **Task 7.3**：运行 `check_license_map.py` 确保开源许可分层合规。

### Phase 8：原子提交与交付归档
- [x] **Task 8.1**：按照 Git Commit Rules 执行原子逻辑提交。
- [x] **Task 8.2**：更新计划状态为 Complete。
