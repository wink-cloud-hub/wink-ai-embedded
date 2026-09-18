# 实施计划：wink-micro-app 嵌套目录发现（ADR-0079）

- 日期：2026-09-09
- 关联决策：[ADR-0079](../../decisions/core/0079-micro-app-nested-app-discovery.md)
- 状态：已实施（2026-09-09）
- 范围：Python 工具链、unisim、embedded-frontend 三处发现器；CMake 零改动；存量 app 不迁移

## 1. 目标与验收标准

1. `wink-micro-app/<app>`（深度 1）、`<g1>/<app>`（深度 2）、`<g1>/<g2>/<app>`（深度 3）均可被三处发现器识别。
2. 任何目录一旦含 `wink-app.json` 即为 app 边界，向内搜索立即剪枝；深度 >3 的清单不发现并产生告警。
3. 深度 1 app 的 id、构建路径、缓存键、CLI 调用与旧行为逐字节一致。
4. 裸叶子名引用：唯一则命中，歧义则报错并列出全 id。
5. Python / bun / vitest 相关测试全部通过（master 上既有的无关失败除外）。

## 2. 任务拆分与执行记录

### 2.1 Python wink-tools（winkcli 工具链）

- [x] 新增 `tools/app_discovery.py`：`MAX_APP_DEPTH=3`、`DiscoveredApp`、`discover_apps`（受控 DFS + 剪枝）、`find_app`（精确 id → 唯一叶子别名 → `AmbiguousAppRef`）、`find_app_dir`（容忍无清单半成品）、`find_overshoot_manifests`、`app_id_for`、`normalize_ref`。
- [x] `cli/_shared.py`：新增 `_candidate_app_roots`（workspace/samples/向上遍历同级仓库，resolved 去重）、`try_resolve_app_dir`（非致命），`resolve_app_dir` 改为薄包装输出可操作错误。
- [x] `cli/commands/build/sim.py`：`_discover_apps` 改共享发现器，按 resolved 目录去重返回 id；wasm `build_dir = build/wasm/<qualified-id>`。
- [x] `cli/commands/sim/__init__.py`：两处手工拼路径改 `try_resolve_app_dir`，歧义时返回码 1。
- [x] `cli/bootstrap.py`：任意名为 `wink-micro-app` 的祖先均可推出 workspace root（不再只认直接父目录）。
- [x] `cli/commands/create/__init__.py`：`create app` 接受 1～3 段相对 id，拒绝绝对路径 / `..` / 在 app 边界内嵌套。
- [x] `esp32/generate_app_sources.py`：名称解析走共享发现器（懒导入，保持脚本直跑模式）；`common/include` 从 app 父目录逐级向上、最近者胜。
- [x] `lint/packs/user_surface.py`：清单枚举走发现器；C 扫描保留无清单遗留目录（深度受限、边界剪枝、文件去重）；越界清单 WARN。
- [x] 测试：新增 `test_app_discovery.py`（15 例）；更新 `test_cli_build_sim.py`（顺带修复既存 AppContext 构造漂移，新增嵌套/剪枝/qualified build-dir 用例）、`test_esp32_generate_app_sources.py`（嵌套解析、common 逐级向上）、`test_lint_user_surface.py`（嵌套扫描、边界剪枝、重叠根去重）。

### 2.2 unisim

- [x] 新增 `src/discovery/micro-app-discovery.ts`：唯一共享 walker（`findMicroAppDirs`、`matchAppRef`、`AmbiguousMicroAppError`），经 `discovery/index.ts` 桶导出。
- [x] `src/simulation-runner/consistency/batch-consistency-scanner.ts`：删除私有递归，改调共享 walker；`discoverApps` 返回相对 id（平铺仍是裸名），filter 同时匹配 id 与叶子名，按 id 排序。
- [x] `src/discovery/embedded-workspace-resolver.ts`（unisim CLI `winksim run/consistency` 的 app 解析）：`listAvailableMicroApps` 改共享 walker（`MicroAppInfo.name` 现为合格 id）；`resolveMicroAppPath` 支持全 id / 唯一叶子名（歧义抛错）/ 唯一 `app_name`。
- [x] `src/simulation-runner/browser/browser-runner-node.mjs`：纯 ESM 无法导入 TS，内联同构镜像 `findMicroAppDirsNode` 并同样升级解析顺序（文件内标注 keep-in-sync）；两个函数导出供测试。
- [x] 测试：`micro-app-discovery.test.ts`（walker/剪枝/深度/消歧）、`embedded-workspace-resolver.test.ts`（列举/解析/歧义）、`batch-consistency-scanner.test.ts` 4 例、`browser-runner-node-discovery.test.mjs` 3 例。

### 2.3 embedded-frontend

- [x] `workspace-scanner.ts`：Handle 模式抽出 `collectAppDirectoryHandles`（DFS + 剪枝）与 `buildAppDescriptor`；FileList 模式两阶段（收集清单 → 剔除祖先已是 app 的内层清单 → 最长前缀归属），元数据/资产/场景判定全部锚定 app 相对路径；`pickPreferredApp` 精确 id → 唯一叶子回退（歧义不猜）；oled 偏好按叶子段匹配。
- [x] `types.ts`：`MicroAppDescriptor.id` 注释更新；新增 `groupPath?: string`。
- [x] 测试：workspace-scanner 两测试文件新增 Handle/FileList 嵌套、剪枝、深度 4、嵌套 docs 路径、叶子回退用例；为旧的无清单 fixture 补齐 `wink-app.json`。

### 2.4 文档

- [x] ADR-0079（本决策）。
- [x] 活规范回写：`docs/zh|en/design/02-wink-micro-os/03-directory-architecture.md`。
- [x] `wink-micro-app/README.md` 目录约定；wink-tools `docs/01-cli-overview.md` 嵌套调用示例。
- [x] CMake：经核实无需改动（`WINK_APP_DIR` 路径直通，host common 发现走绝对路径）。

## 3. 验证记录

| 验证 | 命令 | 结果 |
|---|---|---|
| Python 相关用例 | `python -m pytest tools/tests/test_app_discovery.py test_cli_build_sim.py test_esp32_generate_app_sources.py test_lint_user_surface.py` | 50 passed（1 个 drivers registry 既有失败与本变更无关） |
| unisim | `bun test src/discovery src/simulation-runner/consistency src/.../browser-runner-node-discovery.test.mjs` | 26 passed，1 failed（matrix 真实资产分歧，HEAD 上同样失败） |
| 前端 | `npx vitest run src/services/embedded-workspace` | 35 passed；`vue-tsc --noEmit` 0 error |
| 真实工作区冒烟 | 临时 `wink-micro-app/zz_nest_smoke/g2/demo/`：`_discover_apps` 出现 `zz_nest_smoke/g2/demo`；全 id 与裸叶子名 `demo` 解析到同一目录；27 个平铺 id 保持裸名；内层清单被剪枝 | 通过后已删除 |
| host configure | `cmake -S wink-micro-os -B build/wasm -DTARGET_PLATFORM=host -DWINK_APP_DIR=wink-micro-app/zz_nest_smoke/g2/demo`（MinGW Makefiles） | `WINK_APP_DIR`/`WINK_APP_JSON` 正确解析，Configuring/Generating done；验证后已删除 |
| create 校验 | `create app mcs51/button_led` 成功；深度 4 / `..` / 绝对路径 / 在 app 边界内嵌套均拒绝 | 通过 |
| 架构门禁 | `winkcli lint --pack layering --pack api` | No lint findings（未改 C 代码） |

## 4. 风险与回滚

- 平铺 id 不变 → 前端 IndexedDB 旧缓存、`build/wasm/<app>` 旧构建目录均兼容。
- 唯一行为收紧点：同名叶子在出现第二个嵌套同名 app 后由「不确定地命中一个」变为显式歧义错误；错误信息直接给出全 id。
- 回滚：三处发现器改动相互独立，可按仓库分别 revert；共享模块 `app_discovery.py` 为纯新增。
