# README
- en: [English](./README.md)
- zh_CN: [简体中文](./README.zh_CN.md)

# Wink-Micro-OS ESP32 固件

将 `wink-micro-os/samples/` 下的业务应用编译为可烧录到 ESP32 的固件。

---

## 🎯 核心特性：业务代码零改固件

**业务代码放在 `wink-micro-os/samples/<AppName>/`，换 App / 增删源文件，`esp32_firmware/` 源码一行都不用改！**

由 `tools/esp32/generate_app_sources.py` 在 CMake configure 阶段自动扫描并注入构建。

---

## 📜 App 源文件扫描器 (tools/esp32/generate_app_sources.py)

### 作用

自动扫描 `wink-micro-os/samples/<AppName>/` 下的所有 `.c` 源文件（排除 host 端到端测试文件 `test_*.c` 与 BAL-only 文件），生成 CMake 片段 `main/app_sources.cmake`，供构建系统自动引入。CMake 会在 configure 阶段自动调用它，日常构建**无需手动执行**。

**彻底消除：**
- 换 App 时手动改 `CMakeLists.txt` 路径
- 新增源文件时手动添加到 `SRCS` 列表
- 硬编码耦合业务目录结构

### 使用方式

**方式一：CMake 自动调用（推荐，完全无感）**
```powershell
# 直接 build 即可，脚本会在 configure 阶段自动运行
idf.py build
```

**方式二：手动指定 App**
```powershell
# 编译指定 App（零改源码）
idf.py build -DWINK_APP=devkitc_smoke
idf.py build -DWINK_APP=avoidance_car
idf.py build -DWINK_APP=oled_dashboard
```

**方式三：单独跑扫描器（调试用）**
```powershell
# 从仓库根目录跑，生成指定 App 的源文件清单
python ..\wink-micro-os\tools\esp32\generate_app_sources.py --app-dir ..\wink-micro-app\devkitc_smoke --esp32-firmware-dir .

# 或按 App 名字（解析到 wink-micro-os/samples/<name>）
python ..\wink-micro-os\tools\esp32\generate_app_sources.py --app-name avoidance_car --esp32-firmware-dir .
```

### 生成产物

脚本输出到 `main/app_sources.cmake`（**自动生成，请勿手动修改**）：
```cmake
set(WINK_APP_NAME "devkitc_smoke")
set(WINK_APP_DIR ".../wink-micro-os/samples/devkitc_smoke")
set(WINK_APP_SOURCES
    .../app_callbacks.c
    .../board_config.c
    .../device_tree.c
)
```

---

## 🚀 完整烧录流程

### 1. 通过 Wink CLI 构建（推荐）

从仓库根目录运行：

```powershell
python wink-micro-os/tools/wink.py esp32 --app <path-to-app> build
```

Wink 会自动激活 ESP-IDF（hot PATH → Windows 上通过 `powershell.exe` source EIM profile → `export.ps1` / `export.sh` fallback）。你**不需要**手工 dot-source EIM profile。`PYTHONUTF8=1` / `PYTHONIOENCODING=utf-8` 会被自动注入，避免中文注释乱码。

如需手工调用 `idf.py`（例如 `idf.py menuconfig` 交互式配置），请先在 shell 里激活 EIM profile，参见 [preinstall.md §3](../wink-micro-os/tools/preinstall.md)。

### 2. 编译固件

```powershell
# （可选）彻底清空 build 目录，从零重编
python wink-micro-os/tools/wink.py esp32 --app <path-to-app> -- fullclean

# 编译（默认子命令为 build）
python wink-micro-os/tools/wink.py esp32 --app <path-to-app> build

# `--` 后面的所有参数会原样透传给 idf.py
python wink-micro-os/tools/wink.py esp32 --app avoidance_car -- build -DSOME_EXTRA=1
```

### 3. 烧录 + 串口监视器

```powershell
# 将 COM3 替换为你的实际串口号（Linux/macOS：/dev/ttyUSB0 等）
python wink-micro-os/tools/wink.py esp32 --app <path-to-app> -- -p COM3 flash monitor
```

### 4. 退出监视器

按快捷键：`Ctrl + ]`

---

## 📋 可用 App 列表

| App 名称 | 说明 |
|---|---|
| `devkitc_smoke` | DevKitC 冒烟测试（GPIO / PWM / I2C / 双核 / 看门狗）|
| `avoidance_car` | 避障小车（超声波 + 舵机） |
| `oled_dashboard` | OLED 仪表盘（按键 + LED + SSD1306） |

---

## 💡 常见问题

### Q: 新增了一个 `.c` 源文件，需要改 CMakeLists.txt 吗？
**不需要。** 脚本会自动扫描到，重新 `idf.py build` 即可。

### Q: 编译报中文注释乱码错误？
已在 CMake 中配置 GCC UTF-8 编码标志（`-finput-charset=UTF-8`），正常不会遇到。如果仍有问题，请确保源文件保存为 UTF-8 编码。

### Q: 什么时候需要 `fullclean`？
- 换 App 后
- 修改了 CMake 脚本后
- 遇到奇怪的链接错误 / 构建问题时
- 日常改代码直接 `build` 即可，不需要 `fullclean`

### Q: `IDF_TARGET is not set, guessed 'esp32'` 是错误吗？
**不是。** 这是正常信息——ESP-IDF 从 `sdkconfig` 自动检测到编译目标为 `esp32`，可以忽略。

---

## 📐 架构说明

```
esp32_firmware/
├── CMakeLists.txt              # ESP-IDF 工程入口，EXTRA_COMPONENT_DIRS 引入 targets/esp32
├── sdkconfig                   # ESP32 默认配置（2MB flash）
└── main/
    ├── app_main.c              # FreeRTOS task 创建 + 栈/heap 监控
    ├── CMakeLists.txt          # 引入自动生成的 app_sources.cmake
    └── app_sources.cmake       # ⚙️ CMake configure 阶段由 tools/esp32/generate_app_sources.py 自动生成，请勿手动修改
```

> 构建工具位于 `../wink-micro-os/tools/esp32/`（`activate.py`、`build.py`、`generate_app_sources.py`），详见 [wink-micro-os/tools/README.md](../wink-micro-os/tools/README.md)。
