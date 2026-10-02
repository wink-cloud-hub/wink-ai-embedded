<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 1 P1 标杆 `system/esp_timer` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 1（核心系统与内核调度）P1 标杆治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261003-LANE1-P1-ESP-TIMER-GOVERNANCE-v1.0 |
| 任务目标 | 达成 `system/esp_timer`（Display ID: 127）零修改镜像治理与高保真高精度定时器确定性仿真认证 |
| 泳道与优先级 | **Lane 1（核心系统与高精度定时器） / P1 标杆** |
| 状态 | 🟢 **Complete** |
| 日期 | 2026-10-03 |
| 上游路径 | `examples/system/esp_timer` |
| 目标载体目录 | `wink-micro-app/vendor/esp_idfv61/system/esp_timer/` |
| 场景路径 | `system/esp_timer/unisim-scenarios/system_esp_timer.scenario.json` |
| 依赖能力 | `cap.core.fiber_task`, `cap.core.sync_tokens` |
| 交付工件 | 原厂零修改代码镜像、UniSim 确定性场景、仿真执行报告、Gate 1 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在完成 `get-started/hello_world`（系统生命周期）、`peripherals/gpio/generic_gpio`（基础 IO 与中断唤醒）、`system/esp_event/default_event_loop`（事件循环分发）以及 `system/freertos/real_time_stats`（多任务度量与运行时统计）之后，Lane 1（核心系统与内核调度）进入微秒级高精度定时器与软硬件时间基准对齐领域。
`system/esp_timer`（Display ID: 127）是 ESP-IDF 官方演示高精度定时器（High Resolution Timer）完整功能特性的黄金标准示例：
- 演示多定时器并发（周期性定时器与单次定时器并发运行）；
- 演示绝对时间指定触发（`esp_timer_start_periodic_at` 与 `esp_timer_start_once_at`）；
- 演示单次定时器触发并在回调中动态重启周期性定时器（`esp_timer_stop` + `esp_timer_start_periodic` 以及 `esp_timer_restart_at`）；
- 演示高精度系统时间单调性（`esp_timer_get_time` 微秒度量）；
- 演示定时器状态诊断快照打印（`esp_timer_dump` 格式化导出）；
- 演示优雅停止与销毁清理（`esp_timer_stop` 与 `esp_timer_delete`）。

### 1.2 战略收益
1. **闭环补齐 `esp_timer` 高级调度能力**：在底座 `esp_timer.c` 基础上，完整实现 `start_periodic_at`、`start_once_at`、`restart_at`、`dump` 及 `is_active` 等高阶 API，消除系统定时器能力断档。
2. **打通 `usleep` 与多任务协程推进**：通过在仿真环境中将 `usleep` 精确映射至 FreeRTOS 任务延时（`vTaskDelay`），确保业务任务延时期间后台软件定时器服务任务平滑推进。
3. **消除核心系统层关键 P1 断档**：使 Lane 1 的核心高精度定时器标杆（Display ID: 127）达成正式交付。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 上游路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\system\esp_timer`；
- 镜像文件保持原汁原味，校验并锁定 SHA-256：
  - `main/esp_timer_example_main.c`: `20a88b5266f303339dcc3c6711a4e0aec53a8e40523990f2768de5f2346edc2d`；
- 严禁对官方源码进行任何修改，所有适配统一通过 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json` 及底座框架门面完成。

### 2.2 定时器扩展模型 (Layer A & Runtime)
- `esp_timer_start_periodic_at` 与 `esp_timer_start_once_at`：
  - 解析绝对时间 `alarm_us` / `first_alarm_us`，计算相对延迟 `delay_us = alarm_us - now`；
  - 若已超时则返回 `ESP_ERR_INVALID_ARG`；
  - 先按初始延迟触发，回调后再按 `period_us` 自动周期重载。
- `esp_timer_restart_at`：
  - 重设定时器周期与绝对触发时间并激活。
- `esp_timer_dump`：
  - 遍历所有注册定时器，向 `stream` 输出标准表格格式：
    ```text
    Timer stats:
    Name                  Period      Alarm       
    ...
    ```
- `usleep` 支持：
  - 在 `esp_system.c` 实现 `int usleep(useconds_t us)`，换算为 Tick 调用 `vTaskDelay`，驱动仿真时间步进并允许 `prvTimerTask` 及时分发回调。
- 睡眠策略：
  - 在 `sdkconfig.h` 中配置 `#define SOC_LIGHT_SLEEP_SUPPORTED 0`，避免依赖硬件 RTC 唤醒。

### 2.3 确定性因果时序与断言设计 (Layer S)
- 业务因果链路：
  - 启动阶段：断言供电 3.3V，断言输出 `"Started timers, time since boot:"`；
  - 周期回调：断言匹配到 `"Periodic timer called"` 与 `"Timed periodic timer called"`；
  - 单次触发与周期重设：
    - 断言匹配到 `"One-shot timer called"` 与 `"Restarted periodic timer with 1s period"`；
    - 断言匹配到 `"Timed one-shot timer called"` 与 `"Restarted timed periodic timer with 1s period"`；
  - 状态诊断：断言至少 4 次匹配到 `"Timer stats:"` 与 `"Name                  Period      Alarm"`；
  - 退出清理：断言匹配到 `"Stopped and deleted timers"`。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/system/esp_timer/`。
- [x] **Task 1.2**：镜像 `esp_timer_example_main.c`，核验并固化 SHA-256。
- [x] **Task 1.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 1.4**：在 `run_esp32_headless_evidence.ps1` 登记 `esp_timer`。

### Phase 2：Layer A 底座对齐与高精度定时器扩展
- [x] **Task 2.1**：在 `esp_timer.h` 中导出 `esp_timer_start_once_at`、`esp_timer_start_periodic_at`、`esp_timer_restart_at`、`esp_timer_dump`、`esp_timer_is_active`。
- [x] **Task 2.2**：在 `esp_timer.c` 中完整实现上述 API，维护 `period_us` 与 `alarm_us`。
- [x] **Task 2.3**：在 `esp_system.c` 中实现 `usleep` 桥接 `vTaskDelay`。
- [x] **Task 2.4**：编译并验证 `test_freertos_timers.c` 单测，确保无退化。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/system_esp_timer.scenario.json` 业务因果断言集合。
- [x] **Task 3.2**：校准 `checklist.data.json` 中该项的 `target_app_dir` 与 `scenario_path`。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App esp_timer`，确保正向仿真 100% 通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：注入非等价业务变异，验证指定断言在窗口期内精准失败并被捕获（Canary Kill 100%）。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 `run_gates.py --gate 1` 确保 Gate 1 静态语义门禁 100% 通过。
- [x] **Task 7.2**：运行 `evidence_verifier.py --verify-all` 达成全量全绿通过（20/20）。
- [x] **Task 7.3**：运行 `check_license_map.py` 确保开源许可分层合规。

### Phase 8：原子提交与交付归档
- [x] **Task 8.1**：按照 Git Commit Rules 执行原子逻辑提交。
- [x] **Task 8.2**：更新计划状态为 Complete。
