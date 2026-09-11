# Stage3：头文件体系与命名空间归位

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S3-HEADERS` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | ✅ 已完成（2026-09-11，自审结论见 §7） |
| **优先级** | 🟡 P1 |
| **关联 CPL** | CPL-09（桥裸包含/硬调用）、CPL-13（sfr_map）、CPL-14（公共头）、CPL-21（ADC0832）、CPL-24（前缀落地） |
| **前置依赖** | stage2（context 已纯净） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ `REG_CMS8S78XX.H` 等下沉 `chips/cms8s78xx/include/`；`mcs51_sfr_map.h` 缩水为 Intel 标准，专有部分改 `CMS8S_` 前缀下沉。
- ✅ `wink_mcu.h` 上移出 51 框架，51 内仅留 51 路由；`wink_mcs51_wdt.h` 去 `cms8s_sys_notify` 硬导出。
- ✅ `mcs51_bridge.cpp` 去裸 include，TA 改钩子派发；ADC0832 下沉 `devices/`。
- ✅ 转发 shim 保留一版（告警），下阶段删除；前缀 lint 全绿。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `chips/cms8s78xx/include/` | 🆕 | REG/cms8s 头 + `cms8s_sfr_map.h` + allowlist 下沉 |
| `include/mcs51_sfr_map.h` | ✏️ | 仅标准 SFR（`T2CON` 保留，`CKCON` 地址可提语义下沉） |
| `include/wink_mcu.h` | ✏️/搬迁 | 上移公共层（新建 `wink-micro-os/include/` 并评估构建引用，51 内残留改名 `mcs51_family_route.h`） |
| `include/wink_mcs51_wdt.h` | ✏️ | 去厂商硬导出 |
| `src/mcs51_bridge.cpp` | ✏️ | 去裸 include，去 TA 硬调用（TA hook 首注册保序） |
| `devices/adc0832/` | 🆕 | 状态机下沉：`include/adc0832.h` + `src/mcs51_adc0832.cpp`，trap 接入 |
| `tools/mcs51_shim_audit.py` + freshness 门禁 | ✏️ | `include/REG_CMS8S78XX.H` 硬编码路径随头搬迁同步；allowlist 生成目标改为 chips 路径 |
| 消费侧同步（本阶段遗漏即断链） | ✏️ | 6 个 `wink-micro-app/mcs51_*` 应用头引用、`tools/mcs51_cleanup.py` 重写目标、`tools/sdcc_gate/` 分发头、`test/CMakeLists.txt` facade 测试与 shim 抑制段、unit 测试直引 `cms8s_adc.h`/`ADC0832.H` 处、`test/.../samples/cms8s_adc_test.c` |

## 3. 任务拆分

### Task S3-1：寄存器表与厂商头下沉 `[状态: ✅ 已完成（2026-09-11，73ea58f + 49ce3eb + e4b9041 + 0a652bd）]`

- [x] **Step 1**：建 `chips/cms8s78xx/include/` 并搬移 4 头（`REG_CMS8S78XX.H`, `cms8s78xx.h`, `cms8s_adc.h`, `cms8s_buzzer.h`）+ `mcs51_xsfr_allowlist.h`（改名 `cms8s_xsfr_allowlist.h`）；原路径留一版转发 shim（S3-D1：`#pragma message` + `TODO(stage7)`，见 §7）。
- [x] **Step 2**：`mcs51_sfr_map.h` 删 `P0EXTIF/T34MOD/EIE2/EIF2/PS_*`（仅留标准 `T2CON` + 通用地址 `CKCON`），专有寄存器改 `CMS8S_` 前缀下沉到 `chips/cms8s78xx/include/cms8s_sfr_map.h`（新建）；过渡期 core TU（extint/timer/uart，stage4 连代码一起走）直引芯片头，lint 加 stage4 到期过渡 waiver（S3-D2）。
- [x] **Step 3**：`wink_mcu.h` 上移至 `wink-micro-os/runtime/include/wink_mcu.h`（`runtime/include/` 已存在且三条消费链——app `sample_common.cmake`、test（lib PUBLIC 透传）、wasm `_WASM_MCS51_INCLUDES`——均已含该目录，零新增 `-I`，6 app 零改）；51 内残留瘦身并改名 `mcs51_family_route.h`（禁留同名文件，lint 加 by-design waiver）；`test_mcs51_wink_mcu` 转测路由头（ctest 名保留）；同步 §2 消费侧表全部条目（sdcc_gate 系 standalone 无需改；cleanup 重写目标仅头名不变无需改；test 内无其他引用）。
- [x] **Step 4**：`mcs51_shim_audit.py` 的 `REG_CMS8S78XX.H` 路径与 allowlist 生成目标同步到 chips 路径（freshness 门禁同改；isr 交叉校验的家族化留 stage5）。验证（2026-09-11 实测）：`--check-xsfr-allowlist chips/.../cms8s_xsfr_allowlist.h` → `fresh: 93 addresses`；全量漂移审计 → `No hard mismatches`。
  `python wink-micro-os/frameworks/mcs51/tools/mcs51_shim_audit.py --check-xsfr-allowlist wink-micro-os/frameworks/mcs51/chips/cms8s78xx/include/cms8s_xsfr_allowlist.h`；
  重生成：`--emit-xsfr-allowlist` 同路径（输出与检入文件 `diff` 为空方为绿）。

### Task S3-2：桥 + WDT + ADC0832 `[状态: ✅ 已完成（2026-09-11，d7d0563 + 8b9bc1d + abb13e3）]`

- [x] **Step 1**：bridge 删 `ADC0832.H`/`cms8s_adc.h`/`wink_mcs51_wdt.h` 裸包含与 `MCS51_HAS_ADC0832`/`board_config` 探测块；`cms8s_sys_notify_sfr_write` 改为 per-context `sfr_write_notify` 槽派发（S3-D3：芯片 init 以显式 ctx 直接安装，无新注册 API；TA 首派发保序不变，GAP-07 约束见总纲 §3.1b-5）。
- [x] **Step 2**：`wink_mcs51_wdt.h` 去厂商导出（含注释去厂商味，全文件零残留）；真通用语义保留原地。附带清掉同属 stage3 的 `wink_mcs51_clock.h`/`wink_mcs51_strict.h` 文档残留（数值冻结不动）；`wink_mcs51_extint.h` 的 mux 文档随代码滚到 stage4（S3-D4）。
- [x] **Step 3**：新建 `devices/adc0832/include/` 与 `devices/adc0832/src/`，将 `include/ADC0832.H` 与 `src/mcs51_adc0832.cpp` 搬移至对应目录下（规范小写头 `adc0832.h`；`git mv -f`；原路径留 `#pragma message` + `TODO(stage7)` 转发 shim，S3-D1）；同步更新 wasm 手写源列表中的文件路径 + 追加 devices include 目录（单体库侧 include 目录同加 PUBLIC，stage6 重定作用域；总纲 §6.1-7 双列表规则）；对外提供 `adc0832_device_attach(ctx, cfg)`（POD 配置：8 引脚 + `ch0/ch1_net_id`，方案 B）经 trap 挂载 + `adc0832_ch_key()` 映射 + `mcs51_adc0832_init` 8 参数兼容包装（默认 32/33）+ `mcs51_adc0832_*` 通道 API（由 core 头迁入）；core 头 shim 已删，rail 保持 64 槽；`MCS51_HAS_ADC0832` 全仓仅 board config（生成/app 侧）与测试 harness 命中；3 个测试迁移（include 换新 + iron 在 hook 里自绑定 codegen 引脚）；02/03 设计规范回写同步。
- [x] **Step 4（板级模拟空间日落决策，日落条款）**：✅ **当期定案选择方案 (B)**（设备私有 pull，board net id 进 `adc0832_device_attach` 参数，rail 回归纯 Pin）。详细对比、采纳理由与落地契约见 **附录 A**。

## 4. 验收

- L0：全仓 include 链不断（shim 期零 error，告警可接受）。
- L4：公共 `include/` 无 `REG_CMS8S78XX.H`；bridge 编译不依赖芯片/器件头；`grep -rn 'MCS51_HAS_ADC0832' src/mcs51_bridge.cpp` 零命中（全仓仅 `devices/` 与 app 侧命中）；前缀 lint 全绿。

## 5. 风险与回滚

- R-03（include 断裂）：shim 缓解；回滚：先 revert 搬移 commit，shim 独立 commit 可单独回退。

## 6. 阶段自审自我检验清单（Self-Audit Checkpoint）
- [x] **目录落位**：`chips/cms8s78xx/include/`（5 头 + 新建 `cms8s_sfr_map.h`）与 `devices/adc0832/{include,src}/` 文件物理路径 100% 符合终态目录树（`git mv` 保留更名链）。
- [x] **通用纯净度**：公共 `include/` 零厂商实质头（仅一版转发 shim，S3-D6）；`src/mcs51_bridge.cpp` 零裸包含 `ADC0832.H`/`cms8s_adc.h`（连 `wink_mcs51_wdt.h` 与 board 探测一并摘除）；`wink_mcs51_wdt.h` 全文件零厂商残留。
- [x] **转发兼容性**：保留的转发 shim 以 `#pragma message` 提示（S3-D1）且三工具链编译不报错（host `-Werror` / wasm emcc 全绿）；`TODO(stage7)` 可被 stage7 残留 grep 捕获。
- [x] **日落决策**：板级模拟空间去留已定案选择 **方案 (B)** 并记录于附录 A，后续阶段只执行结论（本阶段：net-id 映射落器件、私有 pull 链路归器件所有，rail 保持 64 槽）。
- [x] **双轨状态**：host `mcs51` 64/64 + wasm 13/13 + `--compare` 142=142；lint PASS；allowlist fresh（93 地址）+ 全量漂移审计无硬失配。

## 7. 自审签署（2026-09-11）
- **Check 1 目录落位**：通过。`chips/cms8s78xx/include/`（`REG_CMS8S78XX.H`/`cms8s78xx.h`/`cms8s_adc.h`/`cms8s_buzzer.h`/`cms8s_xsfr_allowlist.h` + 新建 `cms8s_sfr_map.h`）、`devices/adc0832/{include/adc0832.h,src/mcs51_adc0832.cpp}`、`wink-micro-os/runtime/include/wink_mcu.h`、`include/mcs51_family_route.h` 全部合树；51 内无同名 `wink_mcu.h` 残留；`test/CMakeLists.txt` 中央注册未复制 wiring。
- **Check 2 依赖单向与残留**：通过。lint PASS（新增：route 头 by-design waiver 2 行；extint/timer/uart stage4 到期过渡 waiver 3 行；sfr_map/wdt 的 stage3 waiver 自然脱钩）；core 头零 `chips/`/`devices/` 私有头包含；`mcs51_context.h` 无厂商 priv 头包含（union 穿透哨兵复查通过）；`MCS51_HAS_ADC0832` 在 `src/mcs51_bridge.cpp` 零命中，全仓代码命中仅 board config（生成/app 侧）与 iron 测试 harness（板侧）。
- **Check 3 双轨与契约**：通过。host `mcs51` 64/64（含 iron 闭环、TA 双模、路由、freshness）、wasm 13/13、`--compare` 142=142 集合相等；`sizeof(Mcu51Context)` 75672 实测不变（notify 槽被对齐填充吸收，零净增，预算测试 ceiling 内通过）；E-02 链（refchain/cms8s_e2e/iron）全绿无复发。17 项非 mcs51 失败与本阶段零文件交集（PAL/DAL `-Werror` 存量 + 缺外仓 codegen），判 master 先存问题（与 stage0 先例同口径）。
- **Check 4 计划闭环**：S3-1/S3-2 checkbox 全勾；S3-D1–D6 六项裁决归档如下；总纲 §5 stage3 状态列待置 `✅ 已完成`（与本提交同批）。
- **执行裁决 S3-D1（shim 提示形式）**：计划字面 `#warning` 不可执行——MSVC 无 `#warning`；GCC 在本仓测试 `-Werror` 下将其变硬错误，击穿 L0。改 `#pragma message(...)` + `TODO(stage7)`（三工具链只提示不报错；stage7 残留 grep 已覆盖 `TODO(stage7)`）。
- **执行裁决 S3-D2（过渡 waiver）**：stage4 前 extint/timer/uart 代码仍在 core，需读已下沉地址——直引芯片头 + lint 加 stage4 到期 waiver（代码与地址 stage4 同行）；route 头路由宏加 by-design waiver（与描述符表同例）。
- **执行裁决 S3-D3（TA 钩子形态）**：未新增注册 API——芯片 init 以显式 ctx 直接装槽（零 active 依赖，per-context 天然成立）；`trap_reset` 按 hook 类纪律同清（reset memset/外设 init 重装，TA 单测双模全绿为证）。
- **执行裁决 S3-D4（extint 文档残留滚存）**：`wink_mcs51_extint.h` 的 mux 文档描述 stage3 现行行为，随代码滚到 stage4 同删；clock/strict/wdt 文档残留本阶段清零。
- **执行裁决 S3-D5（新 attach 取 POD 配置形）**：附录 A 要求 attach 增 `ch0/ch1_net_id` 参数——10 位置参违反参数上限纪律，落为 `Adc0832Config` POD（2 参数）；8 参数 `mcs51_adc0832_init` 兼容包装保留（默认 32/33）。
- **执行裁决 S3-D6（L4 无头条款释义 + app 绑定缺口）**：L4"公共 include/ 无 REG"指无实质内容（一版转发 shim 存活至 stage7）；`mcs51_health_pot` 等外管线 app 在本仓构建图外，其中央桥自绑定缺口由 stage6 codegen 胶（family_select 系）认领，stage7 headless 真链路为终证——本阶段在仓 e2e 代理（iron）已迁 harness 自绑定。
- **Safety review**：Risk level 中（桥派发热路径 + 外设 init + ctx 结构）；Checklist phases run 1、2、3、4、10、12；Findings 无（保序与旧硬调用一致；attach/ch_key 空安全；零堆；trap 纪律不变；emcc 全绿；64+13 全绿）；Fixed 无；Assumptions 沿用 active-ctx 注册惯例（TA 安装用显式 ctx 更强）；Commands run 见 Check 3。

## 8. Stage3 强化补丁追记（S3-H1~H7，2026-09-11，评审复核）
- **事由**：stage3 关闭后的人工架构复审挖出 4 隐患 + 3 规范项（复审结论：6 条采纳、lint 清理名单修正 2 处），本补丁在进 stage4 前一次性关闭。
- **H1 attach 显式 ctx 直挂**：`adc0832_device_attach` 改向传入 `ctx->pin_traps` 直接挂载（含 trap API 同款 OOR 静默忽略守卫），消除 `ctx != active` 时状态与 trap 分家漂移；`cms8s_sys_init` 同构写法为老代码，留 stage4 `xxx_register` 统一转显式 ctx。
- **H2 net-id 零值钳位**：`ch0/ch1_net_id == 0` 逐字段回退默认 32/33（key 0 = MCU P0.0，板器件永不该碰）；与既有 unbound 回退形成纵深。
- **H3 头自包含**：`cms8s_adc.h` 补 `<stdbool.h>`（纯 C 首包含不再爆 `bool`）。
- **H4 SSOT 回归**：`has_wdt()` 改读描述符 `wdt_present`（与 `has_xsfr` 同值、行为中性；`has_xsfr` 另 4 处窗口语义使用者不动；`desc` 永不空故无空检查，与现行 deref 风格一致）。
- **H5 信息隐藏**：`Adc0832State` 移入 TU 私有（公共头仅剩 attach/config API；TU 显式 `#include <stdbool.h>` 自包含）。
- **H6 门禁保鲜**：删 8 个已死 waiver 行（sfr_map×3、clock、strict、wdt、bridge、出域 adc0832.cpp、context.cpp stage2 `ps_sel`）、extint×3 改 tag→stage4、BASELINE 头部加 hygiene 铁律注释；删后 lint 仍 PASS（反证无活行被误删）。
- **H7 契约闭环**：新增 `test/core/test_adc0832_attach_net_id.cpp`（自定义 40/41 端到端读回 + 未绑定回退 + 零值钳位）并注册；基线重采 142→143，`--compare` MATCH。
- **终验**：lint PASS｜host mcs51 65/65｜wasm 13/13｜`sizeof` 75672 不变｜17 项非 mcs51 存量失败不变。

## 附录 A：S3-2 Step 4 板级模拟空间日落决策记录（✅ 方案 B 定案，2026-09-11）

- **决策结论**：正式采纳 **方案 (B)**（设备私有 pull，board net id 进 `adc0832_device_attach` 参数，通用 rail 回归纯 Pin）。
- **决策背景**：Stage 1 为消除 E-02 阻塞性行为失真并防止 `iron_ntc` 回归，将 rail 划分为双空间（0~31 物理 Pin，32~63 板级通道）。Stage 3 外挂器件 `adc0832` 正式下沉 `devices/`，需对板级模拟通道空间的终态归属做日落定案。
- **方案比选与采纳理由**：
  1. **物理真实映射**：板级外挂芯片（ADC0832）的模拟通道连接的是 PCB 模拟走线（Net），非 MCU 片上引脚。Core 模拟轨不应跨层承载板级布线。
  2. **消除空间碰撞隐患**：彻底避免未来引入 STC8H / STC15 等 48/64 引脚 MCS-51 MCU 时物理引脚（Pin 32+）与板级通道（32+）发生命名空间冲突。
  3. **架构单向彻底解耦（CPL-21 彻底根除）**：`devices/adc0832` 自闭环管理其采样与注入，Core 的 `include/mcs51_adc.h` 完全删除 `adc0832` 转发 shim，实现通用 Core 零外设残留。
  4. **RAM 收益落袋**：为 Stage 7 通用 rail 缩容至 32 槽、Context 净省 96 字节 BSS 扫清架构障碍。
- **跨仓平滑演进与零破坏契约**：
  - `adc0832_device_attach` 增加 `ch0_net_id` 与 `ch1_net_id` 参数。
  - 保留 8 参数兼容包装宏/函数 `mcs51_adc0832_init(...)`，内部默认绑定 `ch0_net_id = 32u, ch1_net_id = 33u`。
  - 前端仿真（`PinArbiter` 驱动通道 32）与 `iron_ntc` 应用代码 100% 零修改、零感知；现有单元测试无缝平移。
- **执行阶段分工**：
  - **Stage 3**：`devices/adc0832` 建立私有状态与独立 pull 链路，删除 Core 头中的 `mcs51_adc0832_*` shim；Core 模拟轨保持 64 槽不变（维持 ABI 稳定）。
  - **Stage 7**：正式日落双读兼容层，`MCS51_ADC_MAX_RAIL_KEYS` 由 64 缩减为 32，Core 彻底回归纯 MCU 物理引脚。
