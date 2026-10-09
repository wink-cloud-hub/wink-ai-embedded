<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Phase 1 执行任务卡与质量门禁追踪 (QG-1 Tracking)

| 项 | 内容 |
|---|---|
| 计划编号 | `PLAN-20261009-ESP-IDF-AFG-PHASE1-EXECUTION` |
| 日期 / 时代戳 | 2026-10-09 (Asia/Shanghai) |
| 状态 | **Active / Task T1.1 ~ T1.3 已完成，Task T1.4 启动** |
| 关联主控计划 | [00-MASTER-OVERVIEW.md](00-MASTER-OVERVIEW.md) (v1.1 Active) |
| 阶段实施计划 | [01-PHASE1-PIPELINE-AND-SANDBOX.md](01-PHASE1-PIPELINE-AND-SANDBOX.md) (v1.1 Active) |
| 共同质量门禁 | [05-EXECUTION-QUALITY-GATES.md](05-EXECUTION-QUALITY-GATES.md) (v1.1 Active) |
| 基线前置证据 | [06-QG0-PHASE0-BASELINE-EVIDENCE.md](06-QG0-PHASE0-BASELINE-EVIDENCE.md) (QG-0 PASSED) |

---

## 任务卡索引

- [T1.1 真实采集入口改造与 ProofPlan 预检](#t11-真实采集入口改造与-proofplan-预检) *(已完成 ✅)*
- [T1.2 生效的 L1/L2 变异、归因与恢复](#t12-生效的-l1l2-变异归因与恢复) *(已完成 ✅)*
- [T1.3 构建指纹、加载身份与写隔离](#t13-构建指纹加载身份与写隔离) *(已完成 ✅)*
- [T1.4 共用接受策略与旧写入口收敛](#t14-共用接受策略与旧写入口收敛) *(执行中 🚀)*
- [T1.5 进程树 Win32 Job 隔离与有界回收](#t15-进程树-win32-job-隔离与有界回收) *(待执行)*
- [T1.6 稳定密封与审计绑定](#t16-稳定密封与审计绑定) *(待执行)*
- [T1.7 持锁 CAS、幂等与崩溃恢复](#t17-持锁-cas幂等与崩溃恢复) *(待执行)*
- [T1.8 Pilot A / Pilot B 真实闭环端到端集成与反向自验](#t18-pilot-a--pilot-b-真实闭环端到端集成与反向自验) *(待执行)*

---

## T1.1 真实采集入口改造与 ProofPlan 预检

### 1. 身份与责任
- **Task ID**: `T1.1`
- **问题归属**: `I-03`, `I-05`, `I-07`, `AFG-NEW-01`
- **实现责任人**: 工具维护者 (Tool Maintainer)
- **独立复核人**: 架构复核者 (Architecture Reviewer)
- **工程追踪状态**: **已完成 (Done ✅)**

### 2. 范围与依赖
- **修改文件**:
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/pipeline/pipeline.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/tools/verify_afg_engine.py`
  - `wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1`
  - `wink-micro-app/vendor/esp_idfv61/.governance/tests/unit/test_t1_1_proofplan_preflight.py` (新增单测)
- **必需前置条件**: QG-0 (PASSED)
- **关联技术契约**: [AFG-Engine 契约](../../../zh/tech-designs/esp32/esp-idf-anti-false-green-verification-engine-contract.md)

### 3. 验收设计 (AC-1.1 落地核验)
- [x] **行为目标 1 (拒假 ProofPlan)**: `pipeline.py` 严格校验 `require_proofplan=True`，无 ProofPlan 或 Claims 为空时显式拒绝，报 `MISSING_PROOFPLAN_OR_CLAIMS`；
- [x] **行为目标 2 (拒假实现变异)**: 彻底移除单纯断言匹配器自检标注为 `implementation_mutation` 的伪造逻辑；
- [x] **行为目标 3 (剥离 Pilot 合成假字典)**: `verify_afg_engine.py` 接入物理场景回执与 `device-tree.json`、WASM 二进制哈希，mock 向量严格隔离至 `--algo-exercise`；
- [x] **行为目标 4 (清除特例放行)**: `verify_afg_engine.py` 存量 46 项决策树中彻底删除 `hello_world` / `uart_echo` 的硬编码 bypass；
- [x] **行为目标 5 (统一 Pilot 身份)**: Pilot B 身份统一为 `esp.peripherals.uart.uart_echo`。

### 4. 验证凭据与回执
- **专用单测**: `pytest tests/unit/test_t1_1_proofplan_preflight.py` 6/6 全部通过；
- **全量单测**: `pytest tests/unit/` 30/30 全部通过；
- **元公理测试**: `pytest tests/meta_invariants/` 31/31 全部通过；
- **Batch 0 回归**: `pytest tests/gates/test_batch0_evidence.py` 55/55 全部通过；
- **Pilot 真实模式**: `python tools/verify_afg_engine.py --pilot` 成功加载物理资产与回执执行，Pilot A/B 正式由物理凭据驱动，Pilot C 准确拦截 S-03；
- **算法演练模式**: `python tools/verify_afg_engine.py --algo-exercise` 3 标杆算法判定无退化。

---

## T1.2 生效的 L1/L2 变异、归因与恢复

### 1. 身份与责任
- **Task ID**: `T1.2`
- **问题归属**: `I-05`, `I-06`, `I-10`, `AFG-NEW-02`
- **实现责任人**: 运行时工程师 (Runtime Engineer)
- **独立复核人**: 测试与安全工程师 (Test & Security Engineer)
- **工程追踪状态**: **已完成 (Done ✅)**

### 2. 范围与依赖
- **修改文件**:
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/afg/mutation_runner.py` (新增真实变异与归因实现)
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/mutator.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/mutation_catalog.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/tests/unit/test_t1_2_mutation_runner.py` (新增专用单测)
- **必需前置条件**: T1.1 (Done)
- **关联技术契约**: [AFG-Engine 契约 2.1 节与 2.3 节](../../../zh/tech-designs/esp32/esp-idf-anti-false-green-verification-engine-contract.md)

### 3. 验收设计 (AC-1.2 落地核验)
- [x] **行为目标 1 (L1 真实算子注入)**: 协议/驱动层故障注入算子支持 `fault_handling_pass` 与 `mutant_kill_fail`，输出精确 `FAULT_HANDLED_PASS` / `FAULT_UNHANDLED_FAIL` / `MUTANT_KILLED`；
- [x] **行为目标 2 (L2 源码真实变异与沙箱隔离)**: 源码级变异算子在独立沙箱中通过 AST/Catalog 算子执行真实 C 代码修改，原代码目录零污染；
- [x] **行为目标 3 (击杀归因四态严格判定)**: 变异执行器输出精确状态：
  - `MUTATION_NOT_ACTIVATED`: 变异点未触达或算子零匹配（拒绝伪造击杀）；
  - `MUTANT_SURVIVED`: 变异体存活且业务断言全绿（假绿立证）；
  - `MUTATION_BUILD_FAILED`: 变异导致编译语法错误（不计入击杀预算，META-23）；
  - `MUTANT_KILLED`: 变异体被业务断言有效捕获失败；
- [x] **行为目标 4 (完全原子恢复与脏状态清零)**: `run_recovery` 验证纯净基线复验与 `dirty_state_cleared=True`；
- [x] **行为目标 5 (L2 变异预算守恒)**: 单一 Claim 严格限制最多 1 次 L2 变异（超额抛出 `MutationBudgetExceededError`）。

### 4. 验证凭据与回执
- **专用单测**: `pytest tests/unit/test_t1_2_mutation_runner.py` 7/7 全部通过；
- **全量单测**: `pytest tests/unit/` 37/37 全部通过；
- **元公理回归**: `pytest tests/meta_invariants/` 31/31 全部通过；
- **Batch 0 回归**: `pytest tests/gates/test_batch0_evidence.py` 55/55 全部通过。

---

## T1.3 构建指纹、加载身份与写隔离

### 1. 身份与责任
- **Task ID**: `T1.3`
- **问题归属**: `I-04`, `I-09`, `I-11`, `AFG-NEW-03`
- **实现责任人**: 构建与工具工程师 (Build & Tooling Engineer)
- **独立复核人**: 架构复核者 (Architecture Reviewer)
- **工程追踪状态**: **已完成 (Done ✅)**

### 2. 范围与依赖
- **修改文件**:
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/afg/build_sandbox.py` (全闭包依赖缓存与双重自检实现)
  - `wink-micro-app/vendor/esp_idfv61/.governance/tests/unit/test_t1_3_build_fingerprint.py` (新增专用单测)
- **必需前置条件**: T1.2 (Done)
- **关联技术契约**: [AFG-Engine 契约 2.4 节](../../../zh/tech-designs/esp32/esp-idf-anti-false-green-verification-engine-contract.md)

### 3. 验收设计 (AC-1.3 / AC-1.4 落地核验)
- [x] **行为目标 1 (全依赖闭包缓存指纹)**: 缓存指纹严格绑定源码摘要、头文件闭包摘要、sdkconfig 摘要、编译器/SDK 版本、门面 Git SHA、门面源码摘要、补丁摘要和配置 profile ID；
- [x] **行为目标 2 (未提交修改强制破缓存)**: 显式引入 `facade_source_digest`，门面工作树任何未提交源码变动立即导致指纹变化，杜绝过期产物假命中；
- [x] **行为目标 3 (缓存命中的双向自检)**: `lookup_cache` 强制校验产物存在性、非空性与元数据记录的 `artifact_sha256` 精确一致性，损坏或被篡改产物自动判定 miss 并拒绝服务；
- [x] **行为目标 4 (并发原子写入)**: `store_cache` 通过进程 PID 隔离的临时目录和原子目录替换，防止并发读取半写入产物；
- [x] **行为目标 5 (工作区写隔离保护)**: `verify_write_isolation` 通过全目录哈希快照严格比对，确保构建/变异过程中上游原始目录零文件污染、零篡改。

### 4. 验证凭据与回执
- **专用单测**: `pytest tests/unit/test_t1_3_build_fingerprint.py` 4/4 全部通过；
- **全量单测**: `pytest tests/unit/` 41/41 全部通过；
- **元公理回归**: `pytest tests/meta_invariants/` 31/31 全部通过；
- **Batch 0 回归**: `pytest tests/gates/test_batch0_evidence.py` 55/55 全部通过。

---

## T1.4 共用接受策略与旧写入口收敛

### 1. 身份与责任
- **Task ID**: `T1.4`
- **问题归属**: `I-01`, `I-08`, `I-12`, `AFG-NEW-04`
- **实现责任人**: 门禁与协议工程师 (Gate & Protocol Engineer)
- **独立复核人**: 架构复核者 (Architecture Reviewer)
- **工程追踪状态**: `已完成 (Done ✅)`

### 2. 范围与依赖
- **修改文件白名单**:
  - `wink-micro-app/vendor/esp_idfv61/.governance/gates/report_contract.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/tests/unit/test_t1_4_acceptance_policy.py` (新增)
- **必需前置条件**: T1.3 (Done)
- **关联技术契约**: [AFG-Engine 契约 2.2 节与 2.5 节](../../../zh/tech-designs/esp32/esp-idf-anti-false-green-verification-engine-contract.md)

### 3. 验收设计 (AC-1.4 / AC-1.5 落地核验)
- [x] **行为目标 1 (统一报告接受标准)**: `report_contract.py` 与 `evidence_verifier.py` 彻底收敛共用验证规则，借用应用报告、步骤数量不符、缺业务断言等恶意/损坏报告在所有入口 100% 拦截；
- [x] **行为目标 2 (旧写入器禁止提前覆盖)**: `write_evidence_for_app` 强制在预检通过后方可执行原子替换，禁止在未完成完整验证前预先覆盖历史报告或生成伪造凭据；
- [x] **行为目标 3 (错误码符号规约严格校验)**: 严格验证 POSIX (正数)、ESP-IDF (正数/零) 与 WinkMicroOS (负数错误码) 符号域隔离，模糊匹配器 (status!=0) 拒绝放行。

### 4. 验证凭据与回执
- **专用单测**: `pytest tests/unit/test_t1_4_acceptance_policy.py` 13/13 全部通过；
- **全量单测**: `pytest tests/unit/` 54/54 全部通过；
- **元公理回归**: `pytest tests/meta_invariants/` 31/31 全部通过；
- **Batch 0 回归**: `pytest tests/gates/test_batch0_evidence.py` 55/55 全部通过；
- **核验器单测**: `pytest tests/gates/test_evidence_verifier.py` 22/22 全部通过；
- **对抗门禁回归**: `pytest tests/gates/test_adversarial_suite.py` 35/35 全部通过。

---

## T1.5 进程树 Win32 Job 隔离与有界回收

### 1. 身份与责任
- **Task ID**: `T1.5`
- **问题归属**: `I-04`, `AFG-NEW-01`
- **实现责任人**: 进程与宿主工程师 (Process & Supervisor Engineer)
- **独立复核人**: 架构复核者 (Architecture Reviewer)
- **工程追踪状态**: `已完成 (Done ✅)`

### 2. 范围与依赖
- **修改文件白名单**:
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/harness/process_supervisor.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/tools/loop/process_supervisor.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/tests/unit/test_t1_5_process_supervisor.py` (新增)
- **必需前置条件**: T1.4 (Done)
- **关联技术契约**: [Loop 可靠性契约 2.2 节](../../../zh/tech-designs/esp32/esp-idf-loop-reliability-contract.md)

### 3. 验收设计 (AC-1.6 落地核验)
- [x] **行为目标 1 (Win32 Job Object 强绑定)**: Windows 平台使用 `CREATE_SUSPENDED` 挂起创建进程、加入 Job Object、设置 `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` 并恢复执行，禁止允许后代逃逸的 breakaway 设置；
- [x] **行为目标 2 (多级后代进程树有界清理)**: 父进程先退、控制器崩溃或异常中断时，递归终止所有子孙进程，杜绝孤儿 node/python/wasm 进程泄漏；
- [x] **行为目标 3 (继承管道与文件句柄释放)**: 终止进程后显式关闭所有继承管道与日志重定向文件描述符，支持重试回收机制。

### 4. 验证凭据与回执
- **专用单测**: `pytest tests/unit/test_t1_5_process_supervisor.py` 8/8 全部通过；
- **全量单测**: `pytest tests/unit/` 62/62 全部通过；
- **元公理回归**: `pytest tests/meta_invariants/` 31/31 全部通过；
- **Batch 0 回归**: `pytest tests/gates/test_batch0_evidence.py` 55/55 全部通过；
- **核验器单测**: `pytest tests/gates/test_evidence_verifier.py` 22/22 全部通过；
- **对抗门禁回归**: `pytest tests/gates/test_adversarial_suite.py` 35/35 全部通过。

---

## T1.6 稳定密封与审计绑定

### 1. 身份与责任
- **Task ID**: `T1.6`
- **问题归属**: `I-08`, `I-09`, `AFG-NEW-02`
- **实现责任人**: 凭据密封与发布维护者 (Packaging & Promotion Engineer)
- **独立复核人**: 架构复核者 (Architecture Reviewer)
- **工程追踪状态**: `已完成 (Done ✅)`

### 2. 范围与依赖
- **修改文件白名单**:
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/afg/canonical_sealing.py` (新增规范化密封实现)
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/afg/__init__.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/pipeline/pipeline.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/tools/inspect_candidate.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/tests/unit/test_t1_6_envelope_sealing.py` (新增)
- **必需前置条件**: T1.5 (Done)
- **关联技术契约**: [Loop 可靠性契约 2.4 节](../../../zh/tech-designs/esp32/esp-idf-loop-reliability-contract.md)

### 3. 验收设计 (AC-1.7 落地核验)
- [x] **行为目标 1 (Payload 与 Envelope 严格分离与无自引用)**: payload 清单完全冻结后计算 `payload_sha256`，外部 summary/audit 位于 envelope，严禁自引用与密封后修改 payload；
- [x] **行为目标 2 (跨平台规范化摘要与 JCS 序列化)**: 换行符统一归一化为 LF，JSON 清单严格遵守 RFC 8785 (JCS) 消除字段顺序与空白符差异；
- [x] **行为目标 3 (审计必须绑定独立主体与 AFG 回执)**: 拒绝无报告/无 ProofPlan/自签/借用审计凭据，正式接受入口强制重算实际文件摘要并校验有效性。

### 4. 验证凭据与回执
- **专用单测**: `pytest tests/unit/test_t1_6_envelope_sealing.py` 10/10 全部通过；
- **全量单测**: `pytest tests/unit/` 72/72 全部通过；
- **元公理回归**: `pytest tests/meta_invariants/` 31/31 全部通过；
- **Batch 0 回归**: `pytest tests/gates/test_batch0_evidence.py` 55/55 全部通过；
- **核验器单测**: `pytest tests/gates/test_evidence_verifier.py` 22/22 全部通过；
- **对抗门禁回归**: `pytest tests/gates/test_adversarial_suite.py` 35/35 全部通过。

---

## T1.7 持锁 CAS、幂等与崩溃恢复

### 1. 身份与责任
- **Task ID**: `T1.7`
- **问题归属**: `I-10`, `G-02`
- **实现责任人**: 事务与发布工程师 (Transaction & CAS Engineer)
- **独立复核人**: 架构复核者 (Architecture Reviewer)
- **工程追踪状态**: `已完成 (Done ✅)`

### 2. 范围与依赖
- **修改文件白名单**:
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/services/promotion_service.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/tools/promotion_service.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/harness/lock_lease.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/tests/unit/test_t1_7_cas_promotion.py` (新增)
- **必需前置条件**: T1.5, T1.6 (Done)
- **关联技术契约**: [Loop 可靠性契约 2.7 节](../../../zh/tech-designs/esp32/esp-idf-loop-reliability-contract.md)

### 3. 验收设计 (AC-1.8 落地核验)
- [x] **行为目标 1 (真磁盘并发与乐观 CAS 锁控制)**: 验证 manifest.lock 租约控制、并发 CAS 冲突检测与安全重试，禁止仅凭 TTL 粗暴驱逐活跃锁持有者；
- [x] **行为目标 2 (不可变发布与原子引用替换)**: 先在隔离 reports 目录写入不可变包归档，再在锁保护下原子更新清单引用，杜绝中间态可见；
- [x] **行为目标 3 (事务日志、崩溃恢复与幂等读回)**: 事务各边界遭遇进程终止、崩溃或句柄占用时支持 journal 读回与恢复，已晋升请求幂等返回成功且旧包不损坏。

### 4. 验证凭据与回执
- **专用单测**: `pytest tests/unit/test_t1_7_cas_promotion.py` 7/7 全部通过；
- **全量单测**: `pytest tests/unit/` 79/79 全部通过；
- **元公理回归**: `pytest tests/meta_invariants/` 31/31 全部通过；
- **Batch 0 回归**: `pytest tests/gates/test_batch0_evidence.py` 55/55 全部通过；
- **核验器单测**: `pytest tests/gates/test_evidence_verifier.py` 22/22 全部通过；
- **对抗门禁回归**: `pytest tests/gates/test_adversarial_suite.py` 35/35 全部通过。


---

## T1.8 两项 Pilot 与防线反向自验

### 1. 身份与责任
- **Task ID**: `T1.8`
- **问题归属**: `I-17`, `AFG-NEW-01`
- **实现责任人**: 仿真与端到端集成工程师 (Integration & Pilot Engineer)
- **独立复核人**: 架构复核者 (Architecture Reviewer)
- **工程追踪状态**: `待执行`

### 2. 范围与依赖
- **修改文件白名单**:
  - `wink-micro-app/vendor/esp_idfv61/.governance/tools/verify_afg_engine.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/tests/unit/test_t1_8_pilot_e2e.py` (新增)
- **必需前置条件**: T1.1～T1.7 (Done)
- **关联技术契约**: [AFG-Engine 契约 5 节与 6 节](../../../zh/tech-designs/esp32/esp-idf-anti-false-green-verification-engine-contract.md)

### 3. 验收设计 (AC-1.9 落地核验)
- [ ] **行为目标 1 (Pilot A / Pilot B 真实端到端闭环)**: hello_world 与 uart_echo 通过真实 headless 管道执行，涵盖 baseline、Canary 击杀与现场恢复；
- [ ] **行为目标 2 (机器对账与全量检查集合校验)**: 7 类检查闭包、ProofPlan Claims 与执行身份机器对账无遗漏；
- [ ] **行为目标 3 (防线反向自验与变红校验)**: 分别拔除目标核验防线（如注入恒真断言或注销变异算子），端到端流水线必须准确熔断变红拒绝。



