# 8051 单片机 Target 可移植性评审

**评审日期**：2026-07-28
**评审对象**：
- `wink-micro-os/pal/include/`（OSAL `pal_osal.h` / HAL `pal_hal.h` 等公共契约）
- `wink-micro-os/targets/`（现有 `esp32` / `host` / `wasm` / `common` target 实现）
- `wink-micro-os/CMakeLists.txt`（C99 标准与编译纪律）
**评审视角**：资深嵌入式架构师，静态分发范式（ADR-0004）
**关联决策**：ADR-0002（双 target 同源编译）、ADR-0004（静态分发）、ADR-0007（协作式单任务执行模型）、ADR-0016（Task/ISR 双入口临界区）、ADR-0017（阻塞 API 三层隔离）
**核心问题**：现已支持 ESP32 真机 target，能否再支持 8051（51 单片机）？
**结论**：**理论可行，但不能照 ESP32 target 平移**。分层接口与协作式单任务模型对新 target 友好，8051 的 RAM / 编译器 / 浮点三座大山决定它只能承载一个**受裁剪的极简 profile**，且必须先用 ADR 钉死能力裁剪矩阵 + 一个 SDCC spike 验证「放得下」。

---

## 一、总体结论

WinkMicroOS 的分层（PAL = OSAL + HAL 两个纯 C 接口）与运行时模型对「再加一个真机后端」是友好的：接口可移植性这一关能过。真正的阻力**全部集中在 8051 这颗芯片的物理现实与它的编译器**，而非架构本身。

**一句话**：分层架构支持，协作式单任务模型甚至天生契合；但 8051 只能承载一个极简 profile，必须先 ADR 钉边界 + SDCC spike 证明「放得下」，再谈铺开。

---

## 二、有利条件（项目已铺好的路）

1. **运行时模型天然契合**
   ADR-0007 已将执行模型收敛为**单任务 + 协作式 `switch-case` 挂起**，无抢占式多任务、无双核。8051 正需要这种裸 `while(1)` 模型。`pal_os_task_create` 在单线程 target 上直接返回 `WINK_ERR_UNSUPPORTED` 即可——接口注释（`pal_osal.h:277`「WASM/bare-metal targets may call func synchronously or return UNSUPPORTED」）已允许优雅降级。

2. **已预设 baremetal 语义**
   `pal_osal.h` 多处显式写明 baremetal 分支的实现方式，说明第四类 target 早在设计意图内：
   - 临界区（`pal_osal.h:198`）：baremetal「与 task 版共用关中断原语实现（`pal_bsp_irq_save/restore`）」。
   - ISR 上下文判定（`pal_osal.h:217`）：baremetal 为 no-op。
   - ringbuf（`pal_osal.h:369`）：baremetal「使用 interrupt-disable critical sections 保证原子性」。
   - heap / stack introspection、WDT、reset reason：baremetal 恒 0 / no-op。

3. **C99 + 禁 GCC 扩展的纪律**
   `CMAKE_C_STANDARD 99`（`CMakeLists.txt:5`）、MSVC 链强制纯 C99（`TESTING.md`）、ADR-0002 静态核查禁 `#pragma pack` / 位域——比一般项目更容易往受限编译器上搬。

4. **静态分发少用函数指针**
   ADR-0004 的编译期静态分发，恰好规避了 8051 上函数指针调用昂贵 / Keil banking 受限的问题，是一处幸运的对齐。

---

## 三、硬骨头（会顶到地基，必须先决策）

### 🔴 H-1：内存 —— 头号杀手

- 经典 8051 仅 128–256B RAM；即便增强型（STC8 / 新唐 N76E003）也只有约 1–2KB XRAM、几十 KB Flash。
- 现有 PAL 的 `pal_os_ringbuf_create` / mutex / sem `create` / 任务栈均为**动态分配心智**；`wink_sim_physical`、trace、selftest 等模块的 RAM/ROM 占用按 ESP32（约 520KB RAM）量级编写。
- **8051 上大概率连 runtime + 一个 DAL 驱动都放不下。** 这不是移植问题，而是「要不要做极简子集 profile」的产品决策。

### 🔴 H-2：编译器 —— C99 承诺在 8051 上破功

8051 无 GCC / Clang，只有 **SDCC**（部分 C99）或 **Keil C51**（C90 + 私有扩展）。两者都会冲击现有双 target 纪律：

| 冲击点 | 说明 | 影响的既有设计 |
|---|---|---|
| 非重入默认 | Keil C51 因栈极小，把局部变量做 static overlay，函数**默认不可重入**；与 ISR 共享的函数会踩内存 | ADR-0016 Task/ISR 双入口模型需重新审 |
| 内存模型关键字 | `__data/__xdata/__code/sfr/reentrant` 为非标扩展 | 需宏层吸收（类比现有 `WINK_BLOCKING`） |
| 函数指针 | 无硬件参数栈、分页 Flash，函数指针开销大、Keil banking 受限 | PAL 的 `pal_gpio_isr_t` 回调、ringbuf 仍用函数指针 |
| `_Static_assert` | C90/部分 C99 不支持 | 已有 `wink_app.h:50` pre-C11 fallback（好事），但 SDCC/C51 需实测 |

### 🔴 H-3：浮点 —— 全软件模拟

- 8051 无 FPU，`float` 全靠库软件模拟，极慢且占 ROM。
- PWM duty（`pal_pwm_set_duty(float)`，`pal_hal.h:112`）、任何 control/PID、`wink_sim_physical` 均用 `float`。
- **8051 profile 基本要禁浮点或全面定点化。** 这与 FOC 计划中「ISR 数值定点 vs float」是同源问题，只是此处是全局性的。

### 🟡 H-4：64 位时间戳开销

- `pal_os_get_ms/us` 返回 `uint64_t`（`pal_osal.h:43/48`）。8051 上 64 位运算靠多字节软件模拟，开销可观。
- 8051 profile 可能需降级为 32 位时间戳。

---

## 四、建议（若决定推进）

1. **先定位：完整 target vs 极简 profile？**（强烈建议后者）
   定义 `WINK_PROFILE_MINIMAL`：无堆、无 float、无 ringbuf 动态创建（改静态池）、32 位时间、仅保留 GPIO/PWM/最小 DAL。**先写 ADR 钉死「8051 profile 能力裁剪矩阵」**——哪些 PAL API 合法返回 `WINK_ERR_UNSUPPORTED`。

2. **选型收窄**
   不碰经典 128B RAM 的 8051。锁定 **STC8 / 新唐 N76E003** 这类增强型 8051（≥1KB XRAM、≥16KB Flash），**优先 SDCC**（可进 CMake、开源、比 Keil 更接近 C99），用宏层（如 `pal_cc.h`）吸收编译器差异。

3. **做一个 spike（对标 ADR-0002 做法）**
   最小 BAL/DAL/PAL（一个 LED blink + 一个 GPIO 中断）在 SDCC 下编译 + 真机跑通，**量化 ROM/RAM 占用**。先证明「放得下」，再谈铺开。

4. **仿真同源不受影响**
   8051 只是多一个真机后端，wasm/host 仿真不变——「虚实同源」承诺仍成立，只是 8051 profile 能仿真的能力子集更小。

---

## 五、优先级汇总

| 编号 | 事项 | 严重度 | 时机 |
|------|------|--------|------|
| H-1 | 内存预算：runtime + 最小 DAL 能否放进增强型 8051 | 🔴 | spike 首要验证项 |
| H-2 | 编译器：SDCC/C51 的 C99 缺口、非重入、内存模型关键字宏层 | 🔴 | ADR 决策 + spike |
| H-3 | 浮点全软件模拟：profile 禁浮点 / 全面定点化 | 🔴 | ADR 裁剪矩阵 |
| H-4 | 64 位时间戳降级为 32 位 | 🟡 | profile 设计时 |
| S-1 | 选型收窄至 STC8 / N76E003 + SDCC | 🟡 | ADR 决策 |
| S-2 | `WINK_PROFILE_MINIMAL` 能力裁剪矩阵 ADR | 🔴 | 开工前置 |

**一句话结论**：可行，但必须走「极简 profile ADR + SDCC spike 验证放得下」两步，不能直接照 ESP32 target 平移。

---

## 六、后续建议动作

- 起草 **ADR：8051 / 极简 profile 可行性与能力裁剪矩阵**（关联 ADR-0002 / ADR-0007）。
- 起草 **spike 计划**（对标 ADR-0002 的 time-boxed spike）：SDCC 下最小 BAL/DAL/PAL 编译 + 真机 blink + GPIO 中断，产出 ROM/RAM 占用报告与可行性结论（✅/⚠️/❌）。
