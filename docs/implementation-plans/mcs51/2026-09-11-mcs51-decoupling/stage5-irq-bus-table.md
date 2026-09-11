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

- ✅ 通用 ISR 仅标准 0~5 向量仲裁，其余从 `irq_vector_table + irq_count` 装载；AT89 模式下 CMS8S 中断物理绝缘。
- ✅ `mcs51_xdata.cpp` 去 `mcs51_xsfr_allowlist.h` 强包含，按 `xsfr_base/size` + 芯片校验分发；经典无窗口。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `src/mcs51_isr.cpp` | ✏️ | 标准仲裁 + 描述符装载 |
| `src/mcs51_xdata.cpp` | ✏️ | 窗口参数化，删 allowlist 包含 |
| `chips/cms8s78xx/` | ✏️ | 向量表行 + XSFR 校验归位 |

## 3. 任务拆分

### Task S5-1：向量表驱动 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：通用 ISR 删 `s_default_irq_map` 中 ADC/PWM/I2C/SPI/UART1 硬编码，改读描述符。
- [ ] **Step 2**：AT89 模式断言扩展向量不可达 + XSFR 窗口关闭的绝缘单测。

### Task S5-2：XSFR 参数化 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：`KIND_XSFR` 改为按 `xsfr_size!=0` 判定；allowlist 头下沉芯片包，通用零包含。
- [ ] **Step 2**：未建模 XSFR tripwire 归芯片模型所有，通用只做窗口分发。

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
