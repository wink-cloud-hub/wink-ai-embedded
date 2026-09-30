<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划 04：证据真实性、全自动 PR 门禁与防腐机器拦截实施计划

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260930-ESP-IDF-EVIDENCE-AND-PR-GATES-v1.0 |
| 状态 | 📋 **Draft / Planned（与 01~03 平行的横向基建守护计划）** |
| 日期 | 2026-09-30 |
| 周期估算 | 2.5~3 个工作日 |
| 核心目标 | **解除 CI 手动触发假象 → 闭环反向影响分析 (Fail-Closed) → 建立可信真机实证与只读 CI 校验 → 防腐多层机器门禁 → 异构多后端证据 Schema 迁移 → 多 SSOT 不变量动态自洽** |
| 决策依据 | [ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0043：分层门禁规范与 API 边界](../../../decisions/core/0043-layering-lint-rules.md)<br>[ADR-0091：多配置实例与五维正交 Schema 架构决策](../../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)<br>[ADR-0092：ESP-IDF 官方示例仿真治理前置筑基宪章](../../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md) |
| 管辖数据源 | [`.github/workflows/esp_idf_ci.yml`](../../../../.github/workflows/esp_idf_ci.yml)、[`.governance/gates/`](../../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/)、[`checklist.data.json`](../../../../wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json)、[`CLASSIFICATION-SPEC.md`](../../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md)、[`PLAYBOOK.md`](../../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/PLAYBOOK.md) |
| 实施目标文件 | `.github/workflows/esp_idf_ci.yml`、`.governance/gates/impact_scope.py`、`.governance/gates/rules/g1_can_check_mark.py`、`.governance/gates/rules/g4_impact_regression.py`、`.governance/gates/rules/g5_*.py`、`.governance/tools/*`、`run_esp32_headless_evidence.ps1` |
| 验收门禁 | 自动化 PR 触发拦截有效性验证、核心底层改动全局回归触发验证、未映射路径 Fail-Closed 阻断负测、六要素真实重构重演验证、防腐机器规则负测、多 SSOT 数字不变量零漂移 |

---

## 一、 计划背景与现存致命盲区

在 2026-09-30 的深度工程审计中，团队发现即使完成了 01～03 的 C 语言代码级加固，**现有的 CI/门禁系统依然存在严重的“形式主义伪闭环”与放行退化的漏洞**：

1. **影响分析存在严重漏网（False Negative 无需回归）**：
   - 现行 [`impact_scope.py:46-55`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/gates/impact_scope.py#L46-L55) 仅机械遍历能力目录的 `owned_paths`；
   - 经实测，核心复位与桥接调度代码 [`esp_idf_bridge.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/esp_idf_bridge.c) 与句柄分配核心 [`esp_sim_handle.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/esp_sim_handle.c) **根本未被任何能力声明拥有**；
   - 一旦开发者修改上述核心文件，算法计算出的受影响能力与示例均为 **0**，Gate 4 直接报告通过；
   - 遇到未在能力图谱登记的新文件时直接静默忽略，新示例源码、场景脚本、仿真资产以及将状态置为 `verified` 的变更，也缺乏强制重演触发机制。
2. **PR 根本未自动运行核心治理门禁，且存在凭据安全与路径缩减陷阱**：
   - [`.github/workflows/esp_idf_ci.yml:48, 67, 77, 89, 98`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/.github/workflows/esp_idf_ci.yml#L89) 将私有工具链检出、分层 lint 及 `run_gates.py` 均锁死在 `if: github.event_name == 'workflow_dispatch'` 下；
   - 若仅机械修改 lint/gates 步骤的 `if` 条件，会导致工具链未检出而直接报错崩溃；若向外部 Fork PR 开放 Secret 又会导致私有仓库凭据泄露；
   - 原工作流监听的 `&esp_paths` 涵盖了 `pal/**`、`targets/**`、`cmake/**` 等底层依赖，若缩减触发路径会导致核心底层变动时 CI 完全不被唤醒。
3. **“静态哈希匹配”不等于“真机实证真实”**：
   - 提交者可就地篡改资产、手动更新对应哈希并伪造包含 `"status": "passed"` 的 JSON 报告，静态比对将判定全绿；
   - 门禁在 CI 运行中必须遵循 [CLASSIFICATION-SPEC.md §1.2 门禁只读原则](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md#L60)，绝不能在 CI 校验中边查边写 `checklist.data.json`；
   - 针对 Wasm 三件套（wasm/js/device-tree）缺乏合成单一规范哈希的标准算法定义。
4. **现行 Schema 与实证手册对异构 Target 的结构性冲突**：
   - [CLASSIFICATION-SPEC.md:420-436](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md#L420-L436) 的 JSON Schema 强制所有执行配置必须具备 `assets_sha256` 和 `scenario_sha256`；
   - 物理硬件测试（`native_hardware`）和构建类测试（`build_system`）无法产出 Wasm 资产，导致非 Wasm 实证在 Schema 校验阶段被阻断；现行 PLAYBOOK 的五阶段工作流也机械绑定了 Wasm 构建。
5. **防腐规则不能仅靠狭隘正则，且存在头文件信任链断裂**：
   - 针对示例特化分支的简单正则无法拦截未带特定前缀的比较（如 `strcmp(app, "blink")`）；
   - “仅扫描新增 `.c` 文件”会彻底漏掉在既有驱动（如 `esp_gpio.c`、`esp_timer.c`）中偷加的 `static` 全局未复位状态；
   - [`.github/workflows/harvest-gate.yml:19-21`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/.github/workflows/harvest-gate.yml#L19-L21) 明确公开 CI 未传入私有 Harvester 规则，仅靠 `channels.json` 自锚无法证明头文件未被私自手写。
6. **计划自身的超时回滚悖论**：
   - 计划原文字中“超时则临时调小最大内联数”直接违反了“零未回归合入”的底线，会导致受影响示例被静默跳过。

---

## 二、 详细实施任务拆解 (Action Items)

### 阶段 1：可信 CI 流水线激活与 Required Status Check 锁定 (True CI Enforcement & Security)

- [ ] **任务 T1.1**：重构 [`.github/workflows/esp_idf_ci.yml`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/.github/workflows/esp_idf_ci.yml) 的触发矩阵与凭据安全模型：
  - **保留全量触发路径 (`paths: *esp_paths`)**：
    - 绝不缩减监听范围，必须完整包含 `frameworks/esp_idf/**`、`targets/**`、`pal/**`、`cmake/**`、`CMakeLists.txt`、`vendor/esp_idfv61/**` 等底层全部影响面；
  - **建立可信分支与 Fork PR 隔离机制**：
    - 针对内部分支 PR（`github.event.pull_request.head.repo.full_name == github.repository`）及主干 Push：自动执行私有工具链检出、分层 lint（`winkcli lint --pack layering --pack api`）与完整门禁（`run_gates.py --mode pr`）；
    - 针对外部 Fork PR：**Fail-Closed（快速失败阻断）**，严禁注入私有 Token；输出明确指引：“外部贡献者的 PR 需由仓库维护者在受信分支或专用 Runner 上复核验证后方可合并”；
    - 若私有工具链检出失败或环境变量缺失，步骤必须显式退出非 0，**严禁静默跳过**；
- [ ] **任务 T1.2**：GitHub 必需检查（Required Status Checks）刚性绑定：
  - 在 GitHub 仓库分支保护规则中，将以下检查声明为必须通过项（Required）：
    - `Run Layering / API / ESP-IDF Lint`
    - `Run ESP-IDF Gate System (PR mode)`
    - `Verify Harvested Headers and Asset Channels`
    - `Check SSOT Invariants`
  - 任何检查未绿灯，远端强制禁用 Merge 按钮。

---

### 阶段 2：规范化实证核验与只读 CI 校验引擎 (True Evidence Verifier & Read-Only CI)

- [ ] **任务 T2.1**：标准化 Wasm 三件套复合 SHA-256 规范化算法：
  - 规范定义 Wasm 三件套（`device-tree.json`, `wink_simulator.js`, `wink_simulator.wasm`）的哈希合成规则：
    $$\text{assets\_sha256} = \text{SHA256}\Big(\text{SHA256}(\text{wasm}) \,\|\, \text{"\n"} \,\|\, \text{SHA256}(\text{js}) \,\|\, \text{"\n"} \,\|\, \text{SHA256}(\text{tree})\Big)$$
  - 保证本地生成器（`run_esp32_headless_evidence.ps1`）与门禁校验器（`evidence_verifier.py`）采用 100% 相同的计算规范，杜绝各算各的。
- [ ] **任务 T2.2**：编写门禁与看板共用的判定器模块 `evidence_verifier.py`：
  - **静态一致性校验（Layer 1 Pre-Check）**：
    - 现场计算三件套复合 SHA-256，与 `evidence.assets_sha256` 逐字比对；
    - 现场计算声明的 `scenario.json` 的 SHA-256，与 `evidence.scenario_sha256` 逐字比对；
    - 读取真实 JSON 执行报告，结构化断言 `report.status == "passed"`，`report.summary.failed_steps == 0`，且报告绑定的 Commit / 时间戳自洽；
  - **严守门禁只读原则（CI Read-Only Enforcement）**：
    - 门禁脚本中严禁调用任何写入或修改 `checklist.data.json` 的逻辑；CI 环境下的运行必须是纯函数式验证。
- [ ] **任务 T2.3**：改造本地凭据生成工具 [`run_esp32_headless_evidence.ps1`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1)：
  - 仅作为**开发者本地使用的凭据生成工具**；
  - 增加 `-WriteEvidence` 标志：仅在本地显式声明该标志时，才在测试成功后自动计算复合哈希并规范回写 `checklist.data.json`；
  - 保证在 CI 执行时不带回写参数，杜绝流水线污染工作区。
- [ ] **任务 T2.4**：改造看板生成脚本 [`generate_checklist_v1_1.py`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/tools/generate_checklist_v1_1.py)：
  - 消除硬编码 `executions[0]` 缺陷，遍历所有声明配置调用 `evidence_verifier.py` 综合判定。

---

### 阶段 3：影响分析 Fail-Closed 补完与 Gate 4 真实无头回归 (Fail-Closed Impact Scope & Regression)

- [ ] **任务 T3.1**：重构影响分析算法 [`impact_scope.py`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/gates/impact_scope.py)：
  - **显式注册全局核心影响集（Global Impact Set）**：
    - 将 `esp_idf_bridge.c`、`esp_sim_handle.c`、`CMakeLists.txt`、`cmake/**`、`pal/**`、`targets/**` 等底层调度、复位与构建文件显式定义为 `GLOBAL_IMPACT_PATHS`；
    - 只要命中上述任一路径，算法直接将**所有处于 `verified` 状态的活跃示例全部列入必回归集合**；
  - **未映射未知路径阻断（Fail-Closed on Unknown Paths）**：
    - 遍历 PR 改动中位于 `frameworks/esp_idf/**` 与 `vendor/esp_idfv61/**` 的全部代码文件；
    - 若发现任何未在 `capability-catalog.yaml` 中映射、且不在全局核心集或忽略名单中的路径，立即触发 `FAIL_ON_UNKNOWN_PATH` 错误，门禁退出非 0 并强制要求开发者在目录中补齐映射关系，**绝对严禁返回空集**；
  - **数据、场景与状态变更强制触发回归**：
    - 若 PR 中修改了微应用的源码、`wink-app.json`、`scenario.json`、`unisim-assets/`，或在 `checklist.data.json` 中将条目状态从 `planned` 改为 `verified`，该配置实例必须强制加入受影响测试集执行首次真实验证。
- [ ] **任务 T3.2**：重构 [`g4_impact_regression.py`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/gates/rules/g4_impact_regression.py)：
  - 对计算出的受影响 `verified` 配置，在干净工作区调用 Headless 运行器真实执行微秒仿真；
  - 任何一个断言点失败直接抛出 `FATAL_ERROR` 阻断合并；
  - **废除超时调小回归数的降级设计**：若受影响用例较多，CI 采用分批并发矩阵（Matrix Jobs）分摊耗时；若出现超时或超限，必需检查直接失败挂起，**绝不放行未经回归的代码**。

---

### 阶段 4：防腐多层纵深防御与机器规则落地 (Gate 5 Multi-Layered Anti-Decay)

- [ ] **任务 T4.1**：在 `.governance/gates/rules/` 中落地全方位防腐机器检查规则：
  - **`g5.no_app_specific_branch`（彻底杜绝 App 特化分支）**：
    - 不仅使用正则，结合 AST/词法扫描 `frameworks/esp_idf/src/**` 中新增或修改的代码；
    - 严禁出现对应用标识、目录名字串的比较（如 `strcmp(..., "blink")`、`strstr(..., "esp_idfv61_")`、`CONFIG_APP_*` 自定义应用宏等）；
  - **`g5.no_raw_delay_tasks`（严禁临时延时纤程）**：
    - 检查门面 C 代码中新增的 `xTaskCreate` 调用；
    - 严禁在驱动或协议栈中创建仅为延时回调的 ad-hoc 任务，强制使用带代际 Token 的定时器工作项；
  - **`g5.reset_registration_verified`（全量改动代码深度静态状态扫描）**：
    - 扫描 `frameworks/esp_idf/src/**` 中**所有新增及被修改的 `.c` 文件**；
    - 检查本次改动是否新增了文件作用域的 `static` 变量、全局对象池或状态机；
    - 若新增了状态，强制断言其重置函数必须在 `esp_idf_bridge.c` 的 `pal_wasm_target_clear_pending_reset` 复位链条中被显式调用；
  - **`g5.header_trust_and_contract_registry`（公开头文件来源与契约审查）**：
    - 任何新增的公开头文件必须在 `channels.json` 中登记且满足搬迁与自锚规则；
    - 在 PR 模板与门禁审查中增加强制项：必须登记原厂契约来源、内存/句柄资源预算、Fail-Loud 错误语义与复位钩子实现，并由架构负责人签字。
- [ ] **任务 T4.2**：编写防腐规则端到端正反双向单元测试（Pytest Suites）：
  - 构造合规代码测试，断言门禁绿灯；
  - 注入真实负例（未映射核心文件、篡改资产但伪造报告、`strcmp(app, "blink")`、在既有文件偷加 static 变量未注销复位、私自登记未收割头文件），断言门禁 100% 精准拦截报错。

---

### 阶段 5：异构后端证据 Schema 迁移与 PLAYBOOK 流程解耦 (Heterogeneous Evidence Schema & Playbook)

- [ ] **任务 T5.1**：升级 [`CLASSIFICATION-SPEC.md`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md) 中的 JSON Schema：
  - 将 `evidence` 改造为支持多后端的版本化多态模型（Polymorphic Evidence Schema）：
    ```json
    "evidence": {
      "type": ["object", "null"],
      "oneOf": [
        {
          "properties": {
            "backend": { "const": "wasm_simulation" },
            "run_id": { "type": "string" },
            "assets_sha256": { "type": "string", "pattern": "^[a-f0-9]{64}$" },
            "scenario_sha256": { "type": "string", "pattern": "^[a-f0-9]{64}$" },
            "execution_report_ref": { "type": "string" }
          },
          "required": ["backend", "run_id", "assets_sha256", "scenario_sha256", "execution_report_ref"]
        },
        {
          "properties": {
            "backend": { "const": "esp32_hardware" },
            "run_id": { "type": "string" },
            "firmware_elf_sha256": { "type": "string", "pattern": "^[a-f0-9]{64}$" },
            "serial_log_report_ref": { "type": "string" },
            "board_type": { "type": "string" }
          },
          "required": ["backend", "run_id", "firmware_elf_sha256", "serial_log_report_ref", "board_type"]
        },
        {
          "properties": {
            "backend": { "const": "build_system" },
            "run_id": { "type": "string" },
            "build_log_ref": { "type": "string" },
            "compiler_version": { "type": "string" }
          },
          "required": ["backend", "run_id", "build_log_ref", "compiler_version"]
        }
      ]
    }
    ```
- [ ] **任务 T5.2**：重构 [`PLAYBOOK.md`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/specs/PLAYBOOK.md) 工作流章节：
  - 将五阶段流水线解耦为通用阶段与按 Target 分支的实证阶段；
  - 增加物理硬件（ESP32）串口断言流程与纯构建校验流程指引；
  - 彻底明确“本地生成凭据 (`-WriteEvidence`) vs CI 只读校验”的操作边界。

---

### 阶段 6：数据源与看板不变量动态自洽门禁 (SSOT Invariant Gate)

- [ ] **任务 T6.1**：修正 [`checklist.data.json`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json) 历史数据：
  - 彻底修正写死的 `audited: 10, verified_configs: 10` 脏数据；
  - 严格统一统计口径：
    - `total_entries`: 478
    - `in_scope`: 312 项
      - `active`: 291 项（1 项 `verified`，290 项 `planned`）
      - `deferred`: 21 项（明确暂缓，但属于规划范围）
    - `out_of_scope`: 166 项（明确硬件排除，Fail-Loud 阻断）
- [ ] **任务 T6.2**：编写数据源与看板不变量校验门禁（`check_ssot_invariants.py`）：
  - 动态计算并断言：条目明细之和与头部 `summary` 100% 逐字相等；
  - 断言生成的 `CHECKLIST.md` 渲染数值与实时数据 100% 吻合；
  - 挂载至 CI 必选检查步骤，杜绝任何人工篡改或口径漂移。

---

## 三、 风险评估与应对方案 (Risk & Rollback Matrix)

| 风险项 | 触发场景 | 预防与解决措施 | 绝对禁止项 (Prohibited) |
|---|---|---|---|
| **R-01 Gate 4 真实回归耗时增长** | 底层核心改动触发较多已验证条目回归 | 采用 GitHub Actions Matrix 并行 Runner 分批分片跑回归；优化 Headless 启动开销 | **严禁调小内联回归数来跳过测试；未回归绝不放行** |
| **R-02 外部贡献者 Fork PR 缺乏私有 Token** | 外部开源开发者提交 PR | CI 执行 Fail-Closed 阻断并友好提示：由 Maintainer 审查后推送到受信分支跑完整门禁 | **严禁向外部 Fork 暴露含有私有读权限的 Token** |
| **R-03 核心底层改动未映射** | 开发者修改了未映射的门面或构建代码 | `impact_scope.py` 实施 Fail-Closed：未映射直接报错阻断，要求登记映射或列入全局影响集 | **严禁在未映射时返回 0 受影响项并静默 PASS** |
| **R-04 静态变量在既有文件中偷加** | 开发者在已有驱动文件中增加全局状态而漏掉复位 | AST/语法扫描针对全部修改文件的新增 static 符号进行复位注册检查 | **严禁仅检查“新增的 .c 文件”** |

---

## 四、 全局验收标准 (Definition of Done)

1. **DoD-1（真实 CI 阻断有效性）**：
   - PR 提交时全量触发（涵盖 PAL、targets、CMake 等依赖路径），分层 lint 与 gates 作为必需检查生效；外部 PR 严密保护私有凭据并安全阻断；
2. **DoD-2（影响分析零漏网验证）**：
   - 负测证明：修改 `esp_idf_bridge.c` 或 `esp_sim_handle.c` 时，已交付的 `#001: blink_gpio` 100% 被纳入回归集合；修改未映射路径时门禁 100% 报错拦截；
3. **DoD-3（实证核验纯真机与只读性）**：
   - 伪造资产/报告负测被 100% 拦截；CI 运行全过程保持纯只读，本地工具通过标准复合哈希算法回写凭据；
4. **DoD-4（防腐四铁律纵深拦截）**：
   - Gate 5 系列规则对特化分支（包含 `strcmp(app, "blink")`）、临时延时纤程、既有文件中偷加未复位 static 变量、未登记头文件实现 100% 拦截并拥有完整单测；
5. **DoD-5（异构证据与 SSOT 不变量自洽）**：
   - Schema 成功支持异构 backend 凭据表达，`checklist.data.json` 消除历史脏数据，478 = 312 In-Scope (291 Active + 21 Deferred) + 166 Out-of-Scope 不变量在 CI 中全绿。
