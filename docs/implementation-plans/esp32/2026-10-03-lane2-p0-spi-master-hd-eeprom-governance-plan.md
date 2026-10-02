<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 2 P0 标杆 `peripherals/spi_master/hd_eeprom` 官方示例仿真治理闭环

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261003-LANE2-SPI-MASTER-HD-EEPROM-v1.0 |
| 状态 | 🟡 **In Progress** |
| 日期 | 2026-10-03 |
| 目标平台/SoC | `wasm32-unknown-emscripten` / `esp32 xtensa` 双 Target |
| 工具链/SDK版本| `ESP-IDF v6.1@fff9895c` / `Emscripten 3.1.x` / Node.js 24+ / Python 3.11+ |
| 优先级 | **P0（Lane 2 总线通信核心拼图 · SPI Master 半双工模式与外挂 EEPROM 芯片通信）** |
| 治理依据 | [governance-sop-esp](../../../.agents/skills/governance-sop-esp/SKILL.md)<br>[ADR-0001：负数错误码标准](../../decisions/core/0001-error-code-sign-convention.md)<br>[ADR-0002：双 Target 同源编译](../../decisions/core/0002-dual-target-compilation.md)<br>[ADR-0004：编译期静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0012：Fail-Loud 原则](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0083 / ADR-0084：开源许可分层地图](../../decisions/core/0083-open-source-license-boundary.md)<br>[ADR-0092：ESP-IDF 官方示例仿真治理前置筑基宪章](../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md) |
| 管辖数据源 | `checklist.data.json`（Display ID: 73, `esp.peripherals.spi_master.hd_eeprom`） |
| 实施目标文件 | `wink-micro-os/frameworks/esp_idf/src/drivers/esp_spi.c`（补齐 `spi_device_polling_transmit`、`spi_device_acquire_bus`、`spi_device_release_bus` 及 half-duplex 命令/地址/事务时序）<br>`wink-micro-os/targets/common/src/sim_responder.c`（支持 AT93C46D 虚拟 EEPROM 状态机及 MISO 就绪电平模拟）<br>`wink-micro-app/vendor/esp_idfv61/peripherals/spi_master_hd_eeprom/`（新建示例应用脚手架）<br>`wink-micro-app/vendor/esp_idfv61/peripherals/spi_master_hd_eeprom/spi_eeprom_main.c`（原厂零修改镜像，固化 SHA-256）<br>`wink-micro-app/vendor/esp_idfv61/peripherals/spi_master_hd_eeprom/spi_eeprom.c`（原厂零修改镜像，固化 SHA-256）<br>`wink-micro-app/vendor/esp_idfv61/peripherals/spi_master_hd_eeprom/include/spi_eeprom.h`（原厂零修改镜像，固化 SHA-256）<br>`wink-micro-app/vendor/esp_idfv61/peripherals/spi_master_hd_eeprom/unisim-scenarios/spi_master_hd_eeprom.scenario.json`（业务因果断言场景）<br>`wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1`（登记 carrier） |
| 验收门禁 | `python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --gate 1`<br>`python .github/scripts/check_license_map.py`<br>Headless 正向基线通过 + Canary 变异 100% 击杀<br>`evidence_verifier.py --verify-all` 凭据验证全绿（达成 16/16 黄金基准） |

---

## 一、 战略目标与现状分析

### 1.1 背景与战略价值
在 ESP-IDF 官方示例治理清单中：
- 当前 Lane 2（总线通信）已具备 **UART（`uart_echo`）** 与 **I2C（`i2c_basic`）**；
- **`peripherals/spi_master/hd_eeprom`（Display ID: 73）** 是嵌入式经典三大基础总线（UART/I2C/SPI）中 SPI 总线唯一尚未落地的关键拼图；
- 该示例使用 SPI Master 半双工模式（Half-Duplex Mode）与外挂 AT93C46D EEPROM 芯片通信，涵盖：
  1. **半双工命令/地址/数据事务**：10 位命令/地址字（Start Bit + Opcode + Address）与 8 位数据读写；
  2. **软件/硬件混合 CS 时序控制**：通过 `pre_cb`（`cs_high`）与 `post_cb`（`cs_low`）驱动正逻辑 Chip Select（Active High）；
  3. **写就绪忙检测（Busy/Ready Polling）**：主控拉高 CS 轮询 MISO 引脚状态，直到芯片内部写入完成 MISO 跳变高电平；
  4. **全套事务闭环**：`spi_eeprom_write_enable` -> 逐字节 `spi_eeprom_write` -> 逐字节 `spi_eeprom_read` -> 数据校验回传。
- 攻克该示例将达成嵌入式系统 **“三大经典总线（UART/I2C/SPI）全部覆盖” 的重大里程碑**！

### 1.2 原厂代码与底座缺口分析
权威原厂路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\peripherals\spi_master\hd_eeprom`
固化 SHA-256：
- `main/spi_eeprom_main.c`: `db471d12afbd747aaab4ad7e55e6ddb7bee6661386bbc530204fa2c81dea74e5`
- `components/eeprom/spi_eeprom.c`: `04c02594607dc1e6d35182831ddcbc774f7089d39e96d96fe325baad1ef76354`
- `components/eeprom/spi_eeprom.h`: `7fac6472185ae7152d9bc65b576fd41c7e9c7a3cacf1bb14e27635af27c88d7c`

1. **Layer A 框架缺口与修复方案**：
   - 现行 `esp_spi.c` 仅实现了全双工 `spi_device_transmit`，缺少：
     - `spi_device_polling_transmit`
     - `spi_device_acquire_bus` 与 `spi_device_release_bus`
     - 设备级 `pre_cb` 与 `post_cb` 回调触发
   - `driver/spi_master.h` 事实性头文件中已声明上述函数（在标准编译下为纯声明，无 SLA 阻断）。
   - 外挂 AT93C46D 芯片行为模拟：
     - 需在 `sim_responder.c` 或外设响应层建模 AT93C46D 状态机（支持 EWEN/EWDS/WRITE/READ 10-bit 指令与 128 字节存储）；
     - 当主控拉高 CS 轮询忙状态时，MISO 引脚需反馈 Ready（高电平），确保 `eeprom_wait_done_by_polling` 正常结束不超时。

2. **Layer C 应用载体落地**：
   - 创建 `wink-micro-app/vendor/esp_idfv61/peripherals/spi_master_hd_eeprom/`；
   - 镜像 `spi_eeprom_main.c`、`spi_eeprom.c`、`include/spi_eeprom.h`（保持原厂零修改，固化 SHA-256）；
   - 配置 `CMakeLists.txt`、`include/sdkconfig.h`、`wink-app.json`；
   - 在 `run_esp32_headless_evidence.ps1` 登记 `spi_master_hd_eeprom` carrier。

3. **Layer S 场景断言编撰**：
   - 编撰 `unisim-scenarios/spi_master_hd_eeprom.scenario.json`；
   - 采用因果断言，匹配控制台关键日志序列：
     - `"Initializing bus SPI2..."`
     - `"Initializing device..."`
     - `"Write: Hello World!"`
     - `"Read: Hello World!"`
     - `"Example finished."`

---

## 二、 架构规格与接口设计

### 2.1 C-ABI 头文件契约
全部接口均在原厂已收割的 `include/driver/spi_master.h` 中，无需修改已收割的头文件（严格遵守 ADR-0086 / ADR-0087 免被篡改门禁拦截）。

### 2.2 半双工 SPI 与 AT93C46D 响应模型
- `spi_device_interface_config_t` 配置了 `command_bits = 10`、`spics_io_num = -1`、`flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_POSITIVE_CS`、`pre_cb = cs_high`、`post_cb = cs_low`；
- 在 `spi_device_polling_transmit` 中：
  1. 调用 `pre_cb(trans_desc)`（拉高 CS 13）；
  2. 提取 `trans_desc->cmd`（10 位，含操作码与地址）及读写缓冲区；
  3. 执行 `sim_responder` 或内部状态机读写，若为 READ，将读取数据写回 `trans_desc->rx_data[0]`；
  4. 调用 `post_cb(trans_desc)`（拉低 CS 13）。
- 在 `eeprom_wait_done_by_polling` 中：
  - 应用层手动拉高 CS 13，调用 `gpio_get_level(18)` 读取 MISO；
  - 虚拟器件检测到 CS 为高电平时将 MISO 置高（Ready），检测到 CS 为低电平时恢复低电平，从而使轮询立即通过。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer A 底座框架对齐与 API 补齐
- [x] **Task 1.1**：在 `wink-micro-os/frameworks/esp_idf/src/drivers/esp_spi.c` 补齐 `spi_device_polling_transmit`、`spi_device_acquire_bus`、`spi_device_release_bus`，并支持 `pre_cb`/`post_cb` 回调。
- [x] **Task 1.2**：在 `sim_responder` 或 `esp_spi.c` 中实现 AT93C46D 半双工读写状态机与 MISO 就绪联动。
- [x] **Task 1.3**：在 `esp_gpio.c` 中支持虚拟输入引脚读回与联动，在 `esp_log.c` 中补齐 `esp_rom_printf` 与 `esp_log_get_default_level`。

### Phase 2：Layer C 应用载体与原厂镜像落盘
- [x] **Task 2.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/peripherals/spi_master_hd_eeprom/`。
- [x] **Task 2.2**：镜像 `spi_eeprom_main.c`、`spi_eeprom.c`、`include/spi_eeprom.h`，核验并固化 SHA-256。
- [x] **Task 2.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 2.4**：在 `run_esp32_headless_evidence.ps1` 登记 `spi_master_hd_eeprom`。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/spi_master_hd_eeprom.scenario.json` 业务因果断言：
  - 断言系统 3.3V 供电；
  - 断言控制台日志 `"Initializing bus SPI2..."`；
  - 断言控制台日志 `"Initializing device..."`；
  - 断言控制台日志 `"Write: Hello World!"`；
  - 断言控制台日志 `"Read: Hello World!"`；
  - 断言控制台日志 `"Example finished."`。
- [x] **Task 3.2**：核验 `checklist.data.json` 中该项的 `target_app_dir` 与 `scenario_path`。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App spi_master_hd_eeprom`，确保仿真全绿通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：执行注入业务变异，验证指定断言在窗口期内精准失败并被捕获（Canary Kill 100%）。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 `run_gates.py --gate 1` 确保 Gate 1 静态语义门禁 100% 通过。
- [x] **Task 7.2**：运行 `evidence_verifier.py --verify-all` 达成 16/16 全量全绿通过。
- [x] **Task 7.3**：运行 `check_license_map.py` 确保开源许可分层合规。

### Phase 8：原子提交与交付归档
- [x] **Task 8.1**：按照 Git Commit Rules 执行原子逻辑提交。
- [x] **Task 8.2**：更新计划状态为 Complete。
