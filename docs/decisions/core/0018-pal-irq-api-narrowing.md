# ADR-0018：PAL 中断 API 收窄（面向 AI Codegen 的最小完备集）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-02（提议），2026-07-02（采纳） |
| 触发 | [2026-07-02 PAL 中断 API 收窄评审](../../reviews/core/2026-07-02-pal-irq-api-narrowing-review.md) |
| 影响范围 | `pal/include/pal_irq.h`，各 Target 适配层（esp32/wasm/host），测试代码与 sample |
| 决策者 | 架构委员会 & 用户 |
| 关联评审 | [2026-07-02-pal-irq-api-narrowing-review](../../reviews/core/2026-07-02-pal-irq-api-narrowing-review.md) |
| 关联实施计划 | [PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3](../../implementation-plans/core/2026-07-01-wmos-code-optimization-q3-plan.md) Track F |
| 关联既有 ADR | [ADR-0002 双 target 同源编译](../unisim/0002-dual-target-compilation.md), [ADR-0004 静态分发](0004-static-dispatch-vs-runtime-ops.md), [ADR-0012 契约诚实](0012-contract-honesty-over-silent-degradation.md) |
| 关联设计规范 | [02-pal-platform-abstraction.md §3.3](../../zh/design/02-wink-micro-os/02-pal-platform-abstraction.md#33-pal-irq-公开面收窄adr-0018)（已回写） |

---

## 背景（Context）

WinkMicroOS 的核心愿景是作为“面向 AI 生成嵌入式应用的开发平台”。在当前的 v2.2 设计中，中断抽象层（`pal_irq.h`）公开了约 25 个符号，组合空间约 36 种。这带来了两个严重的架构隐患：

1. **过度工程与虚标契约（违反 ADR-0012）**：
   - `pal_irq_direct_connect` 暗示“硬件矢量直连/零延迟”，但在 ESP32/Host/Wasm 上实际均退化为带 Trampoline 包装的软件分发。这不仅违背契约诚实，还导致 `(pal_isr_t)handler` 的类型强转，在开启控制流完整性（CFI）编译时会触发未定义行为崩溃。
   - `pal_irq_shared_register` 提供 RCU 责任链共享中断，参考了 Linux Shared IRQ。但这在微控制器微应用中需求为 0，且强行引入了运行期 `malloc` 依赖，违反了 ADR-0004 的静态分配原则。
   - `PAL_IRQ_PRIO_REALTIME` 在 ESP32/Host/Wasm 上均被显式或默认拒接（返 `WINK_ERR_UNSUPPORTED`），属于虚标枚举。

2. **AI Codegen 状态空间过大（AI 误用陷阱）**：
   - **多级优先级混淆**：6 级优先级在 ESP32 上退化映射为 3 级（`LOWEST=LOW=LEVEL1`, `HIGH=HIGHEST=LEVEL3`）。物理等价但契约不同的别名极大增加了 AI 误判概率。
   - **临界区滥用**：AI 无法可靠地区分 `pal_irq_save()`（屏蔽所有中断）与 `pal_irq_save_rtos_safe()`（仅屏蔽 RTOS 级）。AI 倾向于生成“更安全”的 `pal_irq_save()` 或 `PAL_CRITICAL_SECTION_STRICT`，这在 ESP32 上会因屏蔽 Wi-Fi 基带中断而直接导致通信时序崩溃或触发硬件看门狗复位。

---

## 方案比选（Options）

### 选项 A：仅做函数改名（原 Q3 计划 Track F 规划）
*   **做法**：仅将 `pal_irq_direct_connect` 改名为 `pal_irq_iram_bind`，修改 Doxygen 去掉“零延迟”承诺。保留其余所有 6 级优先级、共享中断、全屏蔽锁等设计。
*   **优点**：重构成本极低（1 天）。
*   **缺点**：未能解决其他 5 处虚标与过度工程债务，AI Codegen 依然面临极高误用风险。

### 选项 B：全面收窄 API 表面 + 高级 API 物理隔离（推荐）
*   **做法**：
    1. **合并回调原型**：只保留统一的 `pal_isr_t (void *arg)`，彻底删除 `pal_direct_isr_t` 和 `pal_irq_shared_handler_t`，消除 CFI/UBSan 强转隐患。
    2. **收窄优先级枚举**：缩减至 3 级（`LOW` / `NORMAL` / `HIGH`），去除别名与虚标的 `REALTIME`。
    3. **删除 Shared IRQ**：废除 `pal_irq_shared_register` 及其 RCU 链表实现，释放在所有 Target 下的 `malloc` 动作。
    4. **物理隔离高级锁**：新建 `pal_irq_advanced.h`，将 `pal_irq_synchronize`、`pal_irq_save`、`PAL_CRITICAL_SECTION_STRICT` 移入该文件，并使用编译期宏门控进行保护，防止 AI 误用。
    5. **外设对齐**：GPIO 层 `pal_gpio_enable_interrupt_ex` 的优先级入参同步收窄为 3 级。
    6. **定义裸机降级契约**：明确在 Baremetal 裸机下，RTOS 安全锁降级为全屏蔽锁。
*   **优点**：
    - 公开 API 表面收窄 48%，AI Codegen 组合空间缩减 92%（从 ~36 降至 ~3）。
    - 彻底兑现 ADR-0012 契约诚实原则。
    - 移除运行期 `malloc`，代码量缩减约 300 行。
*   **缺点**：
    - 具有破坏性（虽然经过 Grep 确认 DAL/App 层直接调用为 0，但需要重构 `test_pal_irq.c` 和废弃 `smp_uaf_test`）。
    - 工作量升至 3~4 天。

---

## 决策结论（Decision）

采纳 **选项 B**（全面收窄 API 表面 + 高级 API 物理隔离）。

### 落地规则

#### 1. 统一回调签名
废除 `pal_direct_isr_t` 与 `pal_irq_shared_handler_t`，所有的中断注册接口强制使用 [pal_isr_t](../../../wink-micro-os/pal/include/pal_irq.h#L97)：
```c
typedef void (*pal_isr_t)(void *arg);
```

#### 2. 收窄优先级枚举
将 [pal_irq_prio_t](../../../wink-micro-os/pal/include/pal_irq.h#L57) 收窄为 3 级，去除冗余和不支持的物理映射：
```c
typedef enum {
    PAL_IRQ_PRIO_LOW    = 0,  /**< 一般 I/O 与慢速外设 */
    PAL_IRQ_PRIO_NORMAL = 1,  /**< 通信外设（默认值） */
    PAL_IRQ_PRIO_HIGH   = 2,  /**< 时间敏感外设（仍为 RTOS 安全，可调 FromISR） */
    PAL_IRQ_PRIO_COUNT
} pal_irq_prio_t;
```
*   **ESP32 映射**：LOW -> LEVEL1, NORMAL -> LEVEL2, HIGH -> LEVEL3。
*   **STM32 映射**：直接映射到 NVIC 对应的三个硬件优先级优先级区间（均位于 RTOS 临界区边界内）。
*   **Host/Wasm**：仅用于仿真事件的调度优先级。

#### 3. 外设接口对齐
[pal_gpio_enable_interrupt_ex](../../../wink-micro-os/pal/include/hal/pal_hal.h#L122) 的 `prio` 参数同步采用收窄后的 3 级优先级。

#### 4. 删除共享中断（Shared IRQ）
删除 `pal_irq_shared_register`。移除三 target 平台下的 `pal_shared_chain` 分发和动态链表算法实现（对应 `targets/common/src/pal_shared_chain.c` 废弃）。

#### 5. 高级/系统级 API 物理隔离
创建新文件 `pal_irq_advanced.h`，将非通用业务所需的 API 迁出 `pal_irq.h`。并在头部施加宏编译卡口：
```c
#ifndef WINK_ALLOW_ADVANCED_IRQ_APIS
#error "Advanced IRQ APIs are reserved for core drivers. Do not include this file in application code."
#endif
```
受保护并迁移的 API 包含：
- [pal_irq_synchronize](../../../wink-micro-os/pal/include/pal_irq.h#L283)（用于 SMP 下资源热释放同步）
- [pal_irq_save](../../../wink-micro-os/pal/include/pal_irq.h#L313)（全屏蔽关中断锁）
- [PAL_CRITICAL_SECTION_STRICT](../../../wink-micro-os/pal/include/pal_irq.h#L366)（全屏蔽 RAII 临界区）

应用层唯一公开的临界区保护接口为 `PAL_CRITICAL_SECTION`（基于 RTOS 安全锁）。

#### 6. 统一裸机（Baremetal）平台降级契约
在 Baremetal（裸机/无 RTOS）平台下，[pal_irq_save_rtos_safe](../../../wink-micro-os/pal/include/pal_irq.h#L331) 降级等价实现为 `__disable_irq()`（设置 PRIMASK 寄存器全屏蔽）。以保证锁接口在跨 target 编译时的物理完整性。

---

## 后果与约束（Consequences & Constraints）

### 正面后果
- **AI 误用率显著降低**：公开的临界区宏只剩 1 种（`PAL_CRITICAL_SECTION`），优先级剩下 3 种，AI Codegen 状态空间被极大压缩，杜绝了“屏蔽所有中断导致真机看门狗复位”的隐患。
- **消灭内存泄露与堆依赖**：废除 Shared IRQ 移成了中断注册路径上的 `malloc` 动作，彻底净化了 HAL 层的内存模型，完美遵循 ADR-0004（静态分发与内存分配）。
- **零外部影响**：经 Grep 核验，所有的 DAL 驱动和业务 App 中没有一处直接调用 `pal_irq_direct_connect`、`pal_irq_shared_register` 和 `pal_irq_synchronize`。业务迁移成本为 0。
- **设备树 Schema 简化**：优先级数量缩减及直连通道废除，使得未来 `irq_num` 升级设备树集成时的 Schema 极其干净（只需指定物理源 ID 和 3 级优先级描述字符串），降低了 Codegen 解析器的实现复杂度。

### 负面后果与约束
- **测试代码需要重构**：`test_pal_irq.c` 中针对共享中断和多级优先级的用例需要删除或修改。
- **SMP 验证 Sample 归档**：由于 `pal_irq_synchronize` 迁入高级头文件，且业务无热插拔中断的需求，验证 SMP UAF 的示例 `samples/smp_uaf_test/` 整体归档/废弃。

---

## 遵循与后续（Compliance & Follow-up）

1. **执行 Track F 升级计划**（3 天工作量）：
   - 修改 `pal_irq.h`，新建 `pal_irq_advanced.h` 并加入宏隔离。
   - 彻底移除 `pal_shared_chain.c` 及 target 的相关实现（esp32/wasm/host）。
   - 调整 GPIO 中断优先级的映射及校验逻辑。
   - 清理重构 `test_pal_irq.c`。
2. **SSOT 设计规范回写**：
   - 决策 Accepted 后，回写并更新 [02-pal-platform-abstraction.md](../../zh/design/02-wink-micro-os/02-pal-platform-abstraction.md) 中的中断与临界区小节。
3. **Codegen 提示词规整**：
   - 如果未来接入 Codegen 规则库，在 Few-shot 中更新“禁止 `#include "pal_irq_advanced.h"`”及 3 级中断优先级使用指南。

---

*本 ADR 状态变更记录:*
- 2026-07-02: Proposed（伴随 Track F 方案升级提出，等待用户 review 并 Accepted）
- 2026-07-02: Accepted（用户 review 通过 + 代码 landed：`b75f8d8` ADR 草案 + `48fb60d` 全量收窄实现）
  - 回写 SSOT `02-pal-platform-abstraction.md §3.3`；
  - 归档 `tech-designs/pal-unified-interrupt-subsystem.md` 为 v2.x 历史；
  - `python wink-tools/wink.py test` -Optin pass 退役（伴随 `WINK_HOST_ALLOW_REALTIME_FOR_TESTING` 无消费者）；
  - `.claude/skills/burn-firmware-esp32/SKILL.md` 移除 smp_uaf_test；
  - ESP32 GPIO ISR 路径 `pal_hal_esp32_gpio.c` 后续修正（引用了已删除的 `PAL_IRQ_PRIO_REALTIME/LOWEST/HIGHEST`，属 Track F 实现遗漏，已一并修）。

