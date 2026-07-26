# Wink OS SDK 工具链套件 (`tools/`)

`wink-micro-os/tools/` 包含 Wink Micro OS 的核心 SDK CLI 网关、代码生成引擎、工具链探测门控、静态架构治理检查器及 SDK 打包工具。

> 💡 **快速诊断与启动**：在新机器或新开发环境中，第一步永远是运行 `python tools/wink.py doctor`。

---

## 📚 模块化详细文档导航 ([tools/docs/](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/docs))

我们将复杂的工具链拆分为以下 5 个模块化使用指南，点击链接可直达对应文档：

1. 📖 **[01-cli-overview.md](./docs/01-cli-overview.md)** — **Wink CLI 核心命令手册**
   - 详细介绍 `wink gen`, `wink build`, `wink esp32`, `wink web`, `wink test` 的命令行参数与典型用法。
2. 🛠️ **[02-toolchain-and-setup.md](./docs/02-toolchain-and-setup.md)** — **工具链管理与环境搭建指南**
   - 工具链探测门控机制 (ADR-0029/0030)、`wink setup` / `wink doctor` 命令与全局/工作区 `tools.json` 配置。
3. ⚙️ **[03-codegen-guide.md](./docs/03-codegen-guide.md)** — **设备树代码生成引擎指南**
   - `wink-app.json` / `board.json` 描述文件规范、C 语言与 Arduino 绑定生成原理及 Driver 外设插件开发。
4. 🛡️ **[04-architecture-linter.md](./docs/04-architecture-linter.md)** — **静态架构治理检查器指南**
   - (ADR-0043) 架构规约检查矩阵（分层越级、API 规范、内存分配规则）、白名单（`allow_paths`）与 CI/CD 集成。
5. 📦 **[05-sdk-packaging.md](./docs/05-sdk-packaging.md)** — **SDK 发布与打包工具指南**
   - (ADR-0028) Source SDK 与 Binary SDK 打包脚本使用说明及天花板约束校验（Pack Ceilings）。

---

## ⚡ 1 分钟 Quick Start

```bash
# 1. 检查当前环境工具链健康状况
python tools/wink.py doctor

# 2. 为 oled_dashboard 应用生成设备树与 C 代码
python tools/wink.py gen --app oled_dashboard

# 3. 编译并在 Host 上运行仿真
python tools/wink.py build host --app oled_dashboard

# 4. 运行全量架构检查与测试矩阵
python tools/wink.py test
```

---

## 🗂️ 工具链目录架构概览

| 路径 | 核心功能 | 关联子文档 |
| :--- | :--- | :--- |
| `wink.py` | 统一构建与调度 CLI 网关 | [docs/01-cli-overview.md](./docs/01-cli-overview.md) |
| `toolchain/` | 依赖探测、配置文件 (`tools.json`) 与门控系统 | [docs/02-toolchain-and-setup.md](./docs/02-toolchain-and-setup.md) |
| `codegen/` | 设备树渲染、`wink_config.h` 参数及外设驱动插件 | [docs/03-codegen-guide.md](./docs/03-codegen-guide.md) |
| `lint/` | 静态架构规则检查引擎与白名单管理 | [docs/04-architecture-linter.md](./docs/04-architecture-linter.md) |
| `pack_sdk_*.py` | Source SDK / Binary SDK 自动打包脚本 | [docs/05-sdk-packaging.md](./docs/05-sdk-packaging.md) |
| `esp32/` | ESP-IDF 构建环境隔离与代码生成胶水 | [docs/01-cli-overview.md](./docs/01-cli-overview.md) |
| `docs/` | 工具链拆分子文档目录 | —— |

---

## 🛠️ CLI 常用速查表 (Cheatsheet)

```bash
# 生成代码
python tools/wink.py gen --app <app_name>

# 编译 Host/WASM
python tools/wink.py build [host|wasm] --app <app_name>

# ESP32 烧录与串口监视 (推荐使用 '--' 透传参数)
python tools/wink.py esp32 --app devkitc_smoke -- -p COM3 flash monitor

# 配置工具链路径
python tools/wink.py setup --set gcc=/path/to/gcc --workspace

# 架构静态检查
python tools/wink.py lint --changed
```
