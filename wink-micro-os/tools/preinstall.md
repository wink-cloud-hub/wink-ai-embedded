# tools — 预安装环境与工具清单

`wink-micro-os/tools/`（尤其是 `wink.py`）按目标分流构建。按下面三类准备环境后，对应命令才能正常执行。

统一入口：

```powershell
python wink-micro-os/tools/wink.py <command> [options]
```

**先跑一次 `doctor`。** Phase A 之后，`wink.py` 的每条非诊断子命令都会先跑 `tools.toolchain.ensure_for(<profile>)`；缺工具时不再落入 CMake 就直接失败并打出一份 collect-all 报告（缺什么、装哪个、怎么设 `WINK_*` / `wink setup --set`）。因此新机器上第一步永远是：

```powershell
python wink-micro-os/tools/wink.py doctor
```

路径类变量（`WINK_*`）在 monorepo 内通常可省略；消费解压后的 SDK tarball 或拆目录布局时才必须设置。也可用根目录或 App 侧的 `wink-workspace.json`（`sdk_dir` / `frontend_dir` / `esp32_dir` / `scripts_dir`）替代，或用 `wink setup --set` 写入 `~/.wink/tools.json`（含绝对路径，`.gitignore` 已排除）。

---

## 1. 基础必须（`gen` / `build host` / `test`）

覆盖：代码生成、本机 Host 仿真编译、Golden + Host 单测。

### 工具

| 工具 | 作用 | 约定 |
|------|------|------|
| **Python 3** | 跑 `wink.py`、codegen、lint；CMake 也 `find_package(Python3 REQUIRED)` | ≥ 3.10（脚本使用 `Path \| None` 等语法） |
| **Jinja2** | `codegen/app_codegen.py` 模板渲染 | `pip install jinja2` |
| **gcc**（MinGW） | Host 编译 / 单测 | 推荐 WinLibs：`winget install --id BrechtSanders.WinLibs.POSIX.UCRT -e` |
| **cmake** ≥ 3.15 | configure + build | WinLibs 自带或独立安装 |
| **mingw32-make** | Windows 上 `wink.py` 固定 `-G "MinGW Makefiles"` | 随 WinLibs |
| **ctest** | `wink.py test` | 随 cmake |

> Wink 不再硬编码任何 WinLibs 安装路径。若 `gcc` / `mingw32-make` / `cmake` 不在 PATH，请用下列任一方式补上：
>
> - 把 WinLibs 的 `mingw64\bin` 加进 `PATH`；或
> - `python wink-micro-os/tools/wink.py setup --set gcc=C:/software/WinLibs/mingw64/bin`（同理 `cmake=...`、`make=...`）；或
> - 导出 `WINK_GCC_PREFIX=<WinLibs>\mingw64\bin`（等 provider 环境变量，见下）。

### 环境变量（可选）

| 变量 | 何时需要 |
|------|----------|
| `WINK_GCC_PREFIX` | 覆盖 host gcc / mingw32-make 的目录（等价于 `wink setup --set gcc=…`） |
| `WINK_PYTHON` | 覆盖 codegen 用的 Python 解释器（绝对文件路径） |
| `WINK_TOOLS_HOME` | 覆盖 `tools_home`：未来自动安装的落地目录 |
| `WINK_SDK_PATH` | 指向解压后的 Source/Binary SDK，而不是仓内 `wink-micro-os/` |
| `PYTHONPATH=wink-micro-os` | 直接跑 `codegen/tests/test_golden.py` 时（`wink.py test` 会自动设置） |

### 验证与对应命令

```powershell
python --version
python -c "import jinja2; print(jinja2.__version__)"
gcc --version
cmake --version

python wink-micro-os/tools/wink.py doctor
python wink-micro-os/tools/wink.py gen --app <name|path>
python wink-micro-os/tools/wink.py build host --app <name|path>
python wink-micro-os/tools/wink.py test
```

---

## 2. Wasm 构建必须（在「基础」之上）

覆盖：`build wasm`，以及可选的 wasm 烟测。

### 工具

| 工具 | 作用 |
|------|------|
| **Emscripten SDK (emsdk)** | 提供 `emcc` / `emcmake` / `emar` |
| **emcmake + cmake** | `wink.py build wasm` → `emcmake cmake …` 再 `cmake --build` |
| **Node.js**（建议） | wasm 烟测 / stub（如 `wink_sim_stub.js`）；非 `build wasm` 硬依赖，但部分 test 路径会用到 |

版本约定：**Emscripten ≥ 3.1.45**（3.x / 4.x / 部分环境下 `emcc --version` 打印 6.x 均满足；6.0.1 ≥ 3.1.45 依然合规。低于 3.1.45 的版本可能与 Binary SDK ABI 不兼容）。Binary SDK 相关约定见 [ADR-0028](../../docs/design/decisions/0028-host-binary-abi-toolchain-contract.md)。

### 环境变量 / 激活

| 项 | 说明 |
|----|------|
| **`EMSDK`** | emsdk 根目录（本机示例：`D:\software\embedded\emsdk`）。`emsdk` provider 会读取这一变量 |
| **先激活当前 shell** | `. $EMSDK\emsdk_env.ps1`，使 `emcc` / `emcmake` 进入 PATH |
| `EMSDK_QUIET=1` | 可选，减少激活输出 |

未激活时 `emcmake` 不在 PATH → `build wasm` 会被 `ensure_for("wasm")` 拒掉并打出提示。

也可用 `wink setup --set emsdk=D:/software/embedded/emsdk` 记住路径，之后再手工 `. $EMSDK\emsdk_env.ps1` 激活即可。

### 验证与对应命令

```powershell
$env:EMSDK = "D:\software\embedded\emsdk"   # 按本机路径修改
. "$env:EMSDK\emsdk_env.ps1"
emcc --version
emcmake --version

python wink-micro-os/tools/wink.py doctor
python wink-micro-os/tools/wink.py build wasm --app <name|path>
```

---

## 3. ESP32 构建必须（相对独立，不依赖宿主 gcc）

覆盖：`wink.py esp32` → 真机固件 build / flash / monitor。

> **ADR-0030：ESP-IDF 永远不会被 Wink 自动安装。** 体积大、私有 Python venv、tools/ 存在其它工程共享点，全部由用户或 Espressif Installer Manager (EIM) 维护；Wink 只做检测 + 提示。

调用链：

1. `esp32_firmware/generate_app_sources.ps1`
2. `scripts/build_esp32.ps1`（清理 PATH 后调用 `idf.py -C esp32_firmware …`）

### 工具

| 工具 | 作用 |
|------|------|
| **ESP-IDF v6.0.1**（本仓锁定） | `idf.py` + xtensa 工具链 |
| **PowerShell** | 执行上述 `.ps1`（Windows 必选） |
| **Python（IDF 自带 venv）** | IDF 构建用，可与系统 Python 不同 |
| **串口驱动** | 仅 flash / monitor 需要（如 `COM3`） |

### 环境变量

`wink.py esp32` 经 `ensure_for("esp32")` 注入 `IDF_PATH`（以及探测到的 `IDF_TOOLS_PATH` 等），并强制 `PYTHONUTF8=1`。  
`scripts/build_esp32.ps1` **不再硬编码** Espressif 绝对路径：若当前进程已有有效 `IDF_PATH` **且** `idf.py` 在 PATH 上，则直接使用；否则自动尝试 source EIM profile（`C:\Espressif\tools\Microsoft.v6*.PowerShell_profile.ps1`）或 `$IDF_PATH\export.ps1`。手工跑 `idf.py` 时仍建议先激活 EIM profile。

| 变量 | 说明 / 本机脚本典型值 |
|------|----------------------|
| `IDF_PATH` | 如 `D:\software\embedded\esp\v6.0.1\esp-idf`；`idf` provider 读取此项 |
| `IDF_TOOLS_PATH` | 如 `C:\Espressif\tools`（EIM 探测时由 provider 转发） |
| `IDF_PYTHON_ENV_PATH` | 如 `C:\Espressif\tools\python\v6.0.1\venv` |
| `ESP_IDF_VERSION` | `6.0.1` |
| `PYTHONUTF8=1` | 避免中文注释 / 控制台乱码 |
| `PYTHONIOENCODING=utf-8` | 同上 |
| `WINK_APP_DIR` | `wink.py` 注入给 CMake（App 目录） |
| `WINK_SDK_PATH` | `wink.py` 注入给 CMake（SDK 根） |
| `WINK_ESP32_PATH` | 指向 `esp32_firmware`（非 monorepo 时） |
| `WINK_SCRIPTS_PATH` | 指向含 `build_esp32.ps1` 的 `scripts/`（非 monorepo 时） |

也可：

```powershell
python wink-micro-os/tools/wink.py setup --set esp32_dir=D:/path/to/esp32_firmware
# → 写入工作区 wink-workspace.json（不是 .wink/tools.json）
python wink-micro-os/tools/wink.py setup --set idf=D:/software/embedded/esp/v6.0.1/esp-idf
```

> 注意：仅设置 `IDF_PATH` 而不激活 shell 时，`build_esp32.ps1` 会继续尝试 EIM profile，以便把 `idf.py` 放进 PATH。

> `build_esp32.ps1` 会清掉 `EMSDK` / `MSYSTEM` 等，避免 MinGW / emsdk 污染 IDF PATH（ESP-IDF v6 不再支持从 MSYS/MinGW 直接调用）。

手工激活等价于：

```powershell
. 'C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1'
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
idf.py --version
```

### 验证与对应命令

```powershell
python wink-micro-os/tools/wink.py doctor
python wink-micro-os/tools/wink.py esp32 --app <name|path> build
python wink-micro-os/tools/wink.py esp32 --app <name|path> -- -p COM3 flash monitor
```

---

## 4. 诊断与配置：`doctor` / `setup`

| 命令 | 作用 |
|------|------|
| `wink doctor` | 走 `ensure_for("doctor")`，对每个已注册的 capability（gcc / cmake / make / python / jinja2 / emsdk / idf / node / powershell 等）跑一次 `detect()` 并打印 collect-all 报告（不做半路 fail-fast） |
| `wink setup --set KEY=PATH` | 先经 `provider.detect()` 验证 PATH，再把 `paths[KEY]=PATH` 写入 `~/.wink/tools.json`（schema v1） |
| `wink setup --set KEY=PATH --workspace` | 同上，但写到 `<workspace>/.wink/tools.json`；用于同一台机器上给不同工作区绑不同工具版本 |
| `wink setup --install CAP` | Phase B 预留：未来自动装 host 侧可安装的工具（如 emsdk）；当前只打提示，绝不触碰 ESP-IDF |
| `wink <cmd> --skip-toolchain-check` | 逃生舱：跳过 `ensure_for` 直接进 handler，会打红色 warning。日常不要用 |

`tools.json` 例见 `wink-micro-os/tools/toolchain/tools.json.example`；两级合并顺序为 user (`~/.wink/tools.json`) → workspace (`<ws>/.wink/tools.json`)，per-key 覆盖。

---

## 速查：命令 ↔ 最小环境

| 命令 | 最小集合 |
|------|----------|
| `doctor` | Python + `tools/toolchain/` |
| `setup` | Python（不检查其它工具） |
| `gen` | Python + Jinja2 |
| `build host` / `test`（host 部分） | Python + Jinja2 + gcc + cmake + mingw32-make |
| `build wasm` | 上表 Host 基础 + **已激活的 emsdk**（`emcmake` / `emcc`） |
| `esp32` | PowerShell + **ESP-IDF 6.0.1**（用户自装，永不自动安装）+ `esp32_firmware` + `scripts/build_esp32.ps1` |
| `web` | Node / npm + `embedded-frontend`（与固件/仿真编译无关） |

更完整的 CLI 用法与 SDK 打包消费说明见同目录 [README.md](./README.md)。
