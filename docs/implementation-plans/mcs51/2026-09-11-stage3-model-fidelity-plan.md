# 阶段 3 模型保真（A 类主体）实施计划

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-STAGE3-MODEL-FIDELITY` |
| **创建日期** | `2026-09-11` |
| **目标平台/SoC** | `host` / `wasm`（mcs51 仿真；`frameworks/mcs51` 在 ESP_PLATFORM 直接 return） |
| **工具链/SDK版本** | host `GCC/MSVC C++17` / `Emscripten`；真机对照 `Keil C51`（仅文档与 checklist） |
| **计划状态** | `Task 1/2/3/4 完成，Task 5 进行中（GAP-17'/GAP-24 落地；host -R mcs51 59/61，另 2 项为预存 MinGW putchar 链接失败，已在干净 HEAD 树复现；wasm 22 场景回归待 sister repo）` |
| **优先级** | 🔴 P1 主体（A-02 + A-05 打头；A-03/A-04 跟上但有前置 ADR；A-08 粗补） |
| **计划版本** | `v1.0` |
| **关联技术设计** | 无（本计划即 Layer-③；A-03 时钟语义另立 ADR，见 Task 3） |
| **关联设计规范** | `docs/todolist/2026-09-10-mcs51-sim-vs-silicon-gap-todolist.md` §6 阶段 3、`docs/todolist/2026-09-10-mcs51-sim-backend-responsibility-classification.md` §2 A-02/A-03/A-04/A-05/A-08 |
| **关联评审记录** | GAP 清单 §8 第二轮评审（BRT/TMR2/TMR4、WDT 余量依赖、T3M /6 误判修正）、§9 第三轮自查（GAP-22 家族门控、GAP-25 模拟脚数字读） |
| **关联 ADR** | ADR-0072（双时钟域与 Trap 红线）、ADR-0076（Native/ISS 双后端，不加异步定时器）；**待立**：A-03 UART 按波特率 `charge_us` 时钟语义 ADR（GAP-02 Task 4 deferred 同源） |
| **目标里程碑** | 阶段 3 A 类闭环（GAP-05/08/02-记账/07/06/17'/24/25 对应项） |
| **前置依赖计划** | `PLAN-20260910-GAP02-UART-READY`（A-01 已完成，UART 就绪谓词与计数器模式复用）；GAP-10 固件健康计数判决已落地（新计数器一接上即自动被 runner 判决） |
| **替代/废弃** | 无 |
| **计划负责人** | （待定） |
| **所需子代理技能** | `embedded-best-practice` |

---

## 2. 背景与目标（🔴 必选）

### 2.1 问题陈述

阶段 3 是分类文档 §5 路线表中"模型保真（A 类主体）"一行，对应 GAP 清单 §6 阶段 3。判定依据（分类文档 §0 原则）：A 类 = "SFR hook / poll / microstep 里加同步记账 + 语义校验，不加异步定时器，不违反保真度 §3.1"。映射关系经确认如下：

| 阶段 3 内容 | A 类条目 | GAP 对应 | 本计划 Task |
|---|---|---|---|
| ADC 基准链（同步记账 + 基准/外电路/mux 建模） | A-02 | GAP-05 | Task 1 |
| GPIO 方向建模（方向/上下拉/驱动 + 模拟脚数字读屏蔽） | A-05 | GAP-08 + GAP-25 模拟脚子项 | Task 2 |
| UART 波特率记账 | A-03 | GAP-02 修复项 3 | Task 3（需先立 ADR） |
| WDT/TA | A-04 | GAP-07 | Task 4（依赖 Task 3 先行） |
| 同阶段其余粗粒度补齐 | A-08 | GAP-12/14/17'/24/25（SBUF/递归/lint 等） | Task 5 |
| 同阶段其余时钟声明 | A-06 剩余 | GAP-06（CONFIG/Fosc 声明接线） | Task 5 |

A-01（UART 就绪校验）已在 GAP-02 计划 Task 1~3 落地，不在本计划重复。

### 2.2 两个约束（必须遵守）

1. **GAP-05 一半是 C 类**：`ADCLDO.VSEL/LDOEN`、mux、DIV 记账归 A-02（可做）；但"上拉轨到底接 LDO OUT 还是 VDD"（Vrail 物理事实）属 C-01，仿真永不知道，只能靠 §5 checklist + HIL。实施时这两半分开验收，不把 C 的活许给 A。对应本计划：Task 1 只做"给定 Vrail 声明后的码值换算 + 缺失校验"，Vrail 真值核对走应用 DESIGN.md + 烧录 SOP + HIL（Task 5 呈现项）。
2. **A-03 有前置 ADR，且 A-04 依赖 A-03**：UART 按波特率 `charge_us` 涉及 `while(!TI)` 忙等的时钟语义，需先立 ADR（GAP-02 Task 4 已 deferred）；WDT 模型要抓住"遥测阻塞 → 复位"，前提是 Task 3 先让 22 字节遥测在虚拟时间上占 23ms 量级（当前仿真仅 ~110µs），否则 WDT 永不触发。顺序不可反：Task 3 ADR → Task 3 实现 → Task 4。

### 2.3 技术/业务目标

- Task 1（A-02）：`ADC_GO` hook 内按 DIV 同步记账后同步完成；码值 `raw = norm × 4095 × (Vrail/Vref)`，Vref 取 `ADCLDO.VSEL+LDOEN`，Vrail 由 device-tree 声明；`ADEN/LDOEN/mux` 缺失 STRICT 断言 / Release 计数；median-of-3 抗扰在保真度文档显式声明不可证伪，靠三次异值注入单测。
- Task 2（A-05）：TRIS 门控对外通知；HiZ + 上拉缺省读 1；开漏写 1 无强驱；AN mux 置位后数字读返回锁存/告警；强度携带 DR/LEDSDR；STRICT 引脚配置审计。
- Task 3（A-03）：每字节按当前波特率 `charge_us` 后置 TI；`while(!TI)` 自然消耗真实发送时间；需 ADR 确认后实施。
- Task 4（A-04）：记录 WDTCLR 虚拟时间 + WTS 档位，microstep/catch-up 检查溢出 → 仿真复位（或 STRICT 断言 + 计数）；TA 记录 0xAA 时间戳 + 两 TA 间其他 SFR 写即失效。
- Task 5（A-06 剩余 + A-08）：Fosc/CONFIG 声明接线；STOP 补 GPIO 端口中断唤醒；at89 下 XBYTE 总线占用告警；SBUF 未清 TI 再写计数；T2/3/4 公式参数化收尾等。

### 2.4 成功指标（验收出口）

| 指标 | 通过标准 | 验证方法 |
|------|----------|----------|
| 主机单元测试 | 全量 mcs51 host 测试 rc=0，含各 Task 新增用例 | `python wink-tools/wink.py test`（STRICT 目标参照 GAP-02 模式） |
| 既有场景 | 8 应用 22 场景全绿且新增计数器全 0 | sister repo headless + FW_DIAG 判决 |
| 文档 | GAP-05/08/07/06 对应验收 checkbox 可打勾；红线手册对应章节补齐 | 文档 diff |
| 架构 | 无异步定时器；所有新增时间手段均为 hook 内同步记账 | 代码评审（ADR-0072/保真度 §3.1） |

---

## 3. 变更范围与影响分析（🔴 必选）

### 3.1 文件变更清单（预估，按 Task 切片提交）

| 文件路径 | 变更类型 | 说明 |
|----------|----------|------|
| `frameworks/mcs51/src/cms8s_adc.cpp` | ✏️ 修改 | Task 1：ADCLDO/VSEL 解析、mux/DIV 记账、未就绪谓词 + 计数器 |
| `frameworks/mcs51/src/mcs51_adc.cpp` | ✏️ 修改 | Task 1：Vrail/Vref 换算落点（norm→raw 含缩放与钳位） |
| `frameworks/mcs51/src/mcs51_gpio.cpp` | ✏️ 修改 | Task 2：TRIS/UP/OD/DR hook + 通知门控 + AN 数字读屏蔽 |
| `frameworks/mcs51/src/mcs51_uart.cpp` | ✏️ 修改 | Task 3：波特率 charge（ADR 后） |
| `frameworks/mcs51/src/cms8s_sys.cpp` | ✏️ 修改 | Task 4：WDT 粗模型 + TA 窗口收窄 |
| `frameworks/mcs51/src/mcs51_pcon.cpp` | ✏️ 修改 | Task 5：STOP GPIO 唤醒路径 |
| `frameworks/mcs51/src/mcs51_xdata.cpp` | ✏️ 修改 | Task 5：at89 总线占用告警（如需） |
| `test/mcs51/unit/test_cms8s_adc_*.cpp` 等 | 🆕 新增 | 各 Task 单测（STRICT 中止 + Release 计数 + 正向零触发） |
| 红线手册 / GAP 清单 / DESIGN.md checklist | ✏️ 修改 | 各 Task 文档项 |

### 3.2 接口影响分析

| 接口层 | 是否有破坏性变更 | 影响范围 | 备注 |
|--------|------------------|----------|------|
| PAL 公开 API | ❌ 否 | 无 | 仅模型内部 + 新增计数器读函数（GAP-10 模式） |
| 应用层 | ⚠️ 条件 | 配置不完整的仿真应用在 STRICT 下断言 | Release 仅告警+计数；health_pot 配置完整不受影响 |
| 构建/文档 | ✏️ 是（追加） | device-tree 新增 ADC 外电路字段（缺字段构建告警）；手册章节 | 非破坏 |

### 3.3 架构红线

1. 不得引入异步定时器（ADR-0072/保真度 §3.1）：所有时间手段为 hook 内 `charge_us` + 同步完成。
2. 未建模波特率源 / 未使能模块被选中时 STRICT 直接报错，禁止静默假装成功。
3. health_pot 既有场景零回归是合入门槛；新增计数器必须接入 GAP-10 判决（runner 侧 `firmwareDiagnostics`）。

---

## 4. 依赖与风险（🔴 必选）

### 4.1 前置依赖

| 依赖ID | 依赖内容 | 是否阻塞 | 验证状态 | 备注 |
|--------|----------|----------|----------|------|
| D-001 | GAP-02 A-01 已合入（UART 就绪谓词 + 计数器模式） | ✅ 是（模式复用） | ✅ 已完成 | Task 1/2/4 的 STRICT/计数器照此模式 |
| D-002 | A-03 ADR 先行 | ✅ 是（Task 4 被 Task 3 阻塞，Task 3 被 ADR 阻塞） | ⬜ 待立 | 不立 ADR 不做 charge |
| D-003 | Vrail 声明字段（device-tree/wink-app.json） | ✅ 是（Task 1 换算输入） | ⬜ 待做 | 缺字段构建告警，不静默假设 |

### 4.2 风险登记册

| 风险ID | 风险描述 | 概率 | 影响 | 严重度 | 缓解措施 | 触发条件 |
|--------|----------|------|------|--------|----------|----------|
| R-001 | 既有 carrier 配置恰好不完整，STRICT 反绿为红 | 🟡 中 | 🟠 中 | 4 | 先跑回归；应用真缺配置则修应用，检查过严则收紧谓词（GAP-02 R-001 同源） | 回归失败 |
| R-002 | AN 通道→引脚 mux 映射表与手册理解偏差 | 🟢 低 | 🟠 中 | 3 | 以参考手册 ADC 章 + 原厂 gpio.h 为准；不确定映射宁可保守报错 | 评审指出映射错 |
| R-003 | UART charge 值与配额/中断 folded 交互导致场景超时 | 🟡 中 | 🟠 中 | 4 | ADR 中明确 charge 落点与 ISR 上下文禁 yield；先 health_pot 遥测单场景验证 | 场景时间膨胀 |

---

## 5. 优先级路线图

Task 1 → Task 2 →（ADR）→ Task 3 → Task 4 → Task 5；Task 1/2 可并行，Task 3/4 严格串行。

| 优先级 | Task | 说明 |
|--------|------|------|
| 🔴 P1 | Task 1（A-02）、Task 2（A-05） | 打头，本轮首先执行 |
| 🟠 P1 | Task 3（A-03，含 ADR）、Task 4（A-04） | 跟上，顺序不可反 |
| ⚪ P2 | Task 5（A-06 剩余 + A-08） | 粗补收尾 |

---

## 6. 详细任务拆分与进度追踪（🔴 必选）

> Task 完成统一 DoD：代码合规 + 单测 + host 全绿 + 22 场景零诊断 + 文档同步 + 提交合入。

### Task 1：ADC 基准链 A-02 `[ 状态: ✅ 已完成（host；wasm 回归待补） ]`

| 字段 | 内容 |
|------|------|
| **负责人** | （待定） |
| **预估工时** | 8 小时 |
| **优先级** | 🔴 P1 |
| **前置依赖** | D-001、D-003 |
| **修改文件** | `cms8s_adc.cpp`、`mcs51_adc.cpp`、计数器头、新单测 |
| **接口变化** | 新增 ADC 未就绪/基准计数器读函数（GAP-10 判决消费）；device-tree 新增外电路字段 |

#### 详细步骤

- [x] **Step 1**：`do_adc_conversion` 前加纯谓词（无日志无计数）：ADEN=1；ADCLDO.LDOEN=1；VSEL→Vref（1.2/2.0/2.4/3.0）；通道→引脚 mux（ANx 要求对应 PxxCFG=0x01）；DIV（ADCON1 bits6:4）→转换耗时表。
- [x] **Step 2**：换算 `raw = norm×4095×(Vrail/Vref)` + 钳位 0..4095；Vrail 取 device-tree 声明（VREF_OUT/VDD/固定电压）；缺字段构建告警。
  - **实施偏差（2026-09-11）**：device-tree 字段接线 deferred——Vrail/Vref 状态已入 `Mcu51Context`（默认 3000/3000，零回归），`mcs51_adc_set_vrail_mv` 测试缝隙 + host 模拟量注入缝隙（`wink_mcs51_host_set_analog_norm`）先行验证换算；device-tree 声明 + 缺字段构建告警移入 Task 5。
- [x] **Step 3**：DIV 耗时 `charge_us` 后同步完成（同步记账，非异步定时器）；未就绪 STRICT 断言 / Release 计数 + warn-once（复用 uart_notready 模式）。
- [x] **Step 4**：单测：同 ratio 下 VSEL=3V/Vrail=3.3V 与 Vrail=3.0V 码值不同且手算一致；LDO 未使能 STRICT 中止；DIV 改档连采节拍变化；health_pot 等价配置零触发。
  - **落地**：`test_mcs51_adc_refchain`（6 组：VSEL 解析/2048、Vrail 缩放/2252、LDO 门控计数、MUX 门控计数、DIV_256 charge ~170µs、health_pot 零触发）；既有 `test_cms8s_adc_instant`/`cms8s_adc_e2e`/`cms8s_vendor` 补 LDO+mux 前置后全绿（门控真阳性，R-001 修 fixture）。
- [ ] **Step 5**：文档：红线手册 §4.4 补 ADC 电气前提；GAP-05 A 半验收打勾，C 半（Vrail 真值）写入应用 DESIGN.md + §5 checklist，不在本 Task 验收。（待办）

### Task 2：GPIO 方向建模 A-05 `[ 状态: ✅ 已完成（host；wasm 回归待补） ]`

| 字段 | 内容 |
|------|------|
| **负责人** | （待定） |
| **预估工时** | 8 小时 |
| **优先级** | 🔴 P1 |
| **前置依赖** | D-001 |
| **修改文件** | `mcs51_gpio.cpp`、TRIS/UP/OD/DR hook 注册、新单测 |
| **接口变化** | 新增 GPIO 配置告警计数器（如需 runner 判决） |

#### 详细步骤

- [x] **Step 1**：为 TRIS（0x9A/0xA1~0xA3）/UP/OD/DR 注册模型并参与 js 通知：TRIS=输入门控对外通知；HiZ+上拉缺省读 1；开漏写 1 无强驱；强度携带 DR/LEDSDR。
  - **实施偏差（2026-09-11）**：TRIS 极性以原厂 `gpio.h` 为准（1=OUTPUT，0=INPUT）；门控仅 CMS8S 家族（经典 51 无 TRIS，原样直通，零回归）；内部 `pin_traps.on_write` 不受门控（latch 照更新）；DR/LEDSDR 强度携带 deferred（需跨仓契约提案，不单方面改 `js_pal_gpio_write` ABI）。
- [x] **Step 2**：模拟脚数字读屏蔽（GAP-25 子项，随本 Task）：记录 AN mux 状态，数字读返回锁存/告警 + 计数。
- [x] **Step 3**：STRICT 引脚配置审计：复位后扫描被当输出用但从未配 TRIS 的引脚，输出告警清单。
  - **实施偏差**：Step 3 未单独立审计扫描——TRIS 门控计数 + analog 计数已覆盖"忘配方向/上拉"的显式信号（删 P0UP 即读回 latch 而非上拉 1，由单测 T3 演示）；独立审计扫描移入 Task 5。
- [x] **Step 4**：单测：TRIS=输入写锁存无对外通知；UP=1 的 HiZ 输入读回 1；删 health_pot P0UP 后显式告警（演示后恢复）。
  - **落地**：`test_mcs51_gpio_dir`（5 组：经典直通、TRIS 门控、HiZ+上拉、AN 屏蔽、health_pot 等价配置）；host 通知日志（`wink_mcs51_host_gpio_notify_count`）证伪外部驱动。

### Task 3：UART 波特率记账 A-03 `[ 状态: ✅ 已完成（host；wasm 回归待补） ]`

- [x] **Step 0（ADR）**：`docs/decisions/core/0081-uart-tx-per-byte-synchronous-charge.md`（**Accepted 2026-09-11**，回写 §2.8 已落）：整字节同步记账后置 TI（TI 仍在同一调用栈同步置位）；波特率=原厂 `UART_ConfigBaudRate` 求逆（四源全枚举，health_pot 9615bps 锚点）；未就绪不记账；配额/yield/ISR 交互复用 ADR-0072（预期：22B 遥测跨约 2 配额片，中途 Timer0 tick 与真机一致）；异步延迟-TI 方案明确否决（出 A 类，违反 §3.1）。
- [x] **Step 1**：实现 charge；验收：遥测帧在虚拟时间上占 23ms 量级（9600bps 22B）。
  - **落地（2026-09-11）**：`mcs51_uart.cpp on_sbuf_write` 门控后按源计费（TMR1/TMR4 8 位 `Fsys·SMOD/(128·T·n)`、TMR2 16 位 `/384`、BRT `/32·div`；模式 1 计 10 位、模式 3 计 11 位；算不出速率→BAUD 策略，STRICT 中止/Release 跳过记账）；`wink_mcs51_uart_last_baud_hz()` 可观测。
- [x] **Step 2**：单测 + health_pot 遥测回归（帧内容不变，时间膨胀符合预期）。
  - **落地**：`test_mcs51_uart_charge`（+STRICT 孪生）：T1 9615bps/1040µs、22B=22880µs、模式 3 计 1144µs、BRT/TMR2/TMR4 锚点（9615/10416/9375）、未就绪与不可算源零记账+计数、经典家族 12MHz/10416；host 45/45 全绿。**Task 4（WDT）已解锁**。

### Task 4：WDT/TA A-04 `[ 状态: ✅ 已完成（host；wasm 回归待补） ]`

- [x] **Step 1**：WDT 粗模型：记录最近 WDTCLR 虚拟时间 + WTS 档位，microstep/catch-up 检查溢出 → 仿真复位（或 STRICT 断言 + 计数器）。
  - **落地（2026-09-11）**：`cms8s_sys.cpp`：WTS→计数表按原厂 `wdt.h`（`2^17..2^22,2^24,2^26`，注意 `0x06=2^24/0x07=2^26` 非连续），`interval=counts·1e6/Fsys`（`WTS=6@24MHz=699050us`）；使能（`WDTRE 0→1`）与喂狗（`WDTCLR` 置位）均经 TA 解锁写记录 `wdt_last_feed_us`；`cms8s_sys_poll` 接入外设表（每 microstep 检查）+ `wink_mcs51_wdt_check()` 显式缝隙；STRICT 首溢中止 / Release 每轮计数一次（`wdt_overflow_latched` 防轮询刷屏）+ warn-once，`wink_mcs51_wdt_overflow_total()` 以 `KEEPALIVE` 导出供 GAP-10 判决（runner 接线待 sister repo）。整机复位（context reset + main 重入）明确 deferred：验收的安全属性是"最长阻塞 < WDT 间隔"，计数器已可判决。
- [x] **Step 2**：TA 收窄：记录 0xAA 时间戳，超窗口归零；两 TA 间出现特定 SFR 写即失效；受保护写被回滚。
  - **落地**：`ta_aa_us` + 粗窗口 `100us`（背靠背 `AA/55/保护写` 约 2 microstep ≈ 10us 通过，`delay_ms` 级 sloppy 必 fail，文档化为 tripwire 非周期模型）；`cms8s_sys_notify_sfr_write()` 在 bridge 分发前清半开窗口（`CLKDIV/WDCON/TA` 自身除外；经典家族无 TA 直接 no-op）；既有"锁定写回滚"保留。
- [x] **Step 3**：单测：喂狗超 WTS 间隔被复位/断言；health_pot（10ms 喂狗、最长阻塞遥测）不触发；TA 间插无关 SFR 写回滚；DESIGN.md 写入"最长阻塞段（含帧长/波特率）< WTS 间隔"硬约束。
  - **落地**：`test_mcs51_wdt_ta`（+STRICT 孪生）：WTS 间隔三锚点（`5461/699050/2796202us`）、禁能零计数、溢出计数+锁存+喂狗重臂、`health_pot` 节奏（`WTS=6`，`23ms 帧 + 10ms 喂狗 ×3`）零触发+ deadline 验证、锁定写回滚、AA/55 间插 `ACC` 写回滚、 stale-AA（`1000us`）回滚、干净序列对照、经典家族直通；`health_pot/docs/DESIGN.md §8` WDT 行已改硬约束。

### Task 5：A-06 剩余 + A-08 粗补 `[ 状态: 🔧 进行中（GAP-17'/24/25 完成；GAP-06 + deferred 收尾未开始） ]`

- [x] STOP 唤醒补 GPIO 端口中断路径（GAP-17'，2026-09-11）：`poll_port_ints` 边沿命中分支在 `EA+PD` 门控下每边沿 post 一次 wake（与 `mcs51_raise_irq` 的 INT0/1 PD 条款对称；`WUT/LSE/SWE/LVD` 无模型，红线 §4.7 标不支持）；`test_mcs51_low_power` Test 3（命中置位+唤醒+无重发+`EA=0` 门控，反向验证：去修复即红）；host 零回归。
- [ ] Fosc/CONFIG 声明接线（GAP-06）：wink-app.json `optionBytes`/`foscHz` 注入模型 + 缺字段 INFO；烧录 checklist 含 CONFIG 校验。
- [x] at89 总线占用 + IAP（GAP-24，2026-09-11）：无片内 XRAM 家族（描述符 `xram_size==0`）的合法 MOVX 读写标记总线使用，`P0/P2/P3.6-7` 的 GPIO 写入双向查重（STRICT 中止 / Release 饱和计数 + warn-once，`wink_mcs51_classic_bus_conflict_total()` 以 `KEEPALIVE` 导出待 runner 判决；`P1/P3.0-5` 豁免，CMS8S 零回归）；CMS8S IAP 块 `0xF9~0xFF` 读写双 hook 报 `MCS51_FEAT_IAP_FLASH`（=11，枚举末尾追加；经典家族无 hook 纯影子）；`test_mcs51_classic_bus` 双构建通过（含验收例 `XBYTE[0x1234]` 后 `P3.7` 用作 GPIO）；红线 §4.7 第 5 条 + §4.8；host 零回归（59/61，2 预存失败不变）。
- [x] SBUF 复写 + 递归/栈（GAP-25，2026-09-11）：`on_sbuf_write` 入口查 `TI` 仍置位则 verdict（STRICT 中止 / Release 饱和计数 + warn-once，`wink_mcs51_uart_overwrite_total()` 以 `KEEPALIVE` 导出待 runner 判决，字节照发）；`tx_ready` 新增章节 I + B 断言 + STRICT case 4，`uart_charge` 22B 环按习语清 `TI`；lint 包新增 `MCS51-RECURSION`（error，直接自调用 + 调用环逐环去重上报，stub 验证 5/5 + `health_pot.c` 零误报）；`sdcc_gate` 新增 `--stack-min`（默认 32B，`health_pot` 实测 184B 通过、200B 反向红灯）；红线 §4.6 记账/复写段 + §5.4 递归禁令与库重入清单；host 零回归。

---

## 7. 测试策略与验收标准（🔴 必选）

### L0 编译门禁

- host 全绿；wasm 8 应用资产重建成功。

### L1 单元测试

- Task 1：VSEL/Vrail 换算手算一致；LDO 未使能 STRICT；DIV 记账；health_pot 零触发。
- Task 2：TRIS 门控；UP 缺省读 1；AN 数字读屏蔽；P0UP 删除演示告警。
- Task 3/4：虚拟时间量级；WDT 溢出复位/断言；TA 回滚。

### L2 集成测试

| 测试场景 | 验收标准 | 测试环境 | 测量方法 |
|----------|----------|----------|----------|
| health_pot 全场景 | 15/15 PASS，新增计数全 0 | headless 22 场景 | FW_DIAG 判决 |
| 经典 carrier | 各 1/1 PASS，无家族污染 | headless | 同上 |

### L3 文档验收

- 红线手册 §4.4（ADC）、GPIO 章节、UART/WDT 前提；GAP 清单对应 checkbox；DESIGN.md 硬约束（Vrail、CONFIG、最长阻塞<WTS）。

### L4 架构评审

- 无异步定时器；C 半不入 A 验收；ADR（Task 3）Accepted 后回写规范。

---

## 8. 回滚与降级方案（🔴 必选）

### 方案 1：按 Task 回退

- 触发条件：某 Task 合入后场景误报/时间膨胀。
- 操作：`git revert [该 Task commit]`；各 Task 文件正交（adc/gpio/uart/sys/pcon），可独立回退。

### 方案 2：STRICT 开关降级

- Release 保持发送/执行 + 告警计数，不阻断场景；STRICT 误报先核对应用真缺配置还是谓词过严。

### 8.1 回滚验证

- [ ] 回退后 host 全绿 + 22 场景全绿且计数归零。

---

## 9. 参考资料（🔴 必选）

- GAP 清单 GAP-05/08/02/07/06/17'/24/25（含证据行号与验收标准）。
- 后端责任划分 A-02/A-03/A-04/A-05/A-08（含同步记账约束与 accept）。
- CMS8S78xx 参考手册：ADC（ADCLDO/ADCON1 DIV/ADCCHS/mux）、GPIO（TRIS/UP/OD/DR）、WDT（WTS 档位/复位）、TA 窗口、STOP 唤醒源、CONFIG/Fosc。
- 保真度文档 §3.1（异步定时器禁令）、ADR-0072/0076。
- GAP-02 计划 v1.2（STRICT/计数器/runner 判决模式复用）。

---

### 计划版本变更记录

| 版本 | 日期 | 变更内容 | 变更人 |
|------|------|----------|--------|
| v1.0 | 2026-09-11 | 初始版本（待确认）：按 A-02/A-05 打头、A-03（ADR）→A-04、A-08 收尾拆分；GAP-05 C 半与 A-03 前置依赖显式标注 | — |
| v1.1 | 2026-09-11 | Task 1/2 落地：host 43/43 全绿 + STRICT 孪生通过；记录 3 处实施偏差（device-tree 接线→Task 5、DR 强度→契约提案、审计扫描→Task 5）；wasm 22 场景回归待 sister repo | — |
| v1.2 | 2026-09-11 | ADR-0081 Accepted + 回写 Layer-① §2.8；Task 3 落地：host 45/45 全绿 + 3 路 STRICT 孪生通过；Task 4（WDT）解锁 | — |
| v1.3 | 2026-09-11 | Task 4 落地：WDT 粗模型（WTS 表/喂狗记账/poll+显式检查/每轮一次计数+KEEPALIVE 导出）+ TA 收窄（AA 时间戳 100us 窗口/bridge 干预失效）；`test_mcs51_wdt_ta` 双构建通过；`-R mcs51 57/59`（2 预存 putchar 链接失败，干净树复现）；DESIGN 硬约束落；wasm/runner 接线待办 | — |
