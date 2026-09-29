# ESP-IDF 分类管治门禁系统实施计划

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260929-ESP-IDF-GATE-SYSTEM-IMPL |
| 状态 | **📋 待执行（Ready to Execute）** |
| 日期 | 2026-09-29 |
| 前置条件 | ① `checklist.data.json`（478条）已生成 ✅  ② `capability-catalog.yaml` 已创建 ✅  ③ `generate_checklist_v1_1.py` 内含 Gate 1 基础校验 ✅ |
| 决策依据 | [ADR-0090](../../decisions/unisim/0090-centralized-pluggable-gate-system.md) |
| 技术设计 | [esp-idf-classification-gate-system.md](../../zh/tech-designs/esp32/esp-idf-classification-gate-system.md) |
| 估算工时 | 约 6~8 小时（单次连续执行完成） |

---

## 验收标准（全局）

- [ ] `python .gates/run_gates.py --mode pr` 可无错运行，输出结构化 JSON 报告
- [ ] `python .gates/run_gates.py --mode nightly` 可无错运行，Gate 1 全部 11 条规则生效
- [ ] Gate 1 (11条)、Gate 2 (3条)、Gate 4 (影响分析) 均有独立单元测试，覆盖正向（合规）和负向（违规）用例
- [ ] `--rule g1.cap_id_exists` 单规则调试模式可用
- [ ] `impact_scope.py` 可接受 `--changed-files` 或自动推断 git diff 输出影响闭包 JSON
- [ ] 跨平台路径兼容：所有文件路径在 Windows 与 Linux 环境下均被正规化为 POSIX 风格（`/`）
- [ ] `generate_checklist_v1_1.py` 原有逻辑不受影响，两套门禁系统在过渡期可并存
- [ ] 许可门禁 `check_license_map.py` 通过（所有新增文件标注 `Apache-2.0` SPDX）

---

## 任务拆分

### 第一步：建立目录骨架与共享基础设施

**目标**：建立 `.gates/` 目录，实现执行器骨架与增强型上下文构建器，验证插件加载机制可用。

**子任务：**

#### T1.1 建立 `.gates/` 目录骨架

```
wink-micro-app/vendor/esp_idfv61/.gates/
├── gates.yaml          ← 按技术设计 §三 Schema 填写所有规则声明
├── run_gates.py        ← 执行器骨架（按 §五 行为规范实现）
├── gate_context.py     ← 共享上下文构建器（含 Git diff 自动探测与路径正规化）
├── impact_scope.py     ← Gate 4 独立工具（按 §十 算法实现）
├── rules/
│   ├── __init__.py
│   ├── g1_*.py         ← Gate 1 规则（共 11 条）
│   ├── g2_*.py         ← Gate 2 规则（共 3 条）
│   └── g3_*.py         ← Gate 3 规则（共 1 条）
└── reports/
    └── .gitkeep
```

#### T1.2 实现 `gate_context.py`（增强型共享上下文）

负责构建传递给每条规则的共享 `context` dict，必须包含以下防御性增强：
1. **自动 Git diff 探测**：若 `--changed-files` 未指定，自动调用 `git diff --name-only HEAD` 或 `git status --porcelain` 探测变更；若不在 Git 仓库内则返回空列表并记录日志；
2. **POSIX 路径正规化**：所有文件路径（包括 Windows 反斜杠）自动调用 `normpath(p).replace("\\", "/")`，杜绝跨平台匹配失效；
3. **Workspace Root 解析**：定位嵌入式仓库根目录，供文件真实存在性检查规则安全拼接路径。

```python
def build_context(manifest_path, catalog_path, changed_files_path, mode) -> dict:
    return {
        "manifest":       json.load(...),     # checklist.data.json
        "catalog":        yaml.safe_load(...), # capability-catalog.yaml
        "changed_files":  [...],               # POSIX 正规化文件列表
        "workspace_root": "...",               # 仓库根绝对路径
        "spec_version":   "1.1.0",
        "mode":           mode,
    }
```

#### T1.3 实现 `run_gates.py` 执行器骨架

- 解析命令行参数（`--mode`, `--gate`, `--rule`, `--changed-files`, `--output-json`, `--no-fail`）
- 加载 `gates.yaml` 并过滤当前 mode 有效的规则
- 动态加载规则模块，支持按 `rule.run(context, config)` 执行
- 捕获单规则异常转换为 `severity: error` 的 Finding
- 汇总 Finding，按技术设计 §六 格式构建 JSON 报告
- 按退出码语义退出（0=通过/仅警告，1=存在阻断错误，2=执行器异常）

**验收**：`python .gates/run_gates.py --mode pr`（无规则注册时）输出空报告，退出码 0。

---

### 第二步：实现 Gate 1 全量规则（11条）

**目标**：Gate 1 完整覆盖技术设计 §七 的 11 条规则，全面消除 SSOT 数据完整性缺口。

每条规则对应一个 `rules/g1_*.py` 文件，实现 `run(context, config) -> list[Finding]`。

| 子任务 | 规则文件 | 严重度 (PR / Nightly) | 核心逻辑要点 |
|---|---|:---:|---|
| T2.1 | `g1_path_unique.py` | error / error | 遍历 entries，用 `dict` 记录已见 `upstream_path`，发现重复 → error |
| T2.2 | `g1_cap_id_exists.py` | error / error | 比对 `required_capabilities` 与 `catalog["capabilities"]` 键集合，发现未声明能力 → error |
| T2.3 | `g1_assets_sha256.py` | warning / error | `delivery.state=verified` 时检查 `assets_sha256.{device_tree,js,wasm}` 均非空 |
| T2.4 | `g1_oos_has_evidence.py` | error / error | `status=out_of_scope_product` 时检查 `exclusion_evidence`；存量 `sla_block_symbols` 为空在 PR 降级为 warning，Nightly 强制 error |
| T2.5 | `g1_verified_has_negative.py` | warning / error | `delivery.state=verified` 时检查 `acceptance.negative_cases` 至少 1 条 |
| T2.6 | `g1_no_pending_verified.py` | error / error | `scope_and_maturity.status=pending_audit` 且 `delivery.state=verified` → error |
| T2.7 | `g1_id_format.py` | error / error | `re.match(r"^esp\.[a-z0-9_\-.]+$", entry["id"])` 未匹配 → error |
| T2.8 | `g1_version_alignment.py` | info / warning | `entry["written_at_spec_version"] != context["spec_version"]` → info |
| T2.9 | `g1_scenario_exists.py` | warning / error | `delivery.state=verified` 时检查 `os.path.join(workspace_root, app_dir, scenario_path)` 物理磁盘真实存在 |
| T2.10 | `g1_auditor_required.py` | error / error | `audit.verdict=audited` 时 `audit.auditor` 必须非空且非 "TBD"/"none" |
| T2.11 | `g1_soc_matrix_complete.py` | error / error | `soc_matrix` 必须显式包含 `esp32`, `esp32s3`, `esp32c3`, `esp32c6` 四个芯片定义且状态合法 |

**验收**：

```bash
# PR 模式（兼容过渡期存量数据：未填哈希、缺失场景文件、缺负例提示 warning，不阻断）
python .gates/run_gates.py --mode pr --output-json reports/gate_report.json
# 预期：0 errors，若干 warnings（已实证条目的历史存量缺口），退出码 0

# Nightly 模式（严格全量校验，阻断所有缺口）
python .gates/run_gates.py --mode nightly --output-json reports/gate_report_nightly.json
# 预期：发现存量数据中的哈希、场景文件及负例缺口，退出码 1

# 单规则调试
python .gates/run_gates.py --rule g1.cap_id_exists
python .gates/run_gates.py --rule g1.soc_matrix_complete
python .gates/run_gates.py --rule g1.auditor_required
# 预期：0 findings，退出码 0
```

---

### 第三步：实现 Gate 2 规则（3条）与触发逻辑

**目标**：Gate 2 规则仅在 PAL/targets/osal/catalog 相关文件变更时触发（`trigger: git_diff`）。

| 子任务 | 规则文件 | 核心逻辑要点 |
|---|---|---|
| T3.1 | `g2_pal_naming.py` | 对 `context["changed_files"]` 中匹配 `wink-micro-os/pal/**`、`wink-micro-os/targets/**`、`wink-micro-os/osal/**` 的文件，按 `config.forbidden_patterns` 配置扫描黑名单关键词（默认 `["ws2812", "rgb", "pixel", "necir", "at24", "i2c_addr", "touch_pad", "servo", "oled"]`） |
| T3.2 | `g2_cap_has_owned_paths.py` | 遍历 `catalog["capabilities"]`，`owned_paths` 为空列表或缺失字段 → error |
| T3.3 | `g2_cap_cross_mcu.py` | `status=implemented` 的能力缺少 `cross_mcu_evidence` 凭据 → warning |

**run_gates.py 对 trigger 与配置的处理**：

```python
if rule.get("trigger") == "git_diff":
    trigger_paths = rule.get("trigger_paths", [])
    # 路径已在 gate_context 中经过 POSIX 正规化
    matched = [
        f for f in context["changed_files"]
        if any(fnmatch.fnmatch(f, pat) for pat in trigger_paths)
    ]
    if not matched:
        log(f"SKIP {rule['id']}: no matching files in diff")
        continue
```

**验收**：

```bash
# 无底座变更时 Gate 2 全部 SKIP
echo "" > /tmp/no_changes.txt
python .gates/run_gates.py --gate 2 --changed-files /tmp/no_changes.txt
# 预期：gate_2 status=SKIP

# 模拟 PAL/targets/osal 文件变更时触发
echo "wink-micro-os/pal/src/pal_wasm_uart.c" > /tmp/pal_changes.txt
python .gates/run_gates.py --gate 2 --changed-files /tmp/pal_changes.txt
```

---

### 第四步：实现 Gate 3（winkcli 委托与容错降级）

**目标**：将现有 `winkcli lint` 调用纳入门禁系统，并配置全量 6 pack，兼顾环境容错与渐进就绪。

| 子任务 | 说明 |
|---|---|
| T4.1 | 实现 `g3_winkcli_lint.py`：优先检查 `shutil.which("winkcli")`，未安装时以 warning 提示跳过 |
| T4.2 | 探测 `winkcli` 支持的 pack；若环境仅支持 `layering, api`，对未就绪 pack 优雅降级并在报告中标记 info |
| T4.3 | 在 `gates.yaml` 中配置 `packs: [layering, api, dal, isr, user_surface, wasm]` 与超时保护（60s） |
| T4.4 | 与 `winkcli` 维护者对齐 `dal, isr, user_surface, wasm` 规则文件的补全合并时间线 |

**验收**：

```bash
python .gates/run_gates.py --gate 3
# 预期：winkcli layering + api 两个现有 pack 通过，未就绪 pack 优雅跳过不崩溃
```

---

### 第五步：实现 Gate 4 影响闭包工具

**目标**：实现 `impact_scope.py` 独立工具与 `g4_impact_scope.py` 规则，支持 POSIX 路径与阈值切分。

**核心算法实现（参考技术设计 §十）**：

```python
# impact_scope.py
def compute_impact(changed_files: list[str], catalog: dict, manifest: dict) -> dict:
    # 确保 changed_files 全部正规化为 POSIX 格式
    norm_changed = [os.path.normpath(f).replace("\\", "/") for f in changed_files]

    affected_caps = set()
    for cap_id, cap in catalog["capabilities"].items():
        for owned_path in cap.get("owned_paths", []):
            norm_owned = os.path.normpath(owned_path).replace("\\", "/")
            if any(fnmatch.fnmatch(f, norm_owned) for f in norm_changed):
                affected_caps.add(cap_id)

    impact_entries = []
    for entry in manifest["entries"]:
        if any(c in affected_caps for c in entry.get("required_capabilities", [])):
            impact_entries.append(entry["display_id"])

    return {
        "affected_capabilities": sorted(affected_caps),
        "impact_entries":        sorted(impact_entries),
        "pr_inline":             len(impact_entries) <= 30,
    }
```

**验收**：

```bash
# 模拟 I2C PAL 文件变更
echo "wink-micro-os/pal/include/hal/pal_i2c.h" > /tmp/changes.txt
python .gates/impact_scope.py --changed-files /tmp/changes.txt
# 预期输出：
# affected_capabilities: ["cap.bus.i2c_master"]
# impact_entries: [23]
# pr_inline: true
```

---

### 第六步：单元测试体系与文档收尾

**目标**：Gate 1~4 全量覆盖独立单元测试（正向合规 + 负向违规 + 跨平台兼容）。

```
wink-micro-app/vendor/esp_idfv61/.gates/tests/
├── fixtures/
│   ├── minimal_manifest.json    # 极简合规数据（3条）
│   ├── minimal_catalog.yaml     # 极简合规 catalog
│   └── violations/              # 各类违规样本（ID格式错误、OOS缺证据、未声明能力等）
├── test_context.py              # 测试 Git 自动探测、POSIX 路径正规化
├── test_impact_scope.py         # 测试 Gate 4 影响闭包计算与阈值分档
└── test_rules/
    ├── test_g1_path_unique.py
    ├── test_g1_cap_id_exists.py
    ├── test_g1_assets_sha256.py
    ├── test_g1_oos_has_evidence.py
    ├── test_g1_verified_has_negative.py
    ├── test_g1_no_pending_verified.py
    ├── test_g1_id_format.py
    ├── test_g1_version_alignment.py
    ├── test_g1_scenario_exists.py
    ├── test_g1_auditor_required.py
    ├── test_g1_soc_matrix_complete.py
    ├── test_g2_pal_naming.py
    ├── test_g2_cap_has_owned_paths.py
    └── test_g2_cap_cross_mcu.py
```

**验收**：

```bash
cd wink-micro-app/vendor/esp_idfv61/.gates
pytest tests/ -v
# 预期：全部 15+ 个单元测试 PASS
```

---

## 风险与缓解

| 风险 | 可能性 | 影响 | 缓解措施 |
|---|:---:|:---:|---|
| `gates.yaml` 规则注册拼写错误导致规则静默跳过 | 中 | 高 | 执行器启动时做"已注册规则 vs 已加载模块"的一致性自检，不一致时以退出码 2 报错 |
| Windows 与 Linux 跨平台路径反斜杠导致 glob 匹配失败 | 高 | 高 | 在 `gate_context.py` 与 `impact_scope.py` 入口统一执行 `replace("\\", "/")` 强制正规化 |
| 本地开发者执行未指定 `--changed-files` 导致 Gate 2/4 失效 | 中 | 中 | `gate_context.py` 自动尝试执行 `git diff --name-only HEAD` 推断变更；若失败则优雅标记 SKIP |
| 存量 164 条 OOS 与 10 条 verified 存在历史数据未闭环项直接打挂 PR | 高 | 高 | 严格执行分级策略：`assets_sha256`、`scenario_exists`、`sla_block_symbols`、`negative_cases` 在 PR 模式一律降级为 warning，仅在 Nightly 模式阻断 |
| `winkcli` 未安装或不支持新 pack 导致 CI 崩溃 | 中 | 中 | `g3_winkcli_lint.py` 先做 `shutil.which` 探测，并对未实现 pack 增加降级捕获，不阻塞 PR |
| 过渡期两套门禁（`generate_checklist_v1_1.py` + `run_gates.py`）规则冲突 | 低 | 低 | 待 `run_gates.py` Gate 1 全部 11 条规则稳定并通过单元测试后，删除 `generate_checklist_v1_1.py` 中的 `validate_data()` 函数，彻底由 `run_gates.py` 接管 |

---

## 与现有脚本的过渡关系

```
当前状态（并存期）：
  generate_checklist_v1_1.py  → 内含 validate_data() 做 Gate 1 基础校验
  run_gates.py（新建）        → 接管 Gate 1 全量 (11条) + Gate 2 + Gate 3 + Gate 4

过渡目标（第六步完成后）：
  generate_checklist_v1_1.py  → 仅保留 Markdown 渲染逻辑，删除 validate_data()
  run_gates.py                → 唯一门禁入口
  
CI YAML 过渡：
  当前：python generate_checklist_v1_1.py --strict
  目标：python .gates/run_gates.py --mode pr --changed-files $CHANGED
```

---

## 完成标志

全部六步任务完成，且以下命令在本地和 CI 中稳定运行：

```bash
# 开发者日常（PR 快速模式，容错存量警告，0 errors）
python wink-micro-app/vendor/esp_idfv61/.gates/run_gates.py --mode pr

# 每夜全量（Nightly 严格模式，检出全部存量缺口）
python wink-micro-app/vendor/esp_idfv61/.gates/run_gates.py --mode nightly

# 调试单条规则
python wink-micro-app/vendor/esp_idfv61/.gates/run_gates.py --rule g1.soc_matrix_complete
python wink-micro-app/vendor/esp_idfv61/.gates/run_gates.py --rule g1.auditor_required
```

到达此状态后，本计划归档为 **Completed**，`gates.yaml` 成为长期演进入口。

