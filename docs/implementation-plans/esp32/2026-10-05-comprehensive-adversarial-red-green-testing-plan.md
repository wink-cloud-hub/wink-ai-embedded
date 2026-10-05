<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ESP-IDF 官方示例全维度对抗测试、缺陷治理与红绿变异击杀实施计划 (Comprehensive Adversarial & Remediation Plan)

> **计划编号**：`PLAN-ESP-20261005-ADVERSARIAL-RED-GREEN`  
> **制定日期**：2026-10-05  
> **实施周期**：2026-10-05 ~ 2026-10-21 (约2周)  
> **责任体系**：嵌入式仿真核心架构组 / 治理流水线 SOP 团队  
> **适用目标**：`wink-micro-app/vendor/esp_idfv61` 官方示例库及 UniSim 仿真内核  
> **关联架构评审**：[2026-10-05-esp-idf-completed-items-review.md](../../reviews/esp32/2026-10-05-esp-idf-completed-items-review.md)  
> **关联核心规范**：ADR-0001 (负数错误码)、ADR-0004 (静态分发)、ADR-0043 (分层门禁)、ADR-0066 (PWM定点bp)、ADR-0092 (PAL增量规范)

---

## 1. 背景与核心目标

### 1.1 现状与评审暴露的深水区缺陷
WinkMicroOS 当前已建立起一套基于 Wasm Headless 的 ESP-IDF 官方示例仿真治理体系，累计登记 33 个示例。但在 [2026-10-05 深度架构评审](../../reviews/esp32/2026-10-05-esp-idf-completed-items-review.md) 中，通过对源码、Wasm 二进制符号导出（136 个 export 逆向）与内存反例实验的全面审查，揭露出深水区的严峻矛盾：
1. **测试活性通过 $\ne$ 固件因果闭环**：
   - 现存 33 项中，仅 14 项（A 类）具备实质正向物理出口；10 项（C 类）存在严重的实现缺陷或断言绕过固件；
   - **外设虚设与因果短路**：DAC（#013, #014）未产生输出；ADC（#003, #004）断言直接读取测试输入缓存；MQTT（#206）所谓的 RX 接收实际上是在读取本地 TX 发送缓冲。
2. **时钟系统断裂与时序降级**：
   - LEDC Fade（#049）完全忽略 3000ms 渐变时序，瞬时同步完成；
   - TWDT 看门狗（#154）缺失超时调度，漏喂狗绝不报警；
   - FreeRTOS 统计（#132）将 `vTaskDelay` 休眠阻塞时长累加为 CPU 运行时间，发生内核计费倒错。
3. **证据链防线与门禁漏洞**：
   - `evidence_verifier.py` 仅核对 Summary 汇总数值，完全丢失对 `stepResults` 逐步断言与执行身份的校验（E-1）；
   - Canary 变异击杀将编译器报错和 Wasm 加载崩溃误判为“成功杀死变异”（E-2）；
   - 流水线 Daemon 存在“自测自签”审计证书的闭环回音室风险（E-3）。
4. **负向契约与红测试大面积缺失**：
   - 33 个条目中有 21 项的 `negative_cases` 数组留空为 `[]`，缺乏成对的负向场景文件（`*.fail.scenario.json`）。

### 1.2 核心目标
建立**“底座真实语义治理（Root Remediation） + 正向绿测试（Happy Path） + 逆向红测试（Fault Path） + 三维变异击杀（3D Mutation Kill）”**四位一体的终极高保真仿真可靠性防御体系，彻底拔除表层假绿，确保固件在 Wasm 仿真与 ESP32 物理硬件上的同源行为一致性。

---

## 2. 六大落地战略支柱 (The 6 Strategic Pillars)

```
                              【高保真可靠性实证金字塔】

                     ▲       Pillar 4. 三维变异击杀引擎 (3D Mutation Engine)
                    / \      Pillar 2. 成对红绿确定性场景 (Twin Scenarios: Green & Red)
                   /   \     Pillar 3. 底座受控故障注入探针 (Controlled Fault Probes)
                  /=====\    Pillar 1. 负向契约元数据全量补齐 (Negative Contract SSOT)
                 /=======\   Pillar 5. 治理防线铁门与双实证看板 (Gate & Dual-Proof CI)
                /=========\  Pillar 0. 评审暴露缺陷专项治理工单 (Remediation Worklist)
```

---

### Pillar 0: 评审暴露缺陷专项治理与因果拓扑重构 (Remediation Worklist)

针对评审中定性的 S-1~S-5 及 Q-1~Q-7，立项 8 大专项治理工单：

* **W-1 (S-1 / Q-1: PAL DAC 纯增量与 ADC 模拟总线重构)**：
  - 严格遵循 **ADR-0092 Tier 1** 纯增量原则，在 `wink-micro-os/pal/include/hal/` 新增 `pal_dac.h`，无 `esp_*.h` 跨平台洁癖；
  - 同步交付 `targets/wasm/` 虚拟电学总线与 `targets/esp32/` 物理驱动，返回值严格遵循 ADR-0001 负数错误码；
  - 重构 ADC 仿真因果链：断言必须读取固件输出的工程值（如校准 $mV$），彻底切断直接读取输入缓存的短路。
* **W-2 (S-2: LEDC Fade 虚拟时间轮步进)**：
  - 引入离散事件虚拟时间轮（VTE），按毫秒计算阶梯 PWM 步进积分；
  - 占空比严格执行 **ADR-0066** 定点基点规范（`pal_pwm_set_duty_bp` / `PERMILLE`）；
  - 异步分发完成中断，恢复 3000ms 渐变时序断言。
* **W-3 (S-3: TWDT 离散事件超时判定)**：
  - 挂接 FreeRTOS 虚拟 Tick Hook，逐 Tick 遍历订阅列表并比较 `current_tick - last_reset_tick > timeout_ticks`；
  - 确立漏喂狗触发 Panic/报警的逆向判定。
* **W-4 (S-4: FreeRTOS 真实微秒计费模型)**：
  - 废除 `vTaskDelay` 累加运行时间的倒错逻辑；
  - 接入 Fiber 上下文切换钩子（`traceTASK_SWITCHED_IN/OUT`），仅对任务在 CPU 上运行的微秒时间片积分。
* **W-5 (S-5: `esp_idf_bridge_reset` 全生命周期清理)**：
  - 将 ADC、DAC、TWDT 等门面的静态对象池完整纳入软复位销毁范围，确保多场景连续运行无状态污染。
* **W-6 (Q-2: GPTimer 计数寄存器 ABI 导出)**：
  - 在 Wasm Target 中导出真实计数器 ABI（`sim_timer_get_counter`），运行时缺失观察 ABI 时显式 Fail-Loud；
  - 闭环 Alarm 队列分发与重载周期断言。
* **W-7 (Q-3: MQTT 独立双 FIFO 协议栈解耦)**：
  - 废除静态共享变量 `s_last_data`，彻底拆分独立的 `tx_fifo` 与 `rx_fifo`；
  - 由虚拟 Broker 模拟器依据 Topic 路由真实投递，固件事件循环消费。
* **W-8 (Q-4: NVS Blob 官方越界缺陷隔离登记)**：
  - 恪守原厂镜像 0 修改（SHA-256 锁定）红线，在 `.governance/specs/upstream_errata.json` 中正式登记官方越界 Bug；
  - 场景精准断言前两项合法 CRC，对越界日志制定明确捕获策略。

---

### Pillar 1: 负向契约元数据全量补齐 (Negative Contract SSOT)

**目标**：消除 `checklist.data.json` 中 `negative_cases` 留空现象，为全部条目建立标准化形式化负向模型。

1. **元数据 Schema 严格标准化**：
   每个负向用例必须完整声明三要素：
   - `stimulus`（故障激励）：外部注入的具体故障（如 `i2c_nack_address`、`wifi_wrong_password`、`nvs_key_nonexistent`）；
   - `expect_error`（预期表现）：固件底座抛出的标准化错误符号或串口错误日志（如 `ESP_ERR_TIMEOUT`、`ESP_ERR_WIFI_NOT_FOUND`）；
   - `detects`（防假绿目标）：此负向用例专门查杀的底座作弊行为（如 `prevent_empty_stub_ack`、`detect_silent_success_fallback`）。
2. **存量条目契约补齐**：梳理原厂源码，提取错误处理分支，全量更新 `checklist.data.json`。
3. **门禁规则升级 (Gate 1)**：凡 `delivery_state == 'verified'` 的条目，其 `negative_cases` 长度必须 `>= 1`。

---

### Pillar 2: 成对红绿确定性场景规范 (Twin Scenarios: Green & Red Pair)

**目标**：废除单一绿测试文件结构，强制推行**成对场景文件标准**。

1. **场景文件命名与职责划分**：
   - `unisim-scenarios/<app>.scenario.json`（**绿测试**）：黄金正常路径，验证端到端业务成功；
   - `unisim-scenarios/<app>.fail.scenario.json`（**红测试**）：故障注入路径，验证异常捕获与稳态防御。
2. **红测试断言设计标准**：
   - **严禁崩溃**：红测试必须验证仿真器不出现内存越界、死锁或宿主静默退出；
   - **故障感知断言**：断言日志输出标准错误宏，断言状态机迁移至异常态（如 `wifi:sta:state -> DISCONNECTED`）；
   - **时序窗口**：红测试应在故障注入后 $50\text{ms} \sim 200\text{ms}$ 内迅速捕获错误，超时即判失败。
3. **首批打样推进（6 大黄金用例成对化）**：
   - `blink`（时钟源骤停）；`i2c_basic`（从机 NACK 与无应答）；`uart_echo`（RX 溢出与校验错误）；
   - `wifi_sta`（密码错误拒绝关联）；`http_client`（404/500 与 DNS 失败）；`nimble_beacon`（非法广播参数）。

---

### Pillar 3: 底座受控故障注入探针体系 (Controlled Fault Probes)

**目标**：在 `frameworks/esp_idf` 与 `pal` 仿真适配层中建立标准化、零污染的受控故障注入通道。

1. **统一故障注入接口**：
   ```c
   // 仿真受控故障通道接口 (纯仿真编译期隔离)
   wink_status_t sim_esp_fault_inject(uint32_t domain, uint32_t fault_type, uint32_t param);
   wink_status_t sim_esp_fault_clear(void);
   ```
2. **四大领域故障探针实现**：
   - **I2C/SPI 总线**：`FAULT_I2C_NACK`（模拟从机不存在）、`FAULT_I2C_TIMEOUT`（SCL 锁死）；
   - **Wi-Fi / Netif 网络**：`FAULT_WIFI_AUTH_FAIL`（AP 握手失败）、`FAULT_NETIF_DHCP_TIMEOUT`（DHCP 超时）；
   - **NVS 键值存储**：`FAULT_NVS_PARTITION_FULL`（空间不足）、`FAULT_NVS_READ_CORRUPT`（页损坏）；
   - **NimBLE 蓝牙**：`FAULT_BLE_ADV_REJECT`（广播参数被拒绝）。
3. **跨平台洁癖防御**：故障探针使用 `#if defined(__EMSCRIPTEN__)` 严格收敛于仿真底座，严禁渗透通用 PAL 头文件。

---

### Pillar 4: 三维变异引擎升级与击杀判据加固 (3D Mutation Engine)

**目标**：将 `CanaryMutator` 进化为三维变异矩阵，并彻底修复 E-2 虚假击杀漏洞。

1. **三维变异矩阵模型**：
   - **维度 A (Assertion Mutant)**：篡改预期值验证断言器非盲（Matcher 活跃度）；
   - **维度 B (Stimulus Mutant)**：篡改外部环境与输入激励载荷，验证固件能否感知环境劣化；
   - **维度 C (Platform Fault Mutant)**：打入底层硬件故障探针，验证错误防御分支是否 100% 被覆盖。
2. **重构 `verify_kill` 判据状态机（修复 E-2 漏洞）**：
   ```text
   变异执行结果判定:
   ├── exit_code == 0                         ──► [FAIL-GREEN] 致命假绿: 变异存活，立即拦截
   ├── exit_code != 0 && (编译/链接/加载失败)   ──► [INFRA_CRASH] 基础设施崩溃: 一票否决，绝非有效击杀
   └── exit_code != 0 && (命中目标断言精准报错) ──► [KILL-SUCCESS] 真实击杀: 变异被有效拦截，允许晋升
   ```

---

### Pillar 5: 治理防线铁门、证据链防伪与双实证看板 (Gate & Dual-Proof CI)

**目标**：修复 E-1、E-3、E-4 证据漏洞，建立防伪、事务性隔离与红绿双实证看板。

1. **修复 E-1：`evidence_verifier.py` 实施全量逐步 $O(N)$ 校验与 Merkle 证据树**：
   - 必须读取场景文件提取所有预期步骤；
   - 严格遍历 `stepResults` 数组：必须满足 `len(stepResults) == len(scenario.steps)`，且每一步状态必须为 `passed`；
   - 计算所有步骤序列的 SHA-256 签名并与产物绑定，彻底杜绝篡改 Summary 绕过。
2. **修复 E-3：权力制衡机制（Separation of Powers）**：
   - 流水线 Daemon 仅有权产出 `candidate_evidence` 候选凭证包；
   - 封堵 `pipeline.py` 自签 `auditor` 的后门，审计字段必须经双盲对抗审查裁判 Agent 或架构团队签署。
3. **修复 E-4：沙箱隔离与事务性晋升**：
   - 每次运行建立唯一的 `.governance/runs/<timestamp>-<uuid>/` 隔离目录；
   - 全部门禁通过后原子性晋升至正式凭据库，执行失败则完整回滚。
4. **单向派生双实证看板 (`generate_checklist_v1_1.py`)**：
   - 在 `CHECKLIST.md` 渲染器中新增状态列：`[Green ✅ | Red 🛡️]`；
   - 仅当正向场景通过且负向场景精准拦截时，才点亮最高信任度标记 `🟢 VERIFIED (TWIN-PROOF)`。
5. **双 Target 物理实机交叉核验**：
   - 对 6 大黄金用例（`blink`, `ledc`, `gptimer`, `uart_echo`, `http_client`, `wifi_sta`）通过 `wink.py esp32` 在 ESP32 物理硬件上复验，保证同源行为一致。

---

## 3. 时间线与交付里程碑 (Timeline & Milestones)

| 阶段 | 时间周期 | 重点交付目标 | 验收交付物 |
| :--- | :---: | :--- | :--- |
| **M0: 状态止血与证据铁门加固** | Day 1 ~ Day 2 | 1. 修复 E-1：`evidence_verifier.py` 落实逐步强一致性校验；<br>2. 修复 E-2：`mutator.py` 剥离 INFRA_CRASH；<br>3. 修复 E-3/E-4：剥离自签后门并确立 UUID 运行沙箱；<br>4. 在看板中对 10 个 C 类缺陷条目打上 `needs_remediation` 止血。 | 加固版 `evidence_verifier.py`；<br>加固版 `mutator.py`；<br>止血后的看板状态。 |
| **M1: 契约奠基与底座时钟/复位重构** | Day 3 ~ Day 6 | 1. 落实 W-3 (TWDT 超时判定) 与 W-4 (FreeRTOS 真实微秒计费)；<br>2. 落实 W-5 (`esp_idf_bridge_reset` 对象池全生命周期清理)；<br>3. 补齐全量条目的 `negative_cases` 元数据模型 (Pillar 1)。 | 升级版 `esp_task_wdt.c`、`freertos_task.c`、`esp_idf_bridge.c`；<br>`checklist.data.json` 契约补齐。 |
| **M2: 领域外设因果链与成对场景打样** | Day 7 ~ Day 10 | 1. 落实 W-1 (PAL DAC 纯增量驱动与 ADC 模拟总线重构)；<br>2. 落实 W-2 (LEDC Fade 虚拟时间轮步进，ADR-0066 定点 bp)；<br>3. 落实 W-6 (GPTimer 真实 ABI 导出) 与 W-7 (MQTT 独立双 FIFO)；<br>4. 交付 6 大黄金用例成对红绿场景 (`*.fail.scenario.json`)。 | `pal_dac.h` 及双 Target 驱动；<br>升级版 `esp_ledc.c`、`esp_gptimer.c`、`esp_mqtt.c`；<br>6 组双向测试报告。 |
| **M3: 底座探针与三维变异引擎闭环** | Day 11 ~ Day 13 | 1. 交付 I2C、Wi-Fi、NVS、BLE 四大受控故障探针 (Pillar 3)；<br>2. 升级 `mutator.py` 支持三维变异击杀矩阵 (Pillar 4)；<br>3. 自动化流水线变异击杀演练。 | `esp_fault.c` 探针库；<br>三维变异击杀实证实录。 |
| **M4: 双 Target 实机比对与双实证看板交付** | Day 14 ~ Day 16 | 1. 在 ESP32 物理硬件上交叉比对 6 大黄金用例；<br>2. 升级 `generate_checklist_v1_1.py` 渲染双实证看板；<br>3. Gate 1~5 全量门禁复验与正式归档。 | ESP32 硬件比对报告；<br>`CHECKLIST.md` 双实证展示；<br>全流程正式交付包。 |

---

## 4. 风险控制与不可触碰红线 (Risk Governance & Redlines)

1. **零原厂源码污染红线**：
   严禁为了通过测试而在原厂业务源码（`main.c` 等）中插入 `#ifdef SIMULATION` 或异常分支。原厂代码必须 100% 保持上游 SHA-256 镜面一致。
2. **PAL 纯增量演进红线 (ADR-0092 Tier 1)**：
   新增通用外设抽象必须纯增量添加，严禁破坏既有签名，严禁引入 `esp_*.h` 专有类型，必须同时提交 Wasm 仿真桩与 ESP32 物理驱动。
3. **故障隔离红线**：
   故障注入探针必须在测试结束后提供干净的 `sim_esp_fault_clear()`，严禁在多用例批处理时残留故障状态导致后续用例假红。
4. **二值化审计与非崩溃红线**：
   负向测试必须断言明确的错误码或预期的错误表现；严禁将任何未知的崩溃、堆栈溢出、死锁或段错误泛化视为“红测试通过”。

---

## 5. 详细任务分解与执行跟踪清单 (Task Breakdown & Execution Tracker)

### 5.1 已归档基石与核心引擎 (Completed Baseline)
- [x] **Core 1 (Pillar 3 探针架构)**: `esp_fault.h/c` 探针库，打通 I2C/SPI/NVS/WiFi/BLE，Wasm ABI 导出，复位清理链与单测 (`ee4f3f0a`)。
- [x] **Core 2 (Pillar 4 变异矩阵)**: `mutator.py` 三维变异矩阵与 3 状态判据机加固（杜绝虚假崩溃判定，`f33159d3`）。
- [x] **Core 3 (Pillar 5 门禁升级)**: `evidence_verifier.py` $O(N)$ 逐步校验；`generate_checklist_v1_1.py` 双实证标记 (`6eed120d`, `f33159d3`)。
- [x] **Core 4 (Pillar 0 阻断修复)**:
  - [x] W-1: `pal_dac.h` 纯增量 HAL 及 Wasm/Host/ESP32 驱动 (`6b913b1c`)；
  - [x] W-2: `esp_ledc.c` 离散时间轮与定点 bp 步进 (`b915db8f`)；
  - [x] W-3: `esp_task_wdt.c` Tick 钩子超时判定与漏喂狗报警 (`74b62b5d`)；
  - [x] W-4: `freertos_task.c` 微秒级真实 CPU 运行计费 (`74b62b5d`)；
  - [x] W-5: `esp_idf_bridge.c` 外设对象池完整复位销毁 (`74b62b5d`, `ee4f3f0a`)；
  - [x] W-6: GPTimer 硬件计数器 ABI 导出 (`b915db8f`)；
  - [x] W-7: MQTT 独立双 FIFO 环形缓冲解耦 (`b915db8f`)；
  - [x] W-8: NVS 越界缺陷隔离登记至 `upstream_errata.json`。
- [x] **Core 5 (Pillar 2 标杆打样)**: 交付首批 7 大领域成对红测试（Blink GPIO, UART Echo, LEDC Fade, TWDT, WiFi STA, HTTP Client, NimBLE Beacon，`46b899fe`）。

---

### 5.2 待执行分批推进图谱 (Remaining Execution Batches)

```
Batch 0: 治理防线加固与因果短路修复（pipeline.py 解耦 + ADC 因果重构）
   │
   ├──► Batch 1: Lane 1 & Lane 5 核心红测试成对化（内核调度 + NVS 存储，共 12 项）
   │
   ├──► Batch 2: Lane 2 & Lane 4 总线与模拟外设红测试（I2C/SPI/UART + ADC/DAC，共 9 项）
   │
   ├──► Batch 3: Lane 3 & Lane 6 定时电机与网络协议红测试（LEDC/Timer/WiFi/MQTT，共 7 项）
   │
   ├──► Batch 4: SSOT 元数据补齐与 35/35 双实证全亮（CHECKLIST.md 最终派生）
   │
   └──► Batch 5: M4 物理硬件实机比对（通过 wink.py esp32 / run_esp32_headless_evidence.ps1 硬件核验）
```

#### Batch 0: 治理防线加固与因果短路修复
- [ ] **Task 0.1 (E-3 权力制衡)**: 改造 `wink-micro-app/vendor/esp_idfv61/.governance/tools/loop/pipeline.py`：
  - 废除硬编码 `"auditor": "loop_sop_daemon"`；
  - 流水线仅输出 `candidate_evidence` 候选凭证包，由独立审计裁判 Agent 进行离线核验；
  - 封堵自测自签安全隐患。
- [ ] **Task 0.2 (E-4 运行隔离沙箱)**: 在 `pipeline.py` 中引入 `.governance/runs/<timestamp>-<uuid>/` 隔离沙箱：
  - 保证每次测试与变异在独立沙箱内运行；
  - 门禁全部通过后原子性晋升正式目录，失败完整回滚。
- [ ] **Task 0.3 (W-1 未尽细节: ADC 真实工程输出因果闭环)**:
  - 改造 `peripherals/adc_continuous_read` 场景：废除断言模拟轨输入缓存 `target: "adc:34" == 0.484`，改为断言固件串口输出工程日志 `Voltage: 1500 mV` 与 `Unit: 1, _Channel: 6`；
  - 改造 `peripherals/adc_oneshot_read` 场景：改为断言固件实际采样工程值。

#### Batch 1: Lane 1 (内核调度) & Lane 5 (文件存储) 核心示例成对红测试 (12 项)
- **Lane 1: 系统生命周期与内核调度 (8 项)**
  - [ ] **Task 1.1**: `#002 get-started/hello_world` ➔ `hello_world.fail.scenario.json`（注入启动异常断言稳态防御）
  - [ ] **Task 1.2**: `#020 peripherals/gpio_generic_gpio` ➔ `gpio_generic_gpio.fail.scenario.json`（注入非法输入与中断抖动断言）
  - [ ] **Task 1.3**: `#125 system/esp_event_default_event_loop` ➔ `esp_event_default_event_loop.fail.scenario.json`（注入事件循环未启动/handler 异常）
  - [ ] **Task 1.4**: `#126 system/esp_event_user_event_loops` ➔ `esp_event_user_event_loops.fail.scenario.json`（注入队列超限与循环强制终止）
  - [ ] **Task 1.5**: `#127 system/esp_timer` ➔ `esp_timer.fail.scenario.json`（注入周期为 0 与非法参数拒绝）
  - [ ] **Task 1.6**: `#131 system/freertos_basic_freertos_smp_usage` ➔ `basic_freertos_smp_usage.fail.scenario.json`（注入队列满阻塞与超时捕获）
  - [ ] **Task 1.7**: `#132 system/freertos_real_time_stats` ➔ `freertos_real_time_stats.fail.scenario.json`（注入计数溢出与无有效时钟源防御）
  - [ ] **Task 1.8**: `#151 system/startup_time` ➔ `startup_time.fail.scenario.json`（注入启动超时报警）
- **Lane 5: 存储与文件系统 (4 项)**
  - [ ] **Task 1.9**: `#401 storage/nvs_nvs_iteration` ➔ `nvs_nvs_iteration.fail.scenario.json`（注入未初始化的命名空间与空迭代器）
  - [ ] **Task 1.10**: `#402 storage/nvs_nvs_rw_blob` ➔ `nvs_nvs_rw_blob.fail.scenario.json`（注入键不存在 `ESP_ERR_NVS_NOT_FOUND`，校验 `upstream_errata` 隔离）
  - [ ] **Task 1.11**: `#403 storage/nvs_nvs_rw_value` ➔ `nvs_nvs_rw_value.fail.scenario.json`（注入 `FAULT_NVS_READ_CORRUPT` 断言错误码）
  - [ ] **Task 1.12**: `#415 storage/spiffs` ➔ `spiffs.fail.scenario.json`（注入挂载损坏分区与读取不存在文件防御）

#### Batch 2: Lane 2 (通信总线) & Lane 4 (模拟电学) 核心示例成对红测试 (9 项)
- **Lane 2: 通信协议与串行总线 (5 项)**
  - [ ] **Task 2.1**: `#023 peripherals/i2c_basic` ➔ `i2c_basic.fail.scenario.json`（通过 `FAULT_I2C_NACK` 注入从机无应答并断言返回 `ESP_ERR_TIMEOUT`）
  - [ ] **Task 2.2**: `#024 peripherals/i2c_i2c_eeprom` ➔ `i2c_eeprom.fail.scenario.json`（注入写保护及响应超时）
  - [ ] **Task 2.3**: `#073 peripherals/spi_master_hd_eeprom` ➔ `spi_master_hd_eeprom.fail.scenario.json`（通过 `FAULT_SPI_TRANSFER_FAIL` 注入总线阻断）
  - [ ] **Task 2.4**: `#094 peripherals/uart_uart_async_rxtxtasks` ➔ `uart_async_rxtxtasks.fail.scenario.json`（注入异步 RX 环形缓冲区溢出）
  - [ ] **Task 2.5**: `#098 peripherals/uart_uart_events` ➔ `uart_events.fail.scenario.json`（注入 UART 校验与 FIFO 溢出事件断言）
- **Lane 4: 模拟电学与信号转换 (4 项)**
  - [ ] **Task 2.6**: `#003 peripherals/adc_continuous_read` ➔ `adc_continuous_read.fail.scenario.json`（注入采样通道未使能与过采样超时断言）
  - [ ] **Task 2.7**: `#004 peripherals/adc_oneshot_read` ➔ `adc_oneshot_read.fail.scenario.json`（注入非法通道号断言 `ESP_ERR_INVALID_ARG`）
  - [ ] **Task 2.8**: `#013 peripherals/dac_dac_cosine_wave` ➔ `dac_cosine_wave.fail.scenario.json`（注入越界频率断言参数拒绝）
  - [ ] **Task 2.9**: `#014 peripherals/dac_dac_oneshot` ➔ `dac_oneshot.fail.scenario.json`（注入向已禁用通道写入电压断言返回错误）

#### Batch 3: Lane 3 (定时电机) & Lane 6 (网络与无线) 核心示例成对红测试 (7 项)
- **Lane 3: 定时器、计数与电机控制 (2 项)**
  - [ ] **Task 3.1**: `#047 peripherals/ledc_basic` ➔ `ledc_basic.fail.scenario.json`（注入非法占空比 `>10000bp` 断言参数拒绝）
  - [ ] **Task 3.2**: `#083 peripherals/gptimer_alarm` ➔ `gptimer.fail.scenario.json`（注入 Alarm 计数值为 0 或未启动计数断言）
- **Lane 6: 网络、协议与无线 (5 项)**
  - [ ] **Task 3.3**: `#206 protocols/mqtt_tcp` ➔ `mqtt_tcp.fail.scenario.json`（注入 Broker 连接拒绝与断线重连防御）
  - [ ] **Task 3.4**: `#221 wifi/fast_scan` ➔ `fast_scan.fail.scenario.json`（注入扫描超时与无匹配 SSID）
  - [ ] **Task 3.5**: `#223 wifi/getting_started_softAP` ➔ `softap.fail.scenario.json`（注入不合规密码与启动参数校验失败）
  - [ ] **Task 3.6**: `#230 wifi/scan` ➔ `scan.fail.scenario.json`（注入 Wi-Fi 驱动未启动即发起扫描断言错误）
  - [ ] **Task 3.7**: `#384 bluetooth/bleprph` ➔ `bleprph.fail.scenario.json`（通过 `FAULT_BLE_ADV_REJECT` 注入广播参数被拒绝）

#### Batch 4: SSOT 元数据补齐与 35/35 双实证全亮
- [ ] **Task 4.1**: 补齐全量 35 项的 `negative_cases` 元数据模型（`stimulus`, `expect_error`, `detects`）写入 `checklist.data.json`。
- [ ] **Task 4.2**: 运行 `evidence_verifier.py --verify-all` 确保 35 个示例正向逐步强一致性断言 100% 通过。
- [ ] **Task 4.3**: 运行 Gate 1~5 全部门禁通过无告警（0 warnings, 0 errors）。
- [ ] **Task 4.4**: 运行 `generate_checklist_v1_1.py`，生成 35 项全量点亮 `🟢 [Green ✅ | Red 🛡️ (TWIN-PROOF)]` 徽章的 `CHECKLIST.md`。

#### Batch 5: M4 阶段 ESP32 物理硬件实机交叉核验
- [ ] **Task 5.1**: 硬件测试环境确认（ESP32-WROOM/S3 开发板连接、COM 口识别）。
- [ ] **Task 5.2**: 使用 `run_esp32_headless_evidence.ps1` 和 `wink.py esp32` 对 6 大黄金用例（`blink`, `ledc`, `gptimer`, `uart_echo`, `http_client`, `wifi_sta`）烧录物理硬件。
- [ ] **Task 5.3**: 捕获芯片真实物理串口日志，提取时序哈希，与 Wasm 仿真 Trace 比对，输出《双 Target 物理实机交叉核验实证报告》。
