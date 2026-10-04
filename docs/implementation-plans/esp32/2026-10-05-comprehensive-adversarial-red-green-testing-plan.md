# ESP-IDF 官方示例全维度对抗测试与红绿变异击杀实施计划 (Comprehensive Adversarial Red-Green & Mutation Plan)

> **计划编号**：`PLAN-ESP-20261005-ADVERSARIAL-RED-GREEN`  
> **制定日期**：2026-10-05  
> **实施周期**：2026-10-05 ~ 2026-10-19 (2周)  
> **责任体系**：嵌入式仿真核心架构组 / 治理流水线 SOP 团队  
> **适用目标**：`wink-micro-app/vendor/esp_idfv61` 官方示例库及 UniSim 仿真内核  

---

## 1. 背景与核心目标

### 1.1 现状与可靠性核心痛点
WinkMicroOS 当前已建立起一套基于 Wasm Headless 的 ESP-IDF 官方示例仿真治理体系，累计交付 30 个高保真示例：
1. **原厂镜像 0 修改**：源码逐字节镜面映射，SHA-256 严格锁定；
2. **断言防假绿 (Canary Mutation Kill)**：现有流程通过篡改断言 `matcher` 验证了仿真执行器的 Defect Sensitivity，杜绝了“无条件恒真”的盲绿假通过；
3. **正向因果闭环 (Happy Path)**：各用例均具备强领域亲和性的状态跃迁与物理/总线观测。

**但面向工业级与航天级仿真可靠性，存在极其致命的深水区缺口**：
- **“断言变异”不等于“业务红测试”**：现有的变异击杀只证明了断言器在工作，并没有向固件**真正注入外部硬件/网络故障**；
- **异常分支未激发**：原厂示例代码中的 `ESP_LOGE`、超时重试、清理释放分支从未在仿真中真正走通；
- **负向契约缺失**：30 个已交付条目中，有 20 个条目的 `negative_cases` 数组留空为 `[]`，且磁盘上缺少成对的负向场景文件（`*.fail.scenario.json`）。

### 1.2 核心目标
建立**“正向绿测试（Happy Path） + 逆向红测试（Fault Path） + 三维变异击杀（3D Mutation Kill）”**三位一体的终极可靠性实证防御体系，使仿真系统不仅能证明代码“何时能成功”，更能确凿证明代码“遇到何种异常必然能精确感知并稳健防御”。

---

## 2. 五大落地实施计划 (The 5 Strategic Pillars)

```
                              【可靠性实证金字塔】

                     ▲       Pillar 4. 三维变异击杀引擎 (3D Mutation Engine)
                    / \      Pillar 2. 成对红绿确定性场景 (Twin Scenarios: Green & Red)
                   /   \     Pillar 3. 底座受控故障注入探针 (Controlled Fault Probes)
                  /=====\    Pillar 1. 负向契约元数据全量补齐 (Negative Contract SSOT)
                 /=======\   Pillar 5. 分级回归与双实证看板 (Dual-Proof CI Dashboard)
```

---

### Pillar 1: 负向契约元数据全量补齐 (Negative Contract SSOT)

**目标**：消除 `checklist.data.json` 中 `negative_cases` 留空现象，为全部已交付及后续规划示例建立形式化负向用例模型。

1. **元数据 Schema 严格标准化**：
   每个负向用例必须完整声明以下三要素：
   - `stimulus`（故障激励）：外部注入的具体故障（如 `i2c_nack_address`、`wifi_wrong_password`、`nvs_key_nonexistent`、`ble_adv_payload_overflow`）；
   - `expect_error`（预期表现）：固件底座抛出的标准化错误符号或串口错误日志（如 `ESP_ERR_TIMEOUT`、`ESP_ERR_WIFI_NOT_FOUND`、`ESP_ERR_NVS_NOT_FOUND`）；
   - `detects`（防假绿目标）：此负向用例专门查杀的底座作弊行为（如 `prevent_empty_stub_ack`、`detect_silent_success_fallback`）。
2. **存量 30 条目契约补遗**：
   - 梳理已交付的 30 个 Carrier 原厂源码，提取错误处理分支；
   - 批量更新 `checklist.data.json`，将 21 个空条目的 `negative_cases` 补充完整。
3. **门禁规则升级 (Gate 1)**：
   - 在 `g1_execution_configs.py` 中引入硬校验：凡 `delivery_state == 'verified'` 的条目，其 `negative_cases` 数组长度必须 `>= 1`，否则直接驳回。

---

### Pillar 2: 成对红绿确定性场景规范 (Twin Scenarios: Green & Red Pair)

**目标**：废除单一绿测试文件结构，在每个 Carrier 目录下强制推行**成对场景文件标准**。

1. **场景文件命名与职责划分**：
   - `unisim-scenarios/<app>.scenario.json`（**绿测试**）：黄金正常路径，验证端到端业务成功；
   - `unisim-scenarios/<app>.fail.scenario.json`（**红测试**）：故障注入路径，验证异常捕获与稳态防御。
2. **红测试断言设计标准**：
   - **严禁崩溃**：红测试必须验证仿真器不出现内存越界、死锁或静默退出；
   - **故障感知断言**：
     - 断言日志输出包含标准错误宏（如 `ASSERT_BUS_PAYLOAD: uart -> "ESP_ERR_TIMEOUT"` 或 `"failed to initialize"`）；
     - 断言状态机迁移到异常态（如 `wifi:sta:state -> DISCONNECTED`，`ble:gap:adv_state -> IDLE`）；
   - **时序窗口**：红测试应在故障注入后 $50\text{ms} \sim 200\text{ms}$ 内迅速捕获错误，超时即判失败。
3. **首批打样推进（6 大黄金用例成对化）**：
   - `blink`：时钟源骤停故障场景；
   - `i2c_basic`：从机 NACK 与无应答超时场景；
   - `uart_echo`：RX 缓冲区溢出与奇偶校验错误场景；
   - `wifi_sta`：AP 密码错误无法关联场景；
   - `http_client`：HTTP 404/500 与 DNS 解析失败场景；
   - `nimble_beacon`：广播参数非法拒绝启动场景。

---

### Pillar 3: 底座受控故障注入探针体系 (Controlled Fault-Injection Probes)

**目标**：在 `frameworks/esp_idf` 与 `pal` 仿真适配层中建立标准化、零污染的故障注入探针通道。

1. **协议层受控探针 API**：
   基于 UniSim 原生 `pal_fault_inject` 抽象，在 ESP-IDF Facade 层开放故障触发通道：
   ```c
   // 统一故障通道接口
   wink_status_t sim_esp_fault_inject(uint32_t domain, uint32_t fault_type, uint32_t param);
   ```
2. **四大领域故障探针实现**：
   - **I2C/SPI 总线**：
     - `FAULT_I2C_NACK`：强制将下一次寻址 ACK 信号置高，模拟从机不存在；
     - `FAULT_I2C_TIMEOUT`：模拟 SCL 始终被低电平锁死（Bus Lock）；
   - **Wi-Fi / Netif 网络**：
     - `FAULT_WIFI_AUTH_FAIL`：模拟 AP 握手失败，触发 `WIFI_EVENT_STA_DISCONNECTED`；
     - `FAULT_NETIF_DHCP_TIMEOUT`：模拟 DHCP Server 不分配 IP；
   - **NVS 键值存储**：
     - `FAULT_NVS_PARTITION_FULL`：强制返回 `ESP_ERR_NVS_NOT_ENOUGH_SPACE`；
     - `FAULT_NVS_READ_CORRUPT`：模拟哈希校验失败与页损坏；
   - **NimBLE 蓝牙**：
     - `FAULT_BLE_ADV_REJECT`：模拟 Host 与 Controller 同步超时，拒绝进入广播态。
3. **跨平台洁癖防御**：
   - 故障探针代码使用编译期宏 `#if defined(__EMSCRIPTEN__)` 严格收敛于仿真底座；
   - 严禁向通用 PAL 头文件渗透任何平台专有结构。

---

### Pillar 4: 变异引擎三维升级 (Three-Dimensional Mutation Engine)

**目标**：将 `CanaryMutator` 从单一的“改断言 Expected 值”进化为**三维变异矩阵（3D Mutation Matrix）**。

1. **三维变异矩阵模型**：
   - **维度 A (Assertion-Level Mutant)**：现有机制，篡改预期值验证断言器非盲；
   - **维度 B (Stimulus/Environment Mutant)**：自动篡改场景中的 `INPUT_BUS` 激励载荷、`INJECT_WIFI_FIXTURE` 的 SSID/密码，验证固件能否感知环境劣化并 Fail-Loud；
   - **维度 C (Platform Fault Mutant)**：通过参数向底座打入强制硬件故障，验证固件的错误分支是否被 100% 覆盖。
2. **升级 `verify_kill` 判据状态机**：
   ```text
   变异执行结果判定:
   ├── 退出码 0  ────────► [FAIL-GREEN] 致命假绿: 变异存活，拦截驳回
   ├── 语法/解析崩溃 ────► [INVALID-RUN] 无效变异: 场景语法错误，非有效杀伤
   └── 断言捕获报错 ─────► [KILL-SUCCESS] 杀伤成功: 精确捕获时序偏差或错误分支
   ```
3. **自动化命令行集成**：
   - `mutator.py --mode assertion`：断言级变异（默认）；
   - `mutator.py --mode stimulus`：输入激励级变异；
   - `mutator.py --mode fault`：底座硬件故障变异。

---

### Pillar 5: 分级回归与双实证看板 (Dual-Proof CI Dashboard)

**目标**：让红绿成对实证在流水线、CI 与交付看板中成为第一公民（First-Class Artifact）。

1. **运行器扩展 (`run_esp32_headless_evidence.ps1`)**：
   - 增加 `-RunRedTests` 开关：自动执行应用目录下的 `*.fail.scenario.json`；
   - 增加 `-TwinMode` 开关：同时执行正向与负向测试，双绿（正向通、负向报出预期错误）才算合格；
   - 升级报告结构：`run-report.json` 中清晰分离 `green_run` 与 `red_run` 结果块。
2. **单向派生看板升级 (`generate_checklist_v1_1.py`)**：
   - 在 `CHECKLIST.md` 渲染器中新增状态列：`[Green ✅ | Red 🛡️]`；
   - 只有同时具备正向通过报告与负向击杀证据时，看板才点亮最高信任度标记 `🟢 VERIFIED (TWIN-PROOF)`。
3. **CI 门禁防护 (Gate 5 Anti-Decay)**：
   - Gate 5 增加双轨扫描：若检测到底座或框架被修改，自动触发全量 30 个已交付条目的成对红绿测试；
   - 任何破坏异常感知能力的变更（如把原本返回错误的代码改为空桩返回 0）直接被红用例拦截。

---

## 3. 时间线与交付里程碑 (Timeline & Milestones)

| 阶段 | 时间周期 | 重点交付目标 | 验收交付物 |
| :--- | :---: | :--- | :--- |
| **M1: 契约与探针奠基** | Day 1 ~ Day 4 | 1. 补齐 30 个已交付条目的 `negative_cases` 元数据；<br>2. 实现 I2C、Wi-Fi、NVS 的 3 大核心故障注入探针。 | `checklist.data.json` 补齐；<br>`esp_fault.c` 探针就绪。 |
| **M2: 成对场景打样** | Day 5 ~ Day 8 | 1. 为 `blink`, `i2c_basic`, `uart_echo`, `wifi_sta`, `http_client`, `nimble_beacon` 建立 6 组成对场景；<br>2. 验证红用例 Fail-Loud 与精准报错。 | 6 个 `*.fail.scenario.json`；<br>双向运行报告。 |
| **M3: 变异引擎升级** | Day 9 ~ Day 11 | 1. 升级 `mutator.py` 支持三维变异（断言、激励、故障）；<br>2. 实现流水线自动变异击杀验证。 | 升级版 `mutator.py`；<br>Canary 测试日志。 |
| **M4: 运行器与看板闭环** | Day 12 ~ Day 14 | 1. 升级 `run_esp32_headless_evidence.ps1`；<br>2. 升级 `generate_checklist_v1_1.py` 展现双实证状态；<br>3. 全量门禁复验。 | `CHECKLIST.md` 双实证展示；<br>Gate 1~5 零警告通过。 |

---

## 4. 风险控制与红线 (Risk Governance)

1. **零原厂源码污染红线**：
   严禁为了通过红测试而在原厂业务源码（`main.c` 等）中插入 `#ifdef SIMULATION` 或异常分支。原厂代码必须 100% 保持上游镜面一致。
2. **故障隔离红线**：
   故障注入探针必须在测试结束后提供干净的 `sim_esp_fault_clear()`，严禁在多用例批处理时残留故障状态导致后续用例假红。
3. **二值化审计红线**：
   负向测试必须断言明确的错误码或预期的错误表现；严禁将任何未知的崩溃、堆栈溢出或段错误泛化视为“红测试通过”。
