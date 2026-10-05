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
