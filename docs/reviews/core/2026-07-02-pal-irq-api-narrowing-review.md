# 2026-07-02 · PAL 中断 API 收窄评审（AI Codegen 友好视角）

| 项 | 内容 |
|----|------|
| 评审日期 | 2026-07-02 |
| 评审对象 | `wink-micro-os/pal/include/pal_irq.h`（v2.2 当前形态） |
| 评审动机 | Q3 计划 Track F 原本只做 `pal_irq_direct_connect → pal_irq_iram_bind` 改名。深挖后发现虚标契约 **不止一处**，本文全面梳理 PAL IRQ 公开面，判断当前设计是否是"跨芯片同一套标准 + AI Codegen 友好"两个核心约束下的最优解 |
| 评审基准 | [ADR-0012 契约诚实](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0002 双 target 同源编译](../../decisions/unisim/0002-dual-target-compilation.md)、[CLAUDE.md · AI Codegen 北极星](../../../../CLAUDE.md) |
| 关联评审 | [2026-06-30 PAL 中断子系统架构评审](2026-06-30-pal-interrupt-subsystem-architecture-review.md)、[2026-07-01 external comprehensive review critique](2026-07-01-external-comprehensive-review-critique.md) |
| 关联计划 | [2026-07-01 Q3 计划](../../implementation-plans/core/2026-07-01-wmos-code-optimization-q3-plan.md) Track F |
| 拟产 ADR | ADR-0018「PAL IRQ 公开 API 收窄（面向 AI Codegen 的最小完备集）」（尚未起草） |
| 评审结论 | ⚠️ **过度工程化**：当前 IRQ API 参考 Linux/Zephyr 的成熟做法，但目标场景（AI 生成的小型嵌入式应用 + Wasm 仿真 + 主要 target 是 ESP32/STM32）用不上这个复杂度。建议 Track F 从"1 天改名"升级为"3~5 天的一次性收窄"，一次性偿还 6 处过度设计债务 |

---

## 1. 评审判据：什么是本项目的"IRQ API 最优解"

本项目有两个比一般 RTOS 更强的约束，也是判断"合理性"的两把尺子：

### 1.1 尺子 A：AI Codegen 友好

CLAUDE.md 明确"面向 AI 生成嵌入式应用的低代码开发平台"。这引出对 API 的具体要求：

- **选项越少越好**：AI 在 3 种 handler 原型 + 6 级优先级 + 2 种临界区之间做选择，缺乏可靠启发式；错选会产生看似合理、实则违约的代码。
- **语义越单一越好**：一个函数只做一件事，且名字诚实反映该事。虚标契约在 AI 面前危害更大——AI 只能读名字/doxygen 挑 API，不会像人一样做"读实现验证语义"的兜底。
- **默认路径要能覆盖 ≥90% 场景**：进阶 API 应显式标注"仅供 X 场景"，避免 AI 在无充分理由时误选。

### 1.2 尺子 B：跨芯片同源 & 契约诚实（ADR-0012）

- **抽象 API 必须在 ESP32/STM32/Wasm 都语义一致**，任何一个 target 虚标即违反 ADR-0012。
- **无法在所有 target 兑现的能力**，要么下沉到"目标专用未公开接口"，要么整个从公开面移除，不能挂在 PAL 公共头文件继续误导 AI 与用户。
- **YAGNI 原则**（[XP 原理](https://martinfowler.com/bliki/Yagni.html)）：非 root-cause-verified 的未来需求预留即负债——尤其是被 AI 训练数据反复"泛化"的旧惯例（Linux shared IRQ、synchronize_irq）。

### 1.3 与外部 RTOS 惯例的对比

作为"合理性"的横向参照：

| 参考对象 | IRQ 公开面复杂度 | AI Codegen 语料 |
|---------|---------------|-----------------|
| **Linux kernel** | `request_irq / request_threaded_irq / shared / synchronize_irq / setup_irq / disable_irq_nosync` … 20+ | 存在（drivers/），但 kernel 场景 |
| **Zephyr** | `IRQ_CONNECT` + `irq_lock / irq_unlock` + `IRQ_ZERO_LATENCY_LEVEL` 布尔 flag。**没有 shared，没有 synchronize** | 有 |
| **Arduino** | `attachInterrupt(pin, isr, mode)` + `noInterrupts()/interrupts()`。**没有优先级** | 大量 |
| **ESP-IDF** | `gpio_isr_handler_add` + `esp_intr_alloc` + `portDISABLE_INTERRUPTS`。**优先级用硬件级别 LEVEL1~3** | 大量 |
| **CMSIS-NVIC** | `NVIC_EnableIRQ / NVIC_SetPriority`。**优先级 = 硬件数值** | 有 |

结论：**Zephyr / Arduino / CMSIS 的公开面都比 wink-micro-os 现在小**，且它们的目标场景比 wink-micro-os 复杂得多。wink-micro-os 是"简化嵌入式给 AI + 学习者用"，公开面反而比 Zephyr 大，这是当前设计的第一违和点。

---

## 2. 现状盘点：`pal_irq.h` v2.2 公开面全景

### 2.1 类型与常量

| 符号 | 语义 | 各 target 状态 |
|------|------|--------------|
| `pal_isr_t` = `void(*)(void*)` | 普通带 arg ISR | ✅ 全 target 支持 |
| `pal_direct_isr_t` = `void(*)(void)` | 无 arg "直连"ISR | ⚠️ **虚标**：三 target 都走软件派发 trampoline，非真硬件直派 |
| `pal_irq_shared_handler_t` = `bool(*)(void*)` | 责任链共享 ISR | ⚠️ 存在但**无任何 sample/DAL 使用**，仅 test_pal_irq.c 有测试 |
| `PAL_IRQ_PRIO_LOWEST` | 最低优先级 | ⚠️ ESP32 映射到 LEVEL1，与 LOW **物理等价** |
| `PAL_IRQ_PRIO_LOW` | 低优先级 | 同上 |
| `PAL_IRQ_PRIO_NORMAL` | 默认 | ESP32 → LEVEL2 |
| `PAL_IRQ_PRIO_HIGH` | 时间敏感 | ESP32 → LEVEL3 |
| `PAL_IRQ_PRIO_HIGHEST` | 最高 RTOS 安全 | ⚠️ ESP32 → LEVEL3，与 HIGH **物理等价** |
| `PAL_IRQ_PRIO_REALTIME` | 硬实时 (NMI 级) | ⚠️ **虚标**：全 target 均返 `WINK_ERR_UNSUPPORTED` |

### 2.2 注册/控制函数

| 函数 | 用途 | 判定 |
|------|------|------|
| `pal_irq_enable(irq, prio, handler, arg)` | 主注册接口 | ✅ 核心，保留 |
| `pal_irq_direct_connect(irq, handler)` | "硬件直派"注册 | ⚠️ 虚标（Q3 Track F 已识别） |
| `pal_irq_shared_register(irq, prio, h, arg)` | 共享 IRQ 责任链 | ⚠️ 无真实使用，仅测试 |
| `pal_irq_disable(irq)` | 注销 | ✅ 核心，保留 |
| `pal_irq_synchronize(irq)` | 等待所有 core 退出 ISR | ⚠️ 未来才可能需要（见 §3.3） |
| `pal_irq_set_pending(irq)` | 软触发 | ✅ 保留（仿真+测试关键） |
| `pal_irq_clear_pending(irq)` | 清 pending | ✅ 保留 |

### 2.3 临界区

| 符号 | 语义 | 判定 |
|------|------|------|
| `pal_irq_save()` | 屏蔽**所有**可屏蔽中断（含 REALTIME） | ⚠️ REALTIME 都虚标了，此接口存在意义削弱 |
| `pal_irq_save_rtos_safe()` | 仅屏蔽 ≤ syscall pri | ✅ 核心，保留 |
| `pal_irq_restore(mask)` | 恢复 | ✅ 保留 |
| `PAL_CRITICAL_SECTION(code)` | RAII，走 rtos_safe | ✅ 推荐默认 |
| `PAL_CRITICAL_SECTION_STRICT(code)` | RAII，走 save() | ⚠️ 与 save() 命运绑定 |

### 2.4 属性宏

| 宏 | 用途 | 判定 |
|----|------|------|
| `PAL_ISR` | 展开 IRAM_ATTR / 空 | ✅ 好抽象，保留 |
| `PAL_DIRECT_ISR` | 目前等同 PAL_ISR | ⚠️ 与虚标接口捆绑，命运待定 |
| `PAL_DEFINE_ISR(name, type, arg)` | 类型安全 ISR 宏 | ✅ AI 友好，强烈保留 |

### 2.5 真实使用情况（grep 佐证）

| 使用点 | 涉及 API |
|-------|---------|
| `samples/devkitc_smoke/app_callbacks.c` | `PAL_ISR`（属性宏） |
| `samples/smp_uaf_test/*` | `pal_irq_enable/disable/synchronize/set_pending`（**测试 SMP UAF 场景本身**，非业务需求） |
| `dal/**/*.c` | **零使用**（都通过 `pal_gpio_enable_interrupt` 间接使用 GPIO ISR） |
| `test/test_pal_irq.c` | 全公开面（包括 `shared_register`、`REALTIME`、`STRICT`） |

**关键事实**：**DAL 与业务 sample 没有一处直接用 `pal_irq_direct_connect / pal_irq_shared_register / pal_irq_synchronize / PAL_IRQ_PRIO_REALTIME / PAL_IRQ_PRIO_HIGHEST / PAL_IRQ_PRIO_LOWEST / PAL_CRITICAL_SECTION_STRICT`**。它们的全部使用者是 `test/test_pal_irq.c` 和 `samples/smp_uaf_test/`（其中 smp_uaf_test 本身就是"验证 synchronize 是否真解决 UAF"的元测试）。

**换言之，收窄这 6~7 个 API 对业务代码零影响。**

---

## 3. 逐项深入评审

以下每一节均按 **现状 → 参考对照 → 判定 → 建议**结构。

### 3.1 🔴 `pal_irq_direct_connect` —— 虚标契约（Q3 Track F 已识别）

**现状**：头文件仍保留"直连中断 / 硬件矢量直派 / 零延迟"暗示语；实现三 target 全部退化为 `pal_irq_enable((pal_isr_t)handler, NULL)`（+ trampoline 修 CFI）。

**参考对照**：Zephyr `IRQ_DIRECT_CONNECT` **是真直派**（直接编译到 vector table），wink 借了这个名字但没借语义。ADR-0012 已明确将此列为契约欠债 G1。

**判定**：**虚标**。当前保留的两个价值（无 arg 简化签名 + CFI 修复）都是实现层面的偶然收益，与"direct connect"字面语义无关。

**建议**：
- **短期（Track F 已在做）**：改名 `pal_irq_iram_bind`，doxygen 删除"零延迟 / 直连 / vector-direct"字样。
- **长期**：真需要硬件矢量直派时（例如接入 STM32 裸机 target 需要极致响应），另开 `pal_irq_direct_connect_unsafe()`（名字里带 `_unsafe` 表达"绕过 PAL 保护"）。

### 3.2 🔴 `pal_irq_shared_register` —— 需求 = 0 的历史包袱

**现状**：责任链模式，参考 Linux Shared IRQ。三 target 都有实现，DAL/sample **零使用**。

**参考对照**：
- **Linux Shared IRQ** 存在因为 PCI 卡物理共享 IRQ 线。
- **Zephyr / RT-Thread / Arduino / ESP-IDF 面向用户的公开 API 中都没有**（ESP-IDF 内部 `gpio_isr_service` 帮用户处理了 GPIO ISR 共享，但用户看到的是"一个 pin 一个 handler"）。
- **wink-micro-os 目标平台的现实**：
  - **ESP32**：GPIO ISR 共享由 ESP-IDF `gpio_isr_service` 处理，wink 的 GPIO ISR 层已经暴露"一个 pin 一个 handler"的接口。
  - **STM32**：`EXTI9_5_IRQHandler` 这类共享向量是**编译期已知**的（一个 handler 里 if-else 各线），不需要运行期链表。
  - **Wasm**：完全不存在共享向量。

**判定**：**需求 = 0**。责任链在 wink-micro-os 场景下没有任何真实用例。三 target 的实现代码（约 100 行 × 3 = 300 行）纯粹是维护负担。

**建议**：**删除**。如果未来某个特殊硬件需要共享（例如接入某块专用板），那是该硬件适配层的内部实现细节，不必污染 PAL 公开面。测试代码 `test_pal_irq.c` 中共享相关测试同步删除。

### 3.3 🟡 `pal_irq_synchronize` —— YAGNI 边缘案例

**现状**：SMP 关键同步原语，用于 `disable → synchronize → free` 模式。参考 Linux `synchronize_irq()`。

**参考对照**：
- **Linux `synchronize_irq()` 存在因为**：内核动态注册/注销 ISR 是常态，且释放 ISR-owned 资源必须等待所有 core 退出 ISR。
- **wink-micro-os 现实**：
  - AI 生成的应用 99% 是"启动时 init → 循环运行到关机"，几乎不会热插拔 ISR。
  - 项目其它地方在**收敛**"运行期动态分配"（见 ADR-0008、多项 review）。
  - ESP32 是 SMP（双核），但业务 ISR 挂在具体 core（`esp_intr_alloc_flags`），运行期迁移少见。
- **反证**：唯一使用者 `samples/smp_uaf_test/` 是"**验证 synchronize 本身**"的测试，不是业务需求驱动的用例。

**判定**：**当前 YAGNI**，但**未来可能需要**（如果 wink-micro-os 加入 hot-plug 传感器或运行期设备重配置能力）。

**建议**：
- **选项 A（激进）**：删除，等真实需求出现时再加。
- **选项 B（保守，推荐）**：**保留但降级为"内部/高级 API"**：
  - 迁出 `pal_irq.h`，进 `pal_irq_advanced.h`（新头文件），需要显式 include。
  - doxygen 标"仅在 hot-plug 或 SMP 资源热释放场景使用；AI Codegen 不应默认生成此调用"。
- 同时删除 `test_pal_irq.c` / `samples/smp_uaf_test/` 中对它的元测试（改成"如果未来需要，验证方法参考 git history"）。

### 3.4 🟡 6 级优先级 → 建议收窄到 3 级

**现状**：`LOWEST / LOW / NORMAL / HIGH / HIGHEST / REALTIME` 共 6 级；ESP32 上 `LOWEST=LOW=LEVEL1`、`HIGH=HIGHEST=LEVEL3` **物理等价但契约不同**（HIGHEST 声称"最高 RTOS 安全"，实际跟 HIGH 一样都在 LEVEL3）。

**参考对照**：
- **STM32 Cortex-M**：16 级 + sub-priority（可配），但业务代码常用 3~4 级即够。
- **ESP32**：硬件只有 3 个 C-ISR 可用级别（LEVEL1/2/3），LEVEL4+ 是 NMI，无法用 C 写。
- **Zephyr**：没有归一化枚举，直接暴露硬件数值。
- **wink-micro-os 的抽象目标**：让 AI 用**语义**选优先级，不是数值。

**问题**：
- 6 个语义级别在 ESP32 上映射到 3 个物理级别 → **有 3 个是别名**。AI 面对 6 选 1 缺乏区分依据。
- `HIGHEST` 声称"RTOS 安全"、`HIGH` 声称"时间敏感"——但物理上等价，语义边界靠 doxygen 支撑，不靠代码强制。这是**契约诚实原则的软违规**。
- `REALTIME` 已经全 target 拒接（返 `WINK_ERR_UNSUPPORTED`）—— **虚标枚举值**。

**判定**：**收窄**。

**建议**：
```c
typedef enum {
    PAL_IRQ_PRIO_LOW    = 0,  // 一般 I/O
    PAL_IRQ_PRIO_NORMAL = 1,  // 通信外设（默认）
    PAL_IRQ_PRIO_HIGH   = 2,  // 时间敏感（仍是 RTOS-safe，可调 FromISR）
    PAL_IRQ_PRIO_COUNT
} pal_irq_prio_t;
```

映射：
- ESP32：LOW=LEVEL1、NORMAL=LEVEL2、HIGH=LEVEL3。
- STM32：直接映射到 3 段硬件优先级区间。
- Wasm/Host：仿真调度顺序而已。

删除：`LOWEST`（与 LOW 等价）、`HIGHEST`（与 HIGH 等价）、`REALTIME`（虚标）。

**未来路径**：真需要"NMI 级零延迟"时，通过独立接口 `pal_irq_direct_connect_unsafe()` + 独立类型（不再作为 prio 枚举值），让"这是特殊路径"在 API 层面就显眼。

### 3.5 🟡 3 种 handler 原型 → 建议合并到 1 种

**现状**：`pal_isr_t = void(*)(void*)` + `pal_direct_isr_t = void(*)(void)` + `pal_irq_shared_handler_t = bool(*)(void*)`。

**判定**：
- `pal_irq_shared_handler_t` 随 §3.2 一同删除。
- `pal_direct_isr_t` 的价值仅剩"回调里不用写 `(void)arg;`"——这是审美偏好，不是价值。AI Codegen 完全能生成 `(void)arg;` 一行。合并到 `pal_isr_t`。
- **未来真直派** 若需要 `void(*)(void)` 签名（因硬件 vector table 强制），那时再引入 `pal_hw_vector_t` 或类似**语义清晰的类型名**，配 `pal_irq_direct_connect_unsafe()`。

**建议**：合并到唯一的 `pal_isr_t`。

### 3.6 🟡 双临界区宏 `PAL_CRITICAL_SECTION` / `PAL_CRITICAL_SECTION_STRICT`

**现状**：普通版本走 `rtos_safe`，STRICT 版本走全屏蔽 `save()`。前者推荐默认。

**判定**：
- 只公开一个宏 `PAL_CRITICAL_SECTION`（走 rtos_safe）。
- `STRICT` 版本迁到 `pal_irq_advanced.h`（同 §3.3 `synchronize`），需要显式 include。理由：AI Codegen 面对二选一会误用（"更 strict 就是更安全"是常见误解，实际会拉长临界区破坏 Wi-Fi 时序）。
- 与之绑定：底层 `pal_irq_save()`（全屏蔽版）也迁到 advanced 头；`pal_irq_save_rtos_safe()` 是唯一公开路径。

**同步收益**：ADR-0016（临界区双入口）后来还引入 `_from_isr` 变体。整套 API 只暴露 `PAL_CRITICAL_SECTION` + `PAL_CRITICAL_SECTION_FROM_ISR` 两个宏，其余全部下沉。

### 3.7 ✅ 值得保留 & 背书的设计

不是全否定，以下是**该保留**的好设计：

| 保留项 | 保留理由 |
|-------|---------|
| **`PAL_ISR` 属性宏抽象** | 把 `IRAM_ATTR` 藏在 PAL 后面，其它 target 展开为空——跨芯片抽象该有的样子 |
| **`PAL_DEFINE_ISR(name, type, arg)`** | 对 AI 尤其友好：`PAL_DEFINE_ISR(my_isr, struct my_ctx, ctx)` → `ctx->field` 直用，无 cast，无 `(void)arg;`。**是好设计** |
| **优先级方向统一（数值越大越高）** | 掩盖 STM32/ESP32 数值方向相反的硬件差异，是真价值 |
| **save/restore 支持嵌套** | 必要，且实现细节合理 |
| **双 target 同源编译能力** | Wasm/host 有等价 IRQ 模拟——对仿真至关重要 |
| **`pal_irq_set/clear_pending`** | 仿真触发中断、单元测试都需要 |
| **头文件顶部"ISR 安全 vs 非 ISR 安全"契约分组** | 分类清晰，是给 AI 的高价值语料 |

---

## 4. 收窄前后对比

### 4.1 公开符号数量

| 分类 | 当前 | 收窄后 | 变化 |
|------|-----|-------|------|
| 类型（typedef） | 3 | 1 | -2（删 direct/shared） |
| 优先级枚举值 | 6 + COUNT | 3 + COUNT | -3 |
| 注册/控制函数 | 7 | 4 | -3（删 direct/shared/synchronize→advanced） |
| 临界区（宏 + 函数） | 6 | 3 | -3（strict/save 全屏蔽版→advanced） |
| 属性宏 | 3 | 2 | -1（删 DIRECT_ISR） |
| **合计** | **~25** | **~13** | **-12（-48%）** |

### 4.2 收窄后的 `pal_irq.h` 大致骨架

```c
// —— 类型（1 个）——
typedef void (*pal_isr_t)(void *arg);

// —— 优先级（3 级）——
typedef enum {
    PAL_IRQ_PRIO_LOW,     // 一般 I/O
    PAL_IRQ_PRIO_NORMAL,  // 通信外设（默认）
    PAL_IRQ_PRIO_HIGH,    // 时间敏感（仍 RTOS-safe）
    PAL_IRQ_PRIO_COUNT
} pal_irq_prio_t;

// —— 属性宏（2 个）——
#define PAL_ISR                          // IRAM_ATTR / 空
#define PAL_DEFINE_ISR(name, T, arg)     // 类型安全宏

// —— 注册/控制（4 个）——
WINK_WARN_UNUSED_RESULT
wink_status_t pal_irq_enable(uint32_t irq_num, pal_irq_prio_t prio,
                              pal_isr_t handler, void *arg);
WINK_WARN_UNUSED_RESULT
wink_status_t pal_irq_disable(uint32_t irq_num);
void pal_irq_set_pending(uint32_t irq_num);
void pal_irq_clear_pending(uint32_t irq_num);

// —— 临界区（3 个）——
uint32_t pal_irq_save_rtos_safe(void);
void     pal_irq_restore(uint32_t mask);
#define  PAL_CRITICAL_SECTION(code)      // RAII 包裹上面两个

// —— 未来接口，尚未暴露 ——
// pal_irq_direct_connect_unsafe()  ← 真做硬件矢量直派时新增
```

`pal_irq_advanced.h`（新增，需显式 include，AI 默认不生成）：
```c
// 高级 API：普通业务代码不应使用
wink_status_t pal_irq_synchronize(uint32_t irq_num);  // hot-plug / SMP UAF 防护
uint32_t      pal_irq_save(void);                     // 全屏蔽（含 REALTIME 保留）
#define       PAL_CRITICAL_SECTION_STRICT(code)       // 走 save()
```

### 4.3 AI Codegen 视角的 before / after

**Before**（当前）：
> AI 面对"给按钮加中断"，可选：`pal_irq_enable / pal_irq_direct_connect / pal_gpio_enable_interrupt`，优先级 6 选 1，handler 类型 3 选 1，临界区 2 选 1。**组合空间 ~36 种**，其中大部分是虚标或不该用。

**After**（收窄后）：
> AI 面对同任务：`pal_gpio_enable_interrupt` 唯一入口，优先级 3 选 1，handler 类型 1 种，临界区 1 种（除非显式 include advanced）。**组合空间 ~3 种，均合法**。

---

## 5. 与 Q3 Track F 的关系

### 5.1 Track F 当前设计（仅改名）的问题

Q3 计划里 Track F 是"1 天工作量，纯 rename + doxygen 清理"。这**只解决了 `pal_irq_direct_connect` 这一个虚标点**，但同类问题（`REALTIME`、`HIGHEST`、`shared`、`STRICT`）都还在。

如果只做改名：
- 短期收益：`grep zero-latency` = 0，Track F 交付达成。
- 长期成本：**其它 5 处过度设计的存量债务全部保留**，未来还要再开 Track 处理，每次都要重新写 ADR、review、迁移。

### 5.2 建议：Track F 升级为"IRQ API 收窄"

**动作序**：

1. **写 ADR-0018「PAL IRQ 公开 API 收窄」**（0.5 天）
   - 以 ADR-0012 契约诚实 + AI Codegen 北极星为背书。
   - 逐项论证 §3.1~3.6 的删/合决策。
   - 关联本文作为触发 review。

2. **ADR Accepted 后，一次性收窄实现**（2~3 天）
   - `pal_irq.h`：删除虚标接口 + 收窄枚举 + 建 `pal_irq_advanced.h`。
   - `targets/{esp32,wasm,host}/pal_irq_*.c`：删除对应实现（约 -300 行代码）。
   - `test/test_pal_irq.c`：删除对已删 API 的测试（保留核心 enable/disable/save/restore/pending 测试）。
   - `samples/smp_uaf_test/`：目录整体移到 `samples/_deprecated/` 或删除（README 保留 git 引用）。
   - 保留 `pal_irq_direct_connect` 为 deprecated alias 一个 sprint（配合原 Track F 策略）。

3. **回写设计规范**（0.5 天）
   - `docs/design/02-wink-micro-os/02-pal-platform-abstraction.md` 中断章节全面重写。
   - `docs/tech-designs/core/pal-unified-interrupt-subsystem.md` 归档并新写 v3.0（reflect 收窄后设计）。

**工作量**：3~4 天（原 Track F 是 1 天纯改名，扩展 2~3 天做真正收窄）。

**性价比**：
- 一次性偿还 6 处过度设计债，未来不再需要"每处开一个 Track"。
- 公开面 ~-48%，AI Codegen 组合空间从 ~36 → ~3。
- 完整落实 ADR-0012 契约诚实原则（当前 ADR-0012 只在 Track F 覆盖了 `direct_connect` 一处，其它虚标点未清）。

### 5.3 如果决定"保持 Track F 只改名"

也是合理选择（用户时间/精力有限时）。但建议：
- 本文归档为"未落地评审记录"，等未来某个 sprint 再处理。
- Q3 计划补一条"IRQ API 收窄"作为 P3/未排期任务，避免遗忘。

---

## 6. 风险与迁移

### 6.1 破坏性变更清单

| 删除项 | 已知使用者 | 迁移路径 |
|-------|----------|---------|
| `pal_irq_direct_connect` | `test_pal_irq.c` | 保留 deprecated alias 一个 sprint；测试改用 `pal_irq_enable` |
| `pal_irq_shared_register` | `test_pal_irq.c` | 直接删除测试 |
| `pal_irq_synchronize` | `test_pal_irq.c` / `samples/smp_uaf_test/` | 迁到 `pal_irq_advanced.h`；测试对应 include；smp_uaf_test 归档 |
| `PAL_IRQ_PRIO_REALTIME` | `test_pal_irq.c` | 删除该级测试；如有 sample 用（无），改 HIGH |
| `PAL_IRQ_PRIO_HIGHEST` / `LOWEST` | `test_pal_irq.c` | 改为 HIGH / LOW |
| `PAL_CRITICAL_SECTION_STRICT` | `test_pal_irq.c` | 迁 advanced 头 |
| `pal_irq_save()`（全屏蔽版） | `test_pal_irq.c` | 迁 advanced 头 |

**关键事实**：DAL/samples/apps 中**零使用**这些 API。除测试代码外，业务代码不需要任何迁移。

### 6.2 潜在反对意见

| 反对 | 回应 |
|------|-----|
| "shared IRQ 未来某天可能需要" | YAGNI。真需要时通过 hardware-specific adapter 内部实现，不必公共 PAL 提供 |
| "synchronize 是 SMP 安全性关键" | 保留在 `advanced.h`，需要时可用；只是从"AI 默认可见"移出 |
| "6 级优先级已通过大量测试" | 测试证明的是"6 级枚举编译可通过"，不是"6 级在业务场景有区分价值"。业务场景反例：ESP32 上 LOWEST=LOW、HIGH=HIGHEST |
| "REALTIME 保留占位是防止未来重排 enum 值" | 收窄时把新 enum 值起始设为 0，破坏 ABI 的成本一次付清；未来加 `_UNSAFE_NMI` 时作为独立 enum 或独立接口，不与本枚举挂钩 |

### 6.3 回滚策略

- 保留 `pal_irq_direct_connect` 为 `__attribute__((deprecated))` alias 一个完整 sprint，给外部使用者迁移窗口。
- 所有删除的 API 保留 `git blame` 追溯路径。
- `docs/tech-designs/core/pal-unified-interrupt-subsystem.md` v2.x 版本作为历史文档保留（不删除）。

---

## 7. 结论

**当前 wink-micro-os IRQ 设计不是"跨芯片同一套标准 + AI Codegen 友好"两个约束下的最优解**。它是"参考 Linux/Zephyr 惯例做出的合理开局"，但在项目实际目标（AI 生成的小型嵌入式应用，主要 target 是 ESP32/STM32/Wasm）下**过度工程化**了。

具体问题：
1. **6 处虚标或过度设计**：`direct_connect` / `REALTIME` / `HIGHEST` / `LOWEST` / `shared_register` / `synchronize` / 双临界区宏。
2. **业务零使用**：这些 API 在 DAL/samples/apps 中零调用，仅 `test/test_pal_irq.c` 和元测试 `samples/smp_uaf_test/` 使用。
3. **AI Codegen 组合空间过大**：36 种组合中大部分是虚标或误用陷阱。
4. **ADR-0012 契约诚实原则落实不完整**：Track F 只处理 `direct_connect` 一处。

**建议**：Track F 从"1 天纯改名"升级为"3~4 天一次性收窄"，同时起草 ADR-0018 作为背书。收窄后：
- 公开面 -48%。
- AI Codegen 组合空间 -92%。
- 三 target 实现代码 -300 行。
- 契约诚实原则 100% 落实到 IRQ 子系统。

**次优选择**：如果精力/时间受限，Track F 保持"仅改名"计划，本文归档，等未来 sprint 再处理其余 5 处债务。但需在 Q3 计划中明确记录待偿债务，避免遗忘。


---

## 8. 架构师审查补充意见（资深架构师视角）

在对本评审方案进行深度评估后，建议在起草 `ADR-0018` 及落地实施中，补充以下四点关键架构约束与设计：

### 8.1 裸机平台（Baremetal Target）下的降级设计契约
*   **设计冲突**：收窄后仅公开 `pal_irq_save_rtos_safe()`，但在无 RTOS 的裸机环境（如 STM32 裸机适配层）下，无“Syscall 优先级”对应的硬件 BASEPRI 寄存器可配置。
*   **补充契约**：明确在裸机/无 OS Target 下，`pal_irq_save_rtos_safe()` 的底层物理实现直接等价降级为 `pal_irq_save()`（即设置 PRIMASK 寄存器以屏蔽所有可屏蔽中断）。这一降级机制必须在各 Target 适配层契约中明示，确保同源编译时应用层锁语义的连贯性。

### 8.2 GPIO 中断优先级的协同收窄与编译期检查
*   **设计冲突**：`pal_irq_enable` 优先级收窄到 3 级，但外设侧的 `pal_gpio_enable_interrupt_ex` 的优先级仍可能被传入其他已废弃枚举。另外，AI 在多 Pin 共用同一个 GPIO 中断源时，极易误配置不同的优先级从而导致运行期触发 `WINK_ERR_INVALID_ARG` 报错。
*   **补充契约**：
    1.  `pal_gpio_enable_interrupt_ex` 的优先级入参同步收窄为 `LOW / NORMAL / HIGH`。
    2.  利用 Codegen 静态分析器或 CMake 构建脚本，在**编译期**检查并拦截对同一物理中断源（如 ESP32 全局 GPIO 服务）配置不同优先级的行为，防止错误延后到运行期造成崩溃。

### 8.3 高级 API 的宏门控门槛（隔离 AI 误用）
*   **设计冲突**：高级/系统级 API（如 `pal_irq_save`、`pal_irq_synchronize`）移至新头文件 `pal_irq_advanced.h` 后，AI 仍可能根据外部知识库盲目 `#include` 该头文件，破坏“最小公开集”原则。
*   **补充契约**：在 `pal_irq_advanced.h` 头部增加编译期宏检查：
    ```c
    #ifndef WINK_ALLOW_ADVANCED_IRQ_APIS
    #error "Advanced IRQ APIs are reserved for core components. Please do not use them in application code."
    #endif
    ```
    应用层编译时默认不开启该宏，以构建强物理隔离屏障，彻底将 AI 限制在 `pal_irq.h` 提供的默认安全子集内。

### 8.4 利用收窄红利简化设备树中断描述 Schema
*   **补充契约**：取消 Shared IRQ、硬件直连通道并收窄优先级后，设备树（Device Tree）的中断绑定描述结构可大幅简化。未来 `irq_num` 升级设备树集成时，其中断属性只需指定 `<interrupt-parent>`、`<interrupt-source-id>` 及三级优先级描述，大幅降低后续 Codegen 解析器的实现复杂度。

---

*本评审已完成，等待用户决策 Track F 是否升级。若决定升级，下一步动作是起草 ADR-0018。*


