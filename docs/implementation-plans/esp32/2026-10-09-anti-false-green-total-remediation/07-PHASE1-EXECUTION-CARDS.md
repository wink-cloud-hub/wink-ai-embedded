<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Phase 1 执行任务卡与质量门禁追踪 (QG-1 Tracking)

| 项 | 内容 |
|---|---|
| 计划编号 | `PLAN-20261009-ESP-IDF-AFG-PHASE1-EXECUTION` |
| 日期 / 时代戳 | 2026-10-09 (Asia/Shanghai) |
| 状态 | **Active / Task T1.1 已完成，Task T1.2 启动** |
| 关联主控计划 | [00-MASTER-OVERVIEW.md](00-MASTER-OVERVIEW.md) (v1.1 Active) |
| 阶段实施计划 | [01-PHASE1-PIPELINE-AND-SANDBOX.md](01-PHASE1-PIPELINE-AND-SANDBOX.md) (v1.1 Active) |
| 共同质量门禁 | [05-EXECUTION-QUALITY-GATES.md](05-EXECUTION-QUALITY-GATES.md) (v1.1 Active) |
| 基线前置证据 | [06-QG0-PHASE0-BASELINE-EVIDENCE.md](06-QG0-PHASE0-BASELINE-EVIDENCE.md) (QG-0 PASSED) |

---

## 任务卡索引

- [T1.1 真实采集入口改造与 ProofPlan 预检](#t11-真实采集入口改造与-proofplan-预检) *(已完成 ✅)*
- [T1.2 生效的 L1/L2 变异、归因与恢复](#t12-生效的-l1l2-变异归因与恢复) *(执行中 🚀)*
- [T1.3 构建指纹、加载身份与写隔离](#t13-构建指纹加载身份与写隔离) *(待执行)*
- [T1.4 共用接受策略与旧写入口收敛](#t14-共用接受策略与旧写入口收敛) *(待执行)*
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
- **工程追踪状态**: `执行中`

### 2. 范围与依赖
- **修改文件白名单**:
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/afg/mutation_runner.py` (新增)
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/mutator.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/loop/mutation_catalog.py`
  - `wink-micro-app/vendor/esp_idfv61/.governance/tests/unit/test_t1_2_mutation_runner.py` (新增)
- **必需前置条件**: T1.1 (Done)
- **关联技术契约**: [AFG-Engine 契约 2.1 节与 2.3 节](../../../zh/tech-designs/esp32/esp-idf-anti-false-green-verification-engine-contract.md)

### 3. 验收设计 (AC-1.2)
- **行为目标 1 (L1 真实算子注入)**: 协议/驱动层故障注入算子（如丢包、断线、时钟跳变）必须通过场景注入或仿真环境实际生效，禁止空跑；
- **行为目标 2 (L2 源码真实变异与重编)**: 源码级变异算子必须真实修改 C 代码（破坏 app_main、注释重要逻辑、反转分支条件），触发 wasm 重新编译，严禁将原二进制冒充变异体；
- **行为目标 3 (击杀归因四态严格判定)**: 变异执行器输出精确状态：
  - `MUTATION_NOT_ACTIVATED`: 变异点未被执行流触达；
  - `MUTANT_SURVIVED`: 变异体存活且原有业务断言依然全绿（假绿立证）；
  - `MUTATION_BUILD_FAILED`: 变异导致编译语法错误（不计入击杀预算）；
  - `MUTANT_KILLED`: 变异体编译成功、被正常执行、并被业务断言有效捕获失败；
- **行为目标 4 (完全原子恢复与脏状态清零)**: 变异与测试后，受变异源码与资产必须 100% 回滚，重编译基线验证其干净恢复状态 (`dirty_state_cleared=True`)；
- **行为目标 5 (L2 变异预算守恒)**: 单一 Claim 严格执行不超过 1 次有效 L2 变异（Axiom 7 / META-17）。
