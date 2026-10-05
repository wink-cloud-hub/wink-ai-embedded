<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 2 P1 核心外设 `peripherals/uart_async_rxtxtasks` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 2（外设总线与通用通信）P1 基线治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261005-LANE2-P1-UART-ASYNC-RXTXTASKS-GOVERNANCE-v1.0 |
| 任务目标 | 达成 `peripherals/uart_async_rxtxtasks`（Display ID: 94）零修改镜像治理与确定性仿真认证 |
| 泳道与优先级 | **Lane 2（外设总线与通用通信） / P1 核心通信** |
| 状态 | 🟢 **Complete / Verified** |
| 日期 | 2026-10-05 |
| 上游路径 | `examples/peripherals/uart/uart_async_rxtxtasks` |
| 目标载体目录 | `wink-micro-app/vendor/esp_idfv61/peripherals/uart_uart_async_rxtxtasks/` |
| 场景路径 | `peripherals/uart_uart_async_rxtxtasks/unisim-scenarios/peripherals_uart_uart_async_rxtxtasks.scenario.json` |
| 依赖能力 | `cap.bus.uart_stream`, `cap.core.fiber_task`, `cap.core.sync_tokens`, `cap.irq.edge_trigger` |
| 交付工件 | 原厂零修改代码镜像、UniSim 确定性时序场景、仿真执行报告、Gate 1~5 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在嵌入式系统中，异步全双工通信（Asynchronous Full-Duplex UART）是传感器数据流采集与工业控制指令交互的基石架构。`peripherals/uart_async_rxtxtasks`（Display ID: 94）是 ESP-IDF 官方展示通过独立 FreeRTOS RX/TX 任务协同完成非阻塞异步串口数据收发与缓冲打印的经典核心示例。

核心验证链路：
1. **多任务并发调度与初始化**：主任务配置安装 UART1 驱动并分别派生独立 RX 任务与 TX 任务；
2. **TX 任务主动周期发送**：TX 任务向 UART1 总线写入 `"Hello world"` 并通过控制台日志宣告发送字节数；
3. **RX 任务外部激励响应**：外部通过 `INPUT_BUS` 向 UART1 RX 注入异步报文，RX 任务非阻塞读取并格式化打印 HexDump 与字符串日志。

### 1.2 战略收益
1. **完善全双工异步串口通信基准**：建立双向并发总线数据传输仿真检验模型。
2. **原厂源码零修改落地**：锁定 SHA-256（`1dcbfc00c43e89ef61df60b4fbacd2b28f9891cf0c1673c2489b27343cc2862f`）。
3. **闭环双任务因果断言**：覆盖供电轨、总线 TX 载荷、总线 RX 输入与控制台打印响应全链路。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 上游路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\peripherals\uart\uart_async_rxtxtasks/main`；
- 原厂源码逐字节对齐，严格校验并锁定 SHA-256：
  - `uart_async_rxtxtasks_main.c`: `1dcbfc00c43e89ef61df60b4fbacd2b28f9891cf0c1673c2489b27343cc2862f`
- 严禁对官方源码进行任何修改，所有适配统一通过 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json` 完成。

### 2.2 确定性因果时序与断言设计 (Layer S)
- **步骤 1 (供电就绪)**：50ms 断言系统供电轨维持 3.3V；
- **步骤 2 (TX 载荷断言)**：在 `[0ms, 1500ms]` 窗口内断言 UART1 TX 吐出 `"Hello world"`；
- **步骤 3 (RX 外部激励)**：在 100ms 向 UART1 RX 注入 `"WINK_ASYNC_UART"`；
- **步骤 4 (控制台接收日志断言)**：在 `[100ms, 2500ms]` 窗口内断言控制台（UART0）输出匹配 `"Read 15 bytes: 'WINK_ASYNC_UART'"`。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/peripherals/uart_uart_async_rxtxtasks/`。
- [x] **Task 1.2**：拷贝原厂 `uart_async_rxtxtasks_main.c` 源码，核验并固化 SHA-256。
- [x] **Task 1.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h`、`wink-app.json` 与 `device-tree.json`。
- [x] **Task 1.4**：在 `run_esp32_headless_evidence.ps1` 登记 `uart_uart_async_rxtxtasks`。

### Phase 2：Layer A 底座对齐与运行时验证
- [x] **Task 2.1**：校验 FreeRTOS 任务与 UART 抽象驱动完备性。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/peripherals_uart_uart_async_rxtxtasks.scenario.json` 确定性断言。
- [x] **Task 3.2**：校准 `checklist.data.json` 中条目 #94 的 `scenario_path`、`positive_cases` 与 `negative_cases`。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App uart_uart_async_rxtxtasks` 验证全绿通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：注入非等价业务变异（如篡改断言 matcher），验证 100% 击杀报红并恢复。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署（`auditor: loop_sop_daemon`）。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 `run_gates.py --gate 1` 确保 Gate 1 静态语义门禁 100% 通过。
- [x] **Task 7.2**：运行 `evidence_verifier.py --verify-all` 达成全量 35/35 全绿通过。
- [x] **Task 7.3**：运行 `check_license_map.py` 确保开源许可分层合规。

### Phase 8：原子提交与交付归档
- [x] **Task 8.1**：按照 Git Commit Rules 执行原子逻辑提交。
- [x] **Task 8.2**：更新计划状态为 Complete。
