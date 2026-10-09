<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Phase 0 验收证据与执行基线冻结备忘录 (QG-0 Evidence)

| 项 | 内容 |
|---|---|
| 凭据编号 | `EVID-20261009-ESP-IDF-AFG-QG0-BASELINE` |
| 日期 / 时代戳 | 2026-10-09T18:05:00+08:00 (Asia/Shanghai) |
| 状态 | **Verified & Frozen / QG-0 阶段门禁正式成立** |
| 关联总控计划 | [00-MASTER-OVERVIEW.md](00-MASTER-OVERVIEW.md) (v1.1 Active) |
| 共同质量门禁 | [05-EXECUTION-QUALITY-GATES.md](05-EXECUTION-QUALITY-GATES.md) (v1.1 Active) |
| 阶段入口核查角色 | 计划维护者 / 架构复核者 |
| 下游解除阻塞 | Phase 1 ([01-PHASE1-PIPELINE-AND-SANDBOX.md](01-PHASE1-PIPELINE-AND-SANDBOX.md))、Phase 2 ([02-PHASE2-DRIVER-AND-SIM-HARDENING.md](02-PHASE2-DRIVER-AND-SIM-HARDENING.md)) |

---

## 1. T0.1 范围、清单与执行环境冻结

### 1.1 源码与工作区指纹
- **Git HEAD**: `0294acc2c60fe996f535f0cf221cd5f0c8ec1c9b`
- **工作区隔离状态**:
  - `wink-micro-app/vendor/cms8s78xx/`: 存在独立厂商目录的既有修改/未跟踪测试，遵循隔离原则，本工程**绝对不触碰、不重置、不污染**该目录。
  - `wink-micro-os/frameworks/mcs51/`: 存在独立工具与测试，保持隔离。
  - 本工程工作范围严格限制在：`wink-micro-app/vendor/esp_idfv61/.governance/`、`wink-micro-os/frameworks/esp_idf/` 以及 `docs/` 下相关整改计划。

### 1.2 清单 SSOT 统计重算与核对
通过对 [checklist.data.json](../../../../wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json) 进行直接二进制读取与 JSON 结构重算，各项指标完全吻合，冻结如下：

| 指标 | 冻结值 | 状态与说明 |
|---|---:|---|
| 清单文件 SHA-256 | `2910998e93f2c36e904effce4494c8bb73b5b603c8e790e92fc6129c221be122` | 与 v1.1 主计划一致，指纹有效 |
| 清单文件大小 | 1,101,855 字节 | 二进制字节精确匹配 |
| 清单全量应用数 | 478 | 包含现行范围外 (scope_out: 166) 应用 |
| in_scope 应用数 | 312 | 治理全集 |
| in_scope 执行配置数 | 313 | 311 应用为 1 配置，1 项应用 (`esp.wifi.getting_started.station`) 包含双配置 (`wasm_sim_standard`, `wasm_sim_node`) |
| in_scope active 应用 / 配置 | 267 / 268 | 本轮推进与治理基数 |
| in_scope deferred 应用 / 配置 | 45 / 45 | 合法暂缓项，明确保持未交付 |
| 需求审计状态 | audited: 46 / pending: 266 | 266 项 pending 严格保持为 pending，不假填代填 |
| 当前登记 verified 配置 | 46 | 存量历史登记，不等于满足本工程全部新验收标准 |

### 1.3 核心治理元数据与契约文件摘要

```text
wink-micro-app/vendor/esp_idfv61/.governance/
├── data/
│   ├── checklist.data.json                   SHA256: 2910998e93f2c36e904effce4494c8bb73b5b603c8e790e92fc6129c221be122
│   ├── defect_registry.json                  SHA256: 3f84ecce71be5effbcd3407c8842b1e1a43d972387fa80fdd17f9ea4471385bf
│   └── extracted_dependencies_report.json    SHA256: e39430df00ff6401ce017082f67dec1db556069cf45c2771a009e279685f6f1d
├── catalog/
│   └── capability-catalog.yaml               SHA256: 99a3ad78a2de4ff2acd893a836fad8736608086892d5c64a9e9f2d9909d175a5
├── archetypes/
│   ├── archetype_start.yaml                  SHA256: e05d05749c369a7475d6207df6f5f0a220d4a488831edfd81bcf12f2180437ae
│   ├── archetype_uart_stream.yaml            SHA256: 4bed103d02210fc62f262467b97ac7f5fc220db140140240d3ccaf14627ff8ca
│   ├── archetype_adc_sampling.yaml           SHA256: 5a604004c32ce80b10a79b05f0d8574534019bf9b19c58c35c45d52c7cb106b4
│   ├── archetype_gpio_matrix.yaml            SHA256: 29b59ae60d86029ef3bea1f99b41bf26b69b34a86501c6b43ee6d56b5e7554cd
│   └── archetype_ledc_pwm.yaml               SHA256: 5a3af3afcb6fcfdb8d35531af83238dc54626198911b9c158d9f22932a69c249
├── specs/
│   ├── CLASSIFICATION-SPEC.md                SHA256: c3b2ba807e60d3e9c8900f6310c51e88e8427fed536c94a1563927de0255cdd2
│   └── upstream_errata.json                  SHA256: 289e2fb4b5e40f36d715b772fcb80ec4f225eb7226b4ef39b09a4a43dc01a196
├── gates/
│   └── gates.yaml                            SHA256: 2c6a0b033a08ce27c18bd50378a0136043d65f170bc69e1a3420bdf828653d0e
└── toolchain.lock.yaml                       SHA256: e6717c1f3faa9d6847059569bf14ecb26efa58feee0eb7d8973f36c2ecbdd786
```
- **业务场景文件全集**: 共计 103 个 `*.scenario.json` 与 `*.fail.scenario.json`，已全量锁定。

### 1.4 工具链与宿主执行环境
- **操作系统**: Windows 11 AMD64 (PowerShell 5.1 / 7)
- **Python**: `3.11.15` (`C:\Espressif\tools\python\python.exe` / sys.version)
- **Node.js**: `v22.23.2` (`D:\software\nodejs\fnm\node\aliases\default\node.EXE`)
- **Emscripten (emcc)**: `6.0.9` (`D:\software\embedded-tools\emsdk\upstream\emscripten\emcc.EXE`)
- **CMake**: `3.x` (`D:\software\embedded-tools\cmake\bin\cmake.EXE`)
- **Ninja / Make**: `mingw32-make` / `ninja` 正常可用
- **跨仓统管工具**: 确认存在 `d:\workspaces\ai-coding\wink-ai\wink-ai\packages\wink-tools\wink.py`，支持 `winkcli sim run`、`winkcli lint`、`winkcli esp32` 等指令。

---

## 2. T0.2 设计与契约核对

| 架构要素 / 契约 | 来源规范 | 状态与核对结论 |
|---|---|---|
| **六大防假绿公理** | [AFG-Engine 契约](../../../zh/tech-designs/esp32/esp-idf-anti-false-green-verification-engine-contract.md) | **Accepted**；双极性证伪击杀、因果隔离、离散时钟自发、探针版本化、强制扰动、二进制链接见证已完成契约冻结。 |
| **沙箱隔离与 Job 监管** | [Loop 可靠性契约](../../../zh/tech-designs/esp32/esp-idf-loop-reliability-contract.md) | **Accepted**；Win32 Job Object 进程树清理与 `RunContext` 目录树隔离契约冻结。 |
| **密封证据与 CAS 发布** | [Batch 0 证据契约](../../../zh/tech-designs/esp32/esp-idf-batch0-evidence-contract.md) | **Accepted**；`PromotionService` 锁内 CAS、无自引用摘要已定义。 |
| **多配置正交模型** | [ADR-0091](../../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md) | **Accepted**；`(app_id, config_id, backend, target_soc, profile)` 作为最小唯一执行实体。 |
| **治理宪章** | [ADR-0092](../../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md) | **Accepted**；明确分类规范与交付门禁职责。 |
| **编译期静态分发与负错误码** | [ADR-0001](../../../decisions/core/0001-error-code-sign-convention.md)、[ADR-0004](../../../decisions/core/0004-static-dispatch-vs-runtime-ops.md) | **Accepted**；严禁引入 vtable/container_of，Wink API 严格返回负数错误码。 |
| **PWM 占空比万分比定点化** | [ADR-0066](../../../decisions/core/0066-pwm-basis-points-and-float-deprecation.md) | **Accepted**；C 代码强制使用 `PERMILLE()` / `PAL_PWM_DUTY_PCT()`。 |

---

## 3. T0.3 关键失败基线与反例对照表

为防范虚假绿灯，Phase 0 冻结以下 7 项关键反例，Phase 1 验收设施必须对以下坏样本 100% 拒绝，并对正确样本 100% 通过：

| 反例 ID | 目标防线 / 检查项 | 注入坏样本特征 | 预期拒绝原因 / 行为 | 正确对照样本 |
|---|---|---|---|---|
| **NE-01** | `verify_afg_engine` Pilot 真实性 | 仅构造内存假字典/合成记录而不运行物理固件 | 拒绝合成记录，报 `NO_PHYSICAL_EXECUTION_EVIDENCE` | 经由真实 `run_esp32_headless_evidence.ps1` 产出并校验的原始报告 |
| **NE-02** | 借用报告 / 跨应用串用 | 将 `blink` 的运行报告填入 `hello_world` 的凭据包中 | 拒绝，报 `REPORT_SCENARIO_MISMATCH` 与 `RUN_ID_APP_MISMATCH` | `hello_world` 专用且摘要一致的场景回执 |
| **NE-03** | 虚假/未生效变异补丁 | 补丁仅修改注释、空白行或目标符号不存在 | 拒绝，报 `MUTATION_ZERO_MATCH_OR_NO_DIFF` (不计击杀) | 精准替换有效业务逻辑的 AST/源码 Diff |
| **NE-04** | 变异导致编译失败 | 补丁注入了语法错误使固件构建直接退出非零 | 记录编译失败诊断，**不计为实现变异击杀** | 成功完成二进制构建并在运行时被断言捕获的目标变异 |
| **NE-05** | 基础设施挂死与宿主超时 | 外部 runner 死锁触发宿主级 120s 强制 kill | 记录 `INFRASTRUCTURE_TIMEOUT`，不计为业务 SLA 超时 | 固件内部在虚拟时钟到达 deadline 时触发的业务超时断言 |
| **NE-06** | 无锁或并发写冲突 | 在未获取文件锁或租约过期时并发调用 `PromotionService` | 抛出 `LOCK_ACQUISITION_FAILED` 或 CAS 冲突回滚 | 持有效文件锁且原 manifest 摘要匹配的单写 CAS 提交 |
| **NE-07** | 缺失 ProofPlan / 缺失 Claims | 尝试对未声明 ProofPlan 的配置执行全量验收或发布 | 拒绝，报 `MISSING_PROOFPLAN_OR_CLAIMS`，保持 `needs_proofplan_update` | 继承标准 Archetype 并通过 Schema 校验的显式 ProofPlan |

---

## 4. T0.4 任务卡与执行责任索引

| 阶段 | 任务 ID | 任务内容与文件白名单 | 实现责任人 | 独立复核人 |
|---|---|---|---|---|
| **Phase 1** | T1.1 | 真实采集入口改造与 ProofPlan 预检 (`loop/pipeline/pipeline.py`, `tools/verify_afg_engine.py`) | 工具维护者 | 架构复核者 |
| **Phase 1** | T1.2 | L1/L2 变异生效、归因与恢复 (`loop/afg/mutation_runner.py`, `loop/mutator.py`) | 引擎维护者 | 独立验证维护者 |
| **Phase 1** | T1.3 | 构建指纹、加载身份与写隔离 (`loop/build_sandbox.py`, `loop/harness/run_context.py`) | 沙箱维护者 | 架构复核者 |
| **Phase 1** | T1.4 | 共用接受策略与旧写入口收敛 (`gates/evidence_verifier.py`, `gates/report_contract.py`) | 门禁维护者 | 独立验证维护者 |
| **Phase 1** | T1.5 | 进程树 Win32 Job 隔离与有界回收 (`loop/harness/process_supervisor.py`) | 系统维护者 | 工具维护者 |
| **Phase 1** | T1.6 | 稳定密封与审计绑定 (`tools/promotion_service.py`, `tools/inspect_candidate.py`) | 发布维护者 | 独立审计者 |
| **Phase 1** | T1.7 | 持锁 CAS、幂等与崩溃恢复 (`tools/promotion_service.py`) | 发布维护者 | 架构复核者 |
| **Phase 1** | T1.8 | Pilot A (`hello_world`) 真实闭环端到端集成 | 综合执行组 | 独立审计者 |
| **Phase 2** | T2.1 | SPI 门面编译修复 (E-04: `esp_spi.c:251-252` 标识符未声明) 与 S-04 隔离 | C 驱动维护者 | PAL 架构复核者 |
| **Phase 2** | T2.2 | GPTimer 观察修复 (S-01: `esp_gptimer.c` 移除系统时钟回退) | C 驱动维护者 | 仿真架构复核者 |
| **Phase 2** | T2.3 | HWTimer 单次重载修复 (S-02: `pal_wasm_hwtimer.c` 保护重入槽) | C 驱动维护者 | PAL 架构复核者 |
| **Phase 2** | T2.4 | ADC 自发生产与错误传播 (S-03: `esp_adc.c`, `pal_wasm_ch3_adc.c`) | C 驱动维护者 | 仿真架构复核者 |
| **Phase 2** | T2.5 | SPIFFS RAM VFS 后端统一 (S-05: `esp_spiffs.c`) | C 驱动维护者 | 存储架构复核者 |

---

## 5. T0.5 命令与耗时预检实测记录

2026-10-09 只读检查及预检命令执行记录如下：

```powershell
# 1. Gate 1 静态规则检查
python wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --mode nightly --gate 1
# 结果: 12 条规则执行全部 PASS, 耗时 1.8s

# 2. 存量历史报告校验
python wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py --verify-all
# 结果: 46/46 verified entries passed, 耗时 1.2s

# 3. AFG 算法演练入口
python wink-micro-app/vendor/esp_idfv61/.governance/tools/verify_afg_engine.py --pilot
# 结果: Pilot A/B ELIGIBLE, Pilot C REJECTED (拦截 S-03 缺陷), 耗时 1.1s

# 4. 首条真实链路构建与仿真预检 (hello_world)
powershell -File wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1 -App hello_world -Scenario wink-micro-app/vendor/esp_idfv61/get-started/hello_world/unisim-scenarios/hello_world.scenario.json
```

### 关键阻断性发现 (E-04 真实缺陷复现)
在执行 `hello_world` 的 Wasm 资产真实编译预检时，`emcmake` 调用 CMake 编译 `wink_framework_esp_idf`，由于编译单元中包含 `esp_spi.c`，编译器精准报错中断：
```text
D:\workspaces\ai-coding\wink-ai\wink-ai-embedded\wink-micro-os\frameworks\esp_idf\src\drivers\esp_spi.c:251:12: error: use of undeclared identifier 's_eeprom_mem'
  251 |     memset(s_eeprom_mem, 0xFF, sizeof(s_eeprom_mem));
      |            ^~~~~~~~~~~~
D:\workspaces\ai-coding\wink-ai\wink-ai-embedded\wink-micro-os\frameworks\esp_idf\src\drivers\esp_spi.c:252:5: error: use of undeclared identifier 's_eeprom_write_enabled'
  252 |     s_eeprom_write_enabled = false;
      |     ^~~~~~~~~~~~~~~~~~~~~~
3 errors generated.
mingw32-make: *** [Makefile:90: all] Error 2
[wink] ❌ Simulation asset build failed (code 1).
```
- **核心结论**：
  1. 该结果完全印证了主计划第 1 节与问题账本第 2 节的 **E-04 事实**：历史凭据核验通过，但当前 C 驱动源码存在真实语法错误，现行流水线如果依赖旧的预编译产物就会掩盖此错误；
  2. 真实端到端链路要产出真正有效的 Wasm 仿真凭据，在 Phase 1 推进 Pilot A 真实执行的同时，必须由 Phase 2（驱动加固，Task T2.1）将 `esp_spi.c` 的未声明标识符修复，才能成功产出全新二进制并加载运行。

---

## 6. QG-0 出口裁定

依照 [05-EXECUTION-QUALITY-GATES.md](05-EXECUTION-QUALITY-GATES.md) 第 5 节之 QG-0 出口判据核对：

- [x] **基线与配置集合已完全冻结**：清单 312 in_scope 应用 / 313 配置、267 active、45 deferred、46 audited、266 pending，SHA-256 校验码锁定；
- [x] **完整问题账本已映射**：I-01～I-19、S-01～S-05、Q-01～Q-06、G-01～G-03 已分别对应责任任务；
- [x] **任务卡与双人责任已指定**：T1.1～T1.8 及 T2.1～T2.5 已明确责任角色与复核角色；
- [x] **实际命令与环境能力预检已完成**：编译器、Node、Wasm 工具链、`wink.py` 已核验，E-04 真实编译阻断已静态确认并纳入整改路径；
- [x] **设计选择与冲突处理完成**：6 大公理、Job 监管、CAS 事务、双 target 等设计契约全部 Accepted。

**结论**：**QG-0 阶段门禁正式成立通过 (PASSED)**，允许正式进入 Phase 1 实现与 Phase 2 驱动反例设计。
