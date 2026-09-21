# CMS8S78xx 看门狗与复位管理系统高保真实施计划

> **v2.1 修订说明（2026-09-15 架构深度一体化修订）**：在 v2.0 纠正三大事实硬伤的基础上，本版进一步闭环落地执行层的 5 处关键盲区——① 明确纤程在死循环中的主动退出/解包协议（解决方案 B 下 `while(1)` 无法交权与 PC 挂死问题）；② 打通 Host CTest 无 TS Runner 下的原生自闭环多轮重入测试 Harness，实现“内核级自闭环 + 宿主级可挂接”分层解耦；③ 建立复位「粘性标志」（PORF/WDTRF）在 `memset` 下的暂存与回填契约；④ 补齐 `wink_event_queue_deinit`、`edge_queue` 与中断嵌套堆栈（`in_service_depth`）的彻底清污时序；⑤ 结合原厂 34 号源码 `isr.c:242` 对 P3.3 在 `WDTIE+WDTRE` 双使能下的真实行为建立负例双引脚联合判决。

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260915-MCS51-RESET-FIDELITY` |
| **创建日期** | `2026-09-15` |
| **修订版本** | `v2.1`（架构深度一体化修订；v2.0 为初审修订） |
| **目标平台/SoC** | `host` / `wasm`（基于 `frameworks/mcs51` 仿真核心） |
| **工具链/SDK版本** | host `GCC/MSVC C++17` / `Emscripten`（`-sASYNCIFY=1`）/ 原厂 `CMS8S78xx_DemoCode_V2.0.2` |
| **计划状态** | `Draft / 待执行` |
| **优先级** | 🔴 P1（直接打通 Checklist 7 章节剩余 3 项复位核心用例 + 关闭 GAP-07 整机复位项） |
| **关联技术设计** | `docs/todolist/2026-09-10-mcs51-sim-vs-silicon-gap-todolist.md`（GAP-07，Draft，按文档流转规则需提升为 Layer ②） |
| **关联设计规范** | `docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md` §7（编号 33/34/35）；保真度文档 `2026-09-08-mcs51-simulation-vs-silicon-fidelity-and-test-limits.md`；红线手册 `2026-08-27-mcs51-user-code-compatibility-and-limitations-guide.md` |
| **关联 ADR** | ADR-0012（契约诚实）、ADR-0070（C++ 拦截层）、ADR-0076（Native/ISS 行为级等价）；**拟新增 ADR：`MCS51 复位语义、纤程退出与 main 重入机制`（Task 0 产出，Accepted 后回写 Layer ①）** |
| **前置依赖计划** | `PLAN-20260911-STAGE3-MODEL-FIDELITY`（已落地 WDT 粗模型/TA 收窄）；**Task 5（外部复位）额外依赖 GAP-06（CONFIG 建模，未开工）** |
| **跨仓依赖** | unisim 场景步骤 `INPUT_PIN`（已存在，level/pulse/bounce/waveform）；`SYSTEM_CONTROL/HARD_RESET` headless 语义（unisim ADR-0070 D-005b 未实现，当前为 no-op，本计划已设计内核自闭环对其解耦） |
| **目标里程碑** | 1. 消除 GAP-07 WDT 伪复位缺陷，落地芯片级 Reset 状态机与 `main()` 重入；<br>2. 官方子示例 33 (`ResetBySoftware`)、34 (`ResetByWDT`)、35 (`ResetByExtReset`) 按修正后的验收口径适配并绿灯（35 视 GAP-06 依赖可 descope）；<br>3. 建立粘性标志保护、事件队列与中断堆栈彻底清污机制；<br>4. 复位寄存器契约（`wink_mcs51_wdt.h`）、GAP 清单、Checklist 证据全部回写。 |
| **所需技能** | `embedded-best-practice` |

---

## 2. 背景与目标（🔴 必选）

### 2.1 问题陈述

在 `CMS8S78XX_EXAMPLE_CHECKLIST.md` 第 7 章节（看门狗与复位管理）中：

* 编号 32 (`WDT/code`) 已完成，但其本质只验证了看门狗作为**定时中断源**（`WDT_EnableOverflowInt` → Vector 20 翻转 P3.2）的特性；
* 编号 33 (`ResetBySoftware`)、34 (`ResetByWDT`)、35 (`ResetByExtReset`) 仍为 `[ ]` 待办。

当前仿真框架（`frameworks/mcs51/chips/cms8s78xx/src/cms8s_sys.cpp`）缺少**系统级复位控制器（Reset Controller）**，并存在以下 7 项关键架构断层：

1. **WDT 假复位**：`WDCON.WDTRE = 1` 溢出后仅递增 `s_wdt_triggered`，未触发芯片复位（`cms8s_sys.cpp:153-166`）；
2. **SWRST 软复位缺失**：未处理 `WDCON.SWRST`（bit 7）写入，软件复位指令被静默忽略；缺少 `SYS_Enable/DisableSoftwareReset` 等 shim，33 号无法编译；
3. **复位标志寄存器不闭环与粘性丢失**：`PORF`/`WDTRF` 仅有位常量，无置位/清除规则。真机上 `PORF` 具备**粘性（Sticky）**（仅 POR 置 1，仅软件写 0 可清除，热复位不改变其值）。当前 `mcs51_context_reset` 直接 `memset(ctx, 0, ...)`，导致热复位时前次未清的 `PORF` 被非法擦除；
4. **上下文无法重入与纤程死循环挂死**：
   - 官方 33 号原厂代码触发软复位后立即进入 `while(1) { ; }`（转译为 `while(1) { _nop_(); }`）。若复位仅在底层做 latch 而不主动打断纤程，纤程将继续在当前配额片（10ms）内空转；且由于 PC 挂在 `while(1)` 内部，下次调度依然无法回到 `main()` 入口；
   - v1.0 设想的「C 侧直接调用 `pal_wasm_app_init()`」不可行：`pal_wasm_app_init` → `wink_runtime_run` → `sim_scheduler_reset` 断言当前必须无运行任务（`wink_sim_scheduler.c:44-46`）且在 NDEBUG 下销毁运行中 fiber 栈；
5. **Host CTest 与 Wasm Runner 的验证断层**：
   - Wasm 场景由外部 TypeScript Runner 驱动；但 Host 原生 CTest（`test_mcs51_reset_controller.cpp`）是独立 C++ 进程，无 TS Runner。现有 `wink_runtime_run()` 每次调用均重置调度器与上下文，无法在原生 Host 单测中自闭环验证“复位→状态保持→重入 `main()`”；
6. **全局队列与中断嵌套残余污染**：
   - `wink_event_queue_init()` 具幂等防护，热复位若不显式 deinit，旧 boot 积压的未消费事件会泄漏给新 boot；若复位在 ISR 内部触发，`in_service_depth` 与优先级堆栈未清零将引发新 boot 中断锁死；
7. **验收口径与双使能事实未澄清**：
   - 34 号原厂示例在 `main.c:92-100` 每轮喂狗（约 1.3ms ≪ 174.76ms 溢出间隔），未修改源码在仿真中**绝不应复位**；
   - 34 号原厂代码在 `main.c:81` 启用了 `SYS_EnableWDTReset()`，但同时在 `demo_wdt.c:81` 启用了 `WDT_EnableOverflowInt()` 并在 `isr.c:242` 实现了 `P33 = ~P33`。双使能下的仲裁规则（是否派发 IRQ）直接决定 P3.3 的行为判定；
   - 35 号外部复位由 CONFIG 选择且 NRST 复用 P2.4/P2.5，board/device-tree 未暴露，`SET_PIN_IDEAL` 为虚构步骤。

### 2.2 技术与业务目标

1. **完善 CMS8S78xx WDCON 复位寄存器模型与粘性语义**：
   - 支持 TA 解锁保护下 `SWRST` 的 **0→1 沿触发**与自清；
   - 建立**位级写语义矩阵**与**复位源×标志矩阵**；保证 `PORF` 在热复位时的**粘性保持**；
   - 补齐缺失 StdDriver shim（`SYS_Enable/DisableSoftwareReset`、`SYS_Get/ClearPowerOnResetFlag`）。
2. **打通虚拟芯片复位、纤程退出与 `main()` 重入机制**：
   - **双层解耦设计**：Bridge 内核层实现微循环协作重入（确保 Host CTest 原生单测自闭环），Target 宿主层导出状态供 Unisim 外部 Runner 重启；
   - 在 `wink_mcs51_microstep()` 建立统一**安全拦截点**：检测到复位时立即截断当前指令流，阻断死循环配额消耗；
   - 彻底清污：复位时显式执行 `wink_event_queue_deinit()`、清空 `edge_queue`、清除中断嵌套深度（`in_service_depth = 0`）。
3. **闭环验收 Checklist 33、34、35（按修正口径）**：
   - 33：官方 carrier + 判别性场景（复位后 P3.2 继续输出第二轮脉冲串 vs 无复位时停摆）；
   - 34：官方 carrier 做**负例双引脚断言**（正常喂狗不复位、P3.2 持续闪烁且 P3.3 保持初始高电平）+ 专用测试固件做 WDT 复位正例单测；
   - 35：依赖 GAP-06/NRST 模型；未就绪则降级为单测注入并显式标注 deferred。
4. **契约与文档回写**：`wink_mcs51_wdt.h` 契约、GAP-07 状态、红线/保真度文档、Checklist 证据叙述。

### 2.3 评审问题-方案追溯表（v2.1 闭环）

| 编号 | 问题 | 解决方案 | 落点 |
|:---:|------|------|:---:|
| P01 | `main()` 重入路径不可行；纤程挂在 `while(1)` 死循环中无法交权 | 确立 `wink_mcs51_microstep()` 拦截点：检测到复位立即截断后续执行并触发退出/重入协议；Task 0 裁定重入策略 | Task 0/2 |
| P02 | 34 号「不喂狗」与源码不符；虚拟时间下永不复位 | 官方载体做负例（喂狗→不复位）；复位行为用专用测试固件单测验证 | Task 4 |
| P03 | SWRST 为 0→1 沿触发，非电平 | `on_wdcon_write` 内沿检测 + 自清；Disable 写 0 为合法前置 | Task 1 |
| P04 | 「执行器拉低归位」与硅片相反 | 复位态按硅片：P0–P3 锁存 0xFF、引脚弱上高（复用现有种子）；安全分析按高有效驱动表述 | Task 1/2 |
| P05 | 标志回填时序错误（memset 覆盖）与粘性标志丢失 | 流程改为「暂存粘性标志 → 全量重置与种子 → 按源及粘性规则回填」；`PORF` 热复位保持 | Task 0/1 |
| P06 | 位级写语义未定义；状态位可被固件置位；PORF 清 TA 粒度与原厂互斥 | 建立 WDCON 位级写语义矩阵，逐位 hook 分发；手册裁定差异项并文档化 | Task 0/1 |
| P07 | 缺失 shim（SoftwareReset/PowerOnResetFlag 族） | 按原厂 `system.c` 语义补入 `REG_CMS8S78XX.H` 第 12 节；跑 `mcs51_shim_audit.py` | Task 1 |
| P08 | `SET_PIN_IDEAL` 杜撰；NRST 无模型；CONFIG 未建模 | 场景改用 `INPUT_PIN`；新增 NRST 观察模型 + board 声明；依赖 GAP-06；未就绪则 descope | Task 5 |
| P09 | CLI 路径错误；Checklist 打勾缺证据 | 统一 SOP 命令；打勾必须附证据叙述 | Task 6 |
| P10 | 双时钟域（ctx 归零 vs PAL/场景单调） | 场景断言以 PAL 单调时钟为准；复位后 ctx 时基重基、deadline 重算；契约文档化 | Task 0/1/2 |
| P11 | WDT 双使能（RE+IE）与 34 号 P3.3 翻转冲突 | 手册与硅片裁定：复位使能时优先复位并压制 IRQ；34 号负例断言 P3.3 保持初始高电平 | Task 0/4 |
| P12 | 复位后 WDT 状态/CONFIG 强制开未定义 | ADR 定义 WDCON 全清 vs CONFIG 强制（依赖 GAP-06）；复位种子表登记 | Task 0/1/5 |
| P13 | 重入初始化链残余污染（事件队列、边沿队列、中断深度） | 重入前调用 `wink_event_queue_deinit()` 清空事件；清空 `edge_queue`；强置 `in_service_depth=0` | Task 2 |
| P14 | 用户 `.data/.bss` 跨 reboot 不重置 | ADR 三选一：① runner 级重启含实例重载；② 应用数据区快照恢复；③ 显式声明限制（ADR-0012） | Task 0/2 |
| P15 | 嵌套/重入防护缺失 | latch + guard：复位处理中忽略二次触发；ISR 内触发允许；重复到达记诊断 | Task 1 |
| P16 | Host CTest 原生单测缺失 TS Runner 导致的重入验证断层 | Bridge 内核层提供自闭环重入 Harness，使原生 CTest 无需 TS Runner 即可自闭环验证复位重入 | Task 0/2 |

---

## 3. 技术设计规范

### 3.1 WDCON 寄存器位映射与位级写语义 SSOT

位映射（依原厂 `cms8s78xx.h` 与数据手册，`REG_CMS8S78XX.H:257-268` 核对一致）：

```
WDCON (0x97, TA-protected):
┌───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┐
│ bit 7 │ bit 6 │ bit 5 │ bit 4 │ bit 3 │ bit 2 │ bit 1 │ bit 0 │
│ SWRST │ PORF  │   -   │   -   │ WDTIF │ WDTRF │ WDTRE │ WDTCLR│
└───────┴───────┴───────┴───────┴───────┴───────┴───────┴───────┘
```

位级写语义矩阵（**逐位 hook 分发；实施前以参考手册 §8.3 逐格裁定，手册沉默项按原厂 StdDriver 行为 + 显式假设记录**）：

| 位 | 读写属性 | TA | 语义 |
|---|---|---|---|
| SWRST (7) | W（沿） | 需 | 0→1 沿触发软件复位；复位后硬件自清为 0；写 0 无效（合法前置） |
| PORF (6) | R/清 | **无需** | 上电复位标志；固件写 1 不得置位；原厂 `system.c:394` 清除无需 TA，固件写 0 清除 |
| WDTIF (3) | R/W0C | 需 | 溢出且中断使能时置位；固件写 0 清除；写 1 不得置位 |
| WDTRF (2) | R/清 | 需 | WDT 复位时置位；固件写 1 不得置位；清除按原厂 `system.c:309` 需 TA 保护 |
| WDTRE (1) | R/W | 需 | 0→1 使能溢出复位，同时重臂喂狗基准 |
| WDTCLR (0) | W（脉冲） | 需 | 写 1 喂狗，硬件自清为 0 |

> **关键修正（P06）**：现模型 `on_wdcon_write` 对所有写强制要求 TA，否则全部回滚（`cms8s_sys.cpp:246-249`）。由于原厂 `SYS_ClearPowerOnResetFlag()` 无 TA，现模型会误吞 PORF 的清除！必须改为逐位判定。

### 3.2 复位源 × 标志矩阵与粘性标志保护机制

复位源及其对标志位的影响规则：

| 复位源 | 触发机制 | PORF (bit 6) | WDTRF (bit 2) | 其余位 (7,3,1,0) | 说明 |
|---|---|---|---|---|---|
| **上电冷启 (POR)** | 系统首次启动 | **强制置 1** | 清 0 | 硬件默认复位值 | 标志与种子初始化对齐 |
| **软件复位 (SWRST)** | WDCON.7 产生 0→1 沿 | **保持前态 (Sticky)** | 清 0 | 自清为 0 / 复位值 | 固件随后可通过标志区分来源 |
| **看门狗复位 (WDT)** | 计数器溢出且 WDTRE=1 | **保持前态 (Sticky)** | **置 1** | 复位值 | 标志在硬件种子应用后回填 |
| **外部复位 (EXT)** | NRST 引脚持续拉低 | **保持前态 (Sticky)** | 清 0 | 复位值 | 依赖 Task 5 NRST 模型 |

> **粘性保护时序（P05）**：
> 在 `mcs51_context_reset` 执行 `memset(ctx, 0, ...)` 清零前，必须先提取：
> `uint8_t saved_porf = ctx->sfr_shadow[0x97] & 0x40u;`
> 清零与芯片种子配置完成后，若当前复位源为热复位（SWRST / WDT / EXT），将 `saved_porf` 重新写回 `sfr_shadow[0x97]`，确保真机粘性语义不丢失。

### 3.3 复位状态机与清污流程

```
[复位触发源] POR / SWRST 沿 / WDT 溢出 / NRST（Task 5）
                    │
                    ▼
        ① Latch & Trap：记录 reason + reset_pending=1；guard 忽略二次触发
                    │
                    ▼
        ② 安全点拦截与纤程中断（解决 while(1) 挂死）：
           在 wink_mcs51_microstep() 轮询入口检查 reset_pending；
           若为真，立即截断后续代码执行，阻止纤程在死循环中继续空转耗尽配额；
           触发纤程重入/退出协议（见 §3.4）
                    │
                    ▼
        ③ 全系统残余清污（解决状态污染）：
           - 事件队列：调用 wink_event_queue_deinit() 销毁 ringbuf/sem，彻底清空积压事件
           - 边沿队列：清空 ctx->edge_queue (edge_head = edge_tail = 0)
           - 中断嵌套栈：强制 in_service_depth = 0, reti_suppress_one = false
           - 粘性标志暂存：暂存旧 WDCON 的 PORF/WDTRF 状态
                    │
                    ▼
        ④ 全量硬件重置与种子初始化：
           - mcs51_context_reset(ctx) 执行 memset + 经典/芯片外设 reset
           - 按 §3.2 矩阵回填 WDCON（结合粘性暂存与复位源，种子之后回填生效！）
           - 复用统一初始化链：32 脚弱上拉、post-init hook、PCON hook、
             catchup hook、wink_mcs51_isr_enable()、wink_event_queue_init()
                    │
                    ▼
        ⑤ 用户数据策略（ADR 决策：实例重载 / 快照恢复 / 显式声明限制）
                    │
                    ▼
        ⑥ 重新引导 main()：
           - Host CTest 原生态：通过 Bridge 内核微循环重新进入 wink_mcs51_user_main()
           - Unisim Wasm 态：纤程终结后，由 Runner 捕获复位事件并按需驱动后续 tick
           - ctx->virtual_us 从 0 重基，PAL/场景单调时钟平滑保持
```

### 3.4 重入机制方案与双环境架构（Task 0 ADR 定案）

针对 Host CTest（无 TS Runner）与 Unisim Wasm（有 TS Runner）的差异，采取**分层解耦架构**：

| 层级 | 运行环境 | 驱动机制 | 方案设计 | 价值与权衡 |
|---|---|---|---|---|
| **内核层 (Bridge)** | Host CTest / 原生执行 | `mcs51_bridge.cpp` 协作重入环路 | **方案 A 变体（协作微重启）**：在 `mcs51_app_loop` 建立 boot 环路。复位时在安全点执行状态机并重入 `wink_mcs51_user_main()`。 | **完全自包含**：不依赖姐妹仓，让原生 CTest 能够自闭环验证“多轮脉冲输出与标志保留” |
| **宿主层 (Runner)** | Wasm / Unisim 仿真 | TS Runner / Worker 轮询驱动 | **方案 B 导出**：导出 `pal_wasm_has_pending_reset()` / `pal_wasm_get_reset_reason()`，供 Unisim 对接 `SYSTEM_CONTROL/HARD_RESET`。 | **架构正交**：未来姐妹仓 ADR-0070 D-005b 就绪后可无缝启用真正的模块重载与 `.data/.bss` 真物理重置 |

### 3.5 触发路径细则与双使能仲裁

* **SWRST**：TA 解锁通过后检测 `old.bit7==0 && new.bit7==1` → shadow 自清 bit7 → latch(SW)。紧随其后的 `wink_mcs51_microstep()` 拦截点立即触发复位状态机，阻断 `while(1)` 空转。`SYS_DisableSoftwareReset()` 的写 0 为合法前置，不触发（P03）。
* **WDTRE + WDTIE 双使能仲裁与 P3.3 行为（P11）**：
  原厂 34 号源码同时开启了 `WDT_EnableOverflowInt()` 与 `SYS_EnableWDTReset()`，且在 `isr.c:242` 中有 `P33 = ~P33`。
  **仲裁规则**：当 `WDTRE=1` 时，复位属性具有最高优先级，溢出时**强制压制中断派发**（不向 CPU 派发 Vector 20 中断，防止 ISR 与复位重入竞态）。因此在 34 号负例测试中，**P3.3 必须始终保持初始弱上拉高电平**。
* **P15 latch/guard**：`reset_pending` 置位后，后续触发仅记诊断不重复处理；ISR 内触发合法（latch 后由安全点处理）；处理期间再次到达 → 丢弃并计数。

---

## 4. 任务分解与执行切片

### Task 0: 复位语义决策（ADR）与规范回写
* **产出**：新增 ADR（复位语义 + 纤程退出协议 + 粘性标志保护 + 双环境重入策略），并回写 Layer ① 与红线手册。
* **必须裁定的清单**：
  1. 纤程在 `while(1)` 中的主动拦截与退出协议；
  2. Host CTest 自闭环测试 Harness 与 TS Runner 导出的双层重入契约；
  3. `PORF`/`WDTRF` 粘性保持时序规范；
  4. 原厂 34 号 `WDTRE=1 && WDTIE=1` 双使能仲裁（复位压制 IRQ，P3.3 保持静止）；
  5. 复位清污标准清单（`wink_event_queue_deinit`、`edge_queue`、`in_service_depth=0`）；
  6. P14 用户数据策略（实例重载 / 快照恢复 / 显式声明限制）；
  7. 复位响应延迟契约（≤1 microstep / ≤1 quantum）。
* **验收**：ADR `Accepted`；裁定项写入技术设计与本计划修订记录。

### Task 1: 复位寄存器、粘性保护与状态机内核（不含重入，低风险先合入）
* **修改文件**：
  - `wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_sys.cpp`（逐位写语义、SWRST 沿、WDT 仲裁、latch/guard、标志回填入口、PORF 免 TA 清除）
  - `wink-micro-os/frameworks/mcs51/chips/cms8s78xx/include/REG_CMS8S78XX.H`（补齐 `SYS_Enable/DisableSoftwareReset`、`SYS_Get/ClearPowerOnResetFlag`，严按原厂 `system.c` 语义）
  - `wink-micro-os/frameworks/mcs51/include/wink_mcs51_wdt.h`（契约更新：从「不复位」改为「latch + 重入」）
  - `wink-micro-os/frameworks/mcs51/src/mcs51_context.cpp`（`mcs51_context_reset` 内增加 `saved_porf` 粘性暂存与种子后恢复）
  - `wink-micro-os/test/CMakeLists.txt` + 新增 `test/cms8s78xx/test_mcs51_reset_controller.cpp`（含 STRICT 孪生）
* **实现要点**：P03/P05/P06/P07/P11/P15；保持 TA 窗口收窄语义不变。
* **验收**：V-01、V-02；`test_mcs51_wdt_ta` 契约同步更新通过。

### Task 2: 纤程拦截、状态清污与重入机制实现
* **修改文件**：
  - `wink-micro-os/frameworks/mcs51/src/mcs51_bridge.cpp`（在 `wink_mcs51_microstep` 嵌入安全拦截点；实现 Host CTest 可重入 boot 环路；集成统一清污链）
  - `wink-micro-os/targets/wasm/wasm_entry.c`、`targets/wasm/exported_runtime_functions.json`（导出 reset 查询 C-ABI 供 Unisim 挂接）
* **实现要点**：P01/P04/P10/P13/P14/P16；复位时严格调用 `wink_event_queue_deinit()`、清空边沿队列、清空 `in_service_depth`；断言重启后外设弱上拉及 TA 种子恢复。
* **验收**：V-01（含多轮重入断言）、V-04（host 行为）、V-11（wasm 专项）。

### Task 3: 官方示例 33 `ResetBySoftware` 适配与验证
* **创建应用**：`wink-micro-app/vendor/cms8s78xx/reset_software`（原厂源码一行不改；依赖 Task 1 的 shim）
* **业务逻辑**：P3.2 闪烁 250 轮（仿真虚拟时间约 1.25ms）→ `SYS_DisableSoftwareReset()` → `SYS_EnableSoftwareReset()`（沿触发）→ `while(1){;}`。
* **场景**：`unisim-scenarios/reset_software.scenario.json`
  - 判别性设计：取首轮闪烁窗口后，断言 P3.2 **继续**输出脉冲串（复位重入的绝对证据；无复位实现会死在 `while(1)` 保持电平不变）；
  - 记录负向对照：临时禁用 SWRST 触发时该场景必须断言失败。
* **验收**：V-03、V-04。

### Task 4: 官方示例 34 `ResetByWDT` 负例与正例重设计
* **创建应用**：`wink-micro-app/vendor/cms8s78xx/reset_wdt`（原厂源码一行不改）
* **正确验收口径（双引脚负例判决）**：
  `main.c:92-100` 每轮喂狗（约 1.3ms ≪ 174.76ms）→ **双引脚断言：「全程不复位、P3.2 持续正常闪烁；且 P3.3 始终保持高电平，证明 WDT 中断未触发、复位未发生」**。
* **WDT 复位正例**：用**专用测试固件**（`test_mcs51_reset_controller.cpp` 内联 fixture，非载体）验证：不喂狗 → 溢出 → latch → 重入 main → `SYS_GetWDTResetFlag()==1`；验证 RE+IE 双使能下复位压制 IRQ。
* **场景**：`unisim-scenarios/reset_wdt.scenario.json`（负例口径：≥250ms 窗口内 P3.2 翻转 + P3.3 静止高电平 + 复位计数为 0）。
* **验收**：V-05、V-06。

### Task 5: 官方示例 35 `ResetByExtReset`（有条件推进）
* **前置**：GAP-06 CONFIG 建模 + NRST 引脚模型（复用 P2.4/P2.5，CONFIG 选择）+ board 声明 + `INPUT_PIN` 脉冲注入。
* **降级路径（默认）**：上述未就绪时，仅实现 chip 侧 NRST 观察模型的**测试 seam 注入**（单测覆盖 latch + 重入），Checklist 35 标注 `deferred（依赖 GAP-06）`，不虚打 `[x]`。
* **验收**：V-07。

### Task 6: 契约/文档回写与 Checklist 归档
* **内容**：`wink_mcs51_wdt.h` 契约注释；GAP 清单 GAP-07 状态更新；红线手册（复位前提、未支持复位源、粘性标志说明）；保真度文档；Checklist §7 编号 33/34/35 按 SOP 附证据叙述。
* **验收**：V-08~V-13；打勾均有证据链。

---

## 5. 验收标准与测试矩阵（🔴 必选）

| 编号 | 验证目标 | 验证命令 / 方法 | 通过标准 |
|:---:|---|---|---|
| **V-01** | 复位控制器单元测试 | `test_mcs51_reset_controller`（+STRICT 孪生） | 复位源×标志矩阵、**PORF 粘性保持**、SWRST 沿/自清、**事件队列清污**、**ISR 中断栈归零**、WDT 仲裁 100% PASS |
| **V-02** | 既有 mcs51 测试回归 | `python packages/wink-tools/wink.py test` | 全部 host 测试通过、0 回归；`test_mcs51_wdt_ta` 契约更新后通过 |
| **V-03** | 示例 33 资产构建 | SOP：`python packages/wink-tools/wink.py build sim --app vendor_cms8s78xx_reset_software` | `unisim-assets/` 三件套完整、wasm 有效（150~300KB） |
| **V-04** | 示例 33 场景断言（判别性） | SOP headless：`python packages/wink-tools/wink.py sim run --app vendor_cms8s78xx_reset_software --mode headless --scenarios .../reset_software.scenario.json` | P3.2 在首轮闪烁后**继续输出脉冲串**；负向对照（禁用 SWRST）场景断言失败 |
| **V-05** | 示例 34 资产构建与双引脚负例断言 | 同上针对 `reset_wdt` | ≥250ms 窗口内不复位、**P3.2 正常闪烁、P3.3 保持恒高、复位计数为 0** |
| **V-06** | WDT 复位正例（专用固件） | `test_mcs51_reset_controller` fixture | 不喂狗 → 复位 → main 重入 → `WDTRF=1`；IE+RE 仲裁符合 ADR |
| **V-07** | 示例 35（条件项） | 就绪：`reset_extreset` 构建+`INPUT_PIN` 场景；未就绪：单测 seam + Checklist deferred | latch + 重入发生；或 Checklist 显式 deferred 无虚标 |
| **V-08** | 用户数据策略验证 | 按 ADR：实例重载/快照恢复/限制声明 | 所选策略有对应断言（如重入后全局变量符合所选语义） |
| **V-09** | health_pot 生产场景无头验证 | SOP headless 全场景 | 0 故障计数、全场景绿；WDT 节奏无回归 |
| **V-10** | 经典 AT89 家族回归 | `python packages/wink-tools/wink.py test` 中经典 carrier 场景 | 无家族污染、无回归（共享 `mcs51_context_reset`） |
| **V-11** | wasm 重入专项（Node） | `wink-micro-os/frameworks/mcs51/test/wasm` 新增用例 | Asyncify/fiber 下重入无栈破坏；确定性双跑一致 |
| **V-12** | 文档/Checklist 归档 | 人工核对 | 契约/红线/保真度/GAP-07/Checklist 33-35 证据链齐全 |
| **V-13** | 架构门禁 | `python wink-tools/wink.py lint arch --pack layering --pack api` | 0 findings（chip/bridge 未越层） |

---

## 6. 风险与依赖登记

| 编号 | 类别 | 风险/依赖 | 影响 | 缓解 |
|:---:|---|---|---|---|
| R-01 | 依赖 | GAP-06（CONFIG 建模）未开工 | Task 5 无法完整落地；WDT 强制开语义挂起 | Task 5 默认降级为单测 seam + Checklist deferred |
| R-02 | 跨仓 | unisim `SYSTEM_CONTROL/HARD_RESET` 为 no-op（ADR-0070 D-005b） | 宿主级实例重载受阻 | **已在 v2.1 彻底缓解**：Bridge 内核提供微循环重入，Host CTest 完全自闭环，不被姐妹仓进度阻塞 |
| R-03 | 技术 | wasm `-sASYNCIFY=1` 下 `longjmp` 栈破坏 | 场景偶发崩溃/静默错 | 优先采用协作重入循环/状态机，避免跨 Asyncify 帧长跳转；V-11 专项兜底 |
| R-04 | 规范 | 参考手册对 IE+RE 仲裁细节沉默 | 行为偏差 | Task 0 结合原厂 34 号源码显式假设为“复位优先压制 IRQ”，并在测试中对 P3.3 设防 |
| R-05 | 保真 | 用户 `.data/.bss` 策略成本高 | 「芯片重启」承诺降级 | ADR 三选一；选③则同步降级 §7.1 措辞 |
| R-06 | 行为 | 复位中断/重入改变既有 `test_mcs51_wdt_ta` 与 `wink_mcs51_wdt.h` 契约 | 误报/回归 | 契约与测试同 PR 更新；V-02 全量回归 |
| R-07 | 范围 | 复位源仅覆盖 4/7 | 过度承诺风险 | §7.2 显式声明；红线手册登记不支持源 |

---

## 7. 保真度边界说明（Contract Honesty）

### 7.1 可承诺的范围（100% 行为等价）

在**已建模复位源（POR/SWRST/WDT/EXT）与已声明假设集**（CONFIG 声明、任务级复位响应延迟契约、用户数据策略）下，复位寄存器语义、位级写行为、粘性标志保持、复位清污流程、`main()` 重入达到 **100% 行为等价**，并由 V-01~V-13 全量验证；不涉及下列 §7.2 的承诺。

### 7.2 显式排除项（简录，不作任务）

1. **B 类（时序/µs 级）**：复位响应延迟、WDT 到期相位（现粗模型抖动 ≤1 microstep = 5µs）、ISR 响应前导——Native 不承诺（ADR-0076），未来 ISS 后端部分逼近。
2. **C 类（电气/物理）**：复位脚放电斜率/去抖、上电斜率、LVR/BOR 阈值、晶振起振、温漂压漂——HIL 范畴。
3. **未建模复位源**：LVR、CONFIG 状态保护复位、上电配置监控、SCM——按红线手册「不支持」显式声明，不静默假装。
4. **CONFIG/烧录一致性**：仿真只验证「声明的配置」，不保证「实际烧录的配置」——烧录 SOP/真机校验兜底（GAP-06 验收含此条）。
5. **手册歧义裁定项**：IE+RE 仲裁、PORF 清除 TA 粒度等按 Task 0 裁定为显式假设，非「证明」。
6. **用户数据策略若选「声明限制」**：则承诺降级为「main 重跑」而非「芯片级重启」。

---

## 8. 回滚与降级方案

* **按 Task 回退**：各 Task 文件基本正交（sys/chip/header/bridge/target 分离），`git revert` 单 Task 即可；Task 1 是 Task 2/3/4 的前置，回退需连带停用上层。
* **STRICT 降级**：复位行为仅在 Release 生效；STRICT 保持溢出中止 tripwire，避免测试矩阵被复位打断。
* **Checklist/deferred 降级**：Task 5 未满足依赖时仅降级该项（显式 deferred），不阻塞 33/34。
* **回滚验证**：回退后 host 全绿 + health_pot/经典 carrier 场景全绿、诊断计数归零（V-02/V-09/V-10）。

---

## 9. 参考资料

* 原厂代码：`docs/vendors/Cmsemicon/CMS8S78xx_DemoCode_V2.0.2/.../Reset/{ResetBySoftware,ResetByWDT,ResetByExtReset}/code/`；`Libary/StdDriver/src/system.c`（SWRST/标志 API）、`src/wdt.c`、`inc/system.h`。
* 数据手册 V1.0.7：§4.2 复位源、§4.5.1 WDT、§5 用户配置（CONFIG/外部复位/看门狗工作方式）；参考手册 §8.3（WDCON/TA）。
* 仿真核：`cms8s_sys.cpp`、`mcs51_context.cpp`、`mcs51_bridge.cpp`、`wink_entry`/`wink_runtime`/`wink_sim_scheduler`、`wink_mcs51_wdt.h`、`test_mcs51_wdt_ta.cpp`、`test_mcs51_silicon_seeds.cpp`。
* 跨仓：unisim `INPUT_PIN` 场景步骤；`SYSTEM_CONTROL/HARD_RESET`（unisim ADR-0070 D-005b）；SOP（`CMS8S78XX_EXAMPLE_CHECKLIST.md` §四）。
* ADR：0012/0070/0076；本计划 Task 0 新增 ADR。

---

### 计划版本变更记录

| 版本 | 日期 | 变更内容 | 变更人 |
|------|------|----------|--------|
| v1.0 | 2026-09-15 | 初始版本：GAP-07 + Checklist 33/34/35 规划 | — |
| v2.0 | 2026-09-15 | 架构评审修订：修正重入机制/34 号前提/外部复位三处硬伤；新增 Task 0（ADR）与 P01~P15 追溯表；补齐位级写语义、标志矩阵、双时钟、STRICT/runner、嵌套防护、用户数据策略；新增风险登记与回滚章节；§7.2 简录不可承诺边界 | — |
| v2.1 | 2026-09-15 | 架构深度一体化修订：闭环 5 处深层盲区——确立纤程死循环主动拦截机制；打通 Host CTest 原生自闭环测试 Harness；建立复位粘性标志暂存契约；补齐事件队列/中断栈清污时序；明确 34 号双使能下 P3.3 压制判定与双引脚负例断言 | — |
