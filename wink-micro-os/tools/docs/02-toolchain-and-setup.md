# Wink OS 工具链管理与环境搭建指南

Wink Micro OS 采用了一套无侵入的工具链探测与门控管理机制（规约参考 ADR-0029 / ADR-0030）。所有编译与代码生成子命令在执行前，都会通过 **Toolchain Subsystem** 自动完成环境能力校验。

---

## 1. 工具链依赖能力 (Capabilities)

系统共注册了 8 项独立的能力模块，每项能力由对应的 Provider 负责探测与验证：

| 能力 ID (`cap`) | 对应工具/环境 | 涉及的目标平台 | 说明 |
| :--- | :--- | :--- | :--- |
| `python` | Python 3.8+ | 通用 | 运行 `wink.py` 及内部工具链 |
| `jinja2` | PyYAML / Jinja2 | CodeGen | 代码生成模板渲染引擎 |
| `gcc` | Native GCC / MinGW / Clang | Host 仿真 | 编译 Host 运行态与测试单元 |
| `cmake` | CMake 3.20+ | 通用构建 | C/C++ 跨平台构建系统 |
| `make` | Native Make / Ninja | Host / WASM | 驱动底层构建构建器 |
| `emsdk` | Emscripten SDK | WASM 仿真 | 将 C 代码编译为 WASM/JS 模块 |
| `idf` | Espressif ESP-IDF | ESP32 物理硬件 | ESP32 芯片开发 SDK (**从不自动安装**) |
| `node` | Node.js & npm | Web 前端 | 运行 Vue Vite 视效仿真前端 |

---

## 2. 工具链门控机制 (Toolchain Gating - ADR-0029)

### 2.1 门控规则
每次运行 `wink build`, `wink gen`, `wink test`, `wink esp32` 时，工具会自动确定当前命令所需的 **Profile**（如 Host 编译依赖 `gcc+cmake`，ESP32 依赖 `idf+cmake`）。如果缺少所需工具，进程会中断并输出标准错误诊断报告：

```text
[wink] Toolchain gate check failed for profile 'host':
  ✗ gcc: Executable 'gcc' not found in PATH or configured paths
Please install missing tools or run 'wink setup --set gcc=<path>'.
```

### 2.2 紧急逃逸开关 (`--skip-toolchain-check`)
在 CI 或定制环境调试时，若确信依赖已就绪但不想触发 Gate Check，可传入 `--skip-toolchain-check` 开关：
```bash
python tools/wink.py build host --skip-toolchain-check
```

---

## 3. 环境诊断与配置 (`wink doctor` / `wink setup`)

### 3.1 环境诊断 (`wink doctor`)
运行 `wink doctor` 会探测所有已注册能力的健康状况并以可视化彩色树打印：

```bash
python tools/wink.py doctor
```

输出示例：
```text
Wink Micro OS Toolchain Doctor
==================================================
  ✓ python     : Python 3.10.11 (C:\Python310\python.exe)
  ✓ jinja2     : Jinja2 3.1.2 installed
  ✓ gcc        : gcc (x86_64-win32-seh-rev0) 13.1.0
  ✓ cmake      : cmake version 3.28.1
  ✓ emsdk      : emcc (Emscripten GCC-like compiler) 3.1.51
  ✗ idf        : IDF_PATH not set and 'idf.py' not in PATH
```

### 3.2 持久化配置工具路径 (`wink setup`)

你可以通过 `wink setup --set KEY=VALUE` 手动显式绑定工具绝对路径。工具在写入前会自动调用 `provider.detect()` 验证路径有效性，**拒绝写入无效路径**。

* **写入用户级配置** (`~/.wink/tools.json`)：
  ```bash
  python tools/wink.py setup --set emsdk=C:/emsdk
  ```
* **写入工作区级配置** (`<workspace>/.wink/tools.json`)：
  ```bash
  python tools/wink.py setup --set gcc=D:/toolchains/mingw64/bin/gcc.exe --workspace
  ```

---

## 4. 手工预装环境搭建指南 (Pre-installation Guide)

对于全新的开发环境，请参照以下指引完成必要依赖的安装：

### 4.1 Host 仿真环境 (Windows / Linux / macOS)
* **Windows**: 安装 [MSYS2](https://www.msys2.org/) 或 MinGW-w64，确保 `gcc`, `make`, `cmake` 在系统 `PATH` 中。
* **Linux (Ubuntu/Debian)**:
  ```bash
  sudo apt-get update && sudo apt-get install -y build-essential cmake python3 python3-pip
  ```
* **macOS**:
  ```bash
  xcode-select --install
  brew install cmake python
  ```

### 4.2 WASM 仿真环境 (Emscripten)
1. 克隆 EMSdk 仓库：
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ```
2. 安装并激活最新 SDK：
   ```bash
   ./emsdk install latest
   ./emsdk activate latest
   ```
3. 将 `EMSDK` 路径绑定至 Wink：
   ```bash
   python tools/wink.py setup --set emsdk=/path/to/emsdk
   ```

### 4.3 ESP32 硬件开发环境 (ESP-IDF)
> ⚠️ **注意 (ADR-0030)**：Wink 从不自动下载或安装 ESP-IDF，请通过 Espressif 官方方式安装。

1. 推荐使用 [Espressif IDF Installer (EIM)](https://dl.espressif.com/dl/esp-idf/) 进行安装。
2. 安装完成后，确保执行激活脚本或将 `IDF_PATH` 加入环境变量：
   ```bash
   # Windows PowerShell
   . $HOME/esp/v5.1.2/esp-idf/export.ps1
   ```
3. 运行 `python tools/wink.py doctor` 确认 `idf` 标志显示为 `✓`。

---

## 5. 常见问题与故障排查 (Troubleshooting FAQ)

### 5.1 Windows 控制台字符乱码 (CP65001 / Mojibake)
在中文 Windows 环境下，系统控制台默认编码为 CP936，直接打印 `✓`, `✗`, `§` 等字符可能会报 `UnicodeEncodeError`。
* **防护机制**：`wink.py` 内置了 Win32 API 控制台 code page 自动切换 (`CP_UTF8 / 65001`) 机制。
* **手动解决**：如果外部脚本调用时出现乱码，请设置环境变量：
  ```cmd
  set PYTHONUTF8=1
  chcp 65001
  ```

### 5.2 提示 `PyYAML is required` 错误
`wink lint` 依赖 `PyYAML` 模块进行规则 YAML 文件解析。如果未安装，运行 `wink lint` 时会有友好提示。
* **解决方案**：
  ```bash
  python -m pip install -r wink-micro-os/tools/requirements-lint.txt
  ```

### 5.3 工具链包含冲突与 PATH 污染
如果在 Windows 上同时安装了 MSYS2, MinGW, Git Bash 以及 Emscripten/ESP-IDF，PATH 中可能会有同名但不同平台的 `gcc` 或 `make`。
* **解决方案**：推荐使用 `wink setup --set gcc=... --workspace` 显示绑定编译器的完整路径，或者让 `wink.py esp32` 自动净化环境。

