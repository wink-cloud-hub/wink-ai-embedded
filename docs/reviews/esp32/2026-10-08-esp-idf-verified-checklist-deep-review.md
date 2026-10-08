<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ESP-IDF 已完成 Checklist 项现状深度评审

| 项 | 内容 |
|---|---|
| 日期 | 2026-10-08，Asia/Shanghai |
| 评审快照 | `32eb906914ec3ade2f21bf3d77a1098bc9864468`；开始与结束核对 HEAD 相同 |
| 对象 | `esp_idfv61/CHECKLIST.md` 对应 SSOT 中全部 46 个 `verified` execution；配置均为 `wasm_sim_standard` |
| 方法 | 当前状态评审；Standards、Spec 两轴独立审查，主审补核治理工具；用户未要求历史分支差异 |
| 评审者 | Codex AI 辅助评审，不代表人工架构审计签署 |
| 操作边界 | 只读源码、场景、资产、历史报告及门禁；仅新增本评审记录 |
| 未执行 | 固件构建、仿真重放、真机验证、状态迁移、凭据写入、看板生成 |
| 后续性质 | 本文是时间点快照；建议不等同于 Accepted ADR 或已批准实施计划 |

依据：[AGENTS.md](../../../AGENTS.md)、[C 规则索引](../../../.agents/rules/c-code.md)、[分类规范](../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md)、[PLAYBOOK](../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/PLAYBOOK.md)、[治理 SOP](../../../.agents/skills/governance-sop-esp/SKILL.md)、[API 覆盖及降级矩阵](../../../wink-micro-os/frameworks/esp_idf/docs/02-api-coverage-matrix.md)。此前网络评审及整改记录仅作历史参照，不照搬已修问题。

## 1. 评审判断与覆盖边界

**有必要继续深入评审。现有接入、确定性场景和证据治理基础可以保留，但 `verified` 不能统一解释成核心业务、故障处理和长期复用能力均已验收完成。**

本轮全量核对了 46 个配置的登记信息、绑定场景、绑定报告及资产摘要；同时沿 ADC/DAC、GPTimer、LEDC、SPI、SPIFFS、NVS 和治理工具的关键调用链深入抽查。没有逐行审计所有 Wi-Fi、BLE、网络协议及 FreeRTOS 实现，也没有动态复现本文全部源码问题。

原厂镜像业务源码按兼容约束处理，不要求改成 Wink 命名或错误码形式。门面保留原厂 C ABI，内部 PAL 继续遵守本仓静态分发、负错误码与双 Target 约束。已明确登记的行为降级不直接判为 Standards 违规；其是否符合 checklist 声明由 Spec 轴单独评价。

已有的正面基础包括：原厂业务与适配配置分离、静态资源池、代际句柄、虚拟时间接口、独立能力目录、只读门禁，以及配置绑定的双实证格式。主要不足集中在观察对象真实性、回调生命周期、时序语义、模型归属、输入与报告绑定、声明和实现的一致性。

## 2. 本轮实际核验

| 检查 | 结果 | 可以证明的范围 |
|---|---|---|
| `run_gates.py --gate 1` | 12/12 执行，0 skip，0 error，0 warning | 当前 Gate 1 注册规则接受工作区数据 |
| `evidence_verifier.py --verify-all` | 46/46 通过 | 现有普通凭据判据通过 |
| 对每份绑定报告额外调用 `report_contract.validate_scenario_report()` | 46/46 通过 | 场景身份、步骤顺序、类型、matcher、观测字段和计数符合该更严格契约 |
| 绑定正向场景统计 | 295 steps，266 个 `ASSERT_*`，其中 37 个辅助电源断言 | 步骤数量；不等于 266 个完整业务契约或独立覆盖点 |
| `verify_twin_evidence()` 全量只读核对 | 4 项通过：#098、#154、#196、#403 | 当前工具接受这四项配置的正式正向与故障报告配对 |
| `check_vendor_app_upstream.py` | 46 apps，24 errors，1 warning，退出 1 | 本地追溯元数据和该检查器存在不一致；未传 `--idf-tree`，未独立核对官方 SDK |
| #083 归档 Wasm export section | 136 个 exports，没有 timer 相关 export | 当前归档资产未暴露现源码的 `sim_timer_get_counter`，不能把源码现状等同于历史运行实现 |

上游检查的 24 个错误包含 20 个 `app_name` 规则差异、3 个文件路径查找失败和 1 个哈希不匹配。命名差异需要先裁定检查器与当前 Manifest 的规范口径；不能把全部错误解释为源码损坏。

全部现有报告通过更严格的逐步核验，是本轮确认的积极事实。下文报告核验器的反例是其拒绝能力存在缺口，不表示已经发现这 46 份报告互相串用。

## 3. Standards：当前实现问题

### S-01 / P1：GPTimer 计数观察错误地返回全局虚拟时钟

**证据：静态确认。** [esp_gptimer.c](../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_gptimer.c) 第 301–313 行，尤其第 310 行，把内部 `struct gptimer_t *` 传给 `gptimer_get_raw_count()`。后者要求编码后的代际 token。[esp_sim_handle.c](../../../wink-micro-os/frameworks/esp_idf/src/core/esp_sim_handle.c) 第 48–53 行拒绝最低位为 0 的普通对齐地址，因此该调用失败，观察函数随后返回 `pal_os_get_us()`；不存在的槽也返回时钟。

**影响：** 停止、删除或重载 GPTimer 后，观察值仍可能按全局虚拟时间递增。看起来正常的单调计数掩盖了被测设备状态，违反 ADR-0012 契约诚实。

**建议与验收：** 使用有效 token 查询，观察失败必须有显式无效语义；覆盖未创建、运行、暂停、设置非零计数、自动重载和删除后的观察。不能用全局时钟补齐设备值。归档 Wasm 缺少此 export，见 Q-02；本轮没有宣称在该资产上动态复现当前源码 bug。

### S-02 / P1：单次定时器回调中重新装载的闹钟被旧事件关闭

**证据：静态确认。** [pal_wasm_hwtimer.c](../../../wink-micro-os/targets/wasm/pal_wasm_hwtimer.c) 第 128–134 行先调用用户回调，再依据当前槽的 `oneshot` 无条件置 `is_running=false`。[原厂 GPTimer 示例](../../../wink-micro-app/vendor/esp_idfv61/peripherals/gptimer_alarm/gptimer_example_main.c) 第 48–61 行在 v3 回调中重设下一次闹钟；门面会 deinit/init/start 同一槽。

**影响：** 回调刚建立的新单次闹钟会被旧事件的收尾逻辑关闭，动态告警阶段无法按业务契约持续运行。属于回调重入下的生命周期错误，不在已有 10 ms 周期下限降级范围内。

**建议与验收：** 先消费旧事件再调用回调，或以内部槽代际/调度版本确认回调没有重建槽；同时核对绝对 `alarm_count` 到下一次相对等待时间的换算。覆盖回调内重装、停止、删除及重建，至少观察连续三次告警间隔。

### S-03 / P1：ADC 连续采样由读取驱动，并把输入失败转换成有效数据

**证据：静态确认。** [esp_adc.c](../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_adc.c) 第 489–499 行未把 `sample_freq_hz` 转成采样调度；第 526、545 行由 read 主动 pump。Wasm PAL 在 start 时也同步 pump。[pal_wasm_ch3_adc.c](../../../wink-micro-os/targets/wasm/pal_wasm_ch3_adc.c) 第 211–215 行在真实采样失败时填入 `1000 + i * 10`。

**影响：** 样本数量依赖读取调用次数，不能据此验证 #003 的 20 kHz 采样节奏、消费者暂停后的积压/溢出；输入或配置错误又被伪造样本掩盖。违反失败语义与虚拟时间因果要求。

**建议与验收：** 由虚拟时间生产有界采样帧，消费与生产解耦；传播真实采样失败及可观察诊断。保持消费者不读一段时间，核对样本数、溢出与恢复；注入无效通道时不得得到正常数据。

### S-04 / P2：SPI 通用门面内置跨设备共享的 EEPROM 模型

**证据：静态确认；维护性判断另列。** [esp_spi.c](../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_spi.c) 第 23–24 行维护全局 EEPROM 存储及写使能，第 147–179 行对通用 SPI 事务解释 AT93C46D 命令。该状态没有按设备实例隔离。

**影响：** 多个 EEPROM 会共享模型状态，其他 SPI 器件的命令也进入同一解释逻辑。新增器件需要修改通用门面，违反分类规范 §二的门面/器件模型职责边界，也构成可能的 Divergent Change。已有同步轮询、无 DMA 的降级不涵盖该模型归属。

**建议与验收：** 把协议与存储移到按总线/设备实例绑定的模型，门面只适配事务。用两个独立 EEPROM 和一种非 EEPROM SPI 设备检查状态隔离及错误传播；保持静态分发，不引入运行期 vtable。

### S-05 / P2：SPIFFS 控制面与实际文件操作使用不同后端

**证据：静态确认。** [esp_spiffs.c](../../../wink-micro-os/frameworks/esp_idf/src/core/esp_spiffs.c) 第 47–51 行初始化 RAM VFS，却只通过 libc `mkdir()` 创建目录；第 76 行 format 清理 `esp_vfs_ram`。实际[原厂应用 main.c](../../../wink-micro-app/vendor/esp_idfv61/storage/spiffs/main.c) 第 83、101、108 行经 libc `fopen/rename` 操作文件，本仓未见该应用的宏或链接重定向。`info()` 第 93 行固定返回 used=0。

**影响：** format 清理的对象与业务文件不一致；容量、格式化和挂载状态不能可靠描述实际数据。仅一次写入回读场景无法发现该分裂。

**建议与验收：** 统一文件访问、格式化、容量统计和挂载生命周期的实际后端。写入后 used 应按声明模型变化；format 后旧文件不可读；挂载失败、卸载和复位不得残留虚假可用状态。

## 4. Spec：声明、验收和证据缺口

### Q-01 / P1：#013/#014 的 DAC 场景回读了测试输入

**证据：场景与历史报告。** [cosine 场景](../../../wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_cosine_wave/unisim-scenarios/peripherals_dac_dac_cosine_wave.scenario.json) 第 13–24 行用 `INPUT_ANALOG` 注入 0.5/0.8，第 36–44 行断言同通道相同值；后续注入 0.3 再断言 0.3。[oneshot 场景](../../../wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_oneshot/unisim-scenarios/dac_dac_oneshot.scenario.json) 同样注入 0.4/0.7 后回读。历史报告的实际值与输入逐项相等。

**缺口：** 没有证明观测来自固件的 DAC 输出，也没有覆盖余弦频率/相位或 oneshot 的 500 ms 阶梯变化。不满足分类规范第 65 行 A-1/A-2；删除 DAC 业务后会否失败尚无有效证据。

**建议与验收：** 通过公开输出或明确的 DAC→ADC 回环模型检查固件输出；保留 fixture 与预期不变，停用 DAC 写入/启动必须使对应业务断言失败。需要输出通道能力时应记录缺口，不继续以外部模拟输入替代结果。

### Q-02 / P1：#083 的场景不能证明实际 GPTimer 计数与告警

**证据：归档资产、场景与当前源码分别核对。** [场景](../../../wink-micro-app/vendor/esp_idfv61/peripherals/gptimer_alarm/unisim-scenarios/gptimer_alarm.scenario.json) 第 22 行起只检查 500/1000/2000 ms 时计数约为 500000/1000000/2000000，历史实际值恰好等于这些时刻。当前 getter 存在 S-01 缺陷；当前归档 Wasm 的 136 个 exports 又没有 timer 出口。

**缺口：** 这些观察无法归因到当前 C getter，不能检出停止、重载、未启动或动态告警失败。场景虽设 12 s timeout，最后断言在 2 s，后续告警业务缺少检查。不满足 A-1/A-4。

**建议与验收：** 先预检所需观测 ABI，缺失应显式失败；以暂停、非零偏置、重载和实际告警回调/队列消息区分计数器与时钟。新代码重建后单独复验，不能沿用旧资产摘要证明修复完成。

### Q-03 / P1：已声明故障验收尚未形成全量正式闭包

**证据：全量登记与具体场景。** 42 项没有通过当前格式绑定的正式双实证包，仅 #098/#154/#196/#403 有对应包。[MQTT 负例](../../../wink-micro-app/vendor/esp_idfv61/protocols/mqtt_tcp/unisim-scenarios/mqtt_tcp.fail.scenario.json) 只提供正常 Wi-Fi AP，第 45、52 行均断言 `CONNECTED`，没有 Broker 拒绝、断开或重连激励；SSOT 第 13591–13594 行却声明 `MQTT_EVENT_DISCONNECTED` 验收。若干其他 `.fail` 场景只将 matcher 改成 `__CANARY_MUTANT_*`。

**缺口：** 故障文件存在、错误预期敏感性、业务变异杀伤和真实故障处理是不同证据。不能把 42 项缺少正式包直接推断为完全没有历史故障实验，但其完整故障验收目前未由绑定的正式证据证明。违反分类规范第 67、69 行的独立验收要求。

**建议与验收：** 为每个已声明 negative case 明确故障激励、业务出口和恢复基准；MQTT 检查断连事件及受控重连，而不是再次读取正常状态。按配置绑定完整集合，独立归档自检、固件依赖、业务变异和故障处理结果。

### Q-04 / P2：#049 的渐变声明超出已登记降级范围

**证据：源码、场景和文档。** 原厂示例的 `LEDC_TEST_FADE_TIME=3000`；[场景](../../../wink-micro-app/vendor/esp_idfv61/peripherals/ledc_ledc_fade/unisim-scenarios/peripherals_ledc_ledc_fade.scenario.json) 第 21–43 行要求 fade up、fade down 及后续阶段都在 0–800 ms 内出现。SSOT 第 3321 行称“硬件平滑渐变”，并声明 `axis_c_timer=cycle_accurate`。[API 矩阵](../../../wink-micro-os/frameworks/esp_idf/docs/02-api-coverage-matrix.md) 第 221–224 行则明确登记即时阶跃和同步回调降级。

**缺口：** 现有场景固定了降级行为，不能证明原厂渐变时序。该降级已被文档允许，因此不作为 Standards 硬违规；问题在能力、保真合同和验收名称没有保持一致。

**建议与验收：** 明确该配置验证的是阶跃子集，或实现虚拟时间渐变后检查中间占空比、完成时刻和等待语义。正确渐变实现不应因超过 800 ms 被旧测试判失败。

### Q-05 / P2：能力闭包与实际配置存在错位

**证据：SSOT 和实际配置。** #122 在 SSOT 第 7979 行只列 `cap.pm.light_sleep`，实际业务调用 `esp_deep_sleep_start()`；#223 在第 14747 行列 `cap.wifi.station_mode`，实际运行 `WIFI_MODE_AP`，却漏记已有 `cap.wifi.ap_mode`。#001 的 sdkconfig 明确启用 GPIO 分支，依赖仍包括 `cap.proto.ws2812`。

**影响：** 能力反向影响分析会漏选实际受影响应用，或因无关能力阻塞正确配置。能力 ID 存在且状态 satisfied，不能证明依赖归属正确，违反分类规范铁律一、二。

**建议与验收：** 按实际构建分支重审依赖；对于多个 execution 的差异，采用版本化的条件依赖契约，避免复制出第二份未经治理的事实源。改变 deep_sleep、AP 或 WS2812 能力时，受影响配置集合应与实际调用链一致。

### Q-06 / P2：原厂身份锁定元数据无效，追溯工具契约存在漂移

**证据：只读追溯检查。** [DAC Manifest](../../../wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_cosine_wave/wink-app.json) 第 13 行 SHA-256 只有 63 位；[SPIFFS Manifest](../../../wink-micro-app/vendor/esp_idfv61/storage/spiffs/wink-app.json) 第 13 行锁定不存在的 `spiffs_example_main.c`，实际编译 `main.c`。#024/#073 的头文件在 `include/`，锁定键没有目录，现检查器按根目录寻找时失败。

**判读限制：** SPIFFS 的实际 `main.c` 经现检查器规范化后的摘要，恰好等于锁定摘要，因此是本地文件映射失配，不能据此声称业务内容被修改。其余 20 项 app_name 差异也要先解决检查器命名规则与当前完整分类命名之间的漂移。

**建议与验收：** 按 PLAYBOOK 第 30 行要求锁定有效上游摘要；显式记录本地路径与上游路径，校验摘要格式、文件闭包和唯一映射。在 PR 的适用路径运行该检查，与现有 nightly 保持一致；不通过放宽哈希校验隐藏差异。

## 5. 主审补充：治理工具的具体缺陷

### G-01 / P1：普通报告核验接受跨应用报告及错误配置身份

[evidence_verifier.py](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py) 第 179–192 行只对步骤状态和数量作核验，不对场景 header、stepIndex/type/matcher/actual 作完整绑定；第 336 行仍调用此普通核验。也没有把登记 backend/target_soc/profile 与实际资产、报告运行身份交叉对账。

**本轮只在内存复制配置做了两组反例，没有修改磁盘清单、报告或审计字段：**

1. 保留 #003 ADC 的真实资产与场景摘要，只把 report ref 换成 #001 Blink 的真实七步报告：`verify_evidence()` 返回 `(True, [])`。更严格的 `validate_scenario_report(blink_report, adc_scenario)` 则因场景名字不匹配而拒绝。
2. 保留 #003 原始凭据，将 execution 的 `target_soc` 改为 `esp32s3`、backend 改为 `wasm_node`：普通 `verify_evidence()` 仍返回 `(True, [])`。

**建议与验收：** 普通交付复用现有严格报告契约，再验证资产芯片、实际后端、profile、run ID 与输入摘要。跨应用、重复步骤、错误 matcher、缺失观测及错配芯片均必须拒绝。该缺陷说明工具允许错误凭据进入，不能推断当前 46 份报告已经串用。

### G-02 / P1：旧写入器仍会回退配置并提前覆盖正式报告

[evidence_verifier.py](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py) 第 491–503 行在指定 config_id 不存在时继续选首个 Wasm 配置；第 464 行先覆盖正式 `run-report.json`，之后才验证；第 470 行验证时没有传入选定场景；第 512 行直接写 `verified`。默认场景选择仍依赖首个正向文件。

**影响：** 参数笔误可能写错配置，失败运行可能损伤上一份有效报告，多配置并行交付可能互相覆盖。新的候选 collector 已有更严格的唯一配置选择，不代表这个旧 `--write-app` 路径已被封堵。

**建议与验收：** 统一唯一配置选择和候选验证，完整检查后以事务方式晋升；正式报告按配置/run 隔离。未知或歧义配置必须在任何正式写入前失败；覆盖失败、核验失败或中断后旧正式证据应保持完整。

### G-03 / 演进建议：把源码、构建和证据的新鲜度连成一条可核验链

普通证据绑定三件资产和一个场景，不等同于绑定当前 C 源码、sdkconfig、门面/PAL 依赖及构建工具版本。`verified_commit` 是记录信息，当前普通核验器没有据此证明归档产物由这些输入构建。#083 当前源码与归档 ABI 的差异是具体例子。

候选流水线已有 app 输入摘要，可在技术设计中扩展为版本化构建指纹：包含应用、配置、实际依赖源码、工具链、观测 ABI 和 runner 身份，绑定不可变 run 目录。源码影响分析决定哪些配置需要重新验证，不能仅因旧二进制摘要未变就继续证明新实现。该演进需先形成设计/实施计划，本评审不擅自改 Schema 或交付状态。

## 6. 长期维护与扩展的补强顺序

| 方向 | 当前具体风险 | 建议形成的工程约束 |
|---|---|---|
| 观察真实性 | DAC 输入回读、GPTimer 时钟兜底 | 每个业务观察明确生产者、通道、单位、无效值；提供固件依赖与非等价变异证据 |
| 时序与生命周期 | ISR 重装被关闭、ADC 读取触发生产 | 明确回调可执行操作、旧事件消费时点及槽代际；生产由虚拟时间驱动 |
| 架构与实例隔离 | SPI 内嵌单一器件状态、SPIFFS 双后端 | 通用事务适配、器件模型和持久化后端分别有明确所有权；统一状态来源 |
| 兼容性与保真 | 全部填 cycle_accurate、渐变实际降级 | 按配置登记已支持子集及保真边界；测试接受正确实现并能击杀契约内缺陷 |
| 规模化治理 | 宽松旧核验/写入路径与严格新路径并存 | 复用唯一选择、严格报告校验和事务晋升，逐步退役重复规则 |
| 回归选择 | 实际依赖漏记/误记 | 审定调用链和配置分支；让能力反向闭包选中正确的配置 |
| 可复验性 | 源码已变而历史资产仍通过 | 绑定构建输入、依赖、工具链和 run；保存历史有效证据，禁止原地覆盖补齐 |

建议先修观察与拒绝能力（S-01、Q-01、G-01/G-02），再修时序/状态语义与领域模型，随后补齐声明负例、复位/恢复、组合应用和配置差分证据。新增实验应针对可证伪的业务契约，避免继续累计只验证正常日志或直接镜像实现的测试。

仅作为后续候选：NVS 原生文件提交在 [esp_nvs.c](../../../wink-micro-os/frameworks/esp_idf/src/core/esp_nvs.c) 第 540–541 行先 unlink 再 rename，存在中断/rename 失败导致旧提交丢失的窗口；与 API 矩阵第 249 行的“原子写入”声明不一致。该结论限定原生文件分支，不外推到浏览器保存桥；需要隔离故障实验决定修复范围。

实施上述复杂修改前，应按 AGENTS.md 产出技术设计/实施计划并获得用户确认；本轮已授权的评审工作不依赖实施确认。

## 7. 结论口径与后续留档

Standards 轴有 5 项主发现，其中最直接的正确性问题是 S-01 的 GPTimer 观察错误；Spec 轴有 6 项主发现，其中 Q-01 直接影响 DAC 业务验收有效性。两轴分别计数，不用样式合规抵消业务验收缺口。另有主审确认的 2 项治理工具缺陷及 1 项构建证据演进建议。

当前可确认的状态是“46 项登记 verified、46 份现有报告通过核验、4 项正式双实证包被当前工具接受”。本轮没有完成 46 项完整重新验收，也没有修改任何配置的交付状态。整改后应另建复验快照，保留本文历史判断。
