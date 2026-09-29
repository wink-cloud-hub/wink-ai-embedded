# ESP-IDF 分类管治门禁系统技术设计规格

| 字段 | 内容 |
|---|---|
| 文档编号 | TECH-DESIGN-ESP32-GATE-SYSTEM-v1.0 |
| 状态 | **Accepted（设计完成，待实施）** |
| 日期 | 2026-09-29 |
| 决策依据 | [ADR-0090](../../../docs/decisions/unisim/0090-centralized-pluggable-gate-system.md)（集中式可插拔门禁系统）<br>[ADR-0091](../../../docs/decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)（多配置实例与五维正交 Schema 架构） |
| 数据 SSOT | [`checklist.data.json`](../../../wink-micro-app/vendor/esp_idfv61/checklist.data.json) (Schema v2.0)<br>[`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/capability-catalog.yaml) |
| 规范依据 | [`CLASSIFICATION-SPEC.md`](../../../wink-micro-app/vendor/esp_idfv61/CLASSIFICATION-SPEC.md) v2.0 §七 CI Gate 1~4<br>[ESP-IDF 分类数据 Schema 终版规格](esp-idf-classification-schema-spec.md) |
| 实施计划 | [分类规范执行基线整改计划](../../../implementation-plans/esp32/2026-09-29-esp-idf-classification-baseline-remediation-plan.md) (Approved) |

---

## 一、系统目标

为 ESP-IDF v6.1 官方示例仿真适配体系（478 条 Checklist 条目）建立一套**长期可维护、可插拔扩展**的自动化质量门禁系统，确保：

1. **数据完整性**：`checklist.data.json` 与 `capability-catalog.yaml` 两个 SSOT 的结构与引用约束始终有效（Gate 1）；
2. **PAL 架构边界**：底层抽象层不因示例迁移而引入器件协议污染（Gate 2）；
3. **分层合规**：代码库的 App→BAL→DAL→PAL 单向依赖链始终成立（Gate 3）；
4. **变更影响可见**：任何底层文件变更，其上游受影响的示例集合可被自动计算并触发定向回归（Gate 4）。

---

## 二、目录结构与文件职责

```
wink-micro-app/vendor/esp_idfv61/
├── checklist.data.json          ← 数据 SSOT（478 条，Schema v2.0）
├── capability-catalog.yaml      ← 能力字典 SSOT（含 depends_on 依赖图）
├── generate_checklist_v1_1.py   ← CHECKLIST.md 渲染器（过渡期工具；待升级至 v2.0 裁判逻辑）
│
└── .gates/                      ← 门禁系统根目录（本设计主体）
    │
    ├── gates.yaml               ← 【唯一门禁注册表】所有规则的声明式定义
    ├── quarantine.yaml          ← 【存量债务隔离区白名单】14 天硬性 TTL，逾期硬阻断
    ├── run_gates.py             ← 【唯一 CI 入口】统一执行器（只读纯函数，不修改文件）
    ├── gate_context.py          ← 共享上下文构建器（数据加载、git diff 解析）
    │
    ├── rules/                   ← 插件目录（每文件一条规则）
    │   ├── __init__.py
    │   │
    │   │── Gate 1: SSOT 数据完整性 ─────────────────────
    │   ├── g1_path_unique.py          # 路径唯一性
    │   ├── g1_cap_id_exists.py        # 能力 ID 合法性
    │   ├── g1_assets_sha256.py        # 资产哈希完整性
    │   ├── g1_oos_has_evidence.py     # OOS 排除证据与 SLA 符号
    │   ├── g1_verified_has_negative.py# verified 必须有负例
    │   ├── g1_no_pending_verified.py  # pending 状态不得有 verified 交叉
    │   ├── g1_id_format.py            # ID 格式正则校验
    │   ├── g1_version_alignment.py    # spec_version 版本对齐
    │   ├── g1_scenario_exists.py      # verified 场景脚本磁盘真实存在
    │   ├── g1_auditor_required.py     # audited 明确责任审计人
    │   ├── g1_soc_matrix_complete.py  # 四芯片矩阵字段齐全
    │   │
    │   │── Gate 2: PAL 防膨胀 ──────────────────────────
    │   ├── g2_pal_naming.py           # PAL 符号负向命名扫描
    │   ├── g2_cap_has_owned_paths.py  # 每个能力必须有 owned_paths
    │   ├── g2_cap_cross_mcu.py        # 新能力（planned 除外）需有跨 MCU 凭据
    │   │
    │   │── Gate 3: 分层 Lint ────────────────────────────
    │   └── g3_winkcli_lint.py         # 委托 winkcli lint（6 pack 全量）
    │
    │── Gate 4: 依赖反向影响闭包 ──────────────────────────
    ├── impact_scope.py          ← 独立工具（可单独调用）
    │                              输入: 变更文件列表
    │                              输出: 受影响能力 + 受影响示例集合
    │
    └── reports/                 ← 执行报告输出目录（.gitignore）
        └── gate_report.json
```

---

## 三、`gates.yaml` Schema 规范

`gates.yaml` 是门禁系统的**单一配置真相来源**，所有关于"有哪些门禁、在什么时候触发、严重度如何"的信息全部在此声明。

### 顶层结构

```yaml
spec_version: "2.0.0"          # 对齐 CLASSIFICATION-SPEC.md v2.0 版本
description: "WinkMicroOS ESP-IDF 示例分类管治门禁注册表 (SSOT)"

modes:                          # 触发模式定义
  pr:
    max_wall_seconds: 120       # PR 超时阈值
  nightly:
    max_wall_seconds: 3600      # Nightly 全量超时阈值

rules:                          # 规则列表（见下节）
  - ...
```

### 单条规则字段定义

```yaml
rules:
  - id: g1.cap_id_exists           # 必填。规则唯一标识符。格式: g<gate>.<name>
    gate: 1                        # 必填。所属门禁编号 1~4
    severity: error                # 必填。error(阻断) | warning(警告) | info(仅报告)
    modes: [pr, nightly]           # 必填。在哪些触发模式下生效
    module: rules.g1_cap_id_exists # 必填。Python 模块路径（相对 .gates/ 目录）
    description: "..."             # 必填。人类可读描述，用于报告

    # 可选：在特定 mode 下覆盖 severity
    mode_severity_override:
      pr: warning                  # PR 模式降为 warning，nightly 保持 error

    # 可选：仅在 git diff 包含特定路径变更时触发
    trigger: git_diff
    trigger_paths:
      - "wink-micro-os/pal/**"
      - "wink-micro-os/targets/**"

    # 可选：传递给规则 run() 函数的额外配置
    config:
      packs: [layering, api, dal, isr, user_surface, wasm, i18n]
```

---

## 四、插件接口规范

每个 `rules/g*.py` 文件必须实现以下接口，**不得有其他强制约定**：

```python
# rules/g1_cap_id_exists.py
# SPDX-License-Identifier: Apache-2.0

RULE_ID = "g1.cap_id_exists"   # 与 gates.yaml 中的 id 对应，用于自验证

def run(context: dict, config: dict | None = None) -> list[dict]:
    """
    执行规则检查。

    Parameters
    ----------
    context : dict
        由 gate_context.py 构建的共享上下文，包含：
        - manifest      : dict  — 已解析的 checklist.data.json (Schema v2.0)
        - catalog       : dict  — 已解析的 capability-catalog.yaml
        - quarantine    : dict  — 已解析的 .gates/quarantine.yaml 白名单
        - changed_files : list  — 变更文件列表（POSIX 风格路径）
        - spec_version  : str   — 当前规范版本号 ("2.0.0")
        - mode          : str   — "pr" 或 "nightly"
        - now_utc       : datetime — 当前 UTC 时间基准

    config : dict | None
        从 gates.yaml 中该规则的 config 字段注入（可选）

    Returns
    -------
    list[Finding]
        Finding 是 dict，结构如下：
        {
            "rule_id":    str,              # 规则 ID
            "severity":   "error"|"warning"|"info",
            "entry_id":   str | None,       # 涉及的 checklist 条目 ID（可无）
            "display_id": int | None,       # 涉及的条目编号（可无）
            "config_id":  str | None,       # 涉及的配置实例 ID（可无）
            "file_path":  str | None,       # 涉及的文件路径（可无）
            "message":    str,              # 人类可读的描述
        }
        空列表 = 全部通过。
    """
    findings = []
    known_ids = set(context["catalog"].get("capabilities", {}))
    for entry in context["manifest"]["entries"]:
        for cap_id in entry.get("required_capabilities", []):
            if cap_id not in known_ids:
                findings.append({
                    "rule_id":    RULE_ID,
                    "severity":   "error",
                    "entry_id":   entry["id"],
                    "display_id": entry["display_id"],
                    "config_id":  None,
                    "file_path":  None,
                    "message":    f"引用未声明的能力 ID: {cap_id}",
                })
    return findings
```

### 接口约束

| 约束 | 说明 |
|---|---|
| `run()` 是唯一公开入口 | 不允许依赖全局状态或 `sys.argv` |
| 必须是**纯函数语义** | 不修改 context，不写文件，不 print，无副作用 |
| 超时控制由执行器负责 | 规则内不设 timeout，由 `run_gates.py` 注入信号 |
| 异常自动捕获 | 规则内抛出的未捕获异常由执行器捕获，转为 `severity: error` Finding |

---

## 五、`run_gates.py` 执行器行为规范

### 命令行接口

```
python .gates/run_gates.py [选项]

选项：
  --mode {pr,nightly}         触发模式（默认 pr）
  --gate {1,2,3,4}            只运行指定 Gate（可多次指定）
  --rule RULE_ID              只运行指定规则（调试用）
  --changed-files FILE        包含 git diff 文件列表的文本文件（每行一个路径）
  --allow-empty-diff          在 PR 模式下显式允许变更文件列表为空
  --output-json FILE          将结构化报告写入 JSON 文件
  --no-fail                   有错误时也不以非零退出码退出（调试用）
```

### 执行流程

```
1. 加载 gates.yaml，过滤出当前 mode 生效的规则；若生效规则数为 0，退出码 2 报错
2. 构建共享上下文 context（载入 manifest、catalog、quarantine.yaml、UTC 时间）
3. 若有 changed-files，载入变更文件列表并正规化为 POSIX 格式；若 PR 模式下文件为空且未指定 --allow-empty-diff，报错退出
4. 对每条规则：
   a. 检查 trigger_paths（若配置）—— 若变更列表与 trigger 无交集，跳过此规则（输出 SKIP）
   b. 计算当前 mode 下的有效 severity（考虑 mode_severity_override）
   c. 动态加载规则模块，若加载失败以退出码 2 报错
   d. 调用 rule.run(context, config)，捕获单规则异常转换为 severity: error 的 Finding
   e. 收集 Finding 列表
5. 汇总所有 Finding，按 (gate, rule_id, display_id, message) 稳定排序
6. 输出结构化报告（stdout + 可选 JSON 文件）
7. 若存在 severity=error 的 Finding，以退出码 1 退出；否则退出码 0
```

### 退出码语义

| 退出码 | 含义 |
|---|---|
| `0` | 全部通过（允许有 warning/info） |
| `1` | 存在 `severity: error` 的 Finding（CI 阻断） |
| `2` | 执行器自身错误（YAML 解析失败、生效规则为0、模块加载失败等） |

---

## 六、结构化输出报告（`gate_report.json`）

```json
{
  "run_at": "2026-09-29T11:34:00Z",
  "mode": "pr",
  "spec_version": "2.0.0",
  "summary": {
    "total_rules": 16,
    "executed": 13,
    "skipped": 3,
    "passed": 12,
    "errors": 0,
    "warnings": 10,
    "infos": 0
  },
  "gate_summary": {
    "gate_1": { "status": "PASS",  "executed": 10, "errors": 0, "warnings": 10 },
    "gate_2": { "status": "SKIP",  "reason": "no pal/targets/osal changes in PR diff" },
    "gate_3": { "status": "PASS",  "executed": 1, "errors": 0 },
    "gate_4": { "status": "INFO",  "impact_entries": [23, 64, 83] }
  },
  "findings": [
    {
      "rule_id":    "g1.can_check_mark",
      "severity":   "warning",
      "entry_id":   "esp.get_started.blink",
      "display_id": 1,
      "config_id":  "wasm_sim_standard",
      "file_path":  null,
      "message":    "处于存量债务隔离区白名单中（TTL 至 2026-10-13T23:59:59Z）"
    }
  ]
}
```

---

## 七、Gate 1 全量规则清单（对齐 Schema v2.0）

| 规则 ID | 模块 | 严重度(PR/Nightly) | 描述与判定逻辑 |
|---|---|---|---|
| `g1.path_unique` | `g1_path_unique` | error / error | `upstream_path` 在全量 478 条中严禁重复 |
| `g1.cap_id_exists` | `g1_cap_id_exists` | error / error | `required_capabilities` 中每个 ID 必须在 catalog 中已声明 |
| `g1.id_format` | `g1_id_format` | error / error | `id` 必须符合正则 `^esp\.[a-z0-9_]+(\.[a-z0-9_]+)+$` |
| `g1.version_alignment` | `g1_version_alignment` | error / error | `written_at_spec_version` 必须全量统一为 `"2.0.0"` |
| `g1.execution_configs` | `g1_execution_configs` | error / error | 每个条目 `executions` 数组必须非空；`config_id`、`backend`、`target_soc`、`profile` 合法 |
| `g1.orthogonal_states` | `g1_orthogonal_states` | error / error | `scope.inclusion`、`scope.schedule`、`audit.verdict` 状态五维正交，禁止未审先排 |
| `g1.can_check_mark` | `g1_can_check_mark` | **warning** / error | 针对 `delivery_state=verified` 校验六要素；对 `.gates/quarantine.yaml` 内条目在 TTL 前报 warning，白名单外或 TTL 逾期报 error 强阻断 |
| `g1.quarantine_ttl` | `g1_quarantine_ttl` | error / error | 独立扫描 `quarantine.yaml`，当前 UTC 时间超过 `grace_period_expires` 立即阻断 |
| `g1.sla_evidence` | `g1_sla_evidence` | error / error | `scope.inclusion=out_of_scope` 或 `expected_rejection` 必须具备 `sla_error_symbol` 与 `exclusion_evidence` |
| `g1.auditor_required` | `g1_auditor_required` | error / error | `audit.verdict=audited` 时 `auditor` 非空且 `audited_configs` 数组显式覆盖声明的配置 |

---

## 八、Gate 2 规则清单

| 规则 ID | 触发条件 (`trigger_paths`) | 严重度 | 描述 |
|---|---|---|---|
| `g2.pal_naming` | `wink-micro-os/pal/**`, `wink-micro-os/targets/**`, `wink-micro-os/osal/**` 变更 | error | 新增符号不含器件/协议关键词黑名单（词库由 `gates.yaml` 中 `config.forbidden_patterns` 声明） |
| `g2.cap_has_owned_paths` | `capability-catalog.yaml` 变更 | error | 每个能力条目必须有非空 `owned_paths` 列表 |
| `g2.cap_cross_mcu` | `capability-catalog.yaml` 变更 | warning | `status=implemented` 的能力建议提供 `cross_mcu_evidence` 凭据 |

---

## 九、Gate 3 规则清单

| 规则 ID | 模块 | 配置 | 描述 |
|---|---|---|---|
| `g3.winkcli_lint` | `g3_winkcli_lint` | `packs: [layering, api, dal, isr, user_surface, wasm, i18n]` | 委托 `winkcli lint` 执行 7 pack 全量分层校验；CI 环境下若工具缺失强制报错阻断 |

---

## 十、Gate 4：`impact_scope.py` 独立工具与回归规范

### 用途

当底层代码、能力字典或场景脚本被修改时，基于 `capability-catalog.yaml` 的依赖图谱计算反向传递闭包，定位所有受影响的示例与配置，并自动触发定向 Headless 场景回归。

### 核心算法（反向传递闭包与防御性解析）

```python
def compute_impact_closure(changed_files: list[str], catalog: dict, manifest: dict) -> dict:
    """
    1. 根据 owned_paths 定位直接受影响原子能力
    2. 根据 depends_on (mandatory & conditional) 计算反向传递图闭包
    3. 匹配 manifest 中所有直接或间接依赖该能力链的示例与配置
    4. 对非代码变更（Catalog/Scenario/Device-Tree）执行全局或定向扩散
    """
    capabilities = catalog.get("capabilities", {})
    direct_caps = set()

    # 1. 匹配直接受影响能力
    norm_changed = [os.path.normpath(f).replace("\\", "/") for f in changed_files]
    for cap_id, cap_def in capabilities.items():
        for owned in cap_def.get("owned_paths", []):
            norm_owned = os.path.normpath(owned).replace("\\", "/")
            if any(fnmatch.fnmatch(f, norm_owned) for f in norm_changed):
                direct_caps.add(cap_id)

    # 2. 构造反向依赖有向图并递归扩散
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

    # 3. 收集受影响示例与配置
    impact_entries = []
    for entry in manifest["entries"]:
        reqs = set(entry.get("required_capabilities", []))
        if reqs & closure_caps:
            impact_entries.append(entry["display_id"])

    # 4. 判断调度策略
    pr_inline = len(impact_entries) <= 30
    return {
        "affected_capabilities": sorted(list(closure_caps)),
        "impact_entries": sorted(impact_entries),
        "pr_inline": pr_inline,
    }
```

### 输出语义与测试调度闭环

| `pr_inline` | 含义 | CI 动作与状态闭环 |
|---|---|---|
| `true` | 受影响示例 ≤30 条 | PR 内立即调用 Headless 场景回归执行器逐项重测；**测试断言失败直接退出码 1 阻断 PR** |
| `false` | 受影响示例 >30 条 | 生成 Nightly 待重测清单；受波及条目在当次构建中标记为 **`stale`**，杜绝以旧凭证冒充有效 |

---

## 十一、CI 挂载方式（目标状态）

```yaml
# .github/workflows/esp_idf_ci.yml 或 pr.yml
- name: Run ESP-IDF Gate System (PR mode)
  shell: bash
  run: |
    # 提取 PR 相对基线的分支变更文件
    git fetch origin ${{ github.base_ref }} --depth=1
    git diff --name-only origin/${{ github.base_ref }}...HEAD > /tmp/changed_files.txt
    
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

---

## 十二、扩展约定

### 新增规则流程

1. 在 `rules/` 建立 `g<N>_<name>.py`，实现 `run(context, config) -> list[Finding]`；
2. 在 `gates.yaml` 的 `rules:` 下新增一行注册条目；
3. 运行 `python .gates/run_gates.py --rule g<N>.<name>` 验证规则逻辑；
4. Commit。**无需修改其他任何现有文件。**

### 跨 Vendor 复用

`.gates/` 目录的设计是 vendor 无关的。若 `cms8s78xx`、未来新增的 `esp_idfv70` 等 vendor 目录需要类似门禁，可：
- 复制 `run_gates.py` + `gate_context.py`（不变）；
- 复制 `rules/`（可选择性复用 Gate 1~3，按需定制 Gate 2 的 PAL 路径）；
- 为该 vendor 单独维护 `gates.yaml`（修改数据源路径与规则选取）。
