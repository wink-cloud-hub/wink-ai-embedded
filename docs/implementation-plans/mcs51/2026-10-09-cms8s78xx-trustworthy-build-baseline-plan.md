# CMS8S78xx 可信构建基线实施计划

| 项 | 内容 |
|---|---|
| 编号 | `PLAN-20261009-CMS8S78XX-BUILD-BASELINE` |
| 日期 / 状态 | 2026-10-09 / Proposed，等待实施确认；生产代码尚未修改 |
| 起始评审基线 | `09d002f7b34dd4b9b585e164a119642785af071a`；执行时重新冻结实际源码状态 |
| 技术方案 | [源码与构建身份方案](../../zh/tech-designs/mcs51/2026-10-09-cms8s78xx-build-baseline-design.md) |
| 评审依据 | [假绿与完整性审查](../../reviews/mcs51/2026-10-09-cms8s78xx-false-green-and-framework-completeness-review.md) |
| 设计规范 | [MCS-51 仿真拦截](../../zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md) |
| 相关决策 | ADR-0070、ADR-0076、ADR-0082；本计划不改变 Native / ISS 的责任边界 |
| 实施方式 | 按依赖顺序执行；不要求子代理或并行代理 |
| 完成口径 | 源码可追溯、37 App 构建结果明确、38 场景逐一记账、执行产物可绑定；功能验收单独判定 |

## 1. 已知事实与试验

- 当前 37 个 App、38 个场景；Host MCS-51 测试 71/71、转译器测试 10/10 通过。
- 当前 SDCC 为 31/37；六个 EPWM brake 的 `isr.c` 使用后加的推断向量名，官方器件头路径无法解析。
- 公共 `pal_wasm_hwtimer.c` 两处使用不存在的 `pal_hwtimer_cb_t`，阻止正式 Wasm 重建。
- 148 个顶层原厂 `.c/.h` 中 63 个有文本差异，忽略注释/空白后 11 个仍有 token 差异。恢复时逐文件核对，不能将所有差异都归为业务变化。
- **本次计划准备已做隔离验证**：在忽略目录的临时副本恢复六个 EPWM brake 的原厂文件，SDCC **6/6 通过**。未修改生产 App。证据：`artifacts/cms8s78xx-audit-20261009/restored-prototype-sdcc.log`（1/1）、`restored-prototype-sdcc-remaining.log`（5/5）。

上述试验支持“先恢复原件再复验”的路线，不代表恢复全部镜像后的 37 App 或 38 场景已验收。

## 2. 任务顺序与门禁

| 阶段 | 操作 | 验收出口 |
|---|---|---|
| S0 冻结输入 | 固定 App/场景清单、源码状态、原厂参考件及工具链 | 37 App / 38 场景身份明确，未跟踪构建输入也有哈希 |
| S1 修复公共构建 | 两处回调类型对齐现有 PAL 头文件 | 真实 Wasm 重建通过，相关既有 PAL 测试通过 |
| S2 恢复镜像 | 原厂内容按 UTF-8/LF 规范化恢复，加入 lock 和校验器 | 148 文件内容匹配锁定原件，六个失败消失，全量 SDCC 37/37 |
| S3 全量重建 | 显式批次、逐 App clean、源码 SDK 模式、隔离输出 | 37/37 Wasm 重建，111 个资产文件均有哈希 |
| S4 全场景复验 | 正式 CLI 执行 38 场景，绑定实际资产和输入身份 | 每个场景有独立状态；真实失败保留，弱断言不算功能验收通过 |
| S5 交付基线 | 汇总凭据、更新活文档、追加复验记录 | 结果可追溯、构建和业务验收结论分别报告 |

所有阶段在执行前均为待办。S1/S2 导致构建输入变化，必须在 S3 开始前重新冻结身份。

## 3. S0 — 冻结输入与保护现场

- [ ] 保留现有未跟踪文档、旧资产与历史凭据，不覆盖其他工作。
- [ ] 按 `wink-app.json` 显式枚举本目录 App，并保存 37 App / 38 场景的精确路径清单；数量变化必须有明示原因。
- [ ] 保存 embedded HEAD、dirty 状态及实际构建输入哈希；正式交付优先基于原子提交后的稳定源码，未提交试跑不能只引用 HEAD。
- [ ] 保存实际 CLI / 仿真运行时身份，以及 Python、Emscripten、SDCC、CMake、构建器、Node/Bun 的路径、版本和有效配置。
- [ ] 核验 `upstream.source_dir` 均指向预期 `docs/vendors/` 参考树，记录 V2.0.2 原始文件、设备头和 StdDriver 指纹；缺失来源不得生成“已核对”结果。
- [ ] 原厂包来源不能验证时标记 `local_reference`；不将本地文件相同升级解释成官方签名或发布渠道验证。

## 4. S1 — 最小修复 Wasm 公共编译错误

- [ ] 将 `wink-micro-os/targets/wasm/pal_wasm_hwtimer.c` 两处局部回调变量的类型改为现有公共契约 `pal_hwtimer_isr_t`。
- [ ] 检查有无同类残留，不通过新增过时 typedef 掩盖接口漂移。
- [ ] 使用源码 SDK 模式，干净重建 `temperture_sensor` 及通用 smoke；本阶段只判构建，不提前声称温度业务正确。
- [ ] 运行既有 `test_pal_hwtimer`、直接编译 Wasm 实现的 `test_pal_wasm_drain`，以及 MCS-51 Host 回归。Host stub 通过不能代替真实 Wasm 编译。
- [ ] 如遇新的编译错误，保存日志、判断是否为同一公共链路问题；无关功能修复另列，不扩大为临时重构。

## 5. S2 — 恢复官方镜像与 SDCC 一致性

- [ ] 以 manifest 声明的原厂文件逐一恢复现有顶层 `.c/.h`，只做严格解码、UTF-8 无 BOM、LF 换行转换，不改注释、声明顺序、业务数值或 ISR 集合。
- [ ] 首先处理六个 EPWM brake 的后加空 ISR；隔离试验已确认原厂版本可通过 SDCC。
- [ ] 恢复 ResetWDT 的原厂 P33 初始化值与 WDT/GPIO 初始化顺序；复核 LED、温度及其他镜像差异。
- [ ] 新增 `upstream-lock.json` 和镜像校验器，保存 raw/canonical 哈希及规范化规则；普通校验模式禁止自动更新锁。
- [ ] 镜像校验支持“本地原件在场核对”和“对已审定 lock 校验”两个明确状态；上游缺失时禁止静默跳过或改用当前镜像生成基准。
- [ ] 全量运行 SDCC 编译、链接、容量门禁，目标为 37/37；不补缺乏原厂依据的向量数字，不关预算门禁，不忽略失败 App。
- [ ] 运行转译器、shim 审计、Host 回归及新增镜像校验负例。生产原厂源码不用于承载仿真适配补丁。
- [ ] README/PLAYBOOK 的“镜像一致”表述与 UTF-8/LF 规范化规则对齐，保留原厂版权许可；厂商 SDK 和 `docs/vendors/` 不入库。

**必须显式处理的结果变化**：ResetWDT 恢复原件后，旧场景要求 P33 为 1 的前提不再成立。该场景先如实记录 FAIL / 契约冲突，后续用独立复位观测重新设计验证。不为了让本计划全绿而只改 matcher 或重新修改业务源。

## 6. S3/S4 — 全量重建和凭据绑定

- [ ] 新增本目录专用批次工具，按冻结清单逐 App 执行，保存阶段退出码、日志、超时和阻塞原因。
- [ ] 使用 `build sim --clean --sdk-mode source`；清理前确认实际目标位于 workspace 预期构建树。输出到新的 run_id 目录，不依赖既有 `unisim-assets/`。
- [ ] 每个成功 App 保存 device-tree / JS / Wasm 的 SHA-256、大小和路径，共 37 组三件套。
- [ ] 调用正式 `sim run --mode headless`，传入该 App 的场景目录、隔离资产及凭据目录；本计划只使用公开 CLI 契约。
- [ ] 处理正式 CLI 自动构建行为：记录运行前后资产身份并核对输出路径；身份变化或无法绑定时不引用旧 PASS。
- [ ] 批次结束校验源码和配置未改变；中途改变输入的批次不能封为稳定基线。
- [ ] 对全部 38 场景记账；构建失败则相应场景为 BLOCKED，不能从分母移除。首个失败后仍保存其他 App 的阶段结果。
- [ ] 单独列出弱断言与已知模型缺陷；原断言 PASS 只能写“现有场景通过”。EPWM、温度、Compare、Reset 的功能验收不得自动转绿。

以下参数已通过当前正式 CLI 帮助核对。示例展示单 App 执行方式，正式批次由工具逐一套用；这些构建和运行命令尚未在本计划下执行：

```powershell
$embeddedRoot = 'D:\workspaces\ai-coding\wink-ai\wink-ai-embedded'
$toolEntry = 'D:\workspaces\ai-coding\wink-ai\wink-ai\packages\wink-tools\wink.py'
$appDir = Join-Path $embeddedRoot 'wink-micro-app\vendor\cms8s78xx\gpio'
$runId = '{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), ([guid]::NewGuid().ToString('N').Substring(0,8))
$runRoot = Join-Path $embeddedRoot "artifacts\cms8s78xx-baseline\$runId"
$assetDir = Join-Path $runRoot 'apps\gpio\assets'
$evidenceDir = Join-Path $runRoot 'apps\gpio\headless'
$env:WINK_DEV = '1'

python $toolEntry build sim --app $appDir --clean --sdk-mode source --out $assetDir
if ($LASTEXITCODE -ne 0) { throw 'Wasm 构建失败；运行阶段记 BLOCKED。' }
python $toolEntry sim run --app $appDir --mode headless `
  --scenarios (Join-Path $appDir 'unisim-scenarios') `
  --out $assetDir --artifacts $evidenceDir --reporter json
if ($LASTEXITCODE -ne 0) { throw '场景复验失败；保留正式报告和凭据。' }
```

示例自动生成唯一批次名，只展示单 App 的入口与失败处理，不能替代完整身份记录。正式 runner 用子进程参数数组并检查退出码；批次不使用 `build sim --all` 扫描其他 App，也不复用默认只跑五个通用 carrier 的成功摘要。

## 7. 拟变更文件与原子交付

| 文件/范围 | 意图 |
|---|---|
| `targets/wasm/pal_wasm_hwtimer.c` | 公共回调类型修复，独立逻辑提交 |
| `wink-micro-app/vendor/cms8s78xx/*/*.c,*.h` | 还原原厂内容，保留许可；差异按可审查模块聚合 |
| `wink-micro-app/vendor/cms8s78xx/upstream-lock.json` | 固定镜像来源和内容身份 |
| `frameworks/mcs51/tools/audit_vendor_mirror.py`（拟新增）及测试 | 来源/镜像匹配门禁 |
| `frameworks/mcs51/tools/run_cms8s78xx_baseline.py`（拟新增）及必要测试 | 本目录 37 App / 38 场景批次凭据 |
| README / PLAYBOOK / CHECKLIST / 现行设计规范 | 明确可信结果与未完成能力，更新正式入口 |
| `docs/reviews/mcs51/` 新复验记录 | 引用新基线与真实结果；不修改已归档审查记录 |

新工具名称为拟定项，实施时可复用已有等价工具；避免额外搭建通用治理系统。代码变更运行 `winkcli lint --pack layering --pack api` 和 `python .github/scripts/check_license_map.py`，新增工具按 GPL-3.0-only、PAL 修改按 LGPL-3.0-only、第三方原厂源保留许可。

## 8. 验收、负例与风险

| 检查 | 要求 |
|---|---|
| 镜像一致性 | 148/148 对锁定原件规范化内容一致；没有业务改写或无法解释的来源 |
| SDCC | 37/37 编译+链接+预算通过，保留日志；不扩展成 Keil/可烧写承诺 |
| Wasm | 37/37 clean + source 构建，111 个资产文件可定位且有哈希 |
| 场景记账 | 38/38 有状态和对应资产身份，失败/阻塞/未执行全部显式可见 |
| 业务结论 | `scenario PASS`、弱断言、功能验收分别记录；不能以构建通过覆盖模型缺陷 |
| 负例 | 篡改一处镜像、缺失 App/场景、子进程失败/超时、构建输入/资产漂移均能阻止错误成功摘要 |
| 稳定性 | 输入冻结检查通过，工具链和运行模式明确；源码模式无旧二进制 SDK 替代 |

风险主要是恢复官方源暴露转译兼容问题、旧场景依赖改写后的业务、CLI 自动重建造成身份错绑，以及公共构建后续暴露新错误。应分别修工具层、登记场景失败、拒绝身份不一致、保留实际错误，不使用跳过/放宽断言来闭合基线。

执行按 S0→S1→S2→S3→S4→S5 推进。S1/S2 完成后先给出镜像与构建阶段结果，再运行耗时批次；结束时分别报告“构建基线是否恢复”和“业务功能还欠哪些验证”。不为尚未运行的任务填写完成时间或 PASS。
