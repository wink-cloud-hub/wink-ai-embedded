# wink-micro-os PAL 中断子系统架构评审

| 项 | 内容 |
|----|------|
| 评审日期 | 2026-06-30 |
| 评审者 | 资深嵌入式架构师视角（Claude） |
| 评审范围 | `pal/include/pal_irq.h`、`pal/include/hal/pal_hal.h` 的 GPIO IRQ 接口、四个 target（esp32/wasm/host/baremetal）的实现、`samples/smp_uaf_test` |
| 关联文档 | `docs/tech-designs/core/pal-unified-interrupt-subsystem.md` (v2.0)、ADR-IRQ-001 ~ ADR-IRQ-007 |
| 关联提交 | 8327d59 (HEAD), 6aa5883, dd2b205, dda7beb, f3992c1, 4877684, 12019c4, dde99b6, b0ad610 |
| 文档性质 | **时间点快照**（参考 `.claude/rules/docs-adr.md`），归档后只读 |

---

## 0. 总评

| 维度 | 评分 | 一句话定性 |
|------|------|-----------|
| 概念架构 | ⭐⭐⭐⭐ | 三通道（Dispatched / Direct / Shared）+ 双等级临界区 + SMP 同步原语，目标定义到位，Linux 影子明显 |
| API 抽象 | ⭐⭐⭐½ | 头文件契约写得非常清楚，但**部分契约实现没有兑现** |
| 可扩展性 | ⭐⭐½ | **逻辑中断号空间是裸数字 + 局部 hack**，离 ADR-0008 承诺的 device tree 化还有距离 |
| 可维护性 | ⭐⭐⭐ | 文档/ADR 齐整，但 target 间存在**复制粘贴漂移**与平台行为静默不一致 |
| 最佳实践对标 | ⭐⭐⭐⭐ | RCU、in-flight counter、synchronize_irq、shared chain 责任链 —— 工业级模式齐备 |
| 落地完成度 | ⭐⭐½ | 文档定义的 7 条 ADR-IRQ 与实现存在 **5 处明显契约 gap**（见第 4 节） |

**一句话**：**设计图纸是 A+，工地是 B-**。骨架对标 Linux/Zephyr，思路清晰；但目前仍处于"v2.0 落地早期"，多个关键契约靠注释承诺、靠后续 phase 兑现 —— 若不收口将很快积累技术债务。

---

## 1. PAL 层评审（`pal_irq.h` + `pal_hal.h::pal_gpio_*`）

### 1.1 设计亮点 ✅

1. **ISR 契约写在头文件**（`pal_isr_t` doxygen 5 条铁律：< 10µs、< 128B 栈、不调阻塞、`FromISR` 限制等）。这是嵌入式 API 文档的优秀实践 —— **把契约钉在签名旁边**，不是埋在某个 wiki。

2. **类型安全 ISR 宏 `PAL_DEFINE_ISR`**：自动展开为 typed wrapper + `(void*)` trampoline，消除每个 ISR 入口的 `struct foo *s = (struct foo *)arg;` 强转 —— 减少误用面，是优雅的小细节。

3. **双等级临界区（ADR-IRQ-006）**：`pal_irq_save()` 全屏蔽 vs `pal_irq_save_rtos_safe()` 仅屏蔽到 syscall 边界 —— 对偶映射 Linux `local_irq_save` 与 FreeRTOS `taskENTER_CRITICAL`。**正确区分了 Wi-Fi 基带延迟敏感 vs OS 数据保护**两种典型场景。

4. **`pal_irq_synchronize()`（ADR-IRQ-007）**：直接对标 Linux `synchronize_irq()`，SMP 下"先 disable 后 free"的 RCU 闭环。`smp_uaf_test` 作为可执行 spec 验证此原语，是**TDD 思维在系统层的体现**。

5. **REALTIME 优先级作为"逃生通道"**：明确标注 *Non-RTOS-safe，严禁调用任何 FromISR API*。这种"分级承诺 + 例外口"的设计比一刀切的 priority 更工程化。

6. **`PAL_CRITICAL_SECTION` 宏**：默认指向 `_rtos_safe` 版本，将"安全默认"植入 API。`PAL_CRITICAL_SECTION_STRICT` 标注"慎用 < 1µs"。**默认安全、显式上锁**，这是好的 API 设计。

### 1.2 头文件契约本身的可改进点

- `pal_irq_save()` 返回 `uint32_t`，**ESP32 上是 PS 寄存器值，WASM/Host 上是计数器值**。语义/类型在跨 target 时根本不同，但头文件未明示"严禁跨 PAL 调用边界传递此值" —— 当前调用方都是"立刻 save / 临近 restore"成对配置看不出问题，但若有人尝试把这个 mask 存到结构体跨上下文传递，会埋暗坑。建议头文件加 `@warning 此值仅在 save/restore 配对的当前栈帧内有效，禁止持久化或跨 target 比较`。

- `pal_irq_direct_isr_t` 签名是 `void(*)(void)`，但所有 target 的 direct_connect 都退化为 `pal_irq_enable((pal_isr_t)handler, NULL)` —— 类型 cast 容忍 ABI 多塞一个寄存器/栈 arg，但被 CFI/UBSan-function 直接判违例。**头文件承诺 "direct" 但实现不 direct** —— 见第 4 节 G1。

---

## 2. Target 层评审

### 2.1 ESP32（`targets/esp32/pal_hal_esp32.c`，1181 行）

> **巨石本身就是个 smell**：GPIO、PWM、I2C、IRQ、共享中断、SMP 同步全堆一个 TU，1181 行行号要靠头注释和 grep 找。建议至少拆成 `pal_hal_esp32_gpio.c` / `pal_hal_esp32_irq.c` / `pal_hal_esp32_i2c.c` 三个 TU。

**实现做得好的：**

- **GPIO ISR wrapper 严格按 ADR-IRQ-002 顺序**（`gpio_isr_wrapper` L214）：disable → clear → 持锁读分发表 → 调用 ISR → 重 enable。是工业级的清标顺序。
- **SMP 同步真的实现了**：`s_irq_in_flight[32]` + `s_gpio_irq_in_flight[GPIO_NUM_MAX]` 双计数器，wrapper 入口 inc / 出口 dec，`pal_irq_synchronize` 忙等归零 + `esp_memory_barrier()`。代码与 `smp_uaf_test` sample 严格对应 —— **测试驱动设计**。
- **RCU 模式共享链**：copy-on-write 新链、原子替换指针、原链等 synchronize 后 free。教科书式 lock-free reader / locked writer 模式。

**实现存疑的：**

- **`xthal_set_intset/intclear` 只触发本核 CPU 内部 7/8 号软中断**。其他 IRQ 走 `esp_intr_alloc` 的硬件源时，没有"软触发硬件外设中断"的通用方法。`pal_irq_set_pending` 对一般外设来说是 no-op —— **API 名字承诺，实现 silent no-op**，调用者无返回值得知。
- **`irq_num >= 32` 硬上限** + `irq_num == 7 → ETS_INTERNAL_SW0_INTR_SOURCE`、`8 → SW1` 是 hard-coded magic（L955-959）。这是当前能让 `smp_uaf_test` 跑通的最小可工作方案，但它**不是 device-tree-friendly 的** —— device-tree 应该提供 `<逻辑号 → 物理 source>` 的映射表。此实现直接卡住 ADR-0008 的兑现。
- **`GPIO.status1_w1tc` 结构体直接寻址只在 ESP32 经典款上对**。ESP32-C3 / S3 / H2 GPIO 寄存器布局不同 —— 当下没问题（项目只用经典 ESP32），但**没有架构隔离**。

### 2.2 WASM（`targets/wasm/pal_hal_wasm.c`）

- **延迟模型 + pending 队列**思路对 —— 仿真"中断不是同步执行"这一本质行为。但目前**只有 `pal_irq_set_pending` 路径走队列**，GPIO 中断（`js_pal_register_interrupt` 路径）走另一套机制。一个 PAL，两种仿真路径，**保真度不一致**。
- **没有 SMP 同步语义**：`pal_irq_synchronize` 直接 `(void)irq_num`。这与"WASM 单线程"的物理事实一致，但意味着 **`smp_uaf_test` 在 WASM target 上是 useless sample** —— "同源测试"承诺在此处自然失败。建议至少加注释明示"此 sample 仅 ESP32 target 有意义"。
- WASM `pal_irq_save_rtos_safe == pal_irq_save`（直接调用）。详见 G4。

### 2.3 Host（`targets/host/pal_hal_host.c`）

**优点**：是四个 target 里**单测友好度最高**的 ——

- `pal_host_trigger_gpio_interrupt` / `pal_host_trigger_logical_interrupt`：可观测的 ISR 注入点
- `pal_host_get_isr_call_count` / `pal_host_get_pending_count` / `pal_host_get_irq_lock_depth`：可观测的内部状态
- `pal_host_reset_isr_stats`：测试夹具友好
- 锁深度 mismatch 时打印 stderr 但不崩 —— 单测友好

这是**为测试而设计**的 host stub，比"直接复用 ESP32 抽象"更专业。

**缺点**：与 WASM 同样的"双等级锁等价化"问题，参见 G4。

### 2.4 Baremetal（`targets/baremetal/pal_osal_bare.c`）

**根本没有 IRQ 实现**。整个文件只实现 OSAL（task/delay/ring buffer），完全没有 `pal_irq_*` / `pal_gpio_enable_interrupt_ex`。如果将来接入无 RTOS 的低端 MCU（STM32G0 等），**链接器会爆出十几个 unresolved**。建议：
- 明确把 baremetal 标为"不支持 PAL IRQ 子系统"（在文档与 CMake 中拒接），
- 或者补一份基于 NVIC 直挂的最小实现。

**当前是未完成的承诺**。

---

## 3. Sample 层（`samples/smp_uaf_test`）

- ✅ 作为 ADR-IRQ-007 的可执行 spec，覆盖 UAF / 阻塞行为 / 计数器归零三个关键不变量。这种"系统级行为 → 可观测 sample"的对应关系是好的。
- ⚠️ `device_tree.h` 当前仅是 `#define TEST_IRQ_UAF 7` 这样的宏表，离 ADR-0008 期望的 DTS-style 配置还有距离。当前是占位。
- ⚠️ Sample 隐含假设跑在 SMP target 上 —— 头文件应明示 `// @target: esp32 (SMP only)`，否则 WASM/Host 跑这个 sample 是 vacuous pass，反而掩盖真实问题。

---

## 4. 契约 / 实现 Gap（关键问题列表）

### G1. `pal_irq_direct_connect` 实际上**不是 direct**【高优先级】

```c
// pal_irq.h: "直连中断在真机上完全绕过 PAL 软件分发逻辑"
// targets/esp32/pal_hal_esp32.c L996:
wink_status_t pal_irq_direct_connect(uint32_t irq_num, pal_direct_isr_t handler) {
    return pal_irq_enable(irq_num, PAL_IRQ_PRIO_NORMAL, (pal_isr_t)handler, NULL);
}
```

调用 `pal_irq_enable` 走 `generic_isr_wrapper` —— 即 dispatch 表 + in-flight 计数器 + clear-pending + 调用用户函数。**所有"零软件分发延迟"承诺都没兑现**。同时 `(pal_isr_t)handler` 把 `void(*)(void)` cast 成 `void(*)(void*)`，ABI 容忍但 CFI/UBSan 违例。

**建议**：要么真接 `esp_intr_alloc(..., ESP_INTR_FLAG_HIGH | ESP_INTR_FLAG_IRAM, ...)` 让 IDF 直接派发；要么**在头文件诚实下调契约**（"目前为软件分发"，未来实现真直连）。当前是文档欠债。

### G2. ESP32 优先级映射**塌缩到 3 级**【中优先级，语义陷阱】

```c
[LOWEST] = LEVEL1, [LOW] = LEVEL1,
[NORMAL] = LEVEL2,
[HIGH] = LEVEL3, [HIGHEST] = LEVEL3, [REALTIME] = LEVEL3,
```

6 级抽象 → 3 级硬件。问题：

- **REALTIME 与 HIGHEST 物理上无区别**，但前者承诺"非 RTOS 安全"，后者承诺"RTOS 安全" —— **同一物理优先级、相反契约**。用户写 REALTIME ISR + `xQueueSendFromISR` 在 ESP32 上不会 crash（仍在 syscall 边界内），换到 STM32 真把 NMI 挂上去后翻车 —— 这就是 PAL 抽象要避免的事，但目前 ESP32 反而把 bug 隐藏了。
- 注释承认"C 语言 ISR 仅支持到 Level 3，NMI 无法使用 C 注册"。那 REALTIME 在 ESP32 应该**返回 `WINK_ERR_UNSUPPORTED`** 而不是静默降级。

### G3. GPIO 优先级参数被 `(void)prio` 丢弃【中优先级】

```c
// targets/esp32/pal_hal_esp32.c L281
(void)prio;  // "prio 参数预留用于未来扩展"
```

`pal_gpio_enable_interrupt_ex` 接 `prio` 但完全不使用 —— ESP-IDF 的 `gpio_install_isr_service(0)` 用默认全局优先级。这意味着 GPIO ISR 的优先级**不受 PAL 控制**，与 `pal_irq_enable` 的优先级路径行为不一致。WASM/Host 同样静默忽略。当 codegen 出的 APP 期望按钮中断优先级高于传感器中断时，**抽象在所有 target 上都失效**。

### G4. `pal_irq_save_rtos_safe` 在 WASM/Host 上**等价于 `pal_irq_save`**【中优先级，仿真保真度】

```c
// wasm/host: uint32_t pal_irq_save_rtos_safe(void) { return pal_irq_save(); }
```

注释说"单线程模型下行为一致"。后果：**Host 单测无法暴露"在 RTOS 安全锁内调用 REALTIME 限定 API"这类 bug** —— 因为 host 把两种锁等价化了。这正是同源仿真承诺要解决的问题（行为级保真），却在这里被回避。

**建议**：至少在 host 上加一个状态位（`current_lock_level`）并在违规调用时 `assert` 或返回错误，把语义差异显形化。

### G5. 共享中断使用 `malloc` + `pal_irq_synchronize` 在写路径忙等【低优先级，性能/原则】

```c
// esp32 pal_irq_shared_register
new_chain = malloc(sizeof(shared_chain_t));    // RT 上下文外，但仍触发 cache miss
...
pal_irq_synchronize(irq_num);  // 最坏 100ms 忙等！
free(old_chain);
```

- **`pal_irq_shared_register` 最坏会阻塞 100ms**（synchronize 的 timeout）。注册流程被这个忙等夹住，长得离谱。Linux `synchronize_rcu()` 至少调度让出。
- `MAX_SHARED_HANDLERS = 4` 硬编码 → `malloc(40 字节)` 还不如静态池 + 自由位图。
- 这条不致命，但**违反 ADR-0004 的"编译期静态分发"基线** —— 号称静态分发的项目，在共享中断注册这条 hot path 上做 malloc + busy-wait。

---

## 5. 可扩展性维度结论

| 子系统 | 扩展点是否暴露 | 当前能否换 MCU 不动 PAL 头 |
|--------|---------------|---------------------------|
| 优先级枚举 | ✅ 6 级足够覆盖大多场景 | ✅ |
| GPIO IRQ | ⚠️ prio 被吃 | ⚠️ STM32 EXTI/共享线需调整 |
| 软件分发逻辑中断 | ❌ irq_num 是裸数字 | ❌ 跨芯片必踩坑 |
| 直连中断 | ❌ 实现退化为软分发 | ❌ |
| 共享中断 | ✅ RCU 链可扩展 | ⚠️ malloc 依赖 |
| 临界区 | ✅ 双等级清晰 | ✅ |
| 同步原语 | ✅ synchronize_irq 抽象到位 | ✅ |
| Device Tree 映射 | ❌ 尚未实现 | ❌ |

**最大扩展瓶颈：`uint32_t irq_num` 这个裸字段**。ADR-0008 说要 device tree，tech-design 章节 6 画了 device-tree 集成图，但**目前 sample（`smp_uaf_test/device_tree.h`）依然是 `#define TEST_IRQ_UAF 7`** —— device tree 还是宏拼写表。在引入第二款 MCU 之前必须把这件事做完，否则换芯片时所有 sample 都要扫一遍 magic number。

---

## 6. 可维护性维度结论

### 正向 ✅

- 7 条 ADR-IRQ（001~007）+ 一份 v2.0 tech-design 把"为什么这么做"写得很清楚
- 注释密度高，关键路径都有"✅/⚠️" emoji 标注关键不变量
- `smp_uaf_test` 作为可执行的回归保护

### 负向 / 风险 ⚠️

#### M1. target 之间的代码重复

esp32 / wasm / host 各有一份几乎一模一样的 `shared_chain` + RCU 写路径（粗看 3 × ~60 行）。**应抽到 `targets/common/pal_shared_chain.c`**，target 只 hook 自己的同步原语（`pal_irq_synchronize` + `malloc` + 自旋锁）。否则一个 bug 要在三个地方改 —— 看 git log 里 `12019c4 fix(irq): resolve interrupt subsystem critical issues + host build fix` 这种同时改多 target 的 commit 就是信号。

#### M2. `#if defined(ESP_PLATFORM)` 大量散落

esp32 文件里把 `set_pending` 整段 ifdef 包了。`commit dda7beb fix(esp32): compile set/clear pending logic by checking only ESP_PLATFORM` 说明这块刚踩过坑。这违反 CLAUDE.md "Bypass 范围收窄"原则 —— ESP_PLATFORM 应该是 build system 选择 TU 的依据，而不是 TU 内部分支。`targets/esp32/*.c` 文件本身就只在 ESP_PLATFORM 下被编译进来，**TU 内部不需要再 ifdef**。

#### M3. ESP32 巨石 TU

1181 行 + 多职责（GPIO + PWM + I2C + IRQ + 共享链 + SMP），改一处经常需要在长文件中跳跃。建议按 ADR-0004 静态分发的"按外设拆 TU"原则切分。

#### M4. v2.0 落地仍在路上

实施计划 `2026-06-30-pal-unified-interrupt-subsystem-implementation-plan.md` 是今天才创建的；多个契约靠"phase 2 兑现"。**这份评审的时间点正处于一个"设计文档已 freeze 但落地中"的不稳定窗口** —— 评审结论对距离这份计划完成不到 1 周时间的代码不应过苛，但也不应放任 G1~G5 长期挂账。

---

## 7. 最佳实践对标

| 最佳实践 | 是否对标 | 备注 |
|----------|---------|------|
| Linux `synchronize_irq` | ✅ `pal_irq_synchronize` | 实现简化为忙等 + timeout，原理一致 |
| Linux Shared IRQ 责任链 | ✅ ADR-IRQ-005 修正 | v2.0 修正"不提前终止"语义后对齐 |
| RCU 读写分离 | ✅ 共享链 copy-on-write | 写路径需 synchronize，已实现 |
| FreeRTOS `configMAX_SYSCALL_INTERRUPT_PRIORITY` 边界 | ✅ 双等级临界区 | `_rtos_safe` 显式落在此边界 |
| MISRA / CERT C ISR 契约钉头文件 | ✅ doxygen 5 铁律 | 优于很多商业 RTOS |
| Zephyr `IRQ_CONNECT` 编译期注册 | ❌ 目前全运行期 | 与 ADR-0004 静态分发哲学不完全一致 |
| Device Tree 中断号映射 | ❌ ADR-0008 未兑现 | 当前 `#define` 宏表 |
| 类型安全 ISR | ✅ `PAL_DEFINE_ISR` 宏 | 业界少见的细节 |
| IRAM 驻留属性 | ✅ `PAL_ISR = IRAM_ATTR` | ESP32 必需 |

---

## 8. 行动建议（按优先级）

### P0（设计契约欠债，必须收口）

1. **修复 G1**：要么真接 `esp_intr_alloc` 实现真 direct，要么诚实修订头文件契约。
2. **修复 G2**：ESP32 上对 REALTIME 优先级返回 `WINK_ERR_UNSUPPORTED`，避免静默降级隐藏跨平台 bug。
3. **修复 G3**：要么 GPIO IRQ 支持 per-pin 优先级（拆 ISR service），要么头文件明示"GPIO prio 全局生效"。

### P1（可维护性，1~2 周内）

4. **抽 `targets/common/pal_shared_chain.c`**：消除三份 RCU 链复制粘贴。
5. **拆 `pal_hal_esp32.c`**：按外设职责切分为 4~5 个 TU。
6. **清理 esp32 TU 内 `#if defined(ESP_PLATFORM)`**：build system 已经在 ESP_PLATFORM 下才编译此 TU，内部 ifdef 是冗余且引入维护风险。

### P2（仿真保真度，2~4 周内）

7. **修复 G4**：host/wasm 区分 lock level，违规组合 assert 或返错。
8. **WASM target 在 `smp_uaf_test` 上明确标注 vacuous**：避免假阳性掩盖。

### P3（架构演进，下一个 Phase）

9. **兑现 ADR-0008 device tree**：让 `irq_num` 从裸数字升级为 DTS 节点引用，配套 codegen 编译期边界检查（实施计划文档已提及）。
10. **共享中断池静态化**：替换 `malloc` 为编译期池 + 自由位图，对齐 ADR-0004。
11. **补 baremetal 实现或在 CMake 层拒绝**：消除"链接器爆错"风险。

---

## 9. 结论

wink-micro-os 的中断子系统是**一个有清晰愿景、对标工业级的设计**，v2.0 设计文档与 7 条 ADR-IRQ 体现了非常成熟的嵌入式架构思维（特别是 SMP 同步与共享链 RCU）。但当前实现处于**"图纸已 freeze，工地刚动土"**的阶段：

- **设计层面**：A 级
- **实现层面**：B- 级（5 处契约 gap + 3 处可维护性 smell）
- **风险点**：G1~G3 三条契约欠债如果不在本 Phase 收口，会被 codegen 出来的 APP 大量调用并固化下来，形成不可回退的 API 表面。

**建议在合入下一个使用此 API 的 sample/codegen 之前完成 P0 三条修复**，把契约校准到与实现一致的位置。其余 P1~P3 可随后续 Phase 推进。

---

*本评审为时间点快照，归档后只读。后续若 PAL 中断 API 演进，请新建评审记录，不要修改本文档。*

---

## 状态更新（2026-07-01）

§8 P1 三条建议的落地状态（PLAN-20260701-PAL-TARGET-P1-MAINT 执行完成后回写）：

| 建议 | 关联 Task | 落地 Commit 范围 | 状态 |
|------|-----------|------------------|------|
| 抽 `targets/common/pal_shared_chain.c` | PLAN-20260701 Task 1 | `e8bcc7c..7950382`（算法层引入 → esp32 迁移 → wasm/host 迁移 + 算法单测） | ✅ 已完成 |
| 拆 `pal_hal_esp32.c` | PLAN-20260701 Task 2 | `ef2a5e1..e403dcb`（`pal_atomic_esp32.h` → `pal_irq_esp32.c` → `pal_hal_esp32_gpio.c` → `_pwm.c` → `_i2c.c`） | ✅ 已完成 |
| 清理 esp32 内 `ESP_PLATFORM` guard | PLAN-20260701 Task 3 | `3267478..15eb1fc` + 后续 lint 正则修复 `b832979` | ✅ 已完成 |

补充说明：
- **净行数目标（T6：Δ ≤ -250 行）未达成**：实际 Δ = +225 行。见计划附录「执行完成汇总」表；主要原因是新 TU 文件头注释（保留 R-5/ADR-IRQ 语义 rationale）+ 每 TU `#else` 静态分析路径 stub + 新引入的 `pal_shared_chain.{h,c}` / `pal_atomic_esp32.h` / `pal_hal_esp32_internal.h` 四个结构性头文件。
- **Bug 收敛点目标（T7）已达成**：`pal_shared_chain.c` 是三 target 共享的 SSOT，任何 shared-chain 修复无法在 target 间漂移。这是本次整改的首要可维护性目标。
- **§8 P1 三条**在物理位置、责任链算法收敛、guard 清理三个层面均已闭环；`pal_hal_esp32.c` 从 1281 行缩减到 27 行，`targets/esp32/*.c` 内每文件 `#if defined(ESP_PLATFORM)` 出现次数 ≤ 1（由 `python wink-tools/wink.py test` L0 lint 防回归）。
