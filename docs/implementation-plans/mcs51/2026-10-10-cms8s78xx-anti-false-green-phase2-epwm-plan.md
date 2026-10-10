# CMS8S78xx EPWM 外设物理波形与故障刹车闭环实施计划（第二阶段）

| 项 | 内容 |
|---|---|
| 编号 | `PLAN-20261010-MCS51-ANTI-FALSE-GREEN-PHASE2-EPWM` |
| 日期 / 状态 | 2026-10-10 / Completed (已通过全量门禁与变异审计验证) |
| 版本 | v3.0：融合“防假绿双层约束体系”（Frameworks 内核与单测 + Scenario 组合断言），全面封堵 6 大假绿死角 |
| 技术方案 | [EPWM 防假绿技术方案（第二阶段）](../../zh/tech-designs/mcs51/2026-10-10-cms8s78xx-anti-false-green-phase2-epwm-design.md) |
| 审查依据 | [假绿与完整性审查](../../reviews/mcs51/2026-10-09-cms8s78xx-false-green-and-framework-completeness-review.md)（F1、F2、F3） |
| 原厂依据 | `CMS8S78xx参考手册_V1.1.1.pdf` 第 16 章（EPWM） |
| 边界契约 | **双层闭环原则**：零侵入原厂 C 源码、不侵入 UniSim 外部全局内核，所有防假绿硬约束完全在 `frameworks/` 与 `unisim-scenarios/` 两层闭环 |

---

## 1. 阶段目标与准出门禁

| 阶段 | 治理对象 | 核心操作 | 验收出口门禁 |
|---|---|---|---|
| **S0** 寄存器语义核准与环境隔离 | 原厂手册 V1.1.1、垫片 `REG_CMS8S78XX.H` 与运行器 | 1. 核对原厂手册 Ch.16 寄存器位定义；<br>2. 修复垫片中 `EPWM_PWMCON_PWMRUN_Pos` 错误（7 $\to$ 6）；<br>3. 建立 `unisim-scenarios/mutations/` 独立变异目录，在 `run_cms8s78xx_baseline.py` 中过滤变异文件，防止基线统计污染（死角 6）； | 寄存器与垫片语义偏差 100% 消除；基线扫描纯净隔离 |
| **S1** EPWM 核心模型与时钟树 MUX 重构 | `cms8s_epwm.cpp` 与 `cms8s_epwm.h` | 1. 结构体收敛为 PG0~PG3 四路输出；<br>2. 实现时钟树多路复用器 MUX：`PWMnDIV == 0xFF` 直通 $F_{sys}$ 旁路 PSC（死角 2）；<br>3. 接入动态时钟与分数余数累加器 `tick_fraction_rem`；<br>4. 重构 `next_event_us()` 支持 CMP 边沿/零点/周期点预测；<br>5. 实现 Stop 模式刹车时硬件清零 `PWMCNTE` 与状态机锁死； | 模型编译通过；不同调度微步进下边沿相位一致；变频无截断漂移；时钟分频严格符合手册 |
| **S2** 波形发生标杆闭环与组合断言 | `epwm_down_count`<br>`epwm_updown_count` | 1. `epwm_down_count`：`ASSERT_WAVEFORM` (4998.96Hz 50%) + `ASSERT_POINT` 半周期关键点互补反相对立采样，杜绝同相直通假绿（死角 1）；<br>2. `epwm_updown_count`：`ASSERT_WAVEFORM` (2500Hz) + `ASSERT_POINT` 在 $T/8$ 处采样断言为 0，拦截下计数伪装（死角 3）；<br>3. Host C++ 单测注入 25% 占空比验证脉冲严格居中； | 正向波形断言 PASS；同周期互补反相 180° 闭环；脉冲居中特征严格吻合 |
| **S3** 故障刹车全生命周期与稳态锁死 | `epwm_brake_fb`<br>`epwm_brake_stop` | 1. `epwm_brake_fb`：Recover 模式采用 `stableLevel` + `holdForUs: "20ms"`（100 个周期）杜绝偶合假冻结（死角 4），实现“波形 $\to$ 冻结 $\to$ 恢复 $\to$ 重冻结”双循环；<br>2. `epwm_brake_stop`：Stop 模式验证 `PWMCNTE` 硬件清零锁死，清除并重启后恢复；<br>3. 验证 CPU 关中断（`EA=0`）时硬件刹车依然立即锁死； | 刹车全生命周期状态机断言 PASS；长窗口稳态无脉冲漏跑；硬件与中断解耦验证 PASS |
| **S4** 变异防御套件、门禁拆分与交付 | 全量回归与变异审计脚本 | 1. 编写专用变异验证工具 `verify_epwm_anti_false_green.py`，正则匹配错误日志，验证必须精准命中目标断言，排除 Crash/Panic/超时假拦截（死角 5）；<br>2. 改写纠正既有 `test_mcs51_cms8s_epwm.cpp` 错误语义，增加逐 tick 互补直通检测 `assert(!(pg0==1 && pg1==1))`；<br>3. 拆分执行 Host C++、Python 治理、Wasm 场景、SDCC 硬件门禁； | 5 大变异均以退出码 1 及目标错误日志拦截；Host C++ / Python / SDCC 门禁 100% 通过；镜像 0 漂移 |

---

## 2. 详细任务拆分

### S0 — 寄存器语义核准、垫片修复与环境隔离
- [x] **手册寄存器语义核准**：对照原厂手册 V1.1.1 第 16 章，核准通道数（PG0~PG3）、互补位（`PWMMODE[5:4]=01b`）、分频位（`PWMRUN=bit6`，0=使能，1=禁止）、时钟选择 MUX 与刹车模式。
- [x] **审查并修复垫片 `REG_CMS8S78XX.H`**：
  - 纠正 `EPWM_PWMCON_PWMRUN_Pos` 从 7 为 6；
  - 审查 `EPWM_Start()` 与 `EPWM_Stop()`，确认原厂仅操作 `PWMCNTE`，不违规覆写 `PWMCON`。
- [x] **基线运行器隔离与变异目录规范 (解决死角 6)**：
  - 在各标杆用例目录下统一建立 `unisim-scenarios/mutations/` 子目录存放反向用例；
  - 在 `wink-micro-os/frameworks/mcs51/tools/run_cms8s78xx_baseline.py` 中显式排除 `*.fail.scenario.json`，确保 37 个 App 的基线扫描不受变异文件污染；
  - 确认 Master 分支基于 `7d96039d`，确认工作区洁净。

### S1 — EPWM 核心物理模型、时钟树 MUX 与边沿调度重构
- [x] **通道架构收敛**:
  - `Cms8sEpwmState` 改为 `uint8_t pg_pin_level[4]`（收敛为 PG0~PG3 4 通道，废弃 6 通道臆测）。
- [x] **时钟树多路复用器 MUX 实现 (解决死角 2)**:
  - 在 `cms8s_epwm.cpp` 中严格按手册实现两级时钟选择：
    ```cpp
    // 第一级：PWM01PSC / PWM23PSC 预分频
    if (psc_reg == 0) return 0; // PSC=0 预分频停止，通道停止计数
    // 第二级：PWMnDIV
    if (div_reg == 0xFFu) return fsys; // 直通系统主频 Fsys，旁路 PSC！
    const uint32_t f_psc = fsys / (psc_reg + 1u);
    if (div_reg == 0x04u) return f_psc;       // /1
    if (div_reg == 0x00u) return f_psc / 2u;  // /2
    if (div_reg == 0x01u) return f_psc / 4u;  // /4
    if (div_reg == 0x02u) return f_psc / 8u;  // /8
    if (div_reg == 0x03u) return f_psc / 16u; // /16
    ```
- [x] **动态时钟与分数余数累加 (F3 彻底闭环)**:
  - 接入 `wink_mcs51_get_clock_hz()`；
  - 增加 `uint32_t tick_fraction_rem[4]` 累加器，杜绝短步进整数除法截断漂移；
  - 支持变频切换时先结算上一时钟区间。
- [x] **按边沿事件调度预测**:
  - 重构 `cms8s_epwm_next_event_us()`，同时预测 CMP 上升/下降匹配时间、零点时间、周期点时间，返回各活动通道的最近边沿 $\min(\Delta t)$；
  - 验证：在 1us、10us、50us 等不同推进步长下，波形跳变边沿时间戳完全一致。
- [x] **波形发生与模式纠偏**:
  - 互补模式判定修正：检查 `(pwmcon & 0x30) == 0x10`（PWMMODE[5:4] == 01b），禁止使用 `& 0x08`（GROUPEN）；
  - 边沿对齐 (Down-Count)：在 $[CMPn, 0)$ 区间拉高，0 点拉低并重载；
  - 中心对齐 (Up-Down)：上数到达 CMPn 拉高，下数到达 CMPn 拉低，按比较事件状态机建模；
  - PG 输出驱动：检查 `PWMOE` 与 `PxxCFG == 0x04`，调用 `js_pal_gpio_write(pin, level, 3)`。
- [x] **外部 GPIO 刹车仲裁 (F2 彻底闭环)**:
  - `resolve_ps_pin_val()` 全面切换为 `mcs51_gpio_bit_read_pin(port, bit)`。
- [x] **刹车状态机与硬件清零**:
  - Stop 模式：硬件将 `PWMCNTE` 强制清零，输出锁定；
  - 撤销后必须写 `PWMBRKC[3]=1` 且由软件重新置位 `PWMCNTE` 才能恢复；
  - Recover 模式：撤销后等待下一 Reload 加载点自动恢复。

### S2 — 波形发生标杆闭环与组合断言 (解决死角 1、死角 3)
- [x] **`epwm_down_count` (互补反相组合断言，解决死角 1)**:
  - 理论值：$T = (4800 + 1) / 24\text{MHz} = 200.04167\,\mu\text{s}$，$f = 4998.96\,\text{Hz}$；
  - 第一步：`ASSERT_WAVEFORM` 断言 PG0 (Pin 16) 频率 $4998.96 \pm 5\,\text{Hz}$，占空比 $50 \pm 1\%$；
  - 第二步：`ASSERT_WAVEFORM` 断言 PG1 (Pin 17) 频率与 PG0 一致；
  - **第三步（关键互斥抽样）**：使用 `ASSERT_POINT` 在半周期窗口关键点采样：
    - $t = 10050\mu\text{s}$（PG0 有效区间）：断言 `target: "gpio:16", matcher: 1` 且 `target: "gpio:17", matcher: 0`；
    - $t = 10150\mu\text{s}$（PG0 无效区间）：断言 `target: "gpio:16", matcher: 0` 且 `target: "gpio:17", matcher: 1`；
    - 若模型存在同相输出 Bug，此处必定立即红灯！
- [x] **`epwm_updown_count` (脉冲居中特征采样，解决死角 3)**:
  - 理论值：$T = 2 \times 4800 / 24\text{MHz} = 400\,\mu\text{s}$，$f = 2500.00\,\text{Hz}$；
  - 第一步：`ASSERT_WAVEFORM` 断言 PG0 (Pin 16) 频率 $2500 \pm 2\,\text{Hz}$，占空比 $50 \pm 1\%$；
  - **第二步（下计数伪装拦截点）**：中心对称在 50% 占空比下脉冲居中（$t \in [100\mu\text{s}, 300\mu\text{s}]$ 为高），在 $t = 50\mu\text{s}$（$T/8$）处使用 `ASSERT_POINT` 断言 PG0 必须为 0（低电平）；若走下计数（$t \in [0, 200\mu\text{s}]$ 为高），在 $50\mu\text{s}$ 采样为 1 将被立即拦截！
  - 第三步：Host C++ 单测注入 `Duty = 25%`，断言脉冲中心严格居中于 $PERIOD$ 顶点。

### S3 — 故障刹车全生命周期、长窗口稳态与中断解耦 (解决死角 4)
- [x] **`epwm_brake_fb` (Recover 模式双循环 + 长窗口稳态)**:
  - 阶段 1（0~20ms）：`ASSERT_WAVEFORM` 断言 PG0~PG3 正常输出波形；
  - 阶段 2（20ms）：注入 P0.6 (Pin 6 / FB0) 拉高；
  - 阶段 3（20~40ms）：使用 `ASSERT_WAVEFORM` 的 `kind: "stableLevel"` + `level: "HIGH"` + `holdForUs: "20ms"`（100 个周期），断言 PG0/1 持续冻结为 1、PG2/3 持续冻结为 0，杜绝瞬态偶合假绿；
  - 阶段 4（40ms）：撤销 P0.6，断言在下一个 Reload 点输出波形自动恢复；
  - 阶段 5（60ms）：再次注入 P0.6，验证可重复触发闭环。
- [x] **`epwm_brake_stop` (Stop 模式硬件清零与手动重启)**:
  - 软件刹车触发：断言 `PWMCNTE` 硬件清零，断言 P3.3 翻转与 PG 输出冻结；
  - 软件撤销刹车：断言由于 `PWMCNTE` 已被清零，输出依然维持冻结保护；
  - 执行 `PWMBRKC[3]=1` 清除标志并重新置位 `PWMCNTE`：断言计数器重新启动并恢复波形。
- [x] **硬件与中断解耦验证**:
  - 编写测试用例：在全局中断关闭（`EA = 0`）或禁止刹车中断时，注入刹车信号，断言 PG 引脚依然立即冻结为安全电平。

### S4 — 变异防御审计脚本、单测重构与全量门禁 (解决死角 5)
- [x] **改写纠正既有 C++ 单测 (`test_mcs51_cms8s_epwm.cpp`)**:
  - 纠正 line 213 将 Suspend 误测为即时恢复的逻辑；
  - 废弃直接篡改 `sfr_shadow` 模拟 FB0 的做法，改为驱动外部 pin；
  - **增加逐 tick 直通保护断言**：在互补运行全周期循环断言 `assert(!(pg0 == 1 && pg1 == 1))`；
  - 补充 Stop 模式清零 `PWMCNTE` 与手动重启测试。
- [x] **构建专用变异验证脚本 (`frameworks/mcs51/tools/verify_epwm_anti_false_green.py`, 解决死角 5)**:
  - 独立运行，调度 5 大模型变异：
    - `MUT-PG-DISABLE`（屏蔽驱动输出）
    - `MUT-COMP-INVERT`（强制 PG0/PG1 同相）
    - `MUT-FB-IGNORE`（忽略 FB0 刹车引脚）
    - `MUT-CLOCK-FIXED`（硬编码 24MHz）
    - `MUT-STOP-NO-CNTE`（Stop 模式不清零 CNTE）
  - **拦截原因强校验**：不仅检查 `exit_code != 0`，必须正则匹配控制台错误日志命中预期的断言规则（如 `calculated frequency ... did not match ...`），严格排除 Crash、SIGSEGV、Wasm 内存越界或超时。
- [x] **拆分执行全量质量门禁**:
  - Host C++ CTest 单测门禁；
  - Python 治理工具套件回归；
  - 4 个标杆 App Wasm 物理波形场景门禁；
  - 37 个 App 的 SDCC 硬件编译门禁；
  - Upstream 148 文件 0 篡改审计；
  - 分层架构门禁（`winkcli lint --pack layering --pack api`）；
  - 开源许可地图门禁（`check_license_map.py`）。
- [x] 编写并归档审查报告：`docs/reviews/mcs51/2026-10-10-cms8s78xx-anti-false-green-phase2-epwm-review.md`。
- [ ] 执行原子 Git Commit。

---

## 3. 防假绿双层约束对照与验收标准

| 假绿死角编号与描述 | 约束层级 1：`frameworks/` 落地位置与代码 | 约束层级 2：`unisim-scenarios/` 落地位置与断言 | 验收拦截标准与判定 |
|---|---|---|---|
| **死角 1：跨引脚互补盲区**<br>PG0/PG1 同相直通仍能全绿 | `test_mcs51_cms8s_epwm.cpp`：循环遍历各周期断言 `assert(!(pg0==1 && pg1==1))` | `epwm_down_count.scenario.json`：同周期内两处 `ASSERT_POINT` 采样，分别断言 `PG0=1 ∧ PG1=0` 与 `PG0=0 ∧ PG1=1` | 若注入同相变异（MUT-COMP-INVERT），`ASSERT_POINT` 必须以退出码 1 拦截 |
| **死角 2：二级分频旁路陷阱**<br>PWMnDIV=0xFF 直通主频，盲目乘 PSC 变半速 | `cms8s_epwm.cpp`：实现 MUX，`div_reg == 0xFF` 时直通 `fsys`，不除以 `(psc + 1)` | `epwm_down_count.scenario.json`：`ASSERT_WAVEFORM` 严卡 $4998.96 \pm 5\,\text{Hz}$ | 若模型出现乘积错误输出 2500Hz，频率断言必须以退出码 1 拦截 |
| **死角 3：50% 占空比掩盖对称**<br>下计数与中心对称宏观参数相同 | `test_mcs51_cms8s_epwm.cpp`：注入 Duty=25%，验证高电平中心精确对齐 $PERIOD$ 顶点 | `epwm_updown_count.scenario.json`：在 $t=T/8$（50µs）处用 `ASSERT_POINT` 断言 PG0 必须为 0（下计数此时为 1） | 若模型走下计数，50µs 处采样为 1 将被 `ASSERT_POINT` 立即拦截 |
| **死角 4：刹车瞬态单点假冻结**<br>偶合命中高电平伪装成锁死 | `cms8s_epwm.cpp`：Stop 硬件清零 `PWMCNTE`；单测断言 CPU 关中断时硬件刹车依然立即锁死 | `epwm_brake_fb.scenario.json`：使用 `stableLevel` + `holdForUs: "20ms"`（100 个周期无翻转） | 若信号仍在震荡，`maxContinuousHold` 仅 100µs (< 20ms)，稳态断言必红 |
| **死角 5：变异拦截假退出**<br>Crash/超时冒充断言防御成功 | `frameworks/mcs51/tools/verify_epwm_anti_false_green.py`：脚本捕获并正则核验错误日志中的具体 Assertion | 变异场景规范配置具体参数偏差，形成目标失败签名 | 必须输出 `AssertionFailed` 对应字段，排除任何 SIGSEGV 或超时挂起 |
| **死角 6：基线扫描统计污染**<br>glob 把 fail 用例计入正常池迫使放水 | `run_cms8s78xx_baseline.py`：显式排除 `*.fail.scenario.json`；变异用例统一归入 `mutations/` 子目录 | 变异用例由专用脚本独立调度，不进入常规 38-scenario 正常报告池 | 常规基线 38/38 PASS 与变异套件 5/5 RED 互不交叉干扰 |

---

## 4. 功能支持边界声明 (Phase 2 Boundary)

1. **死区功能 (Dead Time)**：4 个标杆 App 均配置 `PWMDTE = 0` 关闭死区。Phase 2 明确互补反相逻辑闭环，但**不宣称死区延迟已闭环**。
2. **非对称模式 (ASYMEN)**：保留寄存器映射，Phase 2 重点闭环标杆用到的对称计数模式。
3. **原厂代码零篡改**：严禁修改任何标杆 App 源码目录下的 Keil C51 代码。
