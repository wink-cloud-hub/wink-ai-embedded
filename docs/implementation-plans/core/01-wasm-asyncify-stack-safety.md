# Phase 1: Wasm Asyncify 修复与栈安全门禁

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.
>
> **核验状态（2026-06-24）：** 已对照 `CMakeLists.txt`、`pal_osal_wasm.c`、`wasm_bridge.h` 与 Emscripten 官方文档逐项确认。

**Goal:** 修复 P0-1：CMake 中 `ASYNCIFY_IMPORTS` 指向死符号 `js_sim_get_ultrasonic_distance`，真实挂起点 `js_pal_delay_ms/us` 未声明；补栈溢出检查与断言门禁；**并补齐计划原本缺失的 JS 侧 Asyncify 挂起契约**。当前 Wasm 无法可靠运行（浏览器主循环卡死 / `Asyncify.StateError` / 栈恢复失败）。

**Architecture（含关键纠正）：**
- Emscripten Asyncify 的挂起由**两端**共同保证，缺一不可：
  1. **C/链接侧**：`ASYNCIFY_IMPORTS` 声明哪些 JS import 是异步的（合法设置名，经官方文档确认——不是 `ASYNCIFY_ONLY`/`ASYNCIFY_ADD`，那是限制 instrumentation 范围的优化项）。
  2. **JS 侧**：被声明的 import 的 JS 实现**必须**用 `Asyncify.handleSleep(wakeUp => { ... })` 或 `Asyncify.handleAsync(async () => ...)` 真正交还控制权并最终调用 `wakeUp()`；否则声明无效，调用 `wakeUp` 会抛 `invalid Asyncify state`。
- 真挂起点：`pal_osal_wasm.c:9-15` → `js_pal_delay_ms` / `js_pal_delay_us`（声明见 `wasm_bridge.h:33-34`）。死符号 `js_sim_get_ultrasonic_distance` 全仓无 C 调用点（DCE 移除），故当前配置等于"声明了一个不存在的异步 import，真正的异步 import 未声明" → runtime 主循环无法可靠让出。

> ⚠️ **本阶段最大风险与盲区**：JS 胶水（`js_pal_delay_ms` 的 JS 实现）在浏览器前端/Workbench 仓库，**不在本仓**。Task 1-1/1-2 的 C/CMake 修复是**必要但不充分**的。Task 1-4 必须把 JS 侧契约写成可验收的规范并跨仓验证，否则"修完仍卡死"。

> 🚨 **次生风险（review [D1](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md)）：修好挂起会激活中断重入崩溃。** `wink_runtime_run`（`runtime/src/wink_runtime.c:21-27`）每个 tick 末尾 `pal_delay_ms → js_pal_delay_ms` 触发 Asyncify 挂起，即**每 tick 打开一个 sleeping 窗口**。Asyncify sleeping 期间整个 wasm 调用栈为中间态，**JS 不得回调任何 wasm 导出**——否则触发 `RuntimeError: invalid Asyncify state`。但 `_trigger_wasm_interrupt`（`targets/wasm/wasm_entry.c:24`，经 `CMakeLists.txt:34` 导出）设计上随时可被 JS 调用以模拟 GPIO 中断，而 GPIO 中断（echo 上升沿、按键）最可能在等待窗口到达。**P0-1 未修时此 bug 被"卡死"表象掩盖；Phase 1 修通挂起的那一刻即把它激活为必然崩溃的活跃 bug。** 闭环见新增 Task 1-5；它是 Phase 4 中断路径的硬前置。

**Tech Stack:** Emscripten (classic Asyncify), CMake, Wasm/wasm32, C99

## Global Constraints
- 不破坏 host target 构建（`TARGET_PLATFORM=host` 全绿）
- 不修改 DAL/PAL 公共头（本阶段只动 wasm target + CMake + 文档）
- `ASYNCIFY_IMPORTS` 中的名字必须是 wasm_bridge.h 中 extern 声明的**裸函数名**（`js_pal_delay_ms`，非 `env.js_pal_delay_ms`）
- 文档同步：CMake 与 docs/design/04 的死符号必须同步清除（Phase 0 Task 0-3 负责 `.claude/skills` 下副本，互不重复）

## Sequencing
- 独立于 Phase 0（文件不相交，可与 Phase 0 并行启动）
- Task 1-1/1-2 串行（同改 CMakeLists.txt）；Task 1-3（文档）可与 1-1/1-2 并行
- **Task 1-3 → Task 1-4 串行**（review [D3](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md)：二者同改 `01-wasm-sandbox-lifecycle.md`——先 1-3 清死符号/裁决 i2c，后 1-4 原地补强 §2.2.2 契约）
- Task 1-4（JS 挂起契约）、**Task 1-5（中断重入约束）**均为**跨仓**项，可并行推进但不阻塞本仓 C 改动合入

---

### Task 1-1: 更新 CMake ASYNCIFY_IMPORTS（去掉死符号，声明真挂起点）

**Files:**
- Modify: `wink-micro-os/CMakeLists.txt:33`

**Source-of-truth check:** 已确认 L33 现为 `"-s" "ASYNCIFY_IMPORTS=['js_sim_get_ultrasonic_distance']"`；L32 为 `ASYNCIFY=1`。`js_sim_get_ultrasonic_distance` 全仓无 C 调用点（死符号）。

**Precise change** —— 将 L33 单行替换为：
```cmake
        "-s" "ASYNCIFY_IMPORTS=['js_pal_delay_ms','js_pal_delay_us']"
```

**设计理由（已核验）：**
- `pal_osal_wasm.c:9-11` `pal_delay_ms` → `js_pal_delay_ms`；`:13-15` `pal_delay_us` → `js_pal_delay_us`。两者是真实 Asyncify 挂起点。
- import 名与 `wasm_bridge.h:33-34` extern 声明逐字一致（裸函数名）。
- `js_pal_get_ms/us`（同步读，不挂起）**不**列入，正确。
- `js_pal_delay_us` 在 Wasm 仿真下当前可能不被调用（ultrasonic 走 js_sim_*），但列入是**防御性**的——若未来其它路径用到微秒延迟，已覆盖。

---

### Task 1-2: 栈溢出检查、断言、Asyncify 栈容量（dev/debug 构建门禁）

**Files:**
- Modify: `wink-micro-os/CMakeLists.txt` —— 在 L37（`EXPORT_NAME` 行）后追加

**Precise change:**
```cmake
        "-s" "STACK_OVERFLOW_CHECK=2"
        "-s" "ASSERTIONS=1"
        "-s" "ASYNCIFY_STACK_SIZE=65536"
```

**说明与边界：**
- `STACK_OVERFLOW_CHECK=2`：溢出时 abort，避免静默栈损坏（Wasm 线性内存越界后果严重）。
- `ASSERTIONS=1` + `STACK_OVERFLOW_CHECK=2` 带**运行时开销与体积代价**，属 **dev/debug 构建**配置。生产构建应评估 `ASSERTIONS=0`（体积/性能）与保留安全门禁的权衡——本阶段先以 debug 为默认，**生产 profile 调优留作后续**（不在本阶段决断）。**附带的早期报警价值（review [D1](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md)）**：这组断言同时是 Task 1-5 中断重入崩溃的早期报警器——重入会带断言 abort 而非静默栈损坏，便于联调期立即定位。
- `ASYNCIFY_STACK_SIZE=65536`：默认偏小，64KB 是针对本仓调用链（`main → wink_runtime_run → app_loop → BAL（算法）→ DAL → pal_delay_ms`）的安全起步值。**最终值须实测**：链路深度 × 局部变量规模。设过大浪费线性内存，过小栈恢复失败。不应直接采信"64KB"为定值——Task 1-4 的实测须包含此项。
  > ⚠️ **项目特有风险（review [D4](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md)）**：本项目业务逻辑（BAL 之上）为 AI 生成 / 可视化拖拽，可能产生**人脑无法预判的深递归调用链**（表达式求值递归下降、嵌套状态机）。Asyncify 栈消耗 ≈ O(递归深度 × 帧大小 + per-frame overhead)，一条深度递归的生成路径即可打穿 64KB，且因其发生在 AI 生成代码中、复现极难、定位极慢。Task 1-4 实测须覆盖"最深 AI 生成调用链"；治本应在 BAL/Codegen 层静态约束递归深度上限（呼应 [00-README](./00-README.md) 第八节 AI 友好 OS 契约）。

---

### Task 1-3: docs/design/04 Wasm 仿真文档同步（清死符号 + 更新 import/栈说明）

**Files:**
- Modify: `docs/design/04-wasm-simulation/archive/01-wasm-sandbox-lifecycle.md`
- Modify: `docs/design/04-wasm-simulation/archive/03-multi-channel-sim-routing.md`

**Depends-On:** 无（与 CMake 改动独立，可并行）。

**Precise changes:**
- 全文清除 `js_sim_get_ultrasonic_distance`，替换为 `js_sim_trigger_ultrasonic` + `js_sim_measure_echo_pulse_us`（与 `wasm_bridge.h:41-42`、`dal_ultrasonic.c:24/27` 一致）。
- Asyncify 章节：更新 import 列表为 `js_pal_delay_ms/us`，补栈配置（`ASYNCIFY_STACK_SIZE` / `STACK_OVERFLOW_CHECK` / `ASSERTIONS`）说明。
- **补一段"Asyncify 两端契约"**（呼应本阶段 Architecture）：明确 C 侧 `ASYNCIFY_IMPORTS` 声明 + JS 侧 `handleSleep/handleAsync` 实现缺一不可。
- **裁决 `js_pal_i2c_transfer` 的 Asyncify 归属（review [D2](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md)）**：`01-wasm-sandbox-lifecycle.md:79` 当前误列其入 `ASYNCIFY_IMPORTS=["js_pal_delay_ms", "js_pal_i2c_transfer"]`，与 `wasm_bridge.h:26` / `pal_hal_wasm.c:44-48` 的**同步 bool 零拷贝 transfer** 实现矛盾（2026-06-22 架构评审 L148 已记录、未裁决）。本 Task 须将其**从 IMPORTS 移除**并注明"i2c 为同步零拷贝、非挂起点，依据 ADR-0003"——机械替换若只改 delay 名而留下 i2c，则 SSOT 仍未闭合。

**Verification:**
- `Select-String -Path "docs\design\04-wasm-simulation\*.md" -Pattern "js_sim_get_ultrasonic_distance"` → 0 命中。
- `Select-String -Path "docs\design\04-wasm-simulation\01-wasm-sandbox-lifecycle.md" -Pattern "js_pal_i2c_transfer"` → 不再出现在 `ASYNCIFY_IMPORTS` 行（已裁决为同步、移出）。

> 注：`.claude/skills/.../simulation.md` 的同名死符号清理由 Phase 0 Task 0-3 负责，本 Task 不重复。

---

### Task 1-4: JS 侧 Asyncify 挂起契约（跨仓，原地补强 `01 §2.2.2`）

> ⚠️ review [D3](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md) 纠正：JS 挂起契约**并非"原计划完全缺失"**——`01-wasm-sandbox-lifecycle.md §2.2.2`（L60-71）已有 `Asyncify.handleSleep(wake => setTimeout(wake, ms))` 雏形。本 Task **原地补强**该既有契约，**不新建 `02-asyncify-suspend-contract.md`**——否则契约散落两文件，制造本项目反复栽过的"同一符号多处漂移"新 SSOT 分裂（`pitfalls.md:31` 活样本）。JS 胶水不在本仓；本 Task 产出**契约补强 + 跨仓验证清单**，不写本仓 C 代码。

**Files:**
- Modify: `docs/design/04-wasm-simulation/archive/01-wasm-sandbox-lifecycle.md`（原地补强 §2.2.2 既有契约）

**Produces（契约规范，须交付给前端/Workbench 仓库实现方）：**
```
js_pal_delay_ms(ms) 的 JS 实现必须通过 Asyncify 真正交还控制权：

  // classic Asyncify（与本仓 ASYNCIFY=1 一致）
  js_pal_delay_ms = (ms) => {
    Asyncify.handleSleep(wakeUp => {
      setTimeout(wakeUp, ms);     // 到时唤醒，恢复 Wasm 栈
    });
  };

验收要求：
  1. 不可写成「setTimeout 后直接 return」——那不会恢复 Wasm 栈，会导致下次调用 invalid state。
  2. wakeUp 必须被调用且仅调用一次；重复/遗漏调用即 StateError。
  3. js_pal_delay_us 同理（精度按浏览器 timer 下限，仿真可接受）。
  4. 入口与调度（review [D5](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md)）：JS 侧须用 `Module.callMain()`（**非** `Module._main()`）启动——`MODULARIZE=1` + `ASYNCIFY=1` 下只有 `callMain` 正确处理被插桩的 main。且 `main` 是 `wink_runtime_run(cb, 0)` 的永不返回驱动循环（靠内部 `delay` 反复挂起-唤醒推进），**JS 不得 `await callMain()` 等其结束**——前端按 `_main()` 直调或期望返回会偶发挂起/重入异常。不得旁路 Asyncify 机制自造挂起。
```

**跨仓验证清单（需 Emscripten + 前端环境，不阻塞本仓合入但须登记为 Phase 1 完成条件之一）：**
1. `emcmake cmake -DTARGET_PLATFORM=wasm ..` 无错误
2. `emmake make` 链接通过
3. `wasm-objdump -x wink_simulator.wasm` 的 Import 段含 `js_pal_delay_ms`、`js_pal_delay_us`
4. **JS 侧实现符合上述契约**（代码审查 + 运行时验证）
5. 浏览器加载，深调用链（`wink_runtime_run` → `app_loop` → `pal_delay_ms` → `js_pal_delay_ms`）能挂起并在 ms 后恢复，无 `Asyncify.StateError`、无主循环卡死
6. 调 `ASYNCIFY_STACK_SIZE` 至刚好不触发恢复失败的下限，记录实测推荐值（修正 Task 1-2 的 65536 起点）。**须用最深 AI 生成调用链压测**（review [D4](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md)），而非仅 `wink_runtime_run → app_loop` 固定链——AI 生成业务逻辑的递归深度不可预测。

**Deferred 声明：** 本仓无 Emscripten 环境时，1–6 标记为「待前端仓联调」，整改跟踪表 P0-1 不得在仅完成 Task 1-1/1-2 时即标"完成"——**须以本清单第 4–5 项 + Task 1-5 中断重入验证（见下）三项全部通过为准**。

---

### Task 1-5: 中断桥 Asyncify 重入约束（review D1 闭环，Phase 4 硬前置）

> ⚠️ 这是 review [D1](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md) 的专项闭环 Task，解决"修好挂起必然激活的中断重入崩溃"（见本文件顶部 Architecture 的 🚨 次生风险框）。它是 **Phase 4（超声波 echo 中断路径）的硬前置**。JS 胶水不在本仓；本 Task 产出**跨仓契约 + 验证清单**，不写本仓 C 代码。

**机制（三段推演，均经源码核验）：**
1. `wink_runtime_run`（`runtime/src/wink_runtime.c:21-27`）每个 tick 末尾 `wink_app_delay_ms → pal_delay_ms → js_pal_delay_ms` 触发 Asyncify 挂起 → **每 tick 打开一个 sleeping 窗口**。
2. Asyncify sleeping 期间，整个 wasm 调用栈为 unwind 完成的中间态（已写入 `ASYNCIFY_STACK_SIZE` 缓冲、控制权在 JS）。Emscripten 官方明确：**sleeping 期间不得从 JS 回调任何 wasm 导出**，否则与已保存的挂起栈冲突，触发 `RuntimeError: invalid Asyncify state` / abort / 栈损坏。
3. `_trigger_wasm_interrupt`（`targets/wasm/wasm_entry.c:24`，`CMakeLists.txt:34` 导出）设计上随时可被 JS 调用以模拟 GPIO 中断（echo 上升沿、按键），内部 `isr(arg)` 完成一次有状态 wasm 调用。GPIO 中断最可能在"等待"语义的 sleeping 窗口到达 → **确定性**崩溃路径（非概率性）。

**解法（三选，推荐 A）：**
- **(A) 中断排队 + tick 边界注入（推荐）**：JS 侧中断入 ring buffer，**仅在 wasm rewind 后、下一次 `delay` 前的 tick 边界 flush**；sleeping 期间拒绝直调 `_trigger_wasm_interrupt`。
- **(B) Asyncify state 守卫**：JS 侧用 `Asyncify` 暴露的运行态判断是否 sleeping，sleeping 时缓存中断、唤醒后再投递。
- **(C) 架构级（治本，长期）**：把"JS 随时注入中断"改为"wasm 主动 poll pending 中断"的轮询模型，从根上消除重入面——需重设 `wasm_entry.c` 中断桥契约（与 Phase 6 Task 6-3 的回调索引边界一并收敛）。

**Files:**
- 跨仓：JS 侧中断注入逻辑契约（交付前端/Workbench 仓库实现方）
- Modify: `targets/wasm/wasm_entry.c`（**仅当采解法 C** 时改中断桥模型；A/B 不改本仓 C）

**跨仓验证清单（需 Emscripten + 前端环境）：**
1. sleeping 窗口期间 JS 触发 GPIO 中断 → 中断被排队（或缓存），**不**直调 `_trigger_wasm_interrupt`
2. wasm 唤醒后、下一次 `delay` 前 → 排队的中断按序 flush，ISR 正确执行
3. 全程无 `RuntimeError: invalid Asyncify state` / 无 abort / 无栈损坏
4. 深调用链 + 高频中断（模拟 echo 上升沿密集到达）压力下稳定

**Deferred 声明：** 本仓无 Emscripten 环境时，1–4 标记为「待前端仓联调」。**P0-1 的关闭以本 Task 验证通过为必要条件之一**（与 Task 1-4 联调清单并列）。

---

## Verification Gate
**本仓可自验（无需 Emscripten）：**
- `python wink-tools/wink.py test` → host target 仍 8 PASS（证明未破坏非 wasm 构建）
- `Select-String -Path "wink-micro-os\CMakeLists.txt" -Pattern "js_sim_get_ultrasonic_distance"` → 0 命中
- `Select-String -Path "wink-micro-os\CMakeLists.txt" -Pattern "js_pal_delay_ms"` → 命中

**须 Emscripten + 前端仓（Task 1-4 清单 1–6 + Task 1-5 中断重入验证）：** 全部通过后方可关闭 P0-1。

## 出口验收
- [ ] 本仓自验三项全过
- [ ] `01-wasm-sandbox-lifecycle.md §2.2.2` 契约原地补强完成，**未新建 `02-asyncify-suspend-contract.md`**（SSOT 不分裂，D3）
- [ ] `js_pal_i2c_transfer` 已从 `ASYNCIFY_IMPORTS` 裁决移除（同步零拷贝，D2）
- [ ] **Task 1-5 中断重入约束已交付前端仓，且 Emscripten 环境验证 sleeping 窗口中断重入不再可触发**（或登记待联调，D1）
- [ ] 整改跟踪表 P0-1 状态按 Task 1-4 联调 + Task 1-5 重入验证完成度如实更新（未联调前为"部分完成"）

**Sources:**
- [Emscripten — Asynchronous Code (Asyncify)](https://emscripten.org/docs/porting/asyncify.html)
- [Emscripten — Settings Reference](https://emscripten.org/docs/tools_reference/settings_reference.html)

