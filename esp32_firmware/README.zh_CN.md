# README
- en: [English](./README.md)
- zh_CN: [简体中文](./README.zh_CN.md)

# Wink-Micro-OS ESP32 固件

将 `wink-micro-os/samples/`（或外部 `wink-micro-app/<name>/`）下的业务应用编译为可烧录到 ESP32 的固件。

> **所有构建、烧录、串口监视操作都通过 Wink CLI。** 你**不需要**手动激活 ESP-IDF shell、dot-source EIM profile、或直接调用 `idf.py`——Wink 会自动处理 IDF 环境激活、UTF-8 环境设置、MSYS/EMSDK 污染清理、源文件自动重扫描。

---

## 🎯 核心特性：业务代码零改固件

**业务代码放在 `wink-micro-os/samples/<AppName>/`（或 `wink-micro-app/<name>/`），换 App / 增删源文件，`esp32_firmware/` 源码一行都不用改！**

源文件由 `tools/esp32/generate_app_sources.py` 在 CMake configure 阶段自动扫描注入（由 Wink CLI 驱动）。

---

## 🚀 通过 Wink CLI 编译与烧录

所有命令从**仓库根目录**（包含 `wink-micro-os/` 与 `esp32_firmware/` 的目录）运行。

### 前置条件
- 通过 Espressif IDE Manager (EIM) 安装 ESP-IDF v6.x —— 参见 [`wink-tools/preinstall.md`](../wink-tools/preinstall.md) §3。Wink 永远不会自动安装 IDF（ADR-0030）。
- PATH 上有 Python 3.10+。

### 1. 编译固件
```powershell
# 编译默认示例（devkitc_smoke）
python wink-tools/wink.py esp32

# 编译 wink-micro-os/samples/<name>/ 下的指定示例
python wink-tools/wink.py esp32 --app avoidance_car

# 编译外部 wink-micro-app 目录下的应用（也可以是绝对路径）
python wink-tools/wink.py esp32 --app wink-micro-app/my_custom_app
python wink-tools/wink.py esp32 --app D:/projects/my_app
```

### 2. 烧录 + 串口监视
```powershell
# 传给 idf.py、以 '-' 开头的参数（如 -p、-v、-b、-D）前面必须加 '--'。
# build/flash/monitor/fullclean/menuconfig 等子命令不需要 '--'。
python wink-tools/wink.py esp32 --app devkitc_smoke -- -p COM3 flash monitor

# 只烧录不打开串口监视器
python wink-tools/wink.py esp32 --app devkitc_smoke -- -p COM3 flash

# 只打开串口监视器（固件已编译好）
python wink-tools/wink.py esp32 --app devkitc_smoke -- -p COM3 monitor
```

串口号：Windows `COM3`；Linux `/dev/ttyUSB0`；macOS `/dev/tty.usbserial-*` 或 `/dev/cu.usbmodem*`。

**退出串口监视器：** 按 `Ctrl + ]`。

### 3. 清空重构
```powershell
# fullclean 删除 esp32_firmware/build/ 后重编
python wink-tools/wink.py esp32 --app devkitc_smoke fullclean
```

### 4. 透传更多参数（verbose、波特率、Kconfig）
凡以 `-` 开头的参数前加 `--`：
```powershell
# 详细构建输出
python wink-tools/wink.py esp32 --app devkitc_smoke -- build -v

# 自定义烧录波特率
python wink-tools/wink.py esp32 --app devkitc_smoke -- -b 921600 -p COM3 flash

# 交互式 menuconfig（请在保持打开的终端中运行）
python wink-tools/wink.py esp32 --app devkitc_smoke menuconfig
```

---

## 🛠️ 什么时候需要直接用 idf.py？

仅在 `idf.py menuconfig`、`idf.py size`、`idf.py gdb`、`idf.py efuse-*` 等高级交互式工作流下。直接调用 `idf.py` 时必须先手工激活 IDF shell（source EIM PowerShell profile 或执行 `export.ps1`/`export.sh`），因为 Wink 不会替你激活。手工激活方式参见 [`preinstall.md §3`](../wink-tools/preinstall.md)。

在已正确激活的 shell 里直接跑 `idf.py build` 仍然可以——CMake configure 阶段仍会调用 Python 扫描器——但绕过了 Wink 的 UTF-8 / MSYS 防护。**日常构建统一用 `wink.py esp32`。**

---

## 📜 App 源文件扫描器 (tools/esp32/generate_app_sources.py)

扫描器通常由 Wink CLI 或 CMake 自动调用，日常无需手动执行。它做三件事：

1. 递归收集 App 目录下所有 `*.c` 文件，排除 host 端到端测试文件（`test_*.c`）。
2. 可选地补入 `wink-micro-os/samples/common/src/*.c`（减去已迁移到 BAL 的 helper）。
3. 以 UTF-8-sig / CRLF 写入 `esp32_firmware/main/app_sources.cmake`，包含 `WINK_APP_NAME`、`WINK_APP_DIR`、`WINK_APP_SOURCES`、`WINK_APP_COMMON_INCLUDE_DIR` 等 CMake 变量，Windows 下 CMake 可直接读取。

调试时手工运行（单行）：
```powershell
python wink-tools/tools/esp32/generate_app_sources.py --app-dir wink-micro-app/devkitc_smoke --esp32-firmware-dir esp32_firmware
```

---

## 📋 可用示例 App

| App 名称 | 说明 |
|---|---|
| `devkitc_smoke` | DevKitC 冒烟测试（GPIO / PWM / I2C / 双核 / 看门狗）|
| `avoidance_car` | 避障小车（超声波 + 舵机） |
| `oled_dashboard` | OLED 仪表盘（按键 + LED + SSD1306） |

---

## 💡 常见问题

### Q: 新增了一个 `.c` 源文件，需要改 CMakeLists.txt 吗？
**不需要。** 扫描器会自动发现新文件，重新运行 `python wink-tools/wink.py esp32 --app <name>` 即可。

### Q: 中文注释编译报乱码错误？
Wink 自动设置 `PYTHONUTF8=1` 与 `PYTHONIOENCODING=utf-8`，GCC 也配置了 `-finput-charset=UTF-8`。如仍遇乱码，请确认源文件以 UTF-8 编码保存、终端使用 Unicode 字体。

### Q: 什么时候需要 `fullclean`？
- 切换 App 之后
- 修改 CMake 脚本或 Kconfig 之后
- 升级 ESP-IDF 之后
- 遇到奇怪的链接错误或构建产物污染时
- 日常改代码直接 `esp32`（默认 `build`）即可，不必 clean

### Q: `IDF_TARGET is not set, guessed 'esp32'` 是错误吗？
**不是。** 这是正常信息——ESP-IDF 从 `sdkconfig` 自动探测到芯片目标为 `esp32`，可忽略。

### Q: `wink doctor` 报 IDF 未找到？
运行 `python wink-tools/wink.py doctor` 按提示排查；最常见原因是未通过 EIM 安装 ESP-IDF。参见 [`preinstall.md §3`](../wink-tools/preinstall.md)。

---

## 📐 架构说明

```
esp32_firmware/
├── CMakeLists.txt              # ESP-IDF 工程入口，通过 EXTRA_COMPONENT_DIRS 引入 targets/esp32
├── sdkconfig                   # ESP-IDF 默认配置（2MB flash）
└── main/
    ├── app_main.c              # FreeRTOS task 创建 + 栈/heap 监控
    ├── CMakeLists.txt          # 引入自动生成的 app_sources.cmake
    └── app_sources.cmake       # ⚙️ CMake configure 阶段由 tools/esp32/generate_app_sources.py 自动生成，请勿手动修改
```

构建工具位于 `../wink-tools/tools/esp32/`——`activate.py`（IDF 环境采集）、`build.py`（净化环境后调用 idf.py）、`generate_app_sources.py`（源文件扫描器）。**统一入口：`python wink-tools/wink.py esp32`。** 详见 [`wink-tools/README.md`](../wink-tools/README.md)。
