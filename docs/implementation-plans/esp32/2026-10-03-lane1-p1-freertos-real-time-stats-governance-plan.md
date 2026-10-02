<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 1 P1 标杆 `system/freertos/real_time_stats` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 1（核心系统与内核调度）P1 标杆治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261003-LANE1-P1-FREERTOS-REAL-TIME-STATS-GOVERNANCE-v1.0 |
| 任务目标 | 达成 `system/freertos/real_time_stats`（Display ID: 132）零修改镜像治理与确定性仿真认证 |
| 泳道与优先级 | **Lane 1（核心系统与内核调度） / P1 标杆** |
| 状态 | 🟢 **Complete** |
| 日期 | 2026-10-03 |
| 上游路径 | `examples/system/freertos/real_time_stats` |
| 目标载体目录 | `wink-micro-app/vendor/esp_idfv61/system/freertos_real_time_stats/` |
| 场景路径 | `system/freertos_real_time_stats/unisim-scenarios/real_time_stats.scenario.json` |
| 依赖能力 | `cap.core.fiber_task`, `cap.core.sync_tokens` |
| 交付工件 | 原厂零修改代码镜像、UniSim 确定性场景、仿真执行报告、Gate 1 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在相继完成 `get-started/hello_world`（入门生命周期）、`peripherals/gpio/generic_gpio`（基础 IO 与中断唤醒）以及 `system/esp_event/default_event_loop`（系统事件循环总线）之后，Lane 1（核心系统与内核调度）进入内核多任务健康监控、高精度运行时状态统计与性能度量领域。
`system/freertos/real_time_stats`（Display ID: 132）是 ESP-IDF 官方演示 FreeRTOS 内核运行时统计特性（Run Time Statistics）的黄金标准示例：
- 演示多任务并发与计数信号量同步（`xSemaphoreCreateCounting`）；
- 演示通过 `uxTaskGetSystemState` 获取全系统任务状态快照（句柄、状态、优先级、运行时间、高水位线）；
- 演示双重采样（Two-Point Sampling）与微秒/Tick 差值计算任务 CPU 占用百分比；
- 演示内核代际句柄的可靠匹配与已销毁/新建任务的状态识别。

### 1.2 战略收益
1. **打通内核度量与状态快照机制**：深度验证 `uxTaskGetSystemState`、`uxTaskGetNumberOfTasks` 在多任务并发调度下的快照一致性与总运行时间单调递增性。
2. **对齐 FreeRTOS 运行时统计类型体系**：在 FreeRTOS 门面层补齐 `configRUN_TIME_COUNTER_TYPE` 与任务运行时间累计，消除内核性能度量断档。
3. **消除核心系统层（System/FreeRTOS）关键 P1 断档**：使 Lane 1 的核心多任务统计标杆达成正式交付。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 上游路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\system\freertos\real_time_stats`；
- 镜像文件保持原汁原味，校验并锁定 SHA-256：
  - `main/real_time_stats_example_main.c`: `e90ba48a308b9e5735de50e86776ae2b9731fac05ede5bd2a0ad9efef73ab29c`；
- 严禁对官方源码进行任何修改，所有适配统一通过 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json` 及底座框架门面完成。

### 2.2 内核度量与门面适配模型 (Layer A & Runtime)
- `configRUN_TIME_COUNTER_TYPE` 宏支持：
  - 在 `FreeRTOS.h` / `FreeRTOSConfig.h` 提供缺省类型定义（`uint32_t`）；
- `uxTaskGetSystemState` 完善：
  - 任务句柄与 `s_tcb[i].token` 强绑定对齐，确保首尾快照两点匹配 100% 成功；
  - `pulTotalRunTime` 基于系统微秒时间 `pal_os_get_us()` 或 Tick 计数推进，确保两点采样差值 `total_elapsed_time > 0`；
  - 任务级 `ulRunTimeCounter` 在任务切换/运行期间累加，准确反映各任务的执行耗时。
- `CONFIG_FREERTOS_NUMBER_OF_CORES`：
  - 在 `sdkconfig.h` 中显式注入 `CONFIG_FREERTOS_NUMBER_OF_CORES 1`，保证占用百分比计算公式分母合法。
- `WINK_ESP_SIM_PROFILE` 自动检测：
  - 在 `esp_idf_target.cmake` 中支持由 `wink-app.json` 的 `simulation_profile`（如 `STANDARD` = 16 任务）动态选取池容量。

### 2.3 确定性因果时序与断言设计 (Layer S)
- 业务因果链路：
  - 启动阶段：断言供电 3.3V，断言主任务创建 6 个 spin 任务与 1 个 stats 任务；
  - 采样周期：
    - 断言输出 `"Getting real time stats over 100 ticks"`；
    - 断言输出表格表头 `"| Task | Run Time | Percentage"`；
    - 断言匹配到各个 spin 任务及 stats 任务的输出；
    - 断言输出 `"Real time stats obtained"`。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/system/freertos_real_time_stats/`。
- [x] **Task 1.2**：镜像 `real_time_stats_example_main.c`，核验并固化 SHA-256。
- [x] **Task 1.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 1.4**：在 `run_esp32_headless_evidence.ps1` 登记 `freertos_real_time_stats`。

### Phase 2：Layer A 底座对齐与内核运行统计增强
- [x] **Task 2.1**：在 `FreeRTOSConfig.h` / `FreeRTOS.h` 中补充 `configRUN_TIME_COUNTER_TYPE` 支持。
- [x] **Task 2.2**：优化 `freertos_task.c` 中的 `uxTaskGetSystemState`，完善 `s->xHandle` 与 `ulRunTimeCounter` 运行时度量。
- [x] **Task 2.3**：在 `esp_idf_target.cmake` 中打通 `simulation_profile` 自动解析，支持多任务标准 Profile。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/real_time_stats.scenario.json` 业务因果断言集合。
- [x] **Task 3.2**：校准 `checklist.data.json` 中该项的 `target_app_dir` 与 `scenario_path`。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App freertos_real_time_stats`，确保正向仿真 100% 通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：注入非等价业务变异，验证指定断言在窗口期内精准失败并被捕获（Canary Kill 100%）。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 `run_gates.py --gate 1` 确保 Gate 1 静态语义门禁 100% 通过。
- [x] **Task 7.2**：运行 `evidence_verifier.py --verify-all` 达成全量全绿通过（19/19）。
- [x] **Task 7.3**：运行 `check_license_map.py` 确保开源许可分层合规。

### Phase 8：原子提交与交付归档
- [x] **Task 8.1**：按照 Git Commit Rules 执行原子逻辑提交。
- [x] **Task 8.2**：更新计划状态为 Complete。
