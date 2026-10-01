---
name: governance-sop-esp
description: 用于 ESP-IDF 官方示例仿真适配与交付治理的标准作业程序（SOP）。当用户或 Agent 涉及 ESP-IDF 官方示例审查（review）、场景编写（authoring）、无头回归核验（reverify）、门禁扫描（Gate 1~5）或正式交付（delivery）时触发。强制执行任务模式分流、因果链路闭环与严格运行时契约，杜绝退化断言与假绿。
---

# ESP-IDF 官方示例仿真治理与防假绿现场规程 (SOP)

本规程是面向 `wink-micro-app/vendor/esp_idfv61` 官方示例（478 个清单项）适配、Wasm 仿真场景设计与实证交付的**强制性现场作业标准**。

> 📖 **关联规范与基准**：
> - 治理总纲与工作规约：[PLAYBOOK.md](../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/PLAYBOOK.md)
> - 分级标准与实证规范：[CLASSIFICATION-SPEC.md](../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md)
> - 领域断言与能力范式：[references/domain-assertion-guide.md](references/domain-assertion-guide.md)

---

## 🧭 四大任务模式分流 (Operational Modes)

在执行任何操作前，Agent 必须首先识别当前任务的**目标模式**，严禁不加区分直接触发状态修改或交付流水线：

| 模式 | 适用场景 | 允许的操作与边界 | 严禁的操作 |
| :--- | :--- | :--- | :--- |
| **1. Review<br>（只读审查）** | 检查示例现状、分析断言亲和性、核对差距报告。 | 读取源码、清单、报告与门禁规则；输出差距与缺口分析。 | **严禁**添加 `-WriteEvidence`；**严禁**修改清单 JSON 或看板。 |
| **2. Authoring<br>（场景编写）** | 在授权范围内编写或优化 `.scenario.json` 场景脚本。 | 核对能力阶梯（仅用 Tier 1/2）；编写合规 Target 与 Matcher；执行 Canary 探针自检。 | **严禁**虚构 Tier 3 未实现接口；**严禁**修改官方 `*_main.c`。 |
| **3. Reverify<br>（无头复验）** | 对已有凭据的示例执行确定性重放与回归。 | 运行无参数采集脚本（纯无头执行，不传 `-WriteEvidence`）；核验结果一致性。 | **严禁**覆盖已有审计或历史有效凭据。 |
| **4. Delivery<br>（正式交付）** | 示例具备全部闭环条件，正式晋升 `verified` 并勾选看板。 | 严格执行标准 8 步 SOP；更新配置级状态；通过全门禁扫描并渲染看板。 | **严禁**跳过因果杀伤校验；**严禁**在门禁报错时强行合入。 |

---

## 🎯 核心定位与真测试四大公理

任何示例在特定配置被标记为已交付（`executions[config_id].delivery_state: verified` / 看板 `[x]`）前，必须经受真实的因果链测试。**严禁以虚假的“全绿报告”掩盖未实现或未测试的代码**。

所有场景断言必须满足**四大真测试公理**：

| 公理 | 原则 | 含义与判定标准 |
| :--- | :--- | :--- |
| **A-1** | **因果性公理 (Causality)** | 任何断言通过的物理量变化，必须能够证明是由该示例的核心业务逻辑（算法、协议栈、状态机）显式驱动，而非上电默认值、静态恒定值或空桩虚设。 |
| **A-2** | **状态跳变公理 (State Transition)** | 动态外设与周期协议必须至少经历一次“有效状态迁移”（如 Low $\to$ High $\to$ Low，Disconnected $\to$ Connected）。只有单一稳态常数的场景无效。 |
| **A-3** | **拓扑闭环公理 (Loop-Closure)** | 交互式总线与协议（UART Echo / HTTP / I2C）必须具备激励输入与对端响应的完整因果闭环；单向发布协议（BLE 广播 / UDP 遥测）以协议栈正确发出合规报文载荷为闭环终点。 |
| **A-4** | **变异杀伤公理 (Killability)** | 优质的断言具备对缺陷的敏感杀伤性。若核心业务被破坏，断言**必须立即变红报错**。无法被变异杀死的测试一律属于假绿。 |

---

## 🚫 防假绿“九不准”红线禁令 (The 9 Anti-Patterns)

在编写 `unisim-scenarios/<name>.scenario.json` 与底层门面代码时，**绝对禁止**以下 9 类作弊或退化模式：

| 编号 | 反模式名称 | 典型表现（违规立即阻断） |
| :---: | :--- | :--- |
| **P-1** | **纯电源静态占位** | 场景内**仅有**静态电源断言（如仅 `power:VCC_3V3 == 3.3`），而完全没有业务外设出口断言（注：允许电源断言作为辅助环境检查，但严禁作为核心业务断言）。 |
| **P-2** | **恒真/无条件断言** | 使用恒真逻辑掩盖错误（如断言 `error != -9999`），或使用运行器不支持的比较字符串（如 `matcher: ">0"` 导致永远判等失败）。 |
| **P-3** | **缺少状态跳变** | 在闪灯、定时器、PWM、状态机用例中，仅在 50ms 断言初始电平为 0，没有后续高低跳变验证。 |
| **P-4** | **单边网络虚应** | 请求-响应式协议（HTTP/I2C Client）仅断言发出了请求，完全不验证虚拟服务器的返回状态码与响应体。 |
| **P-5** | **缺乏故障注入** | 通信协议与状态机示例从未测试断网、超时、非法报文等异常路径，无法验证健壮性。 |
| **P-6** | **未收割 API 静默打桩** | 底座未收割的 ESP-IDF API 直接写空函数返回 `ESP_OK` 掩盖未实现，使固件进入虚假正常流程。 |
| **P-7** | **跨用例状态串扰** | 上一个场景注册的回调、开启的任务或 Mock 路由未在复位时清空，导致下一个场景偶发通过或失败。 |
| **P-8** | **审计结论自填自销** | 测试脚本或 AI 自动将自身未审计的条目填上 `auditor = "arch_team"` 冒充专业架构审查。 |
| **P-9** | **无断言空跑报告** | 场景内无断言步骤，或仅靠 Wasm 进程未崩溃退出（0 步骤 passed 或 steps 为空）即声称“测试通过”。 |

---

## 🛠️ 现场作业八步标准工作流 (8-Step SOP)

```text
┌────────────────────────────────────────────────────────────────────────┐
│ [Step 1] 认领与清单锁定  ──► [Step 2] 原厂源码白盒分析 ──► [Step 3] 门面适配层健全性核验 │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ [Step 4] 场景因果断言设计 ──► [Step 5] 因果链与探针校验 ──► [Step 6] 真实仿真与凭据采集 │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ [Step 7] 自动化门禁复核   ──► [Step 8] 数据源归档与看板自动渲染 (DoD 竣工)            │
└────────────────────────────────────────────────────────────────────────┘
```

### Step 1: 认领与清单锁定
1. 打开清单数据源文件：  
   `wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json`；
2. 依据应用名称或路径找到对应的条目；
3. 锁定目标配置 `config_id`（例如：`wasm_sim_standard` 或 `sim_browser_esp32`）；
4. 确认该配置的 `executions[config_id].delivery_state` 为 `"planned"` 或 `"building"`（`delivery_state` 属于配置级别，非条目根属性）。

### Step 2: 原厂源码白盒分析
1. **原厂源码严禁修改任何一行**（严格执行“一行不改”准则，受上游 SHA-256 锁定）；
2. 深入分析官方 `*_main.c` 源码：
   - **输入源**：按键、网络报文、串口接收、定时器中断、ADC 采样等；
   - **核心因果链**：状态机迁移、数据计算、协议编解码、业务逻辑分支；
   - **出口观测点**：GPIO 翻转、UART 发送、PWM 占空比调整、网络请求与解析、I2C 寄存器写入；
   - **生命周期**：单次任务、循环任务、事件回调驱动还是软复位。

### Step 3: 门面适配层健全性核验
1. 检查示例引用的 ESP-IDF API 在 `wink-micro-os/frameworks/esp_idf/` 中是否完整收割；
2. **严禁手工捏造伪头文件**；
3. 若遇未收割 API：
   - 若属于可支持范围：经由 Harvester 生成头文件并在门面实现文件（`.c`）中提供真实行为实现；
   - 若属于 Out-of-Scope：在对应的门面实现函数中记录错误日志并返回 `ESP_ERR_NOT_SUPPORTED`，条目配置置为 `[-]`，**严禁写静默空函数返回 `ESP_OK`**；
   - ⚠️ **`wink_sla.h` 严格提示**：该文件头部明确标注为自动生成工具只读消费产物，**严禁手工编辑**！

### Step 4: 场景与因果断言设计
1. 在 `<app_dir>/unisim-scenarios/<name>.scenario.json` 编写仿真场景；
2. **严格遵循能力阶梯（参见 [references/domain-assertion-guide.md](references/domain-assertion-guide.md)）**：
   - **Tier 1 (已验证)**：控制类断言 `gpio:<pin>`（电平跳变序列）或 `pwm:<ch>`（定点占空比）；
   - **Tier 2 (部分支持)**：串口注入 `INPUT_BUS`，断言 `ASSERT_BUS_PAYLOAD`；Wi-Fi 注入 `INJECT_WIFI_FIXTURE`，断言 `wifi:sta:state` 与 `netif:sta:ip`；
   - **Tier 3 (拟实现草案)**：HTTP 虚拟路由（`INJECT_NET_FIXTURE`、`http:client:*`）、NimBLE GATT 等，**本地运行时尚未合入解析契约，严禁虚构场景交付**！对应条目保持 `planned` 或 `blocked_on_runtime`；
3. **Matcher 语法红线**：
   - 严禁写 `matcher: ">0"`（会被当作字符串判等失败）；
   - 数值范围断言必须使用 `{ "$between": [1, 2147483647] }` 或 `{ "$near": { ... } }`；
4. **断言 Target 必须已在 `wink-app.json` 的 `devices` 拓扑中声明**（Gate 1 强制检查）。

### Step 5: 因果闭环与 Canary 探针杀伤校验
为证明断言不是假绿，在提交前必须执行严格的因果链检验：
1. **基准通行**：正常执行场景，100% 绿灯通过；
2. **断言器探针自检 (Canary Check)**：临时在场景副本中修改一个预期值（例如将 IP 改为 `0.0.0.0` 或翻转 GPIO 预期电平），执行仿真器，**必须在对应 Target 处准确报出 `AssertionFailed` 错误**（证明断言已被执行且未被静默吞掉；解析错误或进程崩溃不算杀伤）；
3. **非破坏性固件因果检验**：**严禁直接修改原厂源码**！通过场景环境扰动（如输入错误密码、断开总线），或利用可复原的测试编译宏注入故障，指定业务断言必须失败；
4. **恢复基准**：还原所有修改，确认仿真重新 100% 绿灯通过。

### Step 6: 真实构建与无头仿真实证采集
1. 端到端编译 Wasm 产物（生成真实非空的 `unisim-assets/` 三件套）；
2. 运行无头实证采集脚本，必须明确指定 `-ConfigId`：
   ```powershell
   powershell -ExecutionPolicy Bypass -File wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1 -App <app_name> -ConfigId wasm_sim_standard -WriteEvidence
   ```
   > 💡 **只读复验提示**：若不带 `-WriteEvidence`，脚本同样会编译并执行仿真，仅跳过凭据归档与状态写回，用于只读回归。
3. 检查生成的凭据：
   - 专属报告已归档至 `.governance/reports/<target_app_dir>/run-report.json`；
   - 报告满足：`results` 数组非空、`passedSteps > 0`、`failedSteps == 0`、`errorSteps == 0`；
   - `assets_sha256` 与 `scenario_sha256` 计算锁定，写入 `executions[config_id].evidence`。

### Step 7: 完整性与自动化门禁复核
运行官方门禁流水线进行严格核验：
```powershell
# 1. 运行 Gate 1 完整性扫描
python wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --gate 1

# 2. 运行凭据强一致性校验
python wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py --verify-all
```
> ⚠️ **关于 `--mode pr` 的注意事项**：  
> `run_gates.py --mode pr` 会根据 git diff 变更文件筛选执行规则。若未修改相关文件，规则可能被跳过。因此**不能单凭 `--mode pr` 退出码 0 即宣称“全量验证完成”**，正式交付必须运行显式的 `--gate 1` 与 `--verify-all` 检查。

### Step 8: 数据源归档与看板自动渲染 (DoD 竣工)
1. 打开 `wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json`；
2. 确认已核验配置的 `executions[config_id].delivery_state` 置为 `"verified"`；
3. 审计字段必须诚实，未经专人签署禁止自认 `arch_team`（违反 P-8 会被 Gate 1 拦截）；
4. 对更新后的 JSON 数据重跑门禁确认 0 Error；
5. 运行派生生成脚本，单向更新看板：
   ```powershell
   python wink-micro-app/vendor/esp_idfv61/.governance/tools/generate_checklist_v1_1.py
   ```
6. 检查 `CHECKLIST.md`，确认相应条目依据规则规范呈现 `[x]`。

---

## 📋 交付（DoD）深度验收自查清单

在任何 PR 或提交中，核对以下项目是否全部满足：

- [ ] **任务模式明确**：当前属于 Delivery 模式，非 Review 或 Authoring 临时态。
- [ ] **源码纯洁**：原厂业务代码 `*_main.c` 未修改任何一行，哈希与官方源保持一致。
- [ ] **能力合规**：场景所用断言 Target 与 Step 均落在 **Tier 1 / Tier 2 已验证能力阶梯**内，无虚构接口。
- [ ] **语法正确**：无 `">0"` 比较字符串，数值范围使用 `$between` 或 `$near`。
- [ ] **非空产物**：`unisim-assets/` 包含大小合法的 `device-tree.json`、`wink_simulator.js`、`wink_simulator.wasm`。
- [ ] **真实因果**：场景断言命中了示例核心业务出口（非静态电源、非恒真比较）。
- [ ] **状态跳变**：周期或时序用例存在至少 1 次状态/电平跳变断言。
- [ ] **拓扑对应**：断言用到的引脚与器件在 `wink-app.json` 中均有清晰定义。
- [ ] **杀伤自证**：通过了 Canary 预期反转报错自检，且证明该失败是业务断言失败而非 runner 崩溃。
- [ ] **专属凭据**：报告归档于 `.governance/reports/` 且指定 `config_id` 锁定，无共享文件回退。
- [ ] **门禁通过**：`run_gates.py --gate 1` 与 `evidence_verifier.py --verify-all` 均 Exit Code 0，零 ERROR。
- [ ] **单向生成**：`CHECKLIST.md` 仅由 Python 脚本自动渲染产生，未做纯手工编辑。

---

## 📚 常用命令速查

```powershell
# 1. 运行指定应用与配置的无头实证并回写凭据
powershell -ExecutionPolicy Bypass -File wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1 -App <app_name> -ConfigId wasm_sim_standard -WriteEvidence

# 2. 纯只读回归核验（不回写治理凭据）
powershell -ExecutionPolicy Bypass -File wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1 -App <app_name>

# 3. 运行 Gate 1 完整性门禁扫描
python wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --gate 1

# 4. 运行全量已交付凭据核验
python wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py --verify-all

# 5. 运行门禁单元测试套件
pytest wink-micro-app/vendor/esp_idfv61/.governance/gates/tests/ -v

# 6. 重新渲染 CHECKLIST.md 看板
python wink-micro-app/vendor/esp_idfv61/.governance/tools/generate_checklist_v1_1.py
```
