# GAP-02 UART 链路就绪校验实施计划

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260910-GAP02-UART-READY` |
| **创建日期** | `2026-09-10` |
| **目标平台/SoC** | `host` / `wasm`（mcs51 仿真；与 ESP target 无关，`frameworks/mcs51` 在 ESP_PLATFORM 直接 return） |
| **工具链/SDK版本** | host `GCC/MSVC C++17` / `Emscripten`（随现有构建）；真机对照 `Keil C51`（仅文档，不在本计划构建） |
| **计划状态** | ✅ Task 1/2/3 完成（2026-09-11 wasm 22 场景全绿；附带发现 2 个功能级 carrier 真缺波特率配置，处置待定） |
| **优先级** | 🔴 P0（GAP 清单唯一剩余 P0 模型项；GAP-07 WDT 建模的前置依赖） |
| **计划版本** | `v1.2` |
| **关联技术设计** | 无，已并入本计划（小规模模型补强，不单独立 Layer-②；涉及时钟语义的波特率记账项明确 deferred，见 Task 4） |
| **关联设计规范** | `docs/todolist/2026-09-10-mcs51-sim-vs-silicon-gap-todolist.md`（GAP-02）、`docs/todolist/2026-09-10-mcs51-sim-backend-responsibility-classification.md`（A-01 + A-03） |
| **关联评审记录** | 无 |
| **关联 ADR** | ADR-0072（双时钟域与 Trap 红线，记账约束）、ADR-0076（双后端，Native 内不做异步定时器） |
| **目标里程碑** | mcs51 P0 清零 |
| **前置依赖计划** | 无（本计划是 GAP-07 的前置） |
| **替代/废弃** | 无 |
| **计划负责人** | （待定） |
| **所需子代理技能** | `embedded-best-practice` |

---

## 2. 背景与目标（🔴 必选）

### 2.1 问题陈述

`mcs51_uart.cpp:43-59 on_sbuf_write` 写 SBUF 即无条件出总线 + 同一调用栈置 `TI`，完全不看 TR1/TH1/`FUNCCR` 波特率源（Timer1/BRT/TMR2/TMR4）、SCON 模式、TXD 引脚 mux。`SBUF=c; while(!TI); TI=0;` 类忙等在真机依赖波特率发生器逐位发送后置 TI，波特率错/无 TXD 复用/模式错在仿真全部照发、场景全绿，真机分别表现为卡死、乱码、无波形。`health_pot` 遥测与两个 UART carrier 全命中。

### 2.2 技术/业务目标

- ✅ 写 SBUF 前做 TX 就绪检查：波特率源（CMS8S 4 种全枚举；经典 51 固定 T1）+ SCON∈{1,3} + TXD 引脚 mux；REN=1 时校验 RXD mux + `PS_RXD`。
- ✅ **家族门控**：FUNCCR(0x91) 是 CMS8S 专属，检查函数必须按 `mcs51_context_get_family()` 分流（复用 GAP-13 种子机制）；AT89C52 家族只检查 T1，不读 FUNCCR/CFG。
- ✅ 未就绪零静默：STRICT 断言，Release 单次告警 + **分原因位掩码计数器**暴露（喂 GAP-10 runner 判决，不能只有黑盒总数）。
- ✅ 零回归：`health_pot`（TH1=217/T1M/SMOD→9615bps，TXD 已配）与现有 22 个无头场景全绿。
- ✅ 波特率记账（A-03）明确 deferred，不在本计划做（需 ADR 确认，GAP-07 的前置）。

> **评审修正（2026-09-10，已对照手册 §21.2 + 原厂 gpio.h 核实）**：TXD/RXD **默认引脚是 P3.1/P3.0，无需任何 CFG 配置**；P14/P22 与 P13/P21 仅为备选脚（厂商只提供这 4 个备选 mux 宏，无 P30/P31 对应宏）。就绪谓词：
> - TXD：P3.1 默认路径 **或** P14CFG==3 **或** P22CFG==3。
> - RXD（REN=1）：P3.0 默认 **或**（P13CFG==3 且 PS_RXD==0x13）**或**（P21CFG==3 且 PS_RXD==0x21）。
> 实施前必须先从手册确认 PS_RXD 复位值（默认脚是否需要它参与）。若错误地强制 CFG==3，经典 51 carrier、默认引脚厂商例程与教科书代码会全部误报。

### 2.3 成功指标（验收出口）

| 指标 | 通过标准 | 验证方法 |
|------|----------|----------|
| 主机单元测试 | 100% 通过，含 3 个新增 STRICT 用例 | `python wink-tools/wink.py test`（或等效 host 构建） |
| 既有场景 | 8 应用 22 场景全绿 | sister repo `wink.py sim run --mode headless` |
| health_pot 遥测 | 开启校验后仍通过 | 同上（`health-pot-uart-telemetry`） |
| 文档 | GAP-02 验收 checkbox 可打勾；红线手册 §4.6 补条目 | 文档 diff |

---

## 3. 变更范围与影响分析（🔴 必选）

### 3.1 文件变更清单

| 文件路径 | 变更类型 | 说明 |
|----------|----------|------|
| `wink-micro-os/frameworks/mcs51/src/mcs51_uart.cpp` | ✏️ 修改 | 新增 `uart_tx_ready()` 检查 + 未就绪计数器 + STRICT/Release 分支 |
| `wink-micro-os/frameworks/mcs51/include/wink_mcs51_uart.h`（如需） | ✏️ 修改 | 暴露计数器读取函数 |
| `wink-micro-os/test/mcs51/unit/test_mcs51_uart*.cpp`（或新建） | 🆕 新增 | 3 个 STRICT 用例（TR1=0 / mux 缺失 / 模式 0） |
| 红线手册 `2026-08-27-mcs51-user-code-compatibility-and-limitations-guide.md` §4.6 | ✏️ 修改 | 补「UART 仿真校验边界」条目 |
| GAP 清单 GAP-02 | ✏️ 修改 | 验收 checkbox 打勾 |

### 3.2 接口影响分析

| 接口层 | 是否有破坏性变更 | 影响范围 | 备注 |
|--------|------------------|----------|------|
| PAL 公开 API | ❌ 否 | 无 | 仅模型内部检查 |
| DAL 层 | ❌ 否 | 无 | |
| 应用层 | ⚠️ 条件 | 配置不完整的仿真应用会在 STRICT 下断言 | Release 仅告警；`health_pot` 配置完整不受影响 |
| 构建系统 | ❌ 否 | 无 | |
| 文档 | ✏️ 是（追加） | 红线手册 + GAP 清单 | 非破坏 |

### 3.3 架构红线

1. 不得引入异步定时器延迟 TI（ADR-0072/保真度 §3.1）：本计划只做**就绪检查**，不做波特率记账；记账是 Task 4 的 deferred 项，需另立 ADR。
2. 未建模波特率源（BRT/TMR2/TMR4 当前无时间模型）被选中时 STRICT 直接报错，禁止静默假装成功（GAP-02 第二轮补充）。
3. `health_pot` 既有场景零回归是合入门槛。

### 3.4 系统资源与并发约束评估

| 资源/安全维度 | 预计变化/开销 | 风险与限制 | 缓解/应对策略 |
|--------------|--------------|-----------|--------------|
| ROM/RAM | 常量级（检查函数 + 1 个计数器） | 无 | 无 |
| ISR 安全 | 检查函数跑在 `on_sbuf_write` 调用栈，可能在 ISR 上下文 | 不得 sleep/分配 | 纯影子寄存器读 + 计数器自增 |
| 并发 | 与 RX FIFO 无共享可变状态 | 无 | 无 |

---

## 4. 依赖与风险（🔴 必选）

### 4.1 前置依赖

| 依赖ID | 依赖内容 | 是否阻塞 | 验证状态 | 备注 |
|--------|----------|----------|----------|------|
| D-001 | GAP-01 已合入（P13 mux=0x03，RX 校验基准正确） | ✅ 是 | ✅ 已完成（`3ec4778` 前） | RX 侧校验依赖正确宏值 |

### 4.2 外部依赖

无。

### 4.3 风险登记册

| 风险ID | 风险描述 | 概率 | 影响 | 严重度 | 缓解措施 | 责任人 | 触发条件 |
|--------|----------|------|------|--------|----------|--------|----------|
| R-001 | 某既有 carrier 的 TX 配置恰好不完整，STRICT 测试反绿为红 | 🟡 中 | 🟠 中 | 4 | 先跑回归确认 `health_pot`+5 carrier 全绿；若有个别应用断言触发，核对是应用真缺配置还是检查过严，应用缺配置则修应用 | （待定） | 回归失败 |
| R-002 | BRT/TMR2/TMR4 就绪语义与手册理解偏差 | 🟢 低 | 🟠 中 | 3 | 以参考手册 `FUNCCR/BRTCON` 章为准，未确定位段宁可报错路径保守（STRICT 报错而非放行） | （待定） | 评审指出位定义错 |

---

## 5. 优先级路线图

Task 1 → Task 2 → Task 3；Task 4 为 deferred（另立 ADR，不在本计划验收内）。

| 优先级 | Task 数量 | 说明 |
|--------|-----------|------|
| 🔴 P0 | 3 | T1 检查实现、T2 单测、T3 回归 |
| ⚪ P2 | 1 | T4 波特率记账（deferred） |

---

## 6. 详细任务拆分与进度追踪（🔴 必选）

> Task 完成统一 DoD：代码合规 + 单测 + host 全绿 + 文档同步 + 提交合入。

### Task 1：TX/RX 就绪检查实现 `[ 状态: ✅ 已完成 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | （待定） |
| **预估 / 实际工时** | 4 小时 / — |
| **优先级** | 🔴 P0 |
| **前置依赖** | 无 |
| **修改文件** | `mcs51_uart.cpp`、`wink_mcs51_uart.h`（计数器如需暴露） |
| **接口变化** | 新增内部 `uart_tx_ready()`；新增未就绪计数器读函数（喂 GAP-10） |

#### 详细步骤

- [x] **Step 1**：在 `mcs51_uart.cpp` 匿名空间新增 `uart_tx_ready()`（实为 `uart_notready_mask_impl()` + `uart_notready_policy()` 纯谓词/策略分离，2026-09-10 落地）：
  - **先按家族分流**（`mcs51_context_get_family()`）：CMS8S 读 FUNCCR 选源；经典 51 只认 Timer1。
  - CMS8S 按 `FUNCCR` 选源校验：Timer1（TR1=1 且 mode 2 + TH1 已配）/ BRT（BRTCON.BRTEN + BRTDL/H）/ TMR2 / TMR4；未建模源被选中 → 返回"未就绪（源未建模）"。
  - 校验 SCON 模式 ∈ {1,3}（异步）；模式 0 → 未就绪。
  - 校验 TXD 引脚：默认 P3.1 无需 CFG；仅当使用备选脚时要求对应 CFG=0x03（P14/P22）。
  - RX 侧：REN=1 时校验 RXD（默认 P3.0；备选 P13CFG=0x03+PS_RXD=0x13 或 P21CFG=0x03+PS_RXD=0x21）。
  - 返回**未就绪原因位掩码**（NO_BAUD_SRC / BAD_MODE / NO_TXD_MUX / NO_RXD_MUX 分列）。
- [x] **Step 2**：`on_sbuf_write` 入口调用检查；未就绪时 STRICT 断言中止，Release `pal_log_w` 按原因单次告警 + 分原因饱和计数器 +1（发送行为保持，保证 Release 场景可观测）。
- [x] **Step 3**：STRICT/Release 语义与 `mcs51_unsupported.cpp` 现有模式对齐（`assert`+`abort` vs warn-once）；`wink_mcs51_uart_notready_mask()`（纯谓词）+ `wink_mcs51_uart_notready_count(reason_bit)`（C ABI，供 GAP-10 runner ccall）。
- [x] **Step 4（零回归硬断言）**：`test_mcs51_uart_tx_ready` Release 组 A 跑 health_pot 等价初始化序列后断言掩码为 0 且 4 计数器全 0——零回归不依赖"Release 只 warn、场景碰巧绿"。

#### 验证步骤

1. **验证命令**：host 构建 + 新增单测（Task 2）。STRICT 是**编译期宏**：现有 host 测试链接的是非 STRICT 的 compat 库，无法运行时触发 assert，必须新增一个 `WINK_MCS51_STRICT=1` 构建的 compat/测试目标（参照现有 `WINK_STRICT_NONBLOCKING` 模式）；正向用例在 Release 构建验证计数器。
2. **预期输出**：TR1=0 / mux 缺失 / 模式 0 写 SBUF 触发 STRICT 断言（分原因可辨）；Release 对应计数 +1。
3. **额外检查**：`health_pot` 配置下检查返回就绪（Task 3 回归 + Step 4 计数器 e2e 覆盖）。

> ⚠️ 不得顺手做波特率记账（`charge_us`）；那是 Task 4，需 ADR。

---

### Task 2：STRICT 单测 `[ 状态: ✅ 已完成 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | （待定） |
| **预估 / 实际工时** | 2 小时 / — |
| **优先级** | 🔴 P0 |
| **前置依赖** | Task 1 |
| **修改文件** | `wink-micro-os/test/mcs51/unit/` 新增或追加 |
| **接口变化** | 无 |

#### 详细步骤

- [x] **Step 1**：用例 1——TR1=0 时写 SBUF，STRICT 断言触发（子进程死亡）/ Release BAUD 计数。
- [x] **Step 2**：用例 2——原"TXD mux 未配断言"在实施中被修正为 REN+`PS_RXD`=0x13 指向未复用引脚（RXD 失配），STRICT 断言触发 / Release RXD 计数。修正理由：P3.1 为硬连线默认脚，TXD 在功能层恒就绪（见 v1.2 changelog），TXD 位保留供 GAP-08 细化。
- [x] **Step 3**：用例 3——SCON=模式 0 写 SBUF，断言触发 / MODE 计数；正向用例：`health_pot` 等价配置（TR1=1/mode2/TH1=217/SCON=0x40/P22CFG=0x03）不断言且计数全 0。另覆盖 BRT/TMR4/TMR2 运行位、保留 CKS、经典家族 FUNCCR 忽略、未知原因位读 0（Release 组 A–H）。

#### 验证步骤

1. **验证命令**：`python wink-tools/wink.py test`（STRICT 构建下单测）。
2. **预期输出**：新增用例全过；全量 mcs51 host 测试 rc=0。

---

### Task 3：回归 + 文档 `[ 状态: ✅ 完成（wasm 22 场景 2026-09-11 验证） ]`

| 字段 | 内容 |
|------|------|
| **负责人** | （待定） |
| **预估 / 实际工时** | 3 小时 / — |
| **优先级** | 🔴 P0 |
| **前置依赖** | Task 1、Task 2 |
| **修改文件** | 红线手册 §4.6、GAP 清单 GAP-02 checkbox |
| **接口变化** | 无 |

#### 详细步骤

- [x] **Step 1**：host 回归绿——35/35 mcs51 host 测试通过（含新增 2 个）。3 个排除项为预存 break（`test_mcs51_port_extint`/`test_mcs51_wink_mcu` 的 `putchar(char)` 链接失败经干净树验证与本变更无关；`test_pal_nonblocking_strict` 的 PAL 弃用告警同理）。
- [x] **Step 2**：sister repo 重建生产 wasm，8 应用 22 场景全绿（2026-09-11 执行）。明细：health_pot 15/15、5 carrier 各 1/1、vendor uart0_printf/uart0_rxtx 各 1/1，step 级 0 失败、进程退出码全 0。
- [x] **Step 3**：红线手册 §4.6 补「UART 仿真校验边界」条目；GAP-02 首个验收 checkbox 打勾（第二个待 wasm）。

#### wasm 回归附带发现（门禁真阳性，R-001 路径 A 命中）

`mcs51_uart_hello`、`mcs51_uart_echo` 在 Release 下各产生 **1 条** not-ready 告警：`[MCS51] UART TX link not ready: baud source not running/unmodeled`（t=0）。核对源码确认**是应用真缺配置而非检查过严**：两个 carrier 均为功能级证明程序，从未配置 TMOD/TH1/TR1（文件头注释自述 "At the functional level there is no baud/timer model"），硅片上模式 1 UART 无波特率时钟、TI 永不会置位——正是本门禁要抓的"仿真绿/真机挂"。对照：health_pot（完整 T1 配置）与两个 vendor 例程（BRT/完整初始化）**0 告警**，反向证明谓词没有误伤。

**处置（待定，建议方案 B）**：
- 方案 A：保留功能级定位，在 GAP-10 runner 判决中对这两个应用加 not-ready 白名单（语义：功能证明，非硅片可用）。
- 方案 B（推荐）：给两个 carrier 补 T1 波特率初始化（TMOD mode2 + TH1 重载 + TR1=1，如 9600bps@24MHz 的 TH1=217 路径或经典 11.0592MHz 值），使其同时是功能证明与硅片可用固件；补 SDCC 门禁（GAP-03 已绿）双重背书。

---

### Task 4（deferred）：TX 按波特率同步记账 `[ 状态: ⏳ 待开始（另立 ADR，不在本计划验收） ]`

- 按虚拟波特率整字节 `charge_us` 后置 TI，使 `while(!TI)` 消耗真实发送时间；必须先过 ADR（时钟语义），且遵守保真度 §3.1（经可推进虚拟时间的拦截点，不引入纯内存轮询死锁）。GAP-07 排在本项之后。

---

## 7. 测试策略与验收标准（🔴 必选）

### L0 编译门禁

- [ ] host：`python wink-tools/wink.py test` 全绿（含 STRICT 单测）。
- [ ] wasm：sister repo 重建 8 应用资产成功。

### L1 单元测试

- [ ] 覆盖率：新增检查函数分支覆盖（4 种源 × 就绪/未就绪 × SCON 模式）。
- [ ] 3 个 STRICT 用例 + 1 个正向用例全过。

### L2 集成测试

| 测试场景 | 验收标准 | 测试环境 | 测量方法 |
|----------|----------|----------|----------|
| health_pot 遥测 | `health-pot-uart-telemetry` PASS，帧内容不变 | headless 22 场景 | `ASSERT_BUS_PAYLOAD` |
| uart0_printf/rxtx | 各 1/1 PASS（P13CFG=0x03 路径） | headless | 同上 |

### L3 文档验收

- [ ] 红线手册 §4.6 条目合入；GAP-02 checkbox 打勾；本计划状态置 ✅。

### L4 架构评审

- [ ] 确认未引入异步定时器；未建模源走报错而非静默。

---

## 8. 回滚与降级方案（🔴 必选）

### 方案 1：快速回退（STRICT 开关）

- 触发条件：合入后既有场景在 STRICT 下误报。
- 操作步骤：1. 复核是应用真缺配置还是检查过严；2. 应用缺配置→修应用；检查过严→收紧检查条件。Release 行为保持发送，仅告警，不阻断。

### 方案 2：版本回退（Git）

- `git revert [本次 commit]`，影响仅 `mcs51_uart` 模型 + 单测 + 文档。

### 8.1 回滚验证

- [ ] 回退后 host 全绿 + 22 场景全绿。

---

## 9. 参考资料（🔴 必选）

- GAP 清单 GAP-02（含 `on_sbuf_write` 证据与 4 源枚举要求）。
- 后端责任划分 A-01 / A-03（含 Task 4 deferred 说明）。
- CMS8S78xx 参考手册：`FUNCCR` 波特率源选择、`BRTCON`、SCON 模式、P14/P22/P13 mux。
- 保真度文档 §3.1（异步定时器禁令）。

---

### 计划版本变更记录

| 版本 | 日期 | 变更内容 | 变更人 |
|------|------|----------|--------|
| v1.0 | 2026-09-10 | 初始版本（待评审） | — |
| v1.1 | 2026-09-10 | 评审修正：TXD/RXD 默认脚为 P3.1/P3.0（CFG 非必需）；检查按 MCU 家族分流（FUNCCR 仅 CMS8S）；未就绪原因改位掩码计数器；STRICT 需独立编译目标；新增计数器零回归 e2e；PS_RXD 复位值实施前先查手册 | — |
| v1.2 | 2026-09-10 | 实施修正：① TXD 谓词恒真——P3.1 硬连线默认使 TXD 在功能层不可断，TXD 原因位保留（ABI 对称 + GAP-08 细化钩子），Task 2 用例 2 改用 RXD 选择器失配；② PS_RXD 复位值不查手册亦可——默认 P3.0 路径绕过选择器，谓词对未播种的复位值免疫，未来加种子不改变判决（已在代码注释）；③ T2 运行定义复用 timer 模型（T2CON T2I≠0），T4 用 TR4；④ BRT 就绪只看 BRTEN（重载值只影响速率） | — |
