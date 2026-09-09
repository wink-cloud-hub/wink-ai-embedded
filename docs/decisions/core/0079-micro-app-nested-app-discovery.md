# ADR-0079：wink-micro-app 最多三级嵌套目录与清单边界剪枝发现

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-09-09 拍板）** |
| 日期 | 2026-09-09 |
| 触发 | `wink-micro-app/` 长期平铺，app 数量增长（MCS-51 厂商示例、vendor 示例、业务 demo 混放）后缺乏分组能力；而各处 app 发现逻辑（Python wink-tools、unisim 批量一致性扫描、embedded-frontend 工作区扫描）均写死「只枚举一级子目录」，无法演进为分组结构。 |
| 影响范围 | `wink-tools`（新增 `tools/app_discovery.py`；`cli/_shared.py`、`cli/commands/build/sim.py`、`cli/commands/sim/__init__.py`、`cli/bootstrap.py`、`cli/commands/create/__init__.py`、`esp32/generate_app_sources.py`、`lint/packs/user_surface.py`）；`unisim`（新增 `src/discovery/micro-app-discovery.ts`；`embedded-workspace-resolver.ts`、`simulation-runner/consistency/batch-consistency-scanner.ts`、`simulation-runner/browser/browser-runner-node.mjs`）；`embedded-frontend`（`workspace-scanner.ts`、`types.ts`）；CMake 侧经 `WINK_APP_DIR` 路径直通，无需修改。 |
| 决策者 | 嵌入式系统架构团队 |
| 关联 ADR | 无直接前置；目录约定见 [02-wink-micro-os/03-directory-architecture](../../zh/design/02-wink-micro-os/03-directory-architecture.md)。 |
| 关联计划 | [`docs/implementation-plans/core/2026-09-09-micro-app-nested-discovery-plan.md`](../../implementation-plans/core/2026-09-09-micro-app-nested-discovery-plan.md) |

---

## 1. 背景（Context）

历史上所有 app 必须位于 `wink-micro-app/<app>/wink-app.json`。随着 `mcs51_*`、`vendor_cms8s78xx_*` 等家族前缀膨胀，平铺目录的可读性与可维护性下降，业务方需要按家族/厂商/项目分组组织 app。

现状盘点（三处发现逻辑 + 一个构建入口）：

1. **Python wink-tools**：`build-sim --all`、`resolve_app_dir`、esp32 源文件生成、`create app`、user_surface lint 全部假定 app 是 `wink-micro-app/` 的直接子目录。
2. **unisim**：`BatchConsistencyScanner.discoverApps` 只 `readdirSync` 一层。
3. **embedded-frontend**：Handle/FileList 两种工作区扫描都只枚举一级；FileList 模式以 micro-app 段后的第一个路径段作为 app id。
4. **CMake**：`WINK_APP_DIR` 接受任意相对/绝对路径，天然不关心嵌套层级——是本决策中唯一无需改动的一侧。

约束：

- 嵌套不能无上限，否则发现成本与错误配置不可控；
- 必须与现有平铺 app **100% 向后兼容**（现有 28 个 app 不迁移，id、缓存键、CLI 调用、IndexedDB 记录不变）；
- app 目录内部是私有空间（源码、docs、unisim-assets、临时产物），发现逻辑不能无边界地向内穿透。

## 2. 方案比选（Options）

| 方案 | 描述 | 优 | 劣 | 结论 |
|---|---|---|---|---|
| A. 维持平铺，靠命名前缀分组 | 继续用 `mcs51_*` / `vendor_*` 前缀 | 零改动 | 前缀冲突、无法表达两级分组、前端列表无层级 | ❌ 否决 |
| B. 无限递归 + rglob | 任意深度均可放 app | 最灵活 | 扫描不可控、误把 app 内部目录识别为 app、越界清单静默 | ❌ 否决 |
| C. **受限 DFS：最深 3 级 + 清单边界剪枝** | `<app>` / `<g1>/<app>` / `<g1>/<g2>/<app>`；含 `wink-app.json` 即 app 边界，禁止向内搜索 | 分组能力足够、边界语义清晰、三处可一致实现、平铺完全兼容 | 同名叶子需消歧；发现器需统一实现 | ✅ **采纳** |
| D. 强制恰好 3 级 | 取消平铺形态，统一 `<g1>/<g2>/<app>` | 结构绝对统一 | 28 个存量 app 全部要迁移，风险与成本极高 | ❌ 否决（后续可再议迁移） |

## 3. 决策结论（Decision）

### D1. 层级上限与清单剪枝

- app 清单 `wink-app.json` 只能出现在 micro-app 根（`wink-micro-app/`、`apps/`、`micro-apps/`，前端兼容名）下深度 1～3 的目录中。
- 发现算法为受控 DFS：**目录一旦包含 `wink-app.json`，即判定为 app 边界，记录该 app 后立即剪枝，不再递归其子目录**；无清单目录仅在 `depth < 3` 时继续下钻；深度 3 无清单即停止。
- 点开头目录（`.xxx`）与符号链接跳过。
- 深度 >3 出现的清单属于配置错误：Python lint/发现链路以 WARN 暴露（不静默、不纳入）。

### D2. app id 与叶子名别名

- **app id = 相对 micro-app 根的 POSIX 路径**：平铺 app id 仍是裸叶子名（`oled_dashboard`），嵌套 app 为 `mcs51/button_led`。id 全局唯一，作为 CLI 参数、wasm 构建目录（`build/wasm/<id>`）、前端 IndexedDB 缓存键。
- CLI 接受裸叶子名作为向后兼容别名：全树唯一则直接命中；多处同名时为**歧义错误**，stderr 列出所有合格 id，要求用户用全 id 消歧。
- 前端记忆还原（`pickPreferredApp`）同样先精确 id、后唯一叶子回退；歧义时不猜测。
- Windows 反斜杠与结尾斜杠在解析入口统一归一化（`normalize_ref`）。

### D3. 三处发现器语义一致

| 组件 | 实现 |
|---|---|
| wink-tools | 新增 `tools/app_discovery.py`（`discover_apps` / `find_app` / `find_app_dir` / `find_overshoot_manifests` / `app_id_for`），所有消费方共享，禁止再各自 `iterdir` 拼一级路径 |
| unisim | 新增 `src/discovery/micro-app-discovery.ts`（`findMicroAppDirs` / `matchAppRef` / `AmbiguousMicroAppError`）作为唯一 walker；CLI 解析器 `embedded-workspace-resolver.ts`、批量一致性 `batch-consistency-scanner.ts` 共享；纯 ESM 的 `browser-runner-node.mjs` 无法导入 TS，内联同构镜像 `findMicroAppDirsNode`（文件内已标注 keep-in-sync） |
| embedded-frontend | Handle 模式递归收集目录句柄；FileList 模式先收集全部清单前缀，再剔除「祖先本身是 app」的内层清单并按最长前缀归属文件 |

特例：存量无清单目录（如 `resource_conflict` 这类仅用于 C 级冲突验证的目录）在 **Python user_surface lint 的 C 扫描**中保留（受限于深度上限、清单边界剪枝、文件去重）；但清单校验、前端 app 列表、unisim 批量扫描只承认带清单的 app。

### D4. 存量不迁移

本期只加能力，现有 28 个平铺 app 原地不动。后续如按家族迁移（`mcs51/`、`vendor/cms8s78xx/` 等），另立实施计划，并同步回归 CMake、测试、文档中的绝对路径。

## 4. 后果与约束（Consequences）

**正面：**

- 平铺 app 的 id / 路径 / CLI / 构建目录 / 前端缓存键逐字节不变，零回归面；
- 分组目录天然在 `build/wasm/<g1>/<app>` 下分层隔离，同名 app 不再撞构建目录；
- 「清单即边界」让 app 内部文件（含误放的 `wink-app.json`）永远不会被识别成第二个 app。

**约束 / 代价：**

- 新增 app 发现入口必须复用 `app_discovery.py`，禁止再写死一级枚举（lint 评审关注项）；
- `wink create app` 只接受 1～3 段的相对 id，拒绝绝对路径、`..`、以及在已有 app 边界内建 app；
- 嵌套 app 的 `common/include` 按「从 app 父目录逐级向上、最近者胜」解析，组级 `common` 可遮蔽根级 `common`；
- 深度 4 及更深的 app 不被发现且会告警——这是有意的 fail-loud，而不是静默支持。
