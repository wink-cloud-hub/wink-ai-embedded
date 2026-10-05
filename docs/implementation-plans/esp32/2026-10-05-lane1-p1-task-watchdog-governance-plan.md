<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 1 P1 核心积木 `system/task_watchdog` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 1（系统生命周期与内核容错）P1 任务看门狗治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261005-LANE1-P1-TASK-WATCHDOG-GOVERNANCE-v1.0 |
| 任务目标 | 达成 `system/task_watchdog`（Display ID: 154）零修改镜像治理与确定性仿真认证 |
| 泳道与优先级 | **Lane 1（系统生命周期与内核容错） / P1 通用积木** |
| 状态 | 🟢 **Complete** |
| 日期 | 2026-10-05 |
| 上游路径 | `examples/system/task_watchdog` |
| 目标载体目录 | `wink-micro-app/vendor/esp_idfv61/system/task_watchdog/` |
| 场景路径 | `system/task_watchdog/unisim-scenarios/system_task_watchdog.scenario.json` |
| 依赖能力 | `cap.core.fiber_task`, `cap.core.sync_tokens` |
| 交付工件 | 原厂零修改代码镜像、`esp_task_wdt` 底座支持、UniSim 确定性时序场景、仿真执行报告、Gate 1~5 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在完成 `#125 default_event_loop`、`#126 user_event_loops`、`#127 esp_timer` 以及 `#131 freertos_basic_freertos_smp_usage` 后，系统的事件机制、高精定时器与多任务调度已全面就绪。然而，在嵌入式系统中保障任务不发生饥饿、死锁与失控的关键机制 —— **任务看门狗（Task Watchdog Timer, TWDT）**，尚缺少原生运行时代理解析与官方标准测试载体。

`system/task_watchdog`（Display ID: 154）是 ESP-IDF 官方验证 TWDT 监控生命周期的标准核心示例：
1. **TWDT 基础设施初始化与参数配置**（`esp_task_wdt_init`，配置超时时间 `timeout_ms` 与空闲核心掩码 `idle_core_mask`）；
2. **多重订阅模型**：
   - 任务级自订阅（`esp_task_wdt_add(NULL)` 与状态查询 `esp_task_wdt_status(NULL)`）；
   - 用户态句柄动态订阅（`esp_task_wdt_add_user("func_a", &hdl)`）；
3. **周期性喂狗心跳协作**（`esp_task_wdt_reset()` 与 `esp_task_wdt_reset_user(hdl)`）；
4. **安全退订与资源清理**（`esp_task_wdt_delete_user`、`esp_task_wdt_delete` 与 `esp_task_wdt_deinit`）；
5. **任务间退出通知**（`xTaskNotifyGive` 与 `ulTaskNotifyTake` 协同主任务安全回收）。

### 1.2 战略收益
1. **闭环补齐系统内核生命周期监视器**：构建首个支持任务自检与多实体心跳订阅（TWDT）的仿真契约。
2. **零修改落地原厂源码**：严格锁定 SHA-256（`5855b893bf480e818fb8421b23bcbdd50480c9d21a013725de9df14e4e56dc96`）。
3. **深化 Lane 1 护城河**：使 WinkMicroOS 在 Wasm 仿真中具备行为级保真的系统级看门狗容错与健康监控语义。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 上游路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\system\task_watchdog/main`；
- 原厂源码逐字节对齐，严格校验并锁定 SHA-256：
  - `task_watchdog_example_main.c`: `5855b893bf480e818fb8421b23bcbdd50480c9d21a013725de9df14e4e56dc96`
- 严禁对官方源码进行任何修改，所有适配统一通过 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json` 及底座框架门面完成。

### 2.2 TWDT 底座门面实现 (Layer A & Runtime)
- 接口规范：使用已存在的 [esp_task_wdt.h](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/esp_task_wdt.h) 导出 ABI。
- 实体实现：在 `wink-micro-os/frameworks/esp_idf/src/core/esp_task_wdt.c` 实现：
  - 全局 TWDT 控制块（状态、超时配置、互斥锁保护）；
  - 任务订阅池（支持最多 16 个并发任务注册，以 `TaskHandle_t` 索引，默认 NULL 绑定当前任务）；
  - 用户订阅池（支持最多 16 个自定义心跳对象注册，以动态 handle 索引）；
  - `esp_task_wdt_reset()` / `esp_task_wdt_reset_user()` 刷新时间戳；
  - `esp_task_wdt_status()` 返回当前任务订阅状态（`ESP_OK` 或 `ESP_ERR_NOT_FOUND`）；
  - 纳入 `esp_idf_sources.cmake` 统一构建。

### 2.3 确定性因果时序与断言设计 (Layer S)
- **启动交互链路**：
  - 启动阶段：断言供电 3.3V；
  - 断言初始化：`"TWDT initialized"`；
  - 断言订阅成功：`"Subscribed to TWDT"`；
  - 断言主任务等待：`"Delay for 10 seconds"`；
  - 断言退订注销：`"Unsubscribed from TWDT"`；
  - 断言看门狗去初始化：`"TWDT deinitialized"`；
  - 断言生命周期完整闭环：`"Example complete"`。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/system/task_watchdog/`。
- [x] **Task 1.2**：拷贝原厂 `task_watchdog_example_main.c` 源码，核验并固化 SHA-256。
- [x] **Task 1.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h`、`wink-app.json` 与 `device-tree.json`。
- [x] **Task 1.4**：在 `run_esp32_headless_evidence.ps1` 登记 `task_watchdog`。

### Phase 2：Layer A 底座实现与 `esp_task_wdt` 支持
- [x] **Task 2.1**：在 `src/core/esp_task_wdt.c` 实现看门狗管理逻辑与心跳 API。
- [x] **Task 2.2**：将 `src/core/esp_task_wdt.c` 纳入 `esp_idf_sources.cmake`。
- [x] **Task 2.3**：确保 `include/esp_task_wdt.h` 声明与实现链接契约对齐。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/system_task_watchdog.scenario.json` 确定性断言。
- [x] **Task 3.2**：校准 `checklist.data.json` 中条目 #154 的 `scenario_path`、`positive_cases` 与 `negative_cases`。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App task_watchdog` 验证全绿通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：注入非等价业务变异（如篡改断言 matcher），验证 100% 击杀报红并恢复。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署（`auditor: loop_sop_daemon`）。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 `run_gates.py --gate 1` 确保 Gate 1 静态语义门禁 100% 通过。
- [x] **Task 7.2**：运行 `evidence_verifier.py --verify-all` 达成全量 33/33 全绿通过。
- [x] **Task 7.3**：运行 `check_license_map.py` 确保开源许可分层合规。

### Phase 8：原子提交与交付归档
- [x] **Task 8.1**：按照 Git Commit Rules 执行原子逻辑提交。
- [x] **Task 8.2**：更新计划状态为 Complete。
