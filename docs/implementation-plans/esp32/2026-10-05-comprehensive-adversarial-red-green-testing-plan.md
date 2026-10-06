<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ESP-IDF 官方示例全维度对抗测试、缺陷治理与红绿变异击杀实施计划 (Comprehensive Adversarial & Remediation Plan)

> **计划编号**：`PLAN-ESP-20261005-ADVERSARIAL-RED-GREEN`  
> **制定日期**：2026-10-05  
> **实施周期**：2026-10-05 ~ 2026-10-21 (约2周)  
> **责任体系**：嵌入式仿真核心架构组 / 治理流水线 SOP 团队  
> **适用目标**：`wink-micro-app/vendor/esp_idfv61` 官方示例库及 UniSim 仿真内核  
> **关联架构评审**：[2026-10-05-esp-idf-completed-items-review.md](../../reviews/esp32/2026-10-05-esp-idf-completed-items-review.md)  
> **Batch 0 技术设计**：[候选证据、断言自检与双实证绑定契约](../../zh/tech-designs/esp32/esp-idf-batch0-evidence-contract.md)
> **关联核心规范**：ADR-0001 (负数错误码)、ADR-0004 (静态分发)、ADR-0043 (分层门禁)、ADR-0066 (PWM定点bp)、ADR-0092 (PAL增量规范)

---

## 1. 背景与核心目标

### 1.1 现状与评审暴露的深水区缺陷
WinkMicroOS 当前已建立起一套基于 Wasm Headless 的 ESP-IDF 官方示例仿真治理体系，累计登记 33 个示例。但在 [2026-10-05 深度架构评审](../../reviews/esp32/2026-10-05-esp-idf-completed-items-review.md) 中，通过对源码、Wasm 二进制符号导出（136 个 export 逆向）与内存反例实验的全面审查，揭露出深水区的严峻矛盾：
1. **测试活性通过 $\ne$ 固件因果闭环**：
   - 现存 33 项中，仅 14 项（A 类）具备实质正向物理出口；10 项（C 类）存在严重的实现缺陷或断言绕过固件；
   - **外设虚设与因果短路**：DAC（#013, #014）未产生输出；ADC（#003, #004）断言直接读取测试输入缓存；MQTT（#206）所谓的 RX 接收实际上是在读取本地 TX 发送缓冲。
2. **时钟系统断裂与时序降级**：
   - LEDC Fade（#049）完全忽略 3000ms 渐变时序，瞬时同步完成；
   - TWDT 看门狗（#154）缺失超时调度，漏喂狗绝不报警；
   - FreeRTOS 统计（#132）将 `vTaskDelay` 休眠阻塞时长累加为 CPU 运行时间，发生内核计费倒错。
3. **证据链防线与门禁漏洞**：
   - `evidence_verifier.py` 仅核对 Summary 汇总数值，完全丢失对 `stepResults` 逐步断言与执行身份的校验（E-1）；
   - Canary 变异击杀将编译器报错和 Wasm 加载崩溃误判为“成功杀死变异”（E-2）；
   - 流水线 Daemon 存在“自测自签”审计证书的闭环回音室风险（E-3）。
4. **负向契约与红测试大面积缺失**：
   - 33 个条目中有 21 项的 `negative_cases` 数组留空为 `[]`，缺乏成对的负向场景文件（`*.fail.scenario.json`）。

### 1.2 核心目标
建立**“底座真实语义治理（Root Remediation） + 正向绿测试（Happy Path） + 逆向红测试（Fault Path） + 三维变异击杀（3D Mutation Kill）”**四位一体的终极高保真仿真可靠性防御体系，彻底拔除表层假绿，确保固件在 Wasm 仿真与 ESP32 物理硬件上的同源行为一致性。

---

## 2. 六大落地战略支柱 (The 6 Strategic Pillars)

### 当前执行批次：Batch 0 证据判定与晋升止血（2026-10-05 用户确认）

本节 Batch 0~2 是先行验证的独立编号；§5 中的 Batch 编号用于后续全量推广，两者按各自标题区分。

本轮先修治理工具的验收判据，再开展领域底座修复。历史提交和文件存在只表示实现资产已落地，不代表本轮业务验收完成；§5.1 的完成声明须按下列标准重新核验。只读基线为 Gate 1 执行 12/12、核验器接受 35/35，35 个已登记 verified 的配置中有 20 个 `negative_cases` 为空。

| 顺序 | 修改范围 | 验收标准 |
|---|---|---|
| B0-1 | `mutator.py` 及结构化报告判定 | 仅接受本轮绑定场景中指定业务断言的实际失败；Runner timeout、步骤 passed 后无关退出、编译/加载失败、错误场景、重复步骤、失败发生在其他步骤均不得计为击杀。 |
| B0-2 | `generate_checklist_v1_1.py` 的双实证判定 | 正常与故障处理报告必须绑定同一应用、配置与产物，且对应场景完整通过；只有 `.fail.scenario.json` 文件、旧报告或其他配置报告不能点亮 Red。故障处理声明须能定位真实故障激励与业务出口。 |
| B0-3 | `pipeline.py` 的候选采集和运行隔离 | 每轮使用 UUID 目录，分开保存基准、自检和恢复报告与输入摘要；只产出 candidate_evidence，停止自签 auditor、自动写 verified、自动渲染正式看板及自动提交。失败保留诊断，不覆盖历史正式凭据或用户修改。 |
| B0-4 | 回归与记录 | 对上述反例、有效断言失败、场景/产物错配、恢复失败及候选流程执行回归；运行 Gate 1、现有凭据只读核验和许可门禁，记录实际结果。 |

本轮不签发审计、不晋升正式凭据。候选包采用独立的版本化格式，不改变现有 `scenario_sha256` 单场景语义或 `delivery_state` 枚举；缺少运行身份或能力时保留明确诊断，不构造成功凭据。完整人工审计后的事务晋升属于后续交付任务。

测试结果口径：正常业务、故障处理及恢复场景均应通过；断言器自检应命中指定断言失败，且只证明 Matcher 活性；固件依赖和有效业务实现变异各自需要独立证据，不能用错误密码、断网或改错预期替代。解除故障/变异后必须重新通过正常基准。

Batch 0 完成后，以 UART Echo 建立正常、故障处理、业务变异与恢复的完整示范，再推进 TWDT、CPU 计费和时钟治理。自动流水线本轮只证明所实际执行的检查类别，不宣称完整业务变异或硬件验收。

#### Batch 0 执行记录（2026-10-05）

- [x] B0-1：用反例复现日志关键字误判后，改为绑定场景/报告哈希、执行身份、目标业务步骤及逐步结果；补充真实 Runner error 诊断、JSON 布尔值错绑和产物篡改回归。
- [x] B0-2：移除 `.fail` 文件存在即亮 Red 的规则；正式双实证辅助格式逐配置绑定正常/故障报告、产物、契约、独立 run 与既有审计。只读渲染结果为 35 项正常实证、0 项双实证；正式看板未写入。
- [x] B0-3：候选流水线完成 UUID 应用副本、独立报告、基线/自检/恢复、失败诊断留存；删除自动签审计、写 verified、生成看板、提交和 Git 回退路径。门禁只排除专用候选命名空间，其他未登记载体仍被拦截。
- [x] B0-4：治理测试 **245 passed**；Gate 1 **12/12 执行、0 skipped、0 errors**；旧登记凭据只读核验 **35/35**；全运行时 layering/API lint 无发现；许可门禁、PowerShell 语法与 `git diff --check` 通过。

真实 UART 候选 run 为 `20261005T115442Z-717fb3d159f64cf083ec97d5487df63c`，保存在本机 `.governance/runs/<run_id>/candidate_evidence.json`（Git 忽略，本轮未晋升）：

| 检查 | 实际结果 | 接受结论 |
|---|---|---|
| 正常基线 | exit 0，3 个步骤全部通过 | 正常业务场景通过 |
| 断言器自检 | exit 1，步骤 0/1 通过，指定步骤 2（零基索引）的 UART TX 业务断言失败 | 断言器活性通过；不能作为业务实现变异或故障处理证据 |
| 恢复基线 | exit 0，3 个步骤全部通过 | 恢复通过 |

三轮资产复合哈希一致：`635551a1fc350d479912a953fae47f6bb0cfb88cccec73add63b3622864f1362`；应用原始输入与副本输入保持稳定。对登记、看板及 35 个正式报告共 **37 个文件**进行 Git 属性规范化后的内容核对，均与初始清洁基线一致。正式 `twin-proof.json` 创建数为 0。

真实重建同时暴露并修复四处存量编译阻断：TWDT 配置成员对齐 `trigger_panic`；内部故障探针头更名为 `esp_sim_fault.h`，消除与 SDK 公共 `esp_fault.h` 的碰撞；ADC reset 使用现有 `started` 状态和既有 PAL stop 参数；删除从未读取的 MQTT RX 私有长度计数。厂商公共头与原厂示例未改。上述修复使 UART 生产 Wasm 重建通过，**不代表 W-3/W-5/W-7 的业务验收完成**。

限制：额外 Host 故障探针测试构建被存量 `esp_cpu.h` 的 static inline 仅声明及 GCC 16 的 `-Werror=unused-function` 阻断，未执行测试，未通过关闭警告规避；本轮未执行 ESP32 真机验证。后续先完成 UART 固件依赖/有效业务变异/适用故障处理示范，再推进 TWDT 专项与 Host 头文件兼容性治理。

#### Batch 1：UART 因果示范（2026-10-05 用户确认继续）

沿用 Batch 0 候选隔离，不写正式清单、审计、凭据或看板。测试边界为候选采集公开入口、公开 CLI/场景报告，以及 UART RX 输入到固件 TX 输出；外部进程可以在治理回归中替代，实际业务结论必须来自真实 Wasm 执行。

| 顺序 | 操作 | 验收标准 |
|---|---|---|
| B1-1 | 锁定 UART1、115200 baud、原厂源文件与 `wasm_sim_standard` | 原厂源文件 SHA-256 与 Manifest 一致；场景输入、目标 TX 断言及期限不变。 |
| B1-2 | 在独立应用副本禁用 `uart_write_bytes` 出口 | 初始化/RX 仍执行；步骤 2 的正确 TX 断言在 1000 ms 窗口末失败。编译或运行器失败不计。 |
| B1-3 | 在独立副本把首字节异或 `1` | 原来的输入和正确预期不变；步骤 2 失败，实际 TX 为 `IELLO_ESP32_WINK`。保存源文件副本、差异、固件及报告摘要。 |
| B1-4 | 原样恢复副本，独立进程启动 | 正常基准通过；恢复源码与基准字节一致、恢复固件哈希与基准一致；每轮固件另存避免覆盖证据。 |
| B1-5 | 检查故障适用性并执行接收边界探测 | 原厂 Echo 不使用事件队列、不处理运行期 RX/TX 错误；既有溢出 `ESP_ERR_NO_MEM` 契约和“换字符串”负例不能作为故障处理。用间隔发送的 4 个 96 字节报文探测累计接收边界，记录通过、业务回归或基础设施错误，不将故障缺口标为完成。 |
| B1-5a | 修复边界探测定位的 Wasm UART 重复缓存 | 已由回调消费的数据不再复制留存于 PAL 256 字节 FIFO；无回调的 polling 路径保持缓存/溢出行为。原厂代码不改，同一 4×96 字节场景由失败变为通过。 |
| B1-6 | 治理回归与原始资产保护 | 自检、固件依赖、业务变异分别判定；失败/异常也必须恢复，拒绝陈旧固件；正式清单、看板、历史报告和原厂示例保持不变。 |

完整故障处理仍须选择具有原厂异常分支的 UART Events 或独立门面契约，并核对公开故障注入与观测支持；不得为点亮 Red 给 UART Echo 添加原厂不存在的处理分支。本批次先形成可复核的因果证据与缺口结论。技术格式见 [候选证据契约](../../zh/tech-designs/esp32/esp-idf-batch0-evidence-contract.md)。

#### Batch 1 执行记录（2026-10-05）

- [x] B1-1~4：新增显式 `--proof-profile uart-causality`，原厂哈希锁定，仅在候选副本实施两种源码变异；每阶段另存源码/差异、场景、真实固件、日志和报告。陈旧固件、错误 TX、矛盾匹配计数、无报告、变异存活、运行器错误和恢复失败均被拒绝，异常后也恢复副本。
- [x] B1-5/5a：真实边界场景复现累计接收丢失，定位并修复 Wasm PAL 的回调交付与 polling FIFO 重复缓存；无回调时保留既有 FIFO 行为。原厂 Echo、SDK 公共头和清单未改。
- [x] B1-6：治理测试 **264 passed**；真实 emcc/Node UART 单元测试 **4 passed**（`-Wall -Wextra -Werror`，含 768 字节回调流、切回 polling 与满缓冲拒绝）。对独立旧源码副本复跑为 **4 tests / 1 failure**，命中回调累计接收缺陷。Gate 1 **12 executed / 0 skipped / 0 errors**；Gate 2 的 PAL 命名规则 **1 executed / 0 errors**，另两条目录未变而 skipped；Gate 3 **1/1 executed / 0 errors**；全运行时 layering/API lint、许可门禁、PowerShell 语法及 `git diff --check` 通过。

保留的真实 Wasm 候选（本机 `.governance/runs/<run_id>/`，Git 忽略）：

| 检查 | 修复前 `20261005T122337Z-067b30ce33ea4a1a93e0d4077ea9f223` | 修复后 `20261005T123057Z-2f432bdb3214495cb2f42c291db2ffa8` |
|---|---|---|
| 正常基线 | 3/3 通过 | 3/3 通过 |
| 断言器自检 | 步骤 2 指定失败 | 步骤 2 指定失败，固件与基准一致 |
| 固件依赖 | 禁用 TX，指定断言失败，实际载荷为空 | 相同结果，变异固件哈希与基准不同 |
| 业务变异 | 指定断言失败，实际 TX 为 `IELLO_ESP32_WINK` | 相同结果，变异固件哈希与基准不同 |
| 4×96 字节间隔接收 | 前两报文完整，第三报文仅 64 字节；步骤 6 在 850 ms 失败 | 9/9 步骤通过，四报文完整 |
| 恢复基线 | 3/3 通过 | 3/3 通过，源码与基准一致、资产哈希恢复 |

修复后基准、自检、接收边界与恢复资产复合哈希同为 `e447dbbf28ed37037c2326cc800ea0a80350fb319abf971d94e8b6fb49d7c546`；两种源码变异分别拥有不同固件和独立资产副本。两个候选的源文件、场景、报告、固件和资产绑定均另行只读复核通过。原生 fail-fast 报告保留 pending 尾部的映射已用 RED→GREEN 回归补齐；修复前候选最初判定保持原样，新判定单独保存在该 run 的 `validation-review.json`。

故障处理适用性结论为 **contract_gap**：Echo 未启用事件队列，读写循环没有异常分支；“溢出返回 ESP_ERR_NO_MEM”不能从其原厂业务链路成立。修复后入口因此仍返回非零、阶段 `UART_CONTRACT`、候选状态 `candidate_incomplete`，尽管 `causality_complete: true`。这表示适用范围和证据缺口，不能点亮 TWIN-PROOF。

保护复核：正式清单、看板及 35 个历史报告共 **37 文件**的 Git 属性规范化内容未变，正式 twin-proof 创建数 **0**；原厂 UART Echo 的 SHA-256 仍为 `573aac433eaf223af1b12d924e2997012d57e8fc579d918250f3c4221df01253`。既有凭据只读核验仍为 **35/35**，不代表重建并验收了这 35 个应用。

后续先基于 UART Events 的实际异常分支确定溢出/帧错误契约，补真实激励、事件观测与恢复，随后推进 TWDT。门面的 `rx_buffer_size` 配置、固定 512 字节容量和完整错误传播仍未修复；本轮未验证 UART timing、ESP32 HIL 或共享运行时下的并发采集。未执行会在原应用目录调度构建的 Gate 4 全量影响回归，不作正式交付结论。

复跑：

```powershell
python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/tools/run_loop.py --app uart_echo --config-id wasm_sim_standard --proof-profile uart-causality
powershell -NoProfile -ExecutionPolicy Bypass -File wink-micro-os/test/wasm/run_uart_rx_semantics_emcc.ps1
```

#### Batch 2：UART Events 故障传播与恢复（2026-10-05 用户确认继续）

原厂 `uart_events_example_main.c` 的哈希固定为 `8d173ac3cd0e1a9fec6dc1bf6de48a865172a43cc5f910635c63e00dbef770de`。本批次继续 Authoring/Reverify，使用候选应用副本与独立 Node 进程，通过现有公开 Wasm C-ABI `pal_wasm_push_uart_rx_error(port, flags)` 注入一次性接收错误；通过生产 JS 桥的 UART TX 捕获原厂任务处理结果。此 harness 是独立、版本化的候选证据，不能伪装成原生 CLI 场景报告或正式双实证。

| 顺序 | 操作 | 验收标准 |
|---|---|---|
| B2-1 | 锁定 Events 原厂源码、ESP32 配置和既有正常场景 | 原厂摘要一致；在候选副本完成真实 CLI 正常、自检与恢复，保存生产资产。 |
| B2-2 | 先复现错误丢失，再补 PAL 与 ESP-IDF 门面传播 | 帧错误 `1`、校验错误 `2`、硬件 FIFO 溢出 `4` 分别到达事件队列；未知位与无效端口不产生错误事件；保留既有 polling/回调 RX 回归。 |
| B2-3 | 在生产 Wasm 中执行故障与同实例恢复 | 正常 RX/TX 先通过且无故障日志；注入后原厂任务分别输出 `uart frame error`、`uart parity error`、`hw fifo overflow`；随后在同一实例收到新的完整 echo。输出必须来自 UART TX，不能用探针自身日志代替。 |
| B2-4 | 隔离业务变异与重建恢复 | 仅在副本禁用原厂错误处理分支，保留初始化和正常回显；相同故障断言失败且无运行器错误，变异固件摘要必须变化。原样恢复源码并重建后，正常与故障检查再次通过且资产摘要恢复。 |
| B2-5 | 候选完整性与门禁复核 | 绑定 harness/故障合同、原厂或变异源码、资产、结构化报告和命令；治理回归、相关门禁、lint、许可检查通过，正式资产与历史证据保持原样。 |

边界：故障入口注入 PAL 接收事件，未模拟线路波形、波特率或 UART 校验检测；原厂正常配置禁用 parity，校验错误检查只证明异常事件传播及处理。固定 512 字节接收容量、事件队列满时的丢弃和软件缓冲溢出压力测试另行治理。无有效独立审计与原生故障场景契约时，不晋升正式 TWIN-PROOF。

#### Batch 2 执行记录（2026-10-05）

- [x] B2-1：在 UUID 副本完成正常 CLI 基准、自检、恢复。新增 `--proof-profile uart-events-fault`，固定原厂摘要及 ESP32 标准配置，普通仿真报告与独立 ABI harness 报告分别保存。
- [x] B2-2：生产 Wasm 复现三种错误日志缺失；PAL 单元测试 **6 tests / 2 failures**，命中帧/校验错误未投递及组合标志丢失。修复 PAL 标志投递与门面 SDK 事件映射后，真实 emcc/Node 单元测试 **6 passed**，既有 RX 回归保持通过。
- [x] B2-3/4：生产 Wasm 的无故障对照、帧/校验/FIFO 错误处理及同实例后续回显通过；副本移除原厂错误分支后，正常 CLI 仍通过，三种指定错误日志断言失败，前后回显通过。还原源码并重建后全部恢复，源码和资产摘要回到基准值。
- [x] B2-5：治理测试 **285 passed**，包括原始 TX 重算、错误端口/窗口/注入、资产错配、陈旧变异固件、运行器异常及恢复失败；Gate 1 **12 executed / 0 skipped / 0 errors**，Gate 2 **1 executed / 2 skipped / 0 errors**（能力目录未变），Gate 3 **1 executed / 0 errors**。全运行时 layering/API lint、许可门禁及差异检查通过。

修复前候选为 `20261005T142826Z-7c419ddc94e1490592d8036a8875878d`：正常 CLI 三项已接受，独立错误检查保存在 `uart-events-fault-review.json` 及 `uart-events-fault-red/`，无故障对照通过，帧/校验/FIFO 三项均因错误日志缺失失败。该补充检查没有修改原有正常候选的判定或正式凭据。

修复后候选为 `20261005T143640Z-b73d3e5e58c243c4bed09f0f7aace65e`：

| 检查组 | 正常业务与错误处理 | 接受结论 |
|---|---|---|
| 原厂正常 CLI | 基准与恢复各 7/7 通过，自检仅步骤 6 指定失败 | 三项正常/自检候选接受 |
| 原厂 ABI 故障组 | 对照、帧错误、校验错误、FIFO 溢出共 4 报告，各自正常、处理及同实例后续回显通过 | 4/4 接受 |
| 错误分支移除 | 正常 CLI 7/7 通过；对照通过，三种故障仅步骤 1 错误日志断言失败，正常及后续回显通过 | 4/4 接受，固件摘要已变化 |
| 还原组 | 正常 CLI 7/7 通过；上述 4 个 ABI 报告再次通过 | 4/4 接受，源码和资产摘要恢复 |

基准与还原资产复合 SHA-256 同为 `a724e4f8a9cb97ef180f641897d8bcabf1c0331304a551947cd9e4f02214321e`，Wasm 摘要同为 `432c77d3949d4e7f24a12b4f290a2991e957cd5bf89c708c48208aae9e06c83a`；变异 Wasm 为 `1943de0c7375d37325c4ef8778751d3908c87f06b334a099b40a5c9219845bf0`。12 个故障报告及其源码、合同、资产绑定用当前判定器另行只读复核通过，原候选文件保持不变，复核记录为 `validation-review.json`。候选状态 `candidate_ready`、`fault_handling_complete: true` 只代表本专项已执行的检查集合。

同领域 Echo 因果回归为 `20261005T144704Z-9468cefebc574e4e8f28bda28132bd76`：六项检查均接受，4×96 字节场景 9/9 通过；仍以 `UART_CONTRACT` 非零保留原厂 Echo 异常处理缺口。正式清单、看板及 35 个历史报告共 **37 文件**的 Git 规范化内容未变，两份原厂 UART 源码摘要不变，旧凭据只读核验 **35/35**，正式 twin-proof 创建数 **0**。

执行期间，首次重建命令因隔离目录风险被自动审批拒绝，未执行；核对实际 UUID 副本及报告路径后获准执行。一个本轮 pytest 失败副本触发 Gate 1 未登记载体拦截，已验证路径后移入工作区 `build/` 留存，最终回归与门禁通过，未放宽拦截规则。未运行会覆盖原应用资产的 Gate 4，也未进行 UART timing 或真机 HIL 验收。下一专项为 TWDT：先复现漏喂狗不报警，再确定虚拟时钟上的超时、重新喂狗与复位契约。

#### Batch 3：TWDT 自动超时与恢复（2026-10-05 用户确认继续）

本轮沿用已确认的公开 C/SDK API、生产 Wasm ABI、候选 CLI/报告边界。原厂 `system/task_watchdog/task_watchdog_example_main.c` 固定 SHA-256 `5855b893bf480e818fb8421b23bcbdd50480c9d21a013725de9df14e4e56dc96`，只在 UUID 副本中分别省略任务、func_a、func_b 喂狗；正常生命周期仍须完成，不能把编译失败或进程崩溃当作超时检出。

依据本机锁定的 ESP-IDF v6.1 `components/esp_system/task_wdt/task_wdt.c`（公共 API 说明：[Espressif TWDT 文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html)），采用共享计时周期：所有订阅者的 `has_reset` 就绪才刷新共同期限；超时报告本周期漏喂者。替换原计划的“逐任务 elapsed > timeout”模型。复用已有 FreeRTOS generation-tagged work item，在虚拟时间到达期限时自动检查；不增加每 tick 全池扫描或独立 watchdog fiber。

先运行生产资产的正常控制与三个漏喂 RED，再补实现、重放 GREEN。新增隔离 SDK 测试适配器只调用公开 API，并通过公开 Wasm 时钟、ISR 用户钩子、诊断文本与复位 ABI 观察：共同期限、非 panic 继续喂狗、诊断查询无复位副作用、panic 原因、复位后旧 handle 拒绝及重新初始化。适配器的源码、构建输入与生产资产独立绑定，不能冒充原厂应用输出。

范围限制：合作式 Wasm 调度能检查让出 CPU 后的任务/用户漏喂；空闲核监控、永久不让出 CPU 的忙循环抢占及真实硬件 ISR 时序本轮不作保证。`idle_core_mask` 未建模的部分必须告警并留在候选缺口中。结果只留存候选，正式 registry、历史资产/报告与 CHECKLIST 不写回。

#### Batch 3 执行记录（2026-10-05～2026-10-06）

自动超时 RED 留存于 `.governance/runs/20261005T151110Z-4dc929384c4443a79dd47586aefd91d3/red-review/`：完整 16 秒观察中，正常/还原控制通过，任务/func_a/func_b 三个漏喂版本只因缺失自动报警而失败，原厂生命周期与退订后静默均通过。首次 14 秒窗口不足的报告保留为诊断，不计入恢复验收。panic RED 为 `sdk-499f3300bd634bbfac6861051e5e2dd5`：自动 ISR 钩子与诊断 CPU 位图通过，唯独复位请求缺失。句柄 RED 为 `sdk-039f46b6681f432a9aef967f818cba3b`：删除并复用池槽后旧句柄错误地返回成功。

修复发生在内仓 TWDT 门面与复位桥：现有定时器 work item 自动检测共同期限，非 panic 报警后继续执行；诊断查询无 panic 副作用，cpus_fail 为 CPU 位图；可覆盖 ISR 用户钩子带仿真 ISR 限制；panic 通过现有待复位 ABI 请求 WATCHDOG，完成复位后 SDK 原因保留 TASK_WDT。用户句柄使用不可解引用的校验 token，独立单调序列跨删除/复位保留，避免 ABA；deinit 有活跃订阅时拒绝，清理时取消旧 work token。厂商公共头、PAL 签名与原厂源码未修改。

最终生产候选为 `.governance/runs/20261005T224907Z-9773718525a64441b177acfb13cc9b53/candidate_evidence.json`，`automatic_timeout_complete=true`。正常 CLI 基准/断言自检/恢复 3 份报告接受；三个漏喂与最终还原的原生生命周期报告均通过。5 份独立 ABI 超时/恢复报告全部接受：漏喂版在订阅后恰好 3000000 us 首次报警，随后在 6000000、9000000 us 报告同一漏喂者；退订后无残留报警。基准与还原的生产资产 composite SHA-256 同为 `922bc851d70b207eafe28e84efa351fea5b05b7caadaa2359b30e8d08f6b171e`，Wasm SHA-256 同为 `23504a2c73ac01078335e61850a74f4c6b86a16a3c860b46f63b08540afca401`；三个漏喂固件摘要分别不同。

最终 SDK 补充候选为 `.governance/runs/sdk-bd5231dcf9fb421590e4d0fa5b27bb9b/evidence.json`。3 个独立实例共 62 项检查通过：panic 21 项、共同周期/非 panic 恢复 28 项、删除复用及外来句柄 13 项；相关运行时源码已冻结并核对构建前后未变。包括诊断查询不触发复位、主机 WATCHDOG=2、复位后 SDK TASK_WDT=6、旧期限清理、应用重启、复位/复用后旧句柄拒绝、重新喂狗后持续静默。SDK 适配器只验证公开 API，不冒充原厂应用故障场景。

治理回归 **307 passed**（新增 TWDT 报告/CLI 22 项）。最终 Gate 1：12 executed / 0 skipped / 0 errors；Gate 2：1 executed / 2 skipped / 0 errors；Gate 3：1 executed / 0 errors。分层/API lint、许可门禁、Node 语法检查及 diff whitespace 通过。正式清单、CHECKLIST 与 35 份历史原生报告共 37 个文件与 Git 基线内容一致；原厂源哈希保持固定。校验记录在 `build/twdt-validation/`。

执行中 Windows 长路径、场景必填字段及 SDK 100 Hz 下 1 ms 转零 tick 阻断过测试适配器；已修正并终止仅本轮停滞进程，相关失败不算业务 RED。自动审批一度因使用额度耗尽未能完成，越过恢复时间后通过审批继续；最终构建完成。样式修正引出的有符号索引编译错误已修复，未关闭 Werror。未进行会覆盖原目录资产的 Gate 4、Host 头兼容修复或真机 HIL；共享有限定时器池饱和、空闲核及忙循环抢占仍列为缺口。W-3、W-4（CPU 微秒级运行记账）已彻底闭环落地。下一优先项为 Batch 0 治理防线加固（pipeline.py 解耦与沙箱、ADC 因果闭环）及 Batch 1~3 成对红测试成套交付。

```
                              【高保真可靠性实证金字塔】

                     ▲       Pillar 4. 三维变异击杀引擎 (3D Mutation Engine)
                    / \      Pillar 2. 成对红绿确定性场景 (Twin Scenarios: Green & Red)
                   /   \     Pillar 3. 底座受控故障注入探针 (Controlled Fault Probes)
                  /=====\    Pillar 1. 负向契约元数据全量补齐 (Negative Contract SSOT)
                 /=======\   Pillar 5. 治理防线铁门与双实证看板 (Gate & Dual-Proof CI)
                /=========\  Pillar 0. 评审暴露缺陷专项治理工单 (Remediation Worklist)
```

---

### Pillar 0: 评审暴露缺陷专项治理与因果拓扑重构 (Remediation Worklist)

针对评审中定性的 S-1~S-5 及 Q-1~Q-7，立项 8 大专项治理工单：

* **W-1 (S-1 / Q-1: PAL DAC 纯增量与 ADC 模拟总线重构)**：
  - 严格遵循 **ADR-0092 Tier 1** 纯增量原则，在 `wink-micro-os/pal/include/hal/` 新增 `pal_dac.h`，无 `esp_*.h` 跨平台洁癖；
  - 同步交付 `targets/wasm/` 虚拟电学总线与 `targets/esp32/` 物理驱动，返回值严格遵循 ADR-0001 负数错误码；
  - 重构 ADC 仿真因果链：断言必须读取固件输出的工程值（如校准 $mV$），彻底切断直接读取输入缓存的短路。
* **W-2 (S-2: LEDC Fade 虚拟时间轮步进)**：
  - 引入离散事件虚拟时间轮（VTE），按毫秒计算阶梯 PWM 步进积分；
  - 占空比严格执行 **ADR-0066** 定点基点规范（`pal_pwm_set_duty_bp` / `PERMILLE`）；
  - 异步分发完成中断，恢复 3000ms 渐变时序断言。
* **W-3 (S-3: TWDT 离散事件超时判定)**：
  - 对齐 ESP-IDF 的共同喂狗周期，全部订阅者喂狗后刷新共同期限，使用既有虚拟定时器 work item 自动检测；
  - 确立任务/用户漏喂报警、非 panic 恢复和 panic 复位原因的逆向判定；空闲核及忙循环抢占单独验收。
* **W-4 (S-4: FreeRTOS 真实微秒计费模型)**：
  - 废除 `vTaskDelay` 累加运行时间的倒错逻辑；
  - 接入 Fiber 上下文切换钩子（`traceTASK_SWITCHED_IN/OUT`），仅对任务在 CPU 上运行的微秒时间片积分。
* **W-5 (S-5: `esp_idf_bridge_reset` 全生命周期清理)**：
  - 将 ADC、DAC、TWDT 等门面的静态对象池完整纳入软复位销毁范围，确保多场景连续运行无状态污染。
* **W-6 (Q-2: GPTimer 计数寄存器 ABI 导出)**：
  - 在 Wasm Target 中导出真实计数器 ABI（`sim_timer_get_counter`），运行时缺失观察 ABI 时显式 Fail-Loud；
  - 闭环 Alarm 队列分发与重载周期断言。
* **W-7 (Q-3: MQTT 独立双 FIFO 协议栈解耦)**：
  - 废除静态共享变量 `s_last_data`，彻底拆分独立的 `tx_fifo` 与 `rx_fifo`；
  - 由虚拟 Broker 模拟器依据 Topic 路由真实投递，固件事件循环消费。
* **W-8 (Q-4: NVS Blob 官方越界缺陷隔离登记)**：
  - 恪守原厂镜像 0 修改（SHA-256 锁定）红线，在 `.governance/specs/upstream_errata.json` 中正式登记官方越界 Bug；
  - 场景精准断言前两项合法 CRC，对越界日志制定明确捕获策略。

---

### Pillar 1: 负向契约元数据全量补齐 (Negative Contract SSOT)

**目标**：消除 `checklist.data.json` 中 `negative_cases` 留空现象，为全部条目建立标准化形式化负向模型。

1. **元数据 Schema 严格标准化**：
   每个负向用例必须完整声明三要素：
   - `stimulus`（故障激励）：外部注入的具体故障（如 `i2c_nack_address`、`wifi_wrong_password`、`nvs_key_nonexistent`）；
   - `expect_error`（预期表现）：固件底座抛出的标准化错误符号或串口错误日志（如 `ESP_ERR_TIMEOUT`、`ESP_ERR_WIFI_NOT_FOUND`）；
   - `detects`（防假绿目标）：此负向用例专门查杀的底座作弊行为（如 `prevent_empty_stub_ack`、`detect_silent_success_fallback`）。
2. **存量条目契约补齐**：梳理原厂源码，提取错误处理分支，全量更新 `checklist.data.json`。
3. **门禁规则升级 (Gate 1)**：凡 `delivery_state == 'verified'` 的条目，其 `negative_cases` 长度必须 `>= 1`。

---

### Pillar 2: 成对红绿确定性场景规范 (Twin Scenarios: Green & Red Pair)

**目标**：废除单一绿测试文件结构，强制推行**成对场景文件标准**。

1. **场景文件命名与职责划分**：
   - `unisim-scenarios/<app>.scenario.json`（**绿测试**）：黄金正常路径，验证端到端业务成功；
   - `unisim-scenarios/<app>.fail.scenario.json`（**红测试**）：故障注入路径，验证异常捕获与稳态防御。
2. **红测试断言设计标准**：
   - **严禁崩溃**：红测试必须验证仿真器不出现内存越界、死锁或宿主静默退出；
   - **故障感知断言**：断言日志输出标准错误宏，断言状态机迁移至异常态（如 `wifi:sta:state -> DISCONNECTED`）；
   - **时序窗口**：红测试应在故障注入后 $50\text{ms} \sim 200\text{ms}$ 内迅速捕获错误，超时即判失败。
3. **首批打样推进（6 大黄金用例成对化）**：
   - `blink`（时钟源骤停）；`i2c_basic`（从机 NACK 与无应答）；`uart_echo`（RX 溢出与校验错误）；
   - `wifi_sta`（密码错误拒绝关联）；`http_client`（404/500 与 DNS 失败）；`nimble_beacon`（非法广播参数）。

---

### Pillar 3: 底座受控故障注入探针体系 (Controlled Fault Probes)

**目标**：在 `frameworks/esp_idf` 与 `pal` 仿真适配层中建立标准化、零污染的受控故障注入通道。

1. **统一故障注入接口**：
   ```c
   // 仿真受控故障通道接口 (纯仿真编译期隔离)
   wink_status_t sim_esp_fault_inject(uint32_t domain, uint32_t fault_type, uint32_t param);
   wink_status_t sim_esp_fault_clear(void);
   ```
2. **四大领域故障探针实现**：
   - **I2C/SPI 总线**：`FAULT_I2C_NACK`（模拟从机不存在）、`FAULT_I2C_TIMEOUT`（SCL 锁死）；
   - **Wi-Fi / Netif 网络**：`FAULT_WIFI_AUTH_FAIL`（AP 握手失败）、`FAULT_NETIF_DHCP_TIMEOUT`（DHCP 超时）；
   - **NVS 键值存储**：`FAULT_NVS_PARTITION_FULL`（空间不足）、`FAULT_NVS_READ_CORRUPT`（页损坏）；
   - **NimBLE 蓝牙**：`FAULT_BLE_ADV_REJECT`（广播参数被拒绝）。
3. **跨平台洁癖防御**：故障探针使用 `#if defined(__EMSCRIPTEN__)` 严格收敛于仿真底座，严禁渗透通用 PAL 头文件。

---

### Pillar 4: 三维变异引擎升级与击杀判据加固 (3D Mutation Engine)

**目标**：将 `CanaryMutator` 进化为三维变异矩阵，并彻底修复 E-2 虚假击杀漏洞。

1. **三维变异矩阵模型**：
   - **维度 A (Assertion Mutant)**：篡改预期值验证断言器非盲（Matcher 活跃度）；
   - **维度 B (Stimulus Mutant)**：篡改外部环境与输入激励载荷，验证固件能否感知环境劣化；
   - **维度 C (Platform Fault Mutant)**：打入底层硬件故障探针，验证错误防御分支是否 100% 被覆盖。
2. **重构 `verify_kill` 判据状态机（修复 E-2 漏洞）**：
   ```text
   变异执行结果判定:
   ├── exit_code == 0                         ──► [FAIL-GREEN] 致命假绿: 变异存活，立即拦截
   ├── exit_code != 0 && (编译/链接/加载失败)   ──► [INFRA_CRASH] 基础设施崩溃: 一票否决，绝非有效击杀
   └── exit_code != 0 && (命中目标断言精准报错) ──► [KILL-SUCCESS] 真实击杀: 变异被有效拦截，允许晋升
   ```

---

### Pillar 5: 治理防线铁门、证据链防伪与双实证看板 (Gate & Dual-Proof CI)

**目标**：修复 E-1、E-3、E-4 证据漏洞，建立防伪、事务性隔离与红绿双实证看板。

1. **修复 E-1：`evidence_verifier.py` 实施全量逐步 $O(N)$ 校验与 Merkle 证据树**：
   - 必须读取场景文件提取所有预期步骤；
   - 严格遍历 `stepResults` 数组：必须满足 `len(stepResults) == len(scenario.steps)`，且每一步状态必须为 `passed`；
   - 计算所有步骤序列的 SHA-256 签名并与产物绑定，彻底杜绝篡改 Summary 绕过。
2. **修复 E-3：权力制衡机制（Separation of Powers）**：
   - 流水线 Daemon 仅有权产出 `candidate_evidence` 候选凭证包；
   - 封堵 `pipeline.py` 自签 `auditor` 的后门，审计字段必须经双盲对抗审查裁判 Agent 或架构团队签署。
3. **修复 E-4：沙箱隔离与事务性晋升**：
   - 每次运行建立唯一的 `.governance/runs/<timestamp>-<uuid>/` 隔离目录；
   - 全部门禁通过后原子性晋升至正式凭据库，执行失败则完整回滚。
4. **单向派生双实证看板 (`generate_checklist_v1_1.py`)**：
   - 在 `CHECKLIST.md` 渲染器中新增状态列：`[Green ✅ | Red 🛡️]`；
   - 仅当正向场景通过且负向场景精准拦截时，才点亮最高信任度标记 `🟢 VERIFIED (TWIN-PROOF)`。
5. **双 Target 物理实机交叉核验**：
   - 对 6 大黄金用例（`blink`, `ledc`, `gptimer`, `uart_echo`, `http_client`, `wifi_sta`）通过 `wink.py esp32` 在 ESP32 物理硬件上复验，保证同源行为一致。

---

## 3. 时间线与交付里程碑 (Timeline & Milestones)

| 阶段 | 时间周期 | 重点交付目标 | 验收交付物 |
| :--- | :---: | :--- | :--- |
| **M0: 状态止血与证据铁门加固** | Day 1 ~ Day 2 | 1. 修复 E-1：`evidence_verifier.py` 落实逐步强一致性校验；<br>2. 修复 E-2：`mutator.py` 剥离 INFRA_CRASH；<br>3. 修复 E-3/E-4：剥离自签后门并确立 UUID 运行沙箱；<br>4. 在看板中对 10 个 C 类缺陷条目打上 `needs_remediation` 止血。 | 加固版 `evidence_verifier.py`；<br>加固版 `mutator.py`；<br>止血后的看板状态。 |
| **M1: 契约奠基与底座时钟/复位重构** | Day 3 ~ Day 6 | 1. 落实 W-3 (TWDT 超时判定) 与 W-4 (FreeRTOS 真实微秒计费)；<br>2. 落实 W-5 (`esp_idf_bridge_reset` 对象池全生命周期清理)；<br>3. 补齐全量条目的 `negative_cases` 元数据模型 (Pillar 1)。 | 升级版 `esp_task_wdt.c`、`freertos_task.c`、`esp_idf_bridge.c`；<br>`checklist.data.json` 契约补齐。 |
| **M2: 领域外设因果链与成对场景打样** | Day 7 ~ Day 10 | 1. 落实 W-1 (PAL DAC 纯增量驱动与 ADC 模拟总线重构)；<br>2. 落实 W-2 (LEDC Fade 虚拟时间轮步进，ADR-0066 定点 bp)；<br>3. 落实 W-6 (GPTimer 真实 ABI 导出) 与 W-7 (MQTT 独立双 FIFO)；<br>4. 交付 6 大黄金用例成对红绿场景 (`*.fail.scenario.json`)。 | `pal_dac.h` 及双 Target 驱动；<br>升级版 `esp_ledc.c`、`esp_gptimer.c`、`esp_mqtt.c`；<br>6 组双向测试报告。 |
| **M3: 底座探针与三维变异引擎闭环** | Day 11 ~ Day 13 | 1. 交付 I2C、Wi-Fi、NVS、BLE 四大受控故障探针 (Pillar 3)；<br>2. 升级 `mutator.py` 支持三维变异击杀矩阵 (Pillar 4)；<br>3. 自动化流水线变异击杀演练。 | `esp_fault.c` 探针库；<br>三维变异击杀实证实录。 |
| **M4: 双 Target 实机比对与双实证看板交付** | Day 14 ~ Day 16 | 1. 在 ESP32 物理硬件上交叉比对 6 大黄金用例；<br>2. 升级 `generate_checklist_v1_1.py` 渲染双实证看板；<br>3. Gate 1~5 全量门禁复验与正式归档。 | ESP32 硬件比对报告；<br>`CHECKLIST.md` 双实证展示；<br>全流程正式交付包。 |

---

## 4. 风险控制与不可触碰红线 (Risk Governance & Redlines)

1. **零原厂源码污染红线**：
   严禁为了通过测试而在原厂业务源码（`main.c` 等）中插入 `#ifdef SIMULATION` 或异常分支。原厂代码必须 100% 保持上游 SHA-256 镜面一致。
2. **PAL 纯增量演进红线 (ADR-0092 Tier 1)**：
   新增通用外设抽象必须纯增量添加，严禁破坏既有签名，严禁引入 `esp_*.h` 专有类型，必须同时提交 Wasm 仿真桩与 ESP32 物理驱动。
3. **故障隔离红线**：
   故障注入探针必须在测试结束后提供干净的 `sim_esp_fault_clear()`，严禁在多用例批处理时残留故障状态导致后续用例假红。
4. **二值化审计与非崩溃红线**：
   负向测试必须断言明确的错误码或预期的错误表现；严禁将任何未知的崩溃、堆栈溢出、死锁或段错误泛化视为“红测试通过”。

---

## 5. 详细任务分解与执行跟踪清单 (Task Breakdown & Execution Tracker)

### 5.1 历史实现资产与待复验基线 (Historical Implementation Baseline)

以下提交编号用于追溯历史实现。2026-10-05 复核发现 TWDT 自动超时调度、CPU 微秒计费、精准击杀及双实证判定尚未满足本文验收标准；勾选仅保留历史落地记录，不能作为晋升依据。现有 Blink、UART、TWDT 的 `.fail` 文件分别覆盖正常时序、正常回显与生命周期，尚不能视为对应故障处理证明。
- [x] **Core 1 (Pillar 3 探针架构)**: `esp_fault.h/c` 探针库，打通 I2C/SPI/NVS/WiFi/BLE，Wasm ABI 导出，复位清理链与单测 (`ee4f3f0a`)。
- [x] **Core 2 (Pillar 4 变异矩阵)**: `mutator.py` 三维变异矩阵与 3 状态判据机加固（杜绝虚假崩溃判定，`f33159d3`）。
- [x] **Core 3 (Pillar 5 门禁升级)**: `evidence_verifier.py` $O(N)$ 逐步校验；`generate_checklist_v1_1.py` 双实证标记 (`6eed120d`, `f33159d3`)。
- [x] **Core 4 (Pillar 0 阻断修复)**:
  - [x] W-1: `pal_dac.h` 纯增量 HAL 及 Wasm/Host/ESP32 驱动 (`6b913b1c`)；
  - [x] W-2: `esp_ledc.c` 离散时间轮与定点 bp 步进 (`b915db8f`)；
  - [x] W-3: `esp_task_wdt.c` Tick 钩子超时判定与漏喂狗报警 (`74b62b5d`)；
  - [x] W-4: `freertos_task.c` 微秒级真实 CPU 运行计费与 Fiber 上下文切换积分已彻底重构落地（遵循 [2026-10-06-w4-freertos-microsecond-runtime-stats-hardening-plan.md](2026-10-06-w4-freertos-microsecond-runtime-stats-hardening-plan.md)，提交 `e7b877b7`, `3e3293ae`, `11fde1dc`）；
  - [x] W-5: `esp_idf_bridge.c` 外设对象池完整复位销毁 (`74b62b5d`, `ee4f3f0a`)；
  - [x] W-6: GPTimer 硬件计数器 ABI 导出 (`b915db8f`)；
  - [x] W-7: MQTT 独立双 FIFO 环形缓冲解耦 (`b915db8f`)；
  - [x] W-8: NVS 越界缺陷隔离登记至 `upstream_errata.json`。
- [x] **Core 5 (Pillar 2 标杆打样)**: 交付首批 7 大领域成对红测试（Blink GPIO, UART Echo, LEDC Fade, TWDT, WiFi STA, HTTP Client, NimBLE Beacon，`46b899fe`）。

---

### 5.2 待执行分批推进图谱 (Remaining Execution Batches)

```
Batch 0: 治理防线加固与因果短路修复（pipeline.py 解耦 + ADC 因果重构）
   │
   ├──► Batch 1: Lane 1 & Lane 5 核心红测试成对化（内核调度 + NVS 存储，共 12 项）
   │
   ├──► Batch 2: Lane 2 & Lane 4 总线与模拟外设红测试（I2C/SPI/UART + ADC/DAC，共 9 项）
   │
   ├──► Batch 3: Lane 3 & Lane 6 定时电机与网络协议红测试（LEDC/Timer/WiFi/MQTT，共 7 项）
   │
   ├──► Batch 4: SSOT 元数据补齐与 35/35 双实证全亮（CHECKLIST.md 最终派生）
   │
   └──► Batch 5: M4 物理硬件实机比对（通过 wink.py esp32 / run_esp32_headless_evidence.ps1 硬件核验）
```

#### Batch 0: 治理防线加固与因果短路修复
- [x] **Task 0.0 (B0-1/B0-2)**: 优先修复结构化指定断言击杀与同配置双实证判定，并复现拒绝 Runner timeout、步骤 passed 后无关退出、只有 fail 场景文件等反例。（已完成：落地 `report_contract.py` 与 `twin_evidence.py`，加固 `mutator.py` 三态判定，并在 `test_batch0_evidence.py` 交付 55 项严苛对抗测试 100% 通过）
- [x] **Task 0.1 (E-3 权力制衡)**: 改造 `wink-micro-app/vendor/esp_idfv61/.governance/tools/loop/pipeline.py`：（已完成：废除硬编码 `"auditor": "loop_sop_daemon"`，流水线仅输出 `candidate_evidence.json` 候选凭证包，禁止自动 git 提交，由外部审计核验）
  - 废除硬编码 `"auditor": "loop_sop_daemon"`；
  - 流水线仅输出 `candidate_evidence` 候选凭证包，由独立审计裁判 Agent 进行离线核验；
  - 封堵自测自签安全隐患。
- [x] **Task 0.2 (E-4 运行隔离沙箱)**: 在 `pipeline.py` 中引入 `.governance/runs/<timestamp>-<uuid>/` 隔离沙箱：（已完成：运行隔离在独立沙箱目录，仅当门禁全部通过后原子性提升至正式目录）
  - 保证每次测试与变异在独立沙箱内运行；
  - 门禁全部通过后原子性晋升正式目录，失败完整回滚。
- [x] **Task 0.3 (W-1 未尽细节: ADC 真实工程输出因果闭环)**:（已完成：彻底闭环 ADC Oneshot 与 Continuous 场景，断言固件真实 UART 打印工程量 `Cali Voltage: 775/1550/2325 mV` 与 DMA 解析工程值 `Value: 1982/3301`，原厂 C 代码 0 修改，35/35 carrier evidence 100% 验证通过）
  - 改造 `peripherals/adc_continuous_read` 场景：废除断言模拟轨输入缓存 `target: "adc:34" == 0.484`，改为断言固件串口输出工程日志 `Value: 1982/3301` 与通道配置；
  - 改造 `peripherals/adc_oneshot_read` 场景：改为断言固件校准工程值（`Cali Voltage: 775/1550/2325 mV`）。

#### Batch 1: Lane 1 (内核调度) & Lane 5 (文件存储) 核心示例成对红测试 (12 项)
- **Lane 1: 系统生命周期与内核调度 (8 项)**
  - [x] **Task 1.1**: `#002 get-started/hello_world` ➔ `hello_world.fail.scenario.json`（注入启动异常断言稳态防御）
  - [x] **Task 1.2**: `#020 peripherals/gpio_generic_gpio` ➔ `generic_gpio.fail.scenario.json`（注入非法输入与中断抖动断言）
  - [x] **Task 1.3**: `#125 system/esp_event_default_event_loop` ➔ `default_event_loop.fail.scenario.json`（注入事件循环未启动/handler 异常）
  - [x] **Task 1.4**: `#126 system/esp_event_user_event_loops` ➔ `system_esp_event_user_event_loops.fail.scenario.json`（注入队列超限与循环强制终止）
  - [x] **Task 1.5**: `#127 system/esp_timer` ➔ `system_esp_timer.fail.scenario.json`（注入周期为 0 与非法参数拒绝）
  - [x] **Task 1.6**: `#131 system/freertos_basic_freertos_smp_usage` ➔ `system_freertos_basic_freertos_smp_usage.fail.scenario.json`（注入队列满阻塞与超时捕获）
  - [x] **Task 1.7**: `#132 system/freertos_real_time_stats` ➔ `real_time_stats.fail.scenario.json`（注入计数溢出与无有效时钟源防御）
  - [x] **Task 1.8**: `#151 system/startup_time` ➔ `system_startup_time.fail.scenario.json`（注入启动超时报警）
- **Lane 5: 存储与文件系统 (4 项)**
  - [x] **Task 1.9**: `#401 storage/nvs_nvs_iteration` ➔ `storage_nvs_nvs_iteration.fail.scenario.json`（注入未初始化的命名空间与空迭代器）
  - [x] **Task 1.10**: `#402 storage/nvs_nvs_rw_blob` ➔ `nvs_nvs_rw_blob.fail.scenario.json`（注入键不存在 `ESP_ERR_NVS_NOT_FOUND`，校验 `upstream_errata` 隔离）
  - [x] **Task 1.11**: `#403 storage/nvs_nvs_rw_value` ➔ `nvs_nvs_rw_value.fail.scenario.json`（注入 `FAULT_NVS_READ_CORRUPT` 断言错误码）
  - [x] **Task 1.12**: `#415 storage/spiffs` ➔ `storage_spiffs.fail.scenario.json`（注入挂载损坏分区与读取不存在文件防御）

#### Batch 2: Lane 2 (通信总线) & Lane 4 (模拟电学) 核心示例成对红测试 (9 项)
- **Lane 2: 通信协议与串行总线 (5 项)**
  - [x] **Task 2.1**: `#023 peripherals/i2c_basic` ➔ `i2c_basic.fail.scenario.json`（通过 `FAULT_I2C_NACK` 注入从机无应答并断言返回 `ESP_ERR_TIMEOUT`）
  - [x] **Task 2.2**: `#024 peripherals/i2c_i2c_eeprom` ➔ `peripherals_i2c_i2c_eeprom.fail.scenario.json`（注入写保护及响应超时）
  - [x] **Task 2.3**: `#073 peripherals/spi_master_hd_eeprom` ➔ `spi_master_hd_eeprom.fail.scenario.json`（通过 `FAULT_SPI_TRANSFER_FAIL` 注入总线阻断）
  - [x] **Task 2.4**: `#094 peripherals/uart_uart_async_rxtxtasks` ➔ `peripherals_uart_uart_async_rxtxtasks.fail.scenario.json`（注入异步 RX 环形缓冲区溢出）
  - [x] **Task 2.5**: `#098 peripherals/uart_uart_events` ➔ `peripherals_uart_uart_events.fail.scenario.json`（注入 UART 校验与 FIFO 溢出事件断言）
- **Lane 4: 模拟电学与信号转换 (4 项)**
  - [x] **Task 2.6**: `#003 peripherals/adc_continuous_read` ➔ `peripherals_adc_continuous_read.fail.scenario.json`（注入采样通道未使能与过采样超时断言）
  - [x] **Task 2.7**: `#004 peripherals/adc_oneshot_read` ➔ `adc_oneshot_read.fail.scenario.json`（注入非法通道号断言 `ESP_ERR_INVALID_ARG`）
  - [x] **Task 2.8**: `#013 peripherals/dac_dac_cosine_wave` ➔ `peripherals_dac_dac_cosine_wave.fail.scenario.json`（注入越界频率断言参数拒绝）
  - [x] **Task 2.9**: `#014 peripherals/dac_dac_oneshot` ➔ `dac_dac_oneshot.fail.scenario.json`（注入向已禁用通道写入电压断言返回错误）

#### Batch 3: Lane 3 (定时电机) & Lane 6 (网络与无线) 核心示例成对红测试 (7 项)
- **Lane 3: 定时器、计数与电机控制 (2 项)**
  - [ ] **Task 3.1**: `#047 peripherals/ledc_basic` ➔ `ledc_basic.fail.scenario.json`（注入非法占空比 `>10000bp` 断言参数拒绝）
  - [ ] **Task 3.2**: `#083 peripherals/gptimer_alarm` ➔ `gptimer.fail.scenario.json`（注入 Alarm 计数值为 0 或未启动计数断言）
- **Lane 6: 网络、协议与无线 (5 项)**
  - [ ] **Task 3.3**: `#206 protocols/mqtt_tcp` ➔ `mqtt_tcp.fail.scenario.json`（注入 Broker 连接拒绝与断线重连防御）
  - [ ] **Task 3.4**: `#221 wifi/fast_scan` ➔ `fast_scan.fail.scenario.json`（注入扫描超时与无匹配 SSID）
  - [ ] **Task 3.5**: `#223 wifi/getting_started_softAP` ➔ `softap.fail.scenario.json`（注入不合规密码与启动参数校验失败）
  - [ ] **Task 3.6**: `#230 wifi/scan` ➔ `scan.fail.scenario.json`（注入 Wi-Fi 驱动未启动即发起扫描断言错误）
  - [ ] **Task 3.7**: `#384 bluetooth/bleprph` ➔ `bleprph.fail.scenario.json`（通过 `FAULT_BLE_ADV_REJECT` 注入广播参数被拒绝）

#### Batch 4: SSOT 元数据补齐与 35 个目标配置逐项结论
- [ ] **Task 4.1**: 补齐全量 35 项的 `negative_cases` 元数据模型（`stimulus`, `expect_error`, `detects`）写入 `checklist.data.json`。
- [ ] **Task 4.2**: 运行 `evidence_verifier.py --verify-all` 确保 35 个示例正向逐步强一致性断言 100% 通过。
- [ ] **Task 4.3**: 运行 Gate 1~5 全部门禁通过无告警（0 warnings, 0 errors）。
- [ ] **Task 4.4**: 运行 `generate_checklist_v1_1.py`，按有效证据生成 `CHECKLIST.md`；仅对同配置正常与故障处理证据完整的条目点亮 TWIN-PROOF，其余保留真实缺口，不以全亮作为验收要求。

#### Batch 5: M4 阶段 ESP32 物理硬件实机交叉核验
- [ ] **Task 5.1**: 硬件测试环境确认（ESP32-WROOM/S3 开发板连接、COM 口识别）。
- [ ] **Task 5.2**: 使用 `run_esp32_headless_evidence.ps1` 和 `wink.py esp32` 对 6 大黄金用例（`blink`, `ledc`, `gptimer`, `uart_echo`, `http_client`, `wifi_sta`）烧录物理硬件。
- [ ] **Task 5.3**: 捕获芯片真实物理串口日志，提取时序哈希，与 Wasm 仿真 Trace 比对，输出《双 Target 物理实机交叉核验实证报告》。
