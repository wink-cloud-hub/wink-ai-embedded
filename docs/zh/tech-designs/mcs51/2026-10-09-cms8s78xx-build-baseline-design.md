# CMS8S78xx 可信构建基线技术方案

| 项 | 内容 |
|---|---|
| 日期 / 状态 | 2026-10-09 / Proposed，尚未实施 |
| 关联实施计划 | [可信构建基线实施计划](../../../implementation-plans/mcs51/2026-10-09-cms8s78xx-trustworthy-build-baseline-plan.md) |
| 触发评审 | [假绿与完整性审查](../../../reviews/mcs51/2026-10-09-cms8s78xx-false-green-and-framework-completeness-review.md) |
| 设计规范 | [MCS-51 仿真拦截](../../design/02-wink-micro-os/07-mcs51-simulation-interception.md) |
| 范围 | 当前 37 个 vendor App 的源码追溯、真实重建与运行凭据；不新增跨仓 ABI |

## 1. 目标和边界

基线应回答三个问题：**构建输入是什么、生成了哪些产物、本次场景是否确实运行了这些产物。** 它不以重新得到 38 个绿色结果作为唯一目标；已有断言盲区和模型缺陷仍须独立治理。

本方案只修公共 Wasm 回调类型错误、恢复官方镜像内容、补镜像校验和全量构建凭据。EPWM 波形、温度数值、Timer2 比较输出及复位观测的功能修复另行实施，不将它们夹入构建修复。

## 2. 官方镜像的身份

采用一个版本化的 `wink-micro-app/vendor/cms8s78xx/upstream-lock.json`，列出 37 个 App 的原厂文件及哈希。原厂文件通过现有 manifest 的 `upstream.source_dir` 定位，厂商 SDK 留在本地忽略目录，禁止整包提交。

每个镜像文件至少保存：

| 字段 | 含义 |
|---|---|
| `upstream_path` / `vendor_version` | 相对来源位置及声明版本 |
| `upstream_raw_sha256` | 原厂原始字节 SHA-256 |
| `upstream_encoding` | 实际严格解码成功的编码 |
| `normalization` | 唯一允许的转换规则：严格解码、UTF-8 无 BOM、换行统一 LF |
| `canonical_sha256` | 按上述规则规范化后的 SHA-256，供镜像内容比对 |
| `mirror_path` | 仓库中被校验的文件位置 |
| `origin_assurance` | 有原始包凭据时记录其来源和包哈希；只有本地 SDK 时明确为 `local_reference` |

不通过忽略注释、删除空 ISR、重排声明或改业务值来实现“相同”。编码/换行规范化以外的内容差异均应失败。解码不能使用 `errors=replace`。

校验器在本地原厂 SDK 存在时同时核验原件 raw/canonical 哈希；缺失时可以核验镜像是否匹配已审定 lock，但必须区分“匹配锁定内容”和“本次重新核对原厂原件”。原件缺失禁止重新生成 lock。修改 lock 必须与来源核对一起审查，不能每次运行自动接受当前镜像。

## 3. 构建与运行身份

每次运行使用新的 `run_id` 和独立证据目录，例如 `artifacts/cms8s78xx-baseline/<run_id>/`。开始时冻结 App、源文件、场景与构建配置清单；最终再次校验输入没有变化。

身份范围包括：

- embedded commit、dirty 状态、参与构建文件的内容哈希；未提交试跑需要覆盖已修改及未跟踪输入，不能只记 HEAD。
- 正式 CLI/仿真运行时的实际版本或源码指纹，以及外仓工具使用的源码/二进制模式；不记录或发布外仓内部 TS 路径。
- Python、Emscripten、SDCC、CMake、构建器、Node/Bun 实际使用路径与版本；记录有效编译配置和 SDK 模式。
- 原厂设备头及 StdDriver 等本地构建依赖的哈希；只记录必要元数据，不复制厂商 SDK 到交付物。
- 每个 App 的 manifest、CMake、场景、镜像源，以及 SDK 公共源码和构建规则的输入身份。
- `device-tree.json`、`wink_simulator.js`、`wink_simulator.wasm` 的原始字节 SHA-256、大小及输出路径。

内容清单采用排序后的相对路径与文件哈希，排除 `.git`、构建产物和证据目录。哈希覆盖规则需版本化并明确列出，不用不透明的“目录哈希”代替输入清单。时间戳用于审计，不能替代内容哈希。

正式 CLI 的 `sim run` 会自动构建，因此不能把前一次 `build sim` 的哈希直接认定为执行产物。运行前后检查资产哈希，从 CLI 日志确认输出路径，并保存正式报告；发生资产变化或无法绑定时记 `identity_mismatch` / `unverified`，不得复用已有 PASS。构建开始后源码变化则该批次不能作为冻结基线。

## 4. 本地凭据目录及结果语义

```text
<run_id>/
  run.json                 # 输入、仓库、工具链、模式、时间及清单身份
  source-manifest.json     # 参与构建的文件列表及哈希
  mirror-audit.json        # 原件和镜像核对结果
  summary.json             # 37 个 App 的逐阶段结果
  summary.md               # 供人工审查的同源摘要
  apps/<app>/
    sdcc.log
    build.log
    assets/                # 本次真实生成的三件套
    asset-manifest.json
    headless/              # 正式 CLI 的场景报告及凭据
    run.log
```

逐 App 分别记录 `mirror_status`、`sdcc_status`、`wasm_build_status`、`asset_identity_status`、`scenario_status`、`oracle_review_status`、`qualification_status`。执行状态至少区分 PASS、FAIL、ERROR、TIMEOUT、BLOCKED、NOT_RUN；未执行、缺报告或工具链缺失不能转成 PASS。

`scenario_status=PASS` 只表示现有断言通过。已知有弱断言的案例保持 `oracle_review_status=weak`，其 `qualification_status` 不能为完整验收通过。恢复原厂 ResetWDT 后若旧场景冲突，保留真实 FAIL，不修改业务源迎合原断言。

批次摘要分别给出源码/构建基线是否满足、证据是否完整、场景是否全过以及功能是否合格；任何单项失败或未执行都不能输出笼统的“全部完成”。默认批次命令在场景失败时返回非零，同时保留已成功的构建阶段结果。需要条件性继续时逐项收集失败，不能中途丢失其他 App 的状态行。

## 5. 复用现有入口

构建统一调用已确认支持的 `winkcli build sim --app ... --clean --sdk-mode source --out ...`；运行调用 `winkcli sim run --app ... --mode headless --scenarios ... --out ... --artifacts ... --reporter json`。本地可使用现有 `wink.py` 正式启动器。

批次显式枚举本目录 37 个 App，不使用扫描整个 workspace 的 `--all`，也不把默认五个 carrier 脚本的通过当作 vendor 全量通过。批次工具通过参数数组启动子进程，保存真实退出码和超时状态；在 Windows 清理前验证实际构建目录位于预期 workspace 构建树中。

这里只定义 embedded 仓库本地审计记录，不推定仿真引擎内部行为，也不新增前端、UniSim 或 wink-tools 的跨仓实现要求。如确需新的运行产物身份契约，应单独提出接口变更并更新设计规范。
