# Wink-AI 嵌入式设计文档库 · 架构评审报告

| 项 | 内容 |
|---|---|
| 评审日期 | 2026-06-22 |
| 评审范围 | `docs/design/` 全部 7 大模块、23 篇设计文档 |
| 评审基线 | `chigo-micro/docs/vendor/embedded-best-practice`（C 语言 OOP 四层架构规范）+ 同工作区既有编码约定 |
| 评审视角 | 资深嵌入式架构师 + 资深项目经理 |
| 评审方法 | 核心内核（02 DAL/PAL）亲自精读 + 其余模块并行专家 agent 深度对照规范 |
| 综合评分 | **7.4 / 10**（架构骨架优秀，地基与边界尚需夯实） |
| 关联决策 | [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md) · [ADR-0002](../../decisions/unisim/0002-dual-target-compilation.md) · [ADR-0003](../../decisions/unisim/0003-simulation-fidelity-boundary.md) |

---

## 一、总体判断

**这是一份方向正确、工程纪律强、但虚实一致性承诺偏乐观的设计。** 平台的架构赌注——"同一份 BAL/DAL 代码，既编译 wasm32 跑仿真、又链接 xtensa 跑真机，再用 Golden Trace 对比"——逻辑自洽，且 MVP 范围控制教科书级。但在三个根本点需要补强：

1. **架构范式与基线规范存在系统性偏离，但文档未声明**（PAL 放弃 ops 表、DAL 用 POD+静态分发、wink_status_t 用正数错误码）。
2. **"虚实一致"是方向性承诺而非现状**——DAL Bypass 切换了 `#ifdef`、无虚拟时钟、中断是协作式、FreeRTOS 多任务仿真缺位。
3. **安全/可信链有多处"软门禁"**——核心约束（BAL 禁调 PAL、AI 不绕过 Safe Codegen、固件防篡改）只有描述没有强制机制。

可作为 MVP-1 实施基线，但 P0 项必须前置闭环。

---

## 二、维度一：嵌入式架构与规范视角（重点）

> 以基线规范的四层架构（应用层 / 抽象层 ops 表 / 实现层 / 注册层 + Platform 层）为标尺逐条对照。

### 2.1 分层映射与"范式差异"——最关键的架构判断

本平台的 **BAL / DAL / PAL** 三层与基线四层**并非一一对应，而是范式重构**。核对 `02-pal-platform-abstraction.md:32` 原文：

> *"PAL 在真机模式下**不采用**动态虚函数表（vtable）或运行期函数指针注册的多态形式，而是通过 CMake 静态条件编译绑定……做到'零运行期封装开销'。"*

再核对 `02-wink-micro-os/01-dal-device-abstraction.md`，`dal_ultrasonic_t` 是**纯 POD 结构体**（无 `ops` 指针、无 `vptr`、无 `dal_base`），`dal_ultrasonic_read` 是**按类型静态分发的自由函数**，状态机 dispatch 用裸 `switch` 而非 ops 表查表。

| 基线四层架构机制 | Wink 平台落地 | 是否偏离 | 评价 |
|---|---|---|---|
| 应用层（只拿句柄、不知子类） | BAL 只 `#include device_tree.h`、只调 `dal_xxx` | **契合** | 应用层契约完全成立，换硬件 BAL 零修改 ✓ |
| 抽象层 ops 表多态（`me->ops->on(me)`） | **无**，DAL 用命名式扁平 API | **偏离** | 见分析 |
| `container_of` 反推子类 | **无**，DAL 无父子结构 | **偏离** | 同上 |
| 实现层填 ops 表 | 每个 DAL 器件一套独立 `.c` + 独立 API | **重构** | 用"编译期 CMake 路由"替代"运行期 ops 分发" |
| 注册层（board_init / MODULE_INIT 链接自动注册） | **device_tree.c 代码生成** | **替换** | 合理，但放弃链接期自动收集 |
| Platform 层静态直调 | PAL CMake 静态绑定 | **契合** | 与同工作区 `CLAUDE.md`"HAL 静态直调"方针一致 ✓ |

**架构师判断：**

- **PAL 静态直调合理**——基线规范自己也说 Platform 层"不在四层内、是工程实践层"，同工作区既有方针就是"静态选择链接、杜绝运行期跳转开销"。
- **DAL 放弃 ops 表/container_of 是真正的偏离点，但属于可接受的工程取舍**：MVP 范围内同一器件类型通常只有一种硬件实现，不需要运行时多态切换；命名式 API 对 AI 代码生成更友好、更可校验。**代价**：放弃了"统一 device 模型"——加新器件要加整套独立 API 而非只填一张 ops 表；失去"父类句柄统一管理"能力。
- **🟡 P1｜文档完全未声明这一范式偏离及其理由**。实现者会按基线"全新四层 ops 架构"理解，结果发现是 POD+静态分发，产生认知冲突。**已在 `01-dal-device-abstraction.md` 补《范式差异说明》。**

### 2.2 错误模型——最严重的规范符合度问题（🔴 P0）

`07-platform-governance/02-error-fault-model.md:25-39` 定义的 `wink_status_t` 采用**正数错误码**（`WINK_OK=0`，`WINK_ERR_INVALID_ARG=1` … `WINK_ERR_INTERNAL=255`），与三处权威基线全部相反：

- 同工作区 `chigo-micro/CLAUDE.md`："错误处理：返回 `int`（**0=成功，负数=错误**）"
- vendor best-practice `02-coding-standards.md` 与 Linux/POSIX 惯例（`-EINVAL=-22`）

**危害：**

1. **反向判断 bug 高发**：C 约定俗成 `if (status)` 判错，但正数方案下 `WINK_OK=0` 为假、错误为真，`if (status) { /* 误把成功分支写这里 */ }` 是 AI 生成代码极易踩的坑。第 46 行注释"便于条件判断"反而掩盖了这个风险。
2. **errno 翻译层成为 bug 源**：PAL 底层返回负 errno，DAL 要把负 errno 翻译成正数，多一层无意义符号翻转。
3. **错误码体系不完备**：`INTERNAL=255` 导致 12–254 全空洞；缺失功能安全必备码——过流、过温、watchdog、数值溢出、assert/panic。

**结论**：意图正确（统一错误语义、禁止隐式返回值），但符号约定选反。详见 [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md)（状态 Proposed）。

### 2.3 DAL/PAL 抽象质量

**亮点**：DAL 设计哲学专业——"屏蔽微秒级时序、暴露物理世界语义"，HC-SR04 把 10µs Trig + Echo 测脉宽封装成 `dal_ultrasonic_read(dev, &distance)`。双模 `#ifdef SIMULATION` 把真机/仿真分流，兼顾仿真性能。

**问题：**
- **🟡 P1｜SSOT 在 struct 字段层破功**：Device Model Registry 只声明 API 签名，但 `dal_ultrasonic_t` 字段在 codegen 示例手写，struct 定义成第二事实源。应把字段纳入 Registry。
- **🟡 P1｜物理引脚泄漏到业务句柄可见域**：`dal_ultrasonic_t` 把 `trig_pin/echo_pin` 放进 BAL 可见的结构体。建议拆分"业务句柄（语义）+ 物理配置（私有）"。
- **🟡 P1｜`app_on_fault` 重入风险**：fault 回调内调 DAL，而 fault 可能在任意 DAL 调用点触发 → 形成"fault → DAL → 再 fault"重入链，无重入深度保护。建议 fault handler 只写静态安全状态字 + trace + flag，由独立 safety task 慢路径执行 fail-safe。

### 2.4 内存 / 并发 / 中断安全

| 安全面 | 覆盖情况 | 评级 |
|---|---|---|
| 内存（禁 malloc/裸指针/递归） | App Safe Codegen 六条禁令覆盖到位 | ✅ 好 |
| 时序（禁 while(1)、loop 有限返回） | 有约束，但 DAL 阻塞调用使 loop WCET 不可静态界定 | 🟡 P1 |
| **真机并发安全** | **完全缺失** | 🔴 P0 |
| 栈深度 | 禁递归 + "大数组阈值 warning"，但**无量化上限、无静态栈分析** | 🟡 P1 |
| 整数/浮点 UB | 未禁有符号溢出/除零/截断/NaN | 🟡 P1 |

**🔴 P0 真机并发安全**：Wasm 单线程可回避，但真机 runtime 跑在 ESP32 双核 FreeRTOS 上，`app_on_fault()` 里的 `dal_motor_stop` 若与 1kHz PID `esp_timer` 回调并发，正是基线 `04-pitfalls`"陷阱6：多线程下 ops 表被修改 / 陷阱3：中断里调多态"的直接翻版。文档对 fail-safe 路径的锁、DAL 并发访问**只字未提**。

---

## 三、维度二：虚实一致性与仿真保真度（平台命门）

平台北极星是"仿真→烧录行为一致"。多个独立分析指认同一结论：**这个承诺目前偏乐观**。

| 失真来源 | 现状 | 影响 |
|---|---|---|
| **DAL Bypass 切换 `#ifdef`** | 仿真跑的不是同源 DAL 实现体 | 🔴 寄存器初始化/CRC/错误恢复路径未经仿真验证 |
| **无虚拟时钟** | 全部依赖墙钟 `setTimeout`，受浏览器 throttle 限制 | 🔴 PID 周期、去抖、超时全部时序漂移 |
| **中断是协作式** | `trigger_wasm_interrupt` 在挂起点同步插入，非抢占 | 🔴 中断延迟不可预测 |
| **FreeRTOS 多任务仿真缺位** | 单 Wasm 栈 + Asyncify 如何模拟多任务抢占未提 | 🔴 从"blink demo"到"真业务"的核心鸿沟 |
| Golden Trace 回放确定性 | 仅回放 input.sensor/fault，未含 PRNG 种子/tick/计数器 | 🟡 可复现性站不住 |
| 一致性判定无统计模型 | 逐事件 seq 对 seq，事件数不等时大面积误报 | 🟡 C4 等级不可操作 |

**判断**：能在浏览器高帧率跑通**行为级（causal）仿真**——保因果序、保逻辑序，适合 UI demo、算法逻辑验证、教学。但**不适合**验证实时时序、中断抢占、驱动协议正确性、模拟电路特性。详见 [ADR-0003](../../decisions/unisim/0003-simulation-fidelity-boundary.md)。

---

## 四、维度三：安全可信链路（S0–S4）

S0→S4 五段链路（静态检查 → Wasm 沙箱 → 隔离编译 → 授权烧录 → 真机 Fault Guard）方向完全正确。但有多处"软门禁"和断链：

| 问题 | 位置 | 评级 |
|---|---|---|
| **核心约束无强制机制**：BAL 禁调 PAL、AI 输出不绕过 Safe Codegen——只有描述 | `01-overview.md:142` | 🔴 P0 |
| **固件 manifest 无密码学签名验签**：记录 sha256 但未签名 | `07-03 §6` | 🔴 P0 |
| 沙箱 UB 缺口：未禁有符号溢出/除零/截断/NaN | `07-03 §2.1` | 🟡 P1 |
| AI tool 侧信道：`repairBal`/`generateBalDsl` 直接产出 BAL DSL/C | `04-integration §6` | 🟡 P1 |
| watchdog 终止后状态持久化未定义 | `03-journey §6.2` | 🟡 P1 |

---

## 五、维度四：可实施性与项目风险（PM 视角）

MVP 范围控制优秀：单板（ESP32）、5 外设、3 示例、6 项非目标声明，"行为级高保真"措辞克制。但有几个硬依赖未量化：

| 风险 | 说明 | 评级 |
|---|---|---|
| **wasm + xtensa 双 target 同源编译未 spike** | 平台技术命门，若不成立则虚实同源破产 | 🔴 P0 |
| **云编译时长冲击"5 分钟烧录"北极星** | ESP-IDF 冷编译动辄数分钟 | 🟡 P1 |
| **WebSerial 不支持 Safari/iOS，无降级路径** | "一键烧录"承诺在非 Chromium 内核破功 | 🔴 P0 |
| **烧录缺 read-back/MD5 verify** | flash 坏块/信号完整性导致"写成功但内容错误" | 🔴 P0 |
| 验收口径不一致："3 示例" vs "≥1 真机烧录" | `02-mvp §5` vs `§9` | 🟢 P2 |
| Registry Lock 机制反复引用但未定义 | 可复现性的隐藏前提 | 🟡 P1 |

详见 [ADR-0002](../../decisions/unisim/0002-dual-target-compilation.md)（双 target spike）。

---

## 六、维度五：文档质量与跨文档一致性

整体文档质量高（8/10）。若干一致性问题：

- 🔴 **跨文档签名冲突**：`js_sim_get_ultrasonic_distance` 在 `07-01` 声明返回 `float`，在 `02-DAL` 实现为返回 `wink_status_t` + out param——同一函数两种签名。
- 🟡 **两处 MVP 边界表不一致**（`01 §8` vs `02-mvp §3.2`）。
- 🟡 **目录路径不一致**：`03-01` 写 `app_main.c`，`03-02` 写 `src/app_main.c`。
- 🟡 **术语混用**：PAL / platform / platform_wasm 三个词混用。
- 🟡 **自相矛盾**：`04-01` 把 `js_pal_i2c_transfer` 声明为 ASYNCIFY（异步），但 `04-03` 实现为同步零拷贝。

---

## 七、关键风险清单（按优先级汇总）

### 🔴 P0 — 阻断性，MVP 前必须闭环

1. **wasm+xtensa 双 target 同源编译可行性未 spike**（平台技术命门）→ [ADR-0002](../../decisions/unisim/0002-dual-target-compilation.md)
2. **`wink_status_t` 正数错误码**违反同工作区约定/Linux 惯例，错误码体系不完备 → [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md)
3. **真机并发安全完全缺失**（ESP32 双核 FreeRTOS，fail-safe 与 PID 回调并发）
4. **核心安全约束无强制机制**（BAL 禁调 PAL、AI 不绕过 Safe Codegen）
5. **固件 manifest 无签名验签**（可信链断在最弱一环）
6. **虚实一致性承诺偏乐观**：DAL Bypass 切 `#ifdef`、无虚拟时钟、中断协作式 → [ADR-0003](../../decisions/unisim/0003-simulation-fidelity-boundary.md)
7. **WebSerial 浏览器兼容无降级**（Safari/iOS 不可用）
8. **烧录缺 read-back/MD5 verify**

### 🟡 P1 — 重要，影响安全/正确性/可信度

- `app_on_fault` 重入风险；loop WCET 不可静态界定
- Golden Trace 回放确定性未论证；一致性判定无统计模型、无时间规整
- Device Model SSOT 在 struct 字段层破功；物理引脚泄漏到业务句柄
- 沙箱 UB 缺口；栈深度无量化
- 预编译 `.a` 缓存与可复现性张力；缓存 key 歧义
- Registry Lock 机制未定义
- AI tool 侧信道绕过 Safe Codegen 风险

---

## 八、建议优先动作（Roadmap）

**Phase 0 前置（不动 MVP 范围，但必须做）：**
1. 立三个 spike：双 target 同源编译、编译时长基线、WebSerial 兼容+降级。
2. 决策并修正 `wink_status_t` 符号约定 + 补齐功能安全错误码。
3. 把两条核心安全约束落为强制机制；固件 manifest 加签名验签。
4. 在 README/01-overview 顶层声明"仿真可信度边界"。

**文档补强（低成本，高收益）：**
5. DAL 文档补《范式差异说明》（✅ 已完成回写）。
6. 补 BAL/DAL/PAL ↔ 基线四层映射表（✅ 已完成回写）。
7. 修复跨文档签名冲突、目录路径、术语混用。
8. 定义 Registry Lock 机制。

**下一阶段核心补强（支撑"虚实一致"从承诺走向可信）：**
9. 虚拟时钟 + OSAL 多任务仿真。
10. Golden Trace 确定性执行契约 + 时间规整 + 统计一致性判定。

---

## 九、分模块评分矩阵

| 模块 | 架构 | 规范符合 | 安全/保真 | 可实施/可复现 | 文档 |
|---|---|---|---|---|---|
| 01 总体设计 | 8.0 | 7.5 | 风险可控 6.5 | MVP 7.0 | 8.0 |
| 02 内核 DAL/PAL | 7.5 | **6.5** | 并发缺失 | 可移植 8.0 | 7.5 |
| 03 BAL/Codegen | 8.0 | 7.0 | 安全 8.5 | 可复现 8.0 | 8.0 |
| 04 Wasm 仿真 | 8.0 | 7.0 | **保真 6.0** | 性能 7.5 | 7.5 |
| 05+06 前端/工具链 | 8.5 | 7.5 | 安全 7.5 | 可复现 8.0 | 8.0 |
| 07 平台治理 | 7.5 | **6.0** | 安全 6.5 | 一致性 **5.5** | 8.0 |
| **加权综合** | **7.9** | **6.9** | — | **7.5** | **7.8** |

> 规范符合度（6.9）和一致性严谨性（5.5）是两个最低分，集中暴露了 wink_status_t 符号、真机并发安全、Golden Trace 数学严谨性三个短板。

---

## 十、整改跟踪

| 评审发现 | 整改方式 | 落地位置 | 状态 |
|---|---|---|---|
| 范式偏离未声明 | 补《范式差异说明》 | `02-wink-micro-os/01-dal-device-abstraction.md` | ✅ 已回写 |
| 分层映射缺失 | 补映射表 | `01-system-overall/01-system-overview.md` | ✅ 已回写 |
| wink_status_t 符号 | ADR 论证 + 决策标注 | `decisions/0001` + `07-02` 标注 | ⏳ Proposed |
| 双 target 编译 | ADR + spike 设计 | `decisions/0002` | ⏳ Proposed |
| 仿真可信度边界 | ADR + 顶层声明 | `decisions/0003` | ⏳ Proposed |
| 其余 P0/P1 | 待团队排期 | — | ⬜ 未开始 |

---

*评审人立场：本报告为 2026-06-22 时点判断快照。决策类项（wink_status_t 等）以对应 ADR 拍板结果为准，结论回写至原设计文档后，本报告中的相关表述不再代表当前事实。*

