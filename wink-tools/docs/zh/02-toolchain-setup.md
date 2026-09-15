<!--
visibility: public
winkcli-version: ">=0.1.0"
-->
# 02 · 工具链配置

`winkcli` 在执行构建、生成、测试等命令前会自动校验所需工具链是否就绪。本文介绍能力清单、诊断与配置方式。

---

## 1. 依赖能力清单

| 能力 | 对应工具 / 环境 | 涉及目标 | 说明 |
|---|---|---|---|
| `python` | Python 3.10+ | 通用 | 运行 `winkcli` 及内部工具链 |
| `jinja2` | Jinja2 | 代码生成 | 模板渲染引擎 |
| `gcc` | Native GCC / MinGW / Clang | Host 仿真 | 编译 Host 运行态与测试单元 |
| `cmake` | CMake | 通用构建 | 跨平台构建系统 |
| `make` | Make / Ninja | Host / Wasm | 驱动底层构建器 |
| `emsdk` | Emscripten SDK | Wasm 仿真 | 编译 Wasm/JS 模块 |
| `idf` | Espressif ESP-IDF | ESP32 真机 | **从不自动安装**，需用户通过 EIM 安装 |
| `node` | Node.js & npm | Web 前端 | 运行外设插件与前端视窗 |

---

## 2. 环境门控行为

执行 `build` / `gen` / `test` / `esp32` / `sim` 等命令时，`winkcli` 会先确定该命令所需的工具集合：

- 全部就绪 → 正常执行；
- 存在缺失 → **中断并输出 collect-all 报告**，一次性列出所有缺失项及修复方式：

```text
[winkcli] Toolchain gate check failed for profile 'host':
  ✗ gcc: Executable 'gcc' not found in PATH or configured paths
Please install missing tools or run 'winkcli setup --set gcc=<path>'.
```

紧急情况下可用逃生舱（会输出警告，日常不要使用）：

```bash
winkcli build host --skip-toolchain-check
```

---

## 3. 环境诊断（`winkcli doctor`）

```bash
winkcli doctor
```

输出示例：

```text
Wink Micro OS Toolchain Doctor
==================================================
  ✓ python     : Python 3.10.11
  ✓ jinja2     : Jinja2 3.1.2 installed
  ✓ gcc        : gcc 13.1.0
  ✓ cmake      : cmake version 3.28.1
  ✓ emsdk      : emcc 3.1.51
  ✗ idf        : IDF_PATH not set and 'idf.py' not in PATH
```

---

## 4. 路径绑定与持久化（`winkcli setup`）

`winkcli setup --set KEY=PATH` 会**先验证路径有效性，再写入配置**（拒绝写入无效路径）：

```bash
# 用户级配置：~/.wink/tools.json
winkcli setup --set emsdk=C:/emsdk

# 工作区级配置：<workspace>/.wink/tools.json
winkcli setup --set gcc=D:/toolchains/mingw64/bin/gcc.exe --workspace

# 初始化 SDK 套件
winkcli setup --init                                   # 浅克隆到 ~/.wink/sdk/wink-ai-embedded
winkcli setup --embedded-dir /path/to/wink-ai-embedded # 绑定本地仓库
```

配置优先级（后者覆盖前者）：环境变量 → 工作区配置 → 用户级配置 → 出厂默认探测。

> 环境变量与工作区描述文件 `wink-workspace.json` 的用法见 [00 · 安装与环境就绪](./00-install.md)。
> 遇到编码、PyYAML、PATH 冲突等问题见 [06 · 故障排查](./06-troubleshooting.md)。
