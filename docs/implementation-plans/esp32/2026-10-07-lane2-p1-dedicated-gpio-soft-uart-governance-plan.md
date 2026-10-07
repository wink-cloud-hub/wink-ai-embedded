<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 2 P1 标杆 `peripherals/dedicated_gpio/soft_uart` 官方示例仿真治理闭环

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261007-LANE2-P1-SOFT-UART-v1.0 |
| 状态 | ✅ **Completed** |
| 创建 / 修订日期 | 2026-10-07 / 2026-10-07 |
| 优先级 | **P1（Lane 2 通用数字总线软件模拟串口）** |
| 目标条目 | Display ID: 17；`esp.peripherals.dedicated_gpio.soft_uart` |
| 配置身份 | `config_id=wasm_sim_standard`；登记 `backend=wasm_browser`；`target_soc=esp32`；`profile=standard` |
| 运行与编译目标 | Wasm Headless 行为验收及 ESP32-S3/Xtensa 原生编译（专用 GPIO 特性需 S3/C3 芯片支持） |
| 固定 SDK | `ESP-IDF v6.1@fff9895c`；原厂镜像零修改策略 |
| 原厂源码 SHA-256 | `ab970da15d29eef8c7466e6338def6b4bce362286d6a846e18ec3cd18e5a7d1a` (`soft_uart_main.c`) |
| 数据源 | `wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json`（Display ID: 17） |
| 治理依据 | [governance-sop-esp](../../../.agents/skills/governance-sop-esp/SKILL.md)、[CLASSIFICATION-SPEC](../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md)、[PLAYBOOK](../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/PLAYBOOK.md) |
| 关联决策 | [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md)、[ADR-0002](../../decisions/unisim/0002-dual-target-compilation.md)、[ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)、[ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0083](../../decisions/core/0083-adopt-gpl-3.0-only-license-policy.md)、[ADR-0084](../../decisions/core/0084-layered-license-map-lgpl-runtime.md) |
| 关联技术规格 | [Batch 0 候选证据契约](../../zh/tech-designs/esp32/esp-idf-batch0-evidence-contract.md)、[全维度对抗与治理计划](2026-10-05-comprehensive-adversarial-red-green-testing-plan.md) |
| 完成口径 | 原厂业务逻辑零修改、DEDIC_GPIO 门面完整实现、Wasm 与 ESP-IDF 双 Target 编译通过、正向与负向双场景闭环、Canary 变异 100% 击杀、Gate 1~5 门禁全绿 |

---

## 一、目标与已核验基线

### 1.1 实施目标与范围

保持原厂 `soft_uart_main.c` 字节级零修改，补齐 ESP-IDF `driver/dedic_gpio.h` 专用 GPIO 门面，实现基于专用 GPIO 的软件模拟串口（Soft UART）驱动在 Wasm 仿真底座与原生硬件上的同源编译与执行。

本示例在 Lane 2（通用数字总线与通信）中具有重要承前启后意义：
1. **延续成果**：在已交付的硬件 UART（#094 异步收发、#097 RS485 半双工、#098 事件队列）基础上，将串口通信扩展至 CPU 指令级软件 Bit-banging 模拟场景；
2. **解锁三联积木**：补齐 `driver/dedic_gpio.h` 门面后，将直接为后续同属专用 GPIO 泳道的 `#015 soft_i2c` 和 `#016 soft_spi` 奠定底层驱动基础。

### 1.2 原厂基准与配置事实

本地原厂路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\peripherals\dedicated_gpio\soft_uart\`

| 文件路径 | 字节大小 | SHA-256 校验和 | 用途与说明 |
|---|---|---|---|
| `main/soft_uart_main.c` | 1,856 | `ab970da15d29eef8c7466e6338def6b4bce362286d6a846e18ec3cd18e5a7d1a` | 原厂主业务逻辑（零修改镜像源） |
| `components/soft_uart/soft_uart.c` | 6,520 | `d6c96e6b420317fd03b468535f4630ad0545d0543b57cfa236a3e6dbfce1dfcb` | 软串口核心驱动实现 |
| `components/soft_uart/include/soft_uart.h` | 2,406 | `bf22efa361c5dd6327cab0eb2589f0103764e00f384bd868aa601c321c826193` | 软串口公共头文件 |
| `components/soft_uart/xtensa/soft_uart.S` | 4,428 | `405954b5c3e09aa5327be1c17b04a8f49da971112ab271f3582220f4d23b8440` | Xtensa 平台专用汇编 Bit-banging 驱动 |
| `components/soft_uart/riscv/soft_uart.S` | 7,180 | `47067ae94a0c673e386f84ca91016be9b180d186562c413e29ba6d578c5f46ec` | RISC-V 平台专用汇编 Bit-banging 驱动 |

#### 原厂业务流程分析

```
[app_main]
  │
  ├──► soft_uart_new(&config, &port)       // 配置 TX=16, RX=17, 115200 波特率，创建 dedic_gpio bundle
  │
  ├──► soft_uart_send(port, &dummy, 1)     // 发送 dummy 字节吸收首次可能损坏
  │
  ├──► soft_uart_send(port, write_buf, 34) // 发送 "Hello, world! This is a message.\r\n"
  │
  ├──► soft_uart_receive(port, read_buf, 16) // 接收 16 字节
  │
  ├──► printf("UART transfers succeeded, received bytes: { ... }\n") // 打印 16 字节十六进制
  │
  └──► soft_uart_del(port)                 // 释放 GPIO bundle 与端口资源
```

#### 硬件与 SoC 支持特征

根据 ESP-IDF 原厂设计与技术手册：
- 经典 ESP32（ESP32-D0WD）**不支持** Dedicated GPIO 指令（`CONFIG_SOC_DEDICATED_GPIO_SUPPORTED=0`）；
- Dedicated GPIO 仅在 ESP32-S2、ESP32-S3、ESP32-C2、ESP32-C3、ESP32-C6 等型号中提供；
- 因此硬件目标编译应适配 `esp32s3` 或在仿真中通过标准 C 模拟汇编例程。

---

## 二、架构设计与技术规格

### 2.1 分层架构与适配设计

```
┌────────────────────────────────────────────────────────┐
│  wink-micro-app: peripherals/dedicated_gpio_soft_uart  │
│  - soft_uart_main.c (原厂镜像，SHA-256 锁定)           │
│  - components/soft_uart/soft_uart.c                   │
└──────────────────────────┬─────────────────────────────┘
                           │
                           ▼
┌────────────────────────────────────────────────────────┐
│  wink-micro-os: frameworks/esp_idf                     │
│  - driver/dedic_gpio.h (已有 harvested C-ABI 头文件)   │
│  - src/drivers/esp_dedic_gpio.c (新建 DEDIC_GPIO 门面)  │
│  - src/drivers/esp_soft_uart_sim.c (Wasm 仿真汇编替身) │
└──────────────────────────┬─────────────────────────────┘
                           │
                           ▼
┌────────────────────────────────────────────────────────┐
│  wink-micro-os: pal & targets                          │
│  - pal/include/hal/pal_gpio.h                         │
│  - targets/wasm/ (虚拟总线 / 回环 FIFO / 故障注入)    │
└────────────────────────────────────────────────────────┘
```

### 2.2 `driver/dedic_gpio.h` 门面实现规格

在 `wink-micro-os/frameworks/esp_idf/src/drivers/esp_dedic_gpio.c` 中完整实现以下 6 个核心 C-ABI 接口：

1. `dedic_gpio_new_bundle(const dedic_gpio_bundle_config_t *config, dedic_gpio_bundle_handle_t *ret_bundle)`
   - 校验参数合法性（非空指针、`array_size > 0`、标志位非互斥）；
   - 动态分配 `struct dedic_gpio_bundle_t`；
   - 遍历 `gpio_array`，调用底层 `gpio_set_direction` 与 `gpio_set_pull_mode` 初始化引脚；
   - 记录 bundle 掩码与偏移量，返回 `ESP_OK`。
2. `dedic_gpio_del_bundle(dedic_gpio_bundle_handle_t bundle)`
   - 校验 handle 有效性并安全释放内存。
3. `dedic_gpio_get_out_offset(dedic_gpio_bundle_handle_t bundle, uint32_t *offset)`
   - 返回 bundle 内部的 GPIO 相对偏移（默认基准为 0）。
4. `dedic_gpio_get_in_offset(dedic_gpio_bundle_handle_t bundle, uint32_t *offset)`
   - 返回 bundle 内部的输入偏移（默认基准为 0）。
5. `dedic_gpio_get_out_mask(dedic_gpio_bundle_handle_t bundle, uint32_t *mask)`
   - 返回 bundle 涵盖的全部 GPIO 位掩码。
6. `dedic_gpio_get_in_mask(dedic_gpio_bundle_handle_t bundle, uint32_t *mask)`
   - 返回 bundle 涵盖的输入 GPIO 位掩码。

### 2.3 Wasm 平台汇编替身设计（`emulate_uart_send` / `emulate_uart_receive`）

由于 Emscripten 无法编译 Xtensa/RISC-V 专用汇编代码，在 Wasm 编译时提供高保真 C 实现替身：
1. **TX 发送模拟 (`emulate_uart_send`)**：
   - 捕获发送数据流，写入内部软串口仿真 FIFO 缓存，并记录 TX 虚拟微秒时序（8.63µs/bit @ 115200bps）；
   - 同时支持将发送内容镜像或提供给 RX 回环读取。
2. **RX 接收模拟 (`emulate_uart_receive`)**：
   - 优先从软串口仿真 FIFO 或 UniSim 虚拟注入通道消费指定长度字节；
   - 若处于自环模式（Loopback），自动消费先前 TX 写入的有效载荷；
   - 支持受控故障注入（超时、载荷损毁、引脚断开）。

### 2.4 故障注入与防假绿契约

- **正向路径（Happy Path）**：
  - 启动软串口，发送 dummy 字节及 34 字节文本；
  - 接收 16 字节载荷；
  - 控制台精准输出 `UART transfers succeeded, received bytes: { 0x... }`。
- **逆向路径（Fault Path）**：
  - 注入故障（例如：配置非法波特率 `SOFT_UART_BAUD_END`、传递 NULL 端口、或注入底层接收超时）；
  - 原厂代码跳转至 `error:` 标签；
  - 控制台精准输出 `ESP_LOGE(EXAMPLE_TAG, "An error occurred while communicating through the UART")`；
  - 断言无内存越界或系统崩溃。

---

## 三、实施步骤与任务拆解

```
Phase 0: 基准锁定与 Carrier App 目录建立
   │
   ├──► Phase 1: 运行时框架扩展 (esp_dedic_gpio.c + Wasm 仿真替身)
   │
   ├──► Phase 2: 单元测试补齐与双 Target 编译验收
   │
   ├──► Phase 3: 确定性场景设计与双实证打样 (*.scenario.json + *.fail.scenario.json)
   │
   ├──► Phase 4: Canary 变异击杀矩阵执行 (100% 击杀验证)
   │
   └──► Phase 5: 治理凭据交付、SSOT 更新与门禁验收
```

### Phase 0: 基准锁定与 Carrier App 目录建立
- [x] **Task 0.1**: 创建 `wink-micro-app/vendor/esp_idfv61/peripherals/dedicated_gpio_soft_uart/` 载体目录；
- [x] **Task 0.2**: 复制原厂 `soft_uart_main.c` 并核验 SHA-256 为 `ab970da15d29eef8c7466e6338def6b4bce362286d6a846e18ec3cd18e5a7d1a`；
- [x] **Task 0.3**: 引入 `components/soft_uart/` 原厂组件源码及头文件；
- [x] **Task 0.4**: 编写 `CMakeLists.txt`、`sdkconfig.h` 与 `wink-app.json`。

### Phase 1: 运行时框架扩展
- [x] **Task 1.1**: 在 `wink-micro-os/frameworks/esp_idf/src/drivers/esp_dedic_gpio.c` 中实现 `dedic_gpio_*` 门面函数；
- [x] **Task 1.2**: 在 `wink-micro-os/frameworks/esp_idf/src/drivers/esp_dedic_gpio.c` 中实现 Wasm 平台的 `emulate_uart_send` 与 `emulate_uart_receive` C 模拟；
- [x] **Task 1.3**: 注册至 `wink-micro-os/frameworks/esp_idf/esp_idf_sources.cmake` 并纳入 `esp_idf_bridge_reset()` 复位清理链。

### Phase 2: 单元测试与双 Target 编译验收
- [x] **Task 2.1**: 编写 `wink-micro-os/frameworks/esp_idf/test/core/test_esp_dedic_gpio.c` 单元测试，覆盖参数校验、bundle 创建/删除、偏移获取与异常分支；
- [x] **Task 2.2**: 运行 `ctest` 验证 CTest 全量通过 (4/4 passed)；
- [x] **Task 2.3**: 验证 Emscripten/Wasm 编译无警告无报错；
- [x] **Task 2.4**: 验证 ESP-IDF S3 硬件编译桩通过。

### Phase 3: 确定性仿真场景设计与双实证打样
- [x] **Task 3.1**: 编写正向场景 `unisim-scenarios/peripherals_dedicated_gpio_soft_uart.scenario.json`；
- [x] **Task 3.2**: 编写成对红测试场景 `unisim-scenarios/peripherals_dedicated_gpio_soft_uart.fail.scenario.json`；
- [x] **Task 3.3**: 运行 headless 仿真生成正向与负向报告，验证断言逐项通过。

### Phase 4: Canary 变异击杀矩阵执行
- [x] **Task 4.1**: 针对正向场景执行变异测试（篡改预期十六进制输出、破坏波特率、注入超时）；
- [x] **Task 4.2**: 针对负向场景执行变异测试（篡改错误提示标签、抑制错误返回）；
- [x] **Task 4.3**: 运行 `mutator.py` 与自主治理 Loop 验证变异击杀率 100%，无 `INFRA_CRASH`。

### Phase 5: 治理凭据交付、SSOT 更新与门禁验收
- [x] **Task 5.1**: 生成正式运行凭据并归档至 `.governance/reports/` 与 `.governance/runs/`；
- [x] **Task 5.2**: 更新 `checklist.data.json`（写入 positive/negative cases、scenario_sha256、assets_sha256、delivery_state=verified）；
- [x] **Task 5.3**: 运行 `generate_checklist_v1_1.py` 重新派生 `CHECKLIST.md`；
- [x] **Task 5.4**: 运行全量治理门禁：
  - `python .governance/tools/evidence/evidence_verifier.py --verify-all`（37/37 项全部通过）；
  - `pytest wink-micro-app/vendor/esp_idfv61/.governance/gates/tests/test_ssot_invariants.py`；
  - `python .github/scripts/check_license_map.py`；
  - `winkcli lint --pack layering --pack api`。
- [x] **Task 5.5**: 按照原子性提交规范分步提交 Git。

---

## 四、风险控制与不可触碰红线

1. **原厂源码 100% 零修改红线**：
   严禁在 `soft_uart_main.c` 中插入任何 `#ifdef SIMULATION`、调试宏或旁路分支，文件 SHA-256 必须严格与上游一致。
2. **纯增量与分层隔离红线 (ADR-0043 / ADR-0092)**：
   `esp_dedic_gpio.c` 属于 `frameworks/esp_idf` 门面层，严禁向公共 PAL 头文件注入 ESP-IDF 专有结构体或类型。
3. **真实因果与防假绿红线**：
   RX 接收数据必须来源于 TX 真实发送内容或仿真注入通道，严禁在 `emulate_uart_receive` 中硬编码假数据欺骗断言。
4. **二值化红测试红线**：
   负向测试必须断言原厂串口报错日志 `An error occurred while communicating through the UART`，严禁将未处理的内存崩溃误判为测试通过。
5. **开源许可证合规红线 (ADR-0083 / ADR-0084)**：
   新建框架驱动代码遵循 `LGPL-3.0-only`，测试用例遵循 `GPL-3.0-only`，应用载体遵循 `Apache-2.0`。
