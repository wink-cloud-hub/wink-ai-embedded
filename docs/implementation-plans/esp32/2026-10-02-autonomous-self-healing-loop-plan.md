<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：ESP-IDF 治理 Loop 底座自愈与双 Agent 对抗审查闭环体系 (v2.3 终极精细版)

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261002-ESP-IDF-AUTONOMOUS-SELF-HEALING-LOOP-v2.3 |
| 状态 | **Ready for Execution** |
| 日期 | 2026-10-02 |
| 目标平台/SoC | `wasm32-unknown-emscripten` / `esp32 xtensa` 双 Target |
| 工具链/SDK版本| `ESP-IDF v6.1-dev` / `Emscripten 3.1.x` / Node.js 24+ / Python 3.11+ / pytest |
| 计划版本 | `v2.3`（工业实战终极精细版：增补**执行后客观闭环复盘与补遗门禁 (Post-Execution Completeness Audit)**，构建 13 阶段完整物理状态机，彻底破除大模型“满足即停 (Premature Closure)”与粗心遗漏缺陷；受控 PAL 纯增量演进分级门禁，统一补丁工件 `patch.diff`，零依赖分级 C 解析器，双盲对抗审查与双轨事务回滚） |
| 优先次序 | **P0 机器防腐分级硬核与 13 状态持久化 ➔ P0 双盲 Agent 对抗审查与补丁工件化 ➔ P0 执行后客观复盘与补遗门禁 ➔ P0 受控 PAL 纯增量演进与事务回滚 ➔ P1 流水线挂载与 CLI 调度 ➔ P2 标杆示例（mqtt_tcp / i2c_basic）实证闭环** |
| 决策与设计依据 | [ADR-0001：负数错误码标准](../../decisions/core/0001-error-code-sign-convention.md)<br>[ADR-0002：双 Target 同源编译](../../decisions/core/0002-dual-target-compilation.md)<br>[ADR-0004：编译期静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0083 / ADR-0084：开源许可分层地图](../../decisions/core/0083-open-source-license-boundary.md)<br>[ADR-0091：多配置实例与五维正交 Schema](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)<br>[ADR-0092：ESP-IDF 官方示例仿真治理前置筑基宪章（Tier 1 基础外设下沉 PAL 规范）](../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md)<br>[PLAN-20261001-ESP-IDF-HEADLESS-ANTI-FALSE-GREEN](./2026-10-01-anti-false-green-verification-plan.md) |
| 管辖数据源 | `checklist.data.json`、`wink-micro-app/vendor/esp_idfv61/.governance/investigations/`、`wink-micro-os/frameworks/esp_idf/`、`wink-micro-os/pal/`、`packages/unisim/` |
| 实施目标文件 | `.governance/tools/loop/remediator.py`（新增）、`.governance/tools/loop/safety_checker.py`（新增，独立分级防腐与 PAL 纯增量白名单规则模块）、`.governance/tools/loop/agent.py`（扩充双盲、执行后自查与补丁生成）、`.governance/tools/loop/pipeline.py`（扩充挂载自愈）、`.governance/tools/loop/runner.py`（扩充双 Agent CLI 调度）、`.governance/gates/tests/test_loop_remediator.py`（新增离线单测套件）、`.agents/skills/governance-sop-esp/SKILL.md`（同步更新） |
| 验收门禁 | `pytest .governance/gates/tests/test_loop_remediator.py`（100% 离线通过）、`pytest .governance/gates/tests/test_loop_runner.py`、`python .github/scripts/check_license_map.py`、`python .governance/gates/run_gates.py --gate 1`、零回归黄金套件全绿 |

---

## 一、 战略总目标与设计哲学

### 1.1 战役背景与深层痛点
在 `PLAN-20261001` 战役完成后，治理 Loop 已经实现了“场景自主编写、Gate 1 语义拦截、Canary 变异击杀、凭据自动签署”的防假绿闭环。然而，当前系统在面对“需要修补底座框架”的场景时，暴露出四个深层次瓶颈：
1. **单向止损造成治理吞吐阻塞**：
   - 当官方示例因底层缺乏协议桩（如 `mqtt_tcp` 缺乏 MQTT Broker 模拟器）、缺乏外设硬件影子模型时，现有 Loop 会在 Phase 3 基线阶段直接报错并执行 `rollback_app` 干净回滚；
   - 缺少框架能力的条目只能永久挂在 `planned` 状态，必须依赖人工排查，无法形成自动化自治推进。
2. **底层 PAL 能力盲区与机械封锁的矛盾（关键破局点）**：
   - 经实测审计，当前 `wink-micro-os/pal/include/hal/` 仅有 11 个基础外设（ADC, DMA, GPIO, HWTimer, I2C, MCPWM, PCNT, PWM, RMT, SPI, UART）；
   - 在面对 478 个官方用例中涉及的 DAC、TouchPad、Temperature Sensor、SDMMC、看门狗 WDT、RTC PM 等外设时，底层 PAL 处于完全空白态；
   - **破局方案**：必须区分“严禁破坏性修改内核 ABI”与“允许通用外设受控纯增量扩展”，打通平台进化通道。
3. **Agent 自我共谋与架构腐化风险（核心防范对象）**：
   - 若草率允许 Agent 在基线失败时自由改动底座，Agent 会出于“通过测试”的贪婪目标走向投机：空桩违规、特判硬编码、私降编译参数、破坏既有资产、引入厂商专有头文件污染。
4. **大模型“满足即停（Premature Closure）”与粗心盲区**：
   - Agent 在第一次实施方案时，往往在实现主要成功路径后便急于结束，容易遗漏原方案承诺的辅助逻辑（如异常分支释放、头文件函数导出、Wasm 与 ESP32 双向闭环）；
   - 若缺乏执行后的客观自查追问，这些遗漏将直接撞上后续耗时的编译或回归测试导致整体回滚。

### 1.2 核心设计哲学（八大钢铁原则）
1. **归因先行与自愈权限分级裁定（Tri-Layer Triage Matrix with PAL Controlled Evolution）**：
   - 严禁盲修代码。必须先有结构化根因分析报告，明确定位缺陷层级；
   - 明确自主修复边界：允许修补应用级配置、协议层 Mock 桩，以及 **PAL 外设 HAL 层的纯增量演进**（严禁破坏性修改既有 ABI 或注入厂商专有结构体；严禁修改 OSAL 调度核心与 UniSim 核心调度器）。
2. **双 Agent 双盲对抗制衡（Double-Blind Adversarial Review）**：
   - Agent A（出案者，默认 `qoderclicn`）负责根因排查与技术方案编制；
   - Agent B（独立裁判，默认 `agy`）依据 8 大反模式红线执行无情找茬与独立审查，出具结构化评审报告；
   - **双盲隔离**：输入给 Agent B 的 Prompt 中剥离 Agent A 的内部思考链（CoT），防止权威认同与思维锚定；
   - **跨平台洁癖审计**：若涉及 PAL 变更，Agent B 必须严格审查其是否具备跨平台（8051/STM32/ESP32）通用性，严禁为 ESP32 私设定制特判；
   - Agent A 必须将审查建议**深度融合吸收**到方案正文中（严禁简单尾部追加），形成最终执行方案。
3. **补丁工件化与事前硬核安全拦截（Patch as an Artifact）**：
   - 严禁 Agent 自由乱改代码库。方案最终交付统一格式的 `patch.diff`；
   - 在任何物理文件被修改前，由 `HeuristicSafetyChecker` 先行扫描 `patch.diff`，出现空桩、特判、PAL 私有污染或路径越权一票否决；
   - 执行 `git apply --check patch.diff` 验证无冲突后方允许物理合入。
4. **分级零依赖机器防腐防线（Tiered Heuristic Defense）**：
   - 不单依赖 LLM 裁判自觉性，系统使用内置零依赖纯 Python Tokenizer 与语句扫描器（可无缝升级 Tree-sitter AST）直接查杀空桩、特判和越权修改，确保在任何标准 Python 3.11 环境 100% 稳固运行。
5. **执行后客观闭环复盘与补遗门禁（Post-Execution Objective Audit）**：
   - 在初版补丁生成后、物理编译前，强制插入一次单次结构化追问复盘；
   - 严禁泛化的哲学提问，必须**对照原方案承诺的 DoD 逐项画勾审计**；
   - 采用二值化判词（`FULLY_COMPLETE` vs `GAPS_FOUND`），发现遗漏产出增量补遗补丁，无遗漏直接放行，单次收敛，严禁过度设计与画蛇添足。
6. **分级零回归门禁（Tiered Zero-Regression Gate）**：
   - 修复底座或扩展 PAL 后，自动分级触发**零回归验证**：先跑同领域用例（L1 快检），再重跑全部已交付黄金用例（L2 全量）；历史资产退化一票否决。
7. **双轨事务回滚与开发现场保护（Transactional Dual-Track Rollback）**：
   - 前置 Pre-flight 检查目标文件是否存在用户未暂存修改；发现脏文件立即终止自愈；
   - 回滚时区分“已跟踪修改文件（`git checkout --`）”与“自愈新增文件（`unlink` 彻底删除）”，严禁粗暴 `git checkout .` 误伤用户现场代码。
8. **动态行为终极质检（Dynamic Behavioral Verification via Canary）**：
   - Phase 4 Canary 变异击杀是检验自愈补丁（如协议 Mock、PAL 新外设响应桩）真实性的试金石：若 Agent 伪造无状态死桩，变异场景将无法被击杀（触发 False-Green 拦截），迫使自愈产出真实的状态机响应。

---

## 二、 架构规格与接口契约

### 2.1 目录组织与排查资产规范

针对每个触发自愈的 checklist 条目，统一在 `.governance/investigations/<entry_id>/` 下建立不可变证据链目录：

```text
wink-micro-app/vendor/esp_idfv61/.governance/investigations/<entry_id>/
├── session_state.json          # 覆盖 13 状态的持久化快照 (支持断点恢复与代数追踪)
├── raw_failure.log             # 触发自愈的初始报错原始日志
├── 01-ROOT-CAUSE-ANALYSIS.md   # 现象、调用栈、因果链断点、归因三维定位
├── 02-REMEDIATION-PLAN.md      # 技术方案、防腐红线、改动范围、验收标准 (融合版)
├── 03-ADVERSARIAL-REVIEW.md    # 独立裁判出具的对抗找茬记录 (双盲审查)
├── patch.diff                  # 关键：方案输出的 Unified Diff 补丁工件 (物理写入唯一凭据)
└── regression_run.log          # L1/L2 回归测试完整执行日志
```

---

### 2.2 归因三级判定矩阵与物理写入白名单（Safe-Write Scope）

#### 1. 归因矩阵与自愈权限
在 `01-ROOT-CAUSE-ANALYSIS.md` 中，Agent 必须明确归因并对照自愈权限：

| 归因层级 | 缺陷范畴 | 自主修复权限 | 处理策略与约束 |
| :--- | :--- | :---: | :--- |
| **Layer C (App 配置层)** | `wink-app.json` 引脚未声明、`sdkconfig.h` 宏开关缺失、场景超时太短 | ✅ **允许自主修复** | 由 Agent 修改应用目录内文件，不影响其他工程。 |
| **Layer A (C 框架外设/协议层)** | `frameworks/esp_idf` 缺少协议 Mock（如 MQTT Responder）、外设从机响应数据模型 | ✅ **受限自主修复** | 仅允许按标准协议行为补充状态机桩（严禁空桩），必须跑分级零回归。 |
| **Layer B1 (UniSim 协议通道层)** | `packages/unisim` 缺少某种网络 Fixture 解析、总线数据流封包格式不对 | ⚠️ **需人类确认后修复** | Agent 生成修复提案后触发熔断，标记 `[BLOCKED_UNISIM_CHANGE_HUMAN_REVIEW]`，等待人类架构师审批后执行。UniSim 为外仓黑盒，严禁 Agent 自主修改内部实现。 |
| **Layer Core-B (PAL 外设受控增量演进)** | 缺少基础通用外设（如 DAC, WDT, Touch, SDMMC 等）、已有外设新增纯增量 API | ✅ **受限纯增量演进 (Additive Only)** | 必须严格遵守**四大钢铁纪律**（纯增量、跨平台纯度、三位一体同源提交、过门禁），严禁修改既有 ABI 或引入厂商专有头文件。 |
| **Layer Core-A (OSAL/内核调度/不可变核心)** | `pal_osal.h` 线程/互斥锁/时钟源核心修改、虚拟时间调度器调度算法变动、既有 ABI 函数签名破坏 | 🛑 **绝对禁止自主修复** | **立即触发熔断**：标记 `[BLOCKED_ARCH_CHANGE_ESCALATION]`，转交人类架构师。 |

#### 2. PAL 纯增量演进四大钢铁纪律（Additive-Only Contract）
1. **纯增量原则（Additive Only）**：
   - 允许新增外设抽象头文件（如 `wink-micro-os/pal/include/hal/pal_dac.h`、`pal_wdt.h`）；
   - 允许在现有 HAL 头文件末尾追加新通用 API 与新枚举值；
   - **绝对禁止修改现有 API 的函数入参、返回值或改动既有结构体字段**（破坏 ABI 兼容性）。
2. **跨平台洁癖门禁（Vendor Neutrality - ADR-0092 Tier 1）**：
   - PAL 头文件严禁出现任何 `esp_*.h`、`freertos/*.h` 或芯片寄存器头文件；
   - 类型必须基于标准 C（`uint8_t`, `size_t` 等）或 Wink 基础类型（`wink_status_t`、`pal_pin_t`）；
   - 函数返回值严格遵守 [ADR-0001](0001-error-code-sign-convention.md)（`0 = 成功，负数 = 错误`）。
3. **三位一体同源闭环（Trinity Implementation）**：
   - Agent 如果在 `pal/include/hal/` 声明了新外设（如 `pal_dac.h`），**必须同时在当前补丁中提交其 Wasm 仿真桩与 ESP32 适配**：
     - `wink-micro-os/pal/include/hal/pal_dac.h`（通用契约）
     - `wink-micro-os/targets/wasm/hal/pal_dac_wasm.c`（仿真行为模型）
     - `wink-micro-os/targets/esp32/hal/pal_dac_esp32.c`（物理芯片驱动）
   - 缺一不可！不能只写头文件扔给编译期报 `undefined reference`。
4. **通过 Gate 2 防膨胀与 Gate 3 分层门禁**：
   - 必须通过 `winkcli lint --pack layering --pack api` 自动化机器扫描。

#### 3. 物理写入安全白名单与黑名单（Safe-Write Whitelist & Blacklist）
```python
# 严格白名单 (修改范围必须 100% 匹配以下前缀)
SAFE_WRITE_WHITELIST = [
    "wink-micro-app/vendor/esp_idfv61/",         # 应用目录自身及其场景
    "wink-micro-os/frameworks/esp_idf/src/",     # C 框架 Shim 及网络/外设响应桩
    "wink-micro-os/frameworks/esp_idf/include/", # C 框架 Shim 头文件
    "wink-micro-os/pal/include/hal/",            # PAL 通用基础外设抽象 (仅限纯增量扩展)
    "wink-micro-os/targets/wasm/",               # Wasm 目标同源仿真桩实现
    "wink-micro-os/targets/esp32/",              # ESP32 目标同源物理驱动适配
]

# 严格黑名单 (出现任何一项立即一票否决并熔断)
STRICT_BLOCKED_PATHS = [
    "wink-micro-os/pal/include/osal/",           # OSAL 操作系统抽象核心 (禁止自愈改动)
    "wink-micro-os/pal/include/internal/",       # PAL 内部核心
    "wink-micro-os/pal/src/osal/",               # OSAL 实现核心
    "packages/unisim/",                          # UniSim 仿真内核 (外仓黑盒)
    ".governance/gates/",                        # 门禁规则自身
    ".github/",                                  # CI/CD 自动化流水线
    "CMakeLists.txt",                            # 根目录与顶层构建配置
]
```

---

### 2.3 机器防腐 8 大硬核过滤准则（Heuristic Anti-Corruption Rules）

在 `patch.diff` 物理合入前，系统通过 `HeuristicSafetyChecker` 执行物理级代码扫描，出现以下任何特征一律硬性拦截并判负：

- **H-1（严禁空桩直接放行）**：任何函数体仅包含 `return ESP_OK;`、`return 0;`、空宏或打日志后立即返回成功（由 Tier 1 Tokenizer / AST 引擎深度判定有效语句 $\le 1$）；
- **H-2（严禁应用名称特判）**：在底座 C 框架中出现 `strstr(..., "<app_name>")` 或对特定工程目录的硬编码分支；
- **H-3（严禁变异测试旁路）**：在场景或代码中企图绕过 Canary 变异击杀机制；
- **H-4（严禁私降编译告警标准）**：修改 `CMakeLists.txt` 注释掉 `-Werror`、`-Wall` 或删除安全门禁配置；
- **H-5（严禁破坏既有 PAL ABI 与引入厂商污染）**：严禁修改已存在的 PAL 函数签名、入参、返回值或既有结构体字段；严禁在 `pal/include/` 头文件中引入任何 `esp_*.h`、`freertos/*.h` 或厂商私有类型；允许纯增量新增通用外设抽象（`pal/include/hal/pal_*.h`）与同源 target 适配实现；必须遵守 [ADR-0001](0001-error-code-sign-convention.md) 负数错误码与 [ADR-0004](0004-static-dispatch-vs-runtime-ops.md) 编译期静态分发规范；
- **H-6（严禁浮点占空比反模式）**：新增 PWM 逻辑必须使用 `pal_pwm_set_duty_bp()`，严禁注入浮点 API（ADR-0066）；
- **H-7（严禁许可证污染）**：新增 C 文件必须带有合法 SPDX 标识，且符合运行时 `LGPL-3.0-only`、工具 `GPL-3.0-only` 分层地图（ADR-0083）；
- **H-8（严禁越界修改受控路径）**：补丁触及的文件路径必须完全属于 `SAFE_WRITE_WHITELIST`，严禁触碰 `STRICT_BLOCKED_PATHS`。

---

### 2.4 双 Agent 对抗评审契约与吸收准则

#### 1. 双盲审查机制与审判 Rubric
Agent B 提示词必须注入强力“找茬与挑刺”角色，且**严格剥离 Agent A 的推理链**，输出必须包含以下 YAML Frontmatter 及结构化内容：

```markdown
---
verdict: APPROVED | REVISE_REQUIRED | REJECTED
he_checks_passed: true | false
blocking_issues_count: 0
---

## 对抗审查详细评估

### 1. 架构防腐红线审查
- 是否存在伪造成功/空桩倾向: [无 / 发现: 详述]
- 是否存在特定工程特判硬编码: [无 / 发现: 详述]
- 变异是否保持可杀伤性 (A-4 原则): [具备 / 丧失: 详述]

### 2. 缺陷归因与最小爆炸半径
- 归因层级是否准确 (未错怪底座或配置): [准确 / 偏差: 详述]
- 修改范围是否收敛在最小必要文件 (符合安全白名单): [是 / 范围过宽: 详述]

### 3. PAL 跨平台纯洁度与通用性审计 (Vendor-Neutrality Audit)
- 是否触及 PAL/HAL 修改: [否 / 是: 详述]
- (若触及) 是否为严格纯增量扩展 (未改动既有 ABI 签名): [是 / 否: 详述]
- (若触及) 是否保持跨平台通用性 (无 esp_*.h 或 ESP32 私有结构体，8051/STM32 能共享): [是 / 否: 详述]
- (若触及) 是否满足三位一体同源提交 (头文件 + wasm 桩 + esp32 驱动齐全): [是 / 否: 详述]

### 4. 阻断性缺陷清单 (Blocking Issues)
1. ...

### 5. 改进吸收建议 (Actionable Recommendations)
1. ...
```

#### 2. 单 CLI 环境降级对策（Single-CLI Fallback）
- 当系统仅检测到一个 Agent CLI 时（例如只配置了 `--agent-cmd` 或仅有 `claude`/`agy`）：
  - 强制使用无状态无上下文的独立子进程分别执行 Agent A 与 Agent B；
  - Agent B 启动时自动注入专门的红队强对抗系统提示词，提高审判严苛度，杜绝自审共谋。

#### 3. 深度吸收与正文重构（Deep Synthesis Protocol）
- **判定为 `REJECTED`**：自愈失败，方案直接被毙，不进入代码执行阶段；
- **判定为 `REVISE_REQUIRED` 或带建议的 `APPROVED`**：
  - 调度 Agent A 启动**深度融合重构**；
  - Agent A 必须在 `02-REMEDIATION-PLAN.md` 的正文中修改具体的技术实现与代码草案，并在文末增加 `## 4. 评审建议融合记录（Synthesis Log）`，逐条对照 Blocking Issues 和 Recommendations 写明“已采纳并重构了 X 模块”；
  - 重新导出更新后的 `patch.diff`；
  - **脚本核验机制**：系统检查 `02-REMEDIATION-PLAN.md` 的版本变更 diff，若检测到仅在末尾做了文本追加而正文未变，判定融合无效打回重做。

---

### 2.5 执行后完整性自查与补遗契约（Post-Execution Audit Protocol）

为了根治大模型在初次写完代码后的“满足即停（Premature Closure）”现象，在 Agent A 完成方案重构与初版 `patch.diff` 生成后，系统强制插入一次**基于原方案承诺 DoD 的结构化追问复盘**。

#### 1. 结构化审计自查 Prompt 规范
```markdown
[TASK] 自愈实施后完整性自查与 DoD 对照审计 (Role A: Post-Execution Audit)

你刚刚已经完成了 02-REMEDIATION-PLAN.md 的物理代码初版实施与 patch.diff 导出。
现在请对照【原方案承诺的验收标准 (DoD)】与【实际生成的 patch.diff】进行严密的客观对比自查：

【强制自查四项清单】:
1. 方案覆盖度对照：02-REMEDIATION-PLAN.md 中承诺的每一个技术要点，在 patch.diff 中是否均有物理代码落地？是否存在遗漏？
2. 三位一体与接口完整性：新增/修改的 C 接口是否在对应头文件中正确导出？Wasm 仿真端与 ESP32 物理端是否双向闭环？
3. 边界与防御完整性：超时、缓冲区溢出、空指针及异常返回路径是否均有正确处理，是否存在未完成的 TODO？
4. 机器合规洁癖：是否存在任何未删掉的测试硬编码、浮点 PWM 或临时日志？

【二值化判决输出规范 (严格防过度设计与反向画蛇添足)】:
你必须输出以下两者之一，严禁无病呻吟发散：
- 情况 A (确实完全交付，无任何遗漏):
  输出:
  VERDICT: FULLY_COMPLETE
  理由简述（列出各项 DoD 已 100% 满足的事实证据）。
  （此时严禁添加任何新特性或重构！）

- 情况 B (发现确凿遗漏):
  输出:
  VERDICT: GAPS_FOUND
  明确列出缺失的要点（例如：漏掉了头文件导出或超时重试分支）。
  并直接给出增量补遗补丁：
  ```diff
  ... 增量修复 diff ...
  ```
```

#### 2. 补遗控制与单次收敛约束
1. **单次收敛铁律（One-Shot Bounded）**：执行后追问**严格限定为 1 轮**，绝不允许反复循环套娃追问，彻底规避大模型因过度自省而产生幻觉；
2. **零改动快速放行**：若返回 `FULLY_COMPLETE`，直接推进到 `HEURISTIC_PRECHECK`；
3. **增量合并（Patch Concatenation）**：若返回 `GAPS_FOUND`，系统提取增量 diff 追加合并至 `patch.diff`，迁移至 `PATCH_SUPPLEMENTING`，随后一并提交机器防腐硬核检验。

---

## 三、 核心模块详细设计与类接口

### 3.1 覆盖物理执行的 13 阶段状态机与断点恢复矩阵

#### 1. 完整状态流转图
```
[INIT] ──> [RCA_AUTHORED] ──> [PLAN_AUTHORED] ──> [ADVERSARIAL_REVIEWING]
                                                         │
                        ┌────────────────────────────────┴──────────────────┐
                        ▼                                                   ▼
                [REVISE_REQUIRED] (融合重构)                           [REJECTED] ──> [ROLLED_BACK]
                        │                                                   ▲
                        ▼                                                   │ (任一环节失败)
                [PLAN_SYNTHESIZED]                                          │
                        │                                                   │
                        ▼                                                   │
             [POST_EXEC_AUDITING] (二次追问客观复盘)                         │
                        │                                                   │
       ┌────────────────┴────────────────┐                                  │
       ▼ (GAPS_FOUND)                    ▼ (FULLY_COMPLETE)                 │
[PATCH_SUPPLEMENTING]                    │ (无遗漏直接放行)                 │
       │ (合并增量补丁)                   │                                  │
       └────────────────┬────────────────┘                                  │
                        ▼                                                   │
               [HEURISTIC_PRECHECK] ────────────────────────────────────────┤
                        │ (通过)                                            │
                        ▼                                                   │
                [PATCH_APPLYING] ──> [PATCH_APPLIED] ───────────────────────┤
                                            │                               │
                                            ▼                               │
                                   [DUAL_TARGET_COMPILING] ─────────────────┤
                                            │ (编译通过)                    │
                                            ▼                               │
                                     [L1_REGRESSION] ───────────────────────┤
                                            │ (同领域通过)                  │
                                            ▼                               │
                                     [L2_REGRESSION] ───────────────────────┤
                                            │ (全量黄金通过)                │
                                            ▼                               │
                                      [CANARY_KILL] ────────────────────────┘
                                            │ (击杀成功)
                                            ▼
                                       [COMPLETED] ──> 流水线 Phase 5 签署凭据
```

#### 2. 持久化快照 Schema (`session_state.json`)
```json
{
  "schema_version": 2,
  "app_id": "esp.peripherals.dac",
  "attempt": 1,
  "max_attempts": 2,
  "current_state": "POST_EXEC_AUDITING",
  "history": [
    { "state": "INIT", "timestamp": "2026-10-02T08:15:00Z" },
    { "state": "RCA_AUTHORED", "timestamp": "2026-10-02T08:15:20Z" },
    { "state": "PLAN_AUTHORED", "timestamp": "2026-10-02T08:15:45Z" },
    { "state": "REVIEW_DONE", "verdict": "REVISE_REQUIRED", "timestamp": "2026-10-02T08:16:10Z" },
    { "state": "PLAN_SYNTHESIZED", "timestamp": "2026-10-02T08:16:35Z" },
    { "state": "POST_EXEC_AUDITING", "verdict": "FULLY_COMPLETE", "timestamp": "2026-10-02T08:16:45Z" }
  ],
  "modified_files": [
    "wink-micro-app/vendor/esp_idfv61/peripherals/dac/dac_continuous/wink-app.json"
  ],
  "created_files": [
    "wink-micro-os/pal/include/hal/pal_dac.h",
    "wink-micro-os/targets/wasm/hal/pal_dac_wasm.c",
    "wink-micro-os/targets/esp32/hal/pal_dac_esp32.c"
  ]
}
```

#### 3. 中断恢复与清理矩阵（Interruption Recovery Matrix）
| 中断时状态 (`current_state`) | 磁盘物理状态 | 恢复策略 |
|---|---|---|
| `INIT` ~ `PLAN_SYNTHESIZED` | 物理代码未动 | 直接载入已有 md 文档，推进到下一分析状态。 |
| `POST_EXEC_AUDITING` / `PATCH_SUPPLEMENTING` | 物理代码未动 | 重新运行二次自查或合并增量 diff。 |
| `HEURISTIC_PRECHECK` | 物理代码未动 | 重新运行安全扫描。 |
| `PATCH_APPLYING` | 可能存在半写入 | 执行事务双轨清理，重新从 `patch.diff` 应用。 |
| `PATCH_APPLIED` ~ `CANARY_KILL` | 物理代码已修改 | 感知到补丁已打入，继续运行后续验证阶段（编译/回归/Canary）。 |
| 验证失败 (`FAIL`) | 代码需清理 | 调用 `TransactionalGitTracker.rollback()` 执行双轨回滚，累加 attempt 代数。若超过 2 代则标记 `CIRCUIT_BREAKER_ESCALATED`。 |

---

### 3.2 核心模块类与接口设计

#### 1. 分级防腐与 PAL 纯增量安全检查器 (`safety_checker.py`)
```python
class TieredCParser:
    """Zero-dependency C Tokenizer and Bracket Scanner with optional tree-sitter acceleration."""
    def is_empty_stub(self, func_body: str) -> bool:
        """Strips comments/strings/logs, counts non-trivial statements. Returns True if body <= 1 statement returning constant."""
        ...

class HeuristicSafetyChecker:
    """Enforces H-1 to H-8 rules on proposed patch.diff and modified source."""
    def __init__(self):
        self.c_parser = TieredCParser()

    def validate_patch_scope(self, patch_path: Path) -> Tuple[bool, str]:
        """Checks touched files against SAFE_WRITE_WHITELIST and STRICT_BLOCKED_PATHS."""
        ...

    def validate_pal_additive_increment(self, patch_path: Path) -> Tuple[bool, List[str]]:
        """
        Validates that any changes touching wink-micro-os/pal/ are strictly additive:
        1. Only allows adding new files in pal/include/hal/ or appending new functions to existing headers.
        2. Prohibits altering existing function signatures or modifying existing struct definitions.
        3. Prohibits any vendor header includes (e.g. #include "esp_*.h" or "freertos/*.h").
        4. Verifies Trinity completeness (pal/include/hal/ + targets/wasm/ + targets/esp32/).
        """
        ...

    def validate_patch_content(self, patch_path: Path) -> Tuple[bool, List[str]]:
        """Scans patch diff chunks for stubs (H-1), app-name branching (H-2), compiler warning drops (H-4), float PWM (H-6), etc."""
        ...
```

#### 2. 双轨事务性 Git 差异跟踪器 (`TransactionalGitTracker` in `remediator.py`)
```python
class TransactionalGitTracker:
    """Protects workspace hygiene and performs precise dual-track rollback."""
    def __init__(self, workspace_root: Path):
        self.ws_root = workspace_root

    def pre_flight_check(self, target_files: List[Path]) -> Tuple[bool, str]:
        """Ensures candidate files have no uncommitted dirty modifications before healing starts.
        Terminates immediately if dirty files found, asking user to git stash or commit."""
        ...

    def apply_patch(self, patch_file: Path) -> Tuple[bool, str]:
        """Runs git apply --check followed by atomic git apply."""
        ...

    def rollback(self, modified_files: List[Path], created_files: List[Path]):
        """
        Dual-track precise rollback:
        1. Tracked modified: git checkout -- <file>
        2. Untracked created: file.unlink(missing_ok=True)
        Never executes blanket git checkout . or git clean -fd.
        """
        ...
```

#### 3. 分级零回归验证器 (`ZeroRegressionRunner` in `remediator.py`)
```python
class ZeroRegressionRunner:
    """Drives tiered regression: L1 domain fast-check + L2 full golden suite."""
    def __init__(self, workspace_root: Path, runner_script: Path):
        self.ws_root = workspace_root
        self.runner_script = runner_script

    def run_l1_domain_regression(self, touched_files: List[Path]) -> Tuple[bool, str]:
        """Runs verified apps sharing the same domain based on DOMAIN_FILE_MAP."""
        ...

    def run_l2_golden_regression(self) -> Tuple[bool, str]:
        """Runs all verified apps in checklist.data.json (currently 6 golden benchmarks)."""
        ...
```

#### 4. 执行后自查与自愈控制器主编排 (`remediator.py`)
```python
class Remediator:
    def __init__(
        self,
        workspace_root: Path,
        agent_synthesizer: AgentSynthesizer,
        max_attempts: int = 2,
    ):
        ...

    def run_post_execution_audit(
        self,
        app_entry: dict,
        plan_content: str,
        patch_text: str,
    ) -> Tuple[str, Optional[str]]:
        """Executes one-shot DoD-based completeness audit. Returns (verdict, optional_supplementary_diff)."""
        ...

    def remediate_app(self, app_entry: dict, app_dir: Path, failure_log: str) -> Tuple[bool, str]:
        """Executes full autonomous investigation, review, post-exec audit, patch verification, and regression."""
        ...
```

---

## 四、 详细任务拆分与路线图

```
Phase 1: 凭据规格、分级防腐硬核与双盲 Agent 状态机 (P0)
   ├── T1.1: 13 状态机脚手架与断点恢复管理器 (InvestigationWorkspace & session_state.json)
   ├── T1.2: AgentSynthesizer 双盲 Prompt、patch.diff 格式规范与独立 CLI 调度
   ├── T1.3: 分级解析引擎架构 (safety_checker.py: Tier 1 纯 Python Tokenizer + PAL 纯增量硬检 + H-1~H-8 规则)
   ├── T1.4: 双 Agent 审查与深度融合状态机 (含 PAL 跨平台洁癖审计与深度吸收)
   ├── T1.5: 离线 Mock 单测套件 (test_loop_remediator.py, 秒级 100% 覆盖)
   └── T1.6: PostExecutionAuditor 执行后完整性自查器与增量补遗状态机

Phase 2: 安全修补沙箱、双Target同源校验与分级零回归 (P0)
   ├── T2.1: 事务性 Git 差异捕获与双轨精准回滚 (Pre-flight + checkout + unlink)
   ├── T2.2: 双 Target 同源编译与 PAL 三位一体门检 (Header + Wasm + ESP32 缺一不可)
   ├── T2.3: ZeroRegressionRunner 分级零回归执行器 (L1 领域 / L2 全量黄金套件)
   ├── T2.4: UniSim 跨仓变更熔断与提案归档 (严禁自主修改 TS，仅报提案)
   └── T2.5: 熔断机制与代数控制 (锁定 2 代上限与 CircuitBreaker)

Phase 3: 流水线挂载、标杆示例自愈实证与治理闭环 (P1/P2)
   ├── T3.1: LoopPipeline Phase 3 挂载自愈分支与 CLI 调度器扩充 (--auto-heal, --agent-a-cmd, --agent-b-cmd)
   ├── T3.2: 标杆示例自愈闭环实证 (mqtt_tcp Broker 响应 / i2c_basic 影子从机，Canary 击杀实证)
   ├── T3.3: 规则文档与治理 SOP (governance-sop-esp/SKILL.md) 全量回写
   └── T3.4: CHECKLIST 看板自动重新派生与全套门禁验收
```

---

### Phase 1：凭据规格、分级防腐硬核与双盲 Agent 状态机 (P0)

- [ ] **T1.1（13 状态机脚手架与断点恢复管理器）**
  - **位置**：`wink-micro-app/vendor/esp_idfv61/.governance/tools/loop/remediator.py`
  - **要求**：实现 `InvestigationWorkspace` 类，维护 `session_state.json`（Schema v2）；
  - **能力**：支持完整的 13 阶段状态迁移验证（含 `POST_EXEC_AUDITING` 与 `PATCH_SUPPLEMENTING`）；支持根据当前物理状态安全恢复或回滚；保存原始报错日志 `raw_failure.log`。

- [ ] **T1.2（AgentSynthesizer 双盲 Prompt、patch.diff 格式规范与独立 CLI 调度）**
  - **位置**：`wink-micro-app/vendor/esp_idfv61/.governance/tools/loop/agent.py`
  - **要求**：
    1. 新增 `build_root_cause_prompt(...)`：指导归因定位并产出 01-RCA、02-PLAN 初稿与标准的 Unified Diff 补丁；
    2. 新增 `build_adversarial_review_prompt(...)`：执行**双盲审查**，仅输入原始日志、方案正文与 `patch.diff`，严格剥离思考链，注入对抗找茬与 PAL 跨平台洁癖 Rubric；
    3. 新增 `build_synthesis_prompt(...)`：注入审查意见，重构方案正文与 `patch.diff`，并生成 Synthesis Log；
    4. 新增 `build_post_exec_audit_prompt(...)`：构建基于 DoD 验收清单的执行后完整性追问 Prompt；
    5. 支持 `--agent-a-cmd` 与 `--agent-b-cmd` 独立配置，单 CLI 时自动开启无上下文进程隔离；
    6. **`patch.diff` 格式约束**：必须为标准 Unified Diff 格式，以 `a/` `b/` 前缀标识源/目标路径（与 `git diff` 输出一致），上下文行数固定为 3。Agent 生成 C 代码后，由 `Remediator` 统一调用 `git diff --no-index` 或 `git diff --` 导出，禁止 Agent 自行拼接 diff 文本。

- [ ] **T1.3（分级解析引擎架构与 safety_checker.py 独立模块）**
  - **位置**：`safety_checker.py`（独立模块）
  - **要求**：
    - **Tier 1 纯 Python C 语句解析器**：剥离注释/字符串，检测单语句常量返回、空桩判定，零外部依赖，100% 宿主开箱即用；
    - **Tier 2 AST 增强**：检测到环境有 `tree-sitter` 时无缝启用；
    - **PAL 纯增量与厂商头文件扫描器**：针对 `pal/` 改动执行 ABI 纯增量与厂商专有头文件拦截；
    - 实现 **H-1 至 H-8 规则集**：严格拦截空桩、特判、CMake 告警降级、破坏既有 PAL ABI、浮点 PWM、许可证违规及越界路径。

- [ ] **T1.4（双 Agent 审查与深度融合状态机）**
  - **位置**：`remediator.py`
  - **要求**：实现完整审判吸收循环；机器硬核前置过滤通过后才允许生成最终 `patch.diff`；核验正文 diff，杜绝尾部敷衍追加。

- [ ] **T1.5（离线 Mock 单测套件）**
  - **位置**：`.governance/gates/tests/test_loop_remediator.py`
  - **要求**：编写包含 `MockAgentSynthesizer` 与静态报错快照的离线单测套件，覆盖脚手架、13 状态迁移、YAML 提取、PAL 纯增量硬检、执行后自查补遗分支、H-1~H-8 拦截、双轨回滚，测试必须零网络且毫秒级 100% 通过。

- [ ] **T1.6（PostExecutionAuditor 执行后自查与补遗集成）**
  - **位置**：`remediator.py`、`agent.py`
  - **要求**：在方案合成与物理预检之间挂接单次 DoD 对照自查；支持二值化判词提取（`FULLY_COMPLETE` 与 `GAPS_FOUND`）；支持增量 diff 提取与合并追加。

---

### Phase 2：安全修补沙箱、双Target同源校验与分级零回归 (P0)

- [ ] **T2.1（事务性 Git 差异捕获与双轨精准回滚器）**
  - **位置**：`remediator.py` (`TransactionalGitTracker`)
  - **要求**：
    - Pre-flight 检查目标文件无预存未暂存修改；发现脏文件时**立即终止自愈并报错**，提示用户先 `git stash` 或 `git commit`，不做自动 stash 以避免引入额外状态管理复杂度；
    - `git apply --check patch.diff` 验证无冲突；
    - 记录 `modified_files` 与 `created_files`；
    - 双轨精准回滚：修改项 `git checkout --`，新增项 `file.unlink()`，彻底防止未跟踪文件残留。

- [ ] **T2.2（双 Target 同源编译与 PAL 三位一体门检）**
  - **位置**：`remediator.py`
  - **要求**：
    - 若补丁触及 PAL 新增抽象，强制检验“头文件 + Wasm 仿真桩 + ESP32 物理驱动”三位一体完整性；
    - 驱动 Wasm 仿真编译，检查 C 代码是否干净编译；
    - 环境感知探测：宿主具备 `xtensa-esp32-elf` 则执行硬件级语法预检，不具备则优雅降级为 Clang 语法及宏定义校验，不硬报环境阻塞。

- [ ] **T2.3（分级零回归执行器 ZeroRegressionRunner）**
  - **位置**：`remediator.py`
  - **要求**：
    - **L1 快速回归**：依据改动路径自动重跑同领域的已验证用例。领域映射由 `DOMAIN_FILE_MAP` 静态配置驱动（按改动文件名前缀匹配），并在 `session_state.json` 中记录本次触发的 domain key 便于审计：
      ```python
      DOMAIN_FILE_MAP = {
          "network": ["esp_mqtt", "esp_http", "wifi", "tcpip", "sim_net_responder"],
          "peripheral": ["i2c", "spi", "uart", "gpio", "pwm", "adc", "dac"],
          "timer": ["gptimer", "hw_timer", "mcpwm"],
      }
      ```
    - **L2 全量回归**：重跑 `checklist.data.json` 中全量 6 大黄金用例（`blink`, `ledc_basic`, `gptimer`, `uart_echo`, `esp_http_client`, `station`）；若触及 PAL，L2 零回归一票否决权置顶。

- [ ] **T2.4（UniSim 跨仓变更熔断与提案归档）**
  - **位置**：`remediator.py`
  - **要求**：
    - 若归因判定涉及 `packages/unisim`（Layer B1），Agent 严禁自主改动 TS 代码；
    - 自动保留排查分析与修复提案，触发熔断标记 `[BLOCKED_UNISIM_CHANGE_HUMAN_REVIEW]`，等待人工架构师审批。

- [ ] **T2.5（熔断机制与代数控制）**
  - **位置**：`remediator.py` (`CircuitBreaker`)
  - **要求**：严格锁定 2 次最大自愈重试代数；超次或遇到架构级接口变更直接熔断，双轨回滚代码，保留调查文档，打上 `[BLOCKED_ON_INFRA_HUMAN_TRIAGE]` 标签。

---

### Phase 3：流水线挂载、标杆示例自愈实证与治理闭环 (P1/P2)

- [ ] **T3.1（LoopPipeline 挂载与 CLI 开关扩充）**
  - **位置**：`pipeline.py`、`runner.py`
  - **要求**：
    - 在 `runner.py` 增加 `--auto-heal`、`--max-heal-attempts`、`--agent-a-cmd`、`--agent-b-cmd`；
    - 在 `pipeline.py` Phase 3 基线失败分支无缝挂接 `remediator.remediate_app()`；
    - 自愈成功后顺畅流转至 Phase 4（Canary 击杀）与 Phase 5（凭据归档）。

- [ ] **T3.2（标杆示例端到端自愈实证）**
  - **目标工程**：`mqtt_tcp`（或 `i2c_basic`）
  - **要求**：
    - 运行 `python run_loop.py --app mqtt_tcp --auto-heal`；
    - 观察系统建立 `.governance/investigations/esp.protocols.mqtt/`；
    - 观察 Agent A 与 Agent B 的双盲审查、方案融合、执行后 DoD 自查补遗与 `patch.diff` 导出；
    - 观察底层 MQTT 仿真桩的补充、分级零回归通过、Phase 4 Canary 变异击杀通过（验证桩的真实动态行为）及 Git 提交。

- [ ] **T3.3（规则文档与治理 SOP 回写）**
  - **位置**：`.agents/skills/governance-sop-esp/SKILL.md`
  - **要求**：正式更新 SOP 文档，增补“底座缺陷自主调查与受控 PAL 增量治理规程”，固化不可违背的架构标准。

- [ ] **T3.4（全量门禁与看板复核）**
  - **要求**：运行 `python run_gates.py --gate 1`、`python check_license_map.py` 与看板生成脚本，全绿交付。

---

## 五、 风险矩阵与应急预案

| 风险编号 | 风险描述 | 影响等级 | 预防与应急机制 |
|---|---|---|---|
| **R-1** | 评审 Agent 产生盲目共谋（虚假 APPROVED） | 高 | 部署 **双盲审查协议**（剥离思考链）+ **Tier 1 纯 Python 机器防腐硬核（HeuristicSafetyChecker）**，物理级扫描空桩和特判，不受 LLM 主观意志影响。 |
| **R-2** | 底座修改引入破坏性变更破坏既有黄金标杆 | 极高 | 部署 **T2.3 分级零回归验证器**，任何历史 verified 项失败立即执行双轨精准回滚。 |
| **R-3** | 自愈生成的 C 框架 Mock 为静态假桩 | 高 | 部署 **Phase 4 Canary 变异击杀** 作为动态行为检验器：假桩由于缺乏真实状态机交互，无法杀死变异场景，直接被判负打回。 |
| **R-4** | 事务回滚粗暴清除用户未暂存代码或遗留新增文件 | 高 | 部署 **T2.1 差异双轨精准追踪**，前置 Pre-flight 检查工作区，回滚时仅对受控修改项 `checkout`、新增项 `unlink`。 |
| **R-5** | Agent 陷入循环重试消耗算力并破坏代码 | 中 | 部署 **T2.5 熔断器**，强制锁定 2 次尝试上限，超时或超次立即冻结回滚。 |
| **R-6** | 宿主缺少编译型 C 解析器依赖导致门禁爆错 | 中 | 部署 **Tier 1 零外部依赖纯 Python Tokenizer**，确保在任何标准 Python 3.11 环境 100% 离线稳定运行。 |
| **R-7** | PAL 机械式一刀切禁止改动导致数百个用例批量阻塞 | 极高 | **解绑一刀切限制，推行 Core-B 受控纯增量演进**：允许新增通用外设抽象与驱动，彻底打通后续 Lane 4/5 模拟量与存储等用例的进化瓶颈。 |
| **R-8** | PAL 增量扩展中混入 ESP32 厂商专有代码或破坏既有 ABI | 极高 | 部署 **H-5 机器硬检**（拦截任何 `esp_*.h`、禁止改动既有函数签名）+ **Agent B 跨平台洁癖审计** + **三位一体同源提交验证**，死守跨平台纯净性。 |
| **R-9** | Agent 执行后过度反思（谄媚怀疑、画蛇添足与过度设计） | 高 | 部署 **T1.6 二值化封闭判词**（强制对照原方案 DoD 条目提供证据）+ **单次收敛上限**（严格最多追问 1 次），严禁发散式重构。 |

---

## 六、 全局验收标准 (DoD)

- [ ] **DoD-1（双重审查防线）**：自愈方案不仅经过 Agent B 的双盲对抗评审与 Agent A 深度融合，还通过了机器 8 大硬核防腐检查；
- [ ] **DoD-2（调查证据不可变性）**：`.governance/investigations/<app_id>/` 留存合规的 Markdown 文档、`patch.diff` 与 `session_state.json`，具备完全的可审计性；
- [ ] **DoD-3（零回归铁律）**：修改底座或扩展 PAL 后，全量 6 大历史已交付黄金用例 100% 回归通过，无一退化；
- [ ] **DoD-4（防假绿动态检验）**：自愈后的应用必须通过 Canary 变异击杀（Fail-Loud），验证底层桩具备真实状态机响应；
- [ ] **DoD-5（单测与门禁完备）**：自愈引擎离线单测套件 100% 毫秒级通过，Gate 1 门检与 License 门检 100% 通过；
- [ ] **DoD-6（离线单测自洽性）**：单测套件内置 Mock 机制，严禁在自动化测试中消耗真实 LLM API 或依赖外网；
- [ ] **DoD-7（补丁工件与受控 PAL 增量演进合规）**：所有代码变更必须以受控 `patch.diff` 为介质且 100% 落在安全白名单内；触及 PAL 的改动必须 100% 满足纯增量、无厂商头文件污染及三位一体同源交付（Header + Wasm + ESP32）；
- [ ] **DoD-8（执行后自查闭环合规性）**：补丁正式应用前必须经过基于 DoD 清单的单次执行后自查，漏项增量补遗必须通过机器防腐硬核检验。
