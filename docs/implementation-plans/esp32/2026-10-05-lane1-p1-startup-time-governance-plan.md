<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 1 P1 核心积木 `system/startup_time` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 1（系统生命周期与快速启动）P1 基线治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261005-LANE1-P1-STARTUP-TIME-GOVERNANCE-v1.0 |
| 任务目标 | 达成 `system/startup_time`（Display ID: 151）零修改镜像治理与确定性仿真认证 |
| 泳道与优先级 | **Lane 1（系统生命周期与内核基线） / P1 通用积木** |
| 状态 | 🟢 **Complete / Verified** |
| 日期 | 2026-10-05 |
| 上游路径 | `examples/system/startup_time` |
| 目标载体目录 | `wink-micro-app/vendor/esp_idfv61/system/startup_time/` |
| 场景路径 | `system/startup_time/unisim-scenarios/system_startup_time.scenario.json` |
| 依赖能力 | `cap.core.fiber_task`, `cap.core.sync_tokens` |
| 交付工件 | 原厂零修改代码镜像、UniSim 确定性时序场景、仿真执行报告、Gate 1~5 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在系统生命周期体系中，**系统启动时间（Startup Time）与日志级别动态调整**是保障工业设备快速冷启动、故障现场快速复原的关键能力。`system/startup_time`（Display ID: 151）作为 ESP-IDF 官方用于基准度量应用冷启动时延与动态恢复 Info 级别日志的微基准官方示例，为整个 Lane 1 提供启动时效性基线保证。

核心验证链路：
1. **冷启动与运行时环境初始化**；
2. **动态调整运行时日志可见性级别**（`esp_log_level_set("*", ESP_LOG_INFO)`）；
3. **完成启动宣告输出**（`ESP_LOGI(TAG, "App started!")`）。

### 1.2 战略收益
1. **完善系统生命周期全集基准**：建立冷启动瞬态输出与确定性日志捕获标准。
2. **原厂源码零修改落地**：锁定 SHA-256（`4754abb465c1cc9adbf910e970b347783ea4596b102e12365c9f4e8abd7789a6`）。
3. **低代码仿真快速吞吐**：验证微秒级冷启动基线。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 上游路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\system\startup_time/main`；
- 原厂源码逐字节对齐，严格校验并锁定 SHA-256：
  - `hello_world_main.c`: `4754abb465c1cc9adbf910e970b347783ea4596b102e12365c9f4e8abd7789a6`
- 严禁对官方源码进行任何修改，所有适配统一通过 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json` 完成。

### 2.2 确定性因果时序与断言设计 (Layer S)
- **启动交互链路**：
  - 启动阶段：断言供电 3.3V 稳定维持；
  - 启动宣告：断言串口输出 `"App started!"`。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/system/startup_time/`。
- [x] **Task 1.2**：拷贝原厂 `hello_world_main.c` 源码，核验并固化 SHA-256。
- [x] **Task 1.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h`、`wink-app.json` 与 `device-tree.json`。
- [x] **Task 1.4**：在 `run_esp32_headless_evidence.ps1` 登记 `startup_time`。

### Phase 2：Layer A 底座对齐与运行时验证
- [x] **Task 2.1**：确保 `esp_log_level_set` 与日志重定向到 UART 接口完备。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/system_startup_time.scenario.json` 确定性断言。
- [x] **Task 3.2**：校准 `checklist.data.json` 中条目 #151 的 `scenario_path`、`positive_cases` 与 `negative_cases`。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App startup_time` 验证全绿通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：注入非等价业务变异（如篡改断言 matcher），验证 100% 击杀报红并恢复。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署（`auditor: loop_sop_daemon`）。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 `run_gates.py --gate 1` 确保 Gate 1 静态语义门禁 100% 通过。
- [x] **Task 7.2**：运行 `evidence_verifier.py --verify-all` 达成全量 34/34 全绿通过。
- [x] **Task 7.3**：运行 `check_license_map.py` 确保开源许可分层合规。

### Phase 8：原子提交与交付归档
- [x] **Task 8.1**：按照 Git Commit Rules 执行原子逻辑提交。
- [x] **Task 8.2**：更新计划状态为 Complete。
