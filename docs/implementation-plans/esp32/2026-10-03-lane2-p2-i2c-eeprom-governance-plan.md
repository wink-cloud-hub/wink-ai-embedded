<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 2 P2 标杆 `peripherals/i2c/i2c_eeprom` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 2（通用数字总线与通信）I2C EEPROM 芯片通信治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261003-LANE2-P2-I2C-EEPROM-GOVERNANCE-v1.0 |
| 任务目标 | 达成 `peripherals/i2c/i2c_eeprom`（Display ID: 24）零修改镜像治理与外挂 I2C EEPROM 器件高保真确定性仿真认证 |
| 泳道与优先级 | **Lane 2（通用数字总线与通信） / P2 进阶标杆** |
| 状态 | 🟢 **Complete** |
| 日期 | 2026-10-03 |
| 上游路径 | `examples/peripherals/i2c/i2c_eeprom` |
| 目标载体目录 | `wink-micro-app/vendor/esp_idfv61/peripherals/i2c_i2c_eeprom/` |
| 场景路径 | `peripherals/i2c_i2c_eeprom/unisim-scenarios/peripherals_i2c_i2c_eeprom.scenario.json` |
| 依赖能力 | `cap.bus.i2c_master`, `cap.core.fiber_task`, `cap.core.sync_tokens` |
| 交付工件 | 原厂零修改代码镜像、UniSim 确定性场景、仿真执行报告、Gate 1 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在 Lane 2（数字总线）中，我们已成功攻克并验证：
- `#020` `peripherals/gpio/generic_gpio`（通用 GPIO 输入输出与中断）
- `#023` `peripherals/i2c/i2c_basic`（现代对象式 I2C Master 与 MPU9250 IMU 传感器通信）
- `#073` `peripherals/spi_master/hd_eeprom`（半双工 SPI Master 与 AT93C46D EEPROM 读写）
- `#096` `peripherals/uart/uart_echo`（UART 环形缓冲通信）

**`peripherals/i2c/i2c_eeprom`（Display ID: 24）是 I2C 族系中紧随 `i2c_basic` 之后的关键示例**：
- 使用现代对象式 I2C Master API（`i2c_new_master_bus`, `i2c_master_bus_add_device`）；
- 驱动外挂 I2C EEPROM 芯片（如 AT24C02/AT24C256，I2C 从机地址 0x50）；
- 演示双字节地址（`addr_wordlen = 2`）的块写入（`i2c_eeprom_write`）；
- 演示等待硬件 EEPROM 写入周期完成的轮询机制（`i2c_eeprom_wait_idle`）；
- 演示带复合传输的跨页随机读取（`i2c_eeprom_read`，内部通过 `i2c_master_transmit_receive` 实现写地址与读数据）；
- 验证回读 48 字节数据与原始写入内容的一致性（通过 `disp_buf` 打印数据矩阵）。

### 1.2 战略收益
1. **完善 I2C 总线器件协议多样性**：从单个传感器（IMU）扩展到总线存储器件（I2C EEPROM），验证 16 位字地址与连续页面读写。
2. **打通总线复合传输因果闭环**：验证 `i2c_master_transmit_receive` 在虚拟总线中的正确分发与时序因果。
3. **扩展实证交付基线至 23 项**：在完成 §1.4 14 项标杆的基础上，进一步推进 Lane 2 P2 示例收敛。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 权威原厂路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\peripherals\i2c\i2c_eeprom`
- 镜像文件与锁定 SHA-256：
  - `main/i2c_eeprom_main.c` -> `wink-micro-app/vendor/esp_idfv61/peripherals/i2c_i2c_eeprom/i2c_eeprom_main.c`:  
    `f2412ff9b6c4dbe4c2204eee6283e707355d2355650edde3aa64d12f6a269cba`
  - `components/i2c_eeprom/i2c_eeprom.c` -> `wink-micro-app/vendor/esp_idfv61/peripherals/i2c_i2c_eeprom/i2c_eeprom.c`:  
    `3d72db2628859e2306a6791c41c874fe38c0a435bfb5eafd874c2127a9317b53`
  - `components/i2c_eeprom/include/i2c_eeprom.h` -> `wink-micro-app/vendor/esp_idfv61/peripherals/i2c_i2c_eeprom/include/i2c_eeprom.h`:  
    `6e22796cce2c280a539af6e154505303050f728380737ef10c1356b624cd9c8c`
- 严禁修改任何原厂源码，所有适配均通过 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json` 完成。

### 2.2 Layer D 外设插件与 Layer A 响应对齐
- 拓扑配置 `wink-app.json`：
  - 声明器件 `i2c_eeprom`，挂载在 I2C 从机地址 `0x50`（80）；
  - SDA 引脚 `21`，SCL 引脚 `22`；
- 底座 `sim_responder.c` 对齐：
  - 现行 `sim_responder.c` 已包含 `sim_i2c_eeprom_at24c02_t`；
  - 增强 `sim_responder_dispatch`：当 I2C 地址为 `0x50` ~ `0x57` 且尚未注册时，自动懒加载初始化 `s_default_at24c02`，并支持 16-bit word address 的读写（首 2 字节为地址，随后为数据/读取）。

### 2.3 Layer S 确定性因果时序与断言设计
- 业务因果链路：
  - 供电断言：`power:VCC_3V3` 为 3.3V；
  - I2C 总线传输断言：断言向从机 0x50 发送地址 0x0010 与首块 8 字节数据（hex: `"00100001020304050607"`）；
  - 日志因果断言：`i2c_eeprom_main.c` 在成功回读 48 字节后，调用 `disp_buf` 打印格式化十六进制数据：
    - 首行：`00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f`
    - 次行：`10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f`
    - 尾行：`20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f`
- Canary 变异缺陷敏感性检验：
  - 篡改断言期望值（例如将 `"00 01 02"` 改为 `"ff ff ff"`），确保 Canary 注入后仿真在窗口期内 100% 失败被击杀。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/peripherals/i2c_i2c_eeprom/`。
- [x] **Task 1.2**：镜像 `i2c_eeprom_main.c`、`i2c_eeprom.c`、`include/i2c_eeprom.h`，核验锁定 SHA-256。
- [x] **Task 1.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 1.4**：在 `run_esp32_headless_evidence.ps1` 登记 `i2c_i2c_eeprom` carrier。

### Phase 2：Layer A / D 底座对齐与 EEPROM 响应增强
- [x] **Task 2.1**：在 `wink-micro-os/targets/common/src/sim_responder.c` 中支持 0x50 地址的自动懒加载初始化与 16 位地址字支持。
- [x] **Task 2.2**：在 `esp_check.h` 补齐 `ESP_GOTO_ON_FALSE` / `ESP_GOTO_ON_ERROR` 宏定义；提供 `sys/cdefs.h` 兼容头文件。
- [x] **Task 2.3**：编译验证 Wasm 目标。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/peripherals_i2c_i2c_eeprom.scenario.json` 业务因果断言集合。
- [x] **Task 3.2**：核验 `checklist.data.json` 中该项元数据（Display ID: 24, scenario_path 等）。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App i2c_i2c_eeprom`，确保正向仿真 100% 通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：注入非等价业务变异，验证指定断言在窗口期内精准失败并被捕获（Canary Kill 100%）。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 `run_gates.py --gate 1` 确保 Gate 1 静态语义门禁 100% 通过。
- [x] **Task 7.2**：运行 `check_license_map.py` 确保开源许可 100% 合规。
- [x] **Task 7.3**：运行 `evidence_verifier.py --verify-all` 确保 23 项全量零回归。
- [x] **Task 7.4**：按模块原子化提交 Git Commit。
