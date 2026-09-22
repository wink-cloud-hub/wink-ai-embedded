# Phase 2 剩余工作交接提示词（历史归档）

> **状态（2026-09-22）**：本提示词所列 **T2.3–T2.6 已全部执行完毕**（Phase 2 完成）。
> 执行记录见 [PLAN-20260921](2026-09-21-cms8s78xx-i2c-spi-deadlock-resolution-plan.md) **v2.0/v2.1**；ADR-0085/0086/0087 已 Accepted 并勾选 follow-up；ADR-0088（原 ADR-X1 通用 ABI 返回值模型）已 Accepted；Checklist 37/38 已摘牌。
> 本文件保留原始交接提示词**原文**（未改动），供复核与复现使用；第 3 节命令仍可直接验证当前状态。

---

## 任务：完成 CMS8S78xx I2C/SPI 死锁治理计划 Phase 2 剩余工作（T2.3–T2.6）

### 0. 工作区与权威文档
- 主仓（待改）：`D:\workspaces\ai-coding\wink-ai\wink-ai-embedded`
  - 计划书（v1.9，任务拆解与验收的 SSOT）：`docs/implementation-plans/mcs51/2026-09-21-cms8s78xx-i2c-spi-deadlock-resolution-plan.md`
- 私有仓（引擎/SDK，可读写）：`D:\workspaces\ai-coding\wink-ai\wink-ai`
  - 引擎：`packages/unisim`；SDK：`packages/unisim-sdk`；插件套件：`packages/wink-plugin-peripherals`（**junction** 指向 embedded 仓的 `wink-plugin-peripherals`，两路径等价，提交落到 embedded 仓）
  - ADR（私有）：`packages/unisim/docs/internals/decisions/`
    - `0085-i2c-transfer-ex-status-abi.md`（Accepted）
    - `0086-i2c-session-stream-abi.md`（Accepted）
    - `0087-spi-session-stream-and-cs-edge-abi.md`（Accepted，本任务主要契约）
  - ABI catalog：`packages/unisim/scripts/abi-catalog/abi-catalog.yaml`（`bun run check:abi-catalog` / `bun run gen:abi-catalog`）
- 先通读：计划书 §4–§8、ADR-0085/0086/0087、`AGENTS.md`、`docs/AGENTS.md`、`.agents/rules/c-code.md`、`.agents/skills/embedded-best-practice/SKILL.md`（写 C 前必读）。

### 1. 已完成（不要重做，只在此基础上扩展）
- Phase 1 全绿：SPI/I2C 片内模型（`cms8s_spi.cpp`/`cms8s_i2c.cpp` 的同步 Mock）、SFR 自旋防护、两个 vendor 微应用与 headless 收敛实证、CTest 全绿。
- T2.0：ADR-0085/0086 Accepted；双仓七步 ABI 落地：
  - `wink-micro-os/targets/wasm/wasm_bridge.h` 已有 `pal_i2c_result_t`、`js_pal_i2c_transfer_ex`、`js_pal_i2c_session_open/restart/write/read/close`，`PAL_WASM_ABI_HASH = 0x6B769176`（`pal_wasm_degradation.c`）。
  - `wink_sim_js.js`/`wink_sim_stub.js` 有 I2C 新符号垫片；`pal_wasm_ch2_bus.c` 有 `pal_wasm_i2c_transfer_ex` 导出；catalog 对应条目 `implemented`。
  - unisim 引擎：`I2CBus.transferEx` + ADR-0086 会话 FSM（单 port 互斥、池 4、ADDR_NACKED、close 幂等）、`BusDomainHandler` I2C 线级回调接线、桥接 8 字节 LE 结果编组；`bun run typecheck` 与 i2c 单测 17 例全绿。
- T2.1/T2.2：builtin 插件 `i2c_eeprom`（AT24C256）与 `spi_eeprom`（M95256，CS 上升沿 WEL 锁存/提交、WIP 窗口）；SDK stub-host 已有 I2C 与 SPI **两套会话助手**；沙箱 14/14 bundle、peripherals 125 用例通过。
- 仓库历史红项已修复（reset_controller/family_insulation/wdt_ta/wasm_node_smoke），embedded 全量 host CTest 应为 165/165 绿，必须保持。

### 2. 剩余任务（按依赖顺序执行）

#### T2.3-A：unisim 引擎 SPI 会话流（ADR-0087 实现）
文件：`packages/unisim/src/core/bus/spi-bus.ts`、`packages/unisim/src/types/runtime/spi.ts`、`src/core/domains/BusDomainHandler.ts`、`src/core/bridge/unisim-bridge-factory.ts`、`src/types/wasm/imports.ts`、`src/worker/wasm-physical-bridge.ts`。
- 按 ADR-0087 §1/§2 实现（与 `I2CBus` 对称，复用同一错误码语义）：
  - `SPIDevice` 扩展线级回调（`onExchangeByte/onTransactionStart/onTransactionEnd`，`csPin` 从设备配置透传）；
  - `SPIBus.transferEx(port, deviceId, tx, mode, sckHz)`：整帧，未知设备 `WINK_ERR_NOT_FOUND`（新错误常量，勿复用 I2C 的语义）；
  - 会话 FSM `sessionOpen/sessionTransfer/sessionClose`：单 port 互斥、全局池 `PAL_SPI_SESSION_POOL_MAX=4`、`close` 幂等、只走 `onExchangeByte`（整帧插件返回 `WINK_ERR_UNSUPPORTED`，不得静默降级）；
  - **CS 边沿绑定**：会话 open/close 时驱动设备声明的 `csPin`（经 `PinArbiter`，可用 `setCsSink(fn)` 注入宿主驱动函数；未声明 csPin 则不动引脚、不伪造边沿），时间线发 `frame-start/frame-end`；
  - `BusDomainHandler.spi` 接线线级回调 + csPin（当前只绑 `onFrame`）；
  - 桥接新增 `js_pal_spi_transfer_ex` 与 `js_pal_spi_session_*` 的转发（`imports.ts` 同步声明），失败经 `safeWrap` 报 8003。
- 测试：为 `spi-bus.ts` 写 `src/core/__tests__/spi-bus.test.ts`（或扩展现有用例）：会话开/关 CS 驱动、池满、port 忙、未知设备、整帧插件 UNSUPPORTED、复位先 close。

#### T2.3-B：embedded SPI ABI 七步同步（ADR-0087）
- `wasm_bridge.h`：声明 `js_pal_spi_transfer_ex`（7 参数，**无 data_width**）与 `js_pal_spi_session_open/transfer/close`（签名严格按 ADR-0087 §1 与 catalog 条目）；
- `wink_sim_js.js`：6 个新 import 实现（Module override 转发 + fail-closed 兜底：未知/无引擎返回 `-7`，`close` 返回 `0`）；`wink_sim_stub.js` 补 `knownBridgeSymbols`；
- `pal_wasm_ch2_spi.c`：新增 `pal_wasm_spi_transfer_ex` 测试导出面（对称 I2C `_ex`）；按 ADR-0087 裁决把 `device_id` 注释/映射改为逻辑设备号（旧 `cs_pin` 传参标废弃，不得破坏 legacy bool 路径）；
- 七步收尾：重算 `PAL_WASM_ABI_HASH`（算法见 `D:\workspaces\ai-coding\wink-ai\wink-ai\scripts\sync_wasm_abi_hash.py`；该脚本只更新 TS `EXPECTED_ABI_HASH`，C 侧哈希需手工写入 `pal_wasm_degradation.c`）→ `exported_runtime_functions.json` 加 `_pal_wasm_spi_transfer_ex` → catalog 条目 `proposed → implemented`（补 `c_header_line`/`ts_decl_line`）→ `bun run gen:abi-catalog`；
- 门禁：`winkcli lint --pack layering --pack api --pack wasm` 必须 0 error。
- 注意：hash bump 后重建两个微应用资产：`winkcli build sim --app i2c_master_at24c256` 与 `--app spi_master_95256`（其余 app 的已提交资产会变旧，可记录为后续批量重建）。

#### T2.3-C：I2C 会话余项（ADR-0086 未勾项）
- 引擎 I2C 会话表进入快照/state-hash（对照 `I2CBus` 现有实现与 worker 的录制/回放、`assertCompatibleRecording`）并 bump `WasmPhysicalBridge.MODEL_ABI_VERSION`（当前 `unisim-phase3-diagnostic-l0-v1`）；
- 复位/纤程退出：先对所有活动 I2C（及 SPI）会话执行 close（STOP/deassert CS），再清表；
- 插件侧 `serializeState` 已就绪（两个 EEPROM），接线到同一 state-hash。

#### T2.3-D：MCS51 控制器模型会话路由（Phase 2 数据面）
文件：`wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_i2c.cpp`、`cms8s_spi.cpp`、`wink-micro-os/frameworks/mcs51/src/mcs51_uni_bridge.cpp`（host fallback）。
- `cms8s_i2c.cpp`：把 `I2CMCR` 命令映射为 `_ex`/`session_*`（计划 §5.3.5 与 ADR-0086 §3 表）：START|RUN→open/restart、RUN→write/read(len=1, ACK/NACK)、STOP→close；`ADD_ACK/DATA_ACK` 由 `pal_i2c_result_t` 回填；ADDR_NACKED 语义保持；tWR 由插件 NACK 驱动。
- `cms8s_spi.cpp`：`SSCR.NSSO1` 边沿→`session_open/close`，`SPDR` 写→`session_transfer(len=1)`；Phase 1 的 SPSR 双步读清/计费保持不变。
- **host 链接硬要求**：`mcs51_uni_bridge.cpp` 目前只有 `js_pal_i2c_transfer`/`js_pal_spi_transfer` 两个 fallback；模型一旦调用 `_ex`/session，必须补全 4+6 个 host fallback（fail-closed），否则 host CTest 链接失败。
- **host 行为设计（关键点，实现前先定案并在注释说明）**：host 无总线引擎，import 返回 `-7`。推荐方案：为 host 提供**可脚本化的总线 mock**（仿 `js_pal_gpio_write` 的记录式 fallback，允许测试注入地址 ACK/读字节），CTest 用它驱动数据面；若成本过高，退而让模型在 `WINK_ERR_UNSUPPORTED` 时回退 Phase 1 片内 Mock（显式分支），但 wasm 路径不得伪造数据。
- 更新 `test_cms8s_i2c.cpp`/`test_cms8s_spi.cpp` 或新增 Phase 2 用例；embedded 全量 CTest 保持全绿。

#### T2.4：Headless 数据实证（双仓）
- 场景从"电平收敛"升级为**插件通道数据断言**：AT24C256 随机读/连续读/页写边界；M95256 WREN→WRITE→READ、WIP 轮询。
- 先调查现有断言能力：`ASSERT_BUS_PAYLOAD` 只解析 UART；计划 §5.5.1 要求走 `plugin:<id>/<channel>`。若引擎/headless 尚无插件通道断言，先做最小实现（插件状态通道 → 断言），或按计划 §5.5.1 注记在 T2.4 内补齐。
- 验收：`winkcli sim run --app i2c_master_at24c256 --mode headless --scenarios <app>/unisim-scenarios` 与 SPI 同形，退出码 0 且数据一致。

#### T2.5：Checklist 摘牌
- `docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md`（**gitignored，本地改**）§8 编号 37/38：`🟡 Phase 1 已解除运行阻塞` → `[x]`，依据 T2.4 证据；同步索引/说明文字。

#### T2.6：Layer-① 回写与 ADR 合流
- 回写 `docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md`：CH2 I2C/SPI 线级契约、会话路由与证据口径。
- ADR-0085/0086/0087 的 follow-up 勾选；删除悬空引用；启动 **ADR-X1**（通用 ABI 返回值模型）立项草案（仅需 Proposed + catalog 注记，或按计划留待评审）。

### 3. 验证命令速查
embedded（工作目录 = 仓根）：
- 构建+全量测试（MinGW）：`cmake -B build_host -S wink-micro-os -G "MinGW Makefiles" -DTARGET_PLATFORM=host -DWINK_APP_DIR=wink-micro-app/fixtures/unisim_smoke -DWINK_TOOLS_ROOT=D:/workspaces/ai-coding/wink-ai/wink-ai/packages/wink-tools`
  然后 `cmake --build build_host -- -k`；`ctest --test-dir build_host --output-on-failure`
- 门禁：`winkcli lint --pack layering --pack api --pack wasm`；`python wink-micro-os/frameworks/mcs51/tools/mcs51_shim_audit.py`；`python .github/scripts/check_license_map.py`；`python wink-micro-os/frameworks/mcs51/tools/lint/lint_fw_core_isolation.py`
- 资产：`winkcli build sim --app i2c_master_at24c256`、`--app spi_master_95256`；`winkcli sim run --app <name> --mode headless --scenarios wink-micro-app/vendor/cms8s78xx/<name>/unisim-scenarios`
unisim（工作目录 = `packages/unisim`）：
- `bun run typecheck`；`bun run test:gates`；`bun run check:abi-catalog`；`bun test src/core/__tests__/`
- SDK 源码改动后必须先 `bun run build`（`packages/unisim-sdk`，dist 为 gitignored，插件与引擎经它解析类型）
- 插件套件（工作目录 = `wink-plugin-peripherals`）：`bun run typecheck`；`bun test`；`bun run test:bundles`；`bun x oxfmt <改动文件>`
- ABI hash 同步：`python D:\workspaces\ai-coding\wink-ai\wink-ai\scripts\sync_wasm_abi_hash.py --embedded-root D:\workspaces\ai-coding\wink-ai\wink-ai-embedded`

### 4. 交付要求与纪律
- 每个子任务完成后跑对应门禁并给出**证据**（命令输出摘要/测试计数/退出码）；任一红灯不得宣称完成。
- 提交策略：按逻辑模块原子提交，英文 message（`feat(...)/fix(...)/ci(...)/docs(...)`），**仅在用户明确要求提交时执行**；两仓分别提交。
- 不得破坏：Phase 1 的 CTest（embedded 165/165）、沙箱 bundle（14/14）、`check:abi-catalog`、许可/分层门禁。
- 不引入动态内存、不用运行期多态、双 target（wasm/ESP32）同源；C 错误码负数语义；只改必要文件。
- 已知坑：
  - `wink-plugin-peripherals` 私有路径是 junction；编辑任一路径等价。
  - `docs/vendors/` 整体 gitignored（Checklist 与厂商 SDK 不入库）。
  - `wink-tools` 完整源码在 `D:/workspaces/ai-coding/wink-ai/wink-ai/packages/wink-tools`（CI 用的 `-DWINK_TOOLS_ROOT` 指向此处）；`wasm_node_smoke` 依赖 fixture 构建（已修复）。
  - `docs/implementation-plans/esp32/`、`docs/todolist/...` 非本任务产物，勿动。
  - 其余 vendor apps 已提交的 `unisim-assets` 在 hash bump 后会变旧（仅需重建两个 Phase 2 应用；如需全量重建，先与用户确认）。
- 完成后更新计划书（新增 vX.Y 执行记录）与相关 ADR follow-up；汇报时列出：改动文件、门禁结果、剩余风险。

### 5. 建议执行顺序
T2.3-A（引擎 SPI FSM + 测试）→ T2.3-B（ABI 七步 + 资产重建）→ T2.3-C（快照/复位）→ T2.3-D（模型路由 + host mock）→ T2.4 → T2.5 → T2.6。
每个阶段结束向我汇报一次（含证据），我确认后再进入下一阶段。

---

## 实际执行结果摘要（2026-09-22）

| 任务 | 状态 | 关键证据 |
| :--- | :---: | :--- |
| T2.3-A | ✅ | `SPIBus.transferEx`/会话 FSM/CS sink；`spi-bus` +18 例、core 71→74 例 |
| T2.3-B | ✅ | `PAL_WASM_ABI_HASH → 0xF402251A`（C/TS/catalog 三方一致）；catalog 4 条 `implemented` |
| T2.3-C | ✅ | 会话表 state-hash + 复位先 close；`MODEL_ABI_VERSION → …l0-v2` |
| T2.3-D | ✅ | `I2CMCR`/`SSCR`/`SPDR` 会话路由 + host 可脚本化 mock（wasm 无回落） |
| T2.4 | ✅ | I2C 8 步 `readbackHex="3233343536"`、SPI 7 步 `readbackHex="08"`，退出码 0 |
| T2.5 | ✅ | Checklist 37/38 `[x] ⚡ Level 3` |
| T2.6 | ✅ | 设计规范 §2.9 回写；ADR follow-up 闭环；ADR-0088 Accepted |
| 收尾（v2.1） | ✅ | 43 app 资产全量重建 + hash 运行时校验 43/43；legacy SPI 同帧实现；CTest 165/165、unisim 781 pass |
