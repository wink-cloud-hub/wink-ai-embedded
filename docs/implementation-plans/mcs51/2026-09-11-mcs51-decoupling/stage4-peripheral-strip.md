# Stage4：外设增强机制与私有外设剥离

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S4-PERIPH` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | ✅ 已完成（2026-09-12，自审结论见 §6） |
| **优先级** | 🟡 P1 |
| **关联 CPL** | CPL-03（GPIO）、CPL-04（UART）、CPL-05（Timer）、CPL-07（ExtInt）、CPL-10（自注册） |
| **前置依赖** | stage3（头文件已归位，钩子可用） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ `cms8s_gpio/uart/timer/extint.cpp` 承接全部专有逻辑，通用文件经 `caps_cache` + per-context 钩子外包（钩子结构 stage2 已定义，本阶段只做挂载）。
- ✅ T2 按语义拆分：标准骨架（T2CON/TL2/TH2/RCAP）留 core，CMS8S 扩展（T2IF W0C/T2PS/捕获解释/CCEN 系/T34MOD）下沉（注意 `0xCA` 既是 RCAP2L 又是 RLDL，按语义拆，地址留 core）。
- ✅ UART 与 Timer 协同拆分：TMR4/TMR2/BRT 波特分支随 FUNCCR 同迁 `cms8s_uart.cpp`；通用 UART 固定引脚 + Timer1 波特（标准 8052 的 RCLK/TCLK T2 波特历来未建模，非本阶段回归；文案已修，见附录 C，实现列入 stage5 附录 B）。
- ✅ `mcs51_peripheral.cpp` 仅 core 三件套 + BSS 注册表；芯片包经 `xxx_register()` 自注册（总纲 §3.1b 协议）；`has_wdt` 判据切换为描述符 `wdt_present`。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `chips/cms8s78xx/src/cms8s_gpio.cpp` | 🆕 | PxxCFG/PxTRIS/PxUP/开漏 |
| `chips/cms8s78xx/src/cms8s_uart.cpp` | 🆕 | FUNCCR/PS_RXD |
| `chips/cms8s78xx/src/cms8s_timer.cpp` | 🆕 | Timer3/4 + T2-CMS8S 扩展 + W0C（含 hook 注册点整体跟搬） |
| `chips/cms8s78xx/src/cms8s_extint.cpp` | 🆕 | 端口中断 + 引脚选择 |
| `src/mcs51_gpio/uart/timer/extint.cpp` | ✏️ | 标准模型 + 快路径短路 |
| `src/mcs51_peripheral.cpp` | ✏️ | core 表 + BSS 注册表 + `mcs51_peripheral_register()`；cms8s 三件套行删除（改走注册） |
| `chips/cms8s78xx/src/cms8s_register.cpp` | 🆕 | `cms8s78xx_register()`（追加芯片包描述符：本阶段终态 7 条 = adc/buzzer/sys + gpio/extint/uart/timer；幂等去重）；`at89c52_register()` 空实现同 commit |
| `src/cms8s_sys.cpp` | ✅ pre-completed | ~~`has_wdt` 改读描述符 `wdt_present`（一行）~~ —— S3-H4 已提前完成（`eb6e8db`，行为中性，wdt 双模测试锁定），stage4 无需重复 |

## 3. 任务拆分

### Task S4-1：GPIO + ExtInt `[状态: ✅ 已完成（2026-09-12，ea1db89 + 2a95f23 + 62e61bf）]`

- [x] **Step 1**：`cms8s_gpio.cpp` 承接 TRIS/OD/UP/CFG 地址表与解释；通用 GPIO 保留准双向，`caps_cache` 短路后走 per-context 钩子（芯片 `init` 向显式 ctx 安装，`reset` 重装；S4-D1 加宽 `may_drive` 带 `level`）。
- [x] **Step 2**：`cms8s_extint.cpp` 承接 `P0EXTIE`/`EICFG`/`PS_INT*`/vectors + W0C 钩子 + 端口采样状态入池；通用仅 INT0/INT1（选择器槽 `SELECTOR_NONE`，芯片 init 补丁；S4-D2 跨 reset 基线不保留）。

### Task S4-2：UART + Timer + 注册 `[状态: ✅ 已完成（2026-09-12，95a7694 + 03ae5a8 + d26d8f4 + 42d351c + c0e5d43 + 62e61bf）]`

- [x] **Step 1**：`cms8s_uart.cpp` 承接 FUNCCR/重映射 + TMR4/TMR2/BRT 波特分支（含 T34MOD/T2CON-baud 交叉读）；通用 UART 固定引脚 + Timer1 波特，TX 引擎经新增 `uart_hooks` 外包判定（S4-D3）。
- [x] **Step 2**：`cms8s_timer.cpp` 承接 T3/T4/捕获/W0C + T2-CMS8S 扩展，hook 注册点整体跟搬；通用保留 Timer0/1/标准 T2；共享 T2 地址钩子链式（芯片后注册并链调 core C-ABI，S4-D4）。
- [x] **Step 3**：peripheral 改自注册（总纲 §3.1b 协议）：core 表删 cms8s 三件套行，加 BSS 注册表 + `mcs51_peripheral_register()` + 测试缝；新增 `cms8s_register.cpp`/`at89_register.cpp`（空实现）；三处循环（reset/microstep/next-event）改遍历"core 表 + 注册表"；~~`has_wdt` 切 `wdt_present`~~（✅ S3-H4 已提前完成，见上表）。附带（评审纠正包）：`s_cms8s_priv_pool` + `cms8s_soc_bind` 自 `cms8s_sys.cpp` 迁入 `cms8s_register.cpp`；T3/T4/捕获/端口采样状态随 `cms8s_timer.cpp`/`cms8s_extint.cpp` 落地同步入池（S2-2D6 移交）；`caps_cache` 调用点迁移清单本阶段认领（GPIO/ADC 热路径断言 caps 短路覆盖率，UART/Timer 同理——见 §6 Check 3）。
- [x] **Step 4（双列表同步，防 stage4~6 窗口 wasm 二次断链）**：新增 6 个芯片 `.cpp`（register×2 + gpio/uart/timer/extint）同时追加单体库 `_MCS51_COMPAT_SRCS` 与 wasm 手写源列表（含 `test/` include 补丁——wasm 共享 e2e 驱动含 harness；注明 stage6 删后者）；总纲 §6.1-7 为常设规则。
- [x] **Step 5（测试注册脚手架，P0 防断裂项）**：新增测试专用 `frameworks/mcs51/test/mcs51_test_harness.h`（仅测试链接，禁入生产库）：幂等 `mcs51_test_use_family(family)` = 注册表复位 + `xxx_register()`；逐个更新 cms8s 单测与 e2e 入口（含走 bridge 的 e2e——测试构建无 generated glue，必须显式调），classic 侧不受影响； sweep 清单落本计划附录（22 文件 + 3 直接挂载 + 2 顺序修正 + 1 拆分，执行时以 `grep -l` 为准——见附录 A）。无此脚手架则静态表摘除后 cms8s 单测集中挂掉。

## 4. 验收

- L1：GPIO 热路径单测 assert 标准件零钩子调用。
- L4：`Select-String -Path src/mcs51_gpio.cpp,src/mcs51_uart.cpp,src/mcs51_timer.cpp -Pattern 'cms8s|0xF0'` 零命中。

## 5. 风险与回滚

- R：钩子时序回归 → 缓解：stage1 E-02 场景 + UART/Timer 回归全跑；回滚 `git revert <S4-commit>`（芯片新文件独立 commit）。

## 6. 阶段自审自我检验清单（Self-Audit Checkpoint）
- [x] **目录落位**：新建外设代码均落在 `chips/cms8s78xx/src/`（`cms8s_gpio.cpp`, `cms8s_uart.cpp`, `cms8s_timer.cpp`, `cms8s_extint.cpp`）+ 注册入口 `cms8s_register.cpp` / `chips/at89c52/src/at89_register.cpp` + 测试脚手架 `test/mcs51_test_harness.h`。
- [x] **通用纯净度**：`src/mcs51_gpio.cpp`、`src/mcs51_uart.cpp`、`src/mcs51_timer.cpp`、`src/mcs51_extint.cpp`、`src/mcs51_peripheral.cpp` 零专有寄存器/厂商符号（`Select-String 'cms8s|0xF0'` 五文件零命中；lint PASS）。
- [x] **快路径零间接开销**：标准 8051 GPIO 读写断言 caps 掩码短路有效（gpio_dir T6：classic 挂载计数钩子后读写，hook 调用次数为 0；T7：增强件调用次数 > 0）；UART（`UART_REMAP` 分发，charge/tx_ready 双家族覆盖）/Timer（注册表掩码选择，无 core 内 caps 分支残留——选择即分发）同理。
- [x] **注册协议**：monolith 时代全量测试在显式 register 后仍全绿（66/66）；classic target 不链接 cms8s 符号（`at89_register.cpp.obj` 仅导出 `_at89c52_register`，`nm` 抽查通过；单体库拆分 stage6 落地）。
- [x] **双轨状态**：UART/Timer/GPIO/ExtInt 单元测试全绿（66/66 mcs51 host+wasm；wasm 轨 13/13；host 全量 17 项非 mcs51 失败与 stage3 基线一致，零新增）。

## 7. 自审签署（2026-09-12）
- **Check 1 目录落位**：通过。6 个芯片 `.cpp` + `mcs51_test_harness.h` + `test_mcs51_extif_w0c.cpp` 全部合树（§3.2 终态目录树；`git mv` 不适用——全为新建/原地改写）。
- **Check 2 依赖单向与残留**：通过。lint PASS（S4 共修剪 14 行 waiver：peripheral×2、trap、clock、gpio×2、extint 头×3 + extint 码×3、uart 头 + uart 码×5、timer×4；删后 PASS 反证无活行误删）；core 头零 `chips/`/`devices/` 私有头包含；`mcs51_context.h` 无厂商 priv 头包含；`MCS51_HAS_ADC0832` 未引入（bridge 保持 S3 状态）。
- **Check 3 双轨与契约**：通过。host mcs51 66/66（含新增 extif_w0c 与 gpio T6/T7 L1 断言、STRICT 五双生）、wasm 13/13；`sizeof(Mcu51Context)` 75536（-136：extint 端口采样 -72、timer 扩展 -72、uart_hooks +8；ceiling 76696 内通过）；E-02 链（refchain/cms8s_e2e/iron）全绿无复发。17 项非 mcs51 失败与本阶段零文件交集（PAL/DAL `-Werror` 存量 + 缺外仓 codegen + 全量构建 `oled_dashboard` 缺 `app_codegen.py`），判 master 先存问题（与 stage3 同口径；失败集数量一致）。
- **Check 4 计划闭环**：S4-1/S4-2 checkbox 全勾；S4-D1–D4 四项裁决归档如下；总纲 §5 stage4 状态列待置 `✅ 已完成`（与本提交同批）。
- **执行裁决 S4-D1（may_drive 钩子加宽）**：stage2 声明的 `(ctx, pin)` 表达不了开漏语义（高电平释放抑制、低电平仍驱动），加宽为 `(ctx, pin, level)`。stage2 零调用点，安全；同步更新 `soc_priv_isolation` 的 2 个测试桩。
- **执行裁决 S4-D2（端口采样基线跨 reset 不保留）**：`port_pins` 入池后 core 无法再在 `context_reset` 中保存/恢复（S2-2 D6 移交的必然代价）。下一次 poll 经 `have_sample` 门重建基线，不合成边沿，等价于冷启动硅片行为；`low_power` T3 已显式 prime，兼容。
- **执行裁决 S4-D3（uart_hooks 新增）**：TX 引擎（SBUF 钩子、capture、TI/IRQ、charge）留 core 单份，源选择（FUNCCR/重映射/BRT/TMR4/TMR2）经新增 `Mcs51UartHooks` 外包——与 GPIO 钩子同纪律，避免复制 40 行陷阱纪律关键代码。stage2"只做挂载"字面让步于零重复原则。
- **执行裁决 S4-D4（T2 骨架留 core + 共享钩子链式）**：T2 中断 plumbing（T2CON/TL2/TH2/RCAP + 0xC9/0xCF 数字地址）保留 core——重写为标准 TF2/ET2 语义属高风险行为重构，超出 stage4 范围；W0C 钩子/PS/CCEN/T3/T4 下沉。共享地址槽位由芯片链式钩子取代（先调 core C-ABI 再同步 compares，触发集与旧代码一一对应，含同值重写；CCEN/CCLx 无需独立触发）。微偏离：运行中切出 timing 模式即解散 compares（旧代码残留 stale，无测试覆盖，视为 bugfix）；T2 默认引脚（P1.6 等模型缺省）保留，不重定基线。
- **测试资产修正（CPL-22 纵深）**：`gap12` (5) 迁出 core（`test_mcs51_extif_w0c.cpp`，harness 为装载性依赖）；`uart_charge`/`uart_tx_ready` 的 `init_ctx` 由 reset-first 纠为 register→set→reset（静态表时代无感，注册表时代缺装 hooks——STRICT child 3 为证）；STRICT 五双生补 `test/` include；`timer_ext_clk` T5/T6 补 chip init/poll/step 调用；`port_extint`/`extint_model`/`low_power` 补 chip 调用点。详见附录 A。
- **Safety review**：Risk level 中（GPIO 热路径 + 外设定时/串口 + 分发循环 + 池迁移）；Checklist phases run 1、2、3、4、10、12；Findings 无（注册表有界 BSS + 断言 + 幂等；池迁移逐字；bridge 胶 pre-stage6 为 no-op；钩子 per-context；emcc 全绿；66/66）；Fixed 两处（wasm harness include 目录；STRICT 双生 include 目录）；Assumptions 沿用 active-ctx 惯例（chip init 显式 ctx；`cms8s_timer_step_to` 以 active 为目标，与 core `step_to` 同约）；Commands run 见 Check 3。

## 附录 A：S4-2 Step 5 测试 sweep 清单（✅ 执行完毕，2026-09-12）- **harness 头**：`test/mcs51_test_harness.h`（`mcs51_test_register_family` 注册专用 + `mcs51_test_use_family` 注册并切 active 上下文；声明 `cms8s78xx_register`/`at89c52_register`）。
- **22 文件 set_family 前插 register**（零语义变更插入，保持原 reset 顺序）：`test_cms8s_buzzer`、`test_cms8s_vendor_stdriver`、`test_cms8s_adc_instant`、`test_extint_model`、`test_mcs51_adc_refchain`、`test_mcs51_cms8s_adc_e2e.c`、`test_mcs51_gpio_dir`（classic+cms8s 双点）、`test_mcs51_port_extint`、`test_mcs51_t234_fsys`、`test_mcs51_uart_charge`、`test_mcs51_uart_tx_ready`、`test_mcs51_wdt_ta`（双点）、`test_mcs51_wink_mcu`、`test_mcs51_xsfr_tripwire`、`test_mcs51_ext_bus`（param）、`test_mcs51_family_schema`（3 点）、`test_mcs51_low_power`、`test_mcs51_silicon_seeds`（双点）、`test_mcs51_soc_priv_isolation`（3 点）、`test_mcs51_timer_ext_clk`、`test_mcs51_xram_aperture`（param）、`test_sfr_operators_coverage`。
- **3 直接挂载**（不走 `context_reset` 的模型直驱测试）：`port_extint` + `extint_model` 显式调 `cms8s_extint_init/reset`；`timer_ext_clk` T5/T6 在 core init 后补 `cms8s_timer_init`（后注册者赢槽位）+ capture/compare 调用点补 chip poll/step。
- **2 顺序修正**：`uart_charge`/`uart_tx_ready` 的 `init_ctx` 改 register→set→reset（STRICT child 3 缺 abort 为证）。
- **1 拆分**：`gap12` (5) 迁 `test_mcs51_extif_w0c.cpp`（+1 用例，65→66）。
- **构建 wiring**：`wink-micro-os/test/CMakeLists.txt`（`add_mcs51_host_test` 加 `test/` include；STRICT 五双生同加；注册 `test_mcs51_extif_w0c`）+ wasm cmake（6 芯片 TU + `test/` include）。

## 附录 B：Stage4 评审加固追记（S4-D5 / S4-H1~H3，2026-09-12）
- **事由**：stage4 关闭后的资深架构复审确认了三项真问题（两项生命周期、一项产线窗口），本追记在进 stage5 前一次性关闭（S3-H1~H7 先例）。
- **S4-D5 产线过渡默认（P0，`633093b`）**：`mcs51_family_select.h` 全仓不存在 → pre-stage6 产线构建 registry 恒空 → CMS8S 产线仿真无芯片模型（片上 ADC 应用直接挂，E-02 产线复活；T3/T4/端口中断/捕获比较挂；其余 permissive 失真）。bridge 加 `#elif defined(WINK_MCU_CMS8S78XX)` 默认调 `cms8s78xx_register()` + stage6 到期 lint waiver（S3-D2 先例）。Classic 构建宏分支消除（`nm` 抽查 bridge 对象无芯片未定义符号）；host 测试 AT89 宏 + harness 双重注册幂等。stage6 codegen 落地即删本分支。
- **S4-H1 上下文钉扎（`92fb063`）**：`mcs51_trap_register_*` 全员解析 active ctx，而 `mcs51_context_reset(ctx)` 从不切换——非 active reset 会把钩子 bleed 到别的实例。reset 双循环前后 save/restore active（无异常代码，直存直取，与 `saved_idx` 同惯用法）。
- **S4-H2 重装统一（`92fb063`）**：新契约——**任何 reset 必须能独立于 init 重建全部运行时注册**，init 只负责首次绑定。core timer/uart 注册块下移 reset（init 转调）；芯片 adc/buzzer 同理；sys/extint/timer 提 `install_*_hooks()` 双调（sys 含 `sfr_write_notify`，此前 standalone reset 会丢 TA 窗口）。
- **S4-H3 harness 排序纪律（`92fb063`）**：`mcs51_trap_reset()` 清全部已装钩子——必须在 harness 之前调，之后调则芯片钩子静默丢失。已注记于 harness 头（全仓现调用点顺序皆合规）。
- **终验**：lint PASS｜mcs51 66/66｜wasm 13/13｜host 全量非 mcs51 失败集与 stage3 基线一致｜`nm` 双抽查（bridge 对象、at89 对象）通过。

## 附录 C：Stage4 复审整改执行记录（S4-H4 清理包，2026-09-12）
- **事由**：stage4 关闭后的两轴复审（Standards / Spec，diff `eb6e8db...HEAD`）确认行为/鲁棒性真问题 4 项 + 卫生项若干；按"立即修 / 对齐 / 验证后记"三档执行（S4-D5/H1-H3 先例）。
- **行为修复（A 档）**：
  - A1 `cms8s_uart.cpp`：删除 `txd_alt` 死代码（`(void)` 占位 + 两个仅此使用的 CFG 常量），TXD 语义改为注释说明。
  - A2 `mcs51_uart.cpp` `uart_baud_hz_std`：恢复 `has_xsfr` 守卫——classic 的 0x8E 是未定义 SFR，杂散写不得改变波特分频；`test_mcs51_uart_charge` §9 新增 stray-write 断言锁定。
  - A3 `cms8s_timer_init` / `cms8s_extint_init` 改为委托对应 `_reset`（S4-H2 契约延伸）：独立 init 也能播种 PS_* 选择器并重装钩子；新增 timer（`test_mcs51_timer_ext_clk` T5 先脏后 init）与 extint（`test_mcs51_extint` setup）断言。
  - C（由"验证后记"升级为修复）`mcs51_extint_next_event_us`：恢复按 `last_sample_us + SAMPLE_PERIOD_US` 通告 INT0/INT1 采样计划。旧 core 对所有家族通告；新代码返 `UINT64_MAX` 会让无定时器的 classic 在 IDLE 落到 Mode B（无周期采样、无调度唤醒，附带注释"经典行为不变"与事实不符）。`test_mcs51_low_power` 新增 Test 4 锁定。
- **B 档对齐**：注册表容量依据（3 core + 7 chip = 10，上限 12 留余量）写入 `mcs51_peripheral.h`；溢出由"NDEBUG 静默丢弃"改为无条件 `abort`（ADR-0012 契约诚实）；stage2 §4 预算表行与 stage4 §2 注册条目文案同步（"三件套"→本阶段终态 7 条）。
- **C 档验证后记（不改码）**：`mcs51_trap.h` M3 注释修正（`extern "C"` 内 `static` 合法且三工具链实测通过，原"GCC rejects"为误）；timer3/4 孪生与 test harness 重复声明按"静态分发可读性优先"保留（后者已去重）；CMakeLists 缩进 churn 留 stage6 重写；`<string.h>` vs `<cstring>` 风格差异不动。
- **B6 文案**：stage4 §1"（+标准 T2）"为历史文案误差（T2 RCLK/TCLK 波特在 stage4 前亦未实现，非本阶段回归）；实现列入 stage5 附录 B（与 S5-1 Step 1b 同批评估）。
- **A4 风格清理**：stage4 新增行的 BARR-C #1 大括号（50+ 处）与 >80 列行全部修复；`lint_mcs51_layering.py` PASS（首轮曾因新增注释含厂商名被门禁拦下，已改写）。
- **终验（2026-09-12）**：lint PASS｜L4 五文件 `cms8s|0xF0` 零命中｜host mcs51 66/66｜wasm 13/13｜host 全量失败 17 项 = stage3 基线（零新增，均非 mcs51）｜新增行 >80 列 0 处｜注册表溢出路径改为 abort（容量 12 > 实需 10，未触发）。
