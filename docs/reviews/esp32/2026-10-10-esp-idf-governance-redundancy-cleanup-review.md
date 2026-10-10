<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ESP-IDF 治理工程（.governance）冗余清理与架构稳定性验收评审

| 项 | 内容 |
|---|---|
| 日期 | 2026-10-10，Asia/Shanghai |
| 类型 / 结论 | 实施完成与质量验收评审；**Verified / 完全达标：冗余安全清理，架构收敛规范，全量 478 测试及门禁验证通过，用户资产 100% 完整保护** |
| 对应计划 | [ESP-IDF 治理工程冗余清理实施计划](../../implementation-plans/esp32/2026-10-09-esp-idf-governance-redundancy-cleanup-plan.md) (`v2.2`) |
| 关联技术设计 | [AFG-Engine 契约规格](../../zh/tech-designs/esp32/esp-idf-anti-false-green-verification-engine-contract.md)、[Loop 可靠性技术契约](../../zh/tech-designs/esp32/esp-idf-loop-reliability-contract.md) |
| 证据归档目录 | `C:\Users\77174\AppData\Local\Temp\wink-governance-cleanup-42246d14127c4532a17fba4dfc368dde` |

---

## 1. 验收结论

本次治理工程（`.governance`）冗余清理与代码规范化任务严格按照 `PLAN-20261009-ESP-IDF-GOVERNANCE-REDUNDANCY-CLEANUP` (v2.2) 方案执行，现已全面完成 Phase 0 至 Phase 4 所有任务：

1. **测试镜像去重完成**：经 `checks/test_dedup_audit.json` 100% 逐字节比对，安全删除了 `gates/tests/` 下 36 个重复测试文件，解决了 pytest 默认收集冲突；所有测试在 `tests/` 下规范组织，CI 工作流（`esp_idf_ci.yml`、`nightly.yml`）完成同步迁移。
2. **路径锚定与拓扑防卫就绪**：建立 `loop/harness/paths.py` 与 PathGuard 机制，彻底阻断 CWD 漂移引发的路径拼接错误与递归嵌套目录创建。
3. **架构收敛与单例防分裂**：服务内核抽取至 `loop/harness/` 与 `loop/services/`，`tools/` 仅保留无额外开销的薄别名转发 Shim；根治 `sys.modules["loop"]` 篡改与 `sys.path` 污染，收敛 `pyproject.toml` 中的 `pythonpath = ["."]`.
4. **全量用例与门禁实证**：
   - 全量回归测试：**478 passed in 21s**（0 failures, 0 errors, 0 skipped）。
   - AFG-Engine 元不变量测试：**31 passed**。
   - 依赖与导入契约 AST 动态测试：**5 passed**。
   - 路径与 PathGuard 单元测试：**6 passed**。
   - Gate 1：12 条规则全部执行，**12 PASS, 0 SKIP, 0 error**。
   - Gates PR：**16 executed, 8 skipped**（均由真实触发路径非命中驱动，符合预期），**0 error**。
   - SSOT 不变量门禁：`check_ssot_invariants.py` **PASSED**。
   - 许可检查门禁：`check_license_map.py` **PASSED**。
   - Headless 单场景确定性实证：`uart_echo` 仿真执行通过（虚拟时间 2000000µs，物理耗时 201ms，3 个 Assert/Bus 步骤全部 PASS）。
5. **工作区状态与用户资产保护**：用户原有修改及 MCS-51 并发任务文件得到严格保护，未发生覆盖或意外变更。

---

## 2. 详细执行与验证记录

### 2.1 Phase 0：基线保存与诊断准备
- 归档基线快照至 `$CleanupEvidence/baseline/`：
  - `workspace_state.json`：锁定 HEAD 提交、已修改文件、保护文件 SHA-256 和 SSOT 文件哈希。
  - `pre_diff.patch`：保存基线工作区 patch。
  - `tests_collected.txt`（467 项）与 `gates_tests_collected.txt`（399 项）。
  - `gate1.json`：Gate 1 基线运行结果（12 PASS）。
  - `environment_inventory.json` 与 `pip_freeze.txt`。

### 2.2 Phase 1：测试去重与 CI 迁移
- 逐文件比对 `gates/tests/` 与 `tests/`，生成 `test_dedup_audit.json`，证明 36 个测试文件及 3 个 fixture 文件完全逐字节相同。
- 通过 `git rm -r` 安全删除 `gates/tests/`。
- 更新 `pyproject.toml`：`testpaths = ["tests"]`，包发现明确收敛为 `include = ["loop*", "gates*", "cli*"]`。
- 同步更新 `.github/workflows/esp_idf_ci.yml` 和 `nightly.yml`，执行测试路径指向 `tests/`。
- 清理 `.governance` 内部历史空嵌套目录 `wink-micro-app/`。

### 2.3 Phase 2：路径收敛与 PathGuard 防重叠哨兵
- 实现 `loop/harness/paths.py`：
  - `resolve_workspace_root()`：基于标志文件锚定仓库根目录，优先支持外部传入与环境变量覆写。
  - `resolve_governance_root()`：锚定治理根目录，验证拓扑隶属关系。
  - `PathGuard`：严格防卫路径遍历与非法相对嵌套，在非法创建/写入前 Fail-Loud 报错。
- 升级 `loop/pipeline/runner.py` 与 `loop/services/admission_service.py` 使用标准路径解析。
- 新增 `tests/harness/test_paths_and_pathguard.py`，6 项针对性测试全部通过。

### 2.4 Phase 3：实现归一化与导入契约
- 服务抽取：
  - `loop/harness/idf_paths.py`：承接原 `esp_path_resolver.py`。
  - `loop/services/afg_verification.py`：承接原 `verify_afg_engine.py` 核心算法。
  - `loop/services/soc_triage.py`：承接原 `triage_soc_support.py` 核心算法。
  - 对应的 `tools/` 脚本转换为纯别名转发 Shim，保持既有 CLI 接口完全兼容。
- 消除单例分裂与导入篡改：
  - 改造 `tools/loop/agent.py`、`remediator.py`、`mutator.py`、`mutation_catalog.py` 为薄 Shim。
  - 移除 `tools/loop/__init__.py` 中对 `sys.modules["loop"]` 的侵入性注入。
  - 收敛 `pyproject.toml` 的 `pythonpath = ["."]`，移除 `conftest.py` 中的内部目录注入，全面改用全限定导入（`gates.*`, `loop.*`, `tools.*`）。
- 新增导入契约测试 `tests/test_governance_import_contract.py`：
  - AST 递归遍历 116 个核心 Python 文件，验证零逆向导入、零 `sys.path` 篡改。
  - 验证双向导入顺序恒等性（先 canonical 或先 legacy，`canonical.AgentSynthesizer is legacy.AgentSynthesizer` 恒成立）。
  - 验证共享状态与 Monkeypatch 跨导入透明传播。

### 2.5 Phase 4：全量实证与质量闭环
- **测试矩阵**：
  - 全量用例：478 passed in 21.10s。
  - 元不变量：31 passed。
  - 契约测试：5 passed。
- **门禁验证**：
  - Gate 1：12 PASS / 0 FAIL / 0 SKIP。
  - Gates PR：16 PASS / 8 SKIP（触发路径不包含 framework/C 源码因此跳过，符合设计）/ 0 FAIL。
  - SSOT 不变量与许可门禁：100% 通过。
- **仿真回归**：
  - `uart_echo` headless deterministic proof 运行通过，生成完整断言与 payload 证据。

---

## 3. 资产与兼容性保证

1. **零外部影响**：所有变更严格限制在 `.governance/`、CI 配置与本说明文档内，未修改任何固件 C 源码或 PAL 驱动。
2. **向下兼容**：历史 `tools/loop/*.py` 导入路径及 `tools/*.py` 命令行调用均经由 Shim 保持原样工作。
3. **回滚凭据**：全量变更补丁与新增文件清单已归档于 `$CleanupEvidence/delivery/`。
