# Stage5：中断向量表与 XDATA/XSFR 表驱动化

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S5-IRQBUS` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
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

### Task S5-1：向量表驱动 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：通用 ISR 删 `s_default_irq_map` 中 ADC/PWM/I2C/SPI/UART1 硬编码（缩水为 0~5）；`cms8s` reset 中经 `wink_mcs51_set_irq_map_entry` 逐项装载扩展 profile（复用既有 API，不新增机制）。
- [ ] **Step 1b（仲裁特判抽离，P0 阻断项）**：`mcs51_isr.cpp:309-312` 的 T2 `0xC9u/0xCFu` 多标志特判抽为 per-context flag-predicate hook（默认 = 标准单 bit 判定即现 `else` 分支；CMS8S reset 安装多标志版；UART0/INT0/INT1 特判为标准语义，留内联）。仲裁循环本身不动。
- [ ] **Step 1c（映射表入 ctx，P0 隔离项）**：file-static `s_irq_map[13]` 迁入 `Mcu51Context`（+104B，记 stage2 §4 预算表；诊断计数器等 M4 项留 file-static）；`reset` 按家族装载（core 默认 0~5 + 芯片扩展 overlay），`set/get/reset_irq_map` 三 API 改操作 active ctx（签名不变）；`ensure` 全局 once 语义删除。否则 CMS8S 装载后切 classic，扩展向量借尸还魂，直接击穿 L1 绝缘验收。
- [ ] **Step 2**：AT89 模式断言扩展向量不可达 + XSFR 窗口关闭的绝缘单测（含双 context 分属异家族并存：一方装载扩展后另一方仍绝缘）；`mcs51_shim_audit.py` 的 isr 交叉校验同步家族化（freshness 门禁同改；默认表/芯片表新位置同步更新脚本；验证命令见 stage3 Step 4，校验对象改为家族化后的两表位置）。

### Task S5-2：XSFR 参数化 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：tripwire 先判窗口存在（`xsfr_size!=0`），再查芯片侧 allowlist；allowlist 头下沉芯片包（`cms8s_xsfr_allowlist.h`，生成器目标路径同步），通用零包含。注：`KIND_XSFR` 是代理协议 tag，不动。
- [ ] **Step 2**：未建模 XSFR tripwire 归芯片模型所有，通用只做窗口分发；`mcs51_xsfr.hpp` 注释去厂商味。

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
- [ ] **标准 8052 T2 波特（RCLK/TCLK）补建模（P2，择机）**：core UART 标准路径目前仅 Timer1（`mcs51_uart.cpp` `uart_baud_hz_std`）；8052 的 `T2CON.RCLK/TCLK` 由 Timer2 溢出驱动收/发波特这一机制从未实现（stage4 计划原文"（+标准 T2）"为文案误差，已修，非 stage4 回归）。实现需 core timer 溢出与 core UART 联动（非芯片包），建议与本阶段 S5-1 Step 1b 的 T2 多标志特判抽离同批评估，避免两次改动 T2 路径。

## 6. 阶段自审自我检验清单（Self-Audit Checkpoint）
- [ ] **目录落位**：`chips/cms8s78xx/include/cms8s_xsfr_allowlist.h` 完全归位芯片目录。
- [ ] **通用纯净度**：`src/mcs51_isr.cpp` 仅包含 0~5 标准向量；`src/mcs51_xdata.cpp` 零厂商 allowlist 头包含。
- [ ] **物理绝缘自审**：AT89 模式下 CMS8S 扩展中断与 XSFR 窗口绝缘单测通过。
- [ ] **双轨状态**：双平台构建全绿，中断调度单测全绿。
