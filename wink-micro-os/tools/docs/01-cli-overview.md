# Wink CLI 核心命令使用手册 (`wink`)

`wink.py` 是 Wink Micro OS 的统一构建调度器与 CLI 命令网关（位于 [wink-micro-os/tools/wink.py](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/wink.py)）。它屏蔽了底层构建工具（CMake, EMSdk, ESP-IDF, GCC）的复杂性，为 Host 仿真、WASM 仿真及 ESP32 物理硬件编译提供了一致的操作接口。

---

## 1. 全局通用选项

所有 `wink` 子命令均继承以下全局通用参数：

| 全局参数 | 描述 |
| :--- | :--- |
| `--skip-toolchain-check` | **工具链逃逸开关**：强行跳过环境门控（Gating Check）。仅用于紧急调试，控制台会输出警告。 |
| `-h`, `--help` | 查看特定子命令的帮助信息。 |

---

## 2. 核心子命令详解

### 2.1 `wink gen` — 设备树与配置宏生成

* **功能**：调用 [codegen/](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/codegen) 引擎，解析应用 JSON 配置（`wink-app.json`）与开发板描述（`board.json`），生成设备树 C 代码（`device_tree.h/c`）、Arduino 兼容绑定及 `wink_config.h`。
* **常用语法**：
  ```bash
  python tools/wink.py gen --app oled_dashboard
  python tools/wink.py gen --app /path/to/custom_app
  ```
* **常用选项**：
  - `--app APP_NAME_OR_PATH`: 指定内置示例应用名称或外部自定义应用目录路径（默认: `oled_dashboard`）。

---

### 2.2 `wink build` — 构建 Host / WASM 仿真器

* **功能**：编译 Host 原生二进制或 WASM 仿真网页模块。支持自动生成代码并触发 CMake/EMSdk 编译。
* **常用语法**：
  ```bash
  # 编译 Host 原生仿真
  python tools/wink.py build host --app oled_dashboard

  # 编译 WASM 仿真模块
  python tools/wink.py build wasm --app avoidance_car --sdk-mode source

  # 清洁构建 Host
  python tools/wink.py build host --clean
  ```
* **常用选项**：
  - `target` (*必填*): `host` 或 `wasm`。
  - `--app APP_NAME`: 指定编译的应用名称或目录。
  - `--clean`: （仅 Host）在编译前清理构建目录。
  - `--sdk-mode [source|binary]`: 指定 SDK 模式。`source` 表示从源码构建，`binary` 表示链接预编译二进制 `.a` 静态库（默认根据 SDK 树自动识别）。

---

### 2.3 `wink esp32` — ESP32 物理固件构建与烧录

* **功能**：封装 Espressif ESP-IDF (`idf.py`)，编译、烧录并监听 ESP32 物理真机芯片。
* **常用语法**：
  ```bash
  # 编译 ESP32 示例固件
  python tools/wink.py esp32 --app devkitc_smoke build

  # 烧录并打开串口监视器 (使用 '--' 传递 idf.py 参数)
  python tools/wink.py esp32 --app devkitc_smoke -- -p COM3 flash monitor

  # 全量清理
  python tools/wink.py esp32 --app devkitc_smoke fullclean
  ```

* **内部沙箱隔离与构建机制**：
  - **环境自动净化 (`tools/esp32/build.py`)**：在执行 `idf.py` 之前，`wink` 会自动净化当前环境变量，强制过滤并剔除 PATH 中 MinGW、MSYS2 及 EMSdk 的二进制目录，防止与 IDF 自带的 GCC 工具链产生冲突；同时强制置 `PYTHONUTF8=1` 避免编码乱码。
  - **应用源码自动发现 (`tools/esp32/generate_app_sources.py`)**：在 CMake 配置阶段自动扫描应用目录下的 `.c` 源码文件，并自动渲染写入 `esp32_firmware/main/app_sources.cmake`，实现无感代码注入。

* **参数透传说明**：
  传递给 `idf.py` 且带有 `-` 的选项（如端口 `-p`、波特率 `-b` 等），需放在 `--` 符号之后，避免被 `wink` 解析器误拦截。

---

### 2.4 `wink web` — 启动 Vue Vite 前端仿真服务器

* **功能**：启动工作区前端 `embedded-frontend` 视效仿真 DevServer，支持浏览器端与 WASM 模块互动。
* **常用语法**：
  ```bash
  python tools/wink.py web
  python tools/wink.py web --port 8080
  ```
* **常用选项**：
  - `--port PORT`: 指定 Vite 服务监听端口（默认: `5173`）。

---

### 2.5 `wink test` — 运行测试与 Sanitizer Pass 矩阵

* **功能**：一键跑通 C/C++ 单元测试、Python 规则测试、以及 UBSan/ASan 内存安全 Sanitizer 测试矩阵。
* **常用语法**：
  ```bash
  # 运行标准测试集
  python tools/wink.py test

  # 运行全量测试矩阵（含 ASan + UBSan + WASM 编译校验 + 架构 Linter）
  python tools/wink.py test --full

  # 单独开启 UBSan / ASan 检查
  python tools/wink.py test --sanitize --asan
  ```
* **常用选项**：
  - `--clean`: 测试前清理测试临时目录。
  - `--detailed`: 输出详细的 CTest 日志 (`-V`)。
  - `--sanitize`: 启用 UBSan Pass (`-fsanitize=undefined`)。
  - `--asan`: 启用 ASan Pass (`-fsanitize=address`)。
  - `--full`: 跑通完整测试矩阵。
  - `--with-wasm`: 附加 WASM 编译校验检查。

---

### 2.6 `wink doctor` — 环境与工具链诊断

* **功能**：探测并报告系统当前安装的每一个工具链能力（Python, GCC, CMake, Make, EMSdk, ESP-IDF, Node.js 等）状态及版本。
* **常用语法**：
  ```bash
  python tools/wink.py doctor
  ```

---

### 2.7 `wink setup` — 检查或配置工具链路径

* **功能**：查看或持久化修改全局用户配置（`~/.wink/tools.json`）或工作区配置（`<workspace>/.wink/tools.json`）。
* **常用语法**：
  ```bash
  # 查看当前配置
  python tools/wink.py setup

  # 手动设置指定工具路径（自动校验生效）
  python tools/wink.py setup --set gcc=C:/toolchain/bin/gcc.exe

  # 持久化到工作区级别配置
  python tools/wink.py setup --set emsdk=C:/emsdk --workspace
  ```

---

### 2.8 `wink lint` — 静态架构治理检查器

* **功能**：(ADR-0043) 扫描 C 代码库规范，防止架构越界、接口裸调及动态内存分配。
* **常用语法**：
  ```bash
  # 运行全量架构检查
  python tools/wink.py lint

  # 增量检查 git 变动文件
  python tools/wink.py lint --changed

  # 解释指定规则 ID
  python tools/wink.py lint --explain LAYER_VIOLATION_BAL_TO_DAL
  ```
* 详细使用请参阅 [04-architecture-linter.md](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/docs/04-architecture-linter.md)。

---

## 3. 工作区配置文件 `wink-workspace.json`

在 monorepo 根目录下放置 `wink-workspace.json`，可以对项目路径进行重定位（Relocatable Workspace）：

```json
{
  "sdk_dir": "wink-micro-os",
  "frontend_dir": "wink-ai-frontend",
  "apps_dir": "wink-micro-app"
}
```
`wink.py` 会自动递归向上查找并加载该配置。
