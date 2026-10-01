<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：ESP-IDF 示例工程 Headless 仿真防假绿验证体系落地战役 (v1.0)

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261001-ESP-IDF-HEADLESS-ANTI-FALSE-GREEN-v1.0 |
| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261001-ESP-IDF-HEADLESS-ANTI-FALSE-GREEN-v1.0 |
| 状态 | 🟢 **In Progress（Phase P0 门禁防空、Phase P1 AI 行为约束器已全部完成，进入 Phase P2）** |
| 日期 | 2026-10-01 |
| 周期估算 | 3~4 个工作日（分 P0 止血防空、P1 执行手卡、P2 首个端到端标杆三阶段） |
| 优先次序 | **P0 门禁防空与证据漏洞修补 ➔ P0 存量假绿场景诚实回退 ➔ P1 AI 行为约束 Skill 建设 ➔ P2 运行器加固与 HTTP 黄金标杆闭环** |
| 决策与设计依据 | [架构蓝图：Anti-False-Green Headless Verification Architecture Blueprint (v1.2.0)](../../../brain/2b8b57be-10c6-457e-991c-ae51204d0d7c/anti_false_green_architecture_blueprint.md)<br>[ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0003：生产口径与保真边界约束（永不承诺虚实恒等）](../../decisions/unisim/0003-simulation-fidelity-boundary.md)<br>[ADR-0091：多配置实例与五维正交 Schema 架构决策](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)<br>[ADR-0092：ESP-IDF 官方示例仿真治理前置筑基宪章](../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md) |
| 管辖数据源 | [`checklist.data.json`](../../../wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json)、[`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/.governance/catalog/capability-catalog.yaml)、[`.governance/gates/`](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/) |
| 实施目标文件 | `.governance/gates/rules/g1_scenario_semantic_integrity.py`、`.governance/gates/evidence_verifier.py`、`.governance/gates/gates.yaml`、`.agents/skills/governance-sop-esp/SKILL.md`、`wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1`、`wink-micro-app/vendor/esp_idfv61/protocols/http_client/*`、`.governance/specs/PLAYBOOK.md` |
| 验收门禁 | `pytest .governance/gates/tests/test_rules/`、`python .governance/gates/run_gates.py --gate 1`、`powershell run_esp32_headless_evidence.ps1 -App http_client -WriteEvidence` |

---

## 一、 战略总目标与全局验收标准 (DoD)

### 1.1 战役背景与破局痛点
根据 2026-10-01 对 `wink-micro-app/vendor/esp_idfv61` 现存场景和实证运行器的深度白盒审查，当前仿真验证体系存在**重大“虚假繁荣”与假绿漏洞**：
1. **纯电源轨断言冒充业务通过（反模式 P-1）**：
   - 现存 9 个示例场景中，`http_client`、`mqtt_tcp`、`bleprph`、`i2c_basic`、`gptimer_alarm` 共 **5 个应用场景中仅包含 `power:VCC_3V3 == 3.3` 或 `power:VCC_WIFI == 3.3` 断言**；
   - 只要 Wasm 虚机上电，哪怕固件在 `app_main` 第一行死循环或发生段错误崩溃，断言依然 100% 变绿，且已被 `evidence_verifier.py` 生成了全绿凭据；
2. **证据核验器存在“空步骤”与“默认首项”防空漏洞（反模式 P-4/P-5/P-7）**：
   - `evidence_verifier.py:143-150` 允许仅包含 `{"status": "passed"}` 且 `failedSteps == 0` 的报告通过，即使 `results` 数组为空、断言步骤数为 0 也判定有效；
   - 场景哈希计算存在仅绑定 `scen_files[0]` 的隐患，且脚本存在若未显式传参则默认回退到历史共享报告或首个配置的路径；
3. **缺乏机器物理拦截（Gate 缺位）**：
   - 现行 Gate 1 的 `g1_carrier_landing_integrity.py` 仅检查 `unisim-assets/` 与 `unisim-scenarios/` 目录和文件是否存在，**完全不审查断言目标与被测应用业务领域的契合度**，导致 AI 或开发者敷衍生成的伪断言畅通无阻；
4. **AI 缺乏步进式执行手卡（SOP 缺位）**：
   - AI 在处理官方示例落地时，由于没有强制加载的操作规程 Skill，容易凭 LLM 惰性直接生成最精简的静态断言交差。

### 1.2 全局验收标准 (DoD)
- [x] **DoD-1（物理门禁硬拦截）**：落地 `g1_scenario_semantic_integrity.py` 规则，将非电源工程仅断言电源轨、未知 Target、零断言执行、以及引脚与 `wink-app.json` 不匹配的伪场景在 CI 中 **100% 阻断（Exit 1）**；
- [x] **DoD-2（证据核验器彻底防空）**：重构 `evidence_verifier.py`，强制核验非空执行集（`passedSteps > 0` 且 `totalSteps == passedSteps`）、必须匹配配置身份、禁止任何形式的共享历史报告隐式回退；
- [x] **DoD-3（存量数据诚实回归）**：对现存全部示例场景执行静态普查，将 5 个仅有电源断言的应用在 `checklist.data.json` 中**诚实撤销或标记为未完成**，绝不将伪证据带入交付看板；
- [ ] **DoD-4（AI 行为约束 SOP 固化）**：建立专用的 `.agents/skills/governance-sop-esp/SKILL.md`，提供八步流水线、断言白名单字典、反模式红线卡与变异自测清单；
- [ ] **DoD-5（标杆样板间真测试闭环）**：将 `protocols/http_client` 彻底重构为**首个黄金标杆**：基于 `sim_net_responder.h` 注入真实 HTTP 路由，断言状态码 `200`、数据流接收量及应用回调，并通过**反向变异金丝雀（Mutation Canary）**证明其具备击杀能力；
- [ ] **DoD-6（运行器沙箱与回滚保障）**：改造 `run_esp32_headless_evidence.ps1`，强制每个场景在全新冷启动的 Wasm 实例中执行，并提供故障注入的 RAII 强回滚清理机制。

---

## 二、 核心架构设计与防线部署

### 2.1 静态与动态防假绿多层拦截网

```
                                  防假绿多层拦截防御体系
+─────────────────────────────────────────────────────────────────────────────────────────────+
│ [第 1 层: AI 行为手卡] .agents/skills/governance-sop-esp/SKILL.md                           │
│  - 强制执行 SOP: 源码因果抽取 -> 领域 Target 绑定 -> 激励编排 -> 变异自检 -> 证据核验       │
+─────────────────────────────────────────────────────────────────────────────────────────────+
                                                │ (开发者/AI 产出场景脚本)
                                                ▼
+─────────────────────────────────────────────────────────────────────────────────────────────+
│ [第 2 层: 静态语义门禁] Gate 1: g1_scenario_semantic_integrity.py (CI 物理拦截)            │
│  - 拦截 1: 绝对禁止退化断言 (非电源工程仅包含 power:* 即刻判定 FAIL)                        │
│  - 拦截 2: 领域强亲和性 (Wi-Fi 必有 wifi:*, HTTP 必有 http:*, GPIO 必有有效引脚)            │
│  - 拦截 3: 三方拓扑闭环 (引脚必须在 wink-app.json 声明，能力必须在 capability-catalog 存在)│
│  - 拦截 4: 零执行与静态常数检查 (断言数 >= 2，必须包含动态状态跳变或因果回路)               │
+─────────────────────────────────────────────────────────────────────────────────────────────+
                                                │ (静态校验通过，拉起无头执行)
                                                ▼
+─────────────────────────────────────────────────────────────────────────────────────────────+
│ [第 3 层: 运行期隔离与金丝雀] run_esp32_headless_evidence.ps1                              │
│  - 物理隔离: 每一个 Scenario / Fault Run 启动全新的 Wasm 虚拟机实例 (杜绝 static 变量污染) │
│  - 出口级观测: 捕获经协议栈/子系统处理后的出口真实净荷 (杜绝 API 镜像直通回显)              │
│  - 变异金丝雀 (Canary): 自动注入破坏性变异，断言必须变红杀伤 (Kill)，证明可证伪性          │
│  - 强回滚保障: RAII 上下文管理器，测试异常崩溃强制重置环境，杜绝跨用例污染                 │
+─────────────────────────────────────────────────────────────────────────────────────────────+
                                                │ (无头仿真产出专属 run-report.json)
                                                ▼
+─────────────────────────────────────────────────────────────────────────────────────────────+
│ [第 4 层: 只读证据校验闭环] evidence_verifier.py                                            │
│  - 严格 Schema: 拒绝 0 步骤报告，拒绝 failedSteps=0 但 passedSteps=0 的虚假报告            │
│  - 强身份绑定: run_id、config_id、全量场景 SHA-256 复合摘要精确对账                        │
│  - 职责分离: 严禁自动填入 audit 审计合格，统一由 Gate 1 裁判器生成 CHECKLIST 看板           │
+─────────────────────────────────────────────────────────────────────────────────────────────+
```

### 2.2 领域 Target 白名单与亲和性矩阵 (Domain Affinity Matrix)

`g1_scenario_semantic_integrity.py` 将根据应用所属的目录或 `wink-app.json` 中配置的类别，强制施加 Target 白名单约束：

| 领域目录 | 允许的核心 Target 模式（至少包含 1 项） | 绝对禁止的断言形态 |
| :--- | :--- | :--- |
| `get-started/blink*`<br>`peripherals/ledc*` | `gpio:<pin>`（必须匹配 `wink-app.json`）、`pwm:<channel>:*` | 仅断言电源轨；仅断言未声明的非法引脚 |
| `peripherals/uart*` | `uart:<port>:rx_bytes`、`uart:<port>:tx_bytes`、数据回环流 | 仅断言串口控制器已初始化，不测数据收发 |
| `peripherals/i2c*` | `i2c:<bus>:slave:<addr>:*`、从机寄存器读写流 | 仅断言 I2C 总线使能，无从机交互 |
| `wifi/*` | `wifi:sta:state`、`wifi:scan:*`、`netif:sta:ip`、网络事件 | 仅断言 `power:VCC_WIFI`，无状态机跃迁 |
| `protocols/http*` | `http:client:status_code`、`http:client:rx_bytes`、响应事件 | 仅断言电源轨；无 `INJECT_NET_FIXTURE` 外部路由注入 |
| `protocols/mqtt*` | `mqtt:client:state`、`mqtt:tx:topic`、`mqtt:rx:payload` | 仅断言网络就绪，无发布/订阅行为 |
| `bluetooth/*` | `ble:gap:adv_state`、`ble:gatt:service_count`、特征值 UUID | 仅断言蓝牙射频电源 3.3V，无广播与服务注册 |

---

## 三、 详细实施任务拆解 (Work Breakdown Structure)

### 阶段 P0：执行防空与门禁门栓（拦截假绿与漏洞修补）

#### 任务 T0.1：[x] 实现 Gate 1 静态语义门禁规则 `g1_scenario_semantic_integrity.py`
- **目标文件**：
  - 新增：`.governance/gates/rules/g1_scenario_semantic_integrity.py`
  - 新增：`.governance/gates/tests/test_rules/test_g1_scenario_semantic_integrity.py`
- **详细内容**：
  1. 遍历扫描所有落盘应用的 `unisim-scenarios/*.scenario.json`；
  2. **规则 1（Degenerate Assertion Penalty）**：若非电源管理类工程，全部 steps 中只有 `power:*` 断言，输出 `severity="error"`；
  3. **规则 2（Domain Affinity Check）**：根据应用路径与能力，核验断言 Target 是否命中上述亲和性矩阵；
  4. **规则 3（Tri-Partite Consistency）**：若断言包含 `gpio:<pin>`，必须在 `wink-app.json` 的 `devices` 中存在；
  5. **规则 4（Temporal Transition Threshold）**：总断言数 $\ge 2$，且在单一 target 上不能全为静态常量；
  6. 配套完备的单元测试，包含全量正向与 8 种典型负向测试用例（覆盖率 100%）。
- **门禁验证**：`pytest .governance/gates/tests/test_rules/test_g1_scenario_semantic_integrity.py` 全部通过（8/8 passed）。

#### 任务 T0.2：[x] 重构修补只读证据核验器 `evidence_verifier.py`
- **目标文件**：
  - 修改：`.governance/gates/evidence_verifier.py`
  - 修改：`.governance/gates/tests/test_evidence_verifier.py`
- **详细内容**：
  1. 收紧 `verify_execution_report`：强制要求 `results` 数组非空，每个结果必须包含 `summary.passedSteps > 0` 且 `summary.failedSteps == 0`、`summary.errorSteps == 0`；**坚决拒绝 0 步骤的“空通过”报告**；
  2. 修复场景摘要绑定：禁止仅取 `scen_files[0]`，必须将目录下所有 `*.scenario.json` 按正规化路径排序后计算复合 SHA-256；
  3. 封堵共享回退漏洞：若指定应用未生成专属报告，严禁回退读取 `artifacts/run-report.json` 历史遗留文件；
  4. 封堵自动审计越权：验证过程中严禁篡改 `audit` 字段。
- **门禁验证**：`pytest .governance/gates/tests/test_evidence_verifier.py` 全部通过（11/11 passed）。

#### 任务 T0.3：[x] 将新规则注册进 Gate 1 系统
- **目标文件**：
  - 修改：`.governance/gates/gates.yaml`
  - 修改：`.governance/gates/run_gates.py`
- **详细内容**：
  1. 在 `gates.yaml` 的 `gate_1.rules` 注册 `g1.scenario_semantic_integrity`；
  2. 配置阻断级别为 `blocking: true`，使其在 `--mode pr` 与 `--mode nightly` 下均强制生效。
- **门禁验证**：`python .governance/gates/run_gates.py --gate 1` 执行正常，精准拦截假绿应用。

#### 任务 T0.4：[x] 存量场景普查与诚实数据对账
- **目标文件**：
  - 普查目标：`wink-micro-app/vendor/esp_idfv61/` 下所有现存应用
  - 修改：`checklist.data.json`
- **详细内容**：
  1. 运行 `g1_scenario_semantic_integrity.py` 扫描现有 9 个应用场景；
  2. 对识别出的 5 个纯电源断言场景（`http_client`, `mqtt_tcp`, `bleprph`, `i2c_basic`, `gptimer_alarm`），执行**诚实数据流转**：
     - 若其在 `checklist.data.json` 中被误标为 `verified`，坚决撤销并回退为 `planned` 或 `building`；
     - 清除虚假的 `evidence` 字段；
  3. 重新运行看板渲染器，确保 `CHECKLIST.md` 真实反映存量现状，杜绝虚假繁荣。
- **门禁验证**：`python .governance/gates/run_gates.py --gate 1` 在真实仓上 0 错误（存量诚实标记为 planned，新规则不再误放行假绿）。

---

### 阶段 P1：AI 行为约束器建设（Skill SOP 固化）

#### 任务 T1.1：[x] 创建专用 AI 技能包 `governance-sop-esp`
- **目标文件**：
  - 新增：`.agents/skills/governance-sop-esp/SKILL.md`
- **详细内容**：
  1. 设定精准触发关键词：`esp-idf vendor example`、`unisim-scenarios`、`headless evidence`、`防假绿`、`DoD 验收`；
  2. 固化 **AI 八步流水线 SOP**（认领与清单锁定 $\to$ 原厂源码白盒分析 $\to$ 门面适配层健全性核验 $\to$ 场景因果断言设计 $\to$ Canary 变异杀伤校验 $\to$ 真实仿真与凭证采集 $\to$ Gate 1~5 门禁全扫描 $\to$ 数据源归档与看板自动渲染）；
  3. 编写**反模式红线禁令速查卡（Anti-Pattern Red Lines）**：醒目警告 AI 严禁在非电源工程仅写 `power:*`，严禁编写单点无跃迁断言，严禁手工修改看板；
  4. 包含 **DoD 深度验收自查清单**与常用命令速查。
- **完成结果**：[SKILL.md](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/.agents/skills/governance-sop-esp/SKILL.md) 编写完成并已生效。

#### 任务 T1.2：[x] 编写领域断言白名单字典与变异检查指导
- **目标文件**：
  - 增补：`.agents/skills/governance-sop-esp/references/domain-assertion-guide.md`
- **详细内容**：
  1. 针对 GPIO、UART、I2C、Wi-Fi、HTTP、MQTT、BLE 7 大门面域，分别给出合规场景模版与断言示例；
  2. 详细说明“断言自检（改错期望值）”与“业务敏感性检查（切断业务行为）”的区别与实操步骤。
- **完成结果**：[domain-assertion-guide.md](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/.agents/skills/governance-sop-esp/references/domain-assertion-guide.md) 落盘就绪。

#### 任务 T1.3：[x] 回写更新治理设计规范
- **目标文件**：
  - 修改：`.governance/specs/PLAYBOOK.md`
  - 修改：`.governance/specs/CLASSIFICATION-SPEC.md`
- **详细内容**：
  1. 在 `PLAYBOOK.md` 阶段三中，正式引入防假绿验收标准与 `g1_scenario_semantic_integrity` 门禁说明；在零章新增硬性门禁 7；
  2. 在 `CLASSIFICATION-SPEC.md` 协同矩阵中接入 `governance-sop-esp`，并在零章确立宪章铁律六（防假绿与语义完整性硬性门禁）；
  3. 明确将“通过业务故障杀伤校验”作为打勾的必备前置条件。
- **完成结果**：两份治理核心法典回写完成并通过 CI 检验。

---

### 阶段 P2：首个端到端黄金标杆闭环（HTTP Client 真实化攻坚）

#### 任务 T2.1：加固运行脚本 `run_esp32_headless_evidence.ps1`
- **目标文件**：
  - 修改：`wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1`
- **详细内容**：
  1. **独立沙箱化执行**：确保每次调用 `wink.py sim run` 时在独立的临时目录或隔离上下文中拉起 Wasm 虚拟机，避免 Node.js 进程常驻导致的全局变量残留；
  2. **引入 `-Canary` 变异校验参数**：支持自动化断言器自检（在内存中临时篡改期望值跑一次，必须报错返回非零，证明断言有效）；
  3. **增强报告路径隔离**：每个应用输出专属 `artifacts/run-report-<app>.json`，杜绝共享文件冲突与旧凭据冒充。

#### 任务 T2.2：重构 `protocols/http_client` 场景与出口级断言
- **目标文件**：
  - 修改：`wink-micro-app/vendor/esp_idfv61/protocols/http_client/unisim-scenarios/http_client.scenario.json`
- **详细内容**：
  1. 彻底废黜现有的两条 `power:VCC_3V3` 假断言；
  2. 依据 `sim_net_responder.h`，在步骤 0 注入虚拟应答路由：
     ```json
     {
       "type": "INJECT_NET_FIXTURE",
       "timeUs": "0ms",
       "protocol": "http",
       "routes": [
         {
           "method": "GET",
           "url_prefix": "http://httpbin.org/get",
           "status_code": 200,
           "headers": { "Content-Type": "application/json" },
           "body": "{\"origin\":\"127.0.0.1\",\"url\":\"http://httpbin.org/get\"}"
         }
       ]
     }
     ```
  3. 编排真正的出口级业务断言：
     - 断言 `http:client:status_code == 200`；
     - 断言 `http:client:rx_bytes_total > 0`；
     - 捕获应用层 HTTP 事件回调；
  4. 编排故障注入步骤（注入 404 或网络阻断），验证应用进入错误处理路径。

#### 任务 T2.3：执行端到端实证与凭据归档闭环
- **详细内容**：
  1. 运行 `powershell run_esp32_headless_evidence.ps1 -App http_client`；
  2. 执行 `-Canary` 自检，验证篡改期望值后准确变红（Kill）；
  3. 执行 `-WriteEvidence` 写入真实有效凭据；
  4. 运行 `python .governance/gates/run_gates.py --mode nightly` 验证全量门禁 100% 绿灯；
  5. 重新生成看板，`protocols/http_client` 正式晋升为全项目第一个**无懈可击的防假绿黄金标杆**！

---

## 四、 实施排期与里程碑矩阵

```
Day 1: [P0 门禁防空与证据封堵]
       ├── T0.1 实现 g1_scenario_semantic_integrity.py 与全量单测
       ├── T0.2 重构修补 evidence_verifier.py 漏洞
       └── T0.3 注册 Gate 1 并进行本地验证
Day 2: [P0 存量对账 + P1 Skill 建设]
       ├── T0.4 存量场景普查与 5 个假绿应用诚实回退
       ├── T1.1 创建 esp-idf-carrier-verification/SKILL.md
       ├── T1.2 编写领域断言白名单字典
       └── T1.3 回写 PLAYBOOK.md 规范
Day 3: [P2 运行器加固与标杆攻坚]
       ├── T2.1 加固 run_esp32_headless_evidence.ps1 (沙箱与 Canary)
       ├── T2.2 重构 http_client.scenario.json 注入真实路由与出口断言
       └── T2.3 执行端到端实证闭环，产出黄金标杆凭据
Day 4: [全量回归与终验验收]
       ├── 全量 Gate 1~5 门禁自动化回归 (run_gates.py --mode nightly)
       ├── 负向攻击测试 (验证 8 类假绿场景均被物理击杀)
       └── 交付评审与归档
```

---

## 五、 风险分析与应对预案

| 潜在风险 | 影响度 | 预警信号 | 应对与缓解预案 |
| :--- | :---: | :--- | :--- |
| **R-1 存量 5 个应用回退引发看板进度下降** | 中 | `CHECKLIST.md` 中 verified 数量减少 5 个 | **契约诚实优于虚假繁荣**（ADR-0012 原则）。在评审中明确说明原 5 个应用为假绿断言，回退是恢复真相的必须动作，建立公众技术信誉。 |
| **R-2 某些复杂协议出口观测探针缺失** | 高 | 示例运行正常，但 UniSim 无法捕获底层事件 | **分层降级防御**：若当前 UniSim 尚无该协议深层出口探针，在 `verification.contract.json` 中标记 `UNSUPPORTED`，保持 `planned`，严禁退化写电源断言蒙混过关。 |
| **R-3 Wasm 多任务调度抖动导致点采样断言 Flaky** | 中 | 同一场景偶发失败（时间戳偏差几毫秒） | **引入时序容差窗口**：优先采用 `ASSERT_EVENT` 或带有 `within: [T_min, T_max]` 的区间断言，避免死板单点微秒采样。 |
| **R-4 故障注入未彻底复位污染后续测试** | 高 | 跑完故障用例后，后续正常用例大面积报红 | **强制 RAII 异常回滚**：脚本使用 `try...finally` 结构，只要发生异常或退出必须强制调用 `reset()`，若重置失败直接废弃整轮凭据。 |

---

## 六、 终验清单与交付工件 (Definition of Deliverables)

在宣布本战役全面完成前，必须核验以下交付工件均已落盘且验收通过：

1. **核心规则代码与测试**：
   - [ ] `.governance/gates/rules/g1_scenario_semantic_integrity.py`
   - [ ] `.governance/gates/tests/test_rules/test_g1_scenario_semantic_integrity.py`（单测 100% 通过）
2. **核验器与运行器加固**：
   - [ ] `.governance/gates/evidence_verifier.py`（拒绝空步骤、拒绝共享报告回退）
   - [ ] `wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1`（沙箱隔离与 Canary 支持）
3. **AI 行为规范手卡**：
   - [ ] `.agents/skills/governance-sop-esp/SKILL.md`（完整八步 SOP 与红线卡）
4. **黄金标杆实证实据**：
   - [ ] `protocols/http_client` 真实场景与 `run-report.json`（包含路由注入、状态码断言与变异杀伤证据）
5. **门禁看板与设计回写**：
   - [ ] `.governance/specs/PLAYBOOK.md` 规范回写完成
   - [ ] `python .governance/gates/run_gates.py --mode nightly` 零错误、零警告全绿通过
   - [ ] `CHECKLIST.md` 诚实单向重新渲染完成
