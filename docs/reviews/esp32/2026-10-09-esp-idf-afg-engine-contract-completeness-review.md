<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ESP-IDF 防假绿引擎契约完整性与全量迁移覆盖评审

| 项 | 内容 |
|---|---|
| 日期 | 2026-10-09，Asia/Shanghai |
| 类型 / 结论 | 技术契约评审快照；**Needs revision：方向成立，尚不适合冻结为统一强制验收契约** |
| 被评审文档 | [AFG-Engine v1.0](../../zh/tech-designs/esp32/esp-idf-anti-false-green-verification-engine-contract.md) |
| 工程基线 | `09d002f7b34dd4b9b585e164a119642785af071a` |
| 关联材料 | [Harness 契约](../../zh/tech-designs/esp32/esp-idf-loop-reliability-contract.md)、[Batch 0 契约](../../zh/tech-designs/esp32/esp-idf-batch0-evidence-contract.md)、[既有整改计划](../../implementation-plans/esp32/2026-10-09-esp-idf-loop-issues-and-remediation-plan.md)、[分类规范](../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md)、[能力字典](../../../wink-micro-app/vendor/esp_idfv61/.governance/catalog/capability-catalog.yaml) |
| 评审边界 | 全量清单与上游入口静态核对，关键领域源码及 API 语义精读；未重新构建、执行仿真或烧录；本记录不改变原契约、凭据、审计或交付状态 |

## 1. 结论

契约已抓住几个必要方向：正向基线不能单独证明正确性；反向证伪必须归因到目标断言；观测必须经过真实业务因果链；连续生产不能依靠 Getter 偷推时间；恢复与机器凭据必须成为交付条件。将 AFG 算法内核与 Harness 的运行隔离、审计及事务发布分开，也有利于长期维护。

但目前把这些方向写成了过强的普适规则，同时缺少能够机器执行的适用性、业务覆盖、观测可信性和证据绑定协议。直接照此实现，存在两种结果：合法示例因不适用的物理要求被误拒绝；核心业务未进入能力清单或 ProofPlan 的示例仍然可以假绿。

**最优先解决的是：证明对象是否完整、配置身份是否真实、不同检查应当如何判定、证据是否确实来自本次执行。增加更多物理扰动或白盒字段不能替代这四件事。**

有限的场景、故障与变异不能证明“绝对无假绿”。建议将承诺改为：在已冻结的业务声明、配置、保真边界与缺陷模型内，所有必需证据完整且可复核；明确剩余未验证范围。`verified` 必须携带这套限定，不能暗示所有芯片、真实射频、抢占竞态或物理时延均已证明。

## 2. 全量范围与核验方法

### 2.1 清单范围

以 [checklist.data.json](../../../wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json) 为机器事实源，而非只读取 Markdown 已勾选项。

| 范围 | 示例数 | 配置数 / 说明 |
|---|---:|---|
| 全部登记 | 478 | 479 个 execution 配置 |
| 纳入迁移 `in_scope` | **312** | **313** 个配置 |
| 当前推进 `active` | 291 | 292 个配置 |
| 暂缓 `deferred` | 21 | 19 个构建示例、2 个 OTA 示例；仍属于契约长期覆盖范围 |
| 产品范围外 `out_of_scope` | 166 | 不计入当前迁移完成率 |
| 登记为 `verified` | 46 | 均为 `wasm_browser / esp32 / standard` |
| 尚未验证的迁移配置 | 267 | 包括 Wi-Fi STA 额外的 `wasm_sim_node` 配置 |

313 个迁移配置分为 `wasm_browser=292`、`host_native=20`、`wasm_node=1`。全部登记 `target_soc=esp32`、`profile=standard`。不能用某个 Browser 配置的通过覆盖 Node、Host 或其他芯片。

| 上游领域 | 纳入迁移 | active | deferred | verified 配置 |
|---|---:|---:|---:|---:|
| bluetooth | 49 | 49 | 0 | 2 |
| build_system | 19 | 0 | 19 | 0 |
| cxx | 3 | 3 | 0 | 0 |
| get-started | 2 | 2 | 0 | 2 |
| network | 5 | 5 | 0 | 0 |
| peripherals | 81 | 81 | 0 | 16 |
| protocols | 35 | 35 | 0 | 8 |
| storage | 27 | 27 | 0 | 4 |
| system | 67 | 65 | 2 | 9 |
| wifi | 24 | 24 | 0 | 5 |
| **合计** | **312** | **291** | **21** | **46** |

### 2.2 实际读取与限制

- 读取全部 478 条清单记录，逐条核对 312 个迁移条目的配置、能力、保真及验收元数据。
- 通过项目路径解析器定位本机 SDK：`D:/software/embedded-tools/esp-idf/.espressif/v6.1/esp-idf`。312 个上游目录全部存在；`tools/cmake/version.cmake` 声明 `6.1.0`，路径解析器无法提供不可变 Git revision。因此这里是本机 SDK 快照，不能将路径名称当作上游提交身份。
- 对全部 312 个目录扫描 README 支持芯片表与 C/C++/汇编源码 include 入口；直接 README 有 307 份，递归源码命中 519 次，包含父子示例目录重叠，**不是 519 个互不重复实现的逐行人工审查**。重点精读 I3C、异步 CRC、BLE Central、LP SPI、Hello World、UART Echo，以及 ADC/I2C/SPI 官方接口契约。
- 仅 46 个条目的 `target_app_dir` 当前实际落地；其他 266 个未落地条目的分析依据是上游源码入口及清单元数据，不能宣称其 Wink 实现已经通过。
- 本轮只读 Gate 1：12 条执行、0 跳过、0 错误；既有凭据核验器：46/46 接受。这两项仅说明**当前核验器接受存量数据**，不代表它已经执行本文建议的 AFG 协议。
- 在治理 Python/JSON 与内核公开 C 源码范围检索，未找到本文契约提出的 `pal_sim_get_probe`、AFG 回执消费及 `L1-OP-*` 实现；未发现命名为 `check_capability_symbols.py` 的工具。因此本评审把它们视为拟议接口，不能按“已实现能力”引用。

### 2.3 能力与配置存在前置缺口

能力字典确有 **20 个前缀、63 个原子能力**，这一数量无误。但 312 个迁移示例仅引用其中 **46 个**，14 个被引用能力仍登记为 `planned`。20 行代表项矩阵不等于 63 项逐项契约，更不等于 312 个业务证明已覆盖。

| 已确认的映射现象 | 对防假绿的影响 |
|---|---|
| 27 个 `system/ulp/*` 全部未引用 `cap.coproc.*` | 可只证明主核/休眠辅助链，漏掉 LP/ULP 程序真实执行 |
| 19 个 `build_system/*` 全部未引用 `cap.build.*` | 第 20 类算子不会自然绑定这些构建示例 |
| `bluetooth/nimble/blecent` 声明 GATT Server，未声明 GATT Client；源码实际调用 `ble_gattc_read/write_flat` | 广播、连接成功可能替代发现、读写、订阅等核心客户端业务 |
| `NimBLE_Security` 未声明 `cap.ble.smp_security` | 普通连接通过可能替代配对、权限和加密证明 |
| `system/deep_sleep` 声明 `light_sleep`，未声明 `deep_sleep` | “Entering deep sleep”日志可能替代深睡复位与保留域证明 |
| 6 个 I2S 示例只有 2 个引用 `cap.proto.i2s_stream`；2 个 DMA 示例未引用 `cap.dma.*`；2 个 Touch 示例未引用 `cap.pulse.touch_pad` | 核心流式、DMA、触摸功能没有稳定的能力闭包入口 |
| I3C、LCD、模拟比较器、Bitscrambler、以太网桥接等核心语义在现有 63 项中没有足够明确的对应项 | 不能仅靠“20 类全覆盖”宣称业务无遗漏 |
| 42 个示例仅声明 `cap.core.*` | 不是 42 个错误的统计，但其中包含 I3C、触摸、DMA 等明确的业务遗漏候选；必须从上游反向审计 |
| 全部 312 条 `header_closure` 与 `sdkconfig_overrides` 为空 | 空值不能证明已经解析真实头文件、条件编译与编译配置闭包 |
| 81 个上游 README 支持芯片表未列 ESP32，而迁移配置全写 ESP32 | 配置身份存在需逐项裁定的冲突信号；I3C 与异步 CRC 明确只列 ESP32-P4 |

81 是**静态支持表冲突信号**，不是仅凭 README 就裁定 81 个迁移全部非法。需要结合 Kconfig、组件目标限制、SoC 宏和明确的跨芯片行为模型裁定。若实际仿真目标与物理芯片不同，必须分别记录，禁止通过 `esp32` 通用标签掩盖。

## 3. 问题与修订建议

优先级用于契约收敛：**P0 表示冻结统一契约前必须解决的证明完整性问题；P1 表示相关领域实施前必须补齐的语义或工程规范。**不将其等同于现有产品已经发生的运行时故障。

### AFG-R01 / P0：缺少从上游业务到证明任务的完整性约束

**依据：**AFG §3、L264–287，仅按 capability 前缀列代表规则；上节列出的映射缺口已存在。当前 `cap.core.fiber_task/sync_tokens` 被全部 312 个示例引用，不能因此要求每个应用都调用表中同一组任务/互斥函数。

**问题：**引擎可以严格执行一份不完整的 ProofPlan，仍然遗漏真正业务；由同一个生成者缩减 capability、claim、assertion 后，所有检查都能通过。

**建议条款：**建立 `upstream source/README intent → app business claims → atomic capabilities → config → scenario/assertion → evidence obligations` 的双向覆盖索引；区分应用核心业务、平台依赖和辅助断言。冻结前由独立复核确认业务声明完整；未映射功能输出 `CLAIM_COVERAGE_GAP`，不得生成可晋升结果。声明删减、容差放宽、故障路径删除均重新审计。

**验收：**故意删除 BLE Central 的读写/订阅 Claim，或删除 LP 程序执行 Claim，即使剩余全部通过也必须拒绝候选；不能仅靠现有 `required_capabilities` 反推完整业务。

### AFG-R02 / P0：芯片、后端与保真身份尚未成为判定输入

**依据：**AFG L11 仅写 xtensa；回执 L301–303 仅有 app/config/target；当前清单 313 个配置全部标 ESP32，但存在 81 个支持表冲突信号。

**问题：**P4 I3C、LP 核及 RISC-V IPC 等例子不能由 xtensa 的统一描述涵盖；Browser、Node、Host 的通过不可互换。SMP、射频、休眠及实际物理时间也超出同一行为模型的统一保证。

**建议条款：**冻结实际 `backend / physical_soc / simulated_soc_or_model / profile / SDK revision / sdkconfig digest / fidelity axes / concurrency model / model version`；原厂支持目标检查与跨芯片模拟映射分开。必要 API 不具备时报告能力缺口，不改 scope 隐藏。保真降级必须明确影响哪些 Claim；未接受降级的 Claim 保持未验证。

**验收：**将 P4-only I3C 作为原生 ESP32 配置提交必须拦截；如声明可移植行为模型，必须显式批准该映射和物理验证限制。STA Browser 凭据不能晋升其 Node 配置。

### AFG-R03 / P0：七类证据缺少适用性协议，六条公理过度普适化

**依据：**AFG L44–49、L93–103 要求每个能力物理扰动、内部探针、外部敏感性；L189 排除日志作为成功依据。

**问题：**Hello World、RTTI、异常、构建工具、稳态 PWM、自主闪灯没有相同的物理输入和探针义务。强制制造无关扰动，会鼓励测试者添加无关断言凑数。

**建议条款：**七类检查都必须登记，但每项具有 `applicability_rule_id / required_when / status / rationale / reviewer_binding`。状态区分 `PASS / FAIL / NOT_APPLICABLE / MISSING / UNSUPPORTED / SKIPPED / ERROR`。N/A 仅由已冻结的领域契约判定；缺实现、缺探针、业务已有负例未完成均不能记为 N/A。核心业务必须具备有效固件依赖和非等价缺陷敏感性证据；环境输入敏感性只对依赖该输入的 Claim 生效。

**验收：**Hello World 的真实输出及重启可通过适用检查；将尚未实现的 ADC 探针写 N/A 必须拒绝。核心断言和辅助电源断言不能以同一“每条断言必须变异”的规则计数。

### AFG-R04 / P0：故障处理成功与变异击杀被混为同一极性

**依据：**AFG L74–83 把硬件故障和源码变异放在同一流程，并将“目标断言 PASS”判为假绿；L102 却要求故障处理按预期降级或自愈。

**问题：**正确检测断网并断言 `DISCONNECTED` 的故障场景应当 PASS，按时序图却会成为 `MUTANT_SURVIVED`；反之，故障处理崩溃可能被错误包装成击杀。

**建议条款：**分开定义四种期望：正常基线业务断言 PASS；断言器自检命中指定检查错误；实现变异保持原业务 oracle 不变并使指定断言 FAIL；故障处理场景以错误码/降级/退出/复位等**预期行为断言 PASS**。恢复再执行原基线。统一结果字段表达“本检查是否满足期望”，另外保留被观察断言的 PASS/FAIL，禁止混用。

**验收：**正确断网处理被接受；禁掉业务接收后正常载荷断言失败才计固件依赖证据；改错 Matcher 只能计断言器自检。L1 环境故障不得替代必须的 L2/行为变异。

### AFG-R05 / P0：正常镜像、变异副本与业务特化旁路边界未写清

**依据：**AFG §2.7 允许源码 Patch，但没有定义正常输入与变异输入的隔离、允许修改对象及不可变校验；治理 SOP 要求原厂镜像源码不变。

**问题：**为了适应通用“自愈/重试”要求修改原厂 App，或者在门面中识别应用名直接填充结果，都可能让契约表面成立而失去同源业务意义。

**建议条款：**基线绑定原厂源树及必要适配的审计哈希；Canary 只在隔离副本修改冻结的目标，保存 before/patch/after 内容与作用域。禁止修改正常 oracle、scope、审计、原始资产；业务生成器与门面不得通过 app_id/示例常量分支实现测试结果。原厂与通用门面的不同证明义务明确分开。

**验收：**变异直接编辑正常源树、同时修改 oracle，或向门面添加 EEPROM 示例专用成功分支均拒绝；合法的专属虚拟 EEPROM 器件模型可以存在，但必须位于独立环境模型中并经过真实 SPI 命令链。

### AFG-R06 / P0：缺少观测与 oracle 的可信边界，白盒不等于独立证明

**依据：**AFG L45、L47、L189 将隔离通道和内部状态作为真实性保障，却没有定义数据来源与 oracle 归属。

**问题：**同一假门面可同时返回 `ESP_OK`、设置 busy/token、伪造正确探针；同一生成者可让测试期望与错误实现一同变化。两条不同通道也可能都读取 Fixture 缓存。

**建议条款：**每个核心观察值绑定 `producer / device-or-logical-endpoint / direction / data provenance / observation phase / validity / freshness`。Fixture 仅提供环境输入；独立监视器记录固件/协议栈实际出口，禁止直接给 Sink 填入预期。oracle 从已冻结的上游业务、规范向量或独立参考计算取得；探针用于补充不变量，不能自证算法正确。

**验收：**固件禁用而 Fixture 保持运行时，业务断言必须失败；旧缓存、缺信号、无效探针和超时不得默认转换成合法的 0、空串、成功码。

### AFG-R07 / P1：“零回环”应改成“禁止绕过固件的自证路径”

**依据：**AFG L123–124、L350 要求镜像输入必须经过物理衰减。

**问题：**UART Echo、WebSocket Echo、SPI 主从、I2S 回送，以及 ADC 测量外部已知输入，本来就可能输入输出相等。衰减既非必要条件，也不足以证明业务参与。

**建议条款：**校验因果图，而非简单值相等：`stimulus → input endpoint → firmware/API processing → output endpoint → monitor`。合法回环明确方向、连接和观察层；断言直接读取激励寄存器才是自证旁路。对 Echo 使用多个不重叠载荷、顺序、长度、延迟和固件依赖变异。

**验收：**RX 输入后由固件经 TX 输出相同字节可以通过；Fixture 直接 RX→TX 复制，即使加了延迟或衰减，也必须失败。

### AFG-R08 / P1：固定物理下限与所有动作非零时间不成立

**依据：**AFG L127–130、L269、L276、L351：转换至少一个采样周期、Flash 固定 10ms/100μs、Wi-Fi 至少 500ms，任何同 Tick 操作都判假。

**问题：**采样周期、转换时长、帧传输时间、API 返回时间和首次结果可用时间是不同量；同步逻辑操作可在同一调度 Tick 完成。固定常量未绑定芯片、器件或已接受保真等级。现有 LEDC 覆盖矩阵还登记了即时阶跃/同步回调降级，不能无迁移协议就重新定义其通过含义。

**建议条款：**为每个相关 Claim 定义时间模型与依据，包括时钟域、起止事件、频率、帧长、分辨率、容差和已接受降级。异步生产按虚拟时间/事件调度推进；API 同 Tick 返回不等于异步硬件已完成。Flash、无线握手等下限来自模型参数或标定依据，不设全局魔数。不能把整数纳秒级协议要求隐含塞进微秒观测分辨率。

**验收：**同 Tick 配置寄存器可接受；ADC 未推进时钟却生成新样本、LEDC 声明平滑渐变却瞬间完成，应被相应模型拒绝。即时阶跃只能证明已明确允许的降级子集。

### AFG-R09 / P1：暂停消费 100ms 必须溢出不是通用异步判据

**依据：**AFG L204–212、L269。官方 `adc_continuous.h` 本身还定义 `max_store_buf_size`、`conv_frame_size`、`flush_pool` 与 `on_pool_ovf`；SPI DMA 可按 flags 选择临时缓冲，而非一律拒绝非 DMA 输入。

**问题：**有限生产、足够容量、流控、覆盖旧数据、丢新数据、循环 DMA、停止生产均可能合法。自动清错与统一 `ESP_ERR_INVALID_STATE` 不能覆盖 UART、I2S、ADC、网络的不同 API。

**建议条款：**以 `capacity / production rate / frame size / flow control / drop-or-overwrite policy / callbacks / read return semantics` 参数化。先证明不读也按时产生数据，再依据具体配置选择不会满、恰好满、越界、背压和恢复边界；用实际容量和速率计算窗口。消费者暂停不得顺便停止整个虚拟时钟。

**验收：**流控生效避免溢出不应被拒绝；buffer 未满却报告溢出、Getter 被调用才开始生产必须失败；ADC、UART、I2S 分别按自己的错误与数据丢失语义断言。

### AFG-R10 / P1：错误码跨 ABI 约定混淆

**依据：**AFG L231 写“负错误码（如 ESP_ERR_TIMEOUT）”；本仓 `esp_err.h` 定义 `ESP_ERR_TIMEOUT=0x107`、`ESP_ERR_INVALID_STATE=0x103`。AFG L185 的 PAL 草案返回裸 `int`，未声明 Wink 状态约定。

**问题：**Wink PAL 负码、ESP-IDF 正 `esp_err_t`、BSD `-1 + errno`、NimBLE `BLE_HS_* / ATT` 不能统一按负码裁判。矩阵以 Bluedroid 的 `esp_ble_* / ESP_GATT_*` 对账 NimBLE 示例，同样可能误判。

**建议条款：**在 Claim 与回执中记录 `error_domain / API / numeric value / symbolic name / optional errno / expected set`；门面忠实保持原 API，跨层转换只在明确边界发生。新增 PAL 查询使用 `wink_status_t` 和负数错误码；不要为了统一引擎而改变原厂 ABI。

**验收：**合法 ESP-IDF 正错误码、BSD errno 与 NimBLE 错误均正确匹配；I2C NACK、地址探测失败和总线忙超时不能统一期待 `ESP_ERR_TIMEOUT`。

### AFG-R11 / P0：探针协议缺少版本、有效性、生命周期和观察纯度

**依据：**AFG L172–185 用一个通用结构混合 heap、FIFO、ISR、power、token；L353 要求 token 必须变化。

**问题：**不是所有实例都有上述字段，0 可能是合法值；`uint32_t handle` 没有类型、epoch、实例域和失效语义；读取本身若推进时间或改变状态，白盒断言就会影响结果。未销毁实例的 token 理应保持稳定。

**建议条款：**定义 versioned、typed、只读、原子快照的 Probe Descriptor，包含结构尺寸、capability/type、实例与 generation/epoch、virtual timestamp、validity bitmap、单位及能力支持集合。未支持/未注册必须 Fail-Loud；分域数据避免巨型不断增长结构。生命周期定义创建/销毁/复位何时变化、旧句柄何时失败。通过公开 C-ABI 对外暴露；跨 MCU PAL 保持通用，ESP 私有诊断不强塞进 PAL。

**验收：**同 Tick 连续读取不推进时间、不生产数据；非法句柄不返回全 0 成功；实例未重建时 token 不变通过，重建后旧代句柄拒绝，跨 run 不串数据。

### AFG-R12 / P1：能力不能等同于最终二进制导出符号集合

**依据：**AFG L146–158，从 `owned_paths` 推导 C ABI，并将 gc-sections 缺符号解释为能力冗余要求删除。

**问题：**文件归属不是必需符号规范；宏、inline、LTO、内部函数和最终产物符号剥离都影响可见性。Wasm 内部驱动不必导出到宿主；保留一个空函数符号也不能证明实现有效。`sdkconfig.h` 与 CMake 的 `idf_component_register` 更不是运行时导出符号。

**建议条款：**将检查拆成：显式公开 ABI 的 export/import 契约；构建依赖/编译单元/link map 的保留证明；运行期业务路径激活与实际出口证据。每项按 target/toolchain/优化模式定义 `required_exports / imports / link_witness / runtime_witness`，允许规范化的内联证据。缺观察能力输出证据缺口，不自动删 SSOT capability。

**验收：**合法内联或剥离内部符号的固件可由替代见证通过；仅导出同名成功空桩仍被业务变异与 oracle 拒绝。

### AFG-R13 / P0：缺少变异生效、实际装载及目标归因的强绑定

**依据：**AFG §2.1.2 仅列运行现象，§4.1 只记 patch hash 和 step；未证明 Patch 进入实际构建、实际固件被装载以及目标操作已被触发。

**问题：**修改未编译文件、禁用无效宏、加载旧 Wasm、Patch 没匹配到源码或只改变注释，都可能错误计作实现证伪。业务期限违约也可能是真正缺陷，不能一律归入“算子设计缺陷”的挂死。

**建议条款：**算子记录目标路径/编译单元/前置条件/语义变更/选定缺陷/预期 assertion ID；保留 Patch 应用、构建配置、产物、加载回执与注入激活见证。分类增加 `NOT_APPLIED / NOT_LOADED / NOT_ACTIVATED / IRRELEVANT / EQUIVALENT / INCONCLUSIVE / INFRA_ERROR`；只有有效非等价变异导致目标谓词在合同窗口内失败才计击杀。区分执行器墙钟超时和正常推进下的业务 deadline 违约；预期 panic 的 API 误用测试单独隔离，不计普通业务击杀。

**验收：**对未编译 reset 文件打 Patch、仍加载原资产或未触发注入必须拒收；目标业务不再在期限内输出且观察器正常时，可以判目标 deadline 失败，不能简单按基础设施超时抹掉。

### AFG-R14 / P1：最多两个 L2 算子不能替代充分性，等价裁定不能补位

**依据：**AFG L249、L255–258。

**问题：**一个复杂状态机可能有多个独立失效机制；两个错误算子无法自动证明全部 Claim。一般语义等价也不总能提供可计算的数学证明；把等价变异“特许放行”可能被误计成必要的有效击杀。

**建议条款：**先定义每个 Claim 的缺陷模型和必需算子族，再设置执行预算；预算耗尽仍有未证明义务时输出 `INCOMPLETE`。等价变异附配置域、输入域、理由及独立裁定，从有效算子分母排除，**不计击杀、不填补必需证据，必须换非等价算子**。存活但尚未裁定的结果保持待补证据；禁止默认白名单。

**验收：**两个等价/无关变异不能晋升；存在第三个必需缺陷族而预算不足时不得以“2/2 已执行”完成。

### AFG-R15 / P0：构建缓存键缺少完整依赖闭包

**依据：**AFG L249 仅以 `(source_sha256, patch_sha256)` 缓存 Wasm。

**问题：**门面、PAL、头文件、SDK、编译器、链接参数、sdkconfig、生成资产或目标发生变化，缓存仍可能返回旧产物；这是既有整改计划已经要求避免的旧资产风险。

**建议条款：**明确 source hash 的覆盖清单并采用完整构建输入内容寻址：App 源码、运行时/门面/头文件闭包、SDK/component lock、effective sdkconfig、toolchain、flags、链接/生成脚本、backend/SoC/profile、patch。执行结果缓存另绑定 scenario、fixture/fault plan、模型/引擎/Matcher 版本与 seed。缓存命中仍校验产物哈希及加载身份。

**验收：**仅修改被依赖头文件、驱动或 sdkconfig 时必须失效；不同配置不可串缓存；命中旧编译结果不能继承旧执行证明。

### AFG-R16 / P0：故障的抽象层、激活见证与处理策略没有契约化

**依据：**AFG L230–232、L271、L275–286，将所有瞬态故障绑定重试、所有永久故障绑定报错或 2PC/自愈。

**问题：**有些原厂例子选择 fail-fast 或重启，并没有自动重连；SPI 物理传输完成也不自然意味着外部器件给出 ACK。网络丢包可能由 TCP 重传完全屏蔽；BLE Legacy 广播长度规则不能套到扩展广播。没有实际注入命中的记录，故障“执行过”只是配置声明。

**建议条款：**Fault Descriptor 定义协议层/器件/方向、参数范围、调度时刻/窗口、随机种子、触发次数、实际生效证据、可见后果与撤销协议。依原厂业务和 API 判定允许结果：重试、拒绝、超时、退出、复位或保持可用；不得强制原厂增加策略。未经对应模型支持的电阻短路、CRC、射频碰撞明确记为模型能力缺口；API 返回码级注入不能冒充真实协议/物理层证明。

**验收：**故障配置存在但未命中设备时拒绝；TCP 合法重传恢复不能因未向 App 暴露一次丢包而失败；SPI 断 CS 应按器件协议输出/业务时限判断，不能笼统强制 `spi_device_transmit` 返回 `ESP_ERR_TIMEOUT`。

### AFG-R17 / P0：恢复不变性没有区分易失、持久和保留状态

**依据：**AFG L103、L268、L273、L279、L327–337、L353 统一要求 heap=0、token 变化、trace hash 相同、静态清零和 2PC 自愈。

**问题：**NVS、文件系统、OTA、RTC 保留区需要保留特定数据；长期任务和运行时缓存可合法保有分配。物理断电、App 重启、Wasm 新实例、场景复位和进程被杀不是同一种 reset。全量 trace 含运行身份时也不会逐字节相同。

**建议条款：**定义 reset taxonomy 及每类状态的 `retain / reset / invalidate / reconcile` 表；指定初始化/清理完成的测量点、允许常驻资源和按 owner 的净增量。资源泄漏判断使用可解释的基线差额和多次生命周期趋势。故障结束必须撤销注入并校验激活计数清零；恢复比较规范化的业务投影和时序，不强制原始 trace hash 相等。持久化验证覆盖提交前后断电切点、旧/新一致态与损坏拒绝，不将特定 2PC 算法强加到所有文件系统。

**验收：**NVS 提交成功后冷启动仍可读；未提交写入按真实 API 契约裁定；RTC 保留数据与普通 BSS 分别验证；反复 create/destroy 不净增长；静态缓存允许但必须有明确所有权与上限。

### AFG-R18 / P1：领域矩阵存在不正确或配置相关的统一判据

**依据：**AFG §3 的 20 行矩阵。

**问题与修订：**

| 领域 | 现有表述的问题 | 应补充的判定 |
|---|---|---|
| Crypto | 单个密文对必须差异 ≥40% 不是正确性保证；正确软件算法不是空操作；原始 CBC API 不必自行检测 IV 重用 | 标准向量、长度/模式/状态与 AEAD 鉴别失败；随机统计与性能/加速保真另立 Claim，不以单次雪崩阈值验算法 |
| BLE | NimBLE 用 `esp_ble_*`/`ESP_GATT_*` 对账；31 字节只适用 Legacy 广播；并非所有配对都需 PIN | 绑定 Host 栈、广播类型、角色、IO/security 配置；按 NimBLE/ATT 错误域；发现、CCCD、通知/指示、权限、bond 持久性与多连接隔离分别证明 |
| OTA | 签名验证、自动回滚及 bootloader 执行依赖配置和产品边界 | 镜像合法性、写分区、选择启动分区、镜像状态与启用的回滚协议分别定义；未模拟 bootloader 不宣称真实启动回滚已证明 |
| DMA | 所有非 DMA 指针都必须拒绝、所有双缓冲都必须半/全中断 | 按 API、对齐、内存能力与是否允许 bounce buffer 判定；定义所有权、完成时序、输出正确性与各自回调语义 |
| IRQ/SMP | 必须模拟优先级嵌套、所有 ISR 阻塞均以同一函数崩溃判定 | 对齐合作式 fiber/虚拟 ISR 的明确能力；验证上下文规则和 happens-before；不将单核仿真扩张成真实 SMP 竞态证明 |
| Camera/Display | 全黑画面一律假；相机表覆盖不了 LCD、JPEG 和像素格式转换 | 使用已知非恒定图案、像素位置/色序/步长/帧完整性与归还语义；黑帧可合法，不能仅按非零判真 |
| Wi-Fi/Network | 连接必须 ≥500ms、故障一定指数退避；事件/STA 覆盖不了 VLAN、桥接、TLS、MQTT5、NAN、FTM | 每个协议/业务模式单独声明：认证、路由/转发方向、载荷、报文状态、计时模型和射频限制 |
| Storage | 所有存储都按 raw Flash 按位与/固定擦写延迟与 Inode=0 判断 | raw Flash、NVS、RAM VFS、文件系统、镜像生成与宿主工具分层；只按公开 API 与选定介质语义断言 |
| Build | `sdkconfig.h` 不是二进制 ABI；配置变化不保证最终二进制字节变化 | 验证配置解析、依赖图、编译命令、预处理/链接效果与应受影响的行为；无语义影响配置不能要求 hash 必变 |

**验收：**每个原子能力补充完整适用条件与依据；代表 API 表只能作为导航，不能成为所有示例必须调用相同函数的规范。

### AFG-R19 / P0：回执是示例 JSON，尚未形成可复算的机器证据契约

**依据：**AFG L295–340。`$schema` URL 不是已交付的 JSON Schema；示例 `app_id=peripherals/spi_master_hd_eeprom`、`config_id=standard` 不符合本清单身份，算子 ID 也不符合前文命名。

**问题：**一个 canary、几个汇总数和 `state_restored=true` 不能证明全部 Claim、七类义务、所有必要场景与步骤完整执行。引擎自己填写两个 true 更不能证明时延和符号校验真实完成。

**建议条款：**提供版本化 Schema 及语义校验规范；绑定 run/attempt、实际配置、source/runtime/SDK/toolchain、产物加载身份、ProofPlan/scenario/model/fixture/seed、算子版本与 Patch、raw report/trace/probe 引用及哈希。每个 Claim/evidence class/case/operator/scenario/assertion 都有稳定 ID、预期执行集合、实际执行集合、原始 observed/expected、时钟域与可追溯判据。拒绝缺失、重复、跳过、额外替代项、错误类型/单位、未知字段版本及过期数据。建议“击杀回执”只存击杀详情，再由完整 AFG result envelope 汇集七类证据；也可一个文件，但结构必须清晰。

**验收：**仅复制基线汇总、漏掉某 Claim 的变异或恢复、替换旧报告、填 null/true 掩盖无观测、把 planned Node 配置贴 Browser 回执均拒绝。摘要字段必须能由原始内容独立复算。

### AFG-R20 / P0：引擎结果、独立审计与 SSOT 晋升职责冲突

**依据：**AFG L40 签发 VERIFIED、L357 写 SSOT `status` 仅由引擎驱动；L19 又声明不涉及 CAS 发布。分类规范实际字段为 `executions[].delivery_state`；Harness §7 还要求密封、独立审计和回归门禁。

**问题：**AFG 结果被当成交付裁决会绕过范围、审计、依赖和发布事务；“禁止任何人工干预”也与独立审计、等价裁定及受控状态回退冲突。`engine_signature` 的 SHA-256 字符串只可能是摘要标识，并不自动构成真实签名或授权证明。

**建议条款：**AFG 输出 `ELIGIBLE / INCOMPLETE / REJECTED / ERROR` 等**候选结果**，不修改 SSOT。Harness/独立 Inspector 校验身份、完整包、适用门禁、能力与审计后，通过已有发布协议修改准确配置的五态 `delivery_state`。回执 hash 与认证签名分字段；若需要密码学签名，明确签名对象、canonicalization、key identity、授权、校验与轮换；否则直称 digest，禁止将其当独立审计身份。

**验收：**AFG 成功但 audit 缺失、scope 不符、依赖不齐或包已改变时，发布必须拒绝；机器不得代填架构审计员；Review/Reverify 保持登记状态。

### AFG-R21 / P1：Runner–Engine 的可执行接口及引擎自身的反例套件缺失

**依据：**AFG L19、§2 与 Harness §4 只描述调用关系，没有冻结请求/响应、支持协商、取消、失败与兼容版本协议。

**问题：**工具实现可能各自猜测谁推进时间、谁重启、谁应用变异、谁产生原始报告以及何时完整；引擎有 Bug 时，强制引擎回执反而成为新的单点假绿。

**建议条款：**冻结 EngineRequest/EngineResult、能力协商、输入只读/输出归属、version compatibility、单时钟 Gate 接口、取消/超时分类、预算不足、恢复失败与重复请求规则。读取观察值不能推进时间；Harness 负责进程与资源隔离，Engine 只在受控接口请求执行。引擎必须通过第 6 节的确定性反例与合法例套件，核验器根据原始数据复算，不能只信 Engine 的 verdict。

**验收：**未知版本、解析不支持、未完成、取消、恢复失败、报告被截断均不能返回可晋升；合法循环与有效故障处理也必须有正例防止误拒绝。

### AFG-R22 / P1：新旧契约迁移与“已覆盖”的度量仍需明确

**依据：**AFG v1.0 同时写 Active / Proposed Specification；L264 宣称杜绝漏网，§4 将无新回执一律未验证；Harness §7 允许历史只读诊断、禁止未经新证明重新发布。

**问题：**提议文档不能自动覆盖已接受的规范。历史 `verified` 代表旧证据门槛，并不代表新 AFG 已通过，也不应在只读评审中被批量改写。按前缀数或击杀个数计覆盖率，会掩盖遗漏 Claim、无效算子或未执行配置。

**建议条款：**明确 Proposed/Accepted 生效时点、与 Harness/Batch 0/SOP 的优先级及迁移策略。每个历史配置保留 evidence contract version，重新交付要求当前版本；发布与新完成率只计当前契约有效凭据。度量分别统计业务声明完整率、适用义务完成率、有效非等价算子击杀、模型能力就绪、配置/场景覆盖、无效/存活/等价/未裁定结果；不以六条公理或 20 类覆盖率承诺绝对无遗漏。

**验收：**46 项历史核验通过只能显示旧版本事实；缺新证明的配置不能被报告为“AFG 全量验证完成”。

## 4. 全部迁移领域应补充的业务契约

以下是从 312 个迁移示例入口归纳的契约族。它们是**逐个示例制定 Claim 的核对清单**，不是新增一张宽泛矩阵后就自动判定覆盖。每个具体配置仍须冻结源码依据、观察出口、oracle、适用负例和容差。

| 契约族 | 对应示例范围 | 必须证明的业务 | 典型有效变异 / 必须明确的边界 |
|---|---|---|---|
| START | get-started 2 | 真正输出、倒计时/重启、GPIO 周期与配置 | 屏蔽输出、错误周期、跳过重启；自主行为无需外部物理输入 |
| ADC-CMP | ADC 2、Comparator 2 | 多输入采样、校准、阈值/回差/ETM、连续时间与帧 | 冻结采样、错通道、错比例/比较阈值；Comparator/ETM 不是 ADC 同义词 |
| DAC-SDM | DAC 4、Sigma-Delta 2 | 直流输出、波形、音频缓冲、频率/幅值与滤波模型 | 错采样序列、幅值/频率、缓冲回调；不以“非零”代替正确波形 |
| GPIO | GPIO 2、Dedicated GPIO 3 | 方向、边沿、键盘扫描与软件总线位时序 | 跳过驱动、行列串扰、位序/时序错误；显式芯片约束 |
| DMA-BITS | DMA 2、Bitscrambler 1 | CRC 参数/结果、颜色转换像素、程序变换、异步完成/所有权 | 错多项式、色序/步长、禁用完成回调；不能只测 heap/busy |
| I2C-I3C | I2C 5、I3C 2 | 真实地址/命令/寄存器、从机角色、器件输出和协议限制 | 错地址/命令、跳过状态、错误字节序；I3C 目标与 API 单独绑定 |
| I2S | 6 | STD/PDM/TDM、通道/slot、编解码器配置、录制与音频数据 | 错 slot、采样顺序/频率、消失一声道；背压和录制持久化按配置 |
| LCD | 6 | 面板初始化、像素/窗口、触控、JPEG 解码、刷新顺序 | 错色序/窗口/stride、坏 JPEG、禁刷新；需显示/图像 oracle |
| LEDC | 4 | 稳态频率/占空比、渐变、gamma 曲线、通道独立性 | 错分频/占空比/曲线；即时降级与平滑子集明确区分 |
| MCPWM | 6 | 有刷/霍尔闭环、FOC/SVPWM、舵机、同步、捕获测距 | 错换相/极性/死区/捕获计算；Plant 与开环控制分开 |
| PARLIO | 3 | RX 采样/时间、TX 矩阵行列、帧完整性 | 交换数据线/bit order、缺行/样本、回调时序 |
| PCNT | 1 | 正反向编码、相位、去毛刺与计数边界 | 交换 A/B、方向位、过滤阈值；无输入不应凭空增长 |
| RMT | 7 | DShot、NEC、WS2812 两种 encoder、蜂鸣器、1-Wire、步进电机各自载荷/时序 | 错编码/校验/复位间隔/步数；七类协议不能只计 pulse 总量 |
| SPI | Master 2、Slave 2、Slave-HD 4 | 主从真实事务、模式/CS、EEPROM 命令/读回、LCD、append/segment 握手 | 错命令/地址/数据/长度、跳过事务；不能把总线传输完成当器件响应正确 |
| TEMP | 2 | 温度单位/范围、阈值监测与回调 | 错换算、错阈值、冻结测量；模型输入来源可追踪 |
| TIMER | 3 | 周期报警、HC-SR04 捕获、Wiegand 位序/间隔 | 错周期、差分计时/溢出、丢位；频率与虚拟精度界限 |
| TOUCH | 2 | 基线、阈值/去抖、触摸事件及触摸唤醒 | 常量触摸、错阈值、未唤醒；需补 capability 与模型契约 |
| UART | 8 | Echo/RS485、异步双向、事件/Select、NMEA 解析、REPL、DMA OTA | 载荷/校验/帧边界/方向错误、禁 TX、回调缺失；不是所有例子处理 overflow |
| ULP-LP | 27 | 协处理器真实程序/构建、HP 睡眠期间独立执行、外设/共享内存/邮箱/唤醒 | 禁止 LP 执行、改 LP 比较阈值、取消 wake；LP Core、FSM、RISC-V 及 combined 明确模型差异 |
| PM | Deep Sleep / Wake Stub / Light Sleep 3 | 指定唤醒原因、保留域、普通 BSS、wake stub 与时钟域 | 丢唤醒、错误原因、错误保留；进入休眠日志不是完成证明 |
| OTA | system/ota 5（2 deferred）及相关 UART/预构建组合 | 镜像数据、分区写入/选择、配置启用的校验与状态、工具行为 | 截断/错镜像、错分区、写后读回/状态错误；回滚/签名以配置为准 |
| CORE-CONC | system 事件/FreeRTOS/IPC/POSIX/ISR 组合 | 队列/事件因果、互斥/临界区、等待唤醒、线程 join、IPC 对端 | 跳过 give/post/callback、重复唤醒、错误 owner；单核合作式不证明真实 SMP |
| HEAP-MAP | heap_task_tracking/himem/xip 4 | 按 owner/能力记账、映射边界、内存数据一致性 | 错 owner/容量、漏释放、错映射；正常常驻分配不是泄漏 |
| SYS-TIME | esp_timer/startup_time/task_watchdog/flash_suspend 4 | 定时器、测量点、看门狗超时/喂狗及 flash suspend 允许并发行为 | 错时钟/跳过喂狗/暂停无效；实际墙钟与虚拟时钟不混用 |
| SYS-OBS | 其余 system console/trace/gcov/gdb/perf/unit_test/base_mac 等 | 输入命令与结果、追踪内容/顺序、测试失败检测、MAC 来源等各自业务 | 改命令处理、删 trace/test failure；不能仅验证程序启动或固定日志 |
| NVS | NVS 7（nvsgen 另属工具） | CRUD、迭代、blob 长度、统计、C++ 封装、持久与损坏拒绝 | 跳 commit、错 key/type/length、数据损坏；bootloader 示例范围明确 |
| FS | FATFS 4（不含 fatfsgen）、LittleFS/SPIFFS/Semihost 3 | 文件内容/seek/目录/挂载/卸载/格式化、路径沙箱与介质策略 | 错偏移/长度、漏 format/flush、挂载坏镜像；宿主访问按 Semihost 契约限定 |
| MEDIA-STORAGE | 原始 Flash/Partition/SD/eMMC/WL/benchmark 9 | 分区范围/映射、介质命令、磨损与数据一致性、性能模型含义 | 越界/坏块/错映射、写擦语义；吞吐不能由虚拟微秒伪装成真机实测 |
| STORAGE-TOOLS | nvsgen/fatfsgen/spiffsgen/parttool 4 | 工具输入、格式/分区数据、生成镜像能被独立消费 | 错 schema/offset/partition、坏镜像；与运行期故障协议分别设计 |
| HTTP-CLIENT | HTTP/HTTPS/DoH/mTLS/X509 等 7 | 方法/URL/header/body、分段响应处理、证书/主机名及配置启用认证 | 错请求/响应解析/证书校验；TLS 安全语义不能由 mock 200 代替 |
| HTTP-SERVER | HTTP Server 8、HTTPS Server 2 | 路由/方法/状态/响应体、文件/REST、异步、持久连接、WS/WSS 帧 | 路由错/handler no-op、帧内容/生命周期错；Fixture 发请求而非直接写响应 |
| MQTT | MQTT/MQTT5 2 | topic/payload/QoS、订阅接收、session 与 MQTT5 properties | 错 topic/QoS/消息处理、漏重订阅；只有 CONNECTED 不够 |
| SOCKET-ICMP | Sockets 9、ICMP/PMTU 2 | TCP/UDP、单播/多播、非阻塞、IPv6/多 netif、ping 序号/MTU | 错目的地/端口/族/序号、分片与 partial I/O；错误域为 BSD/协议各自契约 |
| PROTO-OTHER | local_ctrl/l2tap/smtp/sntp/static_ip 5 | 控制属性、L2 帧、SMTP 流程、时间同步、地址/路由实际生效 | 错属性/帧/时钟偏差/路由；SNTP 取宿主时间不能自动算协议完成 |
| NETWORK | Bridge/eth2ap/sta2eth/sniffer/VLAN 5 | 入口→出口转发、地址学习/环路策略、VLAN 隔离、捕获过滤与数据 | 错出口/VLAN/帧内容、禁止转发；STA GOT_IP 不能替代桥接成功 |
| WIFI | 24 | 基础模式/扫描/APSTA、roaming、EAP/DPP/WPS/SmartConfig、NAN/USD、FTM、iTWT、iperf、ESP-NOW 分别建 Claim | 错认证/候选/路由/服务消息/peer/调度；射频距离、功耗和性能保真必须限界 |
| BLE | 49 | 广播、Central/Peripheral、GATT 服务/客户端、安全、HID/UART、L2CAP、multi/coex/throughput、周期/PAwR/CTE/Channel Sounding/mesh 各自业务 | 禁关键 procedure、错 UUID/CCCD/权限/peer/载荷、隔离多连接；高级能力不能被 GAP 成功替代 |
| CXX | 3 | exception throw/catch/析构、pthread 同步、RTTI/typeid/dynamic_cast 与配置 | 漏 catch/析构、错误 cast 关系、线程不同步；保留原厂 C++ 示例，不能因 Wink 静态分发规则重写其业务 |
| BUILD | 19（全部 deferred） | CMake/v2、组件管理、条件组件、库/预构建导入、plugin/multi-config/multi-binary/wrapper | 缺组件的预期构建拒绝、依赖无效化、配置生效、产物正确性；这里的 build failure 可是被测工具正确行为，而非无效业务变异 |

**尚缺的重点能力建模候选：**Comparator/ETM、I3C、Bitscrambler、LCD/像素处理、SPI Slave/HD、Sigma-Delta、POSIX/eventfd/mqueue、LP Core、Ethernet bridge/VLAN/L2、TLS/PKI、MQTT5、Wi-Fi/BLE 高级协议及 C++/工具业务。是否新增原子 capability 或采用既有 capability 下的 feature/subcontract，应经架构决策；无论形式如何，不能继续默认为 core/STA/GAP 已覆盖。

63 项字典中零引用的 USB、Camera、ULP、DMA、Build 等项，需要区别：部分是未来范围，部分是现有示例漏映射。不能把“当前零引用”一律解释成无需实现，也不能把未来能力自动增加进当前交付分母。

## 5. 建议补入契约的机器模型

### 5.1 最小证明实体与绑定

| 实体 | 必需语义 |
|---|---|
| `AppContract` | 上游源/README 依据、核心业务声明、辅助声明、支持目标与经批准降级；完整性复核绑定 |
| `ExecutionIdentity` | app/config/run/attempt、实际 backend/SoC/profile、SDK/runtime/toolchain/model、effective config 与资产身份 |
| `Claim` | 稳定 ID、能力/feature、输入/输出/不变量/生命周期、精度/时钟/允许错误、oracle 依据 |
| `ObservationDescriptor` | 信号来源与方向、实例/epoch、单位/类型、validity/freshness、只读采样语义与 probe/schema 版本 |
| `EvidenceObligation` | 类别、适用规则、预期案例/断言/时限集合；N/A 的规范依据和审计 |
| `MutationDescriptor` | 类型/版本、前置条件、目标编译单元、缺陷模型、Patch、实际构建/装载/激活、目标失败谓词 |
| `FaultDescriptor` | 抽象层、目标/参数、seed/schedule、激活见证、正确处理策略、撤销及恢复边界 |
| `AFGResult` | 完整/缺失集合、逐项事实/结果、原始证据引用、分类原因、限制与 package digest；不是 SSOT 交付状态 |

这些是**建议的新契约实体**，不宣称当前工具已经接受这些名称或字段。应在技术设计中冻结真实 Schema、CLI/C-ABI 与版本，再据此拆实施任务。

### 5.2 接受逻辑

```text
AFG_ELIGIBLE =
    identity_is_bound
    AND business_claims_are_complete_and_frozen
    AND required_execution_sets_are_exact_and_complete
    AND observations_and_oracles_are_valid
    AND baseline_expectations_satisfied
    AND matcher_self_checks_satisfied
    AND effective_firmware_dependency_checks_satisfied
    AND required_non_equivalent_implementation_mutants_killed
    AND all_applicable_environment_and_fault_expectations_satisfied
    AND all_required_recovery_and_isolation_expectations_satisfied
    AND raw_evidence_can_be_independently_recomputed

PROMOTABLE =
    AFG_ELIGIBLE
    AND scope_audit_capability_dependencies_satisfied
    AND required_regressions_and_gates_satisfied
    AND candidate_package_integrity_satisfied
    AND independent_audit_accepts_this_package
```

未适用检查有规范化裁定；缺失、跳过、模型不支持、超预算、归因不确定不能被折算为满足。Engine 不负责 CAS 细节，发布方仍须确认这些输入与 Harness 契约一致。

### 5.3 七类检查的极性

| 检查 | 场景断言的预期 | 能证明什么 |
|---|---|---|
| baseline | 原业务 PASS | 此配置下声明的正常行为成立 |
| matcher_self_check | 指定错误被 matcher/validator 检出 | 断言机制能拒绝预置错误 |
| env_sensitivity | 预先规定的输入变化→输出/不变量关系 PASS | 相关环境输入实际影响业务；允许规范规定的稳定不变量 |
| firmware_dependency | 选择性禁用真实业务后，同一核心业务断言 FAIL | 业务成功依赖固件执行 |
| implementation_mutation | 有效非等价缺陷导致预定核心断言 FAIL | 测试对该缺陷敏感 |
| fault_handling | 故障下的预期错误/降级/退出/复位等断言 PASS | 系统符合其故障策略；不证明实现变异一定会被捕获 |
| recovery | 规定的复原过程及基线断言 PASS | 注入可撤销，状态按生命周期恢复或保留 |

### 5.4 防止“只测旁边”

对每个核心 Claim，必须能回答以下问题，并由证据具体绑定：

1. 原厂示例核心功能是什么，哪些条件编译分支实际启用？
2. 哪个真实固件操作产生了哪个可观察出口？
3. oracle 来自哪里，为什么不会与被测实现同源退化？
4. 禁掉该操作，哪个同一业务断言失败？
5. 破坏哪种有效非等价实现，哪个目标谓词在什么窗口失败？
6. 有哪些故障属于示例的合同，为什么预期是重试、错误、退出、复位或其他行为？
7. 撤销后哪些数据应保留，哪些句柄/任务/回调应失效，怎样排除历史资产？

## 6. 引擎自身应有的最小验收反例

本表是后续实施的验收要求，**本次没有执行这些测试，也未写实现测试代码**。

| ID | 输入 / 反例 | 必须观察到的引擎结论 |
|---|---|---|
| META-01 | 无业务断言、全辅助电源、缺 Claim、空场景 | INCOMPLETE/REJECTED，不能 eligible |
| META-02 | 删除 Central GATT 操作 Claim，剩余 GAP 通过 | 覆盖缺口，不能证明整个 Central 示例 |
| META-03 | Fixture 直接填写 Sink、旧报告/旧 probe、空值被转 0 | 观测/身份错误 |
| META-04 | 合法 UART/WS Echo；禁止固件转发的对应变异 | 正例接受，变异命中同一出口断言 |
| META-05 | fault_handling 正确返回 DISCONNECTED/错误码 | 正确故障期望 PASS，不归入 survived |
| META-06 | wrong matcher、物理断线、源码变异三种证据互换 | 拒绝替代必需 evidence class |
| META-07 | Patch 未应用、未编译、未加载、未激活 | 分别输出无效/不确定原因，不计击杀 |
| META-08 | 编译报错/无关崩溃/执行器墙钟超时 | 不计目标业务击杀 |
| META-09 | 正常观察器中目标业务超出虚拟 deadline | 可判目标期限违约，保留充分归因 |
| META-10 | 等价/无关变异、有效存活变异、预算耗尽 | 不替代有效击杀；存活拒收、未覆盖保持 incomplete |
| META-11 | 改头文件/runtime/config/toolchain 后缓存命中旧产物 | 身份校验拒绝或缓存失效 |
| META-12 | Getter 不推进时钟；无消费者时异步生产；有流控不溢出 | 分别符合只读、生产与容量/流控模型 |
| META-13 | 无效/旧代 handle 返回默认全 0，或者读取导致生产 | probe 不合法；不能通过白盒证明 |
| META-14 | 实例不重建、重建后旧 handle、reset 后残余回调 | 不重建 token 稳定合法；旧句柄/回调拒绝 |
| META-15 | NVS 提交后冷启动、未提交切点、RTC 保留与 BSS 清零 | 依 reset/persistence 契约分别判定 |
| META-16 | 合法内联/LTO 无内部 export；同名空桩有 export | 前者可用 link/runtime witness；后者行为验证失败 |
| META-17 | ESP 正错误码、BSD errno、NimBLE 错误、错误单位/类型 | 按 error domain/单位/类型严格判定 |
| META-18 | P4-only 示例冒充 ESP32，Browser 凭据贴 Node | 身份/支持目标缺口 |
| META-19 | 构建示例预期缺组件失败；runtime mutant 编译失败 | 前者是 build subject 的正常负例；后者不是业务击杀 |
| META-20 | 少场景/少步骤/重复结果/必要步骤 skipped/截断报告 | 完整性失败 |
| META-21 | 引擎 verdict PASS，但 raw actual 与 oracle 不符 | Inspector 独立复算拒绝 |
| META-22 | 回执成功但 audit 缺失、包变更、scope/依赖不齐 | 不允许晋升 |
| META-23 | 正常源树或 oracle 被 Canary 改写 | 隔离/完整性失败，保留诊断 |
| META-24 | 故障撤销失败或恢复失败 | 保留原失败归因与恢复失败，结果不能 eligible |

## 7. 建议修订顺序与冻结条件

1. **先修正证明定义与边界：**R01–R04、R20、R22；把“绝对无假绿/签发 VERIFIED”改为有边界、可复核的候选验证结果，统一 Harness 与治理 SOP 的语义。
2. **补全业务、配置与能力映射：**逐一处置 312 个示例、313 个配置的业务 Claims；优先核对 ULP/LP、BLE Client/Security、构建、I3C/DMA/LCD、Deep Sleep 与 81 个目标冲突信号。保留延期和产品范围正交。
3. **冻结执行及证据协议：**R05–R06、R11–R17、R19、R21；提供真实 Schema、C-ABI/CLI、变异/故障描述、探针生命周期、reset 模型、缓存与结果分类，确保独立 Inspector 可复算。
4. **校准领域规则：**R07–R10、R18；逐项确认原厂接口、保真度和已接受降级。为每个原子能力定义适用子契约，再为 App 绑定具体声明，避免生成无限泛化的硬编码规则。
5. **以最小代表集验证契约可执行性，再扩展全量：**Hello World、Blink/LEDC、ADC Continuous、UART Echo、SPI EEPROM、NVS、HTTP Client/Server、Wi-Fi STA、BLE Central/Security、LP SPI、DMA CRC、C++ RTTI、构建示例。尚无对应运行时能力的标杆先验证缺口判定与 Schema，不伪造通过。

冻结时必须具备：每个字段/状态的规范、七类检查适用规则、必需执行集合、可追溯 raw evidence、独立复算协议、引擎正反例套件、历史迁移策略，以及对未支持模型/观察接口的明确诊断。应在 Accepted 前同步处理关联 Harness、Batch 0、分类规范及实施计划，避免两个有效契约各自规定相反结论。

## 8. 快照标识与证据入口

| 资产 | SHA-256 |
|---|---|
| checklist.data.json | `3cab0a0e97b0197409edcb62f50e8bf03352d7a5ac6a2a8bc9ee9a68905d0a6a` |
| capability-catalog.yaml | `4f9e8754444fe788d294b6854e7f353313ef7ac31408e8370a9d8dc47d554bcc` |
| 被评审 AFG 契约 | `61d9f21a5792984657d1dbcc44b8430bdd0698f03fd9e31624434916943cc9db` |

关键源码 / 规范证据：

- [ESP 错误码门面](../../../wink-micro-os/frameworks/esp_idf/include/esp_err.h)：L50–51、L77–78，ESP 正错误码。
- [API 覆盖与降级登记](../../../wink-micro-os/frameworks/esp_idf/docs/02-api-coverage-matrix.md)：任务/多核、LEDC、UART、SPI、网络等边界；它是人工登记，仍需与实际实现互证。
- [LEDC 门面](../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_ledc.c)：`ledc_fade_start` 与 Getter 路径说明 API 立即返回、缓存读值、实际输出之间必须区分；本次未声称它已满足新渐变模型。
- [治理 SOP](../../../.agents/skills/governance-sop-esp/SKILL.md) 及 [领域断言指南](../../../.agents/skills/governance-sop-esp/references/domain-assertion-guide.md)：故障处理、变异与自检的区别，以及逻辑通道/物理拓扑、稳态/交互验收边界。
- 本机官方 `components/esp_adc/include/esp_adc/adc_continuous.h`：L53–57、L98–100、L179–182；容量、flush 与 overflow 语义。
- 本机官方 `components/esp_driver_i2c/include/driver/i2c_master.h`：L169–172、L247–250；NACK、探测失败与超时分别定义。
- 本机官方 `components/esp_driver_spi/include/driver/spi_master.h`：L119、L219–225、L244–267；DMA flags、队列/完成时限与同步传输接口，不以“外部器件断线必返回 timeout”定义。
- 本机官方 `examples/bluetooth/nimble/blecent/main/main.c`：L116、L167、L207、L384，真实 GATT Client 操作；相邻 README 给出客户端业务步骤。
- 本机官方 `examples/system/ulp/lp_core/lp_spi/main/lp_spi_main.c`：L38、L44、L105；以及 `main/lp_core/main.c`：L171–184，真实 LP 程序读取环境、判断阈值并唤醒 HP。
- 本机官方 I3C、Async CRC README 首行仅列 ESP32-P4；C++ RTTI README 说明 typeid/dynamic_cast 的核心业务；UART Echo 源码明确关闭流控和事件队列，正常读写不包含通用自动重试策略。

## 附录 A：312 个迁移示例逐项覆盖索引

下面每行绑定一个真实 `in_scope` 条目；不是通过/失败验收清单。`契约族` 指第 4 节需要补入的业务证明范围；`目标表` 仅表示上游 README 是否列出当前登记 ESP32，`未列ESP32` 是待裁定信号，`未提供` 不能按支持或不支持推断。`入口` 为本次静态扫描提取的少量官方头文件提示，不代表完整依赖闭包或功能正确。

所有行都需要落实逐 Claim 的机器绑定。现有 `verified` 仅表示历史登记状态；未落地/未仿真项不能据此生成新凭据。示例路径均相对于本机 SDK `examples/`，完整身份可在机器 SSOT 按该路径唯一定位。Wi-Fi STA 行显式列两个配置，其他每行一个。

配置栏列出实际登记的 `config_id / backend / delivery_state`；所有 profile 均登记 standard。

### A.bluetooth（49 个示例）
| 清单序号 | 上游示例路径 | 契约族 | 登记配置 / 后端 / 状态 | 目标表 | 官方源码入口提示 |
|---|---|---|---|---|---|
| 247 | `bluetooth/ble_get_started/nimble/NimBLE_Beacon` | BLE | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `common.h`<br/>`gap.h` |
| 248 | `bluetooth/ble_get_started/nimble/NimBLE_Connection` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `common.h`<br/>`gap.h` |
| 249 | `bluetooth/ble_get_started/nimble/NimBLE_GATT_Server` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `common.h`<br/>`gap.h` |
| 250 | `bluetooth/ble_get_started/nimble/NimBLE_Security` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `common.h`<br/>`gap.h` |
| 251 | `bluetooth/ble_uart_service` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `nvs_flash.h` |
| 312 | `bluetooth/blufi` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_task_wdt.h`<br/>`host/ble_hs.h` |
| 343 | `bluetooth/esp_hid_device` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`host/ble_gap.h`<br/>`host/ble_hs.h` |
| 344 | `bluetooth/esp_hid_host` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`host/ble_gap.h`<br/>`host/ble_hs.h` |
| 349 | `bluetooth/nimble/ble_ancs` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_timer.h`<br/>`host/ble_hs.h`<br/>`nvs_flash.h` |
| 350 | `bluetooth/nimble/ble_chan_sound_initiator` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_cs.h`<br/>`host/ble_hs.h`<br/>`nvs_flash.h` |
| 351 | `bluetooth/nimble/ble_chan_sound_reflector` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_cs.h`<br/>`host/ble_hs.h`<br/>`nvs_flash.h` |
| 352 | `bluetooth/nimble/ble_cte/ble_periodic_adv_with_cte` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_gap.h`<br/>`host/ble_hs.h`<br/>`host/ble_hs_adv.h` |
| 353 | `bluetooth/nimble/ble_cte/ble_periodic_sync_with_cte` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_hs.h`<br/>`nvs_flash.h` |
| 354 | `bluetooth/nimble/ble_cts/cts_cent` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_esp_gattc_cache.h`<br/>`host/ble_hs.h`<br/>`nvs_flash.h` |
| 355 | `bluetooth/nimble/ble_cts/cts_prph` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_hs.h`<br/>`host/ble_uuid.h`<br/>`nvs_flash.h` |
| 356 | `bluetooth/nimble/ble_dynamic_service` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_hs.h`<br/>`host/ble_uuid.h`<br/>`nvs_flash.h` |
| 357 | `bluetooth/nimble/ble_enc_adv_data/enc_adv_data_cent` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_ead.h`<br/>`host/ble_hs.h`<br/>`nvs_flash.h` |
| 358 | `bluetooth/nimble/ble_enc_adv_data/enc_adv_data_prph` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_hs.h`<br/>`host/ble_uuid.h`<br/>`nvs_flash.h` |
| 359 | `bluetooth/nimble/ble_gattc_gatts_coex` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_hs.h`<br/>`nvs_flash.h` |
| 360 | `bluetooth/nimble/ble_htp/htp_cent` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_esp_gattc_cache.h`<br/>`host/ble_hs.h`<br/>`nvs_flash.h` |
| 361 | `bluetooth/nimble/ble_htp/htp_prph` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_hs.h`<br/>`host/ble_uuid.h`<br/>`nvs_flash.h` |
| 362 | `bluetooth/nimble/ble_l2cap_coc/coc_blecent` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_hs.h`<br/>`nvs_flash.h` |
| 363 | `bluetooth/nimble/ble_l2cap_coc/coc_bleprph` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_hs.h`<br/>`nvs_flash.h` |
| 364 | `bluetooth/nimble/ble_multi_adv` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_hs.h`<br/>`nvs_flash.h` |
| 365 | `bluetooth/nimble/ble_multi_conn/ble_multi_conn_cent` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_hs.h`<br/>`host/ble_uuid.h`<br/>`nvs_flash.h` |
| 366 | `bluetooth/nimble/ble_multi_conn/ble_multi_conn_prph` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_hs.h`<br/>`host/ble_uuid.h`<br/>`nvs_flash.h` |
| 367 | `bluetooth/nimble/ble_pawr_adv/ble_pawr_adv` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_hs.h`<br/>`nvs_flash.h` |
| 368 | `bluetooth/nimble/ble_pawr_adv/ble_pawr_sync` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_hs.h`<br/>`nvs_flash.h` |
| 369 | `bluetooth/nimble/ble_pawr_adv_conn/ble_pawr_adv_conn` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_hs.h`<br/>`nvs_flash.h` |
| 370 | `bluetooth/nimble/ble_pawr_adv_conn/ble_pawr_sync_conn` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_hs.h`<br/>`nvs_flash.h` |
| 371 | `bluetooth/nimble/ble_periodic_adv` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_gap.h`<br/>`host/ble_hs.h`<br/>`host/ble_hs_adv.h` |
| 372 | `bluetooth/nimble/ble_periodic_sync` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_gap.h`<br/>`host/ble_hs.h`<br/>`nvs_flash.h` |
| 373 | `bluetooth/nimble/ble_phy/phy_cent` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_hs.h`<br/>`nvs_flash.h` |
| 374 | `bluetooth/nimble/ble_phy/phy_prph` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `host/ble_hs.h`<br/>`host/ble_uuid.h`<br/>`nvs_flash.h` |
| 375 | `bluetooth/nimble/ble_proximity_sensor/proximity_sensor_cent` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_esp_gattc_cache.h`<br/>`host/ble_hs.h`<br/>`nvs_flash.h` |
| 376 | `bluetooth/nimble/ble_proximity_sensor/proximity_sensor_prph` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_hs.h`<br/>`nvs_flash.h` |
| 377 | `bluetooth/nimble/ble_spi_slave` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/spi_slave.h` |
| 378 | `bluetooth/nimble/ble_spp/spp_client` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/uart.h`<br/>`host/ble_hs.h`<br/>`nvs_flash.h` |
| 379 | `bluetooth/nimble/ble_spp/spp_server` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/uart.h`<br/>`host/ble_hs.h`<br/>`nvs_flash.h` |
| 380 | `bluetooth/nimble/blecent` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_esp_gattc_cache.h`<br/>`host/ble_hs.h`<br/>`nvs_flash.h` |
| 381 | `bluetooth/nimble/blecsc` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_hs.h`<br/>`host/ble_uuid.h`<br/>`nvs_flash.h` |
| 382 | `bluetooth/nimble/blehr` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_hs.h`<br/>`host/ble_uuid.h`<br/>`nvs_flash.h` |
| 383 | `bluetooth/nimble/blemesh` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_hs.h`<br/>`nvs_flash.h` |
| 384 | `bluetooth/nimble/bleprph` | BLE | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `host/ble_hs.h`<br/>`host/ble_uuid.h`<br/>`nvs_flash.h` |
| 385 | `bluetooth/nimble/bleprph_host_only` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/uart.h`<br/>`host/ble_hs.h`<br/>`host/ble_hs_mbuf.h` |
| 386 | `bluetooth/nimble/bleprph_wifi_coex` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`host/ble_hs.h`<br/>`host/ble_uuid.h` |
| 388 | `bluetooth/nimble/power_save` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `host/ble_hs.h`<br/>`host/ble_uuid.h`<br/>`nvs_flash.h` |
| 389 | `bluetooth/nimble/throughput_app/blecent_throughput` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/rtc_io.h`<br/>`driver/uart.h` |
| 390 | `bluetooth/nimble/throughput_app/bleprph_throughput` | BLE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_timer.h`<br/>`host/ble_hs.h`<br/>`host/ble_uuid.h` |

### A.build_system（19 个示例）
| 清单序号 | 上游示例路径 | 契约族 | 登记配置 / 后端 / 状态 | 目标表 | 官方源码入口提示 |
|---|---|---|---|---|---|
| 426 | `build_system/cmake/component_manager` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `cmp.h` |
| 427 | `build_system/cmake/idf_as_lib` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `esp_flash.h` |
| 428 | `build_system/cmake/import_lib` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `esp_vfs_fat.h` |
| 429 | `build_system/cmake/import_prebuilt` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `esp_ota_ops.h`<br/>`esp_partition.h` |
| 430 | `build_system/cmake/import_prebuilt/prebuilt` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 未提供 | `esp_ota_ops.h`<br/>`esp_partition.h` |
| 431 | `build_system/cmake/multi_config` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `func.h` |
| 432 | `build_system/cmake/plugins` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `plugins_api.h`<br/>`sys/queue.h` |
| 433 | `build_system/cmakev2/features/component_manager` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `cmp.h` |
| 434 | `build_system/cmakev2/features/conditional_component` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `esp_log.h`<br/>`logging_util.h` |
| 435 | `build_system/cmakev2/features/idf_as_lib` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `esp_flash.h` |
| 436 | `build_system/cmakev2/features/import_lib` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `esp_vfs_fat.h` |
| 437 | `build_system/cmakev2/features/import_lib_direct` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `esp_log.h`<br/>`lwjson/lwjson.h` |
| 438 | `build_system/cmakev2/features/import_prebuilt` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `esp_ota_ops.h`<br/>`esp_partition.h` |
| 439 | `build_system/cmakev2/features/import_prebuilt/prebuilt` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 未提供 | `esp_ota_ops.h`<br/>`esp_partition.h` |
| 440 | `build_system/cmakev2/features/multi_binary` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `component1.h`<br/>`component2.h` |
| 441 | `build_system/cmakev2/features/multi_config` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `func.h` |
| 442 | `build_system/cmakev2/features/plugins` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `plugins_api.h`<br/>`sys/queue.h` |
| 443 | `build_system/cmakev2/get-started/hello_world` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `esp_flash.h` |
| 444 | `build_system/wrappers` [deferred] | BUILD | `host_native_standard` / `host_native` / `planned` | 列ESP32 | `esp_log.h`<br/>`freertos/FreeRTOS.h` |

### A.cxx（3 个示例）
| 清单序号 | 上游示例路径 | 契约族 | 登记配置 / 后端 / 状态 | 目标表 | 官方源码入口提示 |
|---|---|---|---|---|---|
| 423 | `cxx/exceptions` | CXX | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `iostream` |
| 424 | `cxx/pthread` | CXX | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `chrono`<br/>`esp_log.h` |
| 425 | `cxx/rtti` | CXX | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `algorithm`<br/>`cxxabi.h` |

### A.get-started（2 个示例）
| 清单序号 | 上游示例路径 | 契约族 | 登记配置 / 后端 / 状态 | 目标表 | 官方源码入口提示 |
|---|---|---|---|---|---|
| 1 | `get-started/blink` | START | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/gpio.h` |
| 2 | `get-started/hello_world` | START | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_flash.h` |

### A.network（5 个示例）
| 清单序号 | 上游示例路径 | 契约族 | 登记配置 / 后端 / 状态 | 目标表 | 官方源码入口提示 |
|---|---|---|---|---|---|
| 418 | `network/bridge` | NETWORK | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_console.h`<br/>`esp_eth.h`<br/>`esp_eth_spec.h` |
| 419 | `network/eth2ap` | NETWORK | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_eth_driver.h`<br/>`esp_event.h`<br/>`nvs_flash.h` |
| 420 | `network/simple_sniffer` | NETWORK | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/sdmmc_host.h`<br/>`driver/sdspi_host.h`<br/>`driver/spi_common.h` |
| 421 | `network/sta2eth` | NETWORK | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`esp_eth_netif_glue.h`<br/>`esp_event.h` |
| 422 | `network/vlan_support` | NETWORK | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_eth.h`<br/>`esp_eth_netif_glue.h`<br/>`esp_event.h` |

### A.peripherals（81 个示例）
| 清单序号 | 上游示例路径 | 契约族 | 登记配置 / 后端 / 状态 | 目标表 | 官方源码入口提示 |
|---|---|---|---|---|---|
| 3 | `peripherals/adc/continuous_read` | ADC-CMP | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_adc/adc_continuous.h`<br/>`esp_log.h` |
| 4 | `peripherals/adc/oneshot_read` | ADC-CMP | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_adc/adc_cali.h`<br/>`esp_adc/adc_cali_scheme.h` |
| 5 | `peripherals/analog_comparator/auto_scan` | ADC-CMP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/ana_cmpr_etm.h`<br/>`driver/gpio.h`<br/>`driver/gpio_etm.h` |
| 6 | `peripherals/analog_comparator/etm_periodic_scan` | ADC-CMP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/ana_cmpr.h`<br/>`driver/ana_cmpr_etm.h`<br/>`driver/gpio.h` |
| 7 | `peripherals/bitscrambler` | DMA-BITS | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/bitscrambler_loopback.h`<br/>`esp_heap_caps.h` |
| 11 | `peripherals/dac/dac_continuous/dac_audio` | DAC-SDM | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/dac_continuous.h` |
| 12 | `peripherals/dac/dac_continuous/signal_generator` | DAC-SDM | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/dac_continuous.h`<br/>`driver/dac_oneshot.h`<br/>`driver/gpio.h` |
| 13 | `peripherals/dac/dac_cosine_wave` | DAC-SDM | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/dac_cosine.h` |
| 14 | `peripherals/dac/dac_oneshot` | DAC-SDM | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/dac_oneshot.h` |
| 15 | `peripherals/dedicated_gpio/soft_i2c` | GPIO | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/dedic_gpio.h`<br/>`driver/gpio.h` |
| 16 | `peripherals/dedicated_gpio/soft_spi` | GPIO | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/dedic_gpio.h`<br/>`driver/gpio.h` |
| 17 | `peripherals/dedicated_gpio/soft_uart` | GPIO | `wasm_sim_standard` / `wasm_browser` / `verified` | 未列ESP32 | `driver/dedic_gpio.h`<br/>`driver/gpio.h` |
| 18 | `peripherals/dma/async_color_convert` | DMA-BITS | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_async_color_convert.h`<br/>`esp_heap_caps.h`<br/>`mbedtls/base64.h` |
| 19 | `peripherals/dma/async_crc` | DMA-BITS | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_async_crc.h`<br/>`esp_console.h` |
| 20 | `peripherals/gpio/generic_gpio` | GPIO | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/gpio.h` |
| 21 | `peripherals/gpio/matrix_keyboard` | GPIO | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/dedic_gpio.h`<br/>`driver/gpio.h` |
| 23 | `peripherals/i2c/i2c_basic` | I2C-I3C | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/i2c_master.h` |
| 24 | `peripherals/i2c/i2c_eeprom` | I2C-I3C | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/i2c_master.h` |
| 25 | `peripherals/i2c/i2c_slave_network_sensor` | I2C-I3C | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/i2c_slave.h`<br/>`esp_event.h`<br/>`esp_http_client.h` |
| 26 | `peripherals/i2c/i2c_tools` | I2C-I3C | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/i2c_master.h`<br/>`esp_console.h` |
| 27 | `peripherals/i2c/i2c_u8g2` | I2C-I3C | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/i2c_master.h` |
| 29 | `peripherals/i2s/i2s_basic/i2s_pdm` | I2S | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/i2s_pdm.h` |
| 30 | `peripherals/i2s/i2s_basic/i2s_std` | I2S | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/i2s_std.h` |
| 31 | `peripherals/i2s/i2s_basic/i2s_tdm` | I2S | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`driver/i2s_tdm.h` |
| 32 | `peripherals/i2s/i2s_codec/i2s_es7210_tdm` | I2S | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/i2c_master.h`<br/>`driver/i2s_tdm.h`<br/>`esp_vfs_fat.h` |
| 33 | `peripherals/i2s/i2s_codec/i2s_es8311` | I2S | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/i2c_master.h`<br/>`driver/i2s_std.h` |
| 34 | `peripherals/i2s/i2s_recorder` | I2S | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/i2s_pdm.h`<br/>`driver/spi_common.h` |
| 35 | `peripherals/i3c/i3c_i2c_basic` | I2C-I3C | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/i3c_master.h`<br/>`driver/i3c_master_i2c.h` |
| 36 | `peripherals/i3c/i3c_lsm6dscx` | I2C-I3C | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/i3c_master.h`<br/>`esp_heap_caps.h` |
| 40 | `peripherals/lcd/i2c_oled` | LCD | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/i2c_master.h`<br/>`esp_lcd_panel_io.h`<br/>`esp_lcd_panel_ops.h` |
| 41 | `peripherals/lcd/i80_controller` | LCD | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`esp_lcd_nt35510.h`<br/>`esp_lcd_panel_io.h` |
| 43 | `peripherals/lcd/parlio_simulate` | LCD | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`esp_lcd_panel_io.h`<br/>`esp_lcd_panel_ops.h` |
| 44 | `peripherals/lcd/rgb_panel` | LCD | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`esp_lcd_panel_ops.h`<br/>`esp_lcd_panel_rgb.h` |
| 45 | `peripherals/lcd/spi_lcd_touch` | LCD | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/spi_master.h`<br/>`esp_lcd_panel_io.h` |
| 46 | `peripherals/lcd/tjpgd` | LCD | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/spi_master.h`<br/>`esp_heap_caps.h` |
| 47 | `peripherals/ledc/ledc_basic` | LEDC | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/ledc.h` |
| 48 | `peripherals/ledc/ledc_dimmer` | LEDC | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`driver/ledc.h` |
| 49 | `peripherals/ledc/ledc_fade` | LEDC | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/ledc.h` |
| 50 | `peripherals/ledc/ledc_gamma_curve_fade` | LEDC | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/ledc.h` |
| 51 | `peripherals/mcpwm/mcpwm_bdc_speed_control` | MCPWM | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/pulse_cnt.h`<br/>`esp_timer.h` |
| 52 | `peripherals/mcpwm/mcpwm_bldc_hall_control` | MCPWM | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/mcpwm_prelude.h`<br/>`esp_timer.h` |
| 53 | `peripherals/mcpwm/mcpwm_capture_hc_sr04` | MCPWM | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/mcpwm_cap.h` |
| 54 | `peripherals/mcpwm/mcpwm_foc_svpwm_open_loop` | MCPWM | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h` |
| 55 | `peripherals/mcpwm/mcpwm_servo_control` | MCPWM | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/mcpwm_prelude.h` |
| 56 | `peripherals/mcpwm/mcpwm_sync` | MCPWM | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/mcpwm_prelude.h` |
| 57 | `peripherals/parlio/parlio_rx/logic_analyzer` | PARLIO | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`driver/parlio_rx.h`<br/>`esp_event.h` |
| 58 | `peripherals/parlio/parlio_tx/advanced_rgb_led_matrix` | PARLIO | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/parlio_tx.h`<br/>`esp_timer.h` |
| 59 | `peripherals/parlio/parlio_tx/simple_rgb_led_matrix` | PARLIO | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/dedic_gpio.h`<br/>`driver/gptimer.h`<br/>`driver/parlio_tx.h` |
| 60 | `peripherals/pcnt/rotary_encoder` | PCNT | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/pulse_cnt.h`<br/>`esp_sleep.h` |
| 62 | `peripherals/rmt/dshot_esc` | RMT | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/rmt_tx.h` |
| 63 | `peripherals/rmt/ir_nec_transceiver` | RMT | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/rmt_rx.h`<br/>`driver/rmt_tx.h` |
| 64 | `peripherals/rmt/led_strip` | RMT | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/rmt_tx.h` |
| 65 | `peripherals/rmt/led_strip_simple_encoder` | RMT | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/rmt_tx.h` |
| 66 | `peripherals/rmt/musical_buzzer` | RMT | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/rmt_tx.h` |
| 67 | `peripherals/rmt/onewire` | RMT | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h` |
| 68 | `peripherals/rmt/stepper_motor` | RMT | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`driver/rmt_tx.h` |
| 71 | `peripherals/sigma_delta/sdm_dac` | DAC-SDM | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gptimer.h`<br/>`driver/sdm.h` |
| 72 | `peripherals/sigma_delta/sdm_led` | DAC-SDM | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/sdm.h` |
| 73 | `peripherals/spi_master/hd_eeprom` | SPI | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/gpio.h`<br/>`driver/spi_master.h` |
| 74 | `peripherals/spi_master/lcd` | SPI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/spi_master.h` |
| 75 | `peripherals/spi_slave/receiver` | SPI | `wasm_sim_standard` / `wasm_browser` / `planned` | 未提供 | `driver/gpio.h`<br/>`driver/spi_slave.h` |
| 76 | `peripherals/spi_slave/sender` | SPI | `wasm_sim_standard` / `wasm_browser` / `planned` | 未提供 | `driver/gpio.h`<br/>`driver/spi_master.h`<br/>`esp_timer.h` |
| 77 | `peripherals/spi_slave_hd/append_mode/master` | SPI | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/spi_common.h`<br/>`driver/spi_master.h` |
| 78 | `peripherals/spi_slave_hd/append_mode/slave` | SPI | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/spi_slave_hd.h` |
| 79 | `peripherals/spi_slave_hd/segment_mode/seg_master` | SPI | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/spi_master.h` |
| 80 | `peripherals/spi_slave_hd/segment_mode/seg_slave` | SPI | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/spi_slave_hd.h` |
| 81 | `peripherals/temperature_sensor/temp_sensor` | TEMP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/temperature_sensor.h` |
| 82 | `peripherals/temperature_sensor/temp_sensor_monitor` | TEMP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/temperature_sensor.h` |
| 83 | `peripherals/timer_group/gptimer` | TIMER | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/gptimer.h` |
| 84 | `peripherals/timer_group/gptimer_capture_hc_sr04` | TIMER | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`driver/gpio_etm.h`<br/>`driver/gptimer.h` |
| 85 | `peripherals/timer_group/wiegand_interface` | TIMER | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_heap_caps.h` |
| 86 | `peripherals/touch_sensor/touch_sens_basic` | TOUCH | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/touch_sens.h` |
| 87 | `peripherals/touch_sensor/touch_sens_sleep` | TOUCH | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/touch_sens.h`<br/>`driver/uart.h`<br/>`esp_sleep.h` |
| 93 | `peripherals/uart/nmea0183_parser` | UART | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `ctype.h`<br/>`esp_log.h` |
| 94 | `peripherals/uart/uart_async_rxtxtasks` | UART | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/gpio.h`<br/>`driver/uart.h` |
| 95 | `peripherals/uart/uart_dma_ota` | UART | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/uart.h`<br/>`driver/uhci.h`<br/>`esp_heap_caps.h` |
| 96 | `peripherals/uart/uart_echo` | UART | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/gpio.h`<br/>`driver/uart.h` |
| 97 | `peripherals/uart/uart_echo_rs485` | UART | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/uart.h`<br/>`nvs_flash.h` |
| 98 | `peripherals/uart/uart_events` | UART | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/uart.h` |
| 99 | `peripherals/uart/uart_repl` | UART | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/uart.h`<br/>`esp_console.h` |
| 100 | `peripherals/uart/uart_select` | UART | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/uart.h`<br/>`driver/uart_vfs.h` |

### A.protocols（35 个示例）
| 清单序号 | 上游示例路径 | 契约族 | 登记配置 / 后端 / 状态 | 目标表 | 官方源码入口提示 |
|---|---|---|---|---|---|
| 185 | `protocols/dns_over_https` | HTTP-CLIENT | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_http_client.h`<br/>`esp_netif_sntp.h` |
| 186 | `protocols/esp_http_client` | HTTP-CLIENT | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_event.h`<br/>`esp_http_client.h`<br/>`esp_netif.h` |
| 187 | `protocols/esp_http_client_mutual_auth` | HTTP-CLIENT | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_http_client.h`<br/>`esp_netif.h` |
| 188 | `protocols/esp_local_ctrl` | PROTO-OTHER | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_timer.h`<br/>`lwip/err.h` |
| 189 | `protocols/http_request` | HTTP-CLIENT | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`lwip/dns.h`<br/>`lwip/err.h` |
| 190 | `protocols/http_server/advanced_tests` | HTTP-SERVER | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_eth.h`<br/>`esp_event.h`<br/>`esp_http_server.h` |
| 191 | `protocols/http_server/async_handlers` | HTTP-SERVER | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_eth.h`<br/>`esp_event.h`<br/>`esp_http_server.h` |
| 192 | `protocols/http_server/captive_portal` | HTTP-SERVER | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_http_server.h`<br/>`esp_netif.h` |
| 193 | `protocols/http_server/file_serving` | HTTP-SERVER | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/sdmmc_host.h`<br/>`driver/sdspi_host.h`<br/>`driver/spi_common.h` |
| 194 | `protocols/http_server/persistent_sockets` | HTTP-SERVER | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_eth.h`<br/>`esp_event.h`<br/>`esp_http_server.h` |
| 195 | `protocols/http_server/restful_server` | HTTP-SERVER | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_event.h`<br/>`esp_http_server.h`<br/>`esp_netif.h` |
| 196 | `protocols/http_server/simple` | HTTP-SERVER | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_eth.h`<br/>`esp_event.h`<br/>`esp_http_server.h` |
| 197 | `protocols/http_server/ws_echo_server` | HTTP-SERVER | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_eth.h`<br/>`esp_event.h`<br/>`esp_http_server.h` |
| 198 | `protocols/https_mbedtls` | HTTP-CLIENT | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`mbedtls/error.h` |
| 199 | `protocols/https_request` | HTTP-CLIENT | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`esp_netif_sntp.h` |
| 200 | `protocols/https_server/simple` | HTTP-SERVER | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_eth.h`<br/>`esp_event.h`<br/>`esp_https_server.h` |
| 201 | `protocols/https_server/wss_server` | HTTP-SERVER | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_eth.h`<br/>`esp_event.h`<br/>`esp_https_server.h` |
| 202 | `protocols/https_x509_bundle` | HTTP-CLIENT | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`esp_tls.h` |
| 203 | `protocols/icmp/pmtu_probe` | SOCKET-ICMP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`lwip/inet.h` |
| 204 | `protocols/icmp_echo` | SOCKET-ICMP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_console.h`<br/>`esp_event.h`<br/>`lwip/inet.h` |
| 205 | `protocols/l2tap` | PROTO-OTHER | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_eth.h`<br/>`esp_event.h`<br/>`esp_vfs_l2tap.h` |
| 206 | `protocols/mqtt` | MQTT | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`mqtt_client.h` |
| 207 | `protocols/mqtt5` | MQTT | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`mqtt_client.h` |
| 208 | `protocols/smtp_client` | PROTO-OTHER | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`mbedtls/base64.h`<br/>`mbedtls/error.h` |
| 209 | `protocols/sntp` | PROTO-OTHER | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_event.h`<br/>`esp_netif_sntp.h`<br/>`esp_sleep.h` |
| 210 | `protocols/sockets/icmpv6_ping` | SOCKET-ICMP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif_ip_addr.h`<br/>`lwip/icmp6.h` |
| 211 | `protocols/sockets/non_blocking` | SOCKET-ICMP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`nvs_flash.h`<br/>`sys/socket.h` |
| 212 | `protocols/sockets/tcp_client` | SOCKET-ICMP | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`nvs_flash.h` |
| 213 | `protocols/sockets/tcp_client_multi_net` | SOCKET-ICMP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`nvs_flash.h` |
| 214 | `protocols/sockets/tcp_server` | SOCKET-ICMP | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`lwip/err.h` |
| 215 | `protocols/sockets/tcp_transport_client` | SOCKET-ICMP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`nvs_flash.h` |
| 216 | `protocols/sockets/udp_client` | SOCKET-ICMP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`lwip/err.h` |
| 217 | `protocols/sockets/udp_multicast` | SOCKET-ICMP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`lwip/err.h` |
| 218 | `protocols/sockets/udp_server` | SOCKET-ICMP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`lwip/err.h` |
| 219 | `protocols/static_ip` | PROTO-OTHER | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_eth.h`<br/>`esp_event.h`<br/>`nvs_flash.h` |

### A.storage（27 个示例）
| 清单序号 | 上游示例路径 | 契约族 | 登记配置 / 后端 / 状态 | 目标表 | 官方源码入口提示 |
|---|---|---|---|---|---|
| 391 | `storage/custom_flash_driver` | MEDIA-STORAGE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_flash_chips/spi_flash_chip_boya.h`<br/>`esp_flash_chips/spi_flash_chip_driver.h`<br/>`esp_flash_chips/spi_flash_chip_gd.h` |
| 392 | `storage/emmc` | MEDIA-STORAGE | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/sdmmc_host.h`<br/>`esp_vfs_fat.h` |
| 393 | `storage/fatfs/bdl_wl` | FS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_partition.h`<br/>`esp_vfs.h`<br/>`esp_vfs_fat.h` |
| 394 | `storage/fatfs/ext_flash` | FS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_flash.h`<br/>`esp_flash_spi_init.h`<br/>`esp_partition.h` |
| 395 | `storage/fatfs/fatfsgen` | STORAGE-TOOLS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_vfs.h`<br/>`esp_vfs_fat.h` |
| 396 | `storage/fatfs/fs_operations` | FS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_vfs.h`<br/>`esp_vfs_fat.h` |
| 397 | `storage/fatfs/getting_started` | FS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_vfs.h`<br/>`esp_vfs_fat.h` |
| 398 | `storage/littlefs` | FS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_err.h`<br/>`esp_littlefs.h` |
| 399 | `storage/nvs/nvs_bootloader` | NVS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `nvs_bootloader.h`<br/>`nvs_bootloader_example_utils.h`<br/>`nvs_sec_provider.h` |
| 400 | `storage/nvs/nvs_console` | NVS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/uart.h`<br/>`driver/uart_vfs.h`<br/>`esp_console.h` |
| 401 | `storage/nvs/nvs_iteration` | NVS | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `nvs.h`<br/>`nvs_flash.h` |
| 402 | `storage/nvs/nvs_rw_blob` | NVS | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/gpio.h`<br/>`nvs.h`<br/>`nvs_flash.h` |
| 403 | `storage/nvs/nvs_rw_value` | NVS | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `nvs.h`<br/>`nvs_flash.h` |
| 404 | `storage/nvs/nvs_rw_value_cxx` | NVS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `nvs.h`<br/>`nvs_flash.h`<br/>`nvs_handle.hpp` |
| 405 | `storage/nvs/nvs_statistics` | NVS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `nvs.h`<br/>`nvs_flash.h` |
| 406 | `storage/nvs/nvsgen` | STORAGE-TOOLS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `nvs.h`<br/>`nvs_flash.h` |
| 407 | `storage/partition_api/partition_find` | MEDIA-STORAGE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_partition.h` |
| 408 | `storage/partition_api/partition_mmap` | MEDIA-STORAGE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_partition.h` |
| 409 | `storage/partition_api/partition_ops` | MEDIA-STORAGE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_partition.h` |
| 410 | `storage/parttool` | STORAGE-TOOLS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_err.h`<br/>`esp_log.h` |
| 411 | `storage/perf_benchmark` | MEDIA-STORAGE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/sdmmc_defs.h`<br/>`driver/sdmmc_host.h`<br/>`driver/sdmmc_types.h` |
| 412 | `storage/sd_card/sdmmc` | MEDIA-STORAGE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/sdmmc_host.h`<br/>`esp_vfs_fat.h` |
| 413 | `storage/sd_card/sdspi` | MEDIA-STORAGE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_vfs_fat.h` |
| 414 | `storage/semihost_vfs` | FS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_vfs_semihost.h` |
| 415 | `storage/spiffs` | FS | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_err.h`<br/>`esp_log.h` |
| 416 | `storage/spiffsgen` | STORAGE-TOOLS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_err.h`<br/>`esp_log.h` |
| 417 | `storage/wear_levelling` | MEDIA-STORAGE | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_vfs.h`<br/>`esp_vfs_fat.h` |

### A.system（67 个示例）
| 清单序号 | 上游示例路径 | 契约族 | 登记配置 / 后端 / 状态 | 目标表 | 官方源码入口提示 |
|---|---|---|---|---|---|
| 117 | `system/app_trace_basic` | SYS-OBS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_app_trace.h` |
| 118 | `system/app_trace_to_plot` | SYS-OBS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_app_trace.h`<br/>`esp_trace.h` |
| 119 | `system/base_mac_address` | SYS-OBS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_log.h`<br/>`esp_mac.h` |
| 120 | `system/console/advanced` | SYS-OBS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/rtc_io.h`<br/>`driver/uart.h` |
| 121 | `system/console/basic` | SYS-OBS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_console.h`<br/>`esp_vfs_dev.h`<br/>`esp_vfs_fat.h` |
| 122 | `system/deep_sleep` | PM | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `driver/gpio.h`<br/>`driver/rtc_io.h`<br/>`esp_sleep.h` |
| 123 | `system/deep_sleep_wake_stub` | PM | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/rtc_io.h`<br/>`esp_sleep.h` |
| 125 | `system/esp_event/default_event_loop` | CORE-CONC | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_log.h`<br/>`event_source.h` |
| 126 | `system/esp_event/user_event_loops` | CORE-CONC | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_event_base.h` |
| 127 | `system/esp_timer` | SYS-TIME | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_sleep.h`<br/>`esp_timer.h` |
| 128 | `system/esp_trace` | SYS-OBS | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_heap_caps.h`<br/>`esp_trace.h`<br/>`esp_trace_port_encoder.h` |
| 129 | `system/eventfd` | CORE-CONC | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gptimer.h`<br/>`esp_timer.h`<br/>`esp_vfs.h` |
| 130 | `system/flash_suspend` | SYS-TIME | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gptimer.h`<br/>`esp_flash.h`<br/>`esp_partition.h` |
| 131 | `system/freertos/basic_freertos_smp_usage` | CORE-CONC | `wasm_sim_standard` / `wasm_browser` / `verified` | 未列ESP32 | `esp_console.h`<br/>`esp_timer.h` |
| 132 | `system/freertos/real_time_stats` | CORE-CONC | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_err.h`<br/>`freertos/FreeRTOS.h` |
| 133 | `system/gcov` | SYS-OBS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`esp_app_trace.h` |
| 134 | `system/gdbstub` | SYS-OBS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_log.h`<br/>`esp_system.h` |
| 135 | `system/heap_task_tracking/advanced` | HEAP-MAP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_heap_caps.h`<br/>`esp_heap_task_info.h` |
| 136 | `system/heap_task_tracking/basic` | HEAP-MAP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_heap_caps.h`<br/>`esp_heap_task_info.h` |
| 137 | `system/himem` | HEAP-MAP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_heap_caps.h`<br/>`nvs_flash.h` |
| 138 | `system/ipc/ipc_isr/riscv` | CORE-CONC | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_ipc_isr.h` |
| 139 | `system/ipc/ipc_isr/xtensa` | CORE-CONC | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_ipc_isr.h` |
| 140 | `system/light_sleep` | PM | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/uart.h`<br/>`driver/uart_wakeup.h` |
| 141 | `system/nmi_isr` | CORE-CONC | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h` |
| 142 | `system/ota/advanced_https_ota` [deferred] | OTA | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_http_client.h`<br/>`esp_https_ota.h` |
| 143 | `system/ota/native_ota_example` | OTA | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`esp_event.h`<br/>`esp_flash_partitions.h` |
| 144 | `system/ota/otatool` | OTA | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_ota_ops.h`<br/>`esp_partition.h` |
| 145 | `system/ota/partitions_ota` [deferred] | OTA | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_flash.h`<br/>`esp_flash_partitions.h` |
| 146 | `system/ota/simple_ota_example` | OTA | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_event.h`<br/>`esp_http_client.h`<br/>`esp_https_ota.h` |
| 147 | `system/perfmon` | SYS-OBS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_sleep.h`<br/>`esp_timer.h` |
| 148 | `system/pthread` | CORE-CONC | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `pthread.h` |
| 149 | `system/rt_mqueue` | CORE-CONC | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `mqueue.h`<br/>`pthread.h` |
| 150 | `system/select` | CORE-CONC | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/uart.h`<br/>`driver/uart_vfs.h`<br/>`esp_netif.h` |
| 151 | `system/startup_time` | SYS-TIME | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_log.h` |
| 152 | `system/sysview_tracing` | SYS-OBS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gptimer.h`<br/>`esp_app_trace.h`<br/>`esp_trace.h` |
| 153 | `system/sysview_tracing_heap_log` | SYS-OBS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_app_trace.h`<br/>`esp_heap_trace.h`<br/>`esp_trace.h` |
| 154 | `system/task_watchdog` | SYS-TIME | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_task_wdt.h` |
| 155 | `system/ulp/lp_core/build_system` | ULP-LP | `host_native_standard` / `host_native` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp_build_system_example.h`<br/>`ulp_lp_core.h` |
| 156 | `system/ulp/lp_core/debugging` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp_lp_core.h`<br/>`ulp_lp_core_utils.h` |
| 157 | `system/ulp/lp_core/gpio` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`driver/rtc_io.h`<br/>`esp_sleep.h` |
| 158 | `system/ulp/lp_core/gpio_intr_pulse_counter` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`driver/rtc_io.h`<br/>`esp_sleep.h` |
| 159 | `system/ulp/lp_core/gpio_wakeup` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/rtc_io.h`<br/>`esp_sleep.h`<br/>`ulp_lp_core.h` |
| 160 | `system/ulp/lp_core/inter_cpu_critical_section` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `ulp_lp_core.h`<br/>`ulp_lp_core_critical_section_shared.h`<br/>`ulp_lp_core_print.h` |
| 161 | `system/ulp/lp_core/interrupt` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp_lp_core.h`<br/>`ulp_lp_core_interrupts.h` |
| 162 | `system/ulp/lp_core/lp_adc` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp_lp_core.h`<br/>`ulp_lp_core_lp_adc_shared.h` |
| 163 | `system/ulp/lp_core/lp_i2c` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp_lp_core.h`<br/>`ulp_lp_core_i2c.h` |
| 164 | `system/ulp/lp_core/lp_mailbox` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp_lp_core.h`<br/>`ulp_lp_core_mailbox.h` |
| 165 | `system/ulp/lp_core/lp_spi` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp_lp_core.h`<br/>`ulp_lp_core_print.h` |
| 166 | `system/ulp/lp_core/lp_timer_interrupt` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_sleep.h`<br/>`ulp_lp_core.h`<br/>`ulp_lp_core_interrupts.h` |
| 167 | `system/ulp/lp_core/lp_touch` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/touch_sens.h`<br/>`esp_sleep.h`<br/>`ulp_lp_core.h` |
| 168 | `system/ulp/lp_core/lp_uart/lp_uart_char_seq_wakeup` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp_lp_core.h`<br/>`ulp_lp_core_lp_uart_shared.h` |
| 169 | `system/ulp/lp_core/lp_uart/lp_uart_echo` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp_lp_core.h`<br/>`ulp_lp_core_uart.h` |
| 170 | `system/ulp/lp_core/lp_uart/lp_uart_print` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp_lp_core.h`<br/>`ulp_lp_core_print.h` |
| 171 | `system/ulp/ulp_fsm/ulp_adc` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/gpio.h`<br/>`driver/rtc_io.h`<br/>`esp_sleep.h` |
| 172 | `system/ulp/ulp_fsm_riscv_combined/counter` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`driver/rtc_io.h`<br/>`esp_sleep.h` |
| 173 | `system/ulp/ulp_riscv/adc` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp/example_config.h`<br/>`ulp_adc.h` |
| 174 | `system/ulp/ulp_riscv/ds18b20_onewire` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`driver/rtc_io.h`<br/>`esp_sleep.h` |
| 175 | `system/ulp/ulp_riscv/gpio` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`driver/rtc_io.h`<br/>`esp_sleep.h` |
| 176 | `system/ulp/ulp_riscv/gpio_interrupt` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`esp_sleep.h`<br/>`ulp_riscv.h` |
| 177 | `system/ulp/ulp_riscv/gpio_pulse_counter` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/gpio.h`<br/>`driver/rtc_io.h`<br/>`esp_sleep.h` |
| 178 | `system/ulp/ulp_riscv/i2c` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp_main.h`<br/>`ulp_riscv.h` |
| 179 | `system/ulp/ulp_riscv/interrupts` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp_main.h`<br/>`ulp_riscv.h` |
| 180 | `system/ulp/ulp_riscv/touch` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `driver/touch_sens.h`<br/>`esp_sleep.h`<br/>`ulp_main.h` |
| 181 | `system/ulp/ulp_riscv/uart_print` | ULP-LP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_sleep.h`<br/>`ulp_main.h`<br/>`ulp_riscv.h` |
| 182 | `system/unit_test` | SYS-OBS | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_random.h`<br/>`limits.h` |
| 183 | `system/unit_test/test` | SYS-OBS | `wasm_sim_standard` / `wasm_browser` / `planned` | 未提供 | `unity.h` |
| 184 | `system/xip_from_psram` | HEAP-MAP | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_flash.h`<br/>`esp_partition.h`<br/>`esp_timer.h` |

### A.wifi（24 个示例）
| 清单序号 | 上游示例路径 | 契约族 | 登记配置 / 后端 / 状态 | 目标表 | 官方源码入口提示 |
|---|---|---|---|---|---|
| 220 | `wifi/espnow` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`nvs_flash.h` |
| 221 | `wifi/fast_scan` | WIFI | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_event.h`<br/>`nvs_flash.h` |
| 222 | `wifi/ftm` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_console.h`<br/>`esp_event.h`<br/>`nvs_flash.h` |
| 223 | `wifi/getting_started/softAP` | WIFI | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_event.h`<br/>`lwip/err.h`<br/>`lwip/sys.h` |
| 224 | `wifi/getting_started/station` | WIFI | `wasm_sim_standard` / `wasm_browser` / `verified`<br/>`wasm_sim_node` / `wasm_node` / `planned` | 列ESP32 | `esp_event.h`<br/>`lwip/err.h`<br/>`lwip/sys.h` |
| 225 | `wifi/iperf` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_console.h`<br/>`nvs_flash.h` |
| 226 | `wifi/itwt` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 未列ESP32 | `esp_console.h`<br/>`esp_event.h`<br/>`esp_netif.h` |
| 227 | `wifi/power_save` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `driver/uart.h`<br/>`driver/uart_vfs.h`<br/>`esp_event.h` |
| 228 | `wifi/roaming/roaming_11kvr` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`nvs_flash.h` |
| 229 | `wifi/roaming/roaming_app` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`lwip/err.h`<br/>`lwip/sys.h` |
| 230 | `wifi/scan` | WIFI | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_event.h`<br/>`nvs_flash.h` |
| 231 | `wifi/smart_config` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`nvs_flash.h` |
| 232 | `wifi/softap_sta` | WIFI | `wasm_sim_standard` / `wasm_browser` / `verified` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`esp_netif_net_stack.h` |
| 233 | `wifi/wifi_aware/nan_console` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_console.h`<br/>`esp_event.h`<br/>`esp_netif.h` |
| 234 | `wifi/wifi_aware/nan_publisher` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`nvs_flash.h` |
| 235 | `wifi/wifi_aware/nan_subscriber` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`nvs_flash.h`<br/>`ping/ping_sock.h` |
| 236 | `wifi/wifi_aware/usd_publisher` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`nvs_flash.h` |
| 237 | `wifi/wifi_aware/usd_subscriber` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`nvs_flash.h` |
| 238 | `wifi/wifi_eap_fast` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`nvs_flash.h` |
| 239 | `wifi/wifi_easy_connect/dpp-enrollee` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`nvs_flash.h` |
| 240 | `wifi/wifi_enterprise` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`esp_netif.h`<br/>`nvs_flash.h` |
| 241 | `wifi/wifi_nvs_config` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`lwip/err.h`<br/>`lwip/sys.h` |
| 242 | `wifi/wps` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`nvs_flash.h` |
| 243 | `wifi/wps_softap_registrar` | WIFI | `wasm_sim_standard` / `wasm_browser` / `planned` | 列ESP32 | `esp_event.h`<br/>`nvs_flash.h` |

## 附录 B：63 个原子能力与迁移清单的引用对照

引用数按示例去重，不因 STA 的第二配置重复计数。`status` 是能力字典原字段，与配置 `delivery_state` 正交；`planned` 不自动等于可执行。零引用需经业务核对区分未来能力与漏映射。

| 原子 capability | 字典 status | 迁移示例引用数 |
|---|---|---:|
| `cap.core.fiber_task` | `implemented` | 312 |
| `cap.core.sync_tokens` | `implemented` | 312 |
| `cap.core.category_heap` | `implemented` | 12 |
| `cap.core.hot_restart` | `planned` | 0 |
| `cap.analog.adc_oneshot` | `implemented` | 6 |
| `cap.analog.adc_dma` | `implemented` | 1 |
| `cap.analog.dac_out` | `implemented` | 3 |
| `cap.pulse.tx_buffer` | `implemented` | 7 |
| `cap.pulse.rx_capture` | `planned` | 1 |
| `cap.pulse.pcnt_quad` | `planned` | 2 |
| `cap.pulse.ledc_fade` | `implemented` | 4 |
| `cap.pulse.mcpwm_motor` | `planned` | 4 |
| `cap.pulse.touch_pad` | `planned` | 0 |
| `cap.bus.i2c_master` | `implemented` | 7 |
| `cap.bus.spi_master` | `implemented` | 7 |
| `cap.bus.uart_stream` | `implemented` | 18 |
| `cap.bus.temp_sensor` | `planned` | 2 |
| `cap.bus.parlio` | `planned` | 3 |
| `cap.proto.ws2812` | `implemented` | 5 |
| `cap.proto.nec_ir` | `planned` | 1 |
| `cap.proto.twai_can` | `planned` | 0 |
| `cap.proto.i2s_stream` | `planned` | 2 |
| `cap.vfs.mem_sandbox` | `implemented` | 30 |
| `cap.vfs.nvs_partition` | `implemented` | 129 |
| `cap.vfs.spiffs_format` | `implemented` | 4 |
| `cap.vfs.fatfs_vfs` | `planned` | 20 |
| `cap.storage.wear_levelling` | `planned` | 2 |
| `cap.storage.sdmmc_host` | `planned` | 7 |
| `cap.storage.partition_api` | `implemented` | 16 |
| `cap.net.event_pump` | `implemented` | 79 |
| `cap.net.http_client_mock` | `implemented` | 3 |
| `cap.net.mqtt_client_mock` | `implemented` | 2 |
| `cap.net.http_server` | `implemented` | 8 |
| `cap.net.bsd_socket` | `implemented` | 2 |
| `cap.net.sntp_client` | `implemented` | 1 |
| `cap.net.host_socket` | `planned` | 5 |
| `cap.net.host_ws_tunnel` | `planned` | 0 |
| `cap.wifi.station_mode` | `implemented` | 55 |
| `cap.wifi.ap_mode` | `implemented` | 1 |
| `cap.mesh.esp_now` | `planned` | 1 |
| `cap.ble.gap_adv` | `implemented` | 50 |
| `cap.ble.gatt_server` | `implemented` | 48 |
| `cap.ble.gatt_client` | `planned` | 0 |
| `cap.ble.smp_security` | `planned` | 0 |
| `cap.pm.light_sleep` | `implemented` | 34 |
| `cap.pm.deep_sleep` | `implemented` | 0 |
| `cap.pm.dynamic_freq` | `implemented` | 4 |
| `cap.crypto.mbedtls_shim` | `planned` | 2 |
| `cap.crypto.hw_sha_aes` | `planned` | 0 |
| `cap.system.console_cmd` | `verified` | 15 |
| `cap.system.ota_update` | `implemented` | 10 |
| `cap.system.app_trace` | `planned` | 6 |
| `cap.usb.cdc_acm` | `planned` | 0 |
| `cap.usb.serial_jtag` | `planned` | 0 |
| `cap.coproc.ulp_fsm` | `planned` | 0 |
| `cap.coproc.ulp_riscv` | `planned` | 0 |
| `cap.media.camera_dma` | `planned` | 0 |
| `cap.dma.memcpy_sim` | `planned` | 0 |
| `cap.dma.double_buffer` | `planned` | 0 |
| `cap.irq.isr_dispatch` | `implemented` | 10 |
| `cap.irq.edge_trigger` | `implemented` | 60 |
| `cap.build.component_reg` | `planned` | 0 |
| `cap.build.kconfig_parse` | `planned` | 0 |

## 附录 C：索引校验与上游扫描身份

- 示例索引：312 个唯一 upstream_path，313 个 execution 配置；无范围外条目。
- 契约族计数：ADC-CMP=4, BLE=49, BUILD=19, CORE-CONC=11, CXX=3, DAC-SDM=6, DMA-BITS=3, FS=7, GPIO=5, HEAP-MAP=4, HTTP-CLIENT=7, HTTP-SERVER=10, I2C-I3C=7, I2S=6, LCD=6, LEDC=4, MCPWM=6, MEDIA-STORAGE=9, MQTT=2, NETWORK=5, NVS=7, OTA=5, PARLIO=3, PCNT=1, PM=3, PROTO-OTHER=5, RMT=7, SOCKET-ICMP=11, SPI=8, START=2, STORAGE-TOOLS=4, SYS-OBS=13, SYS-TIME=4, TEMP=2, TIMER=3, TOUCH=2, UART=8, ULP-LP=27, WIFI=24。
- README 目标信号：列 ESP32 226，未列 ESP32 81，未提供 5。
- 原子能力索引：63 项；迁移范围实际引用 46 项；引用中 planned 14 项。
- 上游扫描文件唯一集合：821 个（README 与 C/C++/汇编，不含 SDK 组件源闭包）；路径→原始字节 SHA-256 的排序 JSON（UTF-8，紧凑 separators）摘要为 `a8e87b58871922422671f080eeeb7962332e580a40e67bf2ef63cc49f8a34356`。这是本次本机静态扫描身份，不是厂商发布包或上游 Git revision。
- 全部判断基于第 2 节快照；本索引本身不作为 AFG 验收凭据。
