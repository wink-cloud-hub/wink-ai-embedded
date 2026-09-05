# ADR-0024：Fault 三阶段（Phase）生命周期模型与 DAL deinit 质量铁律

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已接受）** |
| 日期 | 2026-07-06 |
| 触发 | [2026-07-06 BAL/DCST 架构重构方案](../../zh/tech-designs/core/2026-07-06-bal-dcst-architecture-refactor.md) v5 Owner 审阅决策版；2026-07-04 全量评审识别出 DAL deinit 缺失与 fault 模型模糊两项架构债 |
| 影响范围 | `runtime/`（fault 流程、actuator_registry）；所有 `dal/` 驱动（必须补对称 deinit）；codegen（bus-owner 抽象）；app 回调签名（`init_status`/`on_fault_status`） |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0010 Boot safe-lock](0010-boot-safe-lock-recovery-threshold.md)、[ADR-0012 契约诚实](0012-contract-honesty-over-silent-degradation.md)、[ADR-0017 阻塞 API 硬隔离](0017-blocking-api-hard-isolation.md)、[ADR-0023 BAL 分层](0023-bal-business-abstraction-layer.md) |
| 关联设计规范 | （Accepted 后回写：`02-wink-micro-os/04-runtime-fault-model.md`、`02-wink-micro-os/03-device-abstraction-layer.md`、`03-app-codegen/*`） |

---

## 背景（Context）

### 术语约定（避免"阶段"过载）

本 ADR 严格区分两个词：
- **阶段（Stage）**：工程交付里程碑（Stage -1/0/1/2/3/4/5，见 tech-design §7）；
- **Phase**：Fault 处理流程的三个执行阶段（Phase 1/2/3）。

### 现况 fault 模型的三个问题

1. **"fault 时自动 stop 所有服务"是伪需求**：
   - `wink_periodic_stop` 需等信号量，最长阻塞 500ms；fault 上下文要尽快进安全态，不能被阻塞调用卡住；
   - 若 fault 本身由某个 task 死锁/栈溢出引起，等信号量永远超时；
   - 真正致命 fault（HardFault/WDT/Panic）CPU 直接复位，走不到软件链表遍历；
   - 通用链表无法表达"I2C bus 必须在 OLED 之后停"这类依赖顺序。

2. **DAL deinit 质量严重不达标**（2026-07-06 代码走查结果）：
   - `dal_led_deinit`/`dal_button_deinit`/`dal_ultrasonic_deinit` 三个已存在但**只调 `pal_resource_release()`**（纯软件 owner 字符串表，见 `pal_resource_esp32.c:78-98`），**没有 `gpio_reset_pin`、没有停 PWM/RMT 外设、没有断 GPIO 路由、没有 ISR 注销**——属于"伪 deinit"；
   - `dal_servo_deinit`/`dal_ssd1306_deinit`/`dal_eeprom_deinit`/`dal_gps_deinit` **完全不存在**（header 里也没有声明）；
   - 这意味着低功耗唤醒（RAM 保持）后 deinit→init 无法干净复位硬件状态，会出现"DMA 搬了旧数据""GPIO reservation 冲突""I2C 从设备拉死 SDA"等玄学 bug。

3. **Init 失败清理责任不清**：
   - 现有 `app_init(void)` + `WINK_CHECK` longjmp 风格 fault 跳转模糊了"谁启动谁回滚"的契约；
   - app init 半途中 fail，runtime 是否应该自动 stop 已启动的 BAL 服务？当前无明确约定，容易出现双删、资源泄漏、依赖顺序错。

### 同时需要拒绝的反模式

- ❌ fault 路径做阻塞式链表遍历 stop 所有服务（违反安全约束）；
- ❌ 让 runtime 猜 BAL 服务依赖顺序（不可解）；
- ❌ "伪 deinit"（只清软件 bookkeeping 不清 IDF 层 reservation）——**`pal_resource_release ≠ gpio_reset_pin`**，这点对 ESP-IDF v6 尤其关键（参见 [[memory:esp32-idf-gpio-reset-pattern]]）；
- ❌ 单个设备 deinit 销毁共享 I2C/SPI bus（会 double-free，影响同 bus 其他 client）。

---

## 方案比选（Options）

### 方案 A：fault 路径全局链表自动 stop 所有 BAL 服务（原 DCST 方案）

每个 BAL helper start 时自注册到全局链表，fault 时逆序遍历 stop。

- ✅ 初学者省心——"一个 fault 全部自动清理"。
- ❌ **fault 上下文阻塞**：`wink_periodic_stop` 最长 500ms，若 fault 源是死锁则永远等不到；
- ❌ **无法表达依赖顺序**：OLED 先于 I2C bus stop 会总线错误；
- ❌ **双删竞态**：用户手动 stop 与 fault 路径 stop 并发时链表损坏；
- ❌ **致命 fault 走不到**：HardFault/WDT/Panic 直接复位，链表遍历形同虚设。

### 方案 B：三阶段（Phase）fault 模型 + "谁启动谁回滚"契约 + DAL deinit 铁律（**推荐**）

Fault 处理分三个 Phase，每个 Phase 有明确的时间预算与可调用 API 白名单：
- **Phase 1（非阻塞，≤100µs）**：wink_trace_fault + actuator_safe_off_all()——关执行器、记故障码，**不** stop 任务、**不** 等信号量；
- **Phase 2（可短阻塞，≤500ms）**：调 `app_on_fault(code)` 回调，应用层自己决定要不要 stop BAL 服务、要不要上报日志；runtime 不代劳；
- **Phase 3（决策点）**：用户调 `wink_runtime_recover()` 则回到主循环继续运行；否则让 WDT 到期硬件复位 → boot safe-lock（ADR-0010）。

DAL 驱动必须提供对称 `_deinit`，严格满足清场检查单（gpio_reset_pin / 停外设 / ISR 注销 / DMA 清场 / I2C 总线恢复 / bus-owner 分离 / 幂等 / 不阻塞 / 软件态复位）；init 失败遵循"谁启动谁回滚"契约，runtime 不自动 stop BAL 服务。

- ✅ Phase 1 非阻塞硬实时约束满足功能安全要求；
- ✅ 依赖顺序交给应用层 `app_on_fault`（开发者最清楚自己的依赖）；
- ✅ DAL deinit 做干净后低功耗唤醒/软重启可依赖；
- ✅ WDT 硬件复位 + 板级安全电路作为最终兜底，不依赖软件回收所有资源；
- ⚠️ 初学者需要写简单的 `app_on_fault`（但对初学者场景可留空，返回 `WINK_ERR_LOCKED` 让 WDT 兜底即可，一行代码）；
- ⚠️ 阶段 0 工作量较大：3 个 deinit 重写 + 4 个 deinit 新建 + bus-owner 抽象 + host init→deinit→init 幂等单测（1.5-2 天）。

### 方案 C：仅补 deinit，不重构 fault 模型

只把 DAL deinit 补齐，fault 流程保持现状（void `app_init` + longjmp 到 fault handler，无明确 Phase 划分）。

- ✅ 工作量最小；
- ❌ fault 上下文 blocking 问题不解决，未来引入更复杂 helper（OLED、SD 卡、MQTT）后必然炸；
- ❌ "runtime 不自动 stop 服务"没有明确契约，未来开发者会在 BAL 里自发搞全局注册，回到方案 A 的坑；
- ❌ init 失败回滚责任不清，双删/泄漏持续发生。

---

## 决策结论（Decision）

**采纳方案 B**。核心设计点：

### 1. Fault 三阶段（Phase）模型

```
Fault 触发 (wink_runtime_raise_fault / HardFault / WDT NMI)
       │
       ▼
┌──────────────────────────────────────────────────────────┐
│ Phase 1：fault-detect 上下文（非阻塞，硬实时 ≤ 100µs）     │
│   - 运行在普通 task 或 ISR 上下文（非 CPU panic/           │
│     HardFault/NMI 异常向量）                              │
│   - 只允许 ISR-safe 的 SDK API（如 gpio_set_level）       │
│   - wink_trace_fault(code)：记录故障码                   │
│   - wink_actuator_safe_off_all()：非阻塞遍历静态表       │
│     关断所有执行器（回调必须非阻塞、失败继续）             │
│   - 禁止：stop 任务 / 等 sem / 动态分配 / printf          │
└──────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────┐
│ Phase 2：Fault task 上下文（可短阻塞，≤ 500ms）            │
│   - 调 app_on_fault_status(code) 回调                     │
│   - 应用层在这里做：LED 闪故障码 / 日志上报 / 显式停      │
│     BAL 服务（wink_xxx_helper_stop(&dev)）                │
│   - Runtime 不自动 stop 任何 BAL 服务                    │
└──────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────┐
│ Phase 3：决策点                                           │
│   - on_fault_status 返回 WINK_OK         → 恢复运行      │
│   - on_fault_status 返回 WINK_ERR_LOCKED → 等 WDT 复位   │
│     → boot safe-lock 逻辑（ADR-0010：异常启动 ≥3 进      │
│     boot lockout）                                        │
└──────────────────────────────────────────────────────────┘
```

**关键结论**：
- ✅ actuator safe-off（把电机停了、LED 关了）是 runtime 的责任，fault Phase 1 立即做；
- ❌ BAL 服务生命周期（停后台 task、删定时器）**不是** runtime fault 路径的责任；
- ✅ 应用层想优雅停服务就在 `app_on_fault` 里自己 stop（此时在 fault task 上下文，阻塞允许，≤500ms）；
- ✅ 初学者场景 `app_on_fault` 可直接 `return WINK_ERR_LOCKED;`（一行代码），WDT 硬件复位清零，板级安全电路兜底。

### 2. App 回调新签名（`init_status`/`on_fault_status`）

统一采用返回 `wink_status_t` 的新签名，比 legacy void `init`/`on_fault` + `WINK_CHECK` longjmp 风格更干净：

```c
typedef struct {
    /* 新签名（强制）：返回 WINK_OK 表示 init 成功；
     * 返回其他错误码 → runtime 执行 Phase 1 safe-off 并进入 fault 流程 */
    wink_status_t (*init_status)(void);

    void         (*loop)(void);            /* 保留 */
    void         (*on_boot)(const wink_boot_info_t *info);  /* 保留 */

    /* 新签名：返回 WINK_OK 表示已恢复继续运行；
     * 返回 WINK_ERR_LOCKED 表示交给 WDT 硬件复位 */
    wink_status_t (*on_fault_status)(uint32_t code);

    /* Legacy 字段保留过渡期兼容，新代码 PR 卡口禁止使用 */
    void         (*init)(void);            /* @deprecated 旧 void 签名 */
    void         (*on_fault)(uint32_t code); /* @deprecated 旧 void 签名 */
} wink_app_callbacks_t;
```

### 3. Init 失败的"谁启动谁回滚"契约

若 `app_init_status()` 返回非 WINK_OK（包括 init 中途 `WINK_TRY` 触发的 fault 跳转）：
- runtime **只**执行 Phase 1 的 `actuator_safe_off_all()`；
- runtime **不**自动 stop 已启动的 BAL 服务；
- 应用若需要在 init 失败路径释放已 start 的 helper，应在返回错误码前显式调用对应 `_stop`；
- 否则直接返回错误码，Phase 3 WDT 复位兜底，板级安全电路把所有引脚置 Hi-Z。

**设计理由**：runtime 无法猜测 BAL 服务的依赖顺序（参考 §背景 问题 1 的第 3 点），强制 runtime 自动 stop 反而会引发"I2C bus 在 OLED 之前被停"这类顺序错。"谁启动谁回滚"让责任边界清晰。

### 4. DAL `_deinit` 质量铁律（清场检查单）

每个 DAL 驱动的 `dal_xxx_deinit()` **必须**满足以下检查单（与 [[memory:esp32-idf-gpio-reset-pattern]] 一致）：

| # | 检查项 | ESP32 具体要求 |
|---|---|---|
| 1 | 硬件停止 | `ledc_stop` / `rmt_rx_stop` / `rmt_tx_stop` / `i2c_driver_delete`（bus-owner 才做） |
| 2 | **GPIO reservation 撤销（硬要求）** | **必须** `gpio_reset_pin(pin)` 撤销 `esp_gpio_reserve` 位图、断开 GPIO 矩阵路由、复位 pad 为默认 Hi-Z 输入态；**仅调 `pal_resource_release()` 是不够的**——那是软件字符串表，不碰 IDF 层 |
| 3 | 中断注销（顺序硬要求） | 先关外设中断源 → 再 `gpio_isr_handler_remove` → 最后关外设时钟，防中断风暴 |
| 4 | DMA/描述符清理 | 释放 DMA 描述符链表、reset 接收 FIFO、清 pending 中断标志；停 DMA 必须等 burst 完成或硬 reset 通道 |
| 5 | I2C 总线恢复（WDT 脏复位） | I2C 驱动 deinit 前若检测 SDA 被从设备拉低（WDT 复位后从设备状态未清），手动 toggle SCL 9 个时钟释放总线 |
| 6 | **共享 Bus 所有权（关键）** | I2C/SPI 共享 bus 由 codegen 生成的 **bus-owner 静态节点**统一管理（拓扑序 bus 先于 client init、逆序 bus 晚于 client deinit）；**单器件 DAL `_deinit` 只清 client 软件状态，不得调用 `i2c_driver_delete`/`spi_bus_free`**，避免 double-free。ssd1306 + eeprom 共享 I2C 是首个触发场景 |
| 7 | 软件态复位 | 把 `dal_xxx_t` 实例恢复到 init 前状态（`initialized=false`、清 config 副本、清 buffer 计数） |
| 8 | 幂等 | 多次 deinit 安全；`deinit(NULL)` 返回 `WINK_ERR_INVALID_ARG`；对未 init 实例返回 `WINK_OK`（no-op，类似 `free(NULL)` 语义） |
| 9 | 不阻塞 | deinit 不得等待信号量超过 50ms；必要时用强制 abort（如 `rmt_rx_stop` 不等 DMA 完成直接 reset） |
| 10 | 签名统一 | `wink_status_t dal_xxx_deinit(dal_xxx_t *dev);`（返回状态，非 void）；`wink_device_tree_deinit()` 用 `WINK_IGNORE_RESULT` 链式 best-effort 调用 |

### 5. Codegen Bus-owner 抽象（支撑检查单 #6）

codegen 在 device_tree.c 中识别共享 I2C/SPI bus，生成**静态 bus-owner 节点**：
- init 顺序：bus 节点先于挂载在该 bus 上的 client 设备 init；
- deinit 顺序：严格 reverse init 序（client 先 deinit，bus-owner 最后销毁 bus）；
- bus-owner 的 `init/deinit` 负责 `i2c_master_bus_create/i2c_driver_delete`（或 SPI 等价）；
- 单器件 DAL `_init` 接收已初始化的 bus handle，`_deinit` 只清 client 状态，**不**触碰 bus 本身。

### 6. `wink_device_tree_deinit()` 语义（加固，非新增）

codegen 已生成 `wink_device_tree_deinit()`，保持现有语义但需审查加固：

```c
void wink_device_tree_deinit(void) {
    /* 1. 先 unregister actuator thunk（forward init 序 = reverse register 序） */
    WINK_IGNORE_RESULT(wink_actuator_unregister(...));
    /* ... */
    /* 2. Debug 断言：所有 periodic 服务已停止（防泄漏） */
    WINK_ASSERT(wink_periodic_active_count() == 0);  /* WINK_PT_DEBUG 下 */
    /* 3. 再按 reverse init 序 deinit 每个设备（bus-owner 自然最后销毁 bus） */
    WINK_IGNORE_RESULT(dal_ssd1306_deinit(&oled));
    WINK_IGNORE_RESULT(dal_eeprom_deinit(&eeprom));
    WINK_IGNORE_RESULT(dal_i2c_bus_deinit(&i2c0));   /* bus-owner 最后 */
    /* ... */
}
```

- 签名 `void`（best-effort 链式清理，单个设备失败不阻断其他设备）；
- unregister actuator thunk 必须在 deinit 之前（否则 deinit 时 safe-off 表里还有悬垂指针）；
- 入口处 `WINK_PT_DEBUG` 下断言 `wink_periodic_active_count() == 0`，避免泄漏；
- deinit 顺序严格 reverse init 序（bus-owner 晚于 client deinit）。

### 7. WDT 脏复位与总线挂死防护

绝大多数 MCU（如 ESP32）的 WDT 复位是系统级软复位，**通常不会复位外部器件状态**。若系统在 I2C 传输中死锁并 WDT，复位后 I2C 从设备可能仍拉低 SDA。必须：

1. DAL I2C bus-owner `init` 时先尝试总线恢复（toggle SCL 9 个时钟）；
2. DAL I2C `deinit` 前同样做总线恢复（检查单 #5）；
3. 两层防御应对 WDT 脏复位后 I2C 永久挂死。
4. **异常启动计数与锁死前置判断**：系统在极早期（Early Boot，执行 `wink_device_tree_init()` 之前）必须先读取 WDT 复位标志并校验 boot 计数器。若已经触发 ADR-0010 的 boot lockout 锁死阈值，必须直接进入 Safe-lock 挂起状态，**禁止执行任何 DAL 驱动初始化或总线恢复操作**，防止硬件严重短路或器件物理故障导致初始化死锁甚至多次损坏芯片。

### 8. 错误码补充（`WINK_ERR_CANCELED`）

详见关联 ADR-0023 §8：`WINK_ERR_CANCELED = -19` 表示并发撤销（并发 stop 抢占导致 start 自回滚）——**良性可预测并发事件**，与 `WINK_ERR_INVALID_STATE`（编程错）语义分离，不在 fault 路径特殊处理。

### 9. NMI/HardFault Hook（Future Work，不阻塞本次）

CPU 级 panic/HardFault 异常向量上下文是极端场景（栈可能已损坏、spinlock 可能死锁），常规 `wink_actuator_safe_off_all()` 不能直接在该上下文跑。独立设计一套 `wink_panic_minimal_safe_off` 机制（使用底层寄存器直写）作为后续工作，不在本次重构范围。

---

## 后果与约束（Consequences & Constraints）

### 正面后果

1. **功能安全可论证**：Phase 1 非阻塞 ≤100µs 硬实时预算可验证；执行器安全关断与软件清理解耦。
2. **低功耗唤醒可靠**：DAL deinit 严格清场后，低功耗 deinit→init 与冷启动行为一致，不再有"玄学 bug"。
3. **依赖顺序诚实**：runtime 不猜依赖，`app_on_fault` 里开发者按自己知道的顺序 stop，代码可见可审查。
4. **WDT 复位可恢复**：I2C 总线恢复机制 + gpio_reset_pin 清 reservation 保证 WDT 脏复位后系统能 boot。
5. **初学者心智简单**：`app_on_fault_status` 留空 `return WINK_ERR_LOCKED;` 即安全。
6. **共享 bus 安全**：bus-owner 抽象从设计上消灭"I2C 上一个设备 deinit 销毁了 bus，下一个设备 deinit double-free"的 bug 类。

### 约束 / 代价

1. **阶段 0 工作量是 3 重写 + 4 新建，不是补几个缺失**：
   - 重写：`dal_led_deinit`/`dal_button_deinit`/`dal_ultrasonic_deinit`；
   - 新建：`dal_servo_deinit`/`dal_ssd1306_deinit`/`dal_eeprom_deinit`/`dal_gps_deinit`；
   - 每个驱动配 host init→deinit→init 幂等单测；
   - codegen bus-owner 抽象；
   - ESP32 S11 deinit 循环真机验证。
   估 1.5-2 天。

2. **app 回调签名迁移**：新代码一律用 `init_status/on_fault_status`；legacy void 字段保留一个 release 过渡期，新 PR 卡口禁止用 legacy。

3. **stop 阻塞 500ms 上限**：MAY_BLOCK task 的 stop 等 sem 必须有超时（当前已 500ms，保持），fault Phase 2 总预算 ≤500ms 包含所有用户 stop 操作。

### 迁移/落地顺序硬约束

**Stage 0（DAL deinit 补全）是 Stage 1 及之后所有工作的硬前置依赖**——没有干净的 deinit，低功耗、bus-owner、deinit 循环测试全部无法工作；必须先验收通过才能进入 BAL 目录创建。

Stage 0 验收标准：
- 所有 7 个 DAL 驱动走完 §4 清场检查单；
- 每个驱动通过 host `init→deinit→init` 5-10 轮循环幂等单测（无资源泄漏、`initialized` 字段每次回到 true）；
- ESP32 真机：smoke S1-S10 行为与补 deinit 前一致；新增 S11 deinit 循环测试（init→deinit→init 跑 5 轮，不报 GPIO 占用、不 WDT）；
- ssd1306 + eeprom I2C 共享场景验证 bus-owner 正确（deinit ssd1306 不销毁 I2C bus，eeprom 仍可工作）。

---

## 遵循与后续（Compliance & Follow-up）

### 立即执行（Accepted 后）

1. **回写设计规范**：
   - `02-wink-micro-os/04-runtime-fault-model.md`：替换现有 fault 模型描述，加 Phase 1/2/3 时序图；
   - `02-wink-micro-os/03-device-abstraction-layer.md`：加 DAL deinit 清场检查单（§4）；
   - `03-app-codegen/` 相关文档：加 bus-owner 抽象描述、`wink_device_tree_deinit()` 语义。
2. **Stage 0 启动**：3 个 deinit 重写 + 4 个 deinit 新建 + 幂等单测 + bus-owner 抽象 + S11 真机验证。

### 实施期必做

- Reviewer 对照 §4 清场单逐行检查每个 deinit，**特别检查 `gpio_reset_pin` 覆盖所有引脚**（漏调是最常见 bug）；
- `wink_periodic_active_count()` 在 Stage 1 实现后，`wink_device_tree_deinit()` 入口必须加 WINK_PT_DEBUG 断言；
- 真机 S11 deinit 循环测试必做（5 轮 init→deinit→init 不报 GPIO 占用、不 WDT）。

### 不做（Out of Scope）

- NMI/HardFault 异常向量专用 minimal-safe-off 路径（Future Work，独立 ADR）；
- runtime 自动 stop BAL 服务（方案 A 已否决）；
- panic 后完整资源回收（板级安全电路 + WDT 复位是兜底，软件层不承诺）。

---

*本 ADR 状态变更请在此记录：*
- 2026-07-06：Proposed（基于 tech-design v5 Owner 决策版 + 2026-07-06 DAL deinit 代码走查起草）
- 2026-07-06：Accepted（Owner 审阅并采纳，并融入了早期 WDT/lockout 锁死检测前置的优化建议）

