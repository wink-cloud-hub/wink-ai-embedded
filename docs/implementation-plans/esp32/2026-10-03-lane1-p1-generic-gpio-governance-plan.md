<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 1 P1 标杆 `peripherals/gpio/generic_gpio` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 1（核心系统与基础硬件交互）P1 标杆治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261003-LANE1-P1-GENERIC-GPIO-GOVERNANCE-v1.0 |
| 任务目标 | 达成 `peripherals/gpio/generic_gpio`（Display ID: 20）零修改镜像治理与确定性仿真认证 |
| 泳道与优先级 | **Lane 1（核心系统与硬件交互） / P1 通用积木标杆** |
| 状态 | 🟢 **Complete (Verified & Audited)** |
| 日期 | 2026-10-03 |
| 上游路径 | `examples/peripherals/gpio/generic_gpio` |
| 目标载体目录 | `wink-micro-app/vendor/esp_idfv61/peripherals/gpio_generic_gpio/` |
| 场景路径 | `peripherals/gpio_generic_gpio/unisim-scenarios/generic_gpio.scenario.json` |
| 依赖能力 | `cap.core.fiber_task`, `cap.core.sync_tokens`, `cap.irq.edge_trigger` |
| 交付工件 | 原厂零修改代码镜像、UniSim 确定性场景、仿真执行报告、Gate 1 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在攻克 `peripherals/spi_master/hd_eeprom` 完成三大数字总线（UART/I2C/SPI）全线贯通后，系统进入通用硬件输入输出与外部事件中断的核心领域。
`peripherals/gpio/generic_gpio`（Display ID: 20）是 ESP-IDF 官方最基础、最典型的通用引脚输入输出与中断分发示例：
- 演示通过 `gpio_config` 批量配置输入引脚（上拉、输入）与输出引脚；
- 演示多模式边沿中断触发（`GPIO_INTR_POSEDGE` 与 `GPIO_INTR_ANYEDGE`）；
- 演示 FreeRTOS 中断服务与队列唤醒分发模型（`gpio_install_isr_service`、`gpio_isr_handler_add`、`xQueueSendFromISR` -> `gpio_task_example`）；
- 演示动态移除与重新挂载 ISR 处理器（`gpio_isr_handler_remove`）。

### 1.2 战略收益
1. **输入输出与中断链路完整实证**：打通 GPIO 输出脉冲通过硬件跳线（Loopback）或外部信号激励触发引脚中断、进而通过 FreeRTOS 队列唤醒工作任务的完整因果链条。
2. **巩固 Phase 3 虚拟中断事件泵**：全面检验 `s_gpio_isr_slots`、`esp_sim_gpio_inject_edge` 与 FreeRTOS ISR 优先级的安全执行机制。
3. **消除基础外设断档**：使 Lane 1/Lane 2 的最基础交互节点（Display ID: 20）实现正式交付。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 上游路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\peripherals\gpio\generic_gpio`；
- 镜像文件：`main/gpio_example_main.c` 保持原汁原味，校验并锁定 SHA-256：
  - `gpio_example_main.c`: `64cc3c1e28607dfa4ea411efe9c16cd83e07b93336ba9c499f8de530c5eabd67`；
- 严禁对官方源码进行任何修改，所有适配统一通过 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json` 及底座框架门面完成。

### 2.2 引脚配置与信号耦合模型 (Layer A & S)
- 默认配置：
  - `CONFIG_GPIO_OUTPUT_0`: 18
  - `CONFIG_GPIO_OUTPUT_1`: 19
  - `CONFIG_GPIO_INPUT_0`: 4
  - `CONFIG_GPIO_INPUT_1`: 5
- 信号回路（Loopback & 激励）：
  - 输出引脚 18 连接至输入引脚 4，输出引脚 19 连接至输入引脚 5；
  - 当应用层在主循环中拉高/拉低 GPIO 18/19 时，通过底座信号耦合或场景自动将电平边沿投递至 GPIO 4/5，触发中断并向 `gpio_evt_queue` 发送事件；
  - 任务 `gpio_task_example` 接收事件并打印 `"GPIO[4] intr, val: ..."` / `"GPIO[5] intr, val: ..."`；
  - 主循环按 1 秒周期翻转输出并打印 `"cnt: 0"`、`"cnt: 1"` ...。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/peripherals/gpio_generic_gpio/`。
- [x] **Task 1.2**：镜像 `main/gpio_example_main.c`，核验并固化 SHA-256。
- [x] **Task 1.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 1.4**：在 `run_esp32_headless_evidence.ps1` 登记 `gpio_generic_gpio`。

### Phase 2：Layer A 底座对齐与信号耦合扩展
- [x] **Task 2.1**：在 `esp_gpio.c` 中支持 GPIO 18->4 与 GPIO 19->5 硬件回路耦合（当输出引脚电平变化时同步联动目标输入引脚电平与边沿中断）。
- [x] **Task 2.2**：确保 `esp_get_minimum_free_heap_size` 等辅助符号正常导出。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/generic_gpio.scenario.json` 业务因果断言：
  - 断言系统 3.3V 供电；
  - 断言控制台日志输出 `"Minimum free heap size:"`；
  - 断言控制台日志输出 `"cnt: 0"`；
  - 断言控制台日志输出 GPIO 中断事件 `"GPIO[4] intr"` 或 `"GPIO[5] intr"`；
  - 断言控制台日志输出 `"cnt: 1"`。
- [x] **Task 3.2**：核验 `checklist.data.json` 中该项的 `target_app_dir` 与 `scenario_path`。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App gpio_generic_gpio`，确保仿真全绿通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：执行注入业务变异，验证指定断言在窗口期内精准失败并被捕获（Canary Kill 100%）。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署（`auditor: loop_sop_daemon`）。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 `run_gates.py --gate 1` 确保 Gate 1 静态语义门禁 100% 通过。
- [x] **Task 7.2**：运行 `evidence_verifier.py --verify-all` 达成 17/17 全量全绿通过。
- [x] **Task 7.3**：运行 `check_license_map.py` 确保开源许可分层合规。

### Phase 8：原子提交与交付归档
- [x] **Task 8.1**：按照 Git Commit Rules 执行原子逻辑提交。
- [x] **Task 8.2**：更新计划状态为 Complete。
