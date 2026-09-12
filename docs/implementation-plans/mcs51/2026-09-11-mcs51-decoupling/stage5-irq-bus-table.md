# Stage5：中断向量表与 XDATA/XSFR 表驱动化

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S5-IRQBUS` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | ✅ 已完成（2026-09-12，自审签署见 §7） |
| **优先级** | 🟡 P1 |
| **关联 CPL** | CPL-06（向量表）、CPL-08（XDATA/XSFR） |
| **前置依赖** | stage0（schema）、stage4（芯片包可挂载） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ 通用 ISR 默认表缩水为标准 0~5；扩展 profile 由芯片包在 reset 中经既有 `wink_mcs51_set_irq_map_entry` 逐项装载；描述符 `irq_vector_table` 仅做绝缘白名单判定（stage0 已冻结为向量号表，本阶段不改其类型）；AT89 模式下 CMS8S 中断物理绝缘。
- ✅ `mcs51_xdata.cpp` 去 `mcs51_xsfr_allowlist.h` 强包含：tripwire 先判窗口存在（`xsfr_size!=0`，经典直接走外部 RAM 语义），再调芯片侧 allowlist 校验；经典无窗口。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `src/mcs51_isr.cpp` | ✏️ | 默认表缩水 0~5；仲裁逻辑不动 |
| `src/mcs51_xdata.cpp` | ✏️ | 窗口门控 + 芯片校验回调，删 allowlist 包含 |
| `chips/cms8s78xx/` | ✏️ | 扩展 profile 装载（reset 中逐项） + XSFR 校验归位（含 allowlist 文件） |
| `include/mcs51_xsfr.hpp` | ✏️ | 类留 core（通用机制），注释去厂商味（ADCLDO/0xF692 例改为示意地址表述） |
| `tools/mcs51_shim_audit.py` + freshness 门禁 | ✏️ | isr 交叉校验家族化（默认表只剩 0~5，扩展项校验移入芯片侧）；REG 路径已在 stage3 同步 |

## 3. 任务拆分

### Task S5-1：向量表驱动 `[状态: ✅ 已完成（2026-09-12）]`

- [x] **Step 1**：通用 ISR 删 `s_default_irq_map` 中 ADC/PWM/I2C/SPI/UART1 硬编码（缩水为 0~5，6..12 用 0xFF 哨兵行）；`cms8s` reset 中经 `wink_mcs51_set_irq_map_entry` 逐项装载扩展 profile（复用既有 API，不新增机制）。——执行注记：扩展表 `kCms8sIrqExtensions[]`（6 行，UART1 按硅片缺失）落在 `src/cms8s_sys.cpp`（stage6 随 TU 迁 `chips/`），由 `cms8s_irq_map_extend()` 经既有 API 装载；T2IF/T2IE 地址宏下沉 `cms8s_sfr_map.h` 单源，`cms8s_timer.cpp` 同步引用。
- [x] **Step 1b（仲裁特判抽离，P0 阻断项）**：`mcs51_isr.cpp:309-312` 的 T2 `0xC9u/0xCFu` 多标志特判抽为 per-context flag-predicate hook（默认 = 标准单 bit 判定即现 `else` 分支；CMS8S reset 安装多标志版；UART0/INT0/INT1 特判为标准语义，留内联）。仲裁循环本身不动。——执行注记：新增 `ctx->irq_flag_predicate`（0 值 = 现 `else` 分支）；`cms8s_irq_flag_predicate()` 对 T2+0xC9 判定 `(T2IF & T2IE) != 0`（逐字保留旧语义），其余源复现标准单 bit；UART0/INT0/INT1 内联分支原样保留。仲裁扫描/优先级/嵌套逻辑零改动（唯一新增为家族白名单跳过，见 Step 2）。
- [x] **Step 1c（映射表入 ctx，P0 隔离项）**：file-static `s_irq_map[13]` 迁入 `Mcu51Context`（+104B，记 stage2 §4 预算表；诊断计数器等 M4 项留 file-static）；`reset` 按家族装载（core 默认 0~5 + 芯片扩展 overlay），`set/get/reset_irq_map` 三 API 改操作 active ctx（签名不变）；`ensure` 全局 once 语义删除。否则 CMS8S 装载后切 classic，扩展向量借尸还魂，直接击穿 L1 绝缘验收。——执行注记：`ctx->irq_map[IRQ_SOURCE__COUNT]` + 新增 `irq_map_extend`/`irq_flag_predicate`/`xsfr_validate` 三 per-context 钩子；`mcs51_context_reset` 在 peripheral 循环前调 `wink_mcs51_reset_irq_map()`（core 行 + 钩子重放），芯片 descriptor reset 再装扩展；`wink_mcs51_reset_irq_state()` 经 `reset_irq_map` 自动重放芯片扩展（不降级）。实测 `sizeof(Mcu51Context)=75656`（+120 = map 104 + 钩子 12 + 对齐 4，stage2 §4 已回填）。
- [x] **Step 2**：AT89 模式断言扩展向量不可达 + XSFR 窗口关闭的绝缘单测（含双 context 分属异家族并存：一方装载扩展后另一方仍绝缘）；`mcs51_shim_audit.py` 的 isr 交叉校验同步家族化（freshness 门禁同改；默认表/芯片表新位置同步更新脚本；验证命令见 stage3 Step 4，校验对象改为家族化后的两表位置）。——执行注记：可达性门控落在 `mcs51_raise_irq`（未映射/白名单外不置 pending）、`wink_mcs51_dispatch_vector`（扩展向量返回 0）与扫描循环（防御性跳过）；新增 `test/core/test_mcs51_family_insulation.cpp`（扩展向量带 ISR 仍不可达 + 经典 raise 丢弃 + 关闭窗口不触达芯片校验 + 双 context 并存）；脚本新增 `parse_chip_irq_extensions()`，核心表断言仅 0~5、扩展项校验移芯片表，`No hard mismatches` + allowlist 93 地址 fresh。

### Task S5-2：XSFR 参数化 `[状态: ✅ 已完成（2026-09-12）]`

- [x] **Step 1**：tripwire 先判窗口存在（`xsfr_size!=0`），再查芯片侧 allowlist；allowlist 头下沉芯片包（`cms8s_xsfr_allowlist.h`，生成器目标路径同步），通用零包含。注：`KIND_XSFR` 是代理协议 tag，不动。——执行注记：allowlist 头在 stage3 已落 chips/；本阶段 `mcs51_xdata.cpp` 删 `#include "mcs51_xsfr_allowlist.h"` 与二分查找，改为 `xsfr_addr_in_window()` 门控后调 `ctx->xsfr_validate`（`cms8s_xsfr_allowlisted()` 在 `cms8s_sys.cpp`，93 地址二分；无钩子时按未建模处理）。通用 include 侧仅剩 stage7 才删的转发 shim（零消费者）。
- [x] **Step 2**：未建模 XSFR tripwire 归芯片模型所有，通用只做窗口分发；`mcs51_xsfr.hpp` 注释去厂商味。——执行注记：窗口范围/白名单归属拆清（描述符给范围、芯片钩子给成员）；OOB 告警日志的窗口区间改为描述符运行时值（不再硬编码）；`mcs51_xsfr.hpp`/`mcs51_xdata.cpp`/`absacc.h` 注释全部去厂商地址例，lint 三条 stage5 waiver 删除后 PASS 反证零残留。

## 4. 验收

- L1：切换 family 后对方中断/XSFR 不可达（绝缘单测）。
- L4：通用 `include/`、`src/mcs51_xdata.cpp` 无厂商白名单引用。

## 附录 A：中断映射事实表（迁移源，摘自 `src/mcs51_isr.cpp:36-50` + `include/wink_mcs51_isr.h:37-52`，执行时以代码为准）

语义源共 13 个（`IRQ_SOURCE__COUNT = 13`），表宽 28（`WINK_MCS51_NUM_VECTORS = 28`，CMS8S 模块号 0..27，扩展模块 = 向量 + 1）：

| 语义源 | 向量 | 使能位 | 标志位 | 优先级位 | 清除模式 | 归属 |
|--------|------|--------|--------|----------|----------|------|
| INT0 | 0 | IE.EX0 | TCON.IE0 | IP.PX0 | HW_AUTO | 通用（classic + CMS8S） |
| TIMER0 | 1 | IE.ET0 | TCON.TF0 | IP.PT0 | HW_AUTO | 通用 |
| INT1 | 2 | IE.EX1 | TCON.IE1 | IP.PX1 | HW_AUTO | 通用 |
| TIMER1 | 3 | IE.ET1 | TCON.TF1 | IP.PT1 | HW_AUTO | 通用 |
| UART0 | 4 | IE.ES0 | SCON.RI/TI | IP.PS0 | SW | 通用 |
| TIMER2 | 5 | IE.ET2 | T2IF.T2F | IP.PT2 | SW | 通用（8052 标准） |
| TIMER3 | 15 | EIE2.ET3IE | EIF2.TF3 | EIP2 bit0（模块16） | HW_AUTO | CMS8S 下沉 |
| TIMER4 | 16 | EIE2.ET4IE | EIF2.TF4 | EIP2 bit1（模块17） | HW_AUTO | CMS8S 下沉 |
| PWM | 18 | EIE2.PWMIE(3) | EIF2.PWMIF(3) | EIP2 bit3（模块19） | SW | CMS8S 下沉 |
| ADC | 19 | EIE2.ADCIE(4) | EIF2.ADCIF(4) | EIP2 bit4（模块20） | SW | CMS8S 下沉 |
| I2C | 21 | EIE2.I2CIE(6) | EIF2.I2CIF(6) | EIP2 bit6（模块22） | SW | CMS8S 下沉 |
| SPI | 22 | EIE2.SPIIE(7) | EIF2.SPIIF(7) | EIP2 bit7（模块23） | SW | CMS8S 下沉 |
| UART1 | 0xFF（未映射，CMS8S 无串口1） | — | — | — | SW | 保留哨兵，AT89 家族按需另行定义 |

注意：优先级位规则为 vendor `IRQ_SET_PRIORITY` 宏 + `en_Priority_Module`（模块 <8 用 IP，8..15 用 EIP1，16..23 用 EIP2，24..31 用 EIP3），禁止用 `vector-16` 推算（GAP-22 前车之鉴）。

## 5. 风险与回滚

- R：向量号漂移致 ISR 错配 → 缓解：向量表 `static_assert` + dispatch 计数单测；回滚 `git revert <S5-commit>`。

## 附录 B：stage4 复审移入项（2026-09-12）
- [ ] **标准 8052 T2 波特（RCLK/TCLK）补建模（P2，择机）**：core UART 标准路径目前仅 Timer1（`mcs51_uart.cpp` `uart_baud_hz_std`）；8052 的 `T2CON.RCLK/TCLK` 由 Timer2 溢出驱动收/发波特这一机制从未实现（stage4 计划原文"（+标准 T2）"为文案误差，已修，非 stage4 回归）。实现需 core timer 溢出与 core UART 联动（非芯片包），建议与本阶段 S5-1 Step 1b 的 T2 多标志特判抽离同批评估，避免两次改动 T2 路径。——**本阶段评估结论（2026-09-12）**：不与 Step 1b 同批。predicate 仅改"标志有效性判定"，T2 波特需新增 timer 溢出 → UART 重载的跨模块联动（标准件新机制），混批会扩大 stage5 风险面且无共享代码路径；维持 P2 择机，本项未关闭。

## 附录 C：开工前置核查（2026-09-12，Entry Checklist）
- **① sizeof 基线复核（通过）**：`test_mcs51_context_budget` 实测 `sizeof(Mcu51Context)=75536 B`（ceiling 76696 B，余量 1160 B）。Step 1c 迁入 `s_irq_map[13]`（+104 B）后预期 **75640 B**，ceiling 无需上调；落地时同步更新 `include/mcs51_context.h` 头注释与 stage2 §4 预算表 `待填` 行（口径不变：75672 + 1 KB slack）。——**落地实况（2026-09-12）**：实际 **75656 B**（+120 = map +104 + 三个 per-context 钩子 `irq_map_extend`/`irq_flag_predicate`/`xsfr_validate` +12 + 对齐 +4）；ceiling 76696 未动，stage2 §4 与头注释已按 75656 回填。
- **② T2 特判 × stage4 chained hooks 交叉面（已确认，回归基线全绿）**：待抽离点 = `src/mcs51_isr.cpp:309-312`（`T2IF(0xC9) & T2IE(0xCF)` 多标志判定）。供应商为 stage4 芯片侧：`cms8s_timer.cpp` 的 T2 溢出/捕获/比较均可 `mcs51_raise_irq(IRQ_SOURCE_TIMER2)` 且标志位可能只有 CCxIF 而 T2F=0。抽为 per-context flag-predicate 时：默认 = 标准单 bit 判定（现 `else` 分支），CMS8S reset 安装多标志版，语义须与 `(T2IF & T2IE) != 0` 逐字一致。回归基线（本次核查实跑）：`test_mcs51_timer_ext_clk`、`test_mcs51_t234_fsys`、`test_mcs51_low_power`、`test_mcs51_uart_charge(_strict)`、`test_mcs51_wdt_ta`、`test_mcs51_gpio_dir`、`test_mcs51_port_extint`、`test_mcs51_extint`、`test_mcs51_adc_refchain`、`test_mcs51_cms8s_adc_e2e`、`test_mcs51_iron_ntc_e2e` —— 13/13 通过；wasm 轨全绿（含 `wasm_mcs51_timer0_test`、`wasm_mcs51_cms8s_adc_test`、`wasm_mcs51_iron_ntc_test`）。
- **③ S4-D5 bridge 过渡分支（确认在位，无冲突）**：`src/mcs51_bridge.cpp:35-56`（`__has_include` 生成头 + `#elif WINK_MCU_CMS8S78XX` 默认注册）与 lint waiver（`tools/lint/lint_mcs51_layering.py:96-100`，到期标记 `stage6`）均在位；本阶段 §2 变更范围不含 bridge。**约束：stage5 期间不得改动/移除该分支与 waiver**，stage6 codegen 落地才删。
- **备注（Task 0 建议）**：stage4 复审遗留的"风格门禁"（80 列/大括号目前无机械检查）建议作为本阶段 Task 0 立项，否则新增代码会再次漂移。

## 附录 D：stage5 复审未闭环项（2026-09-12，资深架构复审裁决）

| # | 事项 | 裁决 | 处置阶段 |
|---|------|------|----------|
| D-1 | **core 默认表 TIMER2 行仍带 CMS8S 地址 `0xC9`（T2IF），非 8052 标准 `T2CON(0xC8).7`** | 属实，且 classic T2 硬件中断链在 sim 中断路：core `mcs51_timer.cpp:276` 自身把溢出标志写到 `0xC9`、`:306` 用 `0xCF`（CMS8S T2IE）做使能门——`0xC9/0xCF` 均非 Intel 8052 SFR（标准使能是 `IE.ET2=0xA8.5`）。属 S4-D4 明确延期的"标准 TF2/ET2 重写属高风险重构"，非 stage5 引入。**禁止一行改 map**：只把 core 行改 `0xC8.7` 会同时打断 CMS8S 捕获/比较 predicate。正确闭环（三步）：(a) core 表 TIMER2→`0xC8.7`（注释 `T2CON.TF2`）；(b) chip overlay 显式增 `TIMER2→0xC9/T2IE 0xCF`（脚本预期同步）；(c) core timer T2 溢出落点/使能门按家族区分。 | **专设立项（S5.5/附录 D 专项）**，与 D-4 测试同批 |
| D-2 | **`family_vector_reachable` O(N) 循环 → bitmask O(1)** | 合理优化（P3），但 raise/dispatch/scan 非指令级热路径；O(1) 需在 ctx 缓存 `irq_vector_mask`（+4B → 75660）或 stage6 扩描述符/预生成。stage0 冻结的 `irq_vector_table` 类型不在本阶段改。 | stage6（codegen 期）评估 |
| D-3 | **三钩子聚合为 `Mcs51IrqBusHooks` 结构体** | 合理（与 `Mcs51GpioHooks`/`Mcs51UartHooks` 先例一致，尺寸不变）；本轮为控制 diff 未做。 | stage6 或下轮 hook 收敛 |
| D-4 | **Classic T2 中断语义单测** | 需要，但必须先闭环 D-1(c)，否则测试只会暴露 core timer 的家族混用；用例应驱动真实溢出路径（core timer step → TF2/IE.ET2），而非仅手写 shadow。 | 随 D-1 专项 |

## 6. 阶段自审自我检验清单（Self-Audit Checkpoint）
- [x] **目录落位**：`chips/cms8s78xx/include/cms8s_xsfr_allowlist.h` 完全归位芯片目录（stage3 已落）；本阶段新增芯片扩展 profile/校验钩子全部落在 `src/cms8s_sys.cpp`（stage6 随 TU 迁 `chips/cms8s78xx/src/`），无顶层临时文件。
- [x] **通用纯净度**：`src/mcs51_isr.cpp` 默认表仅 0~5 标准向量（6..12 为 0xFF 哨兵）；`src/mcs51_xdata.cpp` 零厂商 allowlist 头包含（lint 三条 stage5 waiver 删除后 PASS 反证）。
- [x] **物理绝缘自审**：AT89 模式下 CMS8S 扩展中断（raise/dispatch 双门控）与 XSFR 窗口（描述符门控 + 钩子不触达）绝缘单测通过（`test_mcs51_family_insulation`）。
- [x] **双轨状态**：host mcs51 57/57、wasm 13/13 全绿；全量 host 17 项非 mcs51 失败与 stage4 基线一致，零新增。

## 7. 自审签署（2026-09-12）
- **Check 1 目录落位**：通过。不新增框架源文件——芯片扩展表/三钩子落 `src/cms8s_sys.cpp`（stage6 搬移），测试新增 `test/core/test_mcs51_family_insulation.cpp`，均在终态目录树规划内。
- **Check 2 依赖单向与残留**：通过。lint PASS（修剪 5 条 stage5 waiver：`wink_mcs51_isr.h`、`mcs51_isr.cpp`、`mcs51_xdata.cpp`×3，删后 PASS 反证零活行；转发 shim 两条 retag `stage7`）；core 头零 `chips/`/`devices/` 私有头包含；芯侧只定义 `cms8s_*`（`cms8s_irq_map_extend`/`cms8s_irq_flag_predicate`/`cms8s_xsfr_allowlisted`）。
- **Check 3 双轨与契约**：通过。host `mcs51` 57/57（含新增绝缘测试；STRICT 双生全绿）、wasm 13/13；`sizeof(Mcu51Context)` **75656**（+120，ceiling 76696 内）；`mcs51_shim_audit.py` 全量 `No hard mismatches` + `--check-xsfr-allowlist` 93 地址 fresh。全量 host 17 项非 mcs51 失败（PAL/DAL `-Werror` 存量 + 缺外仓 codegen）与 stage4 失败集一致，判 master 先存问题。
- **Check 4 计划闭环**：S5-1/S5-2 全勾；附录 B 的 T2 波特项已评估并书面维持 P2；总纲 §5 stage5 状态列同批置 `✅ 已完成`；设计规范 §2.1/§2.7（`docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md`）已回写。
- **执行裁决 S5-D1（映射表扩展钩子形状）**：计划字面"reset 按家族装载 core 0~5 + 芯片 overlay"需要一个"状态复位（`wink_mcs51_reset_irq_state`）后不丢扩展"的机制。采用 per-context `irq_map_extend(ctx)` 函数指针（与 `sfr_write_notify`/gpio_hooks 同纪律），而非 file-static overlay 表或键控文件静态：双 context 分属异家族零串扰、且 `wink_mcs51_reset_irq_map()` 语义无需改写（先 core 行后钩子重放）。
- **执行裁决 S5-D2（可达性门控三落点）**：计划只要求"AT89 模式扩展向量不可达"。门控落在 `mcs51_raise_irq`（源头，不置 pending）、`wink_mcs51_dispatch_vector`（直接调用路径）与扫描循环（防御性跳过 + 清悬挂 pending）。理由：仅靠"默认表 0xFF 哨兵"无法覆盖"手工 `set_irq_map_entry` 把扩展源指到可达向量"与"旧 ctx 残留 pending"两类边界；三处均为 O(向量数) 描述符查表，非热路径。
- **执行裁决 S5-D3（S5-2 未建模 tripwire 归属释义）**：计划 Step 2 "未建模 tripwire 归芯片模型所有"落为"成员判定归芯片（`xsfr_validate` 钩子），窗口判定/计数/告警策略留 core（通用可见性设施）"。芯片包不重复实现计数与 STRICT 政策；core 在无钩子的窗口家族按"全部未建模"处理（fail toward visibility）。
- **执行裁决 S5-D4（依赖懒初始化的直驱测试修正）**：`ensure_irq_map` 删除后，6 个未做 context reset 的直驱测试失效（`uart_isr`/`uart_rx` 依赖懒装载；`cms8s_adc`/`cms8s_vendor`/`extint`/`irq_arbitration` 依赖旧 file-static 扩展表）。修正：core 直驱测试显式 `wink_mcs51_reset_irq_map()`；芯片测试补 `mcs51_context_reset()`（或 `mcs51_test_use_family`）触发芯片 descriptor reset。语义与生产路径一致，未引入测试专用后门。
- **复审追记 S5-H1（lint 扫描面盲区，已闭环）**：原 `core_files()` 仅 glob `mcs51_*.h`/`wink_mcs51_*.h`/`src/mcs51_*.cpp`，未覆盖 `*.hpp` 与 `absacc.h`——`mcs51_xsfr.hpp` 中残留的 3 处 `ADCLDO`/`P00CFG` 注释因此逃过门禁，§7 原"lint PASS 反证零残留"仅对当时扫描面的文件成立，属过宽表述，已修正。处置：扫描面扩为 `mcs51_*.h`/`mcs51_*.hpp`/`wink_mcs51_*.h`/`wink_mcs51_*.hpp`/`absacc.h`（`core_files()` 现 41 文件，已断言 .hpp×2 + absacc.h 在列）；`mcs51_xsfr.hpp`/`mcs51_proxy.hpp` 注释全部去厂商例；扩面后 gate PASS 为真反证。
- **复审追记 S5-H2（钩子 ctx 契约，已闭环）**：两项——① `cms8s_irq_map_extend` 虽收 `ctx` 却经 active-ctx 绑定的 `wink_mcs51_set_irq_map_entry` 装载，多实例/后台调用会污染 active ctx（当前在树调用路径均钉扎 active，属潜在缺陷）；改为直写 `ctx->irq_map[src]`，公开 API 仍为运行时覆盖面（S5-D1 修订）。② `mcs51_xsfr_validate_fn_t` 缺 `ctx` 入参，违反 `mcs51_trap.h` 的 hook C-ABI 自约；签名补 `struct Mcu51Context* ctx`（芯片静态白名单 `(void)ctx`），为换页/bank XSFR 预留而无需再破 ABI。两处均零结构尺寸变化（75656 不变）。
- **Safety review**：Risk level 高（ISR 仲裁路径 + 家族隔离门 + 双 context + 共享 per-context 状态）；Checklist phases run 1–4、10、12（完整）；Findings 无（映射表 POD + memset 清零 + 钩子 per-context；可达性门控为查表无副作用；`irq_map_extend` 重入安全——`reset_irq_map` 单向调用链无递归）；Fixed 6 处测试装载修正 + 1 处测试家族切换顺序（set_family 前的 active 钉扎）；Assumptions 沿用 active-ctx 惯例（芯片 glue 在 context reset 钉扎窗口内安装，与 `mcs51_trap_register_*` 同约）；Commands run 见 Check 3。
