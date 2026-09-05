# PAL 中断子系统 Phase 1 重构实施计划：契约对齐（P0）

| 项 | 内容 |
|----|------|
| 创建日期 | 2026-06-30 |
| 关联评审 | [2026-06-30-pal-interrupt-subsystem-architecture-review](../../reviews/core/2026-06-30-pal-interrupt-subsystem-architecture-review.md) |
| 关联 ADR | ADR-IRQ-001 ~ ADR-IRQ-007、ADR-0004（静态分发）、ADR-0008（Device Tree） |
| 关联技术设计 | [pal-unified-interrupt-subsystem.md](../../tech-designs/core/pal-unified-interrupt-subsystem.md) v2.0 |
| 影响范围 | `pal/include/pal_irq.h`、`pal/include/hal/pal_hal.h`、`targets/{esp32,wasm,host}/`、`test/test_pal_irq.c`、`samples/devkitc_smoke/app_callbacks.c`（潜在） |
| 预计工期 | 3~5 个工作日 |
| 当前状态 | **实施完成**（G1/G2 全落地；G3 仅完成 doc-only 阶段。G3 实现落地 + host/wasm REALTIME 一致化由 [Phase 1.5 计划](2026-07-01-pal-interrupt-phase1p5-gpio-prio-enforcement-plan.md) 补齐，已于 2026-07-01 完成） |
| 风险等级 | 中（涉及 PAL 公开 API） |

---

## 0. 为什么 Phase 1 = 仅做 P0

评审 §8 列出了 P0~P3 共 11 条建议。Phase 1 聚焦**评审中标记为 P0 的三条契约欠债**（G1 / G2 / G3），不做其它优化，理由：

### 0.1 决策矩阵

| 选项 | 触及面 | 不做的后果 | 时间窗 |
|------|--------|-----------|--------|
| **P0（契约 G1/G2/G3）** | **PAL 公开 API 表面** | codegen 一旦大量调用，API 形状被冻结，未来纠正成本指数级上升 | ⏰ **现在打开但即将关闭** |
| P1（巨石 TU 拆分、共享链去重） | 内部 TU 组织 | 维护成本上升，API 不受影响 | 任意时点可做 |
| P2（host/wasm lock-level 保真） | 仿真保真度 | 同源测试覆盖率受损，但不锁死 API | 任意时点可做 |
| P3（device tree / 静态池 / baremetal） | 架构演进 | 触发条件是"接入第二款 MCU"或扩规模 | 任意时点可做 |

### 0.2 时间敏感性证据

`2026-06-30` 通过 grep 调用图确认：

```
pal_irq_direct_connect            → 0 个 sample/dal/runtime 调用者
PAL_IRQ_PRIO_REALTIME             → 0 个 sample/dal/runtime 引用者
pal_gpio_enable_interrupt         → 1 个调用（samples/devkitc_smoke/app_callbacks.c:266）
pal_gpio_enable_interrupt_ex      → 0 个外部调用
```

**当前是 API 形状最后一次低成本调整的窗口**：

- 同日（2026-06-30）创建的实施计划 `2026-06-30-pal-unified-interrupt-subsystem-implementation-plan.md` 进入 Phase 2 后，更多 sample 和 codegen 产物会接入这套 API；
- 一旦 codegen 把当前形态的 `pal_irq_direct_connect / PAL_IRQ_PRIO_REALTIME / GPIO prio` 编进数百份生成代码，再纠正契约就要写"deprecation + 迁移期"长尾路径；
- 评审 §4 指出 G1~G3 都是"承诺/实现不一致" → **隐性 bug 风险**，不是单纯的 API 美学问题。

### 0.3 P1~P3 为什么延后

| 推迟项 | 推迟理由 |
|-------|---------|
| 拆 `pal_hal_esp32.c` 巨石 | 纯结构性重构，diff 巨大，会污染本次 P0 的可读性。单独 Phase 处理。 |
| 抽 `targets/common/pal_shared_chain.c` | 同上 + 涉及 3 个 target 同时联动，宜单独验证。 |
| host/wasm lock level 区分 | 影响仿真保真，但 API 形状不变；可在 P0 完成后再做。 |
| Device Tree 化 | 需独立 ADR，且依赖 ADR-0008 实施计划；非小工程。 |

---

## 1. 任务清单

### Task 1: G1 修复 —— `pal_irq_direct_connect` 契约对齐

**问题回顾**：头文件承诺"直连中断完全绕过 PAL 软件分发逻辑，由硬件矢量控制器直接跳转"，但三个 target 的实现都退化为 `pal_irq_enable((pal_isr_t)handler, NULL)`，走 dispatch wrapper。同时 `(pal_isr_t)handler` 把 `void(*)(void)` cast 成 `void(*)(void*)`，是 CFI/UBSan 违例。

**修复方案 —— 选项 A：诚实下调契约（推荐）**

- 评审 §4 G1 给出两个选项：(A) 真接 IDF 直派 vs (B) 修订头文件契约
- 推荐 **(B)**，理由：当前没有任何调用方依赖"零延迟直派"语义；改头文件成本远小于改实现且立刻消除 cast 问题；未来若真需要硬实时直派，可以新增 `pal_irq_direct_connect_unsafe()` 之类的明确命名

**具体改动**：

1. **`pal/include/pal_irq.h`** 修订 `pal_direct_isr_t` + `pal_irq_direct_connect` 的文档：
   ```c
   /**
    * @brief 直连中断处理函数原型（无 arg 简化签名）
    *
    * ⚠️ v2.1 契约修订（2026-06-30）：
    * 当前实现仍走 PAL 软件分发 wrapper（与
    * pal_irq_enable 共用 generic_isr_wrapper）。"零延迟硬件直派"
    * 是未来 phase 的目标，当前仅作为 API 简化签名（无需 arg 的场景）。
    *
    * 真正的硬件直派会以 pal_irq_direct_connect_unsafe() 新增接口提供，
    * 届时此接口不变。
    */
   typedef void (*pal_direct_isr_t)(void);
   ```
2. **三个 target 的 `pal_irq_direct_connect`** 改成显式 trampoline，消除 cast：
   ```c
   /* 用文件级静态全局数组保存裸 direct handler，trampoline 桥接 (void*) → ()。
    * 避免 (pal_isr_t)direct_handler 这种 CFI/UBSan 违例 cast。
    * ⚠️ 严禁使用 thread_local/__thread，因中断上下文中无法安全访问 TLS 变量。*/
   static pal_direct_isr_t s_direct_handlers[N_IRQ];

   static void PAL_ISR direct_trampoline(void *arg) {
       uint32_t irq = (uint32_t)(uintptr_t)arg;
       if (irq < N_IRQ && s_direct_handlers[irq] != NULL) {
           s_direct_handlers[irq]();
       }
   }

   wink_status_t pal_irq_direct_connect(uint32_t irq, pal_direct_isr_t h) {
       if (irq >= N_IRQ || h == NULL) return WINK_ERR_INVALID_ARG;
       s_direct_handlers[irq] = h;
       return pal_irq_enable(irq, PAL_IRQ_PRIO_NORMAL, direct_trampoline,
                              (void *)(uintptr_t)irq);
   }
   ```
3. **三个 target 的 `pal_irq_disable`** 同步清理直接中断指针：
   - 禁用中断时，必须同步将 `s_direct_handlers[irq_num] = NULL` 清空，防止悬挂指针。
4. **tech-design** 章节"双通道路径"加注 v2.1 修订说明，记录这个契约下调。

**验收**：

- ✅ 头文件 `(pal_isr_t)handler` cast 在三个 target 中消失
- ✅ `clang -fsanitize=cfi-icall`（host build）不再报警
- ✅ `test_pal_irq.c` 新增一个直连 ISR 调用测试，验证签名路径

**预计工时**：0.5 天

---

### Task 2: G2 修复 —— ESP32 `PAL_IRQ_PRIO_REALTIME` 拒接

**问题回顾**：头文件承诺 REALTIME 是"硬实时 Non-RTOS-safe"级别，但 ESP32 把它静默映射到 `ESP_INTR_FLAG_LEVEL3` —— 与 HIGHEST 物理上等价。**同一物理优先级、相反契约**，掩盖跨平台 bug。

**修复方案**：

ESP32 路径上对 `PAL_IRQ_PRIO_REALTIME` 直接返回 `WINK_ERR_UNSUPPORTED`，**不允许静默降级**。在 WASM/Host 上虽然保留映射（单线程下无硬件级实时危害），但加 doxygen 注解说明“真机上不可用”，并在 Host/WASM 注册 `REALTIME` 时打印强烈的警告日志（或执行断言），避免仿真环境静默掩盖未来真机部署的拒接错误。

**具体改动**：

1. **`targets/esp32/pal_hal_esp32.c`** 的优先级映射表前置检查：
   ```c
   wink_status_t pal_irq_enable(uint32_t irq_num, pal_irq_prio_t prio, ...) {
       if (irq_num >= 32 || handler == NULL || prio >= PAL_IRQ_PRIO_COUNT) {
           return WINK_ERR_INVALID_ARG;
       }
       /* ⚠️ ESP32 不支持 REALTIME（NMI 级 C-ISR 不可注册）。
        * 不允许静默降级，避免掩盖跨平台契约违反。*/
       if (prio == PAL_IRQ_PRIO_REALTIME) {
           return WINK_ERR_UNSUPPORTED;
       }
       /* ...继续映射 LOWEST~HIGHEST... */
   }
   ```
2. **映射表删除 `[PAL_IRQ_PRIO_REALTIME] = ESP_INTR_FLAG_LEVEL3`** 项（不再需要）。
3. **`pal/include/pal_irq.h`** 的 REALTIME 注释补一行：
   ```c
   PAL_IRQ_PRIO_REALTIME = 5,  /**< ⚠️ 非 RTOS 安全！极端硬实时场景专用
                                        ESP32 当前不支持（返回 WINK_ERR_UNSUPPORTED），
                                        STM32 NMI / 裸机直连 IRQ 才可用 */
   ```
4. **`pal_gpio_enable_interrupt_ex`** 同样路径补 REALTIME 拒接（一致性）。

**验收**：

- ✅ `pal_irq_enable(irq, PAL_IRQ_PRIO_REALTIME, h, a)` 在 ESP32 target 上返回 `WINK_ERR_UNSUPPORTED`
- ✅ Host/WASM 上仍可注册成功（保持单线程下的"等价行为"）
- ✅ `test_pal_irq.c` 新增 negative test 验证 ESP32 上的拒接行为

**预计工时**：0.5 天

---

### Task 3: G3 修复 —— GPIO 优先级参数的诚实处理

**问题回顾**：`pal_gpio_enable_interrupt_ex` 接 `pal_irq_prio_t prio` 参数，但三个 target 全部 `(void)prio` 静默丢弃，注释写"预留用于未来扩展"。当 codegen 期望按钮中断高于传感器中断时，**抽象在所有 target 上都失效**。

**修复方案 —— 选项 B：诚实下调契约（推荐）**

评审 §8 给出选项：(A) 真实现 per-pin 优先级 vs (B) 头文件明示全局优先级。

推荐 **(B)**，理由：
- ESP-IDF 的 GPIO ISR service 模型本身就是"全局优先级 + 单一 dispatch"，per-pin 优先级需要把每个 pin 单独 `esp_intr_alloc` 注册成独立 source，代价巨大且收益有限（大部分 codegen 场景不依赖此细粒度）
- 在 v2.1 头文件明示后，未来若真需要可以新增 `pal_gpio_enable_interrupt_dedicated()` 走独立 source 路径，不破坏现有 API

**具体改动**：

1. **`pal/include/hal/pal_hal.h`** 的 `pal_gpio_enable_interrupt_ex` 注释：
   ```c
   /**
    * @brief 启用 GPIO 引脚中断（扩展版，支持指定优先级）
    *
    * ⚠️ v2.1 契约修订（2026-06-30）：
    * GPIO ISR 在所有 target 上共享一个 dispatch service，prio 参数仅
    * 在首次初始化 service 时生效（决定整个 GPIO 中断源的硬件优先级）；
    * 后续注册时 prio 参数被忽略，所有 pin 共享同一优先级。
    *
    * 若需要 per-pin 独立优先级（如按钮 HIGH + 传感器 NORMAL 抢占），
    * 未来 Phase 会新增 pal_gpio_enable_interrupt_dedicated() 走独立
    * 中断源路径，届时此接口语义不变。
    *
    * @param prio  首次注册时生效；后续注册若与首次不一致，返回 WINK_ERR_INVALID_ARG
    */
   ```

2. **`targets/esp32/pal_hal_esp32.c::pal_gpio_enable_interrupt_ex`**：
   - 删除 `(void)prio;`
   - 新增静态布尔变量 `s_gpio_service_initialized` 和静态优先级变量 `s_gpio_service_prio`。
   - 注意：非 ex 接口 `pal_gpio_enable_interrupt` 会默认传入 `PAL_IRQ_PRIO_NORMAL`，这也将触发首次初始化逻辑。需要统一受 `s_gpio_service_initialized` 状态变量保护。
   - 第二次以后的注册：若 prio 与记录值不一致 → 返回 `WINK_ERR_INVALID_ARG`（参数冲突错误，而非 `WINK_ERR_BUSY`，以此对齐契约诚实原则）。
   - 在首次初始化调用 `gpio_install_isr_service(prio_map[prio])` 时传入优先级对应的 flag，并严格检查其返回值。

3. **`targets/wasm/pal_hal_wasm.c`** 与 **`targets/host/pal_hal_host.c`** 做相同的"首次记录 + 不一致拒接 `WINK_ERR_INVALID_ARG`"逻辑，保持跨 target 行为一致（仿真保真）。

4. **更新 `samples/devkitc_smoke/app_callbacks.c:266`** 唯一调用点：明确传 `PAL_IRQ_PRIO_NORMAL`（已是默认值，确认即可）。

**验收**：

- ✅ 三个 target 上的 GPIO 中断注册都遵循"首次决定优先级，后续若改 prio 则返回 `WINK_ERR_INVALID_ARG`"
- ✅ ESP32 上 `gpio_install_isr_service` 的 flag 真正使用映射后的优先级（而非 hardcoded `0`）
- ✅ `test_pal_irq.c` 新增 prio 一致性测试：第二次以不同 prio 注册不同 pin 必须返回 `WINK_ERR_INVALID_ARG`

**预计工时**：1 天

---

### Task 4: 文档与契约文件同步

**问题**：评审 §6 M4 指出 v2.0 tech-design / ADR-IRQ 文档与代码同步落地中。本次三处契约修订必须**立刻**回写设计规范（CLAUDE.md "决策结论回写" 原则）。

**具体改动**：

1. **`docs/tech-designs/core/pal-unified-interrupt-subsystem.md`** 加 v2.1 修订说明：
   - 章节 "双通道路径" 标注 G1 修订（direct_connect 当前仍为软分发）
   - 章节 "优先级抽象" 标注 G2 修订（ESP32 REALTIME 拒接而非降级）
   - 章节 "GPIO 中断接口" 标注 G3 修订（per-pin 共享 service 优先级）
   - 顶部元数据表追加版本：`| 文档版本 | v2.1（契约对齐修订版）|`
   - 文末 "11. 核心架构决策记录" 加入：`ADR-IRQ-008（v2.1）：契约诚实化 —— 静默降级 → 显式拒接`

2. **新增 ADR**（可选，按本次修订重要性判断）：
   - 评估是否值得为"contract honesty over silent degradation"单独立 ADR-0012。若立，按 `.claude/rules/docs-adr.md` §3 模板写，状态从 Proposed 起步。
   - **判定标准**：若 P0 三条修复的"诚实化"是项目工程文化的长期决策，立 ADR；若只是当前 phase 的局部调整，不立。
   - **推荐立 ADR**，因为"契约诚实优于静默降级"在嵌入式跨平台 PAL 设计中是反复出现的决策点（已经从 G1~G3 三处遇到了），值得文档化为通用原则。

3. **更新 `2026-06-30-pal-unified-interrupt-subsystem-implementation-plan.md`** 添加 Phase 1.5 链接，指向本计划。

**预计工时**：1 天

---

### Task 5: 回归与验证

每条 P0 修复都对应一组 unit test 增量；除此之外，验证整个 PAL IRQ 子系统在三个 target 上没有回归。

**具体动作**：

1. **`test/test_pal_irq.c`** 增加：
   - `test_direct_connect_calls_handler`：注册直连 → set_pending → 验证 handler 被调用
   - `test_realtime_priority_rejected_on_esp32`（条件编译 `#if defined(ESP_PLATFORM)`，host build skip）
   - `test_gpio_prio_mismatch_invalid_arg`：第二个 pin 注册不同 prio 必须返回 `WINK_ERR_INVALID_ARG`
   - `test_realtime_accepted_on_host`：host build 下 REALTIME 注册成功（但需验证在 stdout 产生警告日志，或验证其 doxygen 注解限制）
2. **运行**：
   - host 单测全量：`cmake --build build-host && ctest`
   - WASM 烟测：`build-wasm` 后跑 `smoke` sample
   - ESP32 真机烟测：在 DevKitC 上跑 `smp_uaf_test` + `devkitc_smoke`，验证按钮中断仍工作（G3 修复后整个 GPIO 共有中断源的优先级得以根据配置配置，需仔细回归）
3. **CI 把关**：在 Host 单测编译中强制开启 `-fsanitize=cfi-icall` 验证，确保没有 `(pal_isr_t)` 类型的强制转换导致的 CFI 违例。

**验收**：

- ✅ `ctest` 全绿
- ✅ ESP32 真机 `devkitc_smoke` 按钮 ISR 行为不变
- ✅ ESP32 真机 `smp_uaf_test` UAF 防护行为不变
- ✅ 三个新增 unit test 全部通过

**预计工时**：1~2 天

---

## 2. 时间线

| 工作日 | Task | 输出 |
|--------|------|------|
| Day 1 上午 | Task 1（G1 direct_connect） | trampoline 实现 + 头文件契约更新 |
| Day 1 下午 | Task 2（G2 REALTIME 拒接） | ESP32 拒接 + 头文件注释 |
| Day 2 全天 | Task 3（G3 GPIO prio） | 三 target 一致行为 + sample 验证 |
| Day 3 上午 | Task 4（文档同步） | tech-design v2.1 + 可选 ADR-0012 |
| Day 3 下午 ~ Day 4 | Task 5（回归验证） | 单测 + 真机烟测 |
| Day 5 | 评审 + 提交 | PR with full test evidence |

---

## 3. 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| G3 修复改动 `gpio_install_isr_service` flag，引入 ESP32 上 GPIO ISR 优先级变更 → 干扰现有 sample | 中 | 中 | Task 5 增加 `devkitc_smoke` 真机回归；首次注册的 prio 若不显式传，默认值 `NORMAL` 即原行为 |
| G1 trampoline 增加一次间接跳转 → REALTIME-class 用户体感延迟变大 | 低 | 低 | 当前无 REALTIME 用户；trampoline 仅 4~6 指令开销 |
| 拒接 REALTIME 后未来真有 STM32 接入硬实时场景 | 低 | 低 | 接 STM32 时新增 `pal_irq_direct_connect_unsafe()` 路径，不影响本次 API |
| 同时修订三个 target + 文档，diff 庞大难 review | 中 | 中 | 拆 4 个独立 commit：G1 / G2 / G3 / docs；按 CLAUDE.md 原子提交原则 |
| ADR-0012 是否值得立的判断错误 | 低 | 低 | 推荐立但允许 Proposed 状态等评审；不立也不影响代码修改 |

---

## 4. 不在本计划范围（明确边界）

以下内容**不做**，归入未来 Phase：

- ❌ 拆 `pal_hal_esp32.c` 巨石（→ Phase 2 P1）
- ❌ 抽 `targets/common/pal_shared_chain.c` 去重（→ Phase 2 P1）
- ❌ 消除 `targets/esp32/*.c` 内的 `#if defined(ESP_PLATFORM)` 散落（→ Phase 2 P1）
- ❌ Host/WASM 区分 lock level（→ Phase 3 P2）
- ❌ `smp_uaf_test` 标注 WASM vacuous（→ Phase 3 P2，仅 sample 注释，几分钟事）
- ❌ Device Tree 化 `irq_num`（→ Phase 4 P3，依赖 ADR-0008 实施）
- ❌ 共享链 `malloc` → 静态池（→ Phase 4 P3，依赖整体内存策略 ADR）
- ❌ baremetal IRQ 实现或拒接（→ Phase 4 P3，触发条件为接入裸机 MCU）

---

## 5. 验收标准（DoD - Definition of Done）

**Phase 1 完成的标志**：

1. ✅ 评审报告 §4 G1 / G2 / G3 三项全部修复并通过测试
2. ✅ `pal_irq.h` 与 `pal_hal.h` 的所有 ISR 相关 doxygen 注释与实现 100% 一致
3. ✅ 三个 target 上的 ISR API 在跨平台调用语义上的差异被显式 doc 或 显式拒接，不再有"静默降级"
4. ✅ tech-design v2.1 完成 + 实施计划链接更新 + （可选）ADR-0012 进入 Proposed
5. ✅ Host 单测 16 → 至少 20 个用例，新增覆盖全部通过
6. ✅ ESP32 真机 `devkitc_smoke` + `smp_uaf_test` 回归通过
7. ✅ 评审报告 §0 "落地完成度" 从 ⭐⭐½ → ⭐⭐⭐½（契约对齐到位）
8. ✅ 没有任何 `(pal_isr_t)` 类型的强制类型转换留在 `pal_irq_direct_connect` 相关的任何 target 实现中，且 Host 开启 CFI 编译验证无违例
9. ✅ 测量并验证 `pal_irq_direct_connect` 引入的 trampoline 指令延迟开销，确保其在纳秒级，不破坏硬实时承诺

---

## 6. Phase 2 预告

Phase 1 完成后，**契约表面冻结**，可以安全推进 Phase 2 内部重构：

- **Phase 2 候选项**（按价值排序）：
  1. 拆 `pal_hal_esp32.c` 巨石（M3）
  2. 抽 `targets/common/pal_shared_chain.c` 去重（M1）
  3. 清理 `#if defined(ESP_PLATFORM)` 散落（M2）

Phase 2 计划在 Phase 1 提交后另起独立 implementation-plan。

