---
name: burn-firmware-esp32
description: 用于 Wink-Micro-OS ESP32 固件的自动扫描、构建、编译、烧录与清理。当用户输入有关 ESP32 固件构建、编译、烧写、清理等命令（或直接提供 App 名称如 devkitc_smoke, avoidance_car, oled_dashboard, resource_conflict 作为参数）时触发。本 Skill 会将紧跟后面的第一个参数解析为 DWINK_APP 名字，并指导进行正确的环境激活和 CMake/idf.py 构建流程。
---

# ESP32 固件构建与烧录 Skill

本 Skill 旨在协助用户对 `wink-micro-os/samples/` 下的业务应用进行 ESP32 固件编译、烧录及清理。

## 🎯 核心逻辑

用户输入有关编译/烧录的指令时，**紧跟后面的第一个参数即为 `DWINK_APP` 的名字**（如 `avoidance_car`）。
你需要提取该 App 名字，并按以下流程在 Windows PowerShell 下执行相应构建和烧录操作。

---

## 🚀 执行步骤与命令链

> [!IMPORTANT]
> Python 迁移后（2026-07-13），推荐通过统一入口 `python wink-micro-os/tools/wink.py esp32` 驱动 ESP32 构建。它会自动激活 IDF 环境（hot PATH → EIM profile → export.ps1 fallback），并调用 Python 版本的 generate/build 管线；用户**不再需要**在 shell 里手工 dot-source EIM profile。所有命令都从仓库根目录执行。

### 1. 编译指定 App
- **目标 App**: 提取自用户输入的第一个参数 `$AppName`（若未指定，默认使用 `devkitc_smoke`）。
- **执行命令**（从仓库根目录，无需先激活 IDF）:
  ```powershell
  python wink-micro-os/tools/wink.py esp32 --app $AppName build
  ```
  wink.py 会自动调用 `tools/esp32/activate.py` 采集 IDF 环境、`tools/esp32/generate_app_sources.py`（CMake configure 阶段）扫描源文件、`tools/esp32/build.py` 剥离 PATH 污染后调 `idf.py`。

### 2. 清理并重编 (Fullclean)
在更换 App、修改 CMake 脚本或遇到奇怪的链接错误时，需要先执行 `fullclean`：
  ```powershell
  python wink-micro-os/tools/wink.py esp32 --app $AppName -- fullclean
  python wink-micro-os/tools/wink.py esp32 --app $AppName build
  ```
  `--` 后面的所有参数会透传给 `idf.py`。

### 3. 烧录与串口监视
将固件烧录到指定串口（例如 `COM3`，请优先从历史命令或用户提示中确认串口号，默认为 `COM3`）：
  ```powershell
  python wink-micro-os/tools/wink.py esp32 --app $AppName -- -p COM3 flash monitor
  ```
  *(注：串口监视器退出快捷键为 `Ctrl + ]`；`-p COM3` 是 Windows 上的串口号，Linux/macOS 请改成 `/dev/ttyUSB0` 等。)*

### 4. 调试脚本运行 (仅生成 CMake 片段，不进行完整编译)
若只需运行扫描脚本（例如排查为什么某个源文件没被编到），从仓库根目录跑：
  ```powershell
  python wink-micro-os/tools/esp32/generate_app_sources.py --app-name $AppName --esp32-firmware-dir esp32_firmware
  ```

---

## 📋 可用 App 列表参考

| App 名称 | 说明 |
|---|---|
| `devkitc_smoke` | DevKitC 冒烟测试（GPIO / PWM / I2C / 双核 / 看门狗） - **默认值** |
| `avoidance_car` | 避障小车（超声波 + 舵机） |
| `oled_dashboard` | OLED 仪表盘（按键 + LED + SSD1306） |
| `resource_conflict` | Track A 资源占用冲突负例样本（用于验证 DAL init 的 claim 拒接） |

---

## 💡 常见构建规范与排错

1. **零改动特性**：
   业务应用代码存放在 `wink-micro-os/samples/$AppName/`。更换 App 或增删源文件时，`esp32_firmware` 目录下的源码不需要做任何修改。CMake configure 阶段会自动调用 `tools/esp32/generate_app_sources.py` 扫描生成 `main/app_sources.cmake`。
2. **乱码处理**：
   `tools/esp32/build.py` 会强制注入 `PYTHONUTF8=1` 和 `PYTHONIOENCODING=utf-8`，避免中文注释编译报错或终端乱码；手工跑 `idf.py` 时仍需自行配置这两个变量。
3. **IDF_TARGET**：
   如果看到 `IDF_TARGET is not set, guessed 'esp32'` 提示，这属于正常现象，无需处理。
