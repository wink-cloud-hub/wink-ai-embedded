---
name: burn-firmware-esp32
description: 用于 Wink-Micro-OS ESP32 固件的自动扫描、构建、编译、烧录与清理。当用户输入有关 ESP32 固件构建、编译、烧写、清理等命令（或直接提供 App 名称如 devkitc_smoke, avoidance_car, oled_dashboard, resource_conflict 作为参数）时触发。本 Skill 会将紧跟后面的第一个参数解析为 App 名字，并指导用户通过统一入口 `wink.py esp32` 驱动构建与烧录流程。
---

# ESP32 固件构建与烧录 Skill

本 Skill 协助用户对 `wink-micro-os/samples/` 或 `wink-micro-app/` 下的业务应用进行 ESP32 固件编译、烧录及清理。

## 🎯 核心逻辑

用户输入有关编译/烧录的指令时，**紧跟后面的第一个参数即为 App 名字**（如 `avoidance_car`），未指定时默认 `devkitc_smoke`。
所有操作**统一通过 Wink CLI**（`python wink-tools/wink.py esp32`）执行——Wink 自动激活 IDF 环境、扫描源文件、剥离 PATH 污染，用户**不需要**在 shell 里手工 dot-source EIM profile。所有命令都从**仓库根目录**执行。

---

## 🚀 命令速查

> [!IMPORTANT]
> **`--` 分隔符规则**：传给 idf.py 的子命令（build / flash / monitor / fullclean / menuconfig 等）可以直接写；但凡是**以 `-` 开头的参数**（-p / -v / -b / -D 等），前面必须加 `--`，让 argparse 停止解析 Wink 自己的 flag。

### 1. 编译指定 App

```powershell
# 默认 App = devkitc_smoke；默认子命令 = build
python wink-tools/wink.py esp32

# 指定 App 名字（在 wink-micro-os/samples/<name>/ 下）
python wink-tools/wink.py esp32 --app $AppName

# 指定 App 目录（相对或绝对路径）
python wink-tools/wink.py esp32 --app wink-micro-app/my_custom_app
```

### 2. 清理并重编 (Fullclean)

更换 App、修改 CMake/Kconfig、升级 IDF 或遇奇怪链接错误时使用：

```powershell
python wink-tools/wink.py esp32 --app $AppName fullclean
```

### 3. 烧录与串口监视

```powershell
# -p 指定串口号（Windows: COM3；Linux: /dev/ttyUSB0；macOS: /dev/tty.usbserial-*）
# 注意 -p 以 '-' 开头，前面必须加 '--'
python wink-tools/wink.py esp32 --app $AppName -- -p COM3 flash monitor

# 只烧录不打开监视器
python wink-tools/wink.py esp32 --app $AppName -- -p COM3 flash

# 仅打开监视器连接已烧录好的固件
python wink-tools/wink.py esp32 --app $AppName -- -p COM3 monitor
```

**退出串口监视器：** `Ctrl + ]`。

### 4. 透传其他 idf.py 参数

```powershell
# 详细构建输出
python wink-tools/wink.py esp32 --app $AppName -- build -v

# 自定义烧录波特率
python wink-tools/wink.py esp32 --app $AppName -- -b 921600 -p COM3 flash

# 交互式 menuconfig
python wink-tools/wink.py esp32 --app $AppName menuconfig
```

### 5. 仅调试扫描器（不编译）

排查某个源文件为什么没被编到：

```powershell
python wink-tools/esp32/generate_app_sources.py --app-name $AppName --esp32-firmware-dir esp32_firmware
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
   业务代码放在 `wink-micro-os/samples/$AppName/` 或 `wink-micro-app/<name>/`，换 App / 增删源文件无需改动 `esp32_firmware/` 任何文件。CMake configure 阶段会自动调用 `tools/esp32/generate_app_sources.py` 生成 `main/app_sources.cmake`。
2. **乱码处理**：
   Wink 自动注入 `PYTHONUTF8=1` 与 `PYTHONIOENCODING=utf-8`；GCC 配置了 `-finput-charset=UTF-8`。手工跑 `idf.py` 时需自行设置这两个变量。
3. **IDF_TARGET 提示**：
   看到 `IDF_TARGET is not set, guessed 'esp32'` 属正常信息，无需处理。
4. **IDF 未找到**：
   运行 `python wink-tools/wink.py doctor` 按提示排查。Wink 永远不会自动安装 IDF（ADR-0030），请通过 Espressif IDE Manager (EIM) 安装 ESP-IDF v6.x，详见 [preinstall.md §3](../../wink-tools/preinstall.md)。
