<!-- SPDX-License-Identifier: Apache-2.0 -->
# ESP-IDF 分类管治门禁系统实施计划 (v2.0 修订版)

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260929-ESP-IDF-GATE-SYSTEM-IMPL-v2.0 |
| 状态 | **📋 待执行（Ready to Execute / v2.0 终版修订）** |
| 日期 | 2026-09-29 |
| 前置条件 | ① `checklist.data.json`（478条，Schema v2.0）已归档冻结 ✅<br>② `capability-catalog.yaml` 已创建并受管 ✅<br>③ `.gates/quarantine.yaml`（10 项存量隔离区，14 天 TTL 至 `2026-10-13T23:59:59Z`）已挂载 ✅<br>④ `generate_checklist_v1_1.py` 与 `scripts/verify_phase_c_closed_loop.py` 验证逻辑完备 ✅ |
| 决策依据 | [ADR-0090](../../decisions/unisim/0090-centralized-pluggable-gate-system.md)（集中式可插拔门禁系统）<br>[ADR-0091](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)（多配置实例与五维正交 Schema 架构决策） |
| 技术设计 | [esp-idf-classification-gate-system.md](../../zh/tech-designs/esp32/esp-idf-classification-gate-system.md) (v2.0 规格)<br>[esp-idf-classification-schema-spec.md](../../zh/tech-designs/esp32/esp-idf-classification-schema-spec.md) (阶段 A 终版标准) |
| 评审记录依据 | [2026-09-29-esp-idf-gate-system-implementation-plan-review.md](../../reviews/esp32/2026-09-29-esp-idf-gate-system-implementation-plan-review.md)（全面吸收 P0/P1/P2 评审结论） |
| 估算工时 | 约 6~8 小时（按 Step 1~7 顺序执行） |

---

## 验收标准（全局）

- [ ] `python .gates/run_gates.py --mode pr --changed-files /tmp/changed.txt` 针对 HEAD 数据可稳定运行，输出结构化 JSON 报告，退出码 0（0 errors，10 warnings 存量隔离警告）
- [ ] `python .gates/run_gates.py --mode nightly` 可无错运行，执行全部 Gate 1~4 规则
- [ ] **存量隔离区与 TTL 防御**：任何不在 `quarantine.yaml` 白名单中的新增 `verified` 若缺少凭证（哈希或报告），在 PR 和 Nightly 模式下一律判定为 `error`（退出码 1 硬阻断）；超过 TTL（`2026-10-13T23:59:59Z`）白名单项自动转为 `error` 硬阻断
- [ ] **执行器防御性退出码**：`gates.yaml` 规则数为 0 或规则插件加载失败时，强制以退出码 2（执行器异常）报错阻断，严禁假成功
- [ ] **Gate 2 PAL 防膨胀**：对 `wink-micro-os/pal/**`、`targets/**`、`osal/**` 增量变更执行器件协议黑名单扫描，拦截违规符号
- [ ] **Gate 3 分层严密性**：委托 `winkcli lint` 执行 7 pack 全量分层校验；CI 环境下若工具链缺失禁止隐式放行
- [ ] **Gate 4 反向传递闭包与回归调度**：`impact_scope.py` 基于 Catalog 的 `depends_on` 递归遍历反向依赖闭包；在 `pr_inline: true`（≤30 条）时实际调用 Headless 场景运行器进行现场回归测试，断言失败以退出码 1 阻断 PR；>30 条时将受影响条目标记为 `stale`
- [ ] **单元测试体系覆盖**：Gate 1 (10条)、Gate 2 (3条)、Gate 3 (1条)、Gate 4 (影响分析与调度) 均具备独立单元测试，正向合规与负向违规用例全覆盖
- [ ] **CI 流水线挂载**：在 `.github/workflows/esp_idf_ci.yml`（或 `pr.yml`）中挂载门禁执行步骤，配置确定的 git diff 提取与报告产物上传
- [ ] 许可门禁 `check_license_map.py` 通过（所有新增 Python 文件标注 `Apache-2.0` SPDX）

---

## 任务拆分

### 第一步：建立目录骨架与增强型共享基础设施

**目标**：建立 `.gates/` 目录，实现执行器骨架、UTC 时间基准与加载 `quarantine.yaml` 的共享上下文构建器，严格定义退出码 0/1/2 语义。

**子任务：**

#### T1.1 建立 `.gates/` 目录骨架

```
wink-micro-app/vendor/esp_idfv61/.gates/
├── gates.yaml          ← 【唯一门禁注册表】声明 Gate 1~4 全量规则配置（Schema v2.0）
├── quarantine.yaml     ← 【存量隔离区白名单】（已就位，10 条，TTL 截至 2026-10-13）
├── run_gates.py        ← 【唯一 CI 入口】执行器骨架（只读纯函数，不修改文件）
├── gate_context.py     ← 增强型共享上下文（加载 manifest, catalog, quarantine, UTC 时间, 安全 diff）
├── impact_scope.py     ← Gate 4 独立工具（反向传递闭包遍历与回归调度器）
├── rules/
│   ├── __init__.py
│   ├── g1_*.py         ← Gate 1 规则（共 10 条，全面对齐 Schema v2.0）
│   ├── g2_*.py         ← Gate 2 规则（共 3 条）
│   ├── g3_*.py         ← Gate 3 规则（共 1 条）
│   └── g4_*.py         ← Gate 4 规则（共 1 条）
└── reports/
    └── .gitkeep
```

#### T1.2 实现 `gate_context.py`（增强型共享上下文）

负责构建传递给每条规则的共享 `context` dict，必须包含以下防御性增强：
1. **加载存量隔离区**：解析 `.gates/quarantine.yaml`，提取 `quarantined_entries` 字典，供 Gate 1 进行限域豁免与 TTL 判断；
2. **UTC 时间基准**：强制注入 `now_utc = datetime.now(timezone.utc)`，杜绝跨时区 Runner 时间判断漂移；
3. **确定性 Git diff 输入**：
   - 优先读取调用方传入的 `--changed-files`；
   - 若未指定且在本地调试，尝试获取 git 状态；**若在 CI 模式下变更列表为空且未传 `--allow-empty-diff`，直接抛出异常，禁止静默回退**；
4. **POSIX 路径正规化**：所有文件路径自动执行 `normpath(p).replace("\\", "/")`；
5. **Workspace Root 解析**：定位嵌入式仓根目录，供场景文件与产物磁盘真实存在性比对。

```python
def build_context(manifest_path, catalog_path, quarantine_path, changed_files_path, mode, allow_empty_diff=False) -> dict:
    return {
        "manifest":       json.load(...),       # checklist.data.json (Schema v2.0)
        "catalog":        yaml.safe_load(...),   # capability-catalog.yaml
        "quarantine":     yaml.safe_load(...),   # .gates/quarantine.yaml
        "changed_files":  [...],                 # POSIX 正规化文件列表
        "workspace_root": "...",                 # 仓库根绝对路径
        "spec_version":   "2.0.0",
        "mode":           mode,
        "now_utc":        datetime.now(timezone.utc),
    }
```

#### T1.3 实现 `run_gates.py` 执行器骨架

- 解析命令行参数（`--mode`, `--gate`, `--rule`, `--changed-files`, `--allow-empty-diff`, `--output-json`, `--no-fail`）；
- 加载 `gates.yaml` 并过滤当前 mode 生效规则；**断言生效规则数 > 0，若为 0 立即退出码 2 报错（防空规则假成功）**；
- 动态加载规则模块，**若模块 import 失败以退出码 2 报错**；
- 调用 `rule.run(context, config)`，捕获规则内部抛出的异常转为 `severity: error` 的 Finding；
- 汇总 Finding，按 `(gate, rule_id, display_id or 0, message)` 强制稳定排序；
- 输出结构化报告（stdout + 可选 JSON 文件）；
- **严格退出码语义**：
  - `0`：全部通过（允许 warning/info）；
  - `1`：存在 `severity: error` 的 Finding（CI 阻断）；
  - `2`：执行器自身异常（YAML 解析错误、零规则执行、插件加载失败等）。

**验收**：
- `python .gates/run_gates.py --mode pr` 在空规则时必须退出码 2；
- 在正确规则注册下可生成合规 JSON 报告。

---

### 第二步：实现 Gate 1 全量规则（10条，对齐 Schema v2.0 与六要素裁判）

**目标**：完全消除旧版 Schema v1.1 字段依赖，全面实现基于 ADR-0091、Schema v2.0、`quarantine.yaml` 14 天 TTL 与六位一体裁判公式的硬核校验。

每条规则对应一个 `rules/g1_*.py` 文件，实现 `run(context, config) -> list[Finding]`。

| 子任务 | 规则文件 | 严重度 (PR / Nightly) | 核心逻辑与防腐要点 (Schema v2.0) |
|---|---|:---:|---|
| T2.1 | `g1_path_unique.py` | error / error | 遍历 entries，记录已见 `upstream_path`，发现重复 → error |
| T2.2 | `g1_cap_id_exists.py` | error / error | 比对每个条目的 `required_capabilities` 与 `catalog["capabilities"]` 键集合，发现未声明能力 → error |
| T2.3 | `g1_id_format.py` | error / error | 校验条目稳定 ID 格式符合正则 `^esp\.[a-z0-9_]+(\.[a-z0-9_]+)+$`，发现不合规 → error |
| T2.4 | `g1_version_alignment.py` | error / error | 校验条目 `written_at_spec_version == "2.0.0"`，且根部 `spec_version == "2.0.0"`，任何旧版版本号 → error |
| T2.5 | `g1_execution_configs.py` | error / error | 校验每个条目持有实体数组 `executions: [...]` 且 `len >= 1`；校验 `config_id`、`backend`（wasm_browser/wasm_node/esp32_hardware/host_native）、`target_soc`、`profile` 合法性 |
| T2.6 | `g1_orthogonal_states.py` | error / error | 校验五维状态正交性：`scope.inclusion`（unknown/in_scope/out_of_scope）、`scope.schedule`（active/deferred）、`audit.verdict`（pending/audited/needs_review）与 `delivery_state` 的组合合法性，禁止未审先排与已完成仍为缺口 |
| T2.7 | `g1_can_check_mark.py` | warning / error | **【六要素闭环裁判与隔离区 TTL 分流核心】**：<br>遍历条目中每个声明 `delivery_state == "verified"` 的配置：<br>1. 校验范围是否为 `in_scope`，审计是否为 `audited` 且覆盖该 `config_id`；<br>2. 校验其依赖闭包计算是否为 `satisfied`（无 unknown/blocked）；<br>3. 校验 `evidence` 必须非空且包含 64 位 `assets_sha256`、`scenario_sha256`、`execution_report_ref`；<br>4. 校验场景文件真实存在于磁盘；<br>5. **隔离区白名单分流**：若 `(entry.id, config_id)` 位于 `quarantine.yaml` 且 `now_utc <= grace_period_expires`，报告 `WARNING (Quarantined Debt)`；**若不在隔离区或 TTL 已逾期，在 PR 和 Nightly 下一律判定为 ERROR 强制阻断**！ |
| T2.8 | `g1_quarantine_ttl.py` | error / error | 独立扫描 `quarantine.yaml`，若 `now_utc > grace_period_expires`（`2026-10-13T23:59:59Z`），对涉及条目立即报告阻断 ERROR，督促提交显式回退 PR |
| T2.9 | `g1_sla_evidence.py` | error / error | 当 `scope.inclusion == "out_of_scope"` 或 `acceptance.type == "expected_rejection"` 时，校验必须显式声明 `sla_error_symbol` 与 `exclusion_evidence` |
| T2.10 | `g1_auditor_required.py` | error / error | 当 `audit.verdict == "audited"` 时，`audit.auditor` 必须非空且非占位符，且 `audit.audited_configs` 数组必须非空 |

**验收**：

```bash
# 针对当前 HEAD 运行 PR 模式（10 项存量债务在 TTL 期内受控发 warning，0 errors）
python .gates/run_gates.py --mode pr --output-json reports/gate_report.json
# 预期：0 errors，10 warnings (隔离区存量条目)，退出码 0

# 模拟新增一个 verified 但未填凭证的条目
# 预期：g1.can_check_mark 报 ERROR，退出码 1 强制阻断

# 模拟系统时间超过 2026-10-13T23:59:59Z
# 预期：g1.quarantine_ttl 报 10 个 ERROR，退出码 1 强制阻断
```

---

### 第三步：实现 Gate 2 规则（3条）与 PAL 防膨胀

**目标**：Gate 2 规则由 `git_diff` 触发，防止底层抽象层因示例接入而引入特定协议/器件关键词污染。

| 子任务 | 规则文件 | 核心逻辑要点 |
|---|---|---|
| T3.1 | `g2_pal_naming.py` | 对 `changed_files` 中涉及 `wink-micro-os/pal/**`、`targets/**`、`osal/**` 的新增代码行，按黑名单关键词正则扫描（`ws2812|rgb|pixel|necir|at24|i2c_addr|touch_pad|servo|oled`），命中即报 ERROR 阻断 |
| T3.2 | `g2_cap_has_owned_paths.py` | 遍历 `catalog["capabilities"]`，任何能力的 `owned_paths` 为空列表或缺失字段 → ERROR |
| T3.3 | `g2_cap_cross_mcu.py` | `status=implemented` 的能力缺少 `cross_mcu_evidence` 凭据 → WARNING |

**验收**：
- 变更普通应用层代码时 Gate 2 自动标记为 `SKIP`；
- 变更 `pal_gpio.h` 并引入 `ws2812` 命名时，Gate 2 报 ERROR 阻断（退出码 1）。

---

### 第四步：实现 Gate 3（winkcli 严格委托与分层守卫）

**目标**：将分层架构防腐检查纳入门禁系统，严格委托 `winkcli lint` 执行 7 pack 全量校验，杜绝环境降级放行。

| 子任务 | 说明 |
|---|---|
| T4.1 | 实现 `g3_winkcli_lint.py`：优先调用 `winkcli lint`，支持指定 `--pack layering --pack api --pack dal --pack isr --pack user_surface --pack wasm --pack i18n` |
| T4.2 | 若环境未全局安装 `winkcli`，自动探测 `wink-tools/wink.py` 或 `wink-micro-os/tools/run_lint.py` 作为等价调用入口 |
| T4.3 | **消除隐式跳过**：在 `gates.yaml` 中配置 `strict_environment: true`；在 PR/Nightly CI 模式下，若工具链确实完全缺失，**判定为阻断 ERROR（退出码 1 或 2）**，严禁在 CI 中无扫描放行 |
| T4.4 | 配置执行超时保护（120s），捕获 lint 输出的违规违例转为结构化 Finding |

**验收**：
- 故意在 App 代码中引入直接依赖底层未导出 PAL 头文件的倒灌代码，Gate 3 稳定报 ERROR 阻断（退出码 1）。

---

### 第五步：实现 Gate 4 影响图谱遍历与真实回归闭环

**目标**：实现 `impact_scope.py` 独立工具与回归调度闭环。不仅输出受影响清单，而且在 PR 预算内（≤30 条）现场调用 Headless 场景运行器执行真实回归测试，超阈值时触发证据失效（`stale`）流转。

#### T5.1 重构 `impact_scope.py` 核心算法（反向传递闭包）

```python
def compute_impact_closure(changed_files: list[str], catalog: dict, manifest: dict) -> dict:
    capabilities = catalog.get("capabilities", {})
    direct_caps = set()

    # 1. 匹配直接受影响能力 (owned_paths)
    norm_changed = [os.path.normpath(f).replace("\\", "/") for f in changed_files]
    for cap_id, cap_def in capabilities.items():
        for owned in cap_def.get("owned_paths", []):
            norm_owned = os.path.normpath(owned).replace("\\", "/")
            if any(fnmatch.fnmatch(f, norm_owned) for f in norm_changed):
                direct_caps.add(cap_id)

    # 2. 构造反向依赖有向图并递归计算传递闭包
    reverse_graph = defaultdict(set)
    for parent_id, cap_def in capabilities.items():
        depends_on = cap_def.get("depends_on", {})
        for req in depends_on.get("mandatory", []):
            reverse_graph[req].add(parent_id)
        for cond in depends_on.get("conditional", []):
            for req in cond.get("requires", []):
                reverse_graph[req].add(parent_id)

    closure_caps = set(direct_caps)
    worklist = list(direct_caps)
    while worklist:
        curr = worklist.pop(0)
        for parent in reverse_graph.get(curr, []):
            if parent not in closure_caps:
                closure_caps.add(parent)
                worklist.append(parent)

    # 3. 收集受影响示例
    impact_entries = []
    for entry in manifest["entries"]:
        reqs = set(entry.get("required_capabilities", []))
        if reqs & closure_caps:
            impact_entries.append(entry["display_id"])

    # 4. 非代码变更扩散（Catalog/场景脚本变动）
    if any("capability-catalog.yaml" in f for f in norm_changed):
        # Catalog 发生变动，标记全部依赖发生修订的能力相关示例
        pass

    pr_inline = len(impact_entries) <= 30
    return {
        "affected_capabilities": sorted(list(closure_caps)),
        "impact_entries": sorted(impact_entries),
        "pr_inline": pr_inline,
    }
```

#### T5.2 实现回归调度与状态机流转闭环

- 当 `pr_inline: true` 时：
  - 自动调度执行器（参考 `scripts/verify_phase_c_closed_loop.py`）针对受影响示例运行 Headless 仿真测试；
  - 若任何一个示例的断言失败或崩溃，**Gate 4 报告 ERROR 并以退出码 1 阻断 PR 合并**；
- 当 `pr_inline: false` 时：
  - 输出结构化 Nightly 待回归清单文件 `reports/nightly_pending_regression.json`；
  - 在生成的门禁报告中将受波及的已验证条目标记为 **`stale`（证据陈旧待重验）**，防止旧凭证被误认为有效。

**验收**：
- 修改底层微秒脉冲驱动，Gate 4 能够向上溯源定位到使用 `ws2812` 的灯带示例 `#064`；
- 故意破坏 `#064` 对应的仿真场景断言，Gate 4 真实触发回归测试并阻断 PR。

---

### 第六步：单元测试体系与负向反例覆盖

**目标**：编写覆盖 Gate 1~4 的全量独立单元测试集，覆盖 15+ 典型场景（涵盖评审报告识别的全部风险用例）。

```
wink-micro-app/vendor/esp_idfv61/.gates/tests/
├── fixtures/
│   ├── minimal_manifest_v2.json  # 极简合规数据（Schema v2.0，3条）
│   ├── minimal_catalog.yaml      # 极简能力字典（含 depends_on）
│   ├── quarantine_sample.yaml    # 隔离区样本（含未过期与已过期条目）
│   └── violations/               # 各类负向破坏样本
├── test_context.py               # 测试 UTC 时间、POSIX 路径、Git diff 严格输入
├── test_impact_scope.py          # 测试反向传递闭包算法与回归调度
└── test_rules/
    ├── test_g1_path_unique.py
    ├── test_g1_cap_id_exists.py
    ├── test_g1_execution_configs.py  # 测试 executions 缺失与非法字段
    ├── test_g1_orthogonal_states.py  # 测试五维状态正交冲突
    ├── test_g1_can_check_mark.py     # 测试六要素裁判、空哈希拦截与隔离区豁免
    ├── test_g1_quarantine_ttl.py     # 测试 14 天 TTL 到期阻断
    ├── test_g1_sla_evidence.py       # 测试 OOS 预期拒绝声明
    ├── test_g2_pal_naming.py         # 测试 PAL 负向正则扫描
    ├── test_g3_winkcli_lint.py       # 测试 Gate 3 严格分层拦截
    └── test_sys_exit_codes.py        # 测试空规则表与插件损坏退出码 2
```

**验收**：
```bash
cd wink-micro-app/vendor/esp_idfv61/.gates
pytest tests/ -v
# 预期：全部单元测试 100% PASS
```

---

### 第七步：CI 流水线挂载与端到端闭环

**目标**：将门禁系统正式挂载至 GitHub Actions CI 流水线，终结“本地有脚本但 CI 零挂载”的失控状态。

**子任务：**

#### T7.1 修改 `.github/workflows/esp_idf_ci.yml`（或 `pr.yml`）

新增标准门禁检查 Step，解决 Shallow Clone 下的 diff 基准提取与 Artifact 上传：

```yaml
- name: Run ESP-IDF Gate System (PR mode)
  shell: bash
  run: |
    # 提取 PR 相对基线的分支变更文件列表
    git fetch origin ${{ github.base_ref }} --depth=1
    git diff --name-only origin/${{ github.base_ref }}...HEAD > /tmp/changed_files.txt
    
    # 运行门禁系统（PR 模式）
    python wink-micro-app/vendor/esp_idfv61/.gates/run_gates.py \
      --mode pr \
      --changed-files /tmp/changed_files.txt \
      --output-json reports/gate_report.json
  continue-on-error: false

- name: Upload Gate Report
  if: always()
  uses: actions/upload-artifact@v4
  with:
    name: esp_idf_gate_report
    path: reports/gate_report.json
```

#### T7.2 修改 `.github/workflows/nightly.yml`

在每日定时构建中挂载 Nightly 严格全量门禁与扩展回归：

```yaml
- name: Run ESP-IDF Gate System (Nightly full mode)
  shell: bash
  run: |
    python wink-micro-app/vendor/esp_idfv61/.gates/run_gates.py \
      --mode nightly \
      --output-json reports/gate_report_nightly.json
  continue-on-error: false
```

---

## 风险与缓解措施 (v2.0)

| 风险 | 可能性 | 影响 | 缓解措施 |
|---|:---:|:---:|---|
| `gates.yaml` 规则声明被清空或模块路径错误 | 中 | 高 | 执行器启动时强制自检：若生效规则数为 0 或规则导入失败，立即以退出码 2 报错阻断 CI（严禁退出 0） |
| CI Shallow Clone（深度为1）导致 diff 获取为空从而静默 SKIP | 高 | 高 | 明确在工作流中执行 `git fetch origin ${{ github.base_ref }} --depth=1`，并通过参数强制要求 `--changed-files`，禁止隐式回退到 `git diff HEAD` |
| 新提交的 PR 声明 `verified` 但未提供防伪哈希与报告 | 高 | 致命 | 严格限制 `.gates/quarantine.yaml` 限域白名单；任何非白名单条目未填凭据在 PR 模式下一律判定为 ERROR 阻断合并 |
| 存量 10 项债务无限期停留在隔离区 | 中 | 中 | 设置 14 天硬性 TTL（`2026-10-13T23:59:59Z`）；`g1_quarantine_ttl` 规则在到期后强制判定为 ERROR，督促提交回退 PR |
| `winkcli` 工具链缺失导致分层防线失效 | 中 | 高 | Gate 3 在 CI 模式下配置 `strict_environment: true`，工具缺失或未通过校验时禁止降级放行 |
| 变更波及 >30 项条目推迟至 Nightly 导致 PR 盲目放行 | 中 | 中 | 对超阈值条目强制将其在当次报告中标记为 `stale`，生成待验证清单，防止以旧凭证冒充有效通过 |

---

## 阶段切换与完成标志

全部七步任务完成后，整个治理体系达到以下不可逆的冻结状态：

```
统一入口：
  run_gates.py --mode pr      → 开发者日常 PR 必过门禁（严格阻断新债务，容错 TTL 期内存量，退出码 0）
  run_gates.py --mode nightly → 每夜全量严格回归（阻断全部缺口，退出码 0）
  
过渡脚本退休：
  generate_checklist_v1_1.py 中的 validate_data() 彻底由 run_gates.py Gate 1 接管
  generate_checklist_v1_1.py 仅保留单向只读 Markdown 渲染职责
```

到达此状态后，本实施计划归档为 **Completed**，四道自动化门禁正式成为保护跨靶同源编译与仿真的坚固长城。
