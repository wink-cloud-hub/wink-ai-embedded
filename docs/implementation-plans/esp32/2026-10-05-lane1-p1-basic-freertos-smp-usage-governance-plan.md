<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 1 P1 核心积木 `system/freertos/basic_freertos_smp_usage` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 1（核心系统与内核调度）P1 标杆治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261005-LANE1-P1-BASIC-FREERTOS-SMP-USAGE-GOVERNANCE-v1.0 |
| 任务目标 | 达成 `system/freertos/basic_freertos_smp_usage`（Display ID: 131）零修改镜像治理与确定性仿真认证 |
| 泳道与优先级 | **Lane 1（系统生命周期与内核调度） / P1 通用积木** |
| 状态 | 🟢 **Complete** |
| 日期 | 2026-10-05 |
| 上游路径 | `examples/system/freertos/basic_freertos_smp_usage` |
| 目标载体目录 | `wink-micro-app/vendor/esp_idfv61/system/freertos_basic_freertos_smp_usage/` |
| 场景路径 | `system/freertos_basic_freertos_smp_usage/unisim-scenarios/system_freertos_basic_freertos_smp_usage.scenario.json` |
| 依赖能力 | `cap.core.fiber_task`, `cap.core.sync_tokens`, `cap.system.console_cmd` |
| 交付工件 | 原厂零修改代码镜像、`esp_console` 门面闭环、UniSim 确定性交互场景、仿真执行报告、Gate 1~5 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在完成 `#125 default_event_loop` 与 `#126 user_event_loops` 双事件循环闭环后，系统核心调度层（Lane 1）已具备多任务并发与事件派发能力。然而，针对 FreeRTOS 任务间同步（队列 `xQueue`、互斥锁 `xSemaphore`、自旋锁 `portMUX_TYPE`、任务通知 `ulTaskNotifyTake`）以及交互式命令行（`esp_console` REPL）的标准原厂示例仍存在断档。

`system/freertos/basic_freertos_smp_usage`（Display ID: 131）是 ESP-IDF 官方用于展示 FreeRTOS SMP 多核调度与任务同步机制的集大成黄金示例：
1. **多核任务绑定与无亲和性调度**（`xTaskCreatePinnedToCore` 与 `tskNO_AFFINITY`）；
2. **多任务队列解耦与缓冲同步**（`xQueueGenericCreate` / `xQueueReceive` / `xQueueGenericSend`）；
3. **互斥锁与自旋锁临界区**（`xSemaphoreCreateMutex` / `portENTER_CRITICAL`）；
4. **轻量任务通知同步**（`ulTaskNotifyTake` / `xTaskNotifyGive`）；
5. **批处理流水线综合协同**（结合队列 + 互斥锁 + 任务通知）；
6. **交互式控制台驱动**（`esp_console` REPL 通过 UART 接收命令分发）。

### 1.2 战略收益
1. **打通 `esp_console` 交互式命令台运行时能力**：构建首个支持串口命令行输入（`INPUT_BUS`）与多指令动态派发的闭环示例。
2. **闭环补齐 FreeRTOS 任务同步全家桶**：验证多任务协作纤程模型下的队列、互斥锁、自旋锁和任务通知因果时序。
3. **收敛 `esp_cpu_get_core_id` 运行时契约**：安全解除 Core 8 范围内的 SLA 阻塞，为多核 SMP 相关示例铺平道路。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 上游路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\system\freertos\basic_freertos_smp_usage/main`；
- 原厂源码逐字节对齐，严格校验并锁定 SHA-256：
  - `basic_freertos_smp_usage.c`: `840337757e2f827cf1f4395402d5316d4569505410ea2a4c61a888c6dd78a451`
  - `basic_freertos_smp_usage.h`: `b7469127cd3c48f6bd96cae33b930ede3656d756b5082a0a846ff6bd13040de9`
  - `create_task_example.c`: `8f94d883f6b9f6ed2aa09e464b6bee682cd2b9bb2109e900837d2e3012712b61`
  - `queue_example.c`: `f30b4a95a71123701523513e88c703dfa7b9c5f5a1e1e52d331ab23a37fce5ff`
  - `lock_example.c`: `62ed678f9a48d85b2b8786c8ae9e8a684ededb7b46754ed734e9a53b84752204`
  - `task_notify_example.c`: `89217861e8e6289a8b047b6ceb54dc27b12c85432760284671c7b0219079bccc`
  - `batch_processing_example.c`: `39099a4ead8deaefc734377cd57b113f89651c30ebf04fc800f026c7333033f2`
- 严禁对官方源码进行任何修改，所有适配统一通过 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json` 及底座框架门面完成。

### 2.2 控制台与 FreeRTOS 同步门面实现 (Layer A & Runtime)
1. **`esp_console` 框架门面**：
   - 在 `wink-micro-os/frameworks/esp_idf/include/esp_console.h` 导出命令注册、参数分割与 REPL 句柄接口；
   - 在 `wink-micro-os/frameworks/esp_idf/src/core/esp_console.c` 实现：
     - 命令注册表管理（`esp_console_cmd_register`、`esp_console_register_help_command`）；
     - 命令行参数解析（`esp_console_split_argv` 纯 C 解析，支持双引号与转义符）；
     - UART 异步交互任务（`esp_console_repl_task` 监听 UART0 输入，自动分发行命令并调用 `.func(argc, argv)`）。
2. **FreeRTOS 队列宏与 CPU 核心查询对齐**：
   - 在 `queue.h` 中补齐 `xQueueGenericCreate` 与 `xQueueGenericSend` 标准宏；
   - 在 `esp_cpu.h` 中实现 `esp_cpu_get_core_id(void)` 返回当前核心 ID（单核/协作模型默认返回 0）。

### 2.3 确定性交互因果时序与断言设计 (Layer S)
- **启动交互链路**：
  - 启动阶段：断言供电 3.3V，断言串口输出控制台就绪提示 `"Please type the component you would like to run."` 及提示符 `"esp32>"`；
- **命令 1：`create_task` 测试**：
  - 激励：`INPUT_BUS` 注入 `"create_task\n"`；
  - 因果断言：断言任务创建成功 `"created task#0"`，以及在核心上调度运行 `"task#0 is running on core#0"`；
- **命令 2：`queue` 测试**：
  - 激励：`INPUT_BUS` 注入 `"queue\n"`；
  - 因果断言：断言队列发送与接收交替 `"sent data = 0"` ➔ `"received data = 0"`。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/system/freertos_basic_freertos_smp_usage/`。
- [x] **Task 1.2**：镜像 7 个原厂 C/H 源码，核验并固化 SHA-256。
- [x] **Task 1.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 1.4**：在 `run_esp32_headless_evidence.ps1` 登记 `freertos_basic_freertos_smp_usage`。

### Phase 2：Layer A 底座对齐与 `esp_console` 门面闭环
- [x] **Task 2.1**：在 `queue.h` 补齐 `xQueueGenericCreate` 与 `xQueueGenericSend`。
- [x] **Task 2.2**：实现 `esp_cpu_get_core_id(void)` 返回 0，消除 Core 8 SLA 阻塞。
- [x] **Task 2.3**：实现 `include/esp_console.h` 与 `src/core/esp_console.c`，纳入 `esp_idf_sources.cmake`。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/system_freertos_basic_freertos_smp_usage.scenario.json` 交互式命令与因果断言。
- [x] **Task 3.2**：校准 `checklist.data.json` 中该项的 `target_app_dir` 与 `scenario_path`。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App freertos_basic_freertos_smp_usage`，确保正向仿真 100% 通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：注入非等价业务变异（如篡改命令匹配或任务创建输出），验证断言在窗口期内精准失败被捕获（Canary Kill 100%）。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署（`auditor: loop_sop_daemon`）。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 `run_gates.py --gate 1` 确保 Gate 1 静态语义门禁 100% 通过。
- [x] **Task 7.2**：运行 `evidence_verifier.py --verify-all` 达成全量 32/32 全绿通过。
- [x] **Task 7.3**：运行 `check_license_map.py` 确保开源许可分层合规。

### Phase 8：原子提交与交付归档
- [x] **Task 8.1**：按照 Git Commit Rules 执行原子逻辑提交。
- [x] **Task 8.2**：更新计划状态为 Complete。
