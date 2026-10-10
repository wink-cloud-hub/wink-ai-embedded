# CMS8S78xx 可信构建基线技术方案

| 项 | 内容 |
|---|---|
| 日期 / 状态 | 2026-10-09 / Proposed，尚未实施 |
| 版本 | v1.4：确立镜像内容守恒判定与恢复范围，并纠正 `sim run` 输出契约的实际语义 |
| 关联实施计划 | [可信构建基线实施计划](../../../implementation-plans/mcs51/2026-10-09-cms8s78xx-trustworthy-build-baseline-plan.md) |
| 触发评审 | [假绿与完整性审查](../../../reviews/mcs51/2026-10-09-cms8s78xx-false-green-and-framework-completeness-review.md) |
| 设计规范 | [MCS-51 仿真拦截](../../design/02-wink-micro-os/07-mcs51-simulation-interception.md) |
| 范围 | 当前 37 个 vendor App 的源码追溯、真实重建与运行凭据；不新增跨仓 ABI |

## 1. 目标和边界

基线应回答四个问题：**构建输入和实际配置是什么、生成了哪些产物、独立重复构建是否得到相同字节、本次场景是否确实运行了这些产物。** 它不以重新得到 38 个绿色结果作为唯一目标；已有断言盲区和模型缺陷仍须独立治理。可追溯、构建可复现和功能正确分别判定。

本方案修公共 Wasm 回调类型错误、按内容守恒口径恢复镜像的官方来源判定，并补镜像/ISR 映射校验、实际构建配置、双次构建及执行凭据。EPWM 波形、温度数值、Timer2 比较输出及复位观测的功能修复另行实施，不将它们夹入构建修复。

## 2. 官方镜像的身份

采用一个版本化的 `wink-micro-app/vendor/cms8s78xx/upstream-lock.json`（`schema_version=cms8s78xx-upstream-lock/v1`），逐条列出 37 个 App 的 148 个顶层 `.c/.h` 及其原厂来源与内容身份。原厂文件通过现有 manifest 的 `upstream.source_dir` 定位，厂商 SDK 留在本地忽略目录，禁止整包提交。

每个镜像文件至少保存：

| 字段 | 含义 |
|---|---|
| `app` / `mirror_path` | 所属 App 及仓库中被校验的文件位置 |
| `upstream_path` / `vendor_version` | 相对来源位置及声明版本 |
| `content_target` | 该文件 approved 内容的口径：`upstream_normalized`（原厂规范化字节即目标）或 `wink_adapted`（保留已审定的适配内容） |
| `upstream_raw_sha256` / `upstream_encoding` | 原厂原始字节 SHA-256 及实际严格解码成功的编码 |
| `upstream_canonical_sha256` / `mirror_canonical_sha256` | 按规范化规则处理后，原厂与镜像各自的 SHA-256，供内容比对 |
| `drift` | 相对原厂规范化内容的判定：`identical`、`comment_or_whitespace_only`、`content_adapted`、`upstream_missing` |
| `reason` | `content_target=upstream_normalized` 的理由，以及 `drift=content_adapted` 的适配说明；缺失即校验失败 |
| `normalization` | 唯一允许的转换规则：严格解码（`utf-8-sig`，其次 `gb18030`，绝不 `errors=replace`）、UTF-8 无 BOM、换行统一 LF；版本 `strict-decode-utf8-no-bom-lf-v1` |
| `token_rule_version` | 注释/空白差异判定规则 `c-strip-comments-token-sequence-v1`：剥离 C 注释后按标识符、数字与单标点符号取序列比较 |
| `origin_assurance` | 有原始包凭据时记录其来源和包哈希；只有本地 SDK 时明确为 `local_reference` |

不通过忽略注释、删除空 ISR、重排声明或改业务值来实现“相同”。编码/换行规范化以外的内容差异必须显式判定，不能默认视为一致。解码不能使用 `errors=replace`。

### 2.1 内容守恒与恢复范围

S2 的目标是**来源可判定**，不是把所有镜像改写成原厂字节。每个文件在 lock 里有一个 `content_target`，判定规则固定为：

- `upstream_normalized`：原厂字节就是 approved 内容，只允许规范化编码与换行；校验时镜像必须与原厂规范化结果 `identical`，任何注释/空白以外的差异都是发现项。`--normalize-mirror` 和 `--restore-upstream` 只作用于这一集合。
- `wink_adapted`：镜像保留有文档依据的适配内容。原厂 raw/canonical 哈希仍逐条记录并保持为证据与比对基准，漂移等级如实写成 `content_adapted`（或注释/空白级），并必须带 `reason`；校验器据此判定，绝不把适配内容报成“与原厂一致”。
- 适配内容不通过修改厂商原件来承载：`--restore-upstream` 永不改写 `wink_adapted` 文件，`--write-lock` 只在全部原厂参考件在场时生成，普通 `--verify` 永不更新锁。

本批实际范围：六个 EPWM brake 的 `isr.c`（镜像曾使用后加、锁定器件头未定义的 INT2/INT3/INT4/UART1/UART2/SPI_I2C 向量名并丢弃官方 ACMP 处理程序，SDCC 无法解析未定义向量宏）取 `upstream_normalized` 并恢复原厂字节；其余 142 个文件为 `wink_adapted`。漂移统计为 `identical` 91 / `comment_or_whitespace_only` 52 / `content_adapted` 5 / `upstream_missing` 0。

`reset_wdt/main.c` 是 5 个 `content_adapted` 之一（P33 锁存写 1 而非原厂的 0，且 `WDT_Config()`/`SYS_EnableWDTReset()` 移到 GPIO 初始化之后）。按本口径 S2 **不回滚**该业务改写，因此恢复动作本身不会使旧场景转 FAIL；但“P33 与原厂不一致、复位语义缺少独立观测”仍是功能验收欠账，`reset_wdt` 的 `qualification_status` 不得因场景现有断言通过而转绿。其余 4 个适配文件（两处 EPWM brake `main.c` 的空语句写法、`led_4com_8seg/isr.c` 删除尾部空官方处理程序、`temperture_sensor/demo_ts.c` 声明上移）同样逐项带 `reason`，其中温度数值与 Timer2 比较输出的功能缺陷仍另列治理。

校验器模式互斥：`--normalize-mirror`（只做编码/换行规范化，不碰内容）、`--restore-upstream`（只恢复 `upstream_normalized` 集合）、`--write-lock`（全部原厂件在场才可生成）、`--verify`（默认，比对已审定 lock；原厂参考树在场时同时复核其 raw/canonical 哈希）、`--isr-audit`、`--self-test`（负例：篡改镜像、宏名合法但映射值错、BOM/CRLF、注入 `WINK_ISR`、原件缺失）。`--lock-only` 表示本轮只对照锁、未重新核对原件，输出必须与“本次重新核对原厂原件”区分；参考件缺失时 `drift=upstream_missing` 并计为 `missing_upstream`，禁止静默跳过或以当前镜像反推基准。退出码 0=干净、1=有发现项、2=阻塞或用法错误。修改 lock 必须与来源核对一起审查，普通校验路径永不自动接受当前镜像。

### 2.2 生效 ISR 的官方数值映射

检查限定于本批 CMS8S78xx 镜像对应的芯片、官方版本及有效预处理配置。生效 ISR 的向量宏必须在锁定官方集合内，并满足：

```text
官方宏求值 = Native WINK_ISR(N) = SDCC __interrupt(N) = 对应框架派发号
```

保存每个 ISR 的源位置、宏、官方数值、生成注册和派发映射。当前转译器使用 `KNOWN_VECTORS` 等硬编码转换，宏名合法不能证明转换值正确；应对值逐一核验，避免 Native 与 SDCC 共用错误映射产生一致假象。优先级模块枚举和向量地址不能当作 interrupt 向量索引。

未解析宏、重复生效注册、缺少官方依据的推断/保留向量均使该项失败；条件编译排除的 ISR 不算生效注册。增加“合法宏被转成错误数值”的负例。这项检查不限制其他芯片框架的合法接口，也不以新手写占位宏取代官方依据。

## 3. 构建与运行身份

每次运行使用新的 `run_id` 和独立证据目录，例如 `artifacts/cms8s78xx-baseline/<run_id>/`。S0 保存起始快照与入口预检，不依赖尚未实现的批次脚本；修复及工具实现完成、单 App 试跑通过后重新冻结 App、源文件、工具、场景与构建配置，正式全量批次使用新 `run_id`。最终再次校验输入没有变化。

身份范围包括：

- embedded commit、dirty 状态、参与构建文件的内容哈希；未提交试跑需要覆盖已修改及未跟踪输入，不能只记 HEAD。
- 专用脚本所选入口、选择原因、启动命令，以及正式 CLI/仿真运行时的实际版本或源码指纹和源码/二进制模式；不记录或发布外仓内部 TS 路径。
- Python、Emscripten、SDCC、CMake、构建器、Node/Bun 实际使用路径与版本；记录有效编译配置和 SDK 模式。
- 当前实际存在的门禁入口、参数、工作目录、测试清单和逐类命令超时；Host 回归保持已验证的 32 位配置并记录编译器/ABI，不能把换用其他配置的结果直接归入原回归基线。
- 每 App SDCC/Emscripten 实际执行的编译及链接命令：工作目录、可执行文件、参数数组、响应文件内容/哈希、宏定义、头文件/库搜索顺序、影响构建的环境项及有效默认值。外层 CLI 命令不能代替这些信息。
- 每 App SDCC memory model、栈策略及来源依据；App、StdDriver、链接和所选运行库必须有一致模型/ABI 或明确兼容依据。当前门禁默认 `small`，实施时应显式锁定；容量预算不等于内存模型，也不自动等于官方 Keil 项目配置。
- Wasm 目标架构、编译/优化配置、栈和内存参数独立记录；SDCC 的 `small/large` 不用于描述 Wasm 配置。S0 冻结预期 profile，S2/S3 采集实际命令并核验，未解释的偏差不能通过。
- 原厂设备头及 StdDriver 等本地构建依赖的哈希；只记录必要元数据，不复制厂商 SDK 到交付物。
- 每个 App 的 manifest、CMake、场景、镜像源，以及 SDK 公共源码和构建规则的输入身份。
- `device-tree.json`、`wink_simulator.js`、`wink_simulator.wasm` 的原始字节 SHA-256、大小及输出路径。

内容清单采用排序后的相对路径与文件哈希，排除 `.git`、构建产物和证据目录。哈希覆盖规则需版本化并明确列出，不用不透明的“目录哈希”代替输入清单。时间戳用于审计，不能替代内容哈希。

正式 CLI 的 `sim run` 会自动构建，因此不能把前一次 `build sim` 的哈希直接认定为执行产物。选定下述 A/B 中的一组一致产物作为执行资产，运行前后核对哈希及输出路径，保存正式报告；再次构建的产物也须匹配该组身份。发生资产变化或无法绑定时记 `identity_mismatch` / `unverified`，不得复用已有 PASS。构建开始后源码变化则该批次不能作为冻结基线。

运行时用已核对的 `--wasm-dir` 指定资产搜索位置，场景与凭据分别用 `--scenarios` / `--artifacts` 传入，并通过正式工具报告或可核验加载记录确认实际选中的 device-tree / JS / Wasm 对象及内容身份，保存证据引用。参数、搜索路径和运行前后静态哈希都不能单独证明加载对象；缺少此类执行证据时绑定状态为 `unverified`，不由本地摘要自填“已加载”。当前公开契约不足时，记录阻塞和所需契约变更。

`sim run` 的 `--out` 不等于“只重定位资产”：试跑实测该参数会被复用为仿真引擎的 `--app` 目录（`sim run` 没有 `--no-build`，`--no-build` 只存在于 `consistency` 子命令），把 A/B 资产目录作为 `--out` 传入会让引擎把它当 App 解析并失败。因此执行阶段省略 `--out`，改为在隔离副本内暂存 App 目录、把选定的一轮三件套资产植入其 `unisim-assets/`，再以 `--app <暂存 App>`、`--wasm-dir <该资产目录>` 运行；`--out` 仅在 `build sim` 一侧用于重定位资产输出。

### 3.1 双次独立构建可复现性

每个 App 在相同冻结源码、依赖、平台、工具链及有效配置下执行 A/B 两轮构建。除声明并审定的目录映射外，配置一致；实际编译树和输出目录独立且从干净状态开始，禁止复用前一轮 App/SDK 目标文件或归档库。固定的工具链运行库属于记录的构建输入。

`--out` 仅选择资产输出目录，两个不同 `--out` 不足以证明编译隔离。实施前须明确实际编译根及源路径；优先使用正式 CLI 支持的独立编译根，否则核验隔离源码副本能否通过同一入口绑定各自 SDK 并生成独立编译树。正式 Wasm 基线不以直调 CMake/emcc 替代 CLI，也不虚构参数。隔离无法证明时记 BLOCKED，所需公开接口另列变更提案。

试跑选定的方法是后者：每轮在证据目录内实复制（非 junction）一份 App 与 SDK，用 `WINK_AI_EMBEDDED_DIR` 把公开 CLI 的 embedded 根指向该副本，从而得到各自的编译根 `build/wasm/<app_id>`，全程仍走 `build sim --clean --sdk-mode source`。副本必须连带 CMake 解析用到的兄弟目录（`wink-tools`、`wink-micro-app/common`），否则板级配置会在多级 EXISTS 回退后指向不存在的文件而构建失败——这属于隔离方法自身的缺陷，不是 App 的构建结果。

A/B 各保存 37 组三件套的原始字节哈希，共 222 个文件身份，逐 App 比较 `device-tree.json`、JS、Wasm 的 SHA-256。全部对应哈希一致才为 `reproducibility_status=PASS`；缺失构建、失败、哈希差异或无法证明独立性均不能通过。保存构建命令、日志、两份资产清单及差异报告。

时间戳、绝对路径、调试信息或生成顺序造成的内容差异需要查明并修正输入/生成过程，不能静默删除字段或剥离段后称字节一致。如果另做规范化比较，须记录具名规则和版本，保留原始哈希，并标为规范化可复现辅助结果；它不替代本计划原始字节一致的硬门禁。

试跑实测的绝对路径差异来自 `WINK_PT_DEBUG` 下 `WINK_ASSERT` 落到 libc `assert()`，其消息文本内嵌 `__FILE__` 并进入 `.rodata`，因此 wasm 字节里带着编译树位置。声明的目录映射在公共构建规则里用 clang `-ffile-prefix-map=<workspace>=/wink-baseline` 实现（MinGW Makefiles 传反斜杠路径、Ninja 传正斜杠，两种分隔形式都要传），只改写调试文本，不改变断言、预算或符号化。产物随后逐字节扫描两种分隔形式的构建路径，命中即 `path_leakage` 失败；映射后的固定前缀本身不算泄漏。

本门禁的结论限定为冻结平台、工具链和配置下的重复构建可复现；不证明跨平台/跨工具链一致，也不能证明业务功能正确。同一功能缺陷可以被稳定地重复构建。

### 3.2 全量前的单 App 试跑

批次工具实现后，先用 `wink-micro-app/vendor/cms8s78xx/gpio` 验证 A/B 构建与正式场景执行。`pilot.json` 固定可核验的实际 App/SDK 源路径、编译根、清理范围、隔离/命令采集方法、加载身份证据、原始哈希，以及缺资产、哈希不符、加载对象不符和缺报告等负例结果。不能靠改写其他工作的共享 SDK 绑定取得隔离。

试跑通过后重新冻结最终输入与方法，使用新的正式批次运行全部 37 App / 38 场景；试跑产物不计入正式 A/B 的 222 个文件身份。方法未证明或契约不足时阻塞全量启动，不以替换底层构建器/runner 或在全量 App 上重复尝试来掩盖缺口。

## 4. 本地凭据目录及结果语义

```text
<run_id>/
  run.json                 # 输入、仓库、工具链、模式、时间及清单身份
  preflight.json           # 入口、参数、profile、测试清单与超时配置
  pilot.json               # 试跑批次证据；正式批次引用通过的试跑记录
  source-manifest.json     # 参与构建的文件列表及哈希
  mirror-audit.json        # 原件和镜像核对结果
  isr-vector-audit.json    # 官方数值、两种转译和派发映射
  summary.json             # 37 个 App 的逐阶段结果
  summary.md               # 供人工审查的同源摘要
  apps/<app>/
    sdcc.log
    sdcc-commands.json     # 实际编译/链接命令和模型
    builds/A/              # 构建命令、日志、资产和清单
    builds/B/              # 独立第二轮对应证据
    reproducibility.json  # 原始哈希对比及差异报告引用
    assets/                # 从 A/B 选定并核对的执行资产
    asset-manifest.json    # 标明选自哪一轮及其原始身份
    headless/              # 正式 CLI 的场景报告及凭据
    run.log
```

逐 App 分别记录 `mirror_status`、`isr_vector_status`、`sdcc_status`、A/B 的 `wasm_build_status`、`reproducibility_status`、`asset_identity_status`、`scenario_status`、`oracle_review_status`、`qualification_status`；同时记录可复现性比较范围和模式。执行状态至少区分 PASS、FAIL、ERROR、TIMEOUT、BLOCKED、NOT_RUN；未执行、缺报告或工具链缺失不能转成 PASS。

`scenario_status=PASS` 只表示现有断言通过。已知有弱断言的案例保持 `oracle_review_status=weak`，其 `qualification_status` 不能为完整验收通过。按 2.1 的口径，`reset_wdt/main.c` 的 P33/WDT 改写保留为 `content_adapted`，S2 不回滚，因此恢复动作不会凭空把旧场景转成 FAIL；但该 App 与原厂初始化的差异及缺少独立复位观测，仍使 `qualification_status` 不能因场景现有断言通过而转绿。若后续另行决定恢复原厂业务源并导致旧断言冲突，保留真实 FAIL，不修改业务源迎合原断言。

覆盖按输入清单中的 38 个场景文件及哈希逐一关联正式报告，不能用断言条数代替；正式全量不使用筛选参数缩小范围。单 App 失败只阻塞该 App 的依赖阶段，其他独立检查继续；公共链路失败则阻塞相关正式验收。输入改变后保存旧批次，修复后用新 `run_id` 全量复验，不拼接旧 PASS。非零退出码、断言 FAIL、TIMEOUT、缺报告分别保存和判定。

摘要至少分为 `build_baseline_status`、`execution_evidence_status`、`scenario_suite_status` 和 `functional_qualification_status`：构建项只有输入/配置、镜像、ISR、SDCC、双次 Wasm 门禁与规定的回归检查全满足才 PASS；执行凭据只有全部场景有真实执行报告且加载身份可绑定才 PASS（断言 FAIL 可有完整执行凭据）；场景项只有 38 个场景文件对应的全部断言通过才 PASS；功能项还须满足独立业务验收。已知失败不能豁免或改成 PASS，但不阻止交付真实审计记录；目标未达成时须明确标注。

批次摘要分别给出源码/ISR/构建基线是否满足、双次构建是否字节一致、证据是否完整、场景是否全过以及功能是否合格；任何单项失败或未执行都不能输出笼统的“全部完成”。双次哈希一致不能将弱断言或功能缺陷转为通过。默认批次命令在必要门禁或场景失败时返回非零，同时保留已成功的阶段结果。需要条件性继续时逐项收集失败，不能中途丢失其他 App 的状态行。

## 5. 复用现有入口

专用 `run_cms8s78xx_baseline.py`（试跑阶段已实现）负责本批清单、门禁调度、双次构建及证据汇总。Wasm 构建统一调用 `build sim --app ... --clean --sdk-mode source --out ...`；运行调用 `sim run --app ... --mode headless --scenarios ... --wasm-dir ... --artifacts ... --reporter json`，按上节所述不传 `--out`。专用脚本只解析入口并调用公开契约，不另写构建器或仿真 runner；镜像、ISR、SDCC 检查仍复用各自门禁工具。

入口统一使用 PATH 中已安装且可用的全局 `winkcli`（亦可通过 `--cli-path` 显式覆盖路径）。显式路径无效或全局命令缺失时报告配置错误，不静默改选。脚本不修改调用者环境。

执行前验证所选入口的版本/指纹、所需参数、配套仿真运行时和依赖，冻结同一批次的入口及身份；构建或运行失败后不得自动换版本合并结果。构建时使用 `--sdk-mode source` 构建 embedded SDK。后备入口缺失或不兼容时记 ERROR/BLOCKED、返回非零并保留原因。

当前本地正式启动器的参数已核对。批次工具本身已实现统一 `winkcli` 解析，在真实批次中规范运行。

批次显式枚举本目录 37 个 App，不使用扫描整个 workspace 的 `--all`，也不把默认五个 carrier 脚本的通过当作 vendor 全量通过。批次工具通过参数数组启动子进程，保存真实退出码和超时状态；在 Windows 清理前验证实际构建目录位于预期 workspace 构建树中。

这里只定义 embedded 仓库本地审计记录，不推定仿真引擎内部行为，也不新增前端、UniSim 或 wink-tools 的跨仓实现要求。如确需新的运行产物身份契约，应单独提出接口变更并更新设计规范。

## 6. 修订记录

- 2026-10-09 / v1.4：与已批准口径对齐——确立 `content_target`（`upstream_normalized` / `wink_adapted`）与 `drift` 判定词汇、`reason` 强制项和恢复范围（仅六个 EPWM brake 的 `isr.c` 取原厂字节，其余 142 个镜像保留适配内容、原厂件仍为证据与锁基准），据此改写 ResetWDT 的结果预期；按试跑实测补上 `sim run --out` 会复用为引擎 `--app` 的执行契约、隔离副本须带 `wink-tools` / `wink-micro-app/common` 兄弟目录，以及 `-ffile-prefix-map` + 构建路径字节扫描的可复现性处理方式；记录镜像校验器模式与退出码。仅更新方案，全量结果尚未验收。
- 2026-10-09 / v1.3：同步起始/最终冻结顺序、Host 配置与预检记录、单 App 试跑、实际加载对象证据、逐场景文件覆盖以及分项验收/失败继续规则。仅修改方案，隔离和加载证据取得方式仍须通过真实试跑验证。
- 2026-10-09 / v1.2：同步专用脚本的本地兄弟仓 / 已安装 winkcli 选择、版本预检和批次冻结要求；明确 SDK 源码模式与工具运行模式的区别，并限定隔离构建仍通过正式 CLI。当前仅为方案修订，后备分支尚未实施。
- 2026-10-09 / v1.1：同步补充实际命令与内存配置、官方 ISR 数值映射、A/B 构建可复现性及结果状态；当前为设计要求，尚未执行或验收。
