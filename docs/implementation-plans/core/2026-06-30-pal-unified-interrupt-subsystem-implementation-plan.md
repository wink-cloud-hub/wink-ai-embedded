# PAL 统一中断子系统实施计划

> ## ⚠️ 本计划已由 ADR-0018 部分作废（2026-07-02）
>
> **状态**：**v2.x 历史归档 — 仅保留作为演进溯源，不应再作为待办**。
>
> - ✅ **Phase 0 / Phase 1 / Phase 1.5**：已通过独立子计划落地。
>   - [2026-06-30-pal-interrupt-phase1-contract-alignment-plan.md](./2026-06-30-pal-interrupt-phase1-contract-alignment-plan.md)（Task 1/2 全落地；Task 3 doc-only 部分完成）
>   - [2026-07-01-pal-interrupt-phase1p5-gpio-prio-enforcement-plan.md](2026-07-01-pal-interrupt-phase1p5-gpio-prio-enforcement-plan.md)（Phase 1.5 G3 补齐 + host/wasm REALTIME 一致化，2026-07-01 完成）
> - ❌ **Phase 2 ESP32 完整实现 / Phase 3 WASM 仿真实现 / Phase 6 中断共享机制**：
>   [ADR-0018 PAL 中断 API 收窄](../../decisions/core/0018-pal-irq-api-narrowing.md)（2026-07-02 Accepted）删除了 `pal_irq_shared_register`、共享中断链表、RCU 实现、`direct_connect`、`REALTIME/HIGHEST/LOWEST` 优先级、`smp_uaf_test` sample。上述 Phase 中的对应任务**已取消**。
> - ➡️ 后续并发验证覆盖由 [`2026-07-02-concurrency-stress-sample-design.md`](../../tech-designs/unisim/2026-07-02-concurrency-stress-sample-design.md) 补位（实施计划待写）。
> - ➡️ 关联 tech-design [`pal-unified-interrupt-subsystem.md`](../../tech-designs/core/pal-unified-interrupt-subsystem.md) 亦已标 v2.x 历史归档。
>
> 下文 Phase 2+ 的原始文本按史料保留，请勿据此启动新工作。

| 项 | 值 |
|----|----|
| **计划日期** | 2026-06-30 |
| **最后修订** | 2026-07-03（状态回填：部分作废） |
| **关联设计文档** | `tech-designs/pal-unified-interrupt-subsystem.md` (**v2.x 历史归档**) |
| **关联 ADR** | ADR-0002, ADR-0008, ADR-0010, ADR-0012, **ADR-0018（收窄 API，废止 Phase 2/3/6 共享中断轨）** |
| **后续 Phase 1.5** | [2026-06-30-pal-interrupt-phase1-contract-alignment-plan](2026-06-30-pal-interrupt-phase1-contract-alignment-plan.md) —— 已完成 |
| **预计工期** | ~~**13.5 天**~~（已作废，不再执行） |
| **状态** | ⚠️ **部分作废** — Phase 0/1/1.5 已由独立子计划落地；Phase 2/3/6 因 ADR-0018 取消 |

---

## 目录

1. [概述与目标](#1-概述与目标)
2. [Phase 0: 准备与验证](#2-phase-0-准备与验证05-天)
3. [Phase 1: 接口定义 + 低风险重构](#3-phase-1-接口定义--低风险重构1-天)
4. [Phase 2: ESP32 完整实现](#4-phase-2-esp32-完整实现2-天)
5. [Phase 3: WASM 仿真实现](#5-phase-3-wasm-仿真实现2-天)
6. [Phase 4: Host 单元测试支持](#6-phase-4-host-单元测试支持1-天)
7. [Phase 5: Device Tree 集成](#7-phase-5-device-tree-集成3-天)
8. [Phase 6: 中断共享机制](#8-phase-6-中断共享机制2-天)
9. [Phase 7: 文档 + 推广](#9-phase-7-文档--推广1-天)
10. [风险与应对](#10-风险与应对)
11. [里程碑](#11-里程碑)
12. [回滚计划](#12-回滚计划)

---

## 1. 概述与目标

### 1.1 项目背景

当前 `devkitc_smoke` 示例中，ISR 业务逻辑被 `#ifdef ESP_PLATFORM` 包裹，破坏了双目标同源编译的架构原则。本项目通过构建 PAL 层统一中断抽象，实现 APP 代码零平台宏，同时提供中断锁语义统一、优先级抽象、硬件直连等高级功能。

### 1.2 核心交付目标

| 目标 | 量化指标 |
|------|---------|
| **同源编译** | APP 代码零 `#ifdef ESP_PLATFORM` |
| **双平台一致性** | WASM 仿真与 ESP32 真机中断时序偏差 < 1 tick |
| **实时性保证** | GPIO 中断响应延迟 < 1μs (@240MHz) |
| **测试覆盖率** | Host 单元测试覆盖率 > 90% |
| **内存开销** | 向量表 + 分发表静态内存 < 1KB |

### 1.3 参考文档

- 技术设计规格：`tech-designs/pal-unified-interrupt-subsystem.md`
- 编码规范：`.claude/rules/c-code.md`
- 文档规范：`.claude/rules/docs-adr.md`

---

## 2. Phase 0: 准备与验证（0.5 天）

**目标**：完成专家评审，确认 v2.0 架构方案，搭建验证基础设施

### 任务清单

- [x] ✅ **已完成**：设计文档专家评审并通过（10 项建议全部采纳）
- [x] ✅ **已完成**：整合所有专家建议，更新设计文档为 **v2.0**
- [ ] 梳理现有中断相关代码，输出《影响范围分析报告》
  - 现有 `pal_gpio_enable_interrupt()` 调用点
  - 现有 `boot_button_isr` 及其他 ISR 位置
  - 现有 `pal_irq_save/restore` 使用场景
- [ ] 搭建静态检查规则骨架（Clang-Tidy 配置）
- [ ] 确认重构边界：需修改文件清单（v2.0 扩展版）
  - `pal/include/pal_irq.h` （新增，v2.0 接口）
  - `pal/include/hal/pal_hal.h` （扩展）
  - `targets/esp32/pal_irq_esp32.c` （新增，含自旋锁）
  - `targets/wasm/pal_irq_wasm.c` （新增，Pareto 延迟模型）
  - `targets/host/pal_irq_host.c` （新增）
  - `examples/devkitc_smoke/app_callbacks.c` （去宏）

### 交付物

- 设计文档 **v2.0（专家评审版）** 已通过评审
- 新增 4 个 ADR 决策记录（SMP 竞态、共享中断语义、双等级锁、synchronize 原语）
- 《影响范围分析报告》
- Clang-Tidy 规则骨架配置

### 验收标准

- [ ] 所有相关工程师确认 v2.0 架构方案（含 SMP 安全、RCU、双等级锁）
- [ ] 明确重构边界和 SMP 相关风险点
- [ ] 静态检查框架可运行

---

## 3. Phase 1: 接口定义 + 低风险重构（1 天）

**目标**：不改变现有功能，消除 APP 层的 `#ifdef ESP_PLATFORM`

### 任务清单

#### 3.1 新增 `pal/include/pal_irq.h` 头文件（v2.0 扩展版）

- [ ] 定义 `pal_irq_prio_t` 枚举（**6 级优先级，v2.0 新增 REALTIME 级**）
  ```c
  typedef enum {
      PAL_IRQ_PRIO_LOWEST   = 0,
      PAL_IRQ_PRIO_LOW      = 1,
      PAL_IRQ_PRIO_NORMAL   = 2,
      PAL_IRQ_PRIO_HIGH     = 3,
      PAL_IRQ_PRIO_HIGHEST  = 4,    // FreeRTOS 安全边界
      PAL_IRQ_PRIO_REALTIME = 5,    // ⚠️ v2.0 新增：硬实时级，严禁调用 RTOS API
      PAL_IRQ_PRIO_COUNT
  } pal_irq_prio_t;
  ```
- [ ] 定义 ISR 函数类型（v2.0 修正语义）
  - `pal_isr_t` - 分发型回调（带 `void *arg`）
  - `pal_direct_isr_t` - 直连型空函数指针（无参数）
  - `pal_irq_shared_handler_t` - 共享中断 handler（返回 bool 仅用于统计，**不终止遍历**）
- [ ] 定义属性注解宏
  - `PAL_ISR` - ESP32 展开为 `IRAM_ATTR`，其他平台为空
  - `PAL_DIRECT_ISR` - 同 `PAL_ISR`
  - `PAL_DEFINE_ISR` - 类型安全宏，自动生成类型转换包装
- [ ] 声明逻辑中断控制器核心接口（v2.0 扩展）
  - `pal_irq_enable()` / `pal_irq_disable()`
  - `pal_irq_set_pending()` / `pal_irq_clear_pending()`
  - **`pal_irq_synchronize()`** - ⭐ v2.0 新增：SMP 下等待 ISR 执行完成
- [ ] 声明硬件直连中断接口
  - `pal_irq_direct_connect()`
- [ ] 声明共享中断注册接口
  - `pal_irq_shared_register()`
- [ ] 声明全局中断锁接口（**v2.0 双等级语义**）
  - `pal_irq_save()` - 全屏蔽，临界区必须 < 1µs
  - **`pal_irq_save_rtos_safe()`** - ⭐ v2.0 新增：仅屏蔽到 FreeRTOS 边界，推荐默认
  - `pal_irq_restore()`
- [ ] 定义 RAII 宏
  - `PAL_CRITICAL_SECTION(code)` - 使用 `pal_irq_save_rtos_safe()`（推荐默认）
  - **`PAL_CRITICAL_SECTION_STRICT(code)`** - ⭐ v2.0 新增：使用 `pal_irq_save()` 全屏蔽

#### 3.2 扩展 `pal/include/hal/pal_hal.h` 的 GPIO 中断接口

- [ ] 新增电平触发类型枚举
  - `PAL_GPIO_INTR_LOW_LEVEL`
  - `PAL_GPIO_INTR_HIGH_LEVEL`
- [ ] 新增 `pal_gpio_enable_interrupt_ex()` 声明（带优先级参数）
- [ ] 添加 `static inline` 兼容包装，旧 `pal_gpio_enable_interrupt()` 调用新接口，默认 `PAL_IRQ_PRIO_NORMAL`

#### 3.3 重构 `devkitc_smoke/app_callbacks.c`

- [ ] 将 `boot_button_isr` 移出 `#if defined(ESP_PLATFORM)` 包裹
- [ ] 添加 `PAL_ISR` 宏注解
- [ ] `app_init` 中中断注册代码移出 `#ifdef` 包裹
- [ ] 验证：业务逻辑完全不变（仅移动位置 + 加宏）

#### 3.4 各平台添加空实现（存根）

- [ ] ESP32: 继续使用现有实现，内部调用链不变
- [ ] Host: 添加 stub 实现，返回 `WINK_ERR_UNSUPPORTED`
- [ ] WASM: 添加 stub 实现，返回 `WINK_ERR_UNSUPPORTED`

### 交付物

- `pal/include/pal_irq.h` （新增）
- `pal/include/hal/pal_hal.h` （更新）
- `examples/devkitc_smoke/app_callbacks.c` （去宏）
- 各平台 stub 实现

### 验收标准

- [ ] ✅ ESP32 编译通过，功能与重构前完全一致
- [ ] ✅ Host/WASM 编译通过
- [ ] ✅ `app_callbacks.c` 零 `#ifdef ESP_PLATFORM`（ISR 相关）
- [ ] ✅ 旧代码无需修改即可编译（向后兼容）

---

## 4. Phase 2: ESP32 完整实现（2 天）

**目标**：ESP32 平台完整实现 PAL 中断抽象，满足所有安全契约

### 任务清单

#### 4.1 创建 `targets/esp32/pal_irq_esp32.c`

##### 4.1.1 优先级映射表（FreeRTOS 安全边界 + REALTIME 级）

- [ ] 实现 `s_prio_map[]` 静态数组（v2.0 扩展）
  ```c
  static const int s_prio_map[PAL_IRQ_PRIO_COUNT] = {
      [PAL_IRQ_PRIO_LOWEST]   = 2,
      [PAL_IRQ_PRIO_LOW]      = 3,
      [PAL_IRQ_PRIO_NORMAL]   = 4,
      [PAL_IRQ_PRIO_HIGH]     = 5,    // = configMAX_SYSCALL_INTERRUPT_PRIORITY
      [PAL_IRQ_PRIO_HIGHEST]  = 5,    // FreeRTOS 安全边界
      [PAL_IRQ_PRIO_REALTIME] = 7,    // ⚠️ v2.0 新增：硬件最高级，非 RTOS 安全
  };
  ```
- [ ] **编译期静态断言**：`s_prio_map[PAL_IRQ_PRIO_HIGHEST] <= configMAX_SYSCALL_INTERRUPT_PRIORITY
- [ ] 文档注释明确标注 `PAL_IRQ_PRIO_REALTIME` 使用限制

##### 4.1.2 GPIO 中断分发机制（**SMP 安全 + 正确清标顺序）

- [ ] 定义 GPIO 分发表 + **自旋锁（v2.0 新增 SMP 安全）**
  ```c
  #define PAL_GPIO_MAX_PIN  50
  static pal_isr_t   s_gpio_isr[PAL_GPIO_MAX_PIN] = {NULL};
  static void       *s_gpio_isr_arg[PAL_GPIO_MAX_PIN] = {NULL};
  // ⭐ v2.0 新增：SMP 双核竞态保护自旋锁
  static portMUX_TYPE s_gpio_table_mux = portMUX_INITIALIZER_UNLOCKED;
  ```
- [ ] 实现 `gpio_isr_wrapper()` 公用 ISR 包装（**v2.0 修正版，四步流程）**
  - **⚠️ 严格执行顺序**：
    1. ✅ **第一步**：`gpio_intr_disable(pin)` + `gpio_clear_intr_status(pin)`
       - API 名称修正：不使用非标准的 `gpio_intr_clr_enable`
    2. ✅ **第二步**：**持有自旋锁**，原子性读取 isr + arg（防止 Core 1 正在 disable 时 Core 0 读到不一致）
    3. ✅ **第三步**：调用用户 ISR
    4. ✅ **第四步**：如果分发表中 isr 非空，重新 `gpio_intr_enable(pin)`

##### 4.1.3 GPIO 中断接口实现

- [ ] 实现 `pal_gpio_enable_interrupt_ex()`
  - 参数校验（pin 范围、callback 非空、prio 范围）
  - 触发类型转换（PAL → ESP-IDF）
  - 自动安装 GPIO ISR 服务（首次调用时）
  - 写入分发表 → `gpio_isr_handler_add()` → 设置触发类型
- [ ] 实现 `pal_gpio_disable_interrupt()`（**SMP 安全版**）
  - 移除 handler → **持有自旋锁** 清空分发表（顺序不可颠倒）

##### 4.1.4 逻辑中断核心接口

- [ ] 定义 `s_irq_handles[32]` 句柄数组
- [ ] 实现 `pal_irq_enable()` - 调用 `esp_intr_alloc()`，flags 含 `ESP_INTR_FLAG_IRAM`
- [ ] 实现 `pal_irq_disable()` - 调用 `esp_intr_free()`
- [ ] 实现 `pal_irq_direct_connect()` - 内部调用 `pal_irq_enable()`
- [ ] 实现 `pal_irq_set_pending()` - 调用 `XT_SET_INTSET()`
- [ ] 实现 `pal_irq_clear_pending()` - 调用 `XT_SET_INTCLEAR()`

##### 4.1.5 全局中断锁（**v2.0 双等级语义**）

- [ ] 实现 `pal_irq_save()` - **全屏蔽模式**
  - **不使用** `XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL)`（只能屏蔽 ≤3 级）
  - ✅ 使用 `XTOS_SET_INTLEVEL(XCHAL_NUM_INTLEVELS)`（屏蔽所有可屏蔽中断）
  - ⚠️ 文档强制：受此锁保护的临界区 **必须 < 1µs**
- [ ] 实现 `pal_irq_save_rtos_safe()` - **v2.0 新增：RTOS 安全模式**
  - 使用 `XTOS_SET_INTLEVEL(configMAX_SYSCALL_INTERRUPT_PRIORITY)`
  - 仅屏蔽 ≤5 级中断，不影响 Wi-Fi 基带等最高优先级中断
  - ✅ **推荐默认使用**，可安全用于较长临界区
- [ ] 实现 `pal_irq_restore()`
  - 调用 `XTOS_RESTORE_JUST_INTLEVEL(mask)`

##### 4.1.6 SMP ISR 执行同步原语

- [ ] 实现 `pal_irq_synchronize(irq_num)` - **v2.0 新增**
  - 等待所有核心上正在执行的该中断 ISR 完成
  - 典型场景：`pal_irq_disable()` → `pal_irq_synchronize()` → `free(资源)`
  - 通过内存屏障 + 等待周期实现，确保 SMP 系统安全释放 ISR 使用的资源

#### 4.2 向后兼容处理

- [ ] 旧 `pal_gpio_enable_interrupt()` 调用新接口，默认 `NORMAL` 优先级
- [ ] 确保现有调用方无需修改即可编译

### 交付物

- `targets/esp32/pal_irq_esp32.c` （新增）
- 更新现有 GPIO 实现文件
- 《中断延迟基准测试报告》

### 验收标准

#### 功能验收

- [ ] ✅ devkitc_smoke S4（GPIO ISR）硬件测试通过
- [ ] ✅ 所有现有测试用例通过

#### 性能验收

- [ ] ✅ 中断延迟基准测试：重构前后延迟一致（或更好）
- [ ] ✅ GPIO 中断响应延迟 < 240 cycles (< 1μs @ 240MHz)
- [ ] ✅ 内存占用对比报告：增加的静态内存 < 200 字节

#### 安全契约验收

- [ ] ✅ **编译期静态断言生效**：优先级不超过 FreeRTOS 安全边界
- [ ] ✅ **双等级中断锁语义验证**：
  - `pal_irq_save_rtos_safe()` 不屏蔽 REALTIME / Wi-Fi 中断
  - `pal_irq_save()` 屏蔽所有可屏蔽中断
- [ ] ✅ **SMP 竞态验证**：一个 core 触发 ISR，另一个 core 反复 disable/enable，无空指针崩溃
- [ ] ✅ **`pal_irq_synchronize()` 验证**：disable + synchronize 后立即释放资源，无 UAF 崩溃
- [ ] ✅ **清标顺序验证**：通过 1MHz 方波 24h 压力测试无重入崩溃
- [ ] ✅ **中断锁 save/restore 耗时** < 10 cycles

---

## 5. Phase 3: WASM 仿真实现（2 天）

**目标**：WASM 平台精确仿真中断语义，与 ESP32 行为一致

### 任务清单

#### 5.1 创建 `targets/wasm/pal_irq_wasm.c`

##### 5.1.1 中断表与状态

- [ ] GPIO ISR 表：`s_gpio_isr[]`, `s_gpio_isr_arg[]`, `s_gpio_intr_type[]`
- [ ] 逻辑中断表：`s_wasm_irq_table[]`, `s_wasm_irq_arg[]`
- [ ] 最后电平状态：`s_gpio_last_level[]` 用于边沿检测

##### 5.1.2 Pending 队列与延迟注入（**v2.0 Pareto 长尾分布**）

- [ ] 定义 pending 队列结构
  ```c
  typedef struct {
      uint32_t      irq_num;
      pal_irq_prio_t prio;
      uint32_t      target_tick;    // 延迟分发的目标 tick
      bool          is_gpio;
  } wasm_pending_irq_t;
  ```
- [ ] 实现 `sort_pending_by_priority()` - 按优先级排序（高优先级先发）
- [ ] 实现 `calc_target_tick()` - **v2.0 改进：Pareto 长尾分布**
  - 80% 中断：1-2 tick 短延迟
  - 20% 中断：3-5 tick 长尾延迟（模拟真实硬件 Flash Cache Miss）
  - 不再使用均匀分布，更贴近真实硬件行为

##### 5.1.3 GPIO 中断条件检测

- [ ] 实现 `pal_wasm_gpio_level_changed(pin, new_level)`
  - 检测触发条件（上升沿/下降沿/任意沿/低电平/高电平）
  - ✅ **只标记 pending，不立即调用 ISR**（模拟硬件延迟）
  - 加入 pending 队列时计算 `target_tick`（延迟注入）

##### 5.1.4 Tick 边界统一分发

- [ ] 实现 `pal_wasm_dispatch_pending_irqs()`
  - `s_current_tick++` 推进仿真时间
  - ✅ **中断锁语义**：`s_irq_lock_nest_count > 0` 时不分发
  - 按优先级排序后，遍历所有到期的 pending 中断并执行
  - 未到期中断保留到下一轮

##### 5.1.5 逻辑中断核心接口

- [ ] 实现 `pal_irq_enable/disable/direct_connect`
- [ ] 实现 `pal_irq_set_pending()` - 加入 pending 队列
- [ ] 实现 `pal_irq_clear_pending()`

##### 5.1.6 中断锁仿真

- [ ] 实现锁计数机制 `s_irq_lock_nest_count`
- [ ] `pal_irq_save()` - 计数 +1，返回保存前的状态
- [ ] `pal_irq_restore()` - 计数 -1，最外层释放时隐含 pending flush

##### 5.1.7 Virtual Peripheral Registry 集成

- [ ] 中断事件可以作为虚拟外设的输入/输出
- [ ] 集成到仿真主循环

### 交付物

- `targets/wasm/pal_irq_wasm.c` （新增）
- 仿真主循环集成代码
- 《仿真保真度验证报告》

### 验收标准

- [ ] ✅ WASM 侧 ISR 行为与 ESP32 一致（调用顺序、参数传递）
- [ ] ✅ **时序偏差测试**：WASM 与 ESP32 中断响应时间差 < 1 tick
- [ ] ✅ **中断锁语义验证**：持有锁期间触发中断，ISR 延迟到锁释放后执行
- [ ] ✅ Virtual Peripheral Registry 集成完成
- [ ] ✅ WASM ISR 分发开销 < 50 JS 操作数 / ISR

---

## 6. Phase 4: Host 单元测试支持（1 天）

**目标**：Host 平台提供完整的中断仿真与测试基础设施，覆盖率 > 90%

### 任务清单

#### 6.1 创建 `targets/host/pal_irq_host.c`

##### 6.1.1 中断挂载表

- [ ] GPIO 中断表：`s_gpio_isr[]`, `s_gpio_isr_arg[]`
- [ ] 逻辑中断表：`s_host_irq_table[]`, `s_host_irq_arg[]`

##### 6.1.2 调用计数统计

- [ ] GPIO ISR 调用计数：`s_isr_call_count[]`
- [ ] 逻辑中断调用计数：`s_host_irq_call_count[]`
- [ ] 提供查询 API：`pal_host_get_isr_call_count()`, `pal_host_get_logical_isr_call_count()`

##### 6.1.3 手动触发 API（单测注入）

- [ ] `pal_host_trigger_gpio_interrupt(pin)` - 手动触发 GPIO 中断
- [ ] `pal_host_trigger_logical_interrupt(irq_num)` - 手动触发逻辑中断

##### 6.1.4 Pending 队列与中断锁语义

- [ ] `s_pending_gpio[]` 队列 + `s_pending_count`
- [ ] `s_irq_lock_depth` 锁深度计数
- [ ] **核心语义**：持有锁时触发中断 → 加入 pending，不立即执行
- [ ] 锁释放（最外层 restore）→ flush 所有 pending ISR
- [ ] 提供查询 API：`pal_host_get_pending_count()`, `pal_host_get_irq_lock_depth()`

##### 6.1.5 泄漏检测

- [ ] 单测结束时可断言 `pal_host_get_irq_lock_depth() == 0`

##### 6.1.6 核心接口实现

- [ ] `pal_gpio_enable_interrupt_ex()`
- [ ] `pal_gpio_disable_interrupt()`
- [ ] `pal_irq_enable/disable/direct_connect/set_pending/clear_pending`
- [ ] `pal_irq_save/restore()`

#### 6.2 编写单元测试（`test/test_pal_irq.c`）

##### 必测用例

- [ ] **`test_gpio_interrupt_registration()`**
  - 注册 ISR → 手动触发 → 验证调用计数
  - 禁用中断 → 触发 → 验证不调用
- [ ] **`test_irq_lock_semantics()`** ⭐ 核心验收
  - 注册 ISR → `pal_irq_save()` → 触发中断
  - 断言：`isr_called == false` 且 `pending_count == 1`
  - `pal_irq_restore()` → 断言：`isr_called == true` 且 `pending_count == 0`
- [ ] **`test_irq_lock_nesting()`**
  - 嵌套 save/restore 两次 → 验证锁深度正确
  - 内层 restore 不触发 flush，最外层才触发
  - 结束时断言 `lock_depth == 0`
- [ ] **`test_type_safe_isr_macro()`**
  - 使用 `PAL_DEFINE_ISR` 定义带结构体参数的 ISR
  - 验证类型转换正确，结构体字段可正确访问
- [ ] **`test_direct_connect_interrupt()`**
  - 测试 `pal_irq_direct_connect()` 注册与触发
- [ ] **`test_invalid_parameters()`**
  - 负数 pin、越界中断号、NULL 回调 → 返回 `WINK_ERR_INVALID_ARG`
- [ ] **`test_interrupt_trigger_types()`**
  - 上升沿/下降沿/任意沿/电平触发 全部覆盖

### 交付物

- `targets/host/pal_irq_host.c` （新增）
- `test/test_pal_irq.c` （新增）
- 《单元测试覆盖率报告》

### 验收标准

- [ ] ✅ 所有中断触发类型的单元测试覆盖
- [ ] ✅ **中断锁语义精确验证**（持有锁时 pending，释放后执行）
- [ ] ✅ 中断锁未配对使用能被正确检测
- [ ] ✅ `PAL_DEFINE_ISR` 类型安全宏工作正常
- [ ] ✅ 无效参数处理正确
- [ ] ✅ Host 平台所有测试通过
- [ ] ✅ 测试覆盖率 > 90%

---

## 7. Phase 5: Device Tree 集成（3 天）

### 7.1 CodeGen 支持编译期安全检查（v2.0 新增）

- [ ] **CMake 配置期参数校验**（Python 脚本中实现）
  - GPIO pin 范围检查：`if pin >= PAL_GPIO_MAX_PIN` 抛出配置错误
  - 优先级范围检查：`if prio >= PAL_IRQ_PRIO_COUNT` 抛出配置错误
  - REALTIME 优先级警告：检测到 `PAL_IRQ_PRIO_REALTIME` 时输出醒目警告
- [ ] **生成 C 编译期断言**
  - `_Static_assert(DT_GPIO_PIN_xxx < PAL_GPIO_MAX_PIN, "DTS GPIO pin out of range")
  - `_Static_assert(DT_IRQ_PRIO_xxx < PAL_IRQ_PRIO_COUNT, "DTS prio out of range")

**目标**：中断配置完全 Device Tree 驱动，零硬编码

### 任务清单

#### 7.1 CodeGen 支持编译期静态配置生成（DTS → C）

##### 7.1.1 Device Tree 解析扩展

- [ ] Python 构建器支持解析 `interrupts` 和 `interrupt-parent` 属性
- [ ] 支持解析 `interrupt-priority` 属性
- [ ] 支持解析 `wasm-irq-latency` 仿真参数

##### 7.1.2 静态配置代码生成

- [ ] 定义 `pal_static_irq_config_t` 结构
  ```c
  typedef struct {
      uint32_t        irq_num;         // 逻辑中断号
      uint32_t        hw_irq_id;        // 硬件中断号
      pal_irq_prio_t  default_prio;     // 默认优先级
      bool            is_direct;         // 是否直连中断
  } pal_static_irq_config_t;
  ```
- [ ] 生成 `pal_irq_config.c` - 包含 `g_pal_static_irq_table[]` 静态数组
- [ ] `device_tree.h` 中导出：
  - 逻辑中断号宏：`DT_IRQ_BOOT_BUTTON`, `DT_IRQ_UART0_RX`, etc.
  - 静态配置表声明：`extern const pal_static_irq_config_t g_pal_static_irq_table[]`
  - 简化注册宏：`DT_GPIO_ENABLE_INTERRUPT(name, edge, isr, arg)`

#### 7.2 运行时配置覆写（Flash 逃生通道）

##### 7.2.1 配置 Blob 格式定义

- [ ] 定义 `irq_config_blob_t` 结构
  ```c
  typedef struct {
      uint8_t  version;                    // 结构版本 = 1
      uint8_t  prio_override[32];          // 优先级覆写，0xFF = 使用默认
      uint32_t checksum;                    // CRC32 校验和
  } irq_config_blob_t;
  ```
- [ ] 实现 `compute_checksum()` - CRC32 校验

##### 7.2.2 配置应用函数

- [ ] 实现 `device_tree_apply_irq_config()`
  - 从 `pal_storage` 读取 `irqcfg` blob
  - ✅ **静默降级原则**：读取失败 / checksum 错误 → 使用编译期默认值，不 Panic
  - 校验通过 → 遍历覆写表，调用 `esp_intr_set_priority()` 更新硬件优先级

##### 7.2.3 启动时预配置

- [ ] 系统启动时，遍历 `g_pal_static_irq_table[]` 进行中断分配器预配置
- [ ] 集成到现有的 Flash 覆写机制（ADR-0008）

#### 7.3 更新示例代码

- [ ] `devkitc_smoke` 使用 `DT_GPIO_ENABLE_INTERRUPT` 宏注册中断
- [ ] `avoidance_car` 超声波中断改为 Device Tree 方式

### 交付物

- Device Tree CodeGen Python 脚本更新
- 生成的 `pal_irq_config.c` 和 `device_tree.h`
- `device_tree_apply_irq_config()` 实现
- 更新后的示例代码

### 验收标准

- [ ] ✅ device tree 示例代码生成正确
- [ ] ✅ **运行时覆写验证**：修改 Flash 配置 → 验证硬件优先级已更新
- [ ] ✅ **静默降级验证**：Flash 配置损坏 / checksum 错误 → 系统正常启动，使用默认值
- [ ] ✅ 所有示例代码编译通过
- [ ] ✅ 示例代码零硬编码（无 `BOOT_BUTTON_PIN` 字面量）

---

## 8. Phase 6: 中断共享机制（2.5 天，**v2.0 语义修正 + RCU SMP 安全**）

**目标**：支持多个外设共享同一硬件中断向量（责任链模式），v2.0 修正语义：不提前终止遍历，RCU 模式安全修改链

### 任务清单

#### 8.1 共享中断核心实现（所有平台）

##### 8.1.1 类型定义（**v2.0 语义修正**）

- [ ] 定义 `pal_irq_shared_handler_t`
  ```c
  typedef bool (*pal_irq_shared_handler_t)(void *arg);
  // ⚠️ v2.0 语义修正（参考 Linux 内核 Shared IRQ）：
  // 返回 true = 认领了该中断（仅用于统计和杂散中断检测）
  // 返回 false = 不是我的中断
  // 无论返回值如何，始终遍历调用所有 handler！
  // 不再提前终止链！避免 USB + ETH 同时触发时反复进入中断
  ```

##### 8.1.2 责任链数据结构（**RCU 模式，SMP 安全**）

- [ ] 采用 RCU（Read-Copy-Update）模式实现无锁读（ISR 中）
  ```c
  #define MAX_SHARED_HANDLERS  4    // 单中断最多 4 个共享者

  typedef struct {
      pal_irq_shared_handler_t  handler;
      void                     *arg;
  } shared_handler_entry_t;

  // RCU 模式：链表头使用原子指针替换
  typedef struct {
      shared_handler_entry_t entries[MAX_SHARED_HANDLERS];
      uint8_t count;
  } shared_chain_t;

  // 每个中断号有一个当前链指针（原子替换）
  static shared_chain_t *s_shared_chain[32];
  static portMUX_TYPE s_shared_chain_mux;  // 仅用于写路径保护
  ```

##### 8.1.3 注册接口实现（**RCU 安全写路径**）

- [ ] 实现 `pal_irq_shared_register(irq_num, prio, handler, arg)`
  - 持有自旋锁保护
  - 创建新链副本 → 添加新 handler → 原子替换指针
  - 调用 `pal_irq_synchronize()` 等待所有 core 退出旧链 ISR
  - 安全释放旧链内存
  - 首次注册：调用 `pal_irq_enable()` 安装共享 Wrapper
  - 后续注册：优先级参数忽略（以首次为准）
  - 链满（4 个）返回 `WINK_ERR_NO_MEM`

##### 8.1.4 共享中断 Wrapper（**v2.0 不终止遍历**）

- [ ] 实现 `shared_irq_wrapper(arg)`
  - 原子读取当前链指针（RCU 读路径，无锁）
  - **按注册顺序遍历调用所有 handler（不提前终止）**
  - 统计认领总数，若所有 handler 都返回 false → 记录警告日志（杂散中断）

#### 8.2 STM32 EXTI 特殊处理（**v2.0 新增**）

- [ ] **原子写清标**：`EXTI->PR = mask`（不是读-改-写）
- [ ] **清标后二次验证**：读取 GPIO 电平确认确实是该引脚触发
- [ ] 避免 EXTI 共享线上的中断误归属

#### 8.3 ESP32 平台适配

- [ ] 共享中断的优先级处理（首次注册生效）
- [ ] 未认领中断的警告日志（使用 `ESP_LOGW`）
- [ ] 性能优化：链遍历时间 < 1μs（4 个 handler）

#### 8.4 单元测试（**v2.0 语义 + SMP 并发测试**）

- [ ] 测试：单 handler 认领中断
- [ ] 测试：**双 handler 同时触发，两个都被调用**（v2.0 核心验收）
- [ ] 测试：无 handler 认领 → 警告日志
- [ ] 测试：链满（4 个）拒绝注册
- [ ] **测试：Core 0 遍历链时 Core 1 注册新 handler（无崩溃，RCU 安全）**

### 交付物

- 共享中断机制实现（各平台，RCU SMP 安全版）
- 共享中断单元测试（含并发测试）
- 《共享中断性能测试报告》

### 验收标准

- [ ] ✅ **双 handler 同时触发时，两个都被调用**（v2.0 核心语义验证）
- [ ] ✅ 链遍历性能测试：4 个 handler < 1μs
- [ ] ✅ 未认领中断能正确记录警告
- [ ] ✅ 最多 4 个共享 handler，超出返回错误
- [ ] ✅ **并发安全验证**：一个 core 遍历链，另一个 core 注册/注销，无崩溃
- [ ] ✅ 所有共享中断测试用例通过

---

## 9. Phase 7: 文档 + 推广（1 天）

**目标**：团队掌握新中断子系统，CI 静态检查生效

### 任务清单

#### 9.1 编写 ISR 编码规范文档

- [ ] ISR 执行时间限制指南（< 10μs，< 10% tick 周期）
- [ ] ISR 中禁止调用的函数列表（Flash 函数、阻塞 API、内存分配）
- [ ] **直连中断特殊规范**：禁止使用任何 FreeRTOS 阻塞同步原语
- [ ] 栈使用量估算方法（递归调用、局部变量大小）
- [ ] 常见陷阱 & 最佳实践
  - 清标顺序陷阱（先清标再调用 ISR）
  - 中断锁语义陷阱（save 后所有中断都停了）
  - 优先级映射陷阱（不是数值越大优先级越高）

#### 9.2 完善静态检查规则（Clang-Tidy）

- [ ] 规则 1：`PAL_ISR` / `PAL_DIRECT_ISR` 函数不能调用非 `IRAM_ATTR` 函数
  - 防止 ESP32 Flash Cache Miss 导致随机崩溃
- [ ] 规则 2：`pal_irq_save()` / `pal_irq_save_rtos_safe()` 必须在同一函数内 `restore()`
  - 防止中断锁泄漏导致系统死锁
- [ ] 规则 3：**禁止在 REALTIME 优先级 ISR 中调用任何 RTOS API**（v2.0 新增）
  - REALTIME 级 > `configMAX_SYSCALL_INTERRUPT_PRIORITY`，调用任何 FromISR API 都会 Hard Fault
- [ ] 规则 4：禁止在直连型中断中直接或间接调用带阻塞的 FreeRTOS API
  - `xQueueReceive()`, `vTaskDelay()`, etc.
- [ ] 集成到 CI 流水线

#### 9.3 编写迁移指南

- [ ] 旧代码如何迁移到新接口的 step-by-step 指南
  - Step 1：给 ISR 添加 `PAL_ISR` 注解
  - Step 2：移出 `#ifdef ESP_PLATFORM`
  - Step 3：可选：改用 `pal_gpio_enable_interrupt_ex()` 指定优先级
  - Step 4：可选：改用 `PAL_DEFINE_ISR` 获得类型安全
  - Step 5：可选：改用 Device Tree 宏
- [ ] 常见问题 FAQ
- [ ] 向后兼容性说明

### 交付物

- ISR 编码规范文档
- Clang-Tidy 规则配置
- 迁移指南文档
- 团队分享 Slides

### 验收标准

- [ ] ✅ ISR 编码规范文档发布
- [ ] ✅ CI 静态检查生效（能检测到违规代码）
- [ ] ✅ 迁移指南发布并与团队分享
- [ ] ✅ 团队培训完成（全员理解新中断架构）

---

## 10. 风险与应对（v2.0 更新版）

| 风险 | 概率 | 影响 | 应对措施 |
|------|------|------|---------|
| **⭐ SMP 双核分发表竞态：Core 0 读 isr 时 Core 1 置空 arg，导致空指针** | 中 | 极高 | GPIO 分发表读写全部加自旋锁；ISR 中原子性读取 isr+arg；双核并发压测 |
| **中断同时触发时提前终止链，导致反复进入中断性能下降** | 中 | 中 | v2.0 已修正语义：不提前终止，始终遍历所有 handler；双外设同时触发压测 |
| **SMP 下 disable 后立即释放资源，另一个 core 仍在执行 ISR（UAF）** | 中 | 极高 | 新增 `pal_irq_synchronize()` API；文档强制 disable→synchronize→free 三部曲 |
| **REALTIME 优先级被误用，ISR 中调用 RTOS API 导致 Hard Fault** | 中 | 极高 | CodeGen 配置 REALTIME 时输出醒目警告；Clang-Tidy 检测 REALTIME ISR 中的 RTOS 调用 |
| **长时间全屏蔽中断导致看门狗复位 / Wi-Fi 时序异常** | 中 | 高 | 默认推荐使用 `pal_irq_save_rtos_safe()`；文档严格限制全屏蔽临界区 < 1µs |
| **Phase 2 引入其他隐蔽的中断竞态** | 中 | 高 | 增加并发压测用例：双核同时操作 GPIO 中断；代码 Review 重点检查清标顺序 |
| **RCU 链修改竞态：Core 0 遍历链时 Core 1 修改链** | 低 | 高 | RCU 模式原子替换指针 + synchronize 等待；写路径加锁保护 |
| **WASM 时序模拟不准确** | 中 | 中 | 建立时序回归测试，每次 PR 对比 ESP32 真机数据；偏差 > 1 tick 自动告警 |
| **ISR 中调用 Flash 函数导致 ESP32 随机 Crash** | 中 | 高 | Phase 7 添加 Clang-Tidy 规则 1，CI 强制检查；Crash 时首先检查栈回溯是否在 Flash |
| **优先级映射方向相反导致实时性失效** | 低 | 高 | 单元测试验证抢占顺序 + 真机时序测试；文档明确"语义优先，数值不保证" |
| **中断锁未配对使用导致系统死锁** | 中 | 高 | Host 平台锁深度检测 + 单测结束断言 `lock_depth == 0`；Clang-Tidy 规则 2 |
| **静态检查误报/漏报** | 高 | 低 | 分阶段启用，先收集数据再调整阈值；初期仅告警不阻断 |
| **GPIO ISR Wrapper 清标顺序错误导致重入崩溃** | 低 | 极高 | Code Review 强制检查清单；1MHz 方波 24h 压力测试作为门禁 |
| **优先级超过 FreeRTOS 阈值导致 Hard Fault** | 低 | 极高 | 编译期静态断言 + 运行时断言双重保护；这是 ESP32 最常见玄学崩溃来源 |
| **STM32 EXTI 读-改-清窗口丢失中断** | 低 | 中 | 原子写清标 + 清标后二次验证 GPIO 电平 |

---

## 11. 里程碑（v2.0 更新版）

| 里程碑 | 日期 | 交付物 | 验收要点 |
|--------|------|--------|---------|
| **M0: 专家评审完成** | Day 0 | v2.0 设计文档 + 4 个新 ADR | 10 项专家建议全部采纳 |
| **M1: 接口定义完成** | Day 1.5 | `pal_irq.h` v2.0, `pal_hal.h` 更新 | 6 级优先级 + 双等级锁 + synchronize API |
| **M2: ESP32 实现完成** | Day 3.5 | ESP32 SMP 安全版实现 | 自旋锁保护分发表 + 所有安全契约验证通过 |
| **M3: WASM 仿真完成** | Day 5.5 | Pareto 长尾延迟仿真 | 时序偏差 < 1 tick + 中断锁语义精确 |
| **M4: 单元测试完成** | Day 6.5 | Host 平台测试覆盖率 > 90% | SMP 并发测试 + 中断锁语义全部验证 |
| **M5: Device Tree 集成** | Day 9.5 | 编译期安全检查 + 示例迁移 | 配置越界 CMake 阶段报错 + 零硬编码 |
| **M6: 中断共享完成** | Day 12 | RCU 模式责任链实现 | 双外设同时触发都被处理 + 并发修改安全 |
| **M7: 全部完成** | Day 13.5 | 文档、CI、指南全部就绪 | 团队培训完成 + 所有 v2.0 增强功能就绪 |

---

## 12. 回滚计划

### 12.1 原子可回滚原则

每个 Phase 都是**原子可回滚**的，不影响前序 Phase 功能：

| Phase | 回滚方式 | 影响 |
|-------|---------|------|
| Phase 1 | 直接 revert 全部修改 | 旧代码结构不变，零影响 |
| Phase 2 | 旧接口仍然存在，可通过 `#ifdef` 切回旧实现 | 仅影响新接口调用方 |
| Phase 3/4 | 都是新增功能，不影响现有代码路径 | 无影响，直接禁用 |
| Phase 5 | Device Tree 宏是可选的，旧方式仍可用 | 回退到硬编码方式 |
| Phase 6 | 共享中断是新增 API，无人使用则无影响 | 无影响 |
| Phase 7 | 文档/CI 规则更新，随时可回退 | 无影响 |

### 12.2 强制回滚条件

任何 Phase 结束后，如出现以下情况之一，**必须立即回滚**并分析原因：

- [ ] 核心测试失败率 > 5%
- [ ] 中断延迟增加 > 20% 且无法在 Phase 内优化
- [ ] 引入新的编译器警告且无法消除
- [ ] 发现违反安全契约的架构缺陷且无法快速修复

### 12.3 回滚执行步骤

1. 创建 revert 分支，执行 `git revert <phase-commits>`
2. 运行完整测试套件验证
3. 提交 PR，说明回滚原因
4. 分析根因，修正后在下一个迭代重新进入

---

## 附录：v1.1 → v2.0 专家评审变更概览

| 变更类别 | 变更内容 | 关联 ADR |
|---------|---------|---------|
| ⭐ 核心架构修正 | SMP 双核分发表竞态修复 - 自旋锁保护所有读写 | ADR-IRQ-004 |
| ⭐ 核心架构修正 | 共享中断语义修正 - 不提前终止遍历，始终调用所有 handler | ADR-IRQ-005 |
| ⭐ 核心架构修正 | ESP32 API 名称陷阱修正 - 四步清标流程 | 实现细节 |
| 🔧 架构增强 | 新增 `PAL_IRQ_PRIO_REALTIME` - 硬实时逃生通道 | ADR-IRQ-003 |
| 🔧 架构增强 | 双等级中断锁语义 - `save()` 全屏蔽 / `save_rtos_safe()` RTOS 安全 | ADR-IRQ-006 |
| 🔧 架构增强 | 新增 `pal_irq_synchronize()` - SMP ISR 执行同步原语 | ADR-IRQ-007 |
| 🔧 架构增强 | CodeGen 编译期安全检查 - CMake 配置阶段检测越界 | 实现细节 |
| 🔧 架构增强 | RCU 模式共享中断链 - 读路径无锁，写路径原子替换 | ADR-IRQ-005 |
| 📈 优化改进 | STM32 EXTI 原子清标 + 二次验证 - 避免中断丢失 | 实现细节 |
| 📈 优化改进 | WASM Pareto 长尾延迟模型 - 80% 短延迟 / 20% 长尾 | 实现细节 |

**工期变更**：12.5 天 → **13.5 天**（增加 RCU 实现、SMP 并发测试、双等级锁测试）

---

*最后更新：2026-06-30*
*对应设计文档版本：**v2.0（专家评审版）** | 10 项专家建议已全部整合*

