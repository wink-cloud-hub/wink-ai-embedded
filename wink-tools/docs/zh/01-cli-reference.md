<!--
visibility: public
winkcli-version: ">=0.1.0"
-->
# 01 · CLI 命令参考

`winkcli` 采用 **命令组（`<group> <verb>`）** 分层组织。本文给出各组核心用法；**全量命令、参数与默认值**见文末机器生成的「附录 A」。

## 全局选项

| 选项 | 说明 |
|---|---|
| `-h`, `--help` | 查看命令帮助 |
| `--json` | 输出结构化 JSON（适合脚本 / CI 解析） |
| `--skip-toolchain-check` | 逃生舱：跳过环境门控（仅紧急调试，会打警告） |

---

## `winkcli build` — 构建目标

| 子命令 | 说明 |
|---|---|
| `build host` | 编译 Host 原生仿真与测试单元 |
| `build wasm` | 编译浏览器 Wasm 仿真模块 |
| `build sim` | 构建仿真资产（device-tree + wasm 包），`--all` 可批量 |
| `build unisim-plugin` | 打包前端外设插件（Vite 预构建） |

```bash
winkcli build host --app oled_dashboard
winkcli build wasm --app avoidance_car
winkcli build sim --app oled_dashboard
```

## `winkcli esp32` — ESP32 真机

```bash
winkcli esp32 --app devkitc_smoke
winkcli esp32 --app devkitc_smoke -- -p COM3 flash monitor   # '--' 后的参数透传给 idf.py
```

## `winkcli sim` — 仿真运行与管理

| 子命令 | 说明 |
|---|---|
| `sim run` | 运行仿真：`--mode headless`（无头）/ `--headed`；`--scenarios` 指定场景；`--reporter json` 输出结构化结果 |
| `sim consistency` | 一致性检查（可指定 `--scenario` / `--app` / `--all`） |
| `sim doctor` / `sim install` / `sim update` | 仿真引擎的诊断、安装与更新 |

```bash
winkcli sim run --app mcs51_button_led --mode headless
winkcli sim run --app mcs51_button_led --scenarios unisim-scenarios/button-led.scenario.json --reporter json
```

## `winkcli gen` — 代码与描述文件生成

| 子命令 | 说明 |
|---|---|
| `gen app-schema` | 从 `wink-app.json` 生成设备树 / 前端资产（`--out` 指定输出） |
| `gen wasm-export` | 扫描导出 API，生成 Wasm 导出符号配置 |
| `gen unisim-plugin-schema` | 生成外设插件定义 JSON Schema |

```bash
winkcli gen app-schema --app oled_dashboard
```

## `winkcli create` — 脚手架

```bash
winkcli create app <app_id>                                  # 新建应用
winkcli create dal --category sensor --role temperature_sensor   # 新建 DAL 驱动骨架
```

## `winkcli lint` — 静态架构治理

```bash
winkcli lint --changed                       # 增量检查改动文件
winkcli lint --strict                        # Warning 升级为 Error（CI 推荐）
winkcli lint --pack layering                 # 只跑指定规则包
winkcli lint --format sarif --output lint.sarif
winkcli lint --explain <RULE_ID>             # 查看规则说明与修复建议
```

详见 [04 · 架构检查指南](./04-lint-guide.md)。

## `winkcli pack` — SDK 打包

```bash
winkcli pack source --out dist/wink-sdk-source
winkcli pack binary --target host --out dist/wink-sdk-host
```

详见 [05 · SDK 打包指南](./05-sdk-packaging.md)。

## 工程与运维

| 命令 | 说明 |
|---|---|
| `winkcli doctor` | 探测全部工具链能力并输出报告 |
| `winkcli setup` | 初始化 / 绑定 SDK 与工具路径（`--init` / `--embedded-dir` / `--set`） |
| `winkcli test` | 运行全量测试矩阵（`--sanitize` / `--asan` / `--full`） |
| `winkcli schema migrate` | 旧版描述文件迁移 |
| `winkcli upgrade`（别名 `update`） | 自更新（`--winget` / `--pip` / `--direct`） |
| `winkcli auth` | 云能力登录状态（`login` / `logout` / `status`） |
| `winkcli usage` | 用量查询与重置（`history` / `reset`） |
| `winkcli i18n scan` | 扫描仓库中的中文硬编码字符串 |
| `winkcli completion` | 生成 Shell 自动补全脚本 |
| `winkcli web` | 启动本地 Web 视窗（`--port`） |

---

<!-- BEGIN AUTO-GENERATED: CLI-TREE -->
## Appendix A - Full Command Reference (auto-generated, do not edit)

> winkcli `v0.1.0` · snapshot schema 1 · generated 2026-09-15T06:13:16Z

### Global flags

- `--skip-toolchain-check` · Bypass toolchain gating (emergency escape hatch; prints WARN).
- `--json` · Output structured JSON telemetry envelope to stdout.

### `winkcli auth`

Inspect authentication status and account mode

#### `winkcli auth login`

Log in to Wink Cloud (reserved for cloud release)

#### `winkcli auth logout`

Log out of Wink Cloud account

#### `winkcli auth status`

Show current authentication mode and entitlements

### `winkcli build`

Build Host or WASM simulators

#### `winkcli build host`

Build host simulator binary for an app

- `--app` · default: `oled_dashboard` · App name in samples/ or path to app directory
- `--clean` · Clean the build directory before building
- `--sdk-mode` · choices: `source`, `binary` · SDK mode: 'source' (build from source) or 'binary' (use precompiled .a).

#### `winkcli build sim`

Build full unisim simulation assets (device-tree.json + wasm bundle)

- `--app` · default: `oled_dashboard` · App name in samples/ or path to app directory
- `--all` · Build simulation assets for all discovered apps
- `--out` · Destination directory (default: <app_dir>/unisim-assets)
- `--clean` · Clean the build directory before building
- `--sdk-mode` · choices: `source`, `binary` · SDK mode: 'source' (build from source) or 'binary' (use precompiled .a).

#### `winkcli build sim-assets`

Build full unisim simulation assets (device-tree.json + wasm bundle)

- `--app` · default: `oled_dashboard` · App name in samples/ or path to app directory
- `--all` · Build simulation assets for all discovered apps
- `--out` · Destination directory (default: <app_dir>/unisim-assets)
- `--clean` · Clean the build directory before building
- `--sdk-mode` · choices: `source`, `binary` · SDK mode: 'source' (build from source) or 'binary' (use precompiled .a).

#### `winkcli build unisim-plugin`

Pre-bundle a peripheral plugin (simulation.js + frontend.js)

- `--path` · **required** · Peripheral project root
- `--out` · Output directory
- `--mode` · choices: `production`, `development` · default: `production` · NODE_ENV forwarded to Vite

#### `winkcli build wasm`

Build WASM simulator binary for an app

- `--app` · default: `oled_dashboard` · App name in samples/ or path to app directory
- `--clean` · Clean the build directory before building
- `--sdk-mode` · choices: `source`, `binary` · SDK mode: 'source' (build from source) or 'binary' (use precompiled .a).

### `winkcli completion`

Generate Shell autocompletion script

- `shell` · choices: `bash`, `powershell`, `zsh` · **required** · Shell type for completion script generation

### `winkcli create`

Create C language DAL driver or App scaffolding

#### `winkcli create app`

Scaffold a new Wink Micro App directory

- `app_name` · **required** · Name of application directory

#### `winkcli create dal`

Scaffold DAL .h/.c + codegen driver plugin

- `type` · **required** · Driver type identifier (e.g., hmc5883l)
- `--category` · choices: `sensor`, `actuator`, `display`, `storage`, `comm`, `input`, `output` · **required** · DAL subcategory directory under dal/include/ and dal/src/
- `--actuator`, `--is-actuator` · Driver represents an actuator (safe_off_fn required)
- `--role` · Optional default_role; also scaffolds roles/<role>.yaml
- `--pin-field` · default: `[]` · Config pin field name (repeatable; default: gpio_pin)
- `--force` · Overwrite existing scaffold files

### `winkcli dev`

Watch + HMR broadcast for peripheral plugins (Phase 2)

#### `winkcli dev unisim-plugin`

Watch + HMR broadcast for a peripheral plugin (Phase 2)

- `--path` · **required** · Peripheral project root
- `--port` <int> · type `int` · default: `5173` · Vite HMR port

### `winkcli doctor`

Probe every registered toolchain capability

### `winkcli esp32`

Build, flash, or monitor ESP32 firmware

- `--app` · default: `devkitc_smoke` · App name in samples/ or path to app directory
- `idf_args` · default: `['build']` · Arguments forwarded to idf.py

### `winkcli gen`

Run device tree & config macro codegen

- `--app` · default: `oled_dashboard` · App name in samples/ or path to app directory

#### `winkcli gen app-schema`

Export micro-app DeviceTree JSON for frontend simulation

- `--app` · default: `oled_dashboard` · App name in samples/ or path to app directory
- `--out` · Output file path (default: stdout)
- `--peripherals-dir` · Path to peripherals directory to dynamically scan manifests
- `--manifest-index` · Optional path to static manifest_index.json

#### `winkcli gen unisim-plugin-schema`

Regenerate schema.json from manifest.json for peripheral plugin

- `--path` · **required** · Peripheral project root
- `--out` · Output schema.json path
- `--no-overwrite` · Fail if schema.json already exists

#### `winkcli gen wasm-export`

Generate CMake WASM export configuration header

- `--input` · **required** · Input exported_runtime_functions.json
- `--output` · **required** · Output .cmake header path

### `winkcli i18n`

Scan repository for unextracted i18n Chinese text strings

#### `winkcli i18n scan`

Scan C/C++/TS/YAML files for raw i18n strings

- `--path` · Specific directory to scan
- `--strict` · Fail with non-zero exit code if unextracted strings found
- `--all` · Run all checks except logic invariant

### `winkcli lint`

Run YAML layer/API/Arduino lints (ADR-0043)

- `--root` · SDK root to scan (default: wink-micro-os/)
- `--config` · default: `[]` · Extra YAML config path (repeatable)
- `--pack` · Rule pack id to run
- `--lint-paths` · default: `[]` · Extra external lint pack directory (repeatable)
- `--rule` · Only report findings for this rule id
- `--paths` · Incremental scan: only these files
- `--changed` · Derive --paths from git diff
- `--format` · choices: `text`, `json`, `sarif` · default: `text` · Output format
- `--output` · Write report to FILE instead of stdout
- `--strict` · Treat warnings as failures
- `--explain` <RULE_ID> · Print rule explanation and exit 0
- `--report-allowlist` · Report allowlisted / expiring allow_paths entries
- `--baseline` · Optional baseline file for fingerprint diff. Known issues in baseline are suppressed. (Do NOT pass --baseline when generating/updating a baseline file)
- `--today` · Override today for until expiry (YYYY-MM-DD)

### `winkcli pack`

Package Wink Micro OS SDK (source or binary)

#### `winkcli pack binary`

Pack precompiled binary SDK release

- `--out` · default: `build/dist` · Output directory for binary SDK
- `--target` · choices: `host`, `wasm`, `esp32`, `all` · default: `host` · Target platform(s) to include
- `--skip-build` · Skip cmake configure/build (use existing build-dirs)

#### `winkcli pack source`

Pack source SDK release tarball

- `--out` · default: `build/dist` · Output directory for tarball

### `winkcli schema`

Manage and migrate driver YAML schemas

#### `winkcli schema migrate`

Write *.migrated.yaml sidecar for Schema 1.1

- `paths` · **required** · Driver or role YAML files to migrate (writes *.migrated.yaml)
- `--force` · Overwrite existing *.migrated.yaml sidecars

### `winkcli setup`

Inspect or edit ~/.wink/tools.json

- `--init`, `--clone` · Auto-clone the official wink-ai-embedded SDK suite to standard location.
- `--git-url` <URL> · default: `https://github.com/wink-ai/wink-ai-embedded.git` · Git repository URL for SDK clone (default: https://github.com/wink-ai/wink-ai-embedded.git).
- `--embedded-dir` <PATH> · Link to an existing local wink-ai-embedded directory.
- `--wizard` · Launch interactive setup wizard.
- `--non-interactive` · Run non-interactively (requires --init or --embedded-dir).
- `--set` <KEY=VALUE> · Validate and write paths[KEY]=VALUE.
- `--workspace` · With --set, write to <workspace>/.wink/tools.json instead of user config.

### `winkcli sim`

Manage, build, and run the Wink Unified Simulation Engine (winksim)

#### `winkcli sim consistency`

Verify dual-run consistency across Headless Direct and Worker Twin engines

- `--app` · Target application path or name (e.g. vendor_cms8s78xx_v202_led_4com_8seg)
- `--all` · Batch verify consistency across all micro-apps in workspace
- `--filter` · Filter pattern for app names when running with --all (e.g. mcs51_*)
- `--scenario` · Explicit path to a .scenario.json file
- `--no-build` · default: `True` · Do not auto-build simulation assets if missing
- `-v`, `--verbose` · Print detailed execution logs

#### `winkcli sim doctor`

Check simulation engine, JS runtime, and plugins health

#### `winkcli sim install`

Install or reinstall compatible winksim engine into ~/.wink/sim/

#### `winkcli sim run`

Auto-build WASM simulation assets and launch simulation

- `--app` · Target application path or name (default: oled_dashboard)
- `--mode` · choices: `headless`, `browser` · Simulation mode: headless (default) or browser
- `--record` · Record WebM video & animated GIF (automatically switches to browser mode)
- `--headed` · Run browser simulator with visible Chromium window
- `--scenarios` · Scenario directory or spec file path
- `--artifacts` · Artifacts output directory (default: ./artifacts)
- `--filter` · Filter scenarios by filename keyword
- `--grep` · Filter scenarios by glob pattern
- `--tags` · Filter scenarios by comma-separated tags
- `--reporter` · choices: `spec`, `json`, `junit` · Report format: spec (default), json, or junit
- `-v`, `--verbose` · Print detailed execution logs
- `--out` · Custom output directory for simulation assets
- `--wasm-dir` · Custom WASM build directory search path
- `--url` · Browser target page URL (default: http://localhost:5173 or http://127.0.0.1:5174)
- `--cdp` · Connect to existing browser / Tauri window via CDP URL (e.g. http://127.0.0.1:9222)
- `--channel` · Browser channel for browser simulation (e.g. chrome, msedge, chromium)

#### `winkcli sim update`

Update winksim simulation engine to latest version

### `winkcli test`

Run Python, C unit tests, sanitizer pass matrix, and lints

- `--clean` · Clean test build directories before running tests
- `--detailed` · Print verbose ctest output (-V)
- `--sanitize` · Enable UBSan sanitize matrix pass
- `--asan` · Enable ASan matrix pass
- `--full` · Run full test matrix
- `--with-wasm` · Run optional WASM compilation check

### `winkcli update`

Alias for 'winkcli upgrade'

- `--check` · Only check for available updates without installing
- `--winget` · Force update via Windows Package Manager (winget upgrade WinkAI.WinkCLI)
- `--pip` · Force update via Python Pip (pip install --upgrade winkcli)
- `--direct` · Force direct download from GitHub Releases

### `winkcli upgrade`

Self-update and upgrade winkcli to the latest version

- `--check` · Only check for available updates without installing
- `--winget` · Force update via Windows Package Manager (winget upgrade WinkAI.WinkCLI)
- `--pip` · Force update via Python Pip (pip install --upgrade winkcli)
- `--direct` · Force direct download from GitHub Releases

### `winkcli usage`

View simulation usage quotas and security storage status

#### `winkcli usage history`

View usage history and events

- `--days` <int> · type `int` · default: `14` · Number of days to inspect (default: 14)

#### `winkcli usage reset`

Emergency reset of usage counters

- `--yes` · Confirm emergency reset without interactive prompt

### `winkcli web`

Start Vue Vite frontend web server

- `--port` <int> · type `int` · default: `5173` · Vite server port (default: 5173)

<!-- END AUTO-GENERATED: CLI-TREE -->
