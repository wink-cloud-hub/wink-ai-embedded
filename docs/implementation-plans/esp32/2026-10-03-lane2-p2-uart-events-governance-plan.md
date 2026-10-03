<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 2 P2 标杆 `peripherals/uart/uart_events` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 2（通用数字总线与通信）UART FreeRTOS 事件驱动与模式检测治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261003-LANE2-P2-UART-EVENTS-GOVERNANCE-v1.0 |
| 任务目标 | 达成 `peripherals/uart/uart_events`（Display ID: 98）零修改镜像治理与 FreeRTOS 队列驱动 UART 事件（DATA 与 PATTERN DET）高保真确定性仿真认证 |
| 泳道与优先级 | **Lane 2（通用数字总线与通信） / P2 进阶标杆** |
| 状态 | 🟢 **Complete** |
| 日期 | 2026-10-03 |
| 上游路径 | `examples/peripherals/uart/uart_events` |
| 目标载体目录 | `wink-micro-app/vendor/esp_idfv61/peripherals/uart_uart_events/` |
| 场景路径 | `peripherals/uart_uart_events/unisim-scenarios/peripherals_uart_uart_events.scenario.json` |
| 依赖能力 | `cap.bus.uart`, `cap.core.fiber_task`, `cap.core.sync_tokens` |
| 交付工件 | 原厂零修改代码镜像、UniSim 确定性场景、仿真执行报告、Gate 1 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在 Lane 2（数字总线）中，我们已成功攻克并验证：
- `#020` `peripherals/gpio/generic_gpio`（通用 GPIO 输入输出与中断）
- `#023` `peripherals/i2c/i2c_basic`（现代对象式 I2C Master 与 MPU9250 IMU 传感器通信）
- `#024` `peripherals/i2c/i2c_eeprom`（I2C EEPROM 芯片跨页读写与轮询响应）
- `#073` `peripherals/spi_master/hd_eeprom`（半双工 SPI Master 与 AT93C46D EEPROM 读写）
- `#096` `peripherals/uart/uart_echo`（UART 环形缓冲基础通信）

**`peripherals/uart/uart_events`（Display ID: 98）是 UART 族系中紧随 `uart_echo` 之后最核心的进阶标杆**：
- 演示 FreeRTOS 队列与底层中断 ISR 的解耦驱动机制（`uart_driver_install` 创建事件队列并挂载）；
- 演示关键事件处理分支：
  - `UART_DATA`：数据到达事件，触发 `uart_read_bytes` 读取并回写回显；
  - `UART_PATTERN_DET`：硬件模式检测中断（配置如连续 3 个 `+`），触发 `uart_pattern_pop_pos` 提取特殊指令位置；
  - `UART_FIFO_OVF` / `UART_BUFFER_FULL`：硬件与环形缓冲溢出防御与刷新；
- 在嵌入式工业实践中，AT 指令解析器、流控控制及命令行交互广泛基于此事件模型。

### 1.2 战略收益
1. **完善 UART 事件驱动与 FreeRTOS 队列因果闭环**：验证从 PAL 串口中断到 FreeRTOS 队列事件的异步调度因果。
2. **打通模式检测（Pattern Detect）高级总线特性**：支持类似经典调制解调器 `+++` 转义指令的模式探测与位置提取。
3. **扩展实证交付基线至 24 项**：进一步收敛 Lane 2 数字总线进阶模式。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 权威原厂路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\peripherals\uart\uart_events`
- 镜像文件与锁定 SHA-256：
  - `main/uart_events_example_main.c` -> `wink-micro-app/vendor/esp_idfv61/peripherals/uart_uart_events/uart_events_main.c`:  
    `8d173ac3cd0e1a9fec6dc1bf6de48a865172a43cc5f910635c63e00dbef770de`
- 严禁修改任何原厂源码，所有适配均通过 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json` 完成。

### 2.2 Layer A 底座对齐与 Pattern Detect 门面实现
- 增强 `wink-micro-os/frameworks/esp_idf/src/drivers/esp_uart.c`：
  - 补充 `uart_flush_input`（映射至 `uart_flush`）；
  - 实现 `uart_enable_pattern_det_baud_intr`：记录目标 pattern 字符与重复次数；
  - 实现 `uart_pattern_queue_reset` 与 `uart_pattern_pop_pos`：维护 pattern 命中队列；
  - 在 `on_pal_uart_event` 中，接收数据流时扫描目标模式（如 `+++`），命中时产生 `UART_PATTERN_DET` 事件，并向队列中记录相对位置。
- 许可证合规：`LGPL-3.0-only`，通过 `check_license_map.py` 审查。

### 2.3 Layer S 确定性因果时序与断言设计
- 业务因果链路：
  - 供电断言：`power:VCC_3V3` 稳定供电；
  - 阶段 1：向 UART0 注入普通数据报文（`"HELLO_ESP32_WINK"`），因果断言捕获 `[DATA EVT]:` 与 `[UART DATA]: 1`；
  - 阶段 2：向 UART0 注入模式触发字符（`"+++"`），因果断言捕获 `[UART PATTERN DETECTED] pos: 0`，以及 `read pat : +++`；
- Canary 变异缺陷敏感性检验：
  - 篡改断言期望值（例如将 `"read pat : +++"` 篡改为 `"read pat : ---"`），确保 Canary 注入后仿真在窗口期内 100% 失败被击杀。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/peripherals/uart_uart_events/`。
- [x] **Task 1.2**：镜像 `uart_events_example_main.c` 为 `uart_events_example_main.c`，固化 SHA-256。
- [x] **Task 1.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 1.4**：在 `run_esp32_headless_evidence.ps1` 登记 `uart_uart_events` carrier。

### Phase 2：Layer A 底座对齐与 UART Pattern 响应增强
- [x] **Task 2.1**：在 `esp_uart.c` 中补齐 `uart_flush_input`、`uart_enable_pattern_det_baud_intr`、`uart_pattern_queue_reset`、`uart_pattern_pop_pos`。
- [x] **Task 2.2**：在 `on_pal_uart_event` 增加 pattern 检测与 `UART_PATTERN_DET` 事件投递。
- [x] **Task 2.3**：编译验证 Wasm 目标。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/peripherals_uart_uart_events.scenario.json` 业务因果断言集合。
- [x] **Task 3.2**：核验 `checklist.data.json` 中该项元数据（Display ID: 98, scenario_path 等）。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App uart_uart_events`，确保正向仿真 100% 通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：注入非等价业务变异，验证指定断言在窗口期内精准失败并被捕获（Canary Kill 100%）。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 `run_gates.py --gate 1` 确保 Gate 1 静态语义门禁 100% 通过。
- [x] **Task 7.2**：运行 `check_license_map.py` 确保开源许可 100% 合规。
- [x] **Task 7.3**：运行 `evidence_verifier.py --verify-all` 确保 24 项全量零回归。
- [x] **Task 7.4**：按模块原子化提交 Git Commit。
