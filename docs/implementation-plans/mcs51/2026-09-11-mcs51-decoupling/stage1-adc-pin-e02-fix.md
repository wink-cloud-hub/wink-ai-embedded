# Stage1：ADC 物理引脚纠偏与 E-02 根除（立即生效）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S1-ADC` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
| **优先级** | 🔴 P0（阻塞性行为失真） |
| **关联 CPL** | CPL-01（32+ch）、CPL-02（ADCLDO）、CPL-17（契约）、CPL-22（测试双轨）、CPL-23（ABI 双读） |
| **前置依赖** | stage0（`caps_cache` 快照；双读告警码可先用硬编码，描述符字段就绪后转接---已完成） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 0. 版本与开关命名锁定（本阶段冻结，后续阶段只引用）

- **Rail key 双空间分区（本阶段冻结，后续阶段只引用）**：
  - `0~31` = MCU fabric 物理 Pin：任一家族的片上外设经芯片层映射后传入（如 CMS8S AN0→Pin 0）。
  - `32~63` = Board fabric 板级通道：device-tree/前端拥有，`devices/` 消费（如 ADC0832 CHx 沿用 `32+ch`）；core 不拥有任何一侧的映射知识。
  - 废除的是"片上路径借用合成区"（空间混淆），不是合成区本身。
- **引脚语义版本**：`v1` = 片上路径误用合成区（`32 + ch` 传 AN 通道，废弃中）→ `v2` = 上述双空间分区。`SimTraceSpecV2` 主版本不动，本升版记录于 ABI 版本附录。
- **双读开关**：芯片层 `cms8s_adc_dual_read_synth`（默认 ON，本阶段；stage7 删除整块逻辑）——**仅覆盖片上路径的 32+ch 误用；板级 32+ch 永久合法、不告警**。
- **可观测计数**：`cms8s_adc_synth_redirect_count`（片上合成通道重定向次数，单测断言旧用例告警且通过）。
- 发版顺序：仿真后端 → 前端插件 → stub/文档；回滚即关开关回 `v1` 语义（开关独立 commit）。

## 1. 目标

- ✅ 通用 rail 收 rail key（0~31 物理 Pin + 32~63 板级通道），core 不做任何映射；删除片上路径的 `32 + ch` 合成计算（板级 `32+ch` 由调用方显式传入，合法保留）。
- ✅ `cms8s_adc.cpp` 以 `AN_TO_PIN[26]` 常表闭环（P3 段 24~27），ADCLDO 基准下沉芯片层。
- ✅ ABI 升版 + 片上路径双读一版（合成通道误用告警 + 重定向），发版顺序后端→前端→stub/文档。
- ✅ 测试拆 `core-tests` / `cms8s-tests`，迁移期双绿；iron_ntc 板（ADC0832 路径）零回归。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `frameworks/mcs51/src/mcs51_adc.cpp`、`include/mcs51_adc.h` | ✏️ | rail key 接口（双空间）；注释改分区表 |
| `frameworks/mcs51/src/cms8s_adc.cpp` | ✏️ | `AN_TO_PIN` 常表 + ADCLDO + 片上双读兼容 |
| `frameworks/mcs51/src/mcs51_adc0832.cpp`、`include/mcs51_adc.h`（shim 段） | ✏️ | 仅 key 显式化（pull/注入点加 `32+`），CH API 与语义不变 |
| `frameworks/mcs51/src/mcs51_uni_bridge.cpp` | ✏️ | host 回退数组双轨（`[64]` 已分区，`[32]` 纯物理，两数组语义注释落盘） |
| `test/` 下 ADC 相关 | ✏️ | 拆 core/cms8s 两组；cms8s 用例注入点改 Pin 键，ADC0832 用例不动 |
| `test/mcs51/` → `frameworks/mcs51/test/` | 🚚 | 整体 `git mv`（unit/e2e/samples/wasm/apps）+ 中央 CMake 只改路径前缀；映射表落附录 |
| `test/mcs51/wasm/add_wink_wasm_mcs51_test.cmake` | ✏️ | 补齐手写源列表缺失项（Step 0a） |

架构红线：通用 rail 禁出现 AN 语义与 ADCLDO；公式法禁入（必须查表）。禁的是"片上 AN→合成区直传"；板级 key（32+ch）由 `devices/` 显式传入，合法保留。`mcs51_adc0832.cpp` 仅两处 key 显式化（pull 与注入 shim 加 `32+`，语义不变），其对外 CH API、用例、前端零改。

## 3. 任务拆分

### Task S1-0：L0 前置——wasm 断链修复 + 测试物理搬迁 `[状态: ✅ 已完成（2026-09-11，`f552b9e` + `2cb4fdd` + `25f62f9`）]`

- [x] **Step 0a（wasm 断链修复，master 已断）**：`test/mcs51/wasm/add_wink_wasm_mcs51_test.cmake:145-177` 手写源列表漏 `mcs51_context/family/peripheral/gpio/pcon/edge_queue/pwm_meter`（`g_active_mcu_context` 未定义即源于此），按 `_MCS51_COMPAT_SRCS` 为准补齐（`uni_bridge` 除外）；验收 `wasm_mcs51_iron_ntc` 链过。stage6 改链分目标库后彻底删除该手写列表。——执行注记：另补 `cms8s_buzzer/sys`（`mcs51_peripheral.cpp` 静态引用，缺则 undefined symbol）；`_SDK_ROOT` 由 `../../..` 改为 `../../../..`（helper 加深一级）；e2e 驱动按映射表分别进 `core/` 与 `cms8s78xx/`。验收：`wasm_` 11/11 全绿（含 `wasm_mcs51_iron_ntc`）。
- [x] **Step 0b（测试物理搬迁，`git mv` 不丢项）**：整体搬 `wink-micro-os/test/mcs51/{unit,*.c,samples,wasm,apps}` → `frameworks/mcs51/test/{core,cms8s78xx}/`（`samples`/`wasm`/`apps` 为共享子目录随迁）；归位规则：含 `cms8s`/XSFR/扩展向量/WDT 的归 `cms8s78xx/`，其余归 `core/`（双家族文件如 `silicon_seeds` 整文件归 `core/`）；注册保留中央 `test/CMakeLists.txt`（只改路径前缀，不复制 wiring）；落文件→目录映射表于本计划附录；"不丢"验收不用人工数数（仓库根目录执行）：搬迁前 `python docs/implementation-plans/mcs51/2026-09-11-mcs51-decoupling/assets/test-baseline-check.py --capture --test-dir build-host/wink-micro-os/test` 刷新基线，搬迁后同命令换 `--compare` 做**集合相等**判定——总数相同但名字一增一减也算失败。注意基线在脏树上捕获，搬迁前先在干净 pre-move 树重跑 `--capture` 刷新，再执行搬迁。——执行注记：基线在干净树（`f552b9e`）刷新（`dirty_files: 0`，140 项集合与旧基线完全一致）；`--compare` → `BASELINE MATCH`；host mcs51 轨 50/52（2 项为 S1-0c 已知基线例外）；`test_mcs51_cleanup.py` 随迁 `core/` 并修 `sys.path`（`../../tools`，自测 8/8）。映射表见附录 A。
- [x] **Step 0c**：后续 S1-1~S1-3 的"分组"即在此物理目录上操作，不再另建逻辑分组。"不丢"（集合相等）与"全绿"（结果门禁）是两个门：全绿看 verdict，已知例外（`wasm_mcs51_iron_ntc` 构建断链待 Step 0a、`test_mcs51_port_extint`/`test_mcs51_wink_mcu` MinGW 链接失败，均 master 基线复现）不计入搬迁回归。——执行注记：Step 0a 已消除 `wasm_mcs51_iron_ntc` 例外；剩余 2 项 MinGW 例外维持。
- [x] **Step 0c**：后续 S1-1~S1-3 的"分组"即在此物理目录上操作，不再另建逻辑分组。

### Task S1-1：通用 rail 收 key 化 `[状态: ✅ 已完成（2026-09-11，`2ee4e93`，与 S1-2 连体落地）]`

- [x] **Step 1**：`mcs51_adc_get_value` 改为收 rail key；`js_pal_adc_read_norm(key)` 直透；删 `32u + ch` 合成计算。ADC0832 侧做等效 key 显式化（模型 pull 点与 `mcs51_adc0832_*` 注入 shim 加 `32+`，语义不变，对外 CH API 不变）。
- [x] **Step 1b（rail 容量扩容，P0 阻断项）**：`MCS51_ADC_MAX_CHANNELS (32u)` 扩容并改名 `MCS51_ADC_MAX_RAIL_KEYS = 64u`（6 处同改：`mcs51_adc.h` 定义、`mcs51_adc.cpp` ×3 越界检查、`mcs51_context.h` ×2 数组）；`adc_injected`/`adc_inject_flag` 同步 64 槽（+96B，记 stage2 §4 预算表；host 回退 `[64]` 本就分区，一致）。不扩则板级 key 32/33 被越界拦截直接返 0，iron_ntc 全红。
- [x] **Step 2**：`mcs51_adc_set_vref/vrail` 转为芯片层调用的通用 rail 参数（通用头去 ADCLDO 注释）。——执行注记：`do_adc_conversion` 与 `cms8s_adc_model_reset` 的直接结构体写入改为 setter 调用。
- [x] **Step 3**：验证养生壶冷启动 NTC 室温读数，E-02 消失；同步验证 iron_ntc 板（ADC0832 路径）读数无回归。——执行注记：`test_mcs51_adc_refchain`（pull 轨 AN0→Pin 0：2048/2252/1024）+ `test_mcs51_cms8s_adc_e2e`（Pin key 注入）+ `test_mcs51_iron_ntc_e2e`（板级 32+ch 零回归）全绿；跨仓真链路终证留待 stage7 headless 门。

### Task S1-2：CMS8S 闭环 + 双读兼容 `[状态: ✅ 已完成（2026-09-11，`2ee4e93`，与 S1-1 连体落地）]`

- [x] **Step 1**：新增 `AN_TO_PIN[26] = {0..7, 8..15, 16..21, 24..27}` + 越界钳位 + `static_assert(AN_TO_PIN[22]==24)`。——执行注记：`do_adc_conversion` 经表取 Pin key；`adc_channel_cfg_addr`（XSFR 地址域，非引脚域）保留；另修正 `instant` §8 注释 `P3.1`→`P3.3`=Pin 27（原注释与 `0xF033` 布设矛盾）。
- [x] **Step 2**：片上合成通道误用（旧前端/旧用例以 32+ch 传 AN）在芯片层兼容一版，判定与重定向发生在芯片层（rail 本身无告警、无重定向）。探测规则（`do_adc_conversion` 内，pull 原值上判定，钳位/缩放前；注入轨直通不受影响）：
  1. 主读 Pin key：`pin_val = pull(AN_TO_PIN[ch])`；
  2. 启发式回退（仅兼容开关 ON 时）：若 `pin_val == 0.0f`（旧前端未驱动物理 Pin）**且** `synth_val = pull(32u + ch) > 0.0f`，则判定旧前端误用，采纳 `synth_val` + 告警 + 计数。单纯 `raw == 0` 禁止作为回退依据——真实 0V（短路故障的真值）必须原样上报，否则掩盖真短路；
  3. 已知残留风险（接受项）：CMS8S 板上 ANx 浮空 **且** ADC0832 同名合成 key 被驱动时可能误重定向；缓解 = 兼容开关 + 告警计数可观测 + 仅存活一版（stage7 删除）。
- [x] **Step 2b（兼容单测）**：注入合成 key（模拟旧前端驱动）→ 断言重定向值 + 告警 + 计数自增；注入 0 值合成 key → 断言不重定向（真 0V 保护）；板级 32+ch 正常 pull → 不告警不计数。板级 32+ch 走 ADC0832 正常 pull，不在双读之列（永久合法）。——执行注记：落 `test_cms8s_adc_instant.cpp` §14 四组（重定向 2048+计数 1 / 真 0V 不计数 / 开关 OFF 不重定向 / Pin 注入直通不计数）；板级不告警由检测点位（仅片上路径）结构保证 + ADC0832 用例全绿。
- [x] **Step 3**：ADCLDO.VSEL 基准计算迁入芯片层。——执行注记：早已在芯片层（`do_adc_conversion` Gate 0），本阶段仅将结构体直写改为 setter 调用 + 通用头去 ADCLDO 注释，无行为差。
- **回滚粒度偏离说明**：计划要求双读开关单独提交；实际 S1-1/S1-2 同文件交织（`cms8s_adc.cpp` 查表与双读同函数），拆 hunk 风险大于收益，故单 commit `2ee4e93` 落地。独立回滚能力由运行时开关等效提供（`cms8s_adc_dual_read_synth=false` 即回 v1 语义，§14(c) 已覆盖），stage7 删整块逻辑不变。

### Task S1-3：契约升版 + 测试双轨 `[状态: ⏳ 部分完成（Step 2/3 ✅，Step 1 待 board 编号确认，见附录 B）]`

- [ ] **Step 1**：C-ABI 引脚语义升版记录（版本号/发版顺序/回滚步骤）+ rail 双空间分区表归档。分区表 board 区编号以向前端口头确认值为准：执行前先确认 iron_ntc NTC→32+0 绑定（当前契约注释值）；若不一致，以确认值为准调整 board 区编号并同步 ADC0832 key 常量——本仓无法验证外仓行为，禁止假设。——执行注记（2026-09-11）：除 board 编号外的全部记录已落附录 B（版本/MCU 区/开关/发版顺序/回滚/L3 关闭条件）；board 区保持 TBD，确认即转正打勾。
- [x] **Step 2**：测试按 `core`（物理 Pin、标准向量）与 `cms8s`（AN 映射、扩展向量）分组，双绿。其中 `test_mcs51_cms8s_adc_e2e.c:34-36` 注入点由通道号 0/1/25 改为 rail key（Pin）0/1/27；ADC0832 相关用例（`test_adc0832_dio_shared.cpp`、`test_mcs51_adc0832_e2e.c`、`test_mcs51_iron_ntc_e2e.c`）保持 32+ch key 不变；另 `test_mcs51_adc_refchain.cpp:78,89,147` 系全仓唯一 pull 轨测试（`host_set_analog_norm(32u,…)`），按其断言的 AN 通道换算为 Pin key 后驱动（规则同 cms8s e2e；`grep host_set_analog_norm` 全仓仅此一文件，边界闭合）。——执行注记：物理分组由 S1-0 落地，键位切换由 S1-1/S1-2 落地（`2ee4e93`），最新全量 52/54（2 基线例外）+ `wasm_` 11/11。
- [x] **Step 3**：host 回退数组随版本双轨。——执行注记：`[64]` 本就分区（S1-1 落语义注释），`[32]` 纯物理（ext_pin 数字轨）；pull 轨版本行为由 `test_mcs51_adc_refchain`（Pin 0 驱动：2048/2252/1024）锁定，无版本分支残留。

## 4. 验收

- L0/L1：双轨全绿；片上旧合成通道用例告警但通过；板级 32+ch 用例无告警通过。
- L2：养生壶场景冷启动 NTC≈25℃，无 E-02；iron_ntc 板读数与基线一致。
- L3：ABI 版本记录（含双空间分区表）归档。
- L4：`mcs51_adc.*` 无 `32u +` 合成计算（key 由调用方显式传入）；`mcs51_adc0832.cpp` + `mcs51_adc0832_*` shim 的 diff 仅含 key 显式化（`32+`），无语义变更。

## 附录 A：测试文件→目录映射表（S1-0b 执行记录，2026-09-11，commit `25f62f9`）

源根 `wink-micro-os/test/mcs51/` → 目标根 `wink-micro-os/frameworks/mcs51/test/`。共享子目录整体随迁：`samples/`（11）、`wasm/`（7）、`apps/iron_ntc/wink-app.json`（1）。

### A.1 `cms8s78xx/`（15：unit 14 + e2e 1）——含芯片头/AN 语义/扩展向量/XSFR/WDT 实质驱动

| 文件 | 归位依据 |
|------|---------|
| `test_cms8s_adc_instant.cpp` / `test_cms8s_buzzer.cpp` / `test_cms8s_vendor_stdriver.cpp` | `#include cms8s_*`，片上外设专属 |
| `test_mcs51_adc_refchain.cpp` | `#include cms8s_adc.h` + LDO/VSEL/AN0（S1-1/S1-2 已改 Pin key：pull 0u 驱动） |
| `test_mcs51_cms8s_adc_e2e.c` | AN0/1/25 通道语义（S1-1/S1-2 已改注入 Pin key 0/1/27） |
| `test_extint_model.cpp` | 驱动 `XSFR_PS_INT*` 引脚复用 |
| `test_mcs51_gpio_dir.cpp` | 直写 `0xF00A`（P0UP）/`0xF000`（P00CFG=AN0，GAP-25 模拟子项） |
| `test_mcs51_irq_arbitration.cpp` | `#include cms8s_adc.h` + EIE2/EIF2 扩展向量 |
| `test_mcs51_port_extint.cpp` | `#include cms8s78xx.h`（P0EXTIE 端口中断） |
| `test_mcs51_t234_fsys.cpp` | T3/T4 扩展定时器 |
| `test_mcs51_uart_charge.cpp` | `XSFR_BRT_*` 波特率定时器 |
| `test_mcs51_uart_tx_ready.cpp` | `XSFR_PS_RXD`/`P22CFG` 重映射 |
| `test_mcs51_wdt_ta.cpp` | WDT/TA 保护 |
| `test_mcs51_wink_mcu.cpp` | 定义 `WINK_MCU_CMS8S78XX` + ADCLDO XSFR 代理断言 |
| `test_mcs51_xsfr_tripwire.cpp` | XSFR 窗口 tripwire（CMS8S 家族概念，classic 无窗口） |

### A.2 `core/`（36：unit 24 + e2e 11 + py 1）——标准 8051 + 板级器件通用语义

unit 24：`mcs51_clock_user.cpp`、`mcs51_static_tu_{a,b,c}.cpp`、`test_adc0832_dio_shared.cpp`（板级器件，S1-3 保持 32+ch）、`test_mcs51_classic_bus.cpp`、`test_mcs51_clock_quantum.cpp`、`test_mcs51_edge_queue.cpp`、`test_mcs51_family_schema.cpp`（双家族描述符断言，机制属 core）、`test_mcs51_gap12.cpp`、`test_mcs51_gpio_dual_path.cpp`（仅 0xFF 锁存值）、`test_mcs51_low_power.cpp`（主体 PCON 通用，仅 1 行 EICFG 附带）、`test_mcs51_shims.cpp`、`test_mcs51_silicon_seeds.cpp`（计划明示整文件归 core）、`test_mcs51_soft_pwm.cpp`、`test_mcs51_timer_ext_clk.cpp`、`test_sfr_edge_dispatch_accuracy.cpp`、`test_sfr_operators_coverage.cpp`、`test_sfr_rmw_latch_integrity.cpp`、`test_static_init_safety.cpp`、`test_uart_isr_dispatch.cpp`、`test_uart_rx_model.cpp`、`test_unisim_clock_mapping.cpp`、`test_mcs51_xram_aperture.cpp`。
e2e 11：`test_mcs51_blinky_host.c`、`test_mcs51_timer0.c`、`test_mcs51_uart.c`、`test_mcs51_uart_echo_e2e.c`、`test_mcs51_gpio.c`、`test_mcs51_gpio_external_e2e.c`、`test_mcs51_int0_e2e.c`、`test_mcs51_seg_display_e2e.c`、`test_mcs51_sfr_rmw_isolation.c`、`test_mcs51_adc0832_e2e.c`、`test_mcs51_iron_ntc_e2e.c`（ADC0832 板级路径，S1-3 零回归锚点）。
工具 1：`test_mcs51_cleanup.py`（通用清洗自测，随迁并修 `sys.path` 为 `../../tools`）。

### A.3 构建接线变更（与搬迁同 commit，原子落地保绿）

- 中央 `wink-micro-os/test/CMakeLists.txt`：仅改路径前缀（`mcs51/unit/`→`../frameworks/mcs51/test/{core,cms8s78xx}/` 等，不复制 wiring）。
- `frameworks/mcs51/test/wasm/add_wink_wasm_mcs51_test.cmake`：`${_SDK_ROOT}/test/mcs51/`→`${_SDK_ROOT}/frameworks/mcs51/test/`；`_SDK_ROOT` 由 `../../..` 改为 `../../../..`（helper 加深一级）；e2e 驱动按 A.1/A.2 分组。
- 验证：`--compare` → `BASELINE MATCH`（140=140）；host mcs51 50/52（2 项 S1-0c 已知例外）；`wasm_` 11/11；cleanup 自测 8/8。

## 附录 B：引脚语义升版 interim 记录（S1-3 Step 1，板区编号 TBD）

- **版本**：`v1`（片上路径误用合成区：以 `32+ch` 传 AN 通道，废弃中）→ `v2`（双空间分区）。`SimTraceSpecV2` 主版本不动。
- **MCU 区（冻结，本阶段生效）**：`0~31` 物理 Pin；CMS8S 经 `AN_TO_PIN[26]` 映射（`{0..7, 8..15, 16..21, 24..27}` + `static_assert`）；classic 直通；core 不拥有映射知识。
- **Board 区（TBD，阻塞项）**：`32~63`，编号归属 device-tree/前端。当前契约注释值（iron_ntc NTC→`32+0`）未经外仓确认，**禁止假设**——本表 board 区编号在确认前保持 TBD；确认后落盘并同步 ADC0832 key 常量（若与 32+0 不一致）。
- **双读开关**：`cms8s_adc_dual_read_synth`（默认 ON，本阶段；stage7 删除整块）+ 可观测计数 `cms8s_adc_synth_redirect_count`；仅覆盖片上路径误用，板级 `32+ch` 永久合法不告警。
- **发版顺序**：仿真后端 → 前端插件 → stub/文档。
- **回滚**：运行时 `cms8s_adc_dual_read_synth=false` 即回 `v1` 语义（§14(c) 覆盖）；或 `git revert 2ee4e93`（S1-1/S1-2 原子 commit）。
- **L3 关闭条件**：board 编号确认落盘 → 本附录转正，S1-3 Step 1 打勾，stage1 全关。

## 5. 风险与回滚

- R-01（版本错配）：双读 + 发版顺序缓解；回滚：关闭双读重定向回旧语义（开关单独提交）；Git revert 本阶段 commit。

## 6. 阶段自审自我检验清单（Self-Audit Checkpoint）
- [x] **目录与接口落位**：`mcs51_adc.cpp` 收 rail key 直透，无合成计算残留；`mcs51_adc0832.cpp` + shim 仅 key 显式化，无语义变更。（L4 grep 实测：core 合成零命中；3 处 `32u+ch` 全为调用方显式板级 key。）
- [x] **E-02 闭环自审**：养生壶冷启动场景室温阻值正常，数码管假短路告警 E-02 彻底消除。（机制证明：pull 轨 AN0→Pin 0 由 `refchain` 2048/2252/1024 锁定 + e2e Pin key 注入全绿；字面养生壶 app 冷启动跨仓真链路留 stage7 headless 门，与 S1-1 Step 3 注记一致。）
- [x] **iron_ntc 无回归**：ADC0832 路径（32+ch 板级 key）读数与基线一致，线上 pull 链路不断。（host + wasm e2e 全绿。）
- [x] **兼容双读验证**：片上旧用例以 32+ch 传 AN 时触发告警且计数正确自增；板级 32+ch 用例不告警。（§14 四组全绿；板级不告警由检测点位结构保证。）
- [x] **双轨状态**：`core-tests` 与 `cms8s-tests` 均全绿。（52/54 + 11/11；2 项为 S1-0c 基线例外；`--compare` 142=142。）

## 7. 自审签署（2026-09-11，S1-3 Step 1 除外）
- **Check 1–5**：全部通过（证据见上）。S1-0/S1-1/S1-2/S1-3-Step2/Step3 关闭。
- **唯一开口**：S1-3 Step 1 的 board 区编号（待外仓确认，附录 B interim 记录已就位，L3 关闭条件明确）。stage1 状态为"除一步外全关"，不得标 ✅；stage3+ 可并行推进（无文件冲突：stage1 剩余仅文档落盘），但 stage7 关闭前必须回填此口。
