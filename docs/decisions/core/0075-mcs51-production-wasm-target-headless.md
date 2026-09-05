# ADR-0075：mcs51 进生产 wasm 链接——第二个自动链接框架，headless 在线实证为阶段 0

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-08-29）** |
| **日期** | 2026-08-29 |
| **触发** | ADR-0070/0071/0074 把 mcs51 拦截层与通道-1 双向数字数据面在**受限 ctest harness**（host fallback + node 桩）内打通，但生产链路从未链接 mcs51：`wink_simulator`（emcc MODULARIZE `WasmSandbox`，worker/headless 可装载）只链接 esp32 风格 app + 可选 arduino；`wink_mcs51_compat` 虽已注册却是 `EXCLUDE_FROM_ALL`，生产链接从不拉它。阶段 0 目标（用户拍板）：让**未修改 Keil 按键→LED 程序**编译成**生产级 `wink_simulator.js/.wasm`**，用姊妹仓 unisim 的 **headless runner**（真实 PinArbiter + 真实 button/led 插件，无 Vue、无 worker）端到端实证——纯本仓 `wink-micro-os`/`wink-micro-app`，不依赖任何前端改动，可独立验收。 |
| **影响范围** | `wink-micro-os/CMakeLists.txt`（mcs51 框架自动链接 + 生产板级 codegen + DAL 全关）、`wink-micro-os/dal/CMakeLists.txt` + `dal/src/wink_dal_stub.c`（空 DAL 兜底 TU）、新增生产 app `wink-micro-app/mcs51_button_led/`（Keil 源 + `wink-app.json` + app CMake + headless 场景 + 生成的 unisim 资产）。**零 unisim 改动**（数据面 MCU 无关，复用既有 `createUnisimImports` 真实桥）。 |
| 决策者 | 项目 Owner |
| **关联 ADR** | [ADR-0070](0070-mcs51-zero-code-simulation-interception-layer.md)（umbrella）、[ADR-0074](0074-mcs51-channel1-external-read-pin.md)（通道-1 双向数字数据面）、[ADR-0071](0071-sfr-proxy-rmw-edge-data-plane.md)（SFR 代理/RMW 红线）、[ADR-0004](0004-static-dispatch-vs-runtime-ops.md)（静态分发）、[ADR-0002](0002-dual-target-compilation.md)（双 target 同源） |
| **关联计划** | [`2026-08-29-mcs51-production-wasm-headless-plan.md`](../../implementation-plans/core/2026-08-29-mcs51-production-wasm-headless-plan.md) |

---

## 1. 背景（Context）

受限 harness 与生产链路之间存在三道缝，使 mcs51 从未跑在"真"仿真上：

1. **框架不进生产链接**。生产 `wink_simulator` 由 `PAL_WASM_SOURCES`（**全部** `pal_wasm_ch*.c` 真实通道 PAL）+ `wasm_entry.c` + OSAL + `WINK_APP_SOURCES` 链成，`--js-library=targets/wasm/wink_sim_js.js`（定义全部 `js_pal_*`，生产由 PinArbiter 实现）。arduino 经一个"框架自动链接块"（gate 在 `WINK_ENABLE_ARDUINO` 或内容探测）按需 `target_link_libraries(wink_simulator PRIVATE wink_arduino_compat)`；mcs51 无对应块。
2. **生产板级 codegen 缺口**。`mcs51_board_config.py` 只被 host/wasm **测试** harness 调用；生产路径从不生成 `mcs51_board_config.h`。`mcs51_bridge.cpp` 用 `__has_include("mcs51_board_config.h")` 可选包含，缺头仍编译但失去自动绑定。
3. **DAL/回调模型冲突**。普通 app 经 `app_codegen.py` 生成 `app_callbacks.c`/`device_tree.c` 并强定义 `wink_app_get_callbacks()`；而 mcs51 框架的 `mcs51_bridge.cpp` **已强定义** `wink_app_get_callbacks()`（init=`mcs51_framework_init`，loop=`mcs51_app_loop`→`wink_mcs51_user_main`，`REGX52.H` 把 `main` 宏成该名）。再链 app 侧回调即重复定义。同时 mcs51 app 的 `wink-app.json` devices（button/led）是**前端插件拓扑**，C 侧 `app_codegen.py` 会把它们当 DAL 驱动拒绝（要求 `gpio_pin`+`event_drive`/`auto_poll_ms`）。

两 Explore agent 已核实关键事实：unisim 桥 `createUnisimImports` 把 `js_pal_gpio_write`→`arbiter.setDriver('mcu:gpio'+pin, SUPPLY)`、`js_pal_gpio_read_state`→`arbiter.readPin`（映射 0/1/2/3）——mcs51 代理用的正是这三个同名 import，**阶段 0 不需改 unisim 一行**；avoidance_car/oled_dashboard 已用同一生产 js 库 + headless 桥跑通。

## 2. 方案比选（Options）

| 方案 | 描述 | 优 | 劣 | 结论 |
|---|---|---|---|---|
| A. 继续只在受限 harness 验证 | mcs51 永不进生产 wasm，前端/worker 集成时再补 | 零改动 | 无法证明生产链接可装载 mcs51；node 桩与 PinArbiter 行为可能漂移；阶段 1 前端无同源 wasm 可载 | ❌ |
| B. 为 mcs51 单独造一套生产入口/桥 | 独立 emcc target + 专用 js 库 | 隔离彻底 | 违背同源/SSOT；PinArbiter 接两套；维护双链路 | ❌ |
| C. mcs51 作为**第二个自动链接框架**接入既有生产链接，框架拥有回调，headless 先证 | 镜像 arduino 自动链接范式；复用真实通道 PAL + `wink_sim_js.js` + PinArbiter 桥；阶段 0 用 headless（无 Vue/worker） | 与 esp32 app 同形 wasm；零 unisim 改动；零回归（gate 仅 mcs51 app 置位）；可独立验收 | 需补板级 codegen、绕开 app_codegen、处理空 DAL | ✅ **采纳** |

## 3. 决策结论（Decision）

### D1. mcs51 = 第二个生产框架，gate 在 app 导出的 `WINK_APP_MCS51`
根 `CMakeLists.txt` 在 arduino 自动链接块之后新增镜像块：

```cmake
if(WINK_APP_MCS51 AND TARGET wink_mcs51_compat)
    message(STATUS "[frameworks] MCS-51 interception enabled for App")
    target_link_libraries(wink_simulator PRIVATE wink_mcs51_compat)
    if(TARGET generate_config)
        add_dependencies(wink_mcs51_compat generate_config)
    endif()
    if(TARGET generate_mcs51_board_config)
        add_dependencies(wink_mcs51_compat generate_mcs51_board_config)
    endif()
endif()
```

gate 用 app 经 `PARENT_SCOPE` 导出的显式布尔 `WINK_APP_MCS51`（与 app 已导出的 `WINK_APP_SOURCES`/`WINK_APP_INCLUDE_DIRS` 同款契约），不扫 JSON、不加 cache var。`add_dependencies` 保序：STATIC 库不继承 exe 的 codegen 依赖，须显式确保 `wink_config.h` 与 `mcs51_board_config.h` 在 bridge TU 编译前生成（include 路径经 pal INTERFACE 传递，文件构建序不传递）。

### D2. mcs51 app 不含 `app_callbacks.c`/`device_tree.c`——框架拥有回调
mcs51 app 的唯一 app 源是 **cleaned Keil `.cpp`**（`mcs51_cleanup.py <src>.c <gen>.cpp`，C++17）。app CMake 在 `EMSCRIPTEN` 分支导出 `WINK_APP_SOURCES=<cleaned.cpp>`、`WINK_APP_INCLUDE_DIRS=<gen dir>`、`WINK_APP_MCS51=TRUE` 后 `return()`；**不**跑 `app_codegen.py`，**不**提供 app 侧回调/设备树（`mcs51_bridge.cpp` 强定义 `wink_app_get_callbacks`，再链即重复符号）。非 EMSCRIPTEN 分支导出空源并 STATUS 标注 wasm-sim only（不为 host/esp32 产源）。

生产链接**不得**带入 ① `app_callbacks.c`（重复 `wink_app_get_callbacks`）或 ② `test/mcs51/wasm/mcs51_wasm_link_stubs.c`（六个通道 `*_reset()` no-op 与真实 `pal_wasm_ch*.c` 重复定义）。受限 wasm 测试那套（regex 滤掉 `_chN`、node 桩 js 库、ENVIRONMENT=node）**不用于**生产。

### D3. 生产链接用真实通道 PAL + `wink_sim_js.js`，与 esp32 app 同形
mcs51 生产 wasm 复用全部 `pal_wasm_ch*.c` 真实通道 PAL 与生产 js 库 `wink_sim_js.js`（MODULARIZE `WasmSandbox`、WASM_BIGINT、ASYNCIFY、`-sERROR_ON_UNDEFINED_SYMBOLS=0`、exports 由 `exported_runtime_functions.json` 经 `wink.py gen wasm-export` 生成）。因此 mcs51 代理的 `js_pal_gpio_write`/`js_pal_gpio_read_state`/`js_pal_adc_read_norm` 与 esp32 app 走**同一 PinArbiter 桥**，headless 与 worker 装载同形。

### D4. 生产补 `mcs51_board_config.h` codegen
根 CMake 新增 custom command/target（`generate_mcs51_board_config`），跑 `${WINK_TOOLS_ROOT}/tools/codegen/generators/mcs51_board_config.py --input <wink-app.json> --output ${WINK_CONFIG_DIR}/mcs51_board_config.h`，DEPENDS app json + generator + `templates/mcs51_board_config.h.j2` + `boards/mcs51_devboard.json`。gate：`TARGET_PLATFORM=wasm` + `EMSCRIPTEN` + source 模式 + **app 板为 mcs51**（`_wink_app_is_mcs51`，由 `wink-app.json` 的 `"board"` 字段 regex 一次检出）+ generator 存在。生成进 `${WINK_CONFIG_DIR}`（已在 pal INTERFACE include 上，`wink_mcs51_compat` 经 `target_link_libraries(... PUBLIC pal)` 传递可见）。纯 button+LED 无 adc0832/heater 时生成头只含引脚 index 辅助宏（`MCS51_PIN_IDX_*`），无 ADC 宏，bridge `__has_include` 命中即可。

### D5. mcs51 app 绕过 C 侧 DAL 裁剪，强制 DAL/bal 为空 stub
mcs51 app 用**零 DAL 驱动**（button/led 是裸 pin + 前端插件）。根 CMake 检出 mcs51 板后：① 不调 `wink_dal_apply_pruning()`/`app_codegen.py`（会把前端 button/led 当 DAL 驱动拒绝）；② 内联 `list_drivers.py --cmake --mode=source` 取驱动表，把每个 `WINK_USE_<driver>` **CACHE FORCE OFF**，使 `dal`/`bal` 只编各自无条件 stub TU。为让 `dal` STATIC 目标在全裁剪后仍有源（此前 `dal` 无无条件源，全关即 `No SOURCES given to target: dal`），新增 `dal/src/wink_dal_stub.c`（空 TU + 占位符号），镜像 `bal/src/wink_bal_stub.c`，最终链接死剥离。

### D6. 阶段 0 验收 = unisim headless 场景；前端/模拟延后
阶段 0 以 `winkcli sim run --app <dir> --mode headless --scenarios <file> --reporter json` 端到端实证（该命令自动构建 WASM、从 `wink-app.json` 生成 `device-tree.json`、抽取资产到 `<app>/unisim-assets/`，再用真实 PinArbiter + 真实 button/led 插件跑场景）。**延后 Stage 1**（前端：Vue mcs51 画板、device-tree→manifest translator/resolver 去 esp32 硬编码、P1.0 等 pin 标签、DIP40 artwork、worker 装载 mcs51 wasm）与 **Stage 2**（模拟：host 桥 `js_pal_adc_read_norm` 由 stub 0.0 接 `arbiter.readAnalog`、adc0832/`thermal_heater_plate` 插件闭环）。

线性引脚约定（复用 ADR-0074 D3）：KEY=P3.2→**26**（active-low，0=按下），LED=P1.0→**8**（8051 板固件低电平点亮，LED 插件 `activeHigh:false`，LOW→on）。

## 4. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| 未修改 Keil 按键→LED 首次编译成**生产** `wink_simulator.wasm`，被真实 PinArbiter + 真实插件 headless 驱动 | mcs51 app 的 `wink-app.json` 是 C 构建与前端 device-tree 的**双消费者 SSOT**：button/led 用 `gpio_pin`（两 manifest 皆别名）+ `active_low`/`active_high` |
| 零 unisim 改动；与 esp32 app 同形 wasm，Stage 1 worker 直接装载 | mcs51 app 不经 `app_codegen.py`，靠板名 regex 检出绕开；新增 mcs51 板型须匹配 `mcs51` 子串 |
| 零回归：gate 仅 mcs51 app 置位；默认 esp32 app wasm 配置/构建不链 mcs51、不生成 mcs51 板头 | `dal` 新增无条件 stub TU（与 bal 对齐）；DAL 全关路径须持续验证不影响正常 app |
| ESP32 零增量（改动全在 wasm/EMSCRIPTEN 分支；mcs51 树仍 `if(ESP_PLATFORM) return()`）；无 `-fpermissive`；无硬编码 GBK 输入字符集；`#ifdef SIMULATION` 仍在最底层 | 生产 `winkcli/wink.py build sim` 对 mcs51 的一键资产打包已可用（`winkcli sim run` 即构建+抽资产）；纯 `build sim` 打包便利性记为后续 |

**资产入库约定**：`<app>/unisim-assets/device-tree.json` 与 `wink_simulator.js`（胶水 + 设备树，可评审）**入库**；`wink_simulator.wasm` 为构建产物，被 `.gitignore` 的 `*.wasm` 排除（同 avoidance_car/oled_dashboard 先例）。`device-tree.json` 由 winkcli 从 `wink-app.json` 生成，**不手工编写**。

## 5. 遵循与后续（Compliance & Follow-up）

- Accepted 后立即回写 Layer-① `02-wink-micro-os/07-mcs51-simulation-interception.md`（生产链接落点、M7/阶段 0 headless 已实证、测试矩阵补 headless 场景、DAL 空 stub）。
- Stage 1（前端）/Stage 2（模拟）见实施计划"延后"节；C 侧生产缝已就绪，Stage 1 不再改 C/链接。

**验收证据（2026-08-29）**：`winkcli sim run` 对 `mcs51_button_led` headless 场景 **7/7 步 PASSED**、`report.ok=true`、虚拟 2000000µs / wall ~189ms、退出码 0——插件按键按下 → 8051 经 `js_pal_gpio_read_state` 读到外部低电平 → `LED=0` → `js_pal_gpio_write(8,0)` → LED 插件 `on=true`（`gpio:8==0` 同步断言），释放后灭（`gpio:8==1`）。回归：MinGW host mcs51 **19/19**、emcc/wasm+Node **8/8**、arch lint（layering+api）**无发现**、默认 esp32 app（avoidance_car）wasm 配置+构建 **通过且 mcs51 gate 静默**、ESP32 零增量。

---

*本 ADR 状态变更请在此记录：*
- 2026-08-29：Proposed → Accepted（生产 mcs51 wasm 目标 + headless 在线实证阶段 0 端到端验证——`winkcli sim run` 7/7 PASSED、host 19/19、wasm 8/8、arch lint 无发现、默认 esp32 app 回归通过、ESP32 零增量；随即回写 Layer-①）。
