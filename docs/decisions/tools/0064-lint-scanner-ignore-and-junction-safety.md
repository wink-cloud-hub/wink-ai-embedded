# ADR-0064：仓库扫描器的忽略规则与 junction/符号链接安全（本地 internals 通道适配）

| 项 | 内容 |
|---|---|
| 状态 | **Proposed（提议中，待 wink-tools 评审签发）** |
| 日期 | 2026-09-13（提议） |
| 触发 | 在挂载本地 internals 通道（`docs/.internals` 为 Windows junction，指向含 `node_modules`/`.git` 的完整姊妹工作区）的 `wink-ai-embedded` 工作区运行 `wink.py test`，扫描阶段抛 `FileNotFoundError (WinError 3)` 并终止整个测试流程 |
| 影响范围 | wink-tools 全部仓库级扫描器（如 `tools/lint/check_pt_variables.py`）及其忽略规则；CI 与开发者本地流程 |
| 决策者 | 工具链组 |
| 关联 | `docs/AGENTS.md`（本地 AI 深度研发通道）、ADR-0043（分层 lint） |

---

## 背景（Context）

`wink-ai-embedded/docs/.internals` 是受 `.gitignore` 保护的本地 junction（Windows junction 不是 symlink，`os.walk` 默认会递归进入），指向姊妹工作区根（含 `packages/*`、`node_modules`、`.git` 等）。

现行仓库级扫描器在遍历时：

1. 不读取 `.gitignore`，进入 `docs/.internals`；
2. 继续深入 `node_modules/`、`.git/` 等非源码目录；
3. 遇到其中失效的符号链接/reparse point 时直接抛异常，**终止整个 `wink.py test`**，造成"本地开发通道存在即无法跑门禁"的可用性缺陷（与产品代码无关）。

触发实证（2026-09-13）：扫描 `.internals/.git/wt-preboard/node_modules/.bun/...` 下的失效路径导致 `test` 阶段整体失败。

## 决策结论（Decision）

1. **忽略规则**：所有仓库级扫描器必须至少跳过 `.git/`、`node_modules/`，并尊重仓库 `.gitignore`（含明确列出的本地通道路径，如 `docs/.internals/`）；忽略规则需集中配置、可审计，禁止散落各扫描器各自硬编码。
2. **reparse point 安全**：默认**不跟随** junction / symlink；无法判定类型时跳过并输出一次汇总告警（不得静默）。
3. **walk 错误 fail-soft**：单个目录/条目读取失败记录 warning 并继续；仅扫描器自身的配置错误（如忽略规则语法错误）允许 fail-fast。
4. **可观测性**：报告中被忽略的顶层路径清单与因错误跳过的条目数，便于审计"没有静默漏扫"。

## 后果与约束（Consequences & Constraints）

- 不改动任何产品代码与构建行为；扫描覆盖面显式收窄（以忽略清单为准）。
- 本地 internals 通道与仓库级门禁可共存：挂载状态下 `wink.py test` 全流程通过。
- 忽略清单本身成为契约：新增需扫描的本地路径必须进配置，而不是放宽规则。

## 遵循与后续（Compliance & Follow-up）

- wink-tools 实现：忽略规则集中配置 + junction 安全遍历 + fail-soft + 报告字段；新增单测覆盖"junction / 失效链接 / 忽略路径"。
- 验收：在挂载 `docs/.internals` 的 `wink-ai-embedded` 工作区执行 `wink.py test`（含保护带 Python/C lint 扫描）全程通过。
- 本仓动作：无（等待 wink-tools 交付后验证；临时清理本地僵尸目录不作为长期方案）。

---

*本 ADR 状态变更请在此记录：*
- 2026-09-13：Proposed（随本地 internals 通道 `wink.py test` 阻塞问题提出）
