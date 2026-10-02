<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 1 P1 标杆 `system/esp_event/default_event_loop` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 1（核心系统与事件分发）P1 标杆治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261003-LANE1-P1-DEFAULT-EVENT-LOOP-GOVERNANCE-v1.0 |
| 任务目标 | 达成 `system/esp_event/default_event_loop`（Display ID: 125）零修改镜像治理与确定性仿真认证 |
| 泳道与优先级 | **Lane 1（系统生命周期与核心事件分发） / P1 标杆** |
| 状态 | 🟢 **Complete (Verified & Audited)** |
| 日期 | 2026-10-03 |
| 上游路径 | `examples/system/esp_event/default_event_loop` |
| 目标载体目录 | `wink-micro-app/vendor/esp_idfv61/system/esp_event_default_event_loop/` |
| 场景路径 | `system/esp_event_default_event_loop/unisim-scenarios/default_event_loop.scenario.json` |
| 依赖能力 | `cap.core.fiber_task`, `cap.core.sync_tokens`, `cap.net.event_pump` |
| 交付工件 | 原厂零修改代码镜像、UniSim 确定性场景、仿真执行报告、Gate 1 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在完成入门级启动（`hello_world`）、GPIO 通用中断（`generic_gpio`）和外设通信总线攻坚后，系统进入系统级异步事件分发与定时器调度的关键基础设施领域。
`system/esp_event/default_event_loop`（Display ID: 125）是 ESP-IDF 官方用于展示系统默认事件循环机制（Default Event Loop）的黄金标准示例：
- 演示默认事件循环的创建与生命周期管理（`esp_event_loop_create_default`）；
- 演示多实例事件处理器注册与独立上下文句柄（`esp_event_handler_instance_register`）；
- 演示泛型事件处理器与通配匹配（`TIMER_EVENTS/ESP_EVENT_ANY_ID` 以及 `ESP_EVENT_ANY_BASE/ESP_EVENT_ANY_ID`）；
- 演示动态注销特定实例处理器（`esp_event_handler_instance_unregister`）；
- 演示基于 FreeRTOS 任务与高精度软件定时器（`esp_timer`）双源并发投递深拷贝事件（`esp_event_post`）并异步派发的完整因果链。

### 1.2 战略收益
1. **打通事件总线真实业务闭环**：验证 `esp_event` 框架在真实双源并发（任务源 + 定时器源）驱动下的深拷贝队列入队、快照保护遍历、实例注销幂等性与安全派发。
2. **闭环补齐 `esp_timer` 软件定时器能力**：将之前处于 `ESP_ERR_NOT_SUPPORTED` 挡板状态的 `esp_timer` 运行时 API（`esp_timer_create`、`start_periodic`、`stop`、`delete`）依托底座 FreeRTOS 软件定时器守护任务完成高可靠实现，消除底座定时器断档。
3. **消除核心系统层（System）断档**：使 Lane 1 的核心事件循环标杆（Display ID: 125）达成正式交付。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 上游路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\system\esp_event\default_event_loop`；
- 镜像文件保持原汁原味，校验并锁定 SHA-256：
  - `main.c`: `be65a6cee63ef66b8309ffcc181423521697dc1a1c57eea87603e5dee1187c4f`；
  - `event_source.h`: `47134f12e87f841dedc32ccdbc46d3494da4473ff1530e82e5d15446acaf0549`；
- 严禁对官方源码进行任何修改，所有适配统一通过 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json` 及底座框架门面完成。

### 2.2 事件循环与定时器桥接模型 (Layer A & Runtime)
- `esp_event`：已具备完整的 `sys_evt` 守护任务与快照派发机制，支持深拷贝负载投递与实例标识注销。
- `esp_timer` 实现：
  - 新建模块 `wink-micro-os/frameworks/esp_idf/src/core/esp_timer.c`，并加入 `esp_idf_sources.cmake`；
  - `esp_timer_create` 映射至 FreeRTOS `xTimerCreate`，桥接回调函数；
  - `esp_timer_start_periodic` / `esp_timer_start_once` 支持微秒向 FreeRTOS Tick 精确转换，动态维护周期重载模式；
  - `esp_timer_stop` / `esp_timer_delete` 释放定时器句柄与资源；
  - 在 `sdkconfig.h` 中注入控制台输出与 UART0 桥接，保证控制台日志稳定投递至 UniSim 观测总线。

### 2.3 确定性因果时序与断言设计 (Layer S)
- 业务因果链路：
  - 启动阶段：断言供电 3.3V，断言输出 `"setting up"` 与 `"starting event sources"`；
  - 定时器启动事件：断言投递与多实例接收：
    - `"TIMER_EVENTS:TIMER_EVENT_STARTED: timer_started_handler, instance 0"`
    - `"TIMER_EVENTS:TIMER_EVENT_STARTED: timer_started_handler, instance 1"`
    - `"TIMER_EVENTS:TIMER_EVENT_STARTED: timer_started_handler_2"`
    - `"TIMER_EVENTS:TIMER_EVENT_STARTED: all_event_handler"`
  - 任务迭代事件：
    - 迭代 1：断言接收 `"TASK_EVENTS:TASK_ITERATION_EVENT: task_iteration_handler, executed 1 times"`；
    - 迭代 3：断言注销 `"TASK_EVENTS:TASK_ITERATION_EVENT: unregistering task_iteration_handler"`；
  - 定时器周期超时与终止：
    - 断言接收 `"TIMER_EVENTS:TIMER_EVENT_EXPIRY: timer_expiry_handler, executed 1 out of 3 times"`；
    - 断言超时 3 次后停止与删除定时器：
      - `"TIMER_EVENTS:TIMER_EVENT_STOPPED: timer_stopped_handler"`
      - `"TIMER_EVENTS:TIMER_EVENT_STOPPED: deleted timer event source"`。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/system/esp_event_default_event_loop/`。
- [x] **Task 1.2**：镜像 `main.c` 与 `event_source.h`，核验并固化 SHA-256。
- [x] **Task 1.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 1.4**：在 `run_esp32_headless_evidence.ps1` 登记 `esp_event_default_event_loop`。

### Phase 2：Layer A 底座对齐与 `esp_timer` 软件定时器驱动
- [x] **Task 2.1**：在 FreeRTOS 定时器层补充 `vTimerSetReloadMode` 支持动态周期模式切换。
- [x] **Task 2.2**：实现 `src/core/esp_timer.c`（`esp_timer_create`、`start_periodic`、`start_once`、`stop`、`delete`），将其纳入 `esp_idf_sources.cmake` 并移除 `esp_system.c` 中的旧 Fail-Loud 桩。
- [x] **Task 2.3**：更新 `test_freertos_timers.c` 中的测试，确保 `esp_timer` 单元测试合规。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/default_event_loop.scenario.json` 业务因果断言集合。
- [x] **Task 3.2**：校准 `checklist.data.json` 中该项的 `target_app_dir` 与 `scenario_path`。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App esp_event_default_event_loop`，确保正向仿真 100% 通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：注入非等价业务变异（如篡改事件 ID 派发或屏蔽定时器停止），验证指定断言在窗口期内精准失败并被捕获（Canary Kill 100%）。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署（`auditor: loop_sop_daemon`）。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 `run_gates.py --gate 1` 确保 Gate 1 静态语义门禁 100% 通过。
- [x] **Task 7.2**：运行 `evidence_verifier.py --verify-all` 达成 18/18 全量全绿通过。
- [x] **Task 7.3**：运行 `check_license_map.py` 确保开源许可分层合规。

### Phase 8：原子提交与交付归档
- [x] **Task 8.1**：按照 Git Commit Rules 执行原子逻辑提交。
- [x] **Task 8.2**：更新计划状态为 Complete。
