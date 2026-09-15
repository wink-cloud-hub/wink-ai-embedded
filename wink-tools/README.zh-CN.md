<!--
visibility: public
winkcli-version: ">=0.1.0"
-->
# WinkCli — WinkMicroOS 工具链（`wink-tools`）

`winkcli` 是 WinkMicroOS 的统一构建与仿真调度命令行工具：屏蔽 CMake / Emscripten / ESP-IDF / 交叉编译的底层差异，为 **Host 仿真**、**浏览器 Wasm 仿真**与 **ESP32 真机**提供一致的开发入口。

- 安装（winget）：`winget install WinkAI.WinkCli`
- 安装（GitHub Releases）：从 [Releases](https://github.com/wink-cloud-hub/wink-ai-embedded/releases) 下载 `winkcli-v<version>-windows-x86_64.zip`，解压后将 `winkcli.exe` 加入 `PATH`
- 在线仿真（零安装）：<http://www.wink-cloud.com/simulator/index.html>
- 完整文档：[`docs/zh/`](./docs/zh/)
- 发行说明：由 [GitHub Releases](https://github.com/wink-cloud-hub/wink-ai-embedded/releases) 承载

[English](./README.md) | **简体中文**

---

## 60 秒上手

```bash
# 1. 环境诊断：检查 Python / GCC / CMake / EMSdk / ESP-IDF 等工具链状态
winkcli doctor

# 2. 拉取或绑定 SDK 套件（wink-ai-embedded）
winkcli setup --init
#    或绑定本地已有仓库：
winkcli setup --embedded-dir /path/to/wink-ai-embedded

# 3. 编译 Host 仿真（含代码生成）
winkcli build host --app oled_dashboard

# 4. 运行全量测试矩阵
winkcli test
```

---

## 常用命令速查

```bash
# 代码生成（设备树 / Wasm 导出 / 插件 Schema）
winkcli gen app-schema --app <app_name>
winkcli gen wasm-export --input <header> --output <header>

# 构建目标
winkcli build host --app <app_name>            # Host 原生仿真
winkcli build wasm --app <app_name>            # 浏览器 Wasm 仿真
winkcli build sim --app <app_name>             # 仿真资产（device-tree + wasm）
winkcli esp32 --app devkitc_smoke              # ESP32 固件
winkcli esp32 --app devkitc_smoke -- -p COM3 flash monitor

# 仿真运行（无头 / 有头）
winkcli sim run --app <app_name> --mode headless
winkcli sim run --app <app_name> --scenarios <file> --reporter json

# 环境与配置
winkcli doctor
winkcli setup --init
winkcli setup --set gcc=D:/toolchains/mingw64/bin

# 架构静态检查（CI 可用 SARIF）
winkcli lint --changed
winkcli lint --format sarif --output lint.sarif --strict

# SDK 打包交付
winkcli pack source --out dist/wink-sdk-source
winkcli pack binary --target host --out dist/wink-sdk-host

# 脚手架
winkcli create app <app_id>
winkcli create dal --category sensor --role temperature_sensor
```

---

## 文档导航

| 文档 | 内容 |
|---|---|
| [00 · 安装与环境就绪](./docs/zh/00-install.md) | 工具链安装（TODO）、按目标准备环境、命令 ↔ 最小环境速查 |
| [01 · CLI 命令参考](./docs/zh/01-cli-reference.md) | 命令组手册 + 机器生成的全量命令附录 |
| [02 · 工具链配置](./docs/zh/02-toolchain-setup.md) | `doctor` / `setup`、配置持久化与路径绑定 |
| [03 · 代码生成指南](./docs/zh/03-codegen-guide.md) | `wink-app.json` / 板级定义规范、驱动 YAML 扩展 |
| [04 · 架构检查指南](./docs/zh/04-lint-guide.md) | 规则包、白名单机制、CI/SARIF 集成 |
| [05 · SDK 打包指南](./docs/zh/05-sdk-packaging.md) | Source / Binary SDK 打包与约束 |
| [06 · 故障排查](./docs/zh/06-troubleshooting.md) | 常见错误与解决方案 |
