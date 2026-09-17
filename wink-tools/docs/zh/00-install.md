<!--
visibility: public
winkcli-version: ">=0.1.0"
-->
# 00 · 安装与环境就绪

`winkcli` 按目标分流构建。按本文准备对应环境后，相关命令才能正常执行。

---

## 1. 安装 WinkCli

支持双通道安装：

**方式一：winget（Windows 推荐）**

```powershell
winget install WinkAI.WinkCli
```

> 该包正在 winget-pkgs 收录审核中（[PR #434970](https://github.com/microsoft/winget-pkgs/pull/434970)）；若 `winget` 暂未检索到，请使用方式二。

**方式二：GitHub Releases（离线分发 / 免包管理器）**

1. 从 [Releases](https://github.com/wink-cloud-hub/wink-ai-embedded/releases) 下载 `winkcli-v<version>-windows-x86_64.zip`；
2. 解压后将 `winkcli.exe` 所在目录加入 `PATH`。

安装完成后先跑一次环境诊断，确认 `winkcli` 可用：

```bash
winkcli doctor
```

---

## 2. 按目标准备环境

### 2.1 Host 仿真（`build host` / `test`）

| 工具 | 作用 | 约定 |
|---|---|---|
| **Python 3** | 运行 `winkcli`、代码生成与检查器 | ≥ 3.10 |
| **Jinja2** | 代码生成模板渲染 | `pip install "jinja2>=3.1.4"` |
| **PyYAML** | `winkcli lint` 规则解析 | `pip install "PyYAML>=6"` |
| **gcc**（Windows 推荐 MinGW / WinLibs） | Host 编译与单测 | 需在 `PATH`，或用 `winkcli setup --set gcc=...` 绑定 |
| **cmake** | 构建系统 | ≥ 3.15 |
| **make / ninja** | 底层构建器 | Windows 推荐 MinGW Makefiles 或 Ninja |
| **ctest** | 测试运行器 | 随 CMake 安装 |

验证：

```bash
python --version
gcc --version
cmake --version
winkcli doctor
```

### 2.2 Wasm 仿真（`build wasm` / `sim run`）

| 工具 | 作用 | 约定 |
|---|---|---|
| **Emscripten SDK (emsdk)** | 提供 `emcmake` / `emcc` | ≥ 3.1.50，必须已激活 |
| **Node.js ≥ 18 或 Bun** | 运行 UniSim 引擎运行时 | 推荐 Bun；`winkcli` 不会代装 JS 运行时 |

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest

# 将 emsdk 路径绑定给 winkcli
winkcli setup --set emsdk=/path/to/emsdk
```

### 2.2.1 UniSim 引擎运行时（`winkcli sim`）

UniSim 引擎以**签名混淆运行时**的形式内嵌在 `winkcli` 中——无需 npm 安装，也不需要 registry 账号。
首次 `winkcli sim run`（或显式 `winkcli sim install`）时，winkcli 会**离线**把运行时准备到 `~/.wink/sim/`：

- 引擎 tarball + SDK tarball + 依赖闭包随 wheel / `winkcli.exe` 一起分发；
- 安装按 `winkcli.build.json` 的 sha256 逐项校验，staging + 原子替换 + `.lock` 防并发，并保留 `.previous/` 用于回滚；
- 首次使用展示 EULA；接受记录写在 `~/.wink/sim/eula-accepted.json`，安装审计写在 `~/.wink/sim/audit.jsonl`。

```bash
winkcli sim install              # 准备/重装内置运行时（离线）
winkcli sim update               # 走签名在线通道（失败时回退内置 floor）
winkcli sim update --offline     # 强制使用内置 floor
winkcli sim versions             # 列出已安装运行时版本与当前生效版本
winkcli sim rollback             # 回滚到上一个运行时
winkcli sim doctor               # 运行时、hash、EULA 与通道诊断
winkcli sim verify-embed --json  # 校验内置产物 hash（分发包门禁）
```

| 环境变量 | 作用 |
|---|---|
| `WINK_NO_AUTO_INSTALL=1` | 禁止自动准备运行时，仅报告引擎缺失 |
| `WINK_ACCEPT_EULA=1` / `=0` | 预同意 / 拒绝引擎 EULA（CI、企业） |
| `WINK_UNISIM_TARBALL=<path.tgz>` | 开发覆盖：安装本地打包的引擎 tarball |
| `WINK_UNISIM_URL=<base>` | 企业镜像覆盖（目录或 URL，含 `manifest.json`） |
| `WINK_UNISIM_MIRRORS=<base,...>` | 追加镜像候选（按可用性探测排序） |
| `WINK_UNISIM_PUBKEY=<hex>` | 覆盖发布签名公钥（企业自有通道） |
| `WINK_QUIET=1` | 抑制运行时准备提示（等价 `--quiet`） |

> 在线更新使用离线 Ed25519 发布密钥验签 + 逐文件 sha256；manifest 或资产被篡改一律拒绝安装。
> 在公钥指纹内置到 winkcli 之前，在线更新会 fail-closed 并回退内置离线 floor。

### 2.3 ESP32 真机（`esp32`）

| 工具 | 作用 | 约定 |
|---|---|---|
| **ESP-IDF v6.x** | ESP32 官方编译环境 | 通过 Espressif IDE Manager (EIM) 安装；`winkcli` 从不自动安装 |

安装后运行 `winkcli doctor`，确认 `idf` 一项显示 ✓。

---

## 3. 路径配置（可选）

Monorepo 布局下通常可省略。以下情况建议显式绑定：

- 消费解压后的 SDK tarball 或拆分目录布局；
- 工具不在 `PATH`；
- 多套工具链共存需要固定某一套。

```bash
# 写入用户级配置（~/.wink/tools.json）
winkcli setup --set gcc=C:/toolchains/mingw64/bin

# 写入工作区级配置（<workspace>/.wink/tools.json）
winkcli setup --set gcc=D:/toolchains/mingw64/bin --workspace
```

也可使用工作区描述文件 `wink-workspace.json`（`sdk_dir` / `frontend_dir` / `esp32_dir`）替代环境变量。

---

## 4. 命令 ↔ 最小环境速查

| 命令 | 最小环境 |
|---|---|
| `doctor` | Python |
| `setup` | Python |
| `gen app-schema` | Python + Jinja2 |
| `build host` / `test`（host 部分） | Python + Jinja2 + gcc + cmake + make/ninja |
| `build wasm` / `sim run` | Host 基础 + 已激活的 emsdk + Node ≥ 18 / Bun |
| `esp32` | ESP-IDF v6.x（用户自装） |
| `build unisim-plugin` / `dev unisim-plugin` | Node / npm + `embedded-frontend` |
| `lint` | Python + PyYAML |

> 缺少依赖时，`winkcli` 会在执行前中断并输出 collect-all 诊断报告（缺什么、怎么装、怎么配置）。诊断与配置详见 [02 · 工具链配置](./02-toolchain-setup.md)。
