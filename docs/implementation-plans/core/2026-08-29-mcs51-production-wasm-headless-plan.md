# 实施计划：mcs51 生产 wasm 目标 + headless 在线实证（阶段 0）

| 项 | 内容 |
|---|---|
| 创建日期 | 2026-08-29 |
| 状态 | **已完成（阶段 0 验收通过，2026-08-29）** |
| 关联 ADR | [ADR-0075](../../decisions/core/0075-mcs51-production-wasm-target-headless.md)（Accepted） |
| 关联设计规范 | [`docs/design/02-wink-micro-os/07-mcs51-simulation-interception.md`](../../design/02-wink-micro-os/07-mcs51-simulation-interception.md) §2.3 |
| 上游计划 | [`2026-08-27-mcs51-zero-code-simulation-plan.md`](2026-08-27-mcs51-zero-code-simulation-plan.md)（M0–M7，本计划承接其 M7 生产集成的阶段 0） |

## 1. 目标与范围

让**未修改 Keil 按键→LED 程序**编译成**生产级 `wink_simulator.js/.wasm`**，并用姊妹仓 unisim 的 **headless runner**（真实 PinArbiter + 真实 button/led 插件，无 Vue、无 worker）端到端实证。阶段 0 纯本仓 `wink-micro-os`/`wink-micro-app` + 调用 unisim headless，**不依赖任何前端改动，可独立验收**。

线性引脚约定（ADR-0074 D3）：KEY=P3.2→**26**（active-low，0=按下），LED=P1.0→**8**（固件低电平点亮，LED 插件 `activeHigh:false`）。

**不在阶段 0**（延后）：
- **Stage 1（前端）**：Vue mcs51 画板、device-tree→manifest translator/resolver 去 esp32 硬编码、P1.0 等 pin 标签、DIP40 artwork、worker 装载 mcs51 wasm。
- **Stage 2（模拟）**：host 桥 `js_pal_adc_read_norm` 由 stub 0.0 接 `arbiter.readAnalog`；adc0832 / `thermal_heater_plate` 插件闭环。

## 2. 任务拆分与执行结果

### T1 — 根 CMake：mcs51 框架自动链接 + 生产板级 codegen（`wink-micro-os/CMakeLists.txt`）✅
- 板名检出 `_wink_app_is_mcs51`（`wink-app.json` 的 `"board"` 字段 regex `mcs51`），**一次检出**兼作板级 codegen gate 与 DAL 绕开判据。
- 新增 `generate_mcs51_board_config` custom target：跑 `mcs51_board_config.py --input <wink-app.json> --output ${WINK_CONFIG_DIR}/mcs51_board_config.h`，DEPENDS app json + generator + 模板 + `boards/mcs51_devboard.json`；gate = wasm + emscripten + source 模式 + **mcs51 板** + generator 存在。
- arduino 自动链接块之后新增镜像块：`WINK_APP_MCS51 AND TARGET wink_mcs51_compat` → `target_link_libraries(wink_simulator PRIVATE wink_mcs51_compat)` + `add_dependencies(wink_mcs51_compat generate_config generate_mcs51_board_config)`（保序：STATIC 库不继承 exe codegen 构建序）。
- mcs51 板检出后：不调 `wink_dal_apply_pruning()`/`app_codegen.py`，内联 `list_drivers.py --cmake --mode=source` 取驱动表，把每个 `WINK_USE_<driver>` CACHE FORCE OFF（dal/bal 只编 stub）。
- **配套**：新增 `dal/src/wink_dal_stub.c`（空 TU 占位，镜像 `bal/src/wink_bal_stub.c`），`dal/CMakeLists.txt` 改为 `add_library(dal STATIC src/wink_dal_stub.c)`——修复全裁剪后 `No SOURCES given to target: dal`。

### T2 — 新生产 app `wink-micro-app/mcs51_button_led/` ✅
- `button_led.c`：自有样例（可提交，非 vendor 夹具），`#include <REGX52.H>`，`sbit KEY=P3^2; sbit LED=P1^0;`，`void main(void){ LED=1; while(1){ if(KEY==0) LED=0; else LED=1; _nop_(); } }`（SPDX Apache-2.0，注明 ADR-0075）。
- `wink-app.json`：`board: mcs51_devboard`、`tick_ms: 10`；devices `btn`（button, gpio_pin 26, active_low true）、`led`（led, gpio_pin 8, active_high false）——C 构建与前端 device-tree 的双消费者 SSOT。
- `CMakeLists.txt`：`mcs51_cleanup.py button_led.c → <gen>/button_led.cpp`（UTF-8，无 `--transcode`），强制 C++17；EMSCRIPTEN 分支导出 `WINK_APP_SOURCES`/`WINK_APP_INCLUDE_DIRS`/`WINK_APP_MCS51=TRUE` 后 `return()`；**不**跑 app_codegen、**不**提供 app_callbacks/device_tree；非 EMSCRIPTEN 导出空源 + wasm-sim-only STATUS。

### T3 — unisim 资产 + headless 场景（阶段 0 实证）✅
- `button-led.scenario.json`：header v2.0、timeoutUs 2000000、fail-fast、prngSeed 42；7 步（释放态 LED off → 按下 SET_PRESSED → LED on + gpio:8==0 → 释放 → LED off + gpio:8==1）。
- 经 `winkcli sim run --app <dir> --mode headless --scenarios <file> --reporter json --skip-toolchain-check` 自动构建 WASM + 生成 `unisim-assets/device-tree.json`（btn `1.l`:26/activeLow、led `A`:8/activeHigh:false）+ 抽 `wink_simulator.js/.wasm`。
- **结果：7/7 步 PASSED，report.ok=true，虚拟 2000000µs / wall ~189ms，退出码 0。**

### T4 — 文档 / ADR / 提交 ✅
- ADR-0075（D1 第二框架自动链接 / D2 框架拥有回调 / D3 真实通道 PAL + 生产 js 库 / D4 生产板级 codegen / D5 DAL 全关 + 空 stub / D6 阶段 0 headless）。
- Layer-① 回写 `07-mcs51-simulation-interception.md` §2.3 + 测试矩阵。
- 本实施计划。

## 3. 验收标准与验证证据（2026-08-29）

| 验收项 | 结果 |
|---|---|
| 生产 wasm 构建 | mcs51 app 配置/构建成功；日志 `[frameworks] MCS-51 interception enabled for App`；`mcs51_board_config.h` 生成于 build `generated/` |
| headless 实证 | **7/7 PASSED**，退出码 0（按下→LED on / gpio:8 LOW；释放→LED off / gpio:8 HIGH） |
| MinGW host mcs51 ctest | **19/19** |
| emcc/wasm + Node ctest | **8/8**（受限 harness 独立，不引用生产链接） |
| 默认 esp32 app 回归 | avoidance_car wasm 配置 + `wink_simulator` 构建通过；**mcs51 gate 静默**、不生成 mcs51 板头 |
| arch lint（layering + api） | **无发现** |
| ESP32 零增量 | 改动全在 wasm/EMSCRIPTEN 分支；mcs51 树仍 `if(ESP_PLATFORM) return()`；无 esp32 target 引用 `frameworks/mcs51` |

## 4. 风险与红线

- **红线**：ESP32 零 mcs51 增量；无 `-fpermissive`；无硬编码 `-finput-charset=GBK`（源 UTF-8，GBK 仅读 vendor 夹具）；RMW 复合赋值永不读外部脚（ADR-0071/0074，本任务不碰代理）；`#ifdef SIMULATION` 在最底层；vendor 夹具 `docs/vendors/` 只读不提交。
- **生产链接禁带**：`app_callbacks.c`（重复 `wink_app_get_callbacks`）、`mcs51_wasm_link_stubs.c`（重复通道 `*_reset`）。
- **实施中踩坑（已解）**：① 全裁剪 DAL 后 `dal` 目标空源 → 加 `wink_dal_stub.c`；② STATIC 库不继承 exe codegen 构建序 → `add_dependencies(wink_mcs51_compat generate_config[_mcs51_board_config])`；③ 板级 codegen gate 初版漏检板型，esp32 app 也打印 mcs51 信息 → gate 加 `_wink_app_is_mcs51`；④ `wink-app.json` 双消费者冲突（C DAL 裁剪 vs 前端插件树）→ mcs51 绕开 C app_codegen、由 winkcli 生成 device-tree；⑤ `unisim-sim.mjs` 拒绝直跑（需 launch ticket）→ 一律经 `winkcli sim run`。

## 5. 变更日志

- 2026-08-29：计划创建并经审批流程批准；T1–T4 全部完成，阶段 0 验收通过（headless 7/7、host 19/19、wasm 8/8、lint 无发现、esp32 回归通过、ESP32 零增量）；ADR-0075 Accepted 并回写 Layer-①。
