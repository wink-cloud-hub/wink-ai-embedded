# ADR-0090：集中式可插拔门禁系统优于分散脚本（ESP-IDF 分类管治法典配套）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-09-29（提议）/ 2026-09-29（采纳） |
| 触发 | [2026-09-29 ESP-IDF 分类规范评审](../../reviews/esp32/2026-09-29-esp-idf-classification-spec-review.md) §4 指出门禁逻辑分散于多脚本，缺乏统一入口与规则注册机制，长期存在"没人知道有哪些检查"的维护失控风险 |
| 影响范围 | `wink-micro-app/vendor/esp_idfv61/.gates/`（新增目录）；`checklist.data.json`、`capability-catalog.yaml`（数据 SSOT）；CI 配置（`.github/workflows/`）；`winkcli lint`（Gate 3 委托） |
| 决策者 | 架构委员会 & 用户 |
| 关联技术设计 | [`docs/zh/tech-designs/esp32/esp-idf-classification-gate-system.md`](../../zh/tech-designs/esp32/esp-idf-classification-gate-system.md) |
| 关联实施计划 | [`2026-09-29-esp-idf-gate-system-implementation-plan.md`](../../implementation-plans/esp32/2026-09-29-esp-idf-gate-system-implementation-plan.md) |
| 关联规范 | [`CLASSIFICATION-SPEC.md`](../../../wink-micro-app/vendor/esp_idfv61/CLASSIFICATION-SPEC.md) v1.1 §四 CI Gate 1~4 定义 |

---

## 背景（Context）

在 `CLASSIFICATION-SPEC.md` v1.1 正式确立了**四道 CI 门禁（Gate 1~4）**之后，门禁的落地实现出现了**两种可选路径**：

**路径 A（现状）：脚本级分散实现**
- 各 Gate 的校验逻辑内嵌于各自的功能脚本中（如 `generate_checklist_v1_1.py` 内含 Gate 1 逻辑）；
- 新增规则需要修改已有脚本；
- CI 配置中需要维护多条独立命令及其依赖顺序；
- 随时间累积，无法从单一地方获取"系统所有门禁的全貌"。

**路径 B（提案）：集中式可插拔门禁系统**
- 所有门禁规则在 `gates.yaml` 统一声明注册；
- 每条规则是 `rules/g*.py` 中实现标准接口的独立插件；
- `run_gates.py` 是唯一 CI 入口，按模式（PR / Nightly）加载并执行规则；
- 新增规则只需建一个 `.py` + `gates.yaml` 加一行注册，无需修改现有代码。

---

## 驱动力（Forces）

1. **规则数量增长确定性**：四道 Gate 的全部规则预计共 15~20 条，且随项目演进会持续新增；分散实现会导致规则散落在 5+ 个脚本中，无法快速定位；
2. **CI 的稳定性诉求**：CI YAML 应该只知道"运行门禁"，而不需要了解门禁由哪些规则组成；规则的增减不应导致 CI 配置变更；
3. **严重度分档的复杂性**：同一条规则在 PR 模式和 Nightly 模式下的严重度不同（如 `g1.assets_sha256` 在 PR 中是 warning，在 Nightly 中是 error）；这种逻辑若内嵌于脚本，随时间变成 if/else 泥潭；
4. **可读性与可审计性**：项目的"有哪些质量门禁"应该是新成员可以通过阅读一个文件就能完整理解的。

---

## 决策（Decision）

**采纳路径 B：在 `.gates/` 目录下建立集中式可插拔门禁系统。**

架构要素：

```
wink-micro-app/vendor/esp_idfv61/.gates/
├── gates.yaml        # 门禁注册表（单一事实来源）
├── run_gates.py      # 统一执行入口（CI 唯一调用点）
└── rules/            # 插件目录（每文件一条规则）
    ├── g1_*.py       # Gate 1: SSOT 数据完整性
    ├── g2_*.py       # Gate 2: PAL 防膨胀
    ├── g3_*.py       # Gate 3: 分层 Lint（委托 winkcli）
    └── g4_*.py       # Gate 4: 依赖反向影响闭包
```

每条规则的标准接口：

```python
def run(context: dict) -> list[dict]:
    """返回 Finding 列表；空列表 = 全部通过"""
    ...
```

`gates.yaml` 声明每条规则的 `id`、`gate`、`severity`、`modes`、`trigger_paths`，所有配置集中于此，执行器通过插件加载机制按需调用。

---

## 备选方案（Alternatives Considered）

### 备选 1：JSON Schema 方案（jsonschema 库驱动）
用标准 JSON Schema 替代自定义 Python 校验。

**否决理由**：JSON Schema 擅长结构性约束（类型、必填字段），但无法表达跨文件关联约束（如"能力 ID 必须在另一个 YAML 文件中存在"）、磁盘存在性校验、`git diff` 触发逻辑等业务语义规则。需要搭配大量自定义脚本，反而比直接写 Python 更绕。

### 备选 2：Makefile + Shell 脚本
用 Makefile target 组织各道门禁。

**否决理由**：Windows 平台下 `make` 工具链依赖问题复杂（MSYS2、WSL、Git Bash 的差异），与项目现有的 PowerShell + Python 技术栈不一致，且无法实现 `gates.yaml` 声明式配置的优雅性。

### 备选 3：pre-commit 框架
用 `.pre-commit-config.yaml` 组织规则。

**否决理由**：pre-commit 适合编辑器/提交时的轻量检查，但 Gate 4 的依赖影响闭包计算、`git diff --name-only` 上下文注入、JSON 结构化输出等需求超出 pre-commit 的设计边界；且 Nightly 全量触发场景与 pre-commit 的 hook 模型不符。

---

## 后果与约束（Consequences）

### 正向后果
- **可读性**：`gates.yaml` 一屏纵览全部门禁，新成员 5 分钟可掌握系统全貌；
- **可扩展性**：新增规则无需修改现有代码，开闭原则；
- **CI 稳定性**：CI YAML 中命令固定为 `python .gates/run_gates.py --mode pr`，规则增减透明；
- **可测试性**：每条规则是独立函数，可以单独单元测试；
- **跨项目复用**：`.gates/` 目录和插件接口可以平移至 `cms8s78xx`、未来新增的 vendor 目录。

### 负向约束
- **引入初始复杂度**：相比直接在 `generate_checklist_v1_1.py` 中写 if/else，初始建立框架有一次性投入成本（约 2~4 小时）；
- **Python 环境依赖**：`pyyaml` 需要安装；已在项目中使用，无额外新增；
- **插件加载机制**：`importlib.import_module` 动态加载有调试栈追踪略深的缺点，需确保规则文件名与 `gates.yaml` 中 `module` 字段严格对应。

### 回写义务
本 ADR 采纳后，必须：
1. ✅ 更新 `CLASSIFICATION-SPEC.md` §四 CI Gate 说明，指向 `.gates/gates.yaml` 作为门禁注册表的单一来源；
2. ✅ 在 `capability-catalog.yaml` 的 `README` 注释中说明 Gate 2 的 `owned_paths` 校验由 `g2_cap_has_owned_paths.py` 执行；
3. ⬜ CI YAML 更新至统一入口（在实施计划完成后执行）。

---

## 关联决策

- [ADR-0012](../core/0012-contract-honesty-over-silent-degradation.md)：PAL 契约诚实原则（Gate 2 门禁所维护的核心价值）
- [ADR-0004](../core/0004-static-dispatch-vs-runtime-ops.md)：静态分发模式（Gate 3 分层 Lint 所维护的架构边界）
- [ADR-0066](../core/0066-pwm-duty-fixed-point.md)：PWM 占空比定点（Gate 3 `api` pack 的校验对象之一）
