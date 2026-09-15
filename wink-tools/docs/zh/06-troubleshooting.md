<!--
visibility: public
winkcli-version: ">=0.1.0"
-->
# 06 · 故障排查

> 遇到问题时，第一步永远是：`winkcli doctor`。它会一次性列出所有缺失/异常的依赖与修复建议。

---

## 1. Windows 控制台字符乱码

中文 Windows 控制台默认编码为 CP936，打印 `✓` / `✗` 等字符可能报 `UnicodeEncodeError` 或显示乱码。

- `winkcli` 内置了控制台代码页自动切换（UTF-8 / 65001）；
- 外部脚本调用时如仍异常，可显式设置：

```cmd
set PYTHONUTF8=1
chcp 65001
```

## 2. 提示 `PyYAML is required`

`winkcli lint` 依赖 PyYAML 解析规则文件：

```bash
python -m pip install "PyYAML>=6"
```

## 3. 工具链 PATH 污染与同名冲突

同时安装 MSYS2 / MinGW / Git Bash / Emscripten / ESP-IDF 时，`PATH` 中可能存在多个 `gcc` 或 `make`。

```bash
# 显式绑定工具完整路径（写入工作区配置）
winkcli setup --set gcc=D:/toolchains/mingw64/bin/gcc.exe --workspace
winkcli setup --set emsdk=C:/emsdk --workspace
```

## 4. `emsdk` 显示未激活

`emcmake` / `emcc` 需要在激活环境中才可用：

```bash
./emsdk install latest
./emsdk activate latest
# Windows PowerShell 下激活：
#   .\emsdk_env.ps1
# Linux/macOS：
#   source ./emsdk_env.sh
```

## 5. `idf` 未检测到

ESP-IDF 需通过 Espressif IDE Manager (EIM) 安装，并确保导出了环境（或将 `IDF_PATH` 加入环境变量）。`winkcli` 不会自动安装 ESP-IDF。

## 6. 环境隔离 / 换机后的路径失效

配置中记录了绝对路径，换机或移动 SDK 目录后失效：

```bash
winkcli doctor
winkcli setup --embedded-dir /new/path/to/wink-ai-embedded
winkcli setup --set gcc=/new/path/to/mingw64/bin
```

更完整的环境准备清单见 [00 · 安装与环境就绪](./00-install.md)；门控行为与配置优先级见 [02 · 工具链配置](./02-toolchain-setup.md)。
