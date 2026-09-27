# CMS8S78xx 官方示例仿真适配与测试标准执行手册 (Playbook)

> **版本**：v1.0  
> **适用芯片**：中微半导体 CMS8S78xx 系列 (1T 高速 8051 内核)  
> **关联清单**：[CHECKLIST.md](CHECKLIST.md)  
> **适用对象**：AI Coding Agents（Antigravity、Claude Code 等）与嵌入式开发工程师。

---

## 零、 核心原则与硬性门禁 (Non-Negotiable Gates)

对于清单 [CHECKLIST.md](CHECKLIST.md) 中的**每一项示例**，在将其状态标记为 `[x]`（已完成）之前，**必须严格执行并通过以下两大操作**：

1. **真实编译输出仿真资产**：
   使用 `wink-ai/packages/wink-tools/` 进行端到端真实构建编译，生成并输出资产三件套至该微应用的 `unisim-assets/` 目录下：
   - `device-tree.json`（设备树描述文件）
   - `wink_simulator.js`（Emscripten Wasm 胶水层）
   - `wink_simulator.wasm`（固件行为级仿真二进制）
2. **确定性 Headless 自动化测试实证**：
   编写对应的场景脚本 `unisim-scenarios/<name>.scenario.json`，使用 `wink-ai/packages/unisim` 驱动 Headless 模式进行自动化测试，确保所有断言步骤（微秒级引脚电平、外设插件状态、时序）**100% 绿灯通过**。
3. **原厂源码“一行不改”准则**：
   从官方工程拷贝的 C 源码（`main.c`, `isr.c`, `demo_*.c`, `demo_*.h`）必须保持原汁原味。所有 Keil C51 特异性关键字（`sbit`, `sfr`, `interrupt`）、寄存器定义及外设行为，均由 WinkMicroOS 编译清洗工具（`mcs51_cleanup.py`）与框架底座拦截层（`REG_CMS8S78XX.H` / C++ 仿真引擎）静态解决。

---

## 一、 标准实施闭环工作流 (5 阶段)

```
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段一：新建 App 与原厂代码镜像 (Setup & Mirror)                        │
│   ├── 创建 wink-micro-app/vendor_cms8s78xx_<feature>/             │
│   ├── 镜像官方源码（保持一行不改）                                      │
│   ├── 配置 CMakeLists.txt (接入 mcs51_cleanup.py 清洗流程)             │
│   ├── 配置 wink-app.json (声明 board, mcu, devices 引脚映射)           │
│   └── 编写 unisim-scenarios/<feature>.scenario.json 确定性场景测试脚本 │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段二：使用 wink-tools 进行真实编译构建输出 (Real Build)               │
│   ├── 运行: python packages/wink-tools/wink.py build sim --app <app>   │
│   ├── [1/3] 校验并生成 device-tree.json                                │
│   ├── [2/3] Emscripten 真实编译 Wasm 二进制                            │
│   └── [3/3] 提取并校验 unisim-assets/ 三件套有效性                     │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段三：使用 unisim 进行 Headless 场景仿真测试 (Headless Verification) │
│   ├── 运行: bun bin/unisim-sim.mjs run --mode=headless ...             │
│   ├── 加载 unisim-assets/ 仿真三件套                                   │
│   ├── 注入微秒级输入激励事件 (INPUT_PLUGIN_EVENT)                      │
│   └── 断言微秒级引脚电平与插件状态 (ASSERT_POINT)                      │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段四：底座 CTest 回归与分层门禁检查 (Unit & Lint Gates)               │
│   ├── 执行 CTest 单元测试验证底层外设模型 (test_mcs51_*)               │
│   └── 运行架构分层门禁: python wink-tools/wink.py lint arch            │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段五：核对清单 (Checklist) 回写与归档 (Checklist Sign-off)            │
│   ├── 更新 CMS8S78XX_EXAMPLE_CHECKLIST.md 状态为 [x]                   │
│   ├── 填入对应 App 目录名与详实验收记录                                │
│   └── 更新总体进度计数                                                 │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 二、 阶段一：新建 App 与原厂代码镜像 (Setup & Mirror)

### 1. 目录命名与结构规范
在 `wink-micro-app/` 下创建新目录，统一遵循命名规范：
`vendor_cms8s78xx_<feature_lowercase>`

标准目录结构如下：
```text
wink-micro-app/vendor_cms8s78xx_<feature>/
├── CMakeLists.txt              # 构建脚本（清洗规则配置）
├── wink-app.json               # 微应用元数据与引脚拓扑配置
├── main.c                      # 官方原始 main.c（一行不改）
├── isr.c                       # 官方原始 isr.c（一行不改，若有）
├── demo_<feature>.c            # 官方功能源码（一行不改）
├── demo_<feature>.h            # 官方头文件（一行不改）
├── unisim-assets/              # 仿真构建资产输出目录（由阶段二自动生成/输出）
│   ├── device-tree.json
│   ├── wink_simulator.js
│   └── wink_simulator.wasm
└── unisim-scenarios/           # 确定性自动化场景用例目录
    └── <feature>.scenario.json
```

### 2. `CMakeLists.txt` 模板
所有 MCS-51 示例统一通过 `mcs51_cleanup.py` 驱动代码清洗：
```cmake
cmake_minimum_required(VERSION 3.20)
project(vendor_cms8s78xx_<feature> C ASM)

set(WINK_MCU "cms8s78xx")
include(${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake)

set(VENDOR_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/demo_<feature>.c
    ${CMAKE_CURRENT_SOURCE_DIR}/isr.c
)

# 自动生成清洗后的源码目标
set(CLEANED_SRCS "")
foreach(SRC ${VENDOR_SRCS})
    get_filename_component(SRC_NAME ${SRC} NAME)
    set(OUT_SRC "${CMAKE_CURRENT_BINARY_DIR}/cleaned_${SRC_NAME}")
    add_custom_command(
        OUTPUT ${OUT_SRC}
        COMMAND ${Python3_EXECUTABLE}
                ${WINK_CODEGEN_ROOT}/generators/mcs51_cleanup.py
                ${SRC} ${OUT_SRC}
        DEPENDS ${SRC} ${WINK_CODEGEN_ROOT}/generators/mcs51_cleanup.py
        COMMENT "Cleaning MCS-51 Keil C51 syntax: ${SRC_NAME}"
    )
    list(APPEND CLEANED_SRCS ${OUT_SRC})
endforeach()

add_executable(${PROJECT_NAME} ${CLEANED_SRCS})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WINK_FRAMEWORKS_DIR}/mcs51/include
)
target_link_libraries(${PROJECT_NAME} PRIVATE wink_framework_mcs51)
```

### 3. `wink-app.json` 配置规范
配置中必须包含 upstream 溯源信息以及正确的开发板和外设引脚映射：
```json
{
  "app_name": "vendor_cms8s78xx_<feature>",
  "display_name": "CMS8S78xx V2.0.2 - <Feature Name>",
  "board": "cms8s78xx_devboard",
  "category": "vendor_example",
  "mcu": "cms8s78xx",
  "tick_ms": 1,
  "upstream": {
    "vendor": "Cmsemicon",
    "version": "V2.0.2",
    "source_dir": "docs/vendors/Cmsemicon/CMS8S78xx_DemoCode_V2.0.2/CMS8S78xx_Example/Example/<Category>/<SubPath>/code"
  },
  "devices": {
    "led": {
      "type": "led",
      "gpio_pin": "$board.headers.P32",
      "active_high": true
    }
  }
}
```

### 4. `unisim-scenarios/<feature>.scenario.json` 测试用例规范
定义微秒级时序步骤、输入激励与预期观测断言：
```json
{
  "header": {
    "version": "2.0",
    "name": "mcs51 CMS8S78xx <feature> headless proof",
    "templateId": "vendor_cms8s78xx_<feature>",
    "accuracyMode": "behavioral",
    "timeoutUs": "2000000",
    "failurePolicy": "fail-fast",
    "determinism": { "prngSeed": 42 }
  },
  "steps": [
    {
      "type": "ASSERT_POINT",
      "timeUs": "100ms",
      "target": "plugin:led/on",
      "matcher": false,
      "description": "[初始状态] 验证上电后 LED 初始熄灭"
    },
    {
      "type": "INPUT_PLUGIN_EVENT",
      "timeUs": "200ms",
      "targetPluginId": "btn",
      "action": "SET_PRESSED",
      "params": { "pressed": true }
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "400ms",
      "target": "plugin:led/on",
      "matcher": true,
      "description": "[功能断言] 触发后 LED 状态翻转为亮起"
    }
  ]
}
```

---

## 三、 阶段二：使用 `wink-tools` 进行真实构建编译 (Real Build)

### 1. 构建入口命令
在 `wink-ai` 仓库根目录下，通过 `wink.py` 的 `build sim`（或 `build-sim`）子命令执行端到端构建：

```bash
# 在 wink-ai/ 目录下执行：
python packages/wink-tools/wink.py build sim --app vendor_cms8s78xx_<feature>
```

> **参数说明**：
> - `--app <app_name>`：微应用目录名（支持自动在 `wink-micro-app/` 中检索）或微应用绝对路径。
> - `--clean`：（可选）构建前清理构建缓存目录 `build/wasm/<app_name>`。
> - `--out <dir>`：（可选）自定义输出目录，默认自动输出到 `<app_dir>/unisim-assets`。

### 2. 内部执行三部曲解析
`build sim` 命令会自动按序执行以下三步流水线：
1. **[1/3] Generating DeviceTree**：
   调用 `tools/frontend/runtime_device_tree.py`，根据 `wink-app.json` 与板级定义（`cms8s78xx_devboard.json`）进行严格校验，并在 `<app_dir>/unisim-assets/` 下生成规范的 `device-tree.json`。
2. **[2/3] Compiling WASM simulator**：
   调用 Emscripten 工具链（`emcmake cmake` 与 `cmake --build`），将 WinkMicroOS 运行时、MCS-51 仿真内核及清洗后的原厂业务代码编译为 WebAssembly 目标文件。
3. **[3/3] Extracting WASM assets**：
   将编译生成的 `wink_simulator.js` 与 `wink_simulator.wasm` 自动提取并拷贝至目标微应用的 `<app_dir>/unisim-assets/` 目录。

### 3. 产物校验清单
构建完成后，必须检查 `<app_dir>/unisim-assets/` 确保包含以下三件套，且文件大小合法非零：
| 产物文件名 | 期望大小范围 | 说明 |
| :--- | :--- | :--- |
| `device-tree.json` | 500B ~ 2KB | 包含 board, mcu, devices 拓扑与引脚映射 |
| `wink_simulator.js` | 100KB ~ 150KB | Emscripten 生成的 JavaScript 运行时加载胶水层 |
| `wink_simulator.wasm` | 150KB ~ 300KB | 包含完整 8051 CPU 解释器、外设模型与清洗后固件的 Wasm 二进制 |

---

## 四、 阶段三：使用 `wink-tools` 或 `unisim` 进行 Headless 场景仿真测试 (Headless Verification)

### 1. 为什么直接运行 `bun bin/unisim-sim.mjs` 会被拒绝？
`unisim` 仿真引擎内置了平台授权与签发票据校验机制（`__WINK_LAUNCH_TICKET__`）。如果直接脱离 `wink-tools` 命令行调度器裸跑，引擎会抛出：
```text
❌ [winksim] 拒绝直接执行：winksim 仿真引擎仅受 winkcli 统一调度管理。
原因: Missing or malformed __WINK_LAUNCH_TICKET__ environment variable
💡 正确使用方式：👉 winkcli sim run --app <your_app>
```

### 2. 测试执行命令 (二选一)

#### 推荐方式 A：使用 `wink-tools` 统一执行（单命令全自动闭环）
在 `wink-ai` 仓库根目录下，通过 `wink.py sim run` 执行。该命令会**全自动编译仿真资产、自动签发合法票据、并自动拉起 Headless 测试**：

```powershell
# 在 D:\workspaces\ai-coding\wink-ai\wink-ai 根目录下执行：
python packages/wink-tools/wink.py sim run `
  --app vendor_cms8s78xx_<feature> `
  --mode headless `
  --scenarios ../wink-ai-embedded/wink-micro-app/vendor_cms8s78xx_<feature>/unisim-scenarios/<feature>.scenario.json
```

> **执行机制**：
> 1. 自动调用 `BuildSimCommand` 检测并生成 `unisim-assets/`（含 `device-tree.json` + `wink_simulator.wasm`）；
> 2. 自动生成 Ed25519 签名票据并注入进程环境变量 `__WINK_LAUNCH_TICKET__`；
> 3. 自动探测本地 `bun` 环境拉起引擎执行微秒级场景断言。

#### 方式 B：在 `packages/unisim/` 目录下通过 `bun` 直接调试（开启开发者通道）
如果开发阶段需要快速反复跑场景脚本，可以在 `wink-ai/packages/unisim` 目录下开启本地开发者绕行标记（`WINK_DEV=1`）：

- **PowerShell**：
  ```powershell
  cd D:\workspaces\ai-coding\wink-ai\wink-ai\packages\unisim
  $env:WINK_DEV="1"
  bun bin/unisim-sim.mjs run --mode=headless `
    --app ../../../wink-ai-embedded/wink-micro-app/vendor_cms8s78xx_<feature> `
    --scenarios ../../../wink-ai-embedded/wink-micro-app/vendor_cms8s78xx_<feature>/unisim-scenarios/<feature>.scenario.json
  ```

- **Git Bash / Linux / macOS**：
  ```bash
  cd packages/unisim
  WINK_DEV=1 bun bin/unisim-sim.mjs run --mode=headless \
    --app ../../../wink-ai-embedded/wink-micro-app/vendor_cms8s78xx_<feature> \
    --scenarios ../../../wink-ai-embedded/wink-micro-app/vendor_cms8s78xx_<feature>/unisim-scenarios/<feature>.scenario.json
  ```

### 3. 参数与环境说明
- `--mode headless`：无头纯 Node/Bun 运行模式，不启动 Chromium 浏览器，执行速度极快（通常 < 500ms），完美适配 CI/CD 与本地快速迭代。
- `--app <path_or_name>`：指向微应用目录或 App 名称（CLI 会自动定位同级 `unisim-assets/` 中的 `device-tree.json`、`wink_simulator.js` 与 `wink_simulator.wasm`）。
- `--scenarios <path>`：指向待执行的确定性场景测试脚本。

### 4. 结果判断与标准输出
- **成功标准**：
  - 控制台按序输出各 Step 的绿色勾选标记 `✔ [ASSERT_POINT] ... PASSED`；
  - 最终汇总输出 `ALL PASSED`，进程退出码为 `0`。
- **失败排查**：
  - 若退出码为非 0（如 `1` 或 `2`），检查失败步骤的 `expected` 与 `actual` 比对日志，定位是中断未响应、引脚未配置还是时序延迟（`timeUs`）不足。

### 5. 命令行 Browser 模式测试与自定义 URL
如果需要驱动真实浏览器页面进行自动化测试（甚至弹窗肉眼可见运行）：
1. 先在一个终端启动前端服务：
   ```powershell
   cd D:\workspaces\ai-coding\wink-ai\wink-ai\packages\embedded-frontend
   npm run dev   # 运行在 http://localhost:5173
   ```
2. 在另一个终端执行 Browser 模式测试（支持 `--url` 自定义前端地址）：
   ```powershell
   # 方式 1：通过 wink-tools 运行（带 --headed 会弹出可见 Chromium 浏览器窗口）
   cd D:\workspaces\ai-coding\wink-ai\wink-ai
   python packages/wink-tools/wink.py sim run `
     --app vendor_cms8s78xx_<feature> `
     --mode browser `
     --headed `
     --url http://localhost:5173 `
     --scenarios ../wink-ai-embedded/wink-micro-app/vendor_cms8s78xx_<feature>/unisim-scenarios/<feature>.scenario.json

   # 方式 2：在 packages/unisim 目录下通过 bun 运行
   cd D:\workspaces\ai-coding\wink-ai\wink-ai\packages\unisim
   $env:WINK_DEV="1"
   bun bin/unisim-sim.mjs run `
     --mode=browser `
     --headed `
     --url=http://localhost:5173 `
     --app ../../../wink-ai-embedded/wink-micro-app/vendor_cms8s78xx_<feature> `
     --scenarios ../../../wink-ai-embedded/wink-micro-app/vendor_cms8s78xx_<feature>/unisim-scenarios/<feature>.scenario.json
   ```
   > **实用参数与浏览器探测机制**：
   > - **自动探测本地浏览器**：Unisim 会自动优先使用 Playwright 沙盒 Chromium；若未通过 `playwright install` 下载，将**自动降级探测并启动本地已安装的 Google Chrome 或 Microsoft Edge（Windows 默认自带）**，实现开箱即跑！
   > - `--channel <channel>`：（可选）显式指定要拉起的浏览器渠道（如 `chrome`, `msedge`, `chromium`）；
   > - `--headed`：弹出真实可见的浏览器窗口（不加该参数默认以无头浏览器静默执行）；
   > - `--url <url>`：指定前端 Web 服务器地址（默认为 `http://localhost:5173` 或 `http://127.0.0.1:5174`）；
   > - `--record`：自动录制浏览器操作过程，生成 WebM 视频与 GIF 动图保存至 `artifacts/` 目录；
   > - `--cdp <endpoint>`：连接已打开的 Chrome/Tauri 调试端口（如 `http://127.0.0.1:9222`）。
   > - 若需要安装 Playwright 专属独立 Chromium，可在终端执行 `bunx playwright install chromium`。

---

## 五、 阶段四：底座 CTest 回归与架构分层门禁 (Unit & Arch Gates)

如果当前适配的示例涉及 MCS-51 仿真内核或新外设模型扩展（如新增 SFR、扩展中断、定时器模式等）：

1. **底层 C++ 单元测试**：
   - 检查 `wink-micro-os/test/mcs51/unit/` 中是否存在或补充对应的测试用例（例如 `test_mcs51_port_extint.cpp`）。
   - 运行 CTest 确保单元测试通过：
     ```bash
     ctest -R test_mcs51_<feature> --output-on-failure
     ```
2. **架构分层与 API 门禁校验**：
   - 依据 ADR-0043，运行架构分层门禁检查：
     ```bash
     python packages/wink-tools/wink.py lint arch --pack layering --pack api
     ```

---

## 六、 阶段五：核对清单 (Checklist) 回写与归档 (Checklist Sign-off)

只有在**阶段二（真实编译输出 assets）**与**阶段三（Headless 场景测试 100% 通过）**均成功完成之后，方可编辑 [CHECKLIST.md](CHECKLIST.md)：

1. 将对应条目的 `状态` 列从 `[ ]` 更新为 `[x]`。
2. 在 `对应 wink-micro-app` 列填入创建的应用名称（例如 `vendor_cms8s78xx_gpio`）。
3. 在 `验收标准与说明` 列详细注明：
   - 触发逻辑与硬件行为；
   - 关联的 Headless 场景用例路径；
   - 关键断言点实证结果。
4. 同步更新文档顶部 `一、 总体适配进度与统计` 中的统计数字：
   - `已完成适配并实证` 计数递增。
   - `待适配` 计数相应递减。

---

## 七、 标杆示范案例对照 (Reference Examples)

| 编号 | 示例类别 | 标杆微应用目录 | 核心断言策略 |
| :---: | :--- | :--- | :--- |
| **01** | 数码管动态显示 (Timer0 + IO) | `vendor_cms8s78xx_led_4com_8seg` | 断言 P00 心跳、COM 位选互斥 `activeDigits==1`、POV 解码文本 `'1234'`、刷新率稳定 `~45Hz`。 |
| **05** | 端口边沿中断 (GPIO + P1EI) | `vendor_cms8s78xx_gpio` | 注入按键按下事件（P12 下降沿），断言触发 P1EI 中断，翻转 P32 状态（LED 点亮，物理电平翻转为 1）；释放按键上升沿不响应。 |

---

> **本手册是 CMS8S78xx 43 项示例适配的唯一权威操作准则，后续开发中每一项必须严格按本手册闭环执行。**
