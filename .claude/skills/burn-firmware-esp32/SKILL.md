---
name: burn-firmware-esp32
description: 用于 Wink-Micro-OS ESP32 固件的自动扫描、构建、编译、烧录与清理。当用户输入有关 ESP32 固件构建、编译、烧写、清理等命令（或直接提供 App 名称如 devkitc_smoke, avoidance_car, oled_dashboard, smp_uaf_test 作为参数）时触发。本 Skill 会将紧跟后面的第一个参数解析为 DWINK_APP 名字，并指导进行正确的环境激活和 CMake/idf.py 构建流程。
---

# ESP32 固件构建与烧录 Skill

本 Skill 旨在协助用户对 `wink-micro-os/samples/` 下的业务应用进行 ESP32 固件编译、烧录及清理。

## 🎯 核心逻辑

用户输入有关编译/烧录的指令时，**紧跟后面的第一个参数即为 `DWINK_APP` 的名字**（如 `avoidance_car`）。
你需要提取该 App 名字，并按以下流程在 Windows PowerShell 下执行相应构建和烧录操作。

---

## 🚀 执行步骤与命令链 (PowerShell)

> [!IMPORTANT]
> 由于 PowerShell 的环境变量和 Profile 激活只在当前进程生效，你**必须**在单个 `run_command` 调用中，使用分号 `;` 将环境激活命令与 `idf.py` 命令链拼接在一起执行。

### 1. 编译指定 App
- **目标 App**: 提取自用户输入的第一个参数 `$AppName`（若未指定，默认使用 `devkitc_smoke`）。
- **执行命令**:
  ```powershell
  $env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'; . 'C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1'; idf.py build -DWINK_APP=$AppName
  ```
  *(注：需在 `esp32_firmware` 目录下运行，通过 `Cwd` 参数指定为 `esp32_firmware` 绝对路径。)*

### 2. 清理并重编 (Fullclean)
在更换 App、修改 CMake 脚本或遇到奇怪的链接错误时，需要先执行 `fullclean`：
  ```powershell
  $env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'; . 'C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1'; idf.py fullclean; idf.py build -DWINK_APP=$AppName
  ```

### 3. 烧录与串口监视
将固件烧录到指定串口（例如 `COM3`，请优先从历史命令或用户提示中确认串口号，默认为 `COM3`）：
  ```powershell
  $env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'; . 'C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1'; idf.py -p COM3 flash monitor
  ```
  *(注：串口监视器退出快捷键为 `Ctrl + ]`)*

### 4. 调试脚本运行 (仅生成 CMake 片段，不进行完整编译)
若只需运行扫描脚本，在 `esp32_firmware` 目录下运行：
  ```powershell
  .\generate_app_sources.ps1 -AppName $AppName
  ```

---

## 📋 可用 App 列表参考

| App 名称 | 说明 |
|---|---|
| `devkitc_smoke` | DevKitC 冒烟测试（GPIO / PWM / I2C / 双核 / 看门狗） - **默认值** |
| `avoidance_car` | 避障小车（超声波 + 舵机） |
| `oled_dashboard` | OLED 仪表盘（按键 + LED + SSD1306） |
| `smp_uaf_test` | SMP UAF 验证测试（多核并发/中断同步安全性测试） |

---

## 💡 常见构建规范与排错

1. **零改动特性**：
   业务应用代码存放在 `wink-micro-os/samples/$AppName/`。更换 App 或增删源文件时，`esp32_firmware` 目录下的源码不需要做任何修改。CMake 配置会在 configure 阶段自动调用 `generate_app_sources.ps1` 扫描生成 `main/app_sources.cmake`。
2. **乱码处理**：
   在 PowerShell 环境中，必须配置 `$env:PYTHONUTF8 = '1'` 和 `$env:PYTHONIOENCODING = 'utf-8'` 以防止中文注释编译报错或终端乱码。
3. **IDF_TARGET**：
   如果看到 `IDF_TARGET is not set, guessed 'esp32'` 提示，这属于正常现象，无需处理。
