# GAP-03 SDCC 编译门禁实施计划

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260910-GAP03-SDCC-GATE` |
| **创建日期** | `2026-09-10` |
| **目标平台/SoC** | `host` CI（门禁本身）；被测对象为 mcs51 carrier 源码（`at89c52/cms8s78xx`） |
| **工具链/SDK版本** | `SDCC -mmcs51 --std-c89`（只编译不链接；CMS8S 器件头用本地占位，见 Task 1）；对照 `Keil C51` 为 Tier-K（条件具备时，不在本计划） |
| **计划状态** | 🟡 执行中（Task 0 工具件已落地并验证：8 应用通过编译+链接+预算门禁，负向样例正确拦截；Task 1~4 CI 接线待做） |
| **优先级** | 🔴 P0（与 GAP-02 并行；先拦 C90 方言/容量问题，否则模型修得再好烧录第一步即挂） |
| **计划版本** | `v1.0` |
| **关联技术设计** | 无，已并入本计划 |
| **关联设计规范** | `docs/todolist/2026-09-10-mcs51-sim-vs-silicon-gap-todolist.md`（GAP-03）、后端责任划分 A-07 |
| **关联评审记录** | 无 |
| **关联 ADR** | 无（纯 CI 门禁；SDCC 与 Keil 方言差异已在 `cleanup --target=sdcc` 处理） |
| **目标里程碑** | mcs51 P0 清零 |
| **前置依赖计划** | 无（与 GAP-02 计划并行，无文件冲突：本计划只动 CI/脚本，GAP-02 动模型） |
| **替代/废弃** | 无 |
| **计划负责人** | （待定） |
| **所需子代理技能** | — |

---

## 2. 背景与目标（🔴 必选）

### 2.1 问题陈述

`health_pot.c` 及全部 carrier 用户源码有史以来只被 C++17 编译过，从未被任何 8051 C 编译器编译。C++17 接受、Keil C51（C90 子集）拒绝的写法（中位声明、`for(uint8_t i;…)` 等）与 FLASH 16KB / DATA 256B / XRAM 1KB 超限，会在真机编译期才暴露，仿真无感。

### 2.2 技术/业务目标

- ✅ CI 对每个 mcs51 carrier 执行 `mcs51_cleanup.py --target=sdcc → sdcc -mmcs51（默认标准）→ 链接产出 .ihx/.mem`。
- ✅ 产物含 DATA/XDATA/CODE 用量与器件预算对比（SDCC `.mem` 解析，**必须链接才有**，且显式传器件容量）。
- ✅ 负向验证：SDCC 能拦的（未声明符号、错误 SFR/位地址、类型不匹配、`__interrupt` 改写错误）红灯可演示。
- ✅ Tier-K（Keil `C51+BL51`）明确 deferred，有机再上。

> **评审修正（2026-09-10，本机 SDCC 4.6.2 对 health_pot/uart_hello 实跑试点后）——原计划 3 处前提需改：**
> 1. **`--std-c89` 路线不成立**：厂商 StdDriver 头自身含 `//` 注释与枚举尾逗号，c89 连厂商头都不过；SDCC 默认标准接受 C99 中位声明，所以"中位声明样例被 SDCC 拦下"**不会红**。中位声明等 Keil-C90 方言只能由 cleanup lint 或 Tier-K 拦。门禁用 SDCC 默认标准。
> 2. **预算必须链接 + 显式容量**：`.mem` 只在链接时产出；SDCC 不认 CMS8S 料号，XRAM 默认报 65536，必须传 `--code-size 16384 --iram-size 256 --xram-size 1024`（at89c52 按外接 XRAM 另算）超预算才成硬错误。
> 3. **不做手写占位头**：已验证可从原厂 `cms8s78xx.h` 机械转译出 SDCC 器件头（约 40 行，试点已跑通，落地为 `mcs51_sdcc_devhdr.py`），与 §9.5 审计脚本同源防漂移。

### 2.3 成功指标（验收出口）

| 指标 | 通过标准 | 验证方法 |
|------|----------|----------|
| carrier 全过 | 5 carrier + health_pot + vendor 例程全部通过 SDCC `-std-c89`（失败项有显式 issue 编号） | CI 日志 |
| 预算报告 | 每个应用 DATA/XDATA/CODE vs 16KB/256B/1KB 对比产出 | CI 产物 |
| 负向验证 | 故意 C99 中位声明样例被拦下 | CI 红灯演示 |
| 文档 | GAP-03 验收 checkbox 打勾 | 文档 diff |

---

## 3. 变更范围与影响分析（🔴 必选）

### 3.1 文件变更清单

| 文件路径 | 变更类型 | 说明 |
|----------|----------|------|
| `.github/workflows/`（mcs51 门禁 job） | 🆕 新增 / ✏️ 修改 | 新增 SDCC 门禁 job（安装 SDCC 4.6.2 → cleanup → 编译+链接 → `.mem` 预算解析） |
| `wink-micro-os/frameworks/mcs51/tools/mcs51_sdcc_devhdr.py` | 🆕 新增 | 试点验证的原厂头转译器：`sfr`→`__sfr __at`、`sbit X=REG^n`→绝对位、XSFR 野指针宏→`__xdata __at`、去 stdint typedef、修枚举尾逗号 |
| `wink-micro-os/frameworks/mcs51/tools/sdcc_gate/`（gate 头树） | 🆕 新增 | gate 版 `wink_mcu.h`（转译器件头 + 直接复用厂商 StdDriver/inc）、`intrins.h` 适配（仅 `_nop_`，SDCC mcs51 不提供）、`absacc.h`（XBYTE 必须为 `((volatile u8 __xdata*)0)` 指针宏）、`classic_alias.h`（SBUF/SCON/TI 经典名→SBUF0 系列） |
| `wink-micro-os/frameworks/mcs51/tools/mcs51_cleanup.py` | ✏️ 修改 | **必修**：`--target=sdcc` 增加用户 `sbit X = Pn^b;` 声明改写（试点实测 health_pot 6 处全挂；当前只改 ISR 签名），SFR 名→位地址映射复用转译器；顺带修 GAP-12 delay 守卫漏判 |
| GAP 清单 GAP-03 | ✏️ 修改 | 验收 checkbox 打勾 |

### 3.2 接口影响分析

| 接口层 | 是否有破坏性变更 | 影响范围 | 备注 |
|--------|------------------|----------|------|
| 应用层 | ❌ 否 | 无 | 门禁只读源码，不改源码 |
| 构建系统 | ✏️ 是（CI 新增 job） | CI 时长增加 | 失败默认阻断（或先 warn 后阻断，评审定） |
| 模型/仿真 | ❌ 否 | 无 | 与 GAP-02 无文件冲突，可并行 |

### 3.3 架构红线

1. 门禁测的是**用户原文件经 cleanup 的 SDCC 改写产物**，报告必须同时标注 C51 编译状态（呼应 GAP-20 溯源要求方向，但 manifest 本体不在本计划）。
2. SDCC 通过 ≠ Keil 通过：报告措辞不得暗示 Keil 背书；Tier-K 另行立项。

### 3.4 系统资源与并发约束评估

CI 时长增加（SDCC 安装 + N 个 carrier 编译），分钟级，无运行时影响。

---

## 4. 依赖与风险（🔴 必选）

### 4.1 前置依赖

| 依赖ID | 依赖内容 | 是否阻塞 | 验证状态 | 备注 |
|--------|----------|----------|----------|------|
| D-001 | CI runner 可安装 SDCC（apt/choco 或缓存） | ✅ 是 | ✅ **已解除**：本机 SDCC 4.6.2 (mingw) 试点编译 health_pot→`.rel`、uart_hello→`.ihx/.mem` 成功；Linux apt 可得，版本钉 4.6.2 | 转译器/gate 头随本计划落地 |

### 4.2 试点实测记录（2026-09-10）

| 检查项 | 结果 |
|--------|------|
| health_pot 经 cleanup+转译器+gate 头编译 | ✅ `.rel` 产出（41.8KB 目标文件） |
| uart_hello（at89c52 家族，`<8052.h>`）编译+链接 | ✅ `.ihx/.mem`，ROM=303B，248B stack |
| 暴露的真实缺口 | ① cleanup 不改写用户 sbit 声明（6 处）② gate 头树 4 件 ③ c89 标准不可用 ④ 预算需链接+显式 size 参数 ⑤ 顺带抓出 GAP-22 优先级位错误（已修，commit 7a8479e） |

### 4.3 风险登记册

| 风险ID | 风险描述 | 概率 | 影响 | 严重度 | 缓解措施 | 责任人 | 触发条件 |
|--------|----------|------|------|--------|----------|--------|----------|
| R-001 | SDCC 与 Keil 方言差异导致误报（SDCC 过/Keil 挂或反之） | 🟡 中 | 🟠 中 | 4 | 报告明确标注 Tier-S 语义；`__interrupt/__code/__at` 由 cleanup 统一处理；Keil 专属项进 Tier-K backlog；**SDCC 默认标准（非 c89），C90 方言检测划归 cleanup lint/Tier-K** | （待定） | carrier 大面积红灯且经核对为方言差 |
| R-002 | cleanup `--target=sdcc` 路径年久失修，大量 fallout | 🟡 中 | 🟢 低 | 3 | **试点已定位首个必修项（用户 sbit 声明不改写），随 Task 0 修复**；其余 fallout 修 cleanup 本体；修不好则对应 carrier 挂显式 issue 豁免，不阻断全部门禁 | （待定） | 首轮跑挂超 3 个应用 |
| R-003 | CMS8S 器件头无 SDCC 版本 | 🟢 已解除 | 🟢 低 | — | **试点验证：原厂头转译器（mcs51_sdcc_devhdr.py）+ 复用厂商 StdDriver/inc 可行**，不再手写占位 | （待定） | — |
| R-004 | 多 TU 例程（main.c/isr.c/demo.c）门禁脚本需目录级批量编译 | 🟢 低 | 🟢 低 | 2 | gate 脚本按应用目录收集全部 TU，统一 cleanup+编译+链接 | （待定） | vendor 例程接入时 |
| R-005 | 链接期未解析符号（如 `__nop`、未来 printf 库） | 🟡 中 | 🟢 低 | 3 | gate 提供最小 stub（intrins 已适配）；未解析项进 warning 而非硬挂，纯用户符号未解析仍硬失败 | （待定） | 链接输出 Undefined Global |

---

## 5. 优先级路线图

Task 0（cleanup sbit 改写 + 转译器，试点已验证可行）→ Task 1（gate 头树 + 单应用试点）→ Task 2（全量 + CI 接线）→ Task 3（fallout 清零/豁免 + 预算报告）→ Task 4（文档）。

| 优先级 | Task 数量 | 说明 |
|--------|-----------|------|
| 🔴 P0 | 5 | Task 0~4 全为门禁落地必需 |

---

## 6. 详细任务拆分与进度追踪（🔴 必选）

### Task 0：cleanup sbit 改写 + 器件头转译器 `[ 状态: ⏳ 待开始 ]`

试点已验证方案可行，正式落地两个工具件（无 CI 依赖，可先合入）：

| 字段 | 内容 |
|------|------|
| **优先级** | 🔴 P0 |
| **修改文件** | `mcs51_cleanup.py`（sdcc 目标新增用户 sbit 声明改写）、新增 `mcs51_sdcc_devhdr.py`（原厂头→SDCC 头转译器）、新增 `tools/sdcc_gate/`（gate 头树） |

- [x] **Step 1**：`--target=sdcc` 改写用户源码中的 `sbit NAME = REG^b;` → `__sbit __at(0x基址+b) NAME;`，基址表（P0=0x80…PSW=0xD0 等可位寻址 SFR）从转译器同源数据获取；当前只改了 ISR 函数签名，声明未处理（health_pot 6 处全挂）。**已落地**：cleanup 新增 sbit 相对/绝对声明改写 + `sbit_unresolved` 计数，自测 2 例。
- [x] **Step 2**：转译器正式化（试点脚本约 40 行已跑通）：98/101 SFR + 相对形式 sbit + 204 XSFR 宏 + typedef 处理 + 枚举尾逗号；输入原厂 cms8s78xx.h，输出 gate include 树可用的 SDCC 器件头。**已落地**：`mcs51_sdcc_devhdr.py`（sfr 101，sbit 无未解析）。
- [x] **Step 3**：gate 头树入库：`wink_mcu.h`（转译器件头 + 厂商 StdDriver/inc）、`intrins.h`（`_nop_` asm 实现 + `_crol_/_cror_/_testbit_` 静态版）、`absacc.h`（指针式 XBYTE/XWORD 宏）、`classic_alias.h`（SBUF/TI 等经典名→SBUF0/TI0）。**已落地**于 `tools/sdcc_gate/`。
- [x] **Step 4**：本地验证 health_pot 出 `.rel`、uart_hello 出 `.ihx/.mem`（试点已达成，正式脚本回归）。**已超额**：`mcs51_sdcc_gate.py` 完成编译+链接+预算全链路，**8 应用全过**（6 官方 carrier + 2 厂商多 TU 例程；health_pot CODE=10923B/16KB、栈余 184B；含 GB18030 厂商源码转码、应用本地头路径、厂商 StdDriver 全套自动链接）；负向样例（未定义符号）正确红灯。**注意**：CMS8S 的 CODE 数含整套 StdDriver（链接器未按调用裁剪），偏保守，最终值以 Tier-K 为准。

### Task 1：SDCC 全量试点（默认标准 + 容量参数） `[ 状态: ⏳ 待开始 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | （待定） |
| **预估 / 实际工时** | 3 小时 / — |
| **优先级** | 🔴 P0 |
| **前置依赖** | 无 |
| **修改文件** | 本地验证脚本（一次性，可不合入） |
| **接口变化** | 无 |

#### 详细步骤

- [ ] **Step 1**：SDCC 版本钉 4.6.2（试点版本）；CI 镜像安装方案确认。
- [ ] **Step 2**：对 `health_pot.c` 跑 Task 0 工具链（cleanup → 转译器件头 → gate 头树 → `sdcc -mmcs51` 默认标准编译+链接），CMS8S 加 `--code-size 16384 --iram-size 256 --xram-size 1024`。
- [ ] **Step 3**：负向样例改为 SDCC 真实可拦类型（未声明符号/错误位地址/类型不匹配）；C90 中位声明检测明确标注为 Tier-K/cleanup lint 职责。

#### 验证步骤

1. 本地试点通过（或失败归因清单产出）；2. SDCC 版本号记录入计划。

---

### Task 2：全量 carrier + CI 接线 `[ 状态: ⏳ 待开始 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | （待定） |
| **预估 / 实际工时** | 4 小时 / — |
| **优先级** | 🔴 P0 |
| **前置依赖** | Task 1 |
| **修改文件** | `.github/workflows/` job |
| **接口变化** | CI 新增 job；失败策略（阻断/warn）评审定，默认阻断 |

#### 详细步骤

- [ ] **Step 1**：carrier 全量（5 carrier + health_pot + vendor 例程）接入编译矩阵。
- [ ] **Step 2**：SDCC `.mem` 解析（**链接产物**，含 ROM/EPROM/FLASH 行与 Internal RAM 布局 + stack 余量），输出 DATA/XDATA/CODE vs 16KB/256B/1KB 对比表；容量超限依赖 `--code-size/--iram-size/--xram-size` 让链接器直接报错。
- [ ] **Step 3**：扩展向量 19/15/16 的 `__interrupt` 改写覆盖核实（GAP 清单点名项）。

---

### Task 3：fallout 清零/豁免 `[ 状态: ⏳ 待开始 ]`

- [ ] cleanup 本体缺陷修（命中 GAP-12 delay 守卫等则修）；修不好挂显式 issue 豁免，不 блоки门禁。
- [ ] 全量绿（或豁免清单 + issue 号）。

### Task 4：文档 `[ 状态: ⏳ 待开始 ]`

- [ ] GAP-03 checkbox 打勾；Tier-K backlog 留痕（Windows CI + `C51+BL51` + `.m51` 预算解析）。

---

## 7. 测试策略与验收标准（🔴 必选）

### L0 编译门禁

- [ ] CI 新 job 在 PR 上运行，绿灯为合入条件（或 warn 过渡，评审定）。

### L1 负向验证

- [ ] SDCC 可拦类型（未声明符号/错误 SFR 位地址/类型不匹配）红灯；正常 carrier 绿灯。
- [ ] C90 中位声明检测由 cleanup lint/Tier-K 承担（报告显式说明，不计入 SDCC 负向用例）。

### L2 预算报告

- [ ] 每个应用 DATA/XDATA/CODE 用量表产出并与预算对比（链接产物 + 显式容量参数，超限链接即失败）。
- [ ] 报告固定标注：**Tier-S 通过 ≠ Keil 通过 ≠ 可烧录**；最终容量以 Tier-K `.m51/.map` 为准。

### L3 文档验收

- [ ] GAP-03 checkbox；本计划状态 ✅。

---

## 8. 回滚与降级方案（🔴 必选）

### 方案 1：降级为 warn

- 触发条件：方言误报大面积红灯且短期修不完。操作：job 设 `continue-on-error`，红灯只告警。

### 方案 2：版本回退（Git）

- `git revert [CI commit]`，恢复无门禁状态。

### 8.1 回滚验证

- [ ] 回退后 CI 主线绿。

---

## 9. 参考资料（🔴 必选）

- GAP 清单 GAP-03（含 Tier-S/Tier-K 分层与验收标准）。
- `mcs51_cleanup.py --target=sdcc` 现有代码路径；GAP-12 delay 守卫缺陷。
- SDCC 文档：`-mmcs51 --std-c89 -c`；Keil C51 C90 子集对照（Tier-K 背景）。

---

### 计划版本变更记录

| 版本 | 日期 | 变更内容 | 变更人 |
|------|------|----------|--------|
| v1.0 | 2026-09-10 | 初始版本（待评审） | — |
| v1.1 | 2026-09-10 | SDCC 4.6.2 实跑试点后修正：弃用 `--std-c89`（厂商头自身不兼容；中位声明检测划归 Tier-K）；预算需链接+`--code/iram/xram-size`；占位头改原厂头机械转译器；新增 Task 0（cleanup 用户 sbit 改写 + gate 头树，试点已验证 health_pot→.rel / uart_hello→.mem）；新增 R-004/R-005 | — |
