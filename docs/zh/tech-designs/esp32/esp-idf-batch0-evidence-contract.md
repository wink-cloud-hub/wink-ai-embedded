<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ESP-IDF Batch 0：候选证据、断言自检与双实证绑定契约

| 项 | 内容 |
|---|---|
| 日期 | 2026-10-05 |
| 范围 | 治理工具的证据判定与候选采集；不新增交付状态，不自动签发审计 |
| 关联实施计划 | [全维度对抗测试计划：Batch 0](../../../implementation-plans/esp32/2026-10-05-comprehensive-adversarial-red-green-testing-plan.md) |
| 关联设计规范 | [虚实一致性规范 §12](../../design/07-platform-governance/04-simulation-consistency.md)、[ESP-IDF 分类规范](../../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md) |
| 依据 | ADR-0090 只读门禁、ADR-0091 配置正交与现行防假绿规范 |

## 1. 各项检查能证明什么

| 检查 | 预期结果 | 证据边界 |
|---|---|---|
| 正常业务基线 | 完整通过 | 所选业务场景在指定产物上通过 |
| 断言器自检（A） | 指定业务断言失败 | Matcher 活性；不能证明固件因果或业务实现缺陷敏感性 |
| 环境敏感性（B/C） | 由用例契约定义 | 外部激励或平台故障影响了观察值；不能替代实现变异 |
| 固件依赖检查 | 停用目标固件行为后指定断言失败 | 业务断言依赖固件，而非夹具缓存 |
| 业务实现变异 | 指定有效变异被目标断言检出 | 对该项实现缺陷敏感 |
| 故障处理 | 完整通过 | 故障实际生效，固件产生预期错误/降级行为 |
| 恢复基线 | 完整通过 | 解除扰动后正常业务恢复 |

默认 `assertion` 流水线自动采集正常基线、A 类自检、恢复基线。Batch 1 增加显式选择的 `uart-causality` 专项，见 §5。完整交付仍须执行其他适用检查并接受独立审计。无业务断言、无支持的变异算子或配置不唯一时拒绝采集，不降级为电源轨检查或换一种检查冒充原检查。

## 2. 结构化报告的接受条件

`report_contract.validate_scenario_report()` 读取原始场景和原始 `run-report.json`。报告必须恰好包含一个场景，`name`、`templateId` 与所选输入一致；根汇总、`ok`、`status`、全部五项步骤计数必须一致。步骤索引必须唯一、有序，类型与输入匹配；已执行业务断言须携带与输入 Matcher 一致的 `expected` 和存在且非 null 的 `actual`。JSON 布尔值不能借用 Python 的 `False == 0` 规则绕过绑定。

正向报告要求所有步骤通过。自检失败仅接受目标断言之前的步骤通过、目标业务断言失败；其余步骤只能显式跳过，或由 fail-fast 报告省略未执行后缀。原生报告还会保留完整的计划尾部并标为 `pending`：仅在场景显式 `failurePolicy: fail-fast`、指定断言已失败、完整尾部全部 pending 且不携带 actual/expected 时接受这种映射；这些步骤既不算已执行，也不计入 skippedSteps。目标及之前的 pending、混合或不完整尾部、矛盾计数均拒绝。失败发生在注入步骤、加载器、未知信号、其他断言或基础设施时拒绝。结构化 error 诊断也阻断接受；控制台关键字仅可用于拒绝基础设施故障，不能作为击杀凭据。

`CanaryMutator.verify_kill()` 还要求进程退出码为 `1`、本轮报告路径及哈希匹配、变异输入路径及哈希匹配，并核对目标步骤、类型和观察目标。退出 `0` 表示变异存活；超时等其他非零码不能作为击杀。随机场景身份防止把旧场景报告拼入当前自检。

该判定检查 Runner 的结构化执行结果，不另行实现完整 Matcher 引擎，也不能凭文件哈希证明固件业务因果。独立的固件依赖和实现变异检查仍是必要条件。

## 3. 候选采集与隔离

入口为 `.governance/tools/run_loop.py`，可以显式选择已登记应用进行复验：

```powershell
python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/tools/run_loop.py `
  --app uart_echo --config-id wasm_sim_standard --workspace-root .
```

每轮创建 `.governance/runs/<UTC时间>-<UUID>/`，包含应用输入副本、登记条目快照、`gate1.log`、副本内的 `unisim-assets/`、各阶段原始报告与日志、变异文件，以及 `candidate_evidence.json`。变异文件采用短 UUID 文件名以限制 Windows 路径长度。当前 Wink CLI 将 `--out` 目录用作引擎的应用入口，会丢失独立资产目录对应的应用身份；本流程保留应用副本的标准资产布局，不使用该参数。

候选格式为 `format_version: 1`、`kind: candidate_evidence`。顶层记录应用、配置、登记后端/芯片/profile、实际 `runner_mode: headless`、Runner 脚本哈希、原始/副本输入哈希、限制说明及 `checks`。每项检查记录独立 `run_id`、配置、输入哈希、命令、退出码、场景/报告路径及哈希、实际 Wasm/JS/设备树三件套复合哈希和接受结论。设备树 `mcu` 必须与选定芯片一致。

状态 `collecting`、`failed`、`candidate_ready` 仅属于候选格式，**不是** `delivery_state` 新枚举。成功要求基线通过、自检命中目标失败、恢复通过，三次产物相同且输入保持稳定；自检无论成功或失败，都尝试恢复。失败保留候选和日志，不执行 Git 回退。`--dry-run` 不执行构建、模型生成或写文件。

候选流程不调用 `-WriteEvidence`，不修改正式登记、审计、历史报告、看板或 Git。`.governance/runs/` 是专用候选命名空间，门禁扫描正式载体/场景时排除这里，同时拒绝把这里登记为正式载体；其他目录仍正常检查。候选目录被 Git 忽略，正式报告目录不受此排除影响。

隔离只覆盖应用副本、场景、报告和资产。运行时源码与工具链构建缓存仍共享；本批拒绝 `--auto-heal` 对共享源码进行自动修复，也不宣称已具备运行时工作区隔离、并发构建认证或故障恢复状态的进程内复用验证。CLI 的本机运行锁/使用记录仍按工具原有机制维护。

## 4. 正式双实证辅助格式

文件位置固定为 `.governance/reports/<target_app_dir>/<config_id>/twin-proof.json`。这是独立审计后才能交付的辅助格式；本批没有生成或晋升此文件。它保留登记数据中 `scenario_sha256` 对单个正常场景的原有含义。

```json
{
  "format_version": 1,
  "kind": "twin_proof",
  "app_id": "esp.example",
  "config_id": "wasm_sim_standard",
  "backend": "wasm_browser",
  "target_soc": "esp32",
  "profile": "standard",
  "baseline_run_id": "baseline-uuid",
  "assets_sha256": "<三件套复合哈希>",
  "positive": {
    "run_id": "baseline-uuid",
    "config_id": "wasm_sim_standard",
    "backend": "wasm_browser",
    "target_soc": "esp32",
    "profile": "standard",
    "assets_sha256": "<同一三件套复合哈希>",
    "scenario_ref": "peripherals/example/unisim-scenarios/example.scenario.json",
    "scenario_sha256": "<场景原始字节哈希>",
    "report_ref": ".governance/reports/peripherals/example/run-report.json",
    "report_sha256": "<报告原始字节哈希>"
  },
  "negative": [
    {
      "run_id": "fault-uuid",
      "config_id": "wasm_sim_standard",
      "backend": "wasm_browser",
      "target_soc": "esp32",
      "profile": "standard",
      "assets_sha256": "<同一三件套复合哈希>",
      "scenario_ref": "peripherals/example/unisim-scenarios/example.fail.scenario.json",
      "scenario_sha256": "<故障场景原始字节哈希>",
      "report_ref": ".governance/reports/peripherals/example/wasm_sim_standard/fault-report.json",
      "report_sha256": "<故障报告原始字节哈希>",
      "case_index": 0,
      "contract_sha256": "<negative_cases[0] 的规范化 JSON 哈希>",
      "stimulus_step_index": 0,
      "assertion_step_index": 1
    }
  ]
}
```

引用均相对 `esp_idfv61/`，解析后须位于该应用的 `unisim-scenarios/` 与正式报告目录中。正常记录必须与登记的场景、报告、基线 run 和资产一致。每项故障记录必须覆盖对应 `negative_cases` 的三要素契约，携带同一配置/产物身份、独立 run/报告，且场景完整通过。复制同一报告内容或复用正常报告不能充当独立故障证据。

当前可自动识别的激励是 `INJECT_PLATFORM_FAULT`、带显式 `fault.action` 的注入，以及 HTTP 4xx/5xx 注入路由。普通回显输入不作为故障。故障激励须先于业务错误断言；声明的错误符号须同时出现在 Matcher 和实际业务观察中，且不能借相似符号的前缀匹配通过。数值错误码、复合状态降级或其他注入类型尚未定义自动映射，保守保持 Red 待验收。

Renderer 先调用现有 `verify_evidence()`，再对**同一个配置**调用 `verify_twin_evidence()`。还须已有独立审计覆盖，禁止 `loop_sop_daemon` 自签。`.fail` 文件存在、候选包通过或另一个配置的报告均不能产生 `TWIN-PROOF`。

辅助格式核对报告与采集上下文的绑定声明，不能鉴别人为伪造的采集记录，也不能自动证明故障和固件输出之间的完整因果。审计必须从本轮原始产物与采集记录核实这些事实；徽标只表达该配置已交付正常/故障处理报告，不代表硬件或完整实现变异验收。

## 5. Batch 1 UART 因果专项

```powershell
python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/tools/run_loop.py `
  --app uart_echo --config-id wasm_sim_standard --proof-profile uart-causality
```

此专项只接受已登记 ESP32 UART Echo、原厂源码哈希与固定的 UART1 正常验收场景，不执行模型生成或自动自愈。六个阶段各起独立 CLI/引擎进程：正常基线、自检、固件依赖、业务变异、接收边界探测、恢复。

- 固件依赖：仅在副本中去掉 Echo TX 调用，保留初始化、读取与调度；原输入与预期不变，TX 断言必须失败且实际候选载荷为空。
- 业务变异：仅在副本中对首字节异或 `1`；原 TX 断言失败，实际载荷必须恰为 `IELLO_ESP32_WINK`。这两种检查还要求固件 Wasm 哈希确实改变、实际 `matchedCount` 为整数 0。
- 接收边界：四个不同的 96 字节报文，每隔 300 ms 注入，逐报文检查 150 ms 观察窗口的完整 TX；它是正常连续接收的边界回归，不冒充故障处理。
- 恢复：`finally` 原样恢复源文件副本并重建、冷启动，所有正常断言重新通过，资产复合哈希回到本轮基准。

每阶段保存执行时的源文件、源差异、场景、报告、日志以及资产副本，分别绑定输入、源码、固件、场景与报告哈希。业务变异阶段的源哈希可与上游不同，**只存在于候选目录**；原厂应用与 Manifest 上游哈希不变。分开保存固件是必要条件，不能用末轮恢复固件覆盖之前变异记录的资产路径。

`causality_complete` 仅表示五项因果/恢复检查成立。`candidate_incomplete` 和 `fault_handling.status: contract_gap` 均属于候选格式，不是交付状态。当前 Echo 明确关闭事件队列，运行期也不处理读写错误；既有“溢出返回 ESP_ERR_NO_MEM”与“换有效字符串”场景不足以证明异常处理。因此六个阶段即使全部接受，入口仍返回非零和 `UART_CONTRACT`，不会亮双实证。

Batch 1 边界探测定位的修复仅发生于内仓 Wasm PAL：已交付回调的数据不再重复留存于 polling FIFO；无回调时仍使用真实 FIFO 和既有满缓冲拒绝行为。门面仍固定使用 512 字节接收缓冲、尚未完整落实 `rx_buffer_size`。错误传播另由下述 Batch 2 验证；768 字节回调消费及 polling 容量测试不等于其他缺口已闭合，更不代表 UART timing 或 ESP32 HIL 已验收。

## 6. Batch 2 UART Events 故障专项

公开候选入口：

```powershell
python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/tools/run_loop.py `
  --app uart_uart_events --config-id wasm_sim_standard --proof-profile uart-events-fault
```

专项锁定 ESP32 标准配置及原厂 Events 源码 SHA-256 `8d173ac3cd0e1a9fec6dc1bf6de48a865172a43cc5f910635c63e00dbef770de`。先复用正常 CLI 的基准、自检、恢复报告，再冻结生产资产。后续运行器为独立 Node 进程加载同一生产 JS/Wasm，经 `wasm_bridge.h` 的既有 `pal_wasm_push_uart_rx_error(0, flags)` 注入一次性事件：`1` 帧错误、`2` 校验错误、`4` FIFO 溢出；`0` 为完整窗口内不应出现错误日志的对照。

每个检查在一个实例内顺序执行不同内容的正常回显、故障处理、后续回显。UART TX 必须包含原厂任务的对应日志一次，随后新输入完整回显；PAL 探针自身日志不计。每阶段使用 500000 us 的 behavioral 观察预算，无故障对照与日志缺失判定必须观察完整窗口；虚拟调度 tick 最后一次可能越过预算至多 11000 us，实际 TX 仍须在预算内。该预算是 harness 的检查界限，不是线路 timing 精度声明。

`fault.contract.json` / `fault.report.json` 使用独立 `kind` 和版本，`backend: wasm_node_abi_harness` 明确区别于登记配置的正常 `wasm_browser` 验收。报告绑定 run、合同、harness、设备树、JS 与 Wasm 哈希，并保留逐帧 UART 端口、时间和十六进制数据。判定器由原始 TX 重算正常、错误日志及恢复结果，拒绝错误端口/窗口、矛盾状态、缺失步骤、错误注入、运行器异常和资产错配。

业务变异只在应用副本移除三处原厂错误处理分支。变异正常 CLI 场景仍须通过，固件摘要须变化；故障检查只能因步骤 1 的错误日志缺失而失败，前后的正常回显须通过。最终恢复原厂字节并重建，正常 CLI 和全部故障检查再次通过，源码及资产摘要均恢复。各阶段保留源码、差异、场景、报告与资产副本，异常出口也必须尝试恢复。

该候选不注册原生故障场景，不签署审计，不晋升正式双实证。线路波形、波特率、校验检测、固定接收容量、软件缓冲/队列满压力与 HIL 均未覆盖；原厂 parity 配置为 disabled，注入校验错误只证明收到该异常事件时的传播及处理。故障处理候选、断言自检和正常因果证据仍须分别解释。

## 7. Batch 3 TWDT 自动超时与 SDK 生命周期

```powershell
python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/tools/run_loop.py `
  --app task_watchdog --config-id wasm_sim_standard --proof-profile twdt-timeout
python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/tools/run_twdt_sdk.py `
  --candidate <该轮的candidate_evidence.json> --case all
```

原厂任务看门狗示例 SHA-256 固定为 `5855b893bf480e818fb8421b23bcbdd50480c9d21a013725de9df14e4e56dc96`。正常 CLI 的基准、断言自检、恢复完成后冻结生产资产；仅在应用副本分别省略原厂注明的任务、func_a、func_b 喂狗调用。三者的原厂生命周期仍应完成，但自动超时应在订阅后 3000000 us 至 3050000 us 内出现，且每次只报告该轮漏喂者。完整观察 16000000 us，覆盖退订/析构后的一个完整超时周期。首次 14000000 us 观察不足的报告只作诊断，不能通过恢复验收。

`twdt_timeout_contract` / `twdt_timeout_report` 使用独立 kind/version/backend，绑定合同、harness、生产 JS/Wasm、设备树、run、隔离源码和对应 patch。判定器由带时间的 UART/log 输出重算期限、漏喂者、完整生命周期及清理后静默，拒绝早/晚报警、错人、重复/缺失步骤、bool 冒充整数、复位、运行异常及资产错配。RED 复核只接受漏喂版在完整窗口无自动报警且生命周期仍完成；不能把任意非零退出当作 RED。还原原厂字节后重建，生产资产摘要须与基准相同。当前原生生命周期场景并不要求报警，三份漏喂固件仍可能通过该场景；必须另读本专项的原始输出。

TWDT 门面复用现有 FreeRTOS generation-tagged work item 自动检测，按 ESP-IDF 共同计时周期维护 has_reset；全部订阅者喂狗后才刷新期限。诊断 API 只报告本周期未喂者，返回 CPU 位图，不执行 panic。自动超时调用可覆盖的 ISR 用户钩子，非 panic 保持执行，panic 发出待处理复位。公开 Wasm 复位原因使用 `PAL_OS_RESET_REASON_WATCHDOG = 2`，执行复位后的 SDK `esp_reset_reason()` 使用 `ESP_RST_TASK_WDT = 6`，两个枚举域不能混用。旧 work token 在复位/退订时失效；用户句柄为受校验的 opaque token，序列不随复位清零，防止池槽复用后旧句柄恢复有效。序列耗尽返回资源错误，不循环复用。

SDK 适配器独立复制到 `.governance/runs/sdk-<UUID>/`，只调用公开 SDK/PAL API、公开 ISR 钩子与 Wasm ABI。三个独立实例验证共同期限及重新喂狗、panic 原因与重新初始化、删除/复位后句柄拒绝；适配器源码、harness、构建就绪报告及各实例报告分别绑定。本适配器只发就绪消息，避免 100 Hz SDK 下 `pdMS_TO_TICKS(1) == 0` 造成零 tick 循环。适配器证据不冒充原厂源码输出，不写正式注册表、历史报告或看板。

边界：当前合作式仿真只建模 CPU 0 上任务/用户漏喂，idle_core_mask 非零明确告警；空闲核饥饿、永久不让出 CPU 的忙循环、真实 ISR 延迟/回溯和 HIL 未覆盖。定时器资源仍为现有有限池/队列，首次调度失败返回资源错误，自动重装失败明确记录日志；本轮不证明饱和资源条件下的硬件独立监控。`automatic_timeout_complete` 仅表示本专项五份超时/恢复报告成立，SDK 补充证据、原生故障合同、完整固件依赖与独立审计仍须分开评估，不能据此晋升完整 TWDT 能力或正式双实证。
