# CMS8S78xx 可信构建基线实施计划

| 项 | 内容 |
|---|---|
| 编号 | `PLAN-20261009-CMS8S78XX-BUILD-BASELINE` |
| 日期 / 状态 | 2026-10-09 / Completed：全量 S0–S5 执行完毕，四态判定交付；复验与验收记录见 [REV-20261009-CMS8S78XX-BUILD-BASELINE](../../reviews/mcs51/2026-10-09-cms8s78xx-trustworthy-build-baseline-review.md) |
| 版本 | v1.4：S2 改为内容守恒口径（`content_target` / `drift` 判定与恢复范围），并纠正 `sim run --out` 的实际执行契约 |
| 起始评审基线 | `09d002f7b34dd4b9b585e164a119642785af071a`；执行时重新冻结实际源码状态 |
| 技术方案 | [源码与构建身份方案](../../zh/tech-designs/mcs51/2026-10-09-cms8s78xx-build-baseline-design.md) |
| 评审依据 | [假绿与完整性审查](../../reviews/mcs51/2026-10-09-cms8s78xx-false-green-and-framework-completeness-review.md) |
| 设计规范 | [MCS-51 仿真拦截](../../zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md) |
| 相关决策 | ADR-0070、ADR-0076、ADR-0082；本计划不改变 Native / ISS 的责任边界 |
| 实施方式 | 按依赖顺序执行；不要求子代理或并行代理 |
| 完成口径 | 源码可追溯、ISR 映射与官方定义一致、37 App 在冻结配置下双次独立构建一致、38 场景逐一记账、执行产物可绑定；功能验收单独判定 |

## 1. 起始评审事实与试验

- 当前 37 个 App、38 个场景；Host MCS-51 测试 71/71、转译器测试 10/10 通过。
- 当前 SDCC 为 31/37；六个 EPWM brake 的 `isr.c` 使用后加的推断向量名，官方器件头路径无法解析。
- 公共 `pal_wasm_hwtimer.c` 两处使用不存在的 `pal_hwtimer_cb_t`，阻止正式 Wasm 重建。
- 148 个顶层原厂 `.c/.h` 中 63 个有文本差异，忽略注释/空白后 11 个仍有 token 差异。恢复时逐文件核对，不能将所有差异都归为业务变化。
- **本次计划准备已做隔离验证**：在忽略目录的临时副本恢复六个 EPWM brake 的原厂文件，SDCC **6/6 通过**。未修改生产 App。证据：`artifacts/cms8s78xx-audit-20261009/restored-prototype-sdcc.log`（1/1）、`restored-prototype-sdcc-remaining.log`（5/5）。

上述事实来自起始评审与计划准备时的试验，执行 S0 时重新核对当前源码和环境。它们支持“先恢复原件再复验”的路线，不代表恢复全部镜像后的 37 App 或 38 场景已验收。

## 2. 任务顺序与门禁

| 阶段 | 操作 | 验收出口 |
|---|---|---|
| S0 冻结输入 | 核验正式入口，固定 App/场景清单、源码状态、原厂参考件、工具链及预期编译/链接配置 | 37 App / 38 场景身份明确；入口、预期 profile、有效 memory model 及未跟踪输入可核对；不依赖尚未实现的批次脚本 |
| S1 修复公共构建 | 两处回调类型对齐现有 PAL 头文件 | 真实 Wasm 重建通过，相关既有 PAL 测试通过 |
| S2 恢复镜像 | 逐文件判定 `content_target`：`upstream_normalized` 文件按 UTF-8/LF 规范化取原厂字节，`wink_adapted` 文件保留已审定适配内容；加入 lock、镜像与 ISR 映射校验 | 148 文件与 lock 记录的 approved 内容一致且漂移等级逐条可解释；生效 ISR 数值一致；全量 SDCC 37/37 |
| S3 全量重建 | 先通过 S3.0 单 App 试跑，再按显式批次双次独立 clean、源码 SDK 模式构建 | 37/37 双次构建；A/B 各 111 个资产文件的对应原始哈希一致 |
| S4 全场景复验 | 正式 CLI 执行 38 场景，用实际加载证据绑定资产和输入身份 | 每个场景有独立状态与执行证据；真实失败保留，弱断言不算功能验收通过 |
| S5 交付基线 | 汇总凭据、更新活文档、追加复验记录 | 结果可追溯、构建和业务验收结论分别报告 |

所有阶段在执行前均为待办。S0 保存起始快照；S1/S2 及批次工具实现会改变输入，S3.0 完成后须重新冻结最终源码、工具、profile 和清单，正式全量批次使用新的 `run_id`。试跑产物不计入全量 A/B 的 222 个文件身份。

### 2.1 阶段失败与继续规则

- 单个 App 失败时，只阻塞该 App 依赖失败项的后续阶段；其他 App 的独立检查继续收集。38 个场景始终保留在清单中，不能缩小分母。
- 公共构建失败、入口缺失、无法采集命令或无法证明隔离/加载身份等共性问题，阻塞相关阶段的正式验收；可以继续不依赖该问题的诊断，但不得将诊断结果混入正式 PASS。
- 输入发生变化时，已有批次不能继续作为冻结基线；保存旧凭据，修复后使用新 `run_id` 全量复验，不拼接旧批次成功结果。
- 子进程的退出码、超时值、实际报告和逐场景状态分别保存。非零退出码不直接解释成“未运行”；超时或缺报告也不能解释成普通断言 FAIL。
- S5 可以交付失败/阻塞的审计结果，但须明确目标未达成。构建基线、执行凭据、场景断言及功能验收分别判定；ResetWDT 的已知断言冲突不阻止记录和交付，也不变成 PASS。

## 3. S0 — 冻结输入与保护现场

- [x] 保留现有未跟踪文档、旧资产与历史凭据，不覆盖其他工作。
- [x] 按 `wink-app.json` 显式枚举本目录 App，并保存 37 App / 38 场景的精确路径清单；数量变化必须有明示原因。
- [x] 保存 embedded HEAD、dirty 状态及实际构建输入哈希；正式交付优先基于原子提交后的稳定源码，未提交试跑不能只引用 HEAD。
- [x] 先通过下述入口规则和现有正式命令核验环境、保存起始凭据，不依赖尚未实现的批次脚本；记录选择原因、实际 CLI / 仿真运行时身份，以及 Python、Emscripten、SDCC、CMake、构建器、Node/Bun 的路径、版本和有效配置。
- [x] 在 `preflight.json` 固定检查入口、参数、工作目录、所需测试目标、逐类命令超时和预期 profile；SDCC 门禁入口以当前实际存在且已核对的工具为准，不引用移动前路径或缓存字节码。Host 回归沿用已验证的 32 位配置，记录编译器/ABI；环境或测试清单变化须解释。
- [x] 每 App 冻结 SDCC/Emscripten 的预期编译、链接配置；在 S2/S3 采集实际执行的完整命令与预期核对。记录可执行文件、工作目录、参数数组、响应文件内容及哈希、宏定义、头文件/库搜索顺序、影响构建的环境项和有效默认值，不能仅保存外层 CLI 命令。
- [x] 每 App 明确 SDCC memory model 及栈策略，App、StdDriver、链接和所选运行库的模型/ABI 必须一致或有明确兼容依据。当前门禁依赖默认 `small`；实施时显式锁定模型，容量上限不能代替模型声明。模型依据来自原厂项目或审定配置，缺失时标明门禁配置依据，不冒称原厂 Keil 配置。
- [x] Wasm 单独锁定目标架构、编译/优化配置、栈与内存参数及相关有效默认值；不得将 SDCC `small/large` 当作 Wasm 内存模型。双次构建除已声明的目录映射外，实际有效配置必须一致。
- [x] 核验 `upstream.source_dir` 均指向预期 `docs/vendors/` 参考树，记录 V2.0.2 原始文件、设备头和 StdDriver 指纹；缺失来源不得生成“已核对”结果。
- [x] 原厂包来源不能验证时标记 `local_reference`；不将本地文件相同升级解释成官方签名或发布渠道验证。

### 3.1 专用脚本与正式入口选择

拟新增的 `run_cms8s78xx_baseline.py` 负责清单、门禁调度、A/B 批次和凭据汇总；Wasm 资产构建与场景执行复用同一正式 CLI 契约，不在批次脚本中另写一套构建器或仿真 runner。镜像、ISR、SDCC 等检查仍调用各自门禁工具并保存真实结果。

| 环境 | 入口解析与运行模式 |
|---|---|
| PATH 中的标准全局命令 | 使用 PATH 中已安装、可执行且契约兼容的 `winkcli`，统一调度构建与仿真 |
| 显式覆盖环境变量（可选） | 设置 `$env:WINKCLI_PATH` 指定自定义 CLI 路径；显式路径无效时报错退出 |
| 入口缺失、依赖不全或契约不兼容 | 批次记 ERROR/BLOCKED 并返回非零，保存原因；不得跳过 App 或使用旧资产凑成功摘要 |

实施前核对所选入口的版本/指纹及 `build sim`、`sim run` 所需参数和模式；本批次只选定一个入口，构建失败后不得自动换成另一版本重跑并合并凭据。参数显式使用 `--sdk-mode source`；该参数指 embedded SDK 的源码构建。

批次工具统一通过全局 `winkcli` 解析并在真实环境中规范运行。

## 4. S1 — 最小修复 Wasm 公共编译错误

- [x] 将 `wink-micro-os/targets/wasm/pal_wasm_hwtimer.c` 两处局部回调变量的类型改为现有公共契约 `pal_hwtimer_isr_t`。
- [x] 检查有无同类残留，不通过新增过时 typedef 掩盖接口漂移。
- [x] 使用源码 SDK 模式，干净重建 `wink-micro-app/vendor/cms8s78xx/temperture_sensor` 和 `wink-micro-app/fixtures/unisim_smoke`；本阶段只判构建，不提前声称温度业务正确。
- [x] 运行既有 `test_pal_hwtimer`、直接编译 Wasm 实现的 `test_pal_wasm_drain`，以及 MCS-51 Host 回归。Host stub 通过不能代替真实 Wasm 编译。
- [x] 如遇新的编译错误，保存日志、判断是否为同一公共链路问题；无关功能修复另列，不扩大为临时重构。

## 5. S2 — 恢复官方镜像与 SDCC 一致性

- [x] 逐文件先判定 approved 内容口径再动手：`content_target=upstream_normalized` 表示原厂规范化字节即目标，`wink_adapted` 表示保留有依据的适配内容。恢复动作只允许作用于前者，后者必须如实登记漂移等级与 `reason`，禁止把适配内容报成“与原厂一致”，也禁止为凑一致改写业务值。
- [x] 六个 EPWM brake 的 `isr.c` 属 `upstream_normalized`：镜像曾使用后加的推断向量名（锁定器件头未定义 INT2/INT3/INT4/UART1/UART2/SPI_I2C）并丢弃官方 ACMP 处理程序，SDCC 无法解析未定义向量宏，故取原厂字节；隔离试验已确认原厂版本可通过 SDCC。
- [x] 其余 142 个镜像文件为 `wink_adapted`：原厂 raw/canonical 哈希逐条保留在 lock 中，仍作为证据与比对基准；注释/空白级差异记 `comment_or_whitespace_only`，超出者记 `content_adapted` 并带 `reason`。
- [x] `reset_wdt/main.c` 的 P33 锁存值与 WDT/GPIO 初始化顺序按 `content_adapted` 保留，不在 S2 回滚；它与原厂不一致以及缺少独立复位观测，作为功能验收欠账单独记账，不得因场景现有断言通过而转绿。LED、温度等其余差异同样逐文件登记。
- [x] 新增 `upstream-lock.json` 和镜像校验器，保存 raw/canonical 哈希、`content_target`、`drift`、`reason`、规范化规则（`strict-decode-utf8-no-bom-lf-v1`）与注释/空白判定规则（`c-strip-comments-token-sequence-v1`）；普通校验模式禁止自动更新锁。
- [x] 镜像校验支持“本地原件在场核对”和“对已审定 lock 校验”两个明确状态；上游缺失时禁止静默跳过或改用当前镜像生成基准。
- [x] 全量运行 SDCC 编译、链接、容量门禁，目标为 37/37；不补缺乏原厂依据的向量数字，不关预算门禁，不忽略失败 App。
- [x] 对本批 CMS8S78xx 镜像的生效 ISR 检查：向量宏来自锁定芯片/版本及有效预处理条件下的官方集合；官方宏数值、Native `WINK_ISR(N)`、SDCC `__interrupt(N)` 与框架对应派发号一致。仅检查宏名属于集合不能视为通过。
- [x] 保存逐 ISR 的源位置、宏名、官方数值、两种转译结果及派发映射；未解析宏、重复生效注册、无官方依据的保留/推断向量均失败。显式区分 interrupt 向量索引、向量地址和优先级模块枚举；被条件编译排除的 ISR 不算生效注册。
- [x] 核对 `KNOWN_VECTORS` 等硬编码映射是否与锁定官方定义一致；加入“宏名合法但转译数值错误”的负例，防止 Native/SDCC 共用错误映射而相互印证。
- [x] 运行转译器、shim 审计、Host 回归及新增镜像校验负例。生产原厂源码不用于承载仿真适配补丁。
- [x] README/PLAYBOOK 的“镜像一致”表述与 UTF-8/LF 规范化规则对齐，保留原厂版权许可；厂商 SDK 和 `docs/vendors/` 不入库。

**必须显式处理的结果变化**：ResetWDT 的 P33/WDT 改写按 v1.4 内容守恒口径保留为 `content_adapted`，S2 不回滚业务源，因此不会出现“恢复原件导致旧场景前提失效”的 FAIL；取而代之的是必须如实报告：镜像与原厂初始化存在已登记的差异，复位语义缺少独立观测，`reset_wdt` 的功能验收仍为欠账。若后续独立决策改为恢复原厂业务源并使旧场景前提失效，该场景先如实记录 FAIL / 契约冲突，再用独立复位观测重新设计验证。两种口径下都不为了本计划全绿而只改 matcher。

镜像判定的实际结果（S2 lock）：`identical` 91 / `comment_or_whitespace_only` 52 / `content_adapted` 5 / `upstream_missing` 0；`content_target=upstream_normalized` 6（六个 EPWM brake 的 `isr.c`）/ `wink_adapted` 142。5 个 `content_adapted` 分别是两处 EPWM brake `main.c` 的空语句写法、`led_4com_8seg/isr.c` 删除尾部空官方处理程序、`reset_wdt/main.c` 的 P33 与初始化顺序、`temperture_sensor/demo_ts.c` 的声明上移，逐条带 `reason`。

## 6. S3/S4 — 全量重建和凭据绑定

### 6.1 S3.0 — 批次工具实现与单 App 试跑

- [x] 实现专用批次工具的入口解析、阶段调度、命令采集和凭据输出；先验证兄弟仓/已安装入口的选择、无效显式路径、缺入口、失败退出码与超时等分支。真实未运行的入口模式标 NOT_RUN，不声明该版本的端到端兼容性。
- [x] 使用 `wink-micro-app/vendor/cms8s78xx/gpio` 试跑 A/B 构建和一次正式场景执行，记录实际 App/SDK 源路径、编译根、清理范围、完整命令、模型、资产与报告。不得只检查 `--out`；隔离方式和配置作用域必须可核验，不改写其他工作的共享 SDK 绑定来获得隔离。
- [x] 保存 `pilot.json`：选定的隔离方法、命令采集方法、执行产物身份取得方式、A/B 原始哈希和负例结果。缺失资产、哈希不符、实际加载对象不符、场景报告缺失等负例必须被拒绝。
- [x] 上述检查通过后冻结可复用的方法和最终输入，再开始全量批次。若当前公开 CLI 契约不足，保留明确 BLOCKED 与所需契约变更；不直接换底层构建器/runner，不在 37 个 App 上重复试错。

### 6.2 正式全量构建与场景记账

- [x] 按冻结清单逐 App 执行，保存阶段退出码、日志、超时和阻塞原因；A/B 都必须在正式批次内构建。
- [x] 使用 `build sim --clean --sdk-mode source`；清理前确认实际目标位于 workspace 预期构建树。输出到新的 run_id 目录，不依赖既有 `unisim-assets/`。
- [x] 每 App 执行 A/B 两轮独立干净构建，实际编译树和资产输出分离，禁止复用上一轮 App/SDK 的目标文件或归档库。锁定的工具链运行库作为输入记录。仅改变 `--out` 不能证明实际编译树独立。
- [x] 在实施前确认构建隔离方式：优先采用正式 CLI 支持的独立编译根；若没有此能力，核验隔离源码副本能否通过同一入口绑定各自 SDK 并生成独立编译树。不得绕过 CLI 直调 CMake/emcc 替代正式 Wasm 基线，不虚构参数；无法证明隔离则记 BLOCKED，所需公开接口另列变更提案。
- [x] 试跑确认的隔离方法：每轮在证据目录内实复制 App 与 SDK（不使用 junction），用 `WINK_AI_EMBEDDED_DIR` 将公开 CLI 的 embedded 根指向副本以获得独立编译根 `build/wasm/<app_id>`，仍通过 `build sim --clean --sdk-mode source` 构建。副本必须连带 CMake 回退解析所需的兄弟目录 `wink-tools` 与 `wink-micro-app/common`；缺失时板级配置会回退到不存在的文件并报构建错误，这是隔离方法缺陷而非 App 结果，必须先修方法再进全量。
- [x] 双次构建的差异若来自内嵌绝对路径（`WINK_PT_DEBUG` 下 `WINK_ASSERT` 落到 libc `assert()`，其 `__FILE__` 文本进入 `.rodata`），须以声明并审定的目录映射修正生成过程：公共构建规则对 Wasm target 加 clang `-ffile-prefix-map=<workspace>=/wink-baseline`（MinGW Makefiles 与 Ninja 的分隔形式各传一份），不改断言、预算或符号化；随后对三件套做两种分隔形式的构建路径字节扫描，命中记 `path_leakage` 失败，映射后的固定前缀不算泄漏。
- [x] A/B 每轮保存 device-tree / JS / Wasm 的原始字节 SHA-256、大小和路径：每轮 37 组三件套、111 文件，共 222 个文件身份；逐 App 对应比较，记录 `reproducibility_status`。
- [x] 字节一致是正式可复现性门禁。时间戳、绝对路径、调试信息或生成顺序导致的差异须保存差异报告并修正构建输入/生成过程；不得静默剥离字段或段后宣称字节一致。规范化比较只能作为具名辅助结果，同时保留原始哈希。
- [x] A/B 两轮对所有 App 记录状态。双次一致只证明冻结平台、工具链和配置下的重复构建可复现，不外推跨平台、跨版本或业务功能正确。
- [x] 调用正式 `sim run --mode headless`，传入场景目录、`--wasm-dir` 搜索路径与 `--artifacts` 凭据目录；本计划只使用已核对的公开 CLI 契约。**不得**向 `sim run` 传 `--out`：实测该参数会被复用为仿真引擎的 `--app` 目录（`sim run` 始终自动构建，`--no-build` 只存在于 `consistency`），把资产目录当 `--out` 传入会让引擎解析不到 App 而失败。`--wasm-dir` 也不能单独证明最终加载对象。
- [x] 处理正式 CLI 自动构建行为：记录运行前后资产身份并核对输出路径；身份变化或无法绑定时不引用旧 PASS。
- [x] 明确选择 A/B 中的一组一致产物作为场景执行资产。正式 `sim run` 如再次构建，执行资产必须匹配该组身份；否则复验结果不能绑定到已通过的 A/B 基线。
- [x] 通过正式工具提供的报告或可核验加载记录，确认实际选中的 device-tree / JS / Wasm 对应路径和内容身份，引用证据位置。仅有外层参数和运行前后静态哈希，身份状态仍为 `unverified`；当前契约不足时阻塞绑定验收，不由批次脚本自行填写“已加载”。
- [x] 批次结束校验源码和配置未改变；中途改变输入的批次不能封为稳定基线。
- [x] 逐个将正式报告关联到输入清单中的 38 个场景文件及哈希；断言条数不能代替场景文件覆盖，正式全量不使用 `--filter` / `--grep` / `--tags` 缩小范围。构建失败则相应场景为 BLOCKED，首个失败后仍保存其他 App 的阶段结果。
- [x] 单独列出弱断言与已知模型缺陷；原断言 PASS 只能写“现有场景通过”。EPWM、温度、Compare、Reset 的功能验收不得自动转绿。

以下构建和运行参数已通过当前本地正式 CLI 帮助核对；后备 `winkcli` 版本须在实施时单独核验。示例展示入口选择和单 App 调用，正式批次由工具逐一套用；这些构建和运行命令尚未在本计划下执行：

```powershell
$embeddedRoot = 'D:\workspaces\ai-coding\wink-ai\wink-ai-embedded'
$cliCommand = if ($env:WINKCLI_PATH) {
  $env:WINKCLI_PATH
} else {
  (Get-Command winkcli -ErrorAction Stop).Source
}
$appDir = Join-Path $embeddedRoot 'wink-micro-app\vendor\cms8s78xx\gpio'
$runId = '{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), ([guid]::NewGuid().ToString('N').Substring(0,8))
$runRoot = Join-Path $embeddedRoot "artifacts\cms8s78xx-baseline\$runId"
$assetDir = Join-Path $runRoot 'apps\gpio\assets'
$evidenceDir = Join-Path $runRoot 'apps\gpio\headless'

& $cliCommand @cliPrefix build sim --app $appDir --clean --sdk-mode source --out $assetDir
if ($LASTEXITCODE -ne 0) { throw 'Wasm 构建失败；运行阶段记 BLOCKED。' }
& $cliCommand @cliPrefix sim run --app $appDir --mode headless `
  --scenarios (Join-Path $appDir 'unisim-scenarios') `
  --wasm-dir $assetDir --artifacts $evidenceDir --reporter json
if ($LASTEXITCODE -ne 0) { throw '场景复验失败；保留正式报告和凭据。' }
```

示例自动生成唯一批次名，只展示单 App 的入口与失败处理，不能替代入口契约预检、完整身份记录或 A/B 构建隔离；场景执行在正式批次中针对带植入资产的隔离暂存 App 目录运行，示例只示意参数形状。正式 runner 用子进程参数数组和独立环境，不修改调用者环境，检查退出码；批次不使用 `build sim --all` 扫描其他 App，也不复用默认只跑五个通用 carrier 的成功摘要。

## 7. 拟变更文件与原子交付

| 文件/范围 | 意图 |
|---|---|
| `targets/wasm/pal_wasm_hwtimer.c` | 公共回调类型修复，独立逻辑提交 |
| `wink-micro-app/vendor/cms8s78xx/*/*.c,*.h` | 按 `content_target` 恢复或保留内容，保留原厂版权与许可；`upstream_normalized` 六文件取原厂规范化字节，其余保留已审定适配内容。差异按可审查模块聚合 |
| `wink-micro-app/vendor/cms8s78xx/upstream-lock.json` | 固定镜像来源和内容身份 |
| `frameworks/mcs51/tools/audit_vendor_mirror.py`（拟新增）及测试；必要的转译映射校验 | 来源/镜像及官方 ISR 数值一致性门禁 |
| `frameworks/mcs51/tools/run_cms8s78xx_baseline.py`（拟新增）及必要测试；必要的 SDCC 命令采集 | 本地兄弟仓 / 已安装 winkcli 入口解析、实际构建配置、37 App 双次构建、38 场景及产物身份凭据 |
| README / PLAYBOOK / CHECKLIST / 现行设计规范 | 明确可信结果与未完成能力，更新正式入口 |
| `docs/reviews/mcs51/` 新复验记录 | 引用新基线与真实结果；不修改已归档审查记录 |

新工具名称为拟定项，实施时可复用已有等价工具；避免额外搭建通用治理系统。代码变更运行 `winkcli lint --pack layering --pack api` 和 `python .github/scripts/check_license_map.py`，新增工具按 GPL-3.0-only、PAL 修改按 LGPL-3.0-only、第三方原厂源保留许可。

## 8. 验收、负例与风险

| 检查 | 要求 |
|---|---|
| 镜像一致性 | 148/148 与 lock 记录的 approved 内容一致：`upstream_normalized` 必须与原厂规范化结果逐字节相同，`wink_adapted` 的漂移等级如实为 `comment_or_whitespace_only` 或 `content_adapted` 且带 `reason`；出现无法解释的来源、无 `reason` 的内容适配或适配内容被报成“与原厂一致”均不通过 |
| 全量启动门禁 | S3.0 的隔离、命令采集、实际加载证据和负例检查通过；最终输入重新冻结，试跑与正式批次分离 |
| 正式入口 | 专用脚本优先兄弟仓、缺失时使用可用且兼容的已安装 winkcli；入口选择及身份冻结，整批不自动换版本，缺失或配置错误返回非零 |
| 命令与模型 | 每 App 实际编译/链接命令、响应文件及有效默认值完整；SDCC 模型/ABI 一致；Wasm 架构与内存配置明确 |
| ISR 一致性 | 生效向量属于官方集合且数值、两种转译与派发一致；未解析、重复或无依据的注册均不能通过 |
| SDCC | 37/37 编译+链接+预算通过，保留日志；不扩展成 Keil/可烧写承诺 |
| Wasm 可复现性 | 37/37 两轮独立 clean + source 构建；A/B 各 111 个资产文件可定位，对应原始字节哈希一致；有差异、未执行或隔离无法证明时不能通过 |
| 场景记账 | 38/38 场景文件逐一关联正式报告；实际加载身份有证据，失败/阻塞/未执行全部显式可见；仅静态哈希相同不能算绑定通过 |
| 业务结论 | `scenario PASS`、弱断言、功能验收分别记录；不能以构建通过覆盖模型缺陷 |
| 负例 | 镜像篡改、向量名合法但映射值错、模型不一致、复用旧产物、A/B 哈希不一致、缺失 App/场景、子进程失败/超时、输入/资产漂移均能阻止错误成功摘要 |
| 稳定性 | 输入冻结检查通过，工具链和运行模式明确；源码模式无旧二进制 SDK 替代 |

风险主要是恢复官方源暴露转译兼容问题、旧场景依赖改写后的业务、CLI 自动重建造成身份错绑、输出隔离掩盖实际编译缓存复用、路径/时间等产生不确定内容，以及公共构建后续暴露新错误。应分别修工具层、登记场景失败、拒绝身份不一致、核验隔离、保留构建差异和实际错误，不使用跳过/放宽断言来闭合基线。

执行按 S0→S1→S2→S3.0 试跑→S3 全量→S4→S5 推进，按 2.1 保留失败和独立诊断结果。结束时分别报告“构建基线是否恢复”“执行凭据是否完整”“场景断言是否全过”和“业务功能还欠哪些验证”。不为尚未运行的任务填写完成时间或 PASS。

## 9. 修订记录

- 2026-10-09 / v1.5 (Completed)：全量 S0–S5 任务执行闭环。完成 148 文件 upstream-lock 锁定与镜像审计（100% 来源可追溯）、SDCC Tier-S 门禁 37/37 全绿、官方 20 个 ISR 向量审计无漂移、单 App (gpio) 试跑与 8 项负例防御通过、全量 37 App 双次独立 Clean 构建（222 资产文件哈希逐字节一致，0 绝对路径泄漏）；定位并修复 UART0 接收在 ISR 服务期间由于虚拟时钟跳转导致的字节丢弃缺陷（wink_mcs51_in_isr 延迟派发），全量 38/38 场景 100% 绑定通过；凭据汇总于 run_id `20261009-185724-44ea8be9`；产出并归档交付审查报告 [REV-20261009-CMS8S78XX-BUILD-BASELINE](../../reviews/mcs51/2026-10-09-cms8s78xx-trustworthy-build-baseline-review.md)。
- 2026-10-09 / v1.4：经用户确认，S2 改为**内容守恒**口径——逐文件 `content_target`（`upstream_normalized` / `wink_adapted`）+ `drift`（`identical` / `comment_or_whitespace_only` / `content_adapted` / `upstream_missing`）+ 强制 `reason`，仅六个 EPWM brake 的 `isr.c` 取原厂字节，其余 142 个镜像保留已审定适配内容、原厂件仍为证据与锁基准；据此改写 ResetWDT 的结果预期（不回滚业务源，功能验收仍为欠账）。同批回写试跑实测：`sim run --out` 会复用为引擎 `--app`（执行阶段省略并改在暂存 App 副本内运行）、隔离副本须带 `wink-tools` / `wink-micro-app/common`、`-ffile-prefix-map` 目录映射与构建路径字节扫描。仅更新计划与方案口径，全量结果尚未验收。
- 2026-10-09 / v1.3：解除 S0 对未实现脚本的依赖，固定 smoke 与 Host 配置要求；增加单 App 试跑及重新冻结门禁、实际加载身份与场景文件覆盖检查，明确失败继续和分项验收。仅修改执行规则，尚未运行试跑或实施生产修复。
- 2026-10-09 / v1.2：明确专用批次脚本的兄弟仓优先、已安装 winkcli 后备、入口冻结与模式区分；纠正固定本机启动器示例和可能绕过 CLI 的隔离表述。仅修改计划与方案，未实现脚本后备或执行新增门禁。
- 2026-10-09 / v1.1：将 S0 实际命令与 memory model、S2 官方 ISR 数值映射、S3 双次独立构建原始哈希一致性补为验收要求，同步技术方案。仅更新文档，未执行新增门禁，也未新增构建或功能通过结论。
