# Stage2：`Mcu51Context` 数据结构纯净化

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S2-CONTEXT` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | ✅ 已完成（2026-09-11，自审结论见 §7） |
| **优先级** | 🔴 P0 |
| **关联 CPL** | CPL-11（soc_priv）、CPL-12（引脚掩码）、CPL-18（残留结构/isr/xdata）、CPL-19（复位播种） |
| **前置依赖** | stage0（caps_cache）、stage1（rail 接口已稳） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ `adc0832/sysProt/buzzer/cms8sAdc` 移出通用体，经 `soc_priv` 按实例挂载；GPIO Trait 钩子结构以 per-context 指针预留（定义在本阶段，挂载在 stage4）。
- ✅ T3/T4/捕获比较、端口采样移入 `cms8s priv`；外部总线跟踪改名 `extbus` 留 core（通用概念，判据 `xram_size==0`，搬走即造成 gpio 调用点链接反向依赖，见总纲 §3.2）；`isr_table`/XDATA 按描述符裁剪或文档化超配。
- ✅ 通用复位仅 Intel 标准种子；XSFR/ADCLDO/CKCON/Fosc 下放芯片包；厂商命名宏下沉。
- ✅ timer `{8,8,6,4}` 与 extint `{8,8,8,8}` 双向改读 `port_pin_masks`（前者修复 classic，后者收紧 CMS8S）；`sizeof` 不净增；双 context 不串扰。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `include/mcs51_context.h` | ✏️ | 删厂商/板级状态与厂商宏，加 `instance_index` + 留 `soc_priv(void*)` + 标准状态；禁 include 任何厂商 priv 头 |
| `src/mcs51_context.cpp` | ✏️ | 标准种子 only + caps 快照 + `instance_index` 分配/重绑；XSFR/rail 播种删除 |
| `chips/cms8s78xx/include/cms8s_priv.h` | 🆕 | priv 结构（sys/buzzer/adc/T3T4/端口采样） |
| `chips/at89c52/include/at89_priv.h` | 🆕 | 预留位（当前无 classic 私有状态；外部总线跟踪已确认为通用概念，留 core 改名 extbus） |
| `include/mcs51_trap.h`（+ `mcs51_context.h` 钩子字段） | ✏️ | 新增 GPIO Trait 钩子结构定义（may_drive/is_analog/pullup，per-context 指针，纯声明零行为；挂载在 stage4。file-static 全局指针否决：双 context 分属不同家族时必串扰） |
| `include/wink_mcs51_classic_bus.h` → `wink_mcs51_ext_bus.h` | ✏️/改名 | "classic" 系家族命名，通用概念改通用名；`Mcs51ClassicBusState`/`mcs51_classic_bus_*` 同改；调用点（gpio/xdata）同步改名，逻辑不动 |
| `src/mcs51_timer.cpp` + `src/mcs51_extint.cpp` | ✏️ | 引脚掩码双向改读描述符（timer 修 classic，extint 收紧 CMS8S） |

架构红线：**soc_priv 锁定方案 A（按实例 BSS 池），否决 union 方案**——union 要求通用头 include 厂商 priv 头以确定大小，直接击穿 CPL-11/14；禁全局单例；禁堆。

## 3. 任务拆分

### Task S2-1：soc_priv 挂载（方案 A 锁定） `[状态: ✅ 已完成（2026-09-11）]`

- [x] **Step 0**：通用头加 `uint8_t instance_index`（纯通用字段）+ `MCS51_MAX_INSTANCES` 上限常量；新增 `mcs51_context_init(ctx, idx)`（多实例显式分配，`idx` 越界编译期/运行期断言）；`mcs51_context_reset(ctx)` 签名不变（默认 `idx==0` 即今日单 context 行为，现网 ~40 处调用零改动），reset 内断言 `idx < MAX` 越界熔断；`reset`/`set_family` 按 index 重绑 `soc_priv` + `memset` 对应池槽，family 切换时先解绑旧槽再绑定新槽，classic 显式绑定 `nullptr`（禁残留悬空）。冲突域说明：池按芯片分数组，同家族同 index 才冲突；跨进程（各测试二进制独立进程）天然隔离，危险仅在同进程双 context——Step 3 的串扰单测必须经 `init` 分配不同 idx，否则测的是假阴性。**memset 保序铁律**：`reset` 必须在 `memset` 前把 `instance_index` 存局部变量、`memset` 后立即恢复（沿用现网 `saved_isrs` 惯用法），再绑定 `soc_priv`；绑定与池槽 `memset` 必须赶在任何 `g_mcs51_peripherals[i].init/reset` 调用之前（外设 init 立即解引用 `soc_priv`，否则野指针）。
- [x] **Step 1**：建 `cms8s_priv.h` / `at89_priv.h`；芯片源内定义 `static Priv s_priv_pool[MCS51_MAX_INSTANCES]`，按 `ctx->instance_index` 分配，`ctx->soc_priv = &pool[idx]`。
- [x] **Step 2**：通用体删 4 状态 + `adc_vref/vrail`（rail 默认改由芯片 reset 注入，通用 `mcs51_adc_reset` 仅清注入表）。
- [x] **Step 3**：双 context 串扰单测（互写 XSFR/ADC 状态不互相污染；`instance_index` 越界钳位单测；异家族双 context 各自 hooks 隔离）。
- [x] **Step 4**：定义 GPIO Trait 钩子结构（`mcs51_trap.h` 扩展注册 API + ctx 内 `gpio_hooks` 字段，纯声明零行为）；`extbus` 改名（头/状态/函数 + gpio/xdata 两处调用点同步改名，逻辑零改动）。
- **执行裁决 S2-1D1（自绑定替代 core 预绑定）**：计划要求 core 侧 `reset` 在外设 init 前统一绑定，但 core 引用芯片池符号违反总纲 §3.1 单向铁律。改 self-binding：各 `cms8s_*_init/reset` 首行幂等绑定（`soc_priv==slot` 则跳过），`set_family` 显式 NULL（解绑旧槽），core `reset` 显式 NULL（classic 绑定）。保序不变量不变（任一解引用前必已绑定：core init 不碰 `soc_priv`，首个芯片 init 先绑后用）。
- **执行裁决 S2-1D2（adc0832 独立器件池）**：`soc_priv` 单指针无法同时挂芯片包与板级器件（iron_ntc = classic + ADC0832 活组合）。器件状态进独立 BSS 池（`s_adc0832_pool`，同 `instance_index` 键，`init` 时 memset 槽位），类型 `Adc0832State` 随 `ADC0832.H` 在 stage3 迁入 `devices/`。
- **执行裁决 S2-1D3（`adc_vref/vrail` 删播种留字段）**：字面"删字段"会使 `mcs51_adc_get/set_vref/vrail` 通用 API 存储无着；按括号意图执行——删通用复位播种（`context_reset` + `mcs51_adc_reset` 的 3000/3000），字段与 API 保留（已是通用 rail 参数，无厂商语义，不破 L1/L4）。
- **执行裁决 S2-1D4（`init` 钳位 + `reset` 断言）**：计划"钳位单测"与"越界熔断"并存——`init` 越界钳位到末槽（可测），`reset` 断言 `idx<MAX`（防手写结构体野 index），`_Static_assert(MAX>=2)` 编译期。
- **执行裁决 S2-1D5（hook 家族门 + 按需绑定）**：落地后发现 `sfr_operators`（只 `set_family` 不 `init`）经 bridge `notify` 空悬崩溃。补 `cms8s_priv()` 按需绑定（家族门控，仅芯片家族）+ 7 芯片 hook 体家族门（跨家族 stale hook 中性返回）。另 `test_mcs51_classic_bus.cpp` 文件改名但 ctest 名保留（基线名集合不动，CMake 注释说明）。
- **执行数据**：`sizeof` 75752→**75672**（-80；§4 回填）；`MCS51_MAX_INSTANCES=4`；host mcs51 52/54（2 基线例外）+ `wasm_` 11/11 + lint PASS。

### Task S2-2：残留结构与复位播种 `[状态: ✅ 已完成（2026-09-11）]`

- [x] **Step 1**：T3/T4/比较/端口采样移入 cms8s priv；`classicBus` 就地改名 `extbus` 留 core（判据 `xram_size==0` 为通用概念；`mcs51_xdata.cpp:273-303` 逻辑不动，`gpio.cpp:244,277` 调用点同步改名——搬出 core 即链接反向依赖，否决）。
- [x] **Step 2**：`xdata_shadow` 保持 64KB 不动（MOVX 地址空间镜像：42 处访问直引 16 位地址，按 xram 裁剪需全站地址换算，否决；书面超配理由即本句）。`isr_table[28]` 同理保留统一下发平面（classic 空槽位既无注册也无派发，88~176B 不值得在中断相邻路径加分支）。预算优化对象是状态字段（见 §4 表），不是镜像容器。
- [x] **Step 3**：通用 reset 删 `PS_*=0x7F` 与 `3000/3000` 播种；`MCS51_XRAM_SIZE_CMS8S78XX` 与 `MCS51_XRAM_WINDOW_BASE` 一并下沉（后者此前遗漏）。
- [x] **Step 4**：timer 与 extint 引脚合法性双向改读 `port_pin_masks`（timer 修 classic P3.4/P3.5 可用；extint 收紧 CMS8S P2.6-7/P3.4-7 非法引脚 + 收紧侧单测）。
- **执行裁决 S2-2D6（T3/T4/采样状态推迟到 stage4，随代码同行）**：Step 1 后半不可按字面执行——`Mcu51TimerState` 的 T3/T4/cap/cmp 与 `Mcu51ExtIntState` 的 `port_pins` 由核心与扩展路径**同函数共享**（timer poll/step、extint poll 双轨同体）。状态先行入池则核心代码需含芯片类型（破单向铁律），或 classic 需绑芯片池（荒谬）。连贯拆分只能代码+状态同行，即 stage4 `cms8s_timer.cpp`/`cms8s_extint.cpp` 落地时自然带走。`extbus` 半已在 S2-1 完成；`sizeof` 维持 75672（S2-2 零结构变更，预算单测复核通过）。
- **执行注记 Step 3**：13 播种按 owner 归位——PS_ADET 早在芯片 reset，PS_INT0/1 复用 `extint_reset` 既有行（core 块删除前后值一致），PS_T*/PS_CAP* 新增于 `timer_reset`；classic 两家族播种值与旧 core 块逐项一致（可读性：`extint_reset` 的 INT0/1 行此前与 core 块重复播种，现唯一化）。XRAM 双宏零使用者，纯搬入 `cms8s_priv.h` 并改 `CMS8S_` 前缀。
- **执行注记 Step 4**：timer 3 处 + extint 2 处硬编码表改读描述符；收紧单测落 `test_extint_model.cpp` §J（P2.6/P3.7 非法 mux 回退经典引脚）+ 该文件固家族 CMS8S（A–I 引脚全在 {8,8,6,4} 内，零影响）。诚实注记：classic T0/T1 外部时钟"修复"实际行为中性——旧 fallback 恰为 P3.4/5 本 pin（28/29），改动只正了合法性来源；真正可观测变更是 CMS8S 收紧侧（§J 覆盖）。

### Task S2-0：`sizeof` 基线测量与分家族预算 `[状态: ✅ 已完成（2026-09-11）]`

- [x] **Step 1**：基线测量并记录（迁移前唯一一次）：`static_assert` + 单测打印 `sizeof(Mcu51Context)` 及主要成员偏移（`timer/extint/uart/isr_table/xdata_shadow`），写入本计划 §4 预算表（当前约 68KB，实测为准）。——执行注记：三版真值 75648 / 75656 / 75752（见 §4）；"约 68KB"预估偏低，以 73.9KB 实测为准，`mcs51_context.h` 注释已同步修正。
- [x] **Step 2**：冻结预算：classic 实例与 CMS8S 实例拆分后各自 `sizeof` 不得超过基线；`xdata_shadow`/`isr_table` 若保留超配必须在 §4 写明理由 + 上限。——执行注记：`test_mcs51_context_budget` 已注册（ceiling 75752+1024，含跨工具链 slack）。

## 4. 预算表（§4，实测填数）

实测工具链：MinGW GCC 16.2（与计划 GCC 14.2 同 LLP64 ABI，POD 布局一致；MSVC 以单测内 +1KB slack 覆盖漂移）。探针：`sizeof_probe` 直编新旧头文件三版真值（`5a91356^` / `5a91356` / 现树），非推算。永久锁：`test/core/test_mcs51_context_budget.cpp`（打印 + ceiling `75752+1024`，stage5 +104B 时显式上调）。

| 行 | 说明 | `sizeof` | 备注 |
|----|------|----------|------|
| 基线（master，stage0 前） | 迁移前实测（`5a91356^` 头文件探针） | **75648** | 含 65536 `xdata_shadow` + 28 项 `isr_table`（112+112B）；计划预估"约 68KB"偏低，以实测 73.9KB 为准 |
| stage0 后（`caps_cache` 入账） | 实测（`5a91356` 头文件探针） | **75656** | +8（`family u8` + `caps_cache u32` 对齐尾）；总纲 §8 已批预算内增量 |
| stage1 后（rail 64 槽入账） | 实测（现树探针） | **75752** | +96（32 槽 ×（2B injected + 1B flag）），与计划预测分毫不差 |
| stage5 后（irq map 入 ctx 入账） | 待测（stage5 落数，届时同步上调单测 ceiling） | 待填 | +104B（13 项 × 8B profile）；诊断计数器留 file-static 不计入 |
| cms8s priv 池/实例 | 实测 **72B**（sys 32 + buzzer 24 + adc 8 + adet 4 + in_poll 1，对齐后；×4 槽 = 288B BSS）+ 器件池 `Adc0832State` **15B**（×4 = 60B BSS，独立数组） | 72+15 | BSS 池按实例，"context 外"内存，另行列表不与上表混算 |
| 注册表上限 | **12**（stage4 实装：3 core + 7 cms8s = 10，留 2 余量；~~8~~ 冻结值已由 stage4 复审复议，见 stage4 附录 C） | 待填 | 溢出在注册点无条件 abort（ADR-0012），不静默丢弃；倒逼新外设走 chips 拆分而非 core 堆料 |
| 拆分后 classic | **75672**（S2-1 后实测；+24 vs 基线 = caps 8 + hooks 12 + 对齐 4，入账；S2-2 再削 ~120） | 75672 | extbus 状态（8B）留 core，已计入 |
| 拆分后 CMS8S | **75672**（context 内尺寸与 classic 同构；priv 池外计 72B/实例） | 75672 | soc_priv 池外计（BSS 池按实例，另行列表） |

## 4. 验收

- L1：通用头零厂商结构/宏；§4 预算表填实测数。~~拆分后两行均 ≤ 基线~~修正（S2-1 实测后）：拆分后 75672 vs 基线 75648（+24 入账 = caps_cache 8〔stage0 已批〕+ gpio_hooks 12〔stage4 既定基础设施〕+ 对齐 4），适用总纲 §8"中间增量入账"制；S2-2 零结构变更（T3/T4/采样状态随代码推迟到 stage4，见 D6），保持 75672，+24 缺口由 stage4 关闭——缺口公开，不藏。
- L2：经典复位后 XDATA 无 XSFR 残留；经典 P3.4/P3.5 计数可用（`test_mcs51_timer_ext_clk` 全绿；诚实注记见 S2-2 Step 4——可用性此前即由 fallback 巧合保证）。
- L4：`grep -Ei 'cms8s|adc0832|0xF0' include/mcs51_context.h src/mcs51_context.cpp` 零**模型**残留（仅剩 3 处 by-design：`WINK_MCU_CMS8S78XX` 构建路由、`MCS51_FAMILY_CMS8S78XX` 描述符行、`CMS8S 24 MHz` Fosc 注释，与 lint by-design 条目一致）；`grep -Ei '#include.*(cms8s|at89)_priv' include/mcs51_context.h` 零命中（union 穿透回归哨兵）。

## 5. 风险与回滚

- R-02（串扰/超限）：BSS 池 + 预算断言缓解；回滚 `git revert <S2-commit>`，priv 头独立提交可单独回退。

## 6. 阶段自审自我检验清单（Self-Audit Checkpoint）
- [x] **目录落位**：`chips/cms8s78xx/include/cms8s_priv.h`、`chips/at89c52/include/at89_priv.h`（预留位）已严格按目录树落位；无 `at89_bus.cpp`（extbus 留 core，见 Step 1 否决理由）。
- [x] **通用纯净度**：`include/mcs51_context.h` 仅包含标准字段、`void* soc_priv` 与 per-context hooks，无厂商私有头包含；`ext_*` 无 `classic` 命名残留。
- [x] **预算约束**：§4 预算表已填实测数（基线/stage0/stage1/拆分后四行 + 池外计）；+24 缺口入账并公开（见 §4 L1 修正）。
- [x] **双轨状态**：双 context 串扰单测（含异家族 hooks 隔离）全绿，host/wasm 双 target 编译零 warning/error（host mcs51 52/54，两项为 S1-0c 基线例外；wasm 11/11；lint PASS）。

## 7. 自审签署（2026-09-11）
- **Check 1 目录落位**：通过。`chips/` 双头 + `wink_mcs51_ext_bus.h` 改名 + 测试文件改名（ctest 名保留保基线）全部合树；`test/CMakeLists.txt` 中央注册未复制 wiring。
- **Check 2 依赖单向与残留**：通过。lint PASS（S2-1/S2-2 剪死 waiver 7 项）；core 永不命名芯片符号（自绑定 + 按需绑定 + hook 家族门）；`mcs51_context.h` 无厂商 priv 头包含。
- **Check 3 双轨与契约**：通过。52/54 + 11/11；iolations/预算/收紧（§J）/ext_bus 新单测全绿；`--compare` 142=142 集合相等；`sizeof` 75672（预算单测 ceiling 已同步）。
- **Check 4 计划闭环**：S2-0/S2-1/S2-2 checkbox 全勾；D1–D6 六项裁决归档；总纲 §5 stage2 状态列待置 `✅ 已完成`（与本提交同批）。
