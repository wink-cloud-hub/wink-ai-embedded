# 实施计划：Lane 4 P1 标杆 `peripherals/dac/dac_oneshot` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 4（模拟量转换与电学传感）P1 标杆治理闭环。

---

## 一、 战略目标与任务概况

| 属性 | 内容 |
|---|---|
| 治理对象 | `examples/peripherals/dac/dac_oneshot`（Checklist Display ID: #014） |
| 目标落地目录 | `wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_oneshot` |
| 泳道与优先级 | **Lane 4（模拟电学） / P1 通用积木标杆** |
| 核心依赖能力 | `cap.analog.adc_oneshot`, `cap.analog.dac_out`, `cap.core.fiber_task`, `cap.core.sync_tokens` |
| 源码合规策略 | `zero_modification_mirror`（零篡改原厂镜像，SHA-256 固化） |
| 验证机制 | Headless 确定性仿真 + Canary 变异击杀 + Gate 1 完整性门禁 + License Compliance |

---

## 二、 现状与缺口分析

1. **原厂源码分析**：
   - 权威原厂路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\peripherals\dac\dac_oneshot\main\dac_oneshot_example_main.c`
   - 业务逻辑：
     - 配置 DAC0 (GPIO 25) 和 DAC1 (GPIO 26) 单次输出句柄 `dac_oneshot_handle_t`；
     - 创建 `dac_output_task` 周期性（每 500ms）向 DAC 输出电压（0 ~ 240，步进 10）；
     - 配置 ADC2 单元及 ADC2_CH8 (GPIO 25)、ADC2_CH9 (GPIO 26)；
     - 创建 `adc_monitor_task` 周期性（每 100ms）通过 `adc_oneshot_read` 采样回读电压并打印至控制台。

2. **Layer A 框架门面缺口**：
   - 缺少 ESP-IDF v6.1 驱动头文件：
     - `driver/dac_oneshot.h`
     - `driver/dac_types.h`
     - `hal/dac_types.h`
   - 缺少门面实现 `src/drivers/esp_dac.c`：
     - `dac_oneshot_new_channel`
     - `dac_oneshot_output_voltage`
     - `dac_oneshot_del_channel`
   - `esp_sim_handle.h` 登记 `ESP_SIM_HANDLE_DAC_ONESHOT = 13`
   - `esp_idf_sources.cmake` 注册 `esp_dac.c`

3. **仿真载体与场景闭环缺口**：
   - 目录 `wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_oneshot` 需创建，包含 `CMakeLists.txt`, `wink-app.json`, `include/sdkconfig.h`；
   - 编撰领域断言场景 `unisim-scenarios/peripherals_dac_dac_oneshot.scenario.json`：
     - 外部模拟注入 `INPUT_ANALOG` 到 GPIO 25 (0.40) 与 GPIO 26 (0.70)；
     - 断言 `adc:25` 与 `adc:26`；
     - 验证控制台正常输出。

---

## 三、 执行步骤拆解

### 阶段 1：ESP-IDF DAC 框架门面实现
- [ ] 1.1 创建 `wink-micro-os/frameworks/esp_idf/include/hal/dac_types.h`
- [ ] 1.2 创建 `wink-micro-os/frameworks/esp_idf/include/driver/dac_types.h`
- [ ] 1.3 创建 `wink-micro-os/frameworks/esp_idf/include/driver/dac_oneshot.h`
- [ ] 1.4 在 `wink-micro-os/frameworks/esp_idf/src/core/esp_sim_handle.h` 注册 `ESP_SIM_HANDLE_DAC_ONESHOT = 13`
- [ ] 1.5 编写 `wink-micro-os/frameworks/esp_idf/src/drivers/esp_dac.c`
- [ ] 1.6 在 `wink-micro-os/frameworks/esp_idf/esp_idf_sources.cmake` 注册 `esp_dac.c`

### 阶段 2：应用载体与原厂镜像落盘
- [ ] 2.1 复制 `dac_oneshot_example_main.c` 落地至 `wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_oneshot/`
- [ ] 2.2 创建 `CMakeLists.txt`, `wink-app.json`, `include/sdkconfig.h`
- [ ] 2.3 在 `run_esp32_headless_evidence.ps1` 注册 `dac_dac_oneshot` 应用载体

### 阶段 3：高保真领域仿真场景编撰
- [ ] 3.1 编写 `unisim-scenarios/peripherals_dac_dac_oneshot.scenario.json`
- [ ] 3.2 配置 `INPUT_ANALOG` 激励与 `adc:25`, `adc:26` 领域断言

### 阶段 4：自动化治理流水线执行
- [ ] 4.1 运行 `run_loop.py --app dac_dac_oneshot`
- [ ] 4.2 验证 Wasm 编译、DeviceTree 生成与 Headless 仿真通过
- [ ] 4.3 自动化 Canary 变异注入与 100% 击杀验证
- [ ] 4.4 机器审计签署与凭据归档入库

### 阶段 5：全量门禁与合规复核
- [ ] 5.1 运行 `python .github/scripts/check_license_map.py`
- [ ] 5.2 运行 `python run_gates.py --gate 1`
- [ ] 5.3 运行 `python evidence_verifier.py --verify-all`
- [ ] 5.4 提交原子 Git Commit
