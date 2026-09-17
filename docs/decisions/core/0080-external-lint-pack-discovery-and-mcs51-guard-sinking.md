# ADR-0080: Lint 外部扩展包动态发现与 MCS-51 守卫下沉

| 项 | 内容 |
|---|---|
| 决策文件名 | `0080-external-lint-pack-discovery-and-mcs51-guard-sinking.md` |
| 归属系统 | `wink-tools`（引擎） + `wink-micro-os/frameworks/mcs51`（规则） |
| 状态 | **Accepted** |
| 关联 | ADR-0004（静态分发）、ADR-0043（分层门禁）、ADR-0070（mcs51 隔离） |
| 实施计划 | 工具链内部计划（`dynamic-lint-pack-discovery-and-mcs51-guard-plan`） |

---

## 1. 背景

`wink lint` 是唯一门禁入口，但 8051 专属规则（Keil 方言、`sbit`/`sfr`、Wasm 仿真行为差异 B-01/B-02）硬编码在通用工具仓 `wink-tools` 中：臃肿、与嵌入式 SDK 版本脱节，且 `tools/cli/commands/test.py` 以 subprocess 散落直调单体脚本。

## 2. 决策

1. **引擎动态扩展**：`runner.py` 自动发现 `frameworks/*/tools/lint/lint_*.py`（优先级 CLI `--lint-paths` > `WINK_LINT_PATHS` > 约定目录）；未知 `--pack` 直接 exit(2) 防 CI 假绿；加载失败产生 `LINT-EXT-LOAD-FAIL` Finding。
2. **规则下沉**：`mcs51_safety`（语言/硬件正确性，FilePack + `applies_to` 认领制）与 `mcs51_sim_compat`（B-01/B-03，单 GlobalPack 内两阶段）收敛至 `frameworks/mcs51/tools/lint/`，组名 `mcs51_all`，默认关闭、显式触发。
3. **隔离守卫留守**：`mcs51_isolation`（ADR-0070）与 `legacy_arduino` 留在 `wink-tools` 核心（`layering` 组常开）——它们防内核反向依赖 sandbox，与外部 Pack 方向相反；被检查对象不得自带守卫（运动员兼裁判）。
4. **精度策略**：Keil 方言归一化 + SFR 全集（多芯片头文件并集 + 用户 `sbit` + 标准核心回退）+ 循环五判（全过才报，FN 优于 FP）；`REENTRANT-OVERLAY` 与 B-01 首版 warning + Baseline，收敛后再收紧为 error；宿主单测线束显式排除出 scope。

## 3. 后果与约束

- 外部 Pack 仅用 Python 3.10+ 标准库 + engine 接口；`exec_module` 信任边界限本地检出仓。
- `wink test` 经 `run_lint()` API 收敛（`test.py` 不再 subprocess 直调）；`check_mcs51_safety.py` 保留一版 deprecation shim 后删除。
- 活规范回写：`07-platform-governance/coding-conventions.md` §6（MCS-51 Lint 门禁）。
