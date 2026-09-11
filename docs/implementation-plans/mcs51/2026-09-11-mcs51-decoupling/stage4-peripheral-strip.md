# Stage4：外设增强机制与私有外设剥离

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S4-PERIPH` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
| **优先级** | 🟡 P1 |
| **关联 CPL** | CPL-03（GPIO）、CPL-04（UART）、CPL-05（Timer）、CPL-07（ExtInt）、CPL-10（自注册） |
| **前置依赖** | stage3（头文件已归位，钩子可用） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ `cms8s_gpio/uart/timer/extint.cpp` 承接全部专有逻辑，通用文件经 `caps_cache` + per-context 钩子外包（钩子结构 stage2 已定义，本阶段只做挂载）。
- ✅ T2 按语义拆分：标准骨架（T2CON/TL2/TH2/RCAP）留 core，CMS8S 扩展（T2IF W0C/T2PS/捕获解释/CCEN 系/T34MOD）下沉（注意 `0xCA` 既是 RCAP2L 又是 RLDL，按语义拆，地址留 core）。
- ✅ UART 与 Timer 协同拆分：TMR4/TMR2/BRT 波特分支随 FUNCCR 同迁 `cms8s_uart.cpp`；通用 UART 固定引脚 + Timer1（+标准 T2）波特。
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
| `chips/cms8s78xx/src/cms8s_register.cpp` | 🆕 | `cms8s78xx_register()`（追加三件套描述符；幂等去重）；`at89c52_register()` 空实现同 commit |
| `src/cms8s_sys.cpp` | ✅ pre-completed | ~~`has_wdt` 改读描述符 `wdt_present`（一行）~~ —— S3-H4 已提前完成（`eb6e8db`，行为中性，wdt 双模测试锁定），stage4 无需重复 |

## 3. 任务拆分

### Task S4-1：GPIO + ExtInt `[状态: ⏳ 待开始]`

- [ ] **Step 1**：`cms8s_gpio.cpp` 承接 TRIS/OD/UP/CFG 地址表与解释；通用 GPIO 保留准双向，`caps_cache` 短路后走 per-context 钩子（芯片 `init` 向 active ctx 安装，`reset` 重装）。
- [ ] **Step 2**：`cms8s_extint.cpp` 承接 `P0EXTIE`/`EICFG`/`PS_INT*`/vectors；通用仅 INT0/INT1。

### Task S4-2：UART + Timer + 注册 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：`cms8s_uart.cpp` 承接 FUNCCR/重映射 + TMR4/TMR2/BRT 波特分支（含 T34MOD/T2CON-baud 交叉读，与 timer 拆分同阶段协同）；通用 UART 固定引脚 + Timer1（+标准 T2）波特。
- [ ] **Step 2**：`cms8s_timer.cpp` 承接 T3/T4/捕获/W0C + T2-CMS8S 扩展，hook 注册点（原 `mcs51_timer_init:884-905` 的 T2IF/EIF2/T34MOD/TL3/CCLx 注册）整体跟搬；通用保留 Timer0/1/标准 T2。
- [ ] **Step 3**：peripheral 改自注册（总纲 §3.1b 协议）：core 表删 cms8s 三件套行，加 BSS 注册表 + `mcs51_peripheral_register()` + 测试缝；新增 `cms8s_register.cpp`/`at89_register.cpp`（空实现）；三处循环（reset/microstep/next-event）改遍历"core 表 + 注册表"；~~`has_wdt` 切 `wdt_present`~~（✅ S3-H4 已提前完成，见上表）。附带（评审纠正包）：`s_cms8s_priv_pool` + `cms8s_soc_bind` 自 `cms8s_sys.cpp` 迁入 `cms8s_register.cpp`（芯片主控入口收敛池归属，消 adc/buzzer 对 sys.cpp 的横向依赖）；T3/T4/捕获/端口采样状态随 `cms8s_timer.cpp`/`cms8s_extint.cpp` 落地同步入池（S2-2D6 移交）；`caps_cache` 调用点迁移清单本阶段认领（S0 铺设、零生产读者——GPIO/ADC 热路径断言 caps 短路覆盖率，UART/Timer 同理）。
- [ ] **Step 4（双列表同步，防 stage4~6 窗口 wasm 二次断链）**：新增 4 个芯片 `.cpp` 同时追加单体库 `_MCS51_COMPAT_SRCS` 与 wasm 手写源列表（注明 stage6 删后者）；总纲 §6.1-7 为常设规则。
- [ ] **Step 5（测试注册脚手架，P0 防断裂项）**：新增测试专用 `frameworks/mcs51/test/mcs51_test_harness.h`（仅测试链接，禁入生产库）：幂等 `mcs51_test_use_family(family)` = 注册表复位 + `xxx_register()`；逐个更新 cms8s 单测与 e2e 入口（含走 bridge 的 e2e——测试构建无 generated glue，必须显式调），classic 侧不受影响； sweep 清单落本计划附录（当前约 15+ 文件，执行时以 `grep -l cms8s_.*init\|mcs51_context_reset` 为准）。无此脚手架则静态表摘除后 cms8s 单测集中挂掉。

## 4. 验收

- L1：GPIO 热路径单测 assert 标准件零钩子调用。
- L4：`Select-String -Path src/mcs51_gpio.cpp,src/mcs51_uart.cpp,src/mcs51_timer.cpp -Pattern 'cms8s|0xF0'` 零命中。

## 5. 风险与回滚

- R：钩子时序回归 → 缓解：stage1 E-02 场景 + UART/Timer 回归全跑；回滚 `git revert <S4-commit>`（芯片新文件独立 commit）。

## 6. 阶段自审自我检验清单（Self-Audit Checkpoint）
- [ ] **目录落位**：新建外设代码均落在 `chips/cms8s78xx/src/`（`cms8s_gpio.cpp`, `cms8s_uart.cpp`, `cms8s_timer.cpp`, `cms8s_extint.cpp`）。
- [ ] **通用纯净度**：`src/mcs51_gpio.cpp`、`src/mcs51_uart.cpp`、`src/mcs51_timer.cpp`、`src/mcs51_extint.cpp` 零专有寄存器/厂商符号。
- [ ] **快路径零间接开销**：标准 8051 GPIO 读写断言 caps 掩码短路有效（芯片测试桩统计 hook 调用次数，标准件场景为 0），未产生间接函数调用。
- [ ] **注册协议**：monolith 时代全量测试在显式 register 后仍全绿；classic target 不链接 cms8s 符号（`nm` 抽查）。
- [ ] **双轨状态**：UART/Timer/GPIO 单元测试全绿。
