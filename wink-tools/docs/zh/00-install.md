<!--
visibility: public
winkcli-version: ">=0.1.0"
-->
# 00 · 安装与环境就绪

`winkcli` 按目标分流构建。按本文准备对应环境后，相关命令才能正常执行。

---

## 1. 安装 WinkCli

> 🚧 **TODO（安装方式待提供）**：winget / GitHub Releases / 其他渠道的最终安装命令将在此补充。

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

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest

# 将 emsdk 路径绑定给 winkcli
winkcli setup --set emsdk=/path/to/emsdk
```

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
| `build wasm` / `sim run` | Host 基础 + 已激活的 emsdk |
| `esp32` | ESP-IDF v6.x（用户自装） |
| `build unisim-plugin` / `dev unisim-plugin` | Node / npm + `embedded-frontend` |
| `lint` | Python + PyYAML |

> 缺少依赖时，`winkcli` 会在执行前中断并输出 collect-all 诊断报告（缺什么、怎么装、怎么配置）。诊断与配置详见 [02 · 工具链配置](./02-toolchain-setup.md)。
