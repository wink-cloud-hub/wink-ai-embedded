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
spec_version: "1.1.0"          # 对齐 CLASSIFICATION-SPEC.md 版本
description: "..."

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
      packs: [layering, api, dal, isr, user_surface, wasm]
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
        - manifest      : dict  — 已解析的 checklist.data.json
        - catalog       : dict  — 已解析的 capability-catalog.yaml
        - changed_files : list  — git diff --name-only 结果（可能为空）
        - spec_version  : str   — 当前规范版本号
        - mode          : str   — "pr" 或 "nightly"

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
                    "file_path":  None,
                    "message":    f"引用未声明的能力 ID: {cap_id}",
                })
    return findings
```

### 接口约束

| 约束 | 说明 |
|---|---|
| `run()` 是唯一公开入口 | 不允许依赖全局状态或 `sys.argv` |
| 必须是**纯函数语义** | 不修改 context，不写文件，不 print |
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
  --output-json FILE          将结构化报告写入 JSON 文件
  --no-fail                   有错误时也不以非零退出码退出（调试用）
```

### 执行流程

```
1. 加载 gates.yaml，过滤出当前 mode 生效的规则
2. 若有 changed-files，加载变更文件列表
3. 对每条规则：
   a. 检查 trigger_paths（若配置）—— 若变更列表为空或无交集，跳过此规则（输出 SKIP）
   b. 计算当前 mode 下的有效 severity（考虑 mode_severity_override）
   c. importlib.import_module 加载规则模块
   d. 调用 rule.run(context, config)，捕获异常
   e. 收集 Finding 列表
4. 汇总所有 Finding，按 Gate 分组统计
5. 输出结构化报告（stdout + 可选 JSON 文件）
6. 若存在 severity=error 的 Finding，以退出码 1 退出；否则退出码 0
```

### 退出码语义

| 退出码 | 含义 |
|---|---|
| `0` | 全部通过（允许有 warning/info） |
| `1` | 存在 `severity: error` 的 Finding（CI 阻断） |
| `2` | 执行器自身错误（YAML 解析失败、文件不存在等） |

---

## 六、结构化输出报告（`gate_report.json`）

```json
{
  "run_at": "2026-09-29T11:34:00+08:00",
  "mode": "pr",
  "spec_version": "1.1.0",
  "summary": {
    "total_rules": 16,
    "executed": 13,
    "skipped": 3,
    "passed": 12,
    "errors": 0,
    "warnings": 1,
    "infos": 0
  },
  "gate_summary": {
    "gate_1": { "status": "PASS",  "executed": 11, "errors": 0, "warnings": 1 },
    "gate_2": { "status": "SKIP",  "reason": "no pal/targets/osal changes in PR diff" },
    "gate_3": { "status": "PASS",  "executed": 1, "errors": 0 },
    "gate_4": { "status": "INFO",  "impact_entries": [23, 64, 83] }
  },
  "findings": [
    {
      "rule_id":    "g1.assets_sha256",
      "severity":   "warning",
      "entry_id":   "esp.get_started.blink",
      "display_id": 1,
      "file_path":  null,
      "message":    "delivery.state=verified 但 assets_sha256 三件套未填写"
    }
  ]
}
```

---

## 七、Gate 1 全量规则清单

| 规则 ID | 模块 | 严重度(PR/Nightly) | 描述 |
|---|---|---|---|
| `g1.path_unique` | `g1_path_unique` | error / error | `upstream_path` 在全量 478 条中严禁重复 |
| `g1.cap_id_exists` | `g1_cap_id_exists` | error / error | `required_capabilities` 中每个 ID 必须在 catalog 中已声明 |
| `g1.assets_sha256` | `g1_assets_sha256` | **warning** / error | `delivery.state=verified` 时 `assets_sha256` 三件套非空 |
| `g1.oos_has_evidence` | `g1_oos_has_evidence` | error / error | `status=out_of_scope_product` 必须有 `exclusion_evidence`；存量 `sla_block_symbols` 为空在 PR 降级为 warning，Nightly 强制 error |
| `g1.verified_has_negative` | `g1_verified_has_negative` | **warning** / error | `delivery.state=verified` 时 `negative_cases` 至少 1 条 |
| `g1.no_pending_verified` | `g1_no_pending_verified` | error / error | `scope_and_maturity.status=pending_audit` 时 `delivery.state` 严禁为 `verified` |
| `g1.id_format` | `g1_id_format` | error / error | `id` 必须符合正则 `^esp\.[a-z0-9_\-.]+$` |
| `g1.version_alignment` | `g1_version_alignment` | **info** / warning | `written_at_spec_version` 应与当前规范版本一致 |
| `g1.scenario_exists` | `g1_scenario_exists` | **warning** / error | `delivery.state=verified` 时 `app_dir/scenario_path` 文件在物理磁盘上真实存在 |
| `g1.auditor_required` | `g1_auditor_required` | error / error | `audit.verdict=audited` 时 `auditor` 字段非空且非占位符 |
| `g1.soc_matrix_complete` | `g1_soc_matrix_complete` | error / error | `soc_matrix` 必须显式且完整包含 `esp32`, `esp32s3`, `esp32c3`, `esp32c6` 四个芯片定义 |

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
| `g3.winkcli_lint` | `g3_winkcli_lint` | `packs: [layering, api, dal, isr, user_surface, wasm]` | 委托 `winkcli lint` 执行 6 pack 全量分层校验 |

> **注**：当前 `winkcli` 仅配置了 `layering, api` 两个 pack。迁移至门禁系统后，逐步补全 `dal, isr, user_surface, wasm` 四个 pack 的规则文件。

---

## 十、Gate 4：`impact_scope.py` 独立工具规范

### 用途

当底层文件（如 `pal_wasm_i2c.c`）被修改时，自动计算"哪些示例必须重新验证"。

### 调用方式

```bash
# PR 模式：输出受影响的示例 display_id 列表
python .gates/impact_scope.py --changed-files changed.txt

# 也可通过 run_gates.py 的 g4.impact_scope 规则自动触发
```

### 核心算法

```
输入：changed_files（变更文件路径列表）

1. 加载 capability-catalog.yaml
2. 对每个 changed_file：
   for cap_id, cap in catalog["capabilities"]:
       if any(changed_file 匹配 cap.owned_paths glob):
           affected_caps.add(cap_id)

3. 加载 checklist.data.json
4. 对每个 entry：
   if any(cap_id in entry.required_capabilities for cap_id in affected_caps):
       impact_entries.add(entry.display_id)

5. 输出 JSON：
   {
     "affected_capabilities": ["cap.bus.i2c_master", ...],
     "impact_entries": [23, 64, 83, ...],
     "pr_inline": true/false    // 受影响 ≤30 条 → true（内联回归）
   }
```

### 输出语义

| `pr_inline` | 含义 | CI 动作 |
|---|---|---|
| `true` | 受影响示例 ≤30 条 | PR 内立即触发定向回归 |
| `false` | 受影响示例 >30 条 | 本次 PR 标记"影响范围大"，推至 Nightly 全量回归 |

---

## 十一、CI 挂载方式（目标状态）

```yaml
# .github/workflows/pr.yml（目标状态）
- name: Run Gate System (PR mode)
  run: |
    git diff --name-only HEAD~1 HEAD > /tmp/changed.txt
    python wink-micro-app/vendor/esp_idfv61/.gates/run_gates.py \
      --mode pr \
      --changed-files /tmp/changed.txt \
      --output-json gate_report.json
  continue-on-error: false

# .github/workflows/nightly.yml（目标状态）
- name: Run Gate System (Nightly full)
  run: |
    python wink-micro-app/vendor/esp_idfv61/.gates/run_gates.py \
      --mode nightly \
      --output-json gate_report_nightly.json
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
