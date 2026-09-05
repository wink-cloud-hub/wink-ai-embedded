# ADR-0017：阻塞 API 硬隔离(`WINK_BLOCKING` 属性 + `WINK_STRICT_NONBLOCKING` 符号剔除)

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-01（提议）；2026-07-02（采纳） |
| 触发 | [2026-07-01 外部综合评审批判性核验](../../reviews/core/2026-07-01-external-comprehensive-review-critique.md) §一.2.2 / §二.1 / §二.5；[PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3](../../implementation-plans/core/2026-07-01-wmos-code-optimization-q3-plan.md) Track E |
| 影响范围 | `pal/include/wink_status.h`(新增属性宏)；`dal/include/sensor/dal_ultrasonic.h` / `dal/src/sensor/dal_ultrasonic.c`(首个应用点);所有协作式调度器构建的 `samples/**/CMakeLists.txt` 与 `runtime_cooperative_*` 相关 target；`python wink-tools/wink.py test` L1 lint 追加 |
| 决策者 | 待定（架构委员会评审） |
| 关联评审 | [2026-07-01-external-comprehensive-review-critique](../../reviews/core/2026-07-01-external-comprehensive-review-critique.md) |
| 关联实施计划 | [PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3](../../implementation-plans/core/2026-07-01-wmos-code-optimization-q3-plan.md) §Track E（M3）、[2026-07-01-sim-cooperative-scheduler-plan](../../implementation-plans/unisim/2026-07-01-sim-cooperative-scheduler-plan.md)（第三层落地窗口） |
| 关联既有 ADR | [ADR-0007 协作式执行模型](0007-cooperative-loop-execution-model.md)、[ADR-0011 Protothread 深度防御](0011-protothread-footgun-defense-in-depth.md) |
| 关联设计规范 | `02-wink-micro-os/03-device-abstraction-layer.md`(Accepted 后回写 DAL API 稳定性章节);`07-platform-governance/*`(若有 API 稳定性章节) |

---

## 背景（Context）

WinkMicroOS 的核心执行模型是**协作式主循环**(ADR-0007):App/DAL 层在一个 10ms tick 内主动 yield,禁止长阻塞 API。当前 DAL 层已经完成"非阻塞化"重构——`dal_ultrasonic` 提供:

- **推荐**:`dal_ultrasonic_request_measurement` + `dal_ultrasonic_get_cached_distance`(状态机化)
- **@deprecated** :`dal_ultrasonic_read`(worst-case ≈60ms busy-wait,`dal_ultrasonic.h:104-115` 注释显式说明 "Not allowed in cooperative runtime loop")

**问题**:对**人**开发者,doxygen `@deprecated` + `@blocking` 注释信号足够;但对**AI Codegen** 而言:

1. AI 常从旧样例/旧 README grep 出 `dal_ultrasonic_read`,直接复用——注释语义 AI 抓不住(2026 年主流 SFT/RLHF 数据集中 doxygen `@blocking` 语义标注稀疏)。
2. 一旦协作式调度器(`2026-07-01-sim-cooperative-scheduler-plan.md`)上线,60ms 忙等会:
    - 在仿真里"看起来对"(Asyncify 会替 `pal_os_delay` 让步);
    - 在真机会**挂死 WDT**。
    这是典型的"两端不同源事故源",违反 ADR-0002 双 target 同源仿真的基本承诺。

### 现况的 `WINK_WARN_UNUSED_RESULT` 只覆盖返回值,不覆盖"是否可调用"

`dal_ultrasonic_read` 已经挂了 `WINK_WARN_UNUSED_RESULT`(`dal_ultrasonic.h:115`),但这只强制**返回值检查**,不阻止**调用本身**。AI Codegen 生成 `wink_status_t s = dal_ultrasonic_read(&dev, &d); if (wink_status_is_error(s)) { ... }` 完全合法——`WINK_WARN_UNUSED_RESULT` 不响,但 60ms 忙等照跑不误。

### 与 `dal_ultrasonic_read` 平行的其他 blocking API

当前项目中被标 `@blocking` 或 doxygen 显式说明 "busy-wait" 的 DAL/PAL API 数量不多(主要是 `dal_ultrasonic_read`),但未来会随硬件传感器扩展持续增加(温度传感器 DHT11 需要 20ms busy-wait 读取时序,RFID 需要 ms 级 SPI 轮询,等等)。**本 ADR 定的是"通用机制",而不是仅为超声波一个 API 定的补丁**。

---

## 方案比选（Options）

### 选项 A：仅 doxygen `@deprecated`(当前状态)

- ✅ 优点:零代码改动。
- ❌ 缺点(决定性):AI Codegen 从旧样例复用陷阱依然存在——参考 §背景,已被识别为北极星风险。
- ❌ 缺点:协作式调度器落地后 WDT 饿死事故会真实发生。

### 选项 B：三层硬隔离(推荐,M3 交付两层 + T5 交付第三层)

**三层保护**:

1. **编译期属性**:`__attribute__((deprecated(msg)))` 触发所有调用点编译警告(GCC / Clang);
2. **符号级剔除**:`-DWINK_STRICT_NONBLOCKING=1` 编译标志下,blocking API 从头文件声明中**消失**(`#ifndef` 包围),链接期报 undefined symbol;
3. **Runtime 检测**:protothread 上下文调用 blocking API 时,`WINK_PT_DEBUG` 下 `wink_trace_fault(WINK_ERR_PANIC) + assert`。

**M0 决策(2026-07-01,本 ADR 附加)**:M3(Track E)交付第一 + 第二层;**第三层延后到协作式调度器 T5 一起做**,因为:
- 第三层依赖 protothread 上下文的 TLS/state 检测机制,当前 protothread 层不存在这个检测点;
- `WINK_STRICT_NONBLOCKING` 符号剔除已覆盖协作式构建路径,第三层在此路径下是**冗余兜底**;
- 协作式调度器 T5 天然引入 PT context,那里加检测最经济。

- ✅ 优点:三层协同,AI Codegen 生成误用代码在**编译期即报错**(层 1) / **链接期即报错**(层 2) / **运行期抓 bug**(层 3),多重防护。
- ✅ 优点:对**人开发者**保留过渡期能力——非严格模式下仍可用(带 warning),便于单测场景。
- ✅ 优点:通用性——`WINK_BLOCKING` 属性可扩展应用到未来的 DHT11 / RFID / 慢速 SPI 等其他 blocking API。
- ⚠️ 代价:M3 需在 `wink_status.h` 新增两个宏,`dal_ultrasonic.h/c` 用 `#ifndef` 包围,协作式调度样例 CMakeLists 追加 `-DWINK_STRICT_NONBLOCKING=1`。
- ⚠️ 代价:第三层延后到 T5——期间协作式调度构建路径已由第二层符号剔除保护,无 gap。

### 选项 C：直接移除阻塞 API(激进)

- ✅ 优点:一劳永逸,永远不会被 AI Codegen 误用。
- ❌ 缺点(决定性):`dal_ultrasonic_read` 尚有过渡期依赖(`test_host_pal.c` 依赖它触发 echo 时间推进),host 单测生态无法一次性切换。
- ❌ 缺点:破坏兼容期承诺,不利于未来其他 blocking API 的过渡演进。

### 选项对比小结

| 维度 | A. 仅注释 | B. 三层硬隔离(推荐) | C. 直接移除 |
|-----|---------|---------------------|-----------|
| AI Codegen 防陷阱 | ❌ | ✅ 三重防护 | ✅ 一劳永逸 |
| 编译期检测 | ❌ | ✅ 属性 warning | N/A |
| 链接期检测 | ❌ | ✅ 符号剔除 | N/A |
| 运行期检测 | ❌ | ✅ PT panic(T5 交付) | N/A |
| 保留过渡期能力 | ✅ | ✅ 非严格模式下可用 | ❌ 直接砍 |
| 通用性(覆盖未来 blocking API) | ❌ | ✅ 机制通用 | ⚠️ 每个都得砍 |
| 迁移成本 | 0 | 中(M3 两层 + T5 一层) | 高(host 单测重构) |
| 与 ADR-0007 契合度 | ⚠️ | ✅ 强 | ✅ 强 |

**选择 B 的关键理由**:三层协同 + 分阶段交付 + 通用机制,兼顾"防陷阱"与"过渡期可运行"。

---

## 决策结论（Decision）

**采纳选项 B**:引入 `WINK_BLOCKING` 属性宏 + `WINK_STRICT_NONBLOCKING` 编译选项,分阶段交付三层硬隔离。

### 落地规则

#### 阶段一(M3 交付,本 ADR 主体范围)

1. **`wink_status.h` 新增属性宏**:

    ```c
    /**
     * @def WINK_BLOCKING
     * @brief Marks an API as blocking / busy-wait > runtime tick.
     *
     * - Under GCC/Clang/MSVC: emits deprecation warning at every call site.
     * - Under -DWINK_STRICT_NONBLOCKING=1: the declaration is removed
     *   from the header (see #ifndef guard in the API's header).
     *   Any call site linking fails with "undefined reference".
     *
     * Reserved for APIs that:
     *   - busy-wait more than a single 10ms runtime tick, OR
     *   - hold a hardware polling loop without yielding, OR
     *   - Otherwise violate the ADR-0007 cooperative execution contract.
     */
    #if defined(__GNUC__) || defined(__clang__)
        #define WINK_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
    #elif defined(_MSC_VER)
        #define WINK_DEPRECATED_MSG(msg) __declspec(deprecated(msg))
    #else
        #define WINK_DEPRECATED_MSG(msg)
    #endif

    #define WINK_BLOCKING \
        WINK_DEPRECATED_MSG("Blocking API forbidden in cooperative runtime; use non-blocking variant")
    ```

2. **`dal_ultrasonic.h` / `dal_ultrasonic.c` 应用**:

    ```c
    // dal_ultrasonic.h
    #ifndef WINK_STRICT_NONBLOCKING
    /**
     * @brief 获取障碍物距离 (cm) —— 阻塞 busy-wait, @deprecated.
     * @note Blocking: Yes. Worst-case ≈ 60ms. Not allowed in cooperative runtime loop.
     *       See dal_ultrasonic_request_measurement + get_cached_distance for non-blocking path.
     */
    WINK_BLOCKING WINK_WARN_UNUSED_RESULT
    wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm);
    #endif  /* WINK_STRICT_NONBLOCKING */
    ```

    ```c
    // dal_ultrasonic.c
    #ifndef WINK_STRICT_NONBLOCKING
    wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm) {
        /* 现有实现保持不变 */
    }
    #endif  /* WINK_STRICT_NONBLOCKING */
    ```

3. **协作式调度构建路径开启严格模式**:所有 `runtime_cooperative_*` 相关 target / sample 的 CMakeLists 追加:

    ```cmake
    target_compile_definitions(<target> PRIVATE WINK_STRICT_NONBLOCKING=1)
    ```

    识别范围:与协作式调度器计划(`2026-07-01-sim-cooperative-scheduler-plan.md`) T5 落地范围一致。

4. **CI L1 lint**:`python wink-tools/wink.py test` 追加验证——构建两次(默认 vs 严格),严格模式下 `nm`(或 objdump 符号表)不出现 `dal_ultrasonic_read`。

#### 阶段二(协作式调度器 T5 交付,本 ADR 授权但不在 M3 范围)

5. **Runtime PT 上下文检测与统一拦截宏（第三层）**:
    - **统一拦截宏设计**：为了避免未来每个慢速设备驱动各自实现重复的断言逻辑，在 `wink_status.h` 或 `wink_app.h` 中提供一个标准化的拦截宏 `WINK_ASSERT_NONBLOCKING()`：
      ```c
      #ifdef WINK_PT_DEBUG
      extern bool wink_pt_in_context(void);
      #define WINK_ASSERT_NONBLOCKING() do { \
          if (wink_pt_in_context()) { \
              wink_trace_fault(WINK_ERR_PANIC); \
              assert(!wink_pt_in_context() && "Fatal: Blocking API called within Protothread context!"); \
          } \
      } while (0)
      #else
      #define WINK_ASSERT_NONBLOCKING() ((void)0)
      #endif
      ```
    - **API 接入**：任何被判定为阻塞的 API，在 C 实现文件的入口首行直接写 `WINK_ASSERT_NONBLOCKING();` 即可。
    - **状态与实现**：在 protothread 框架层引入“当前是否在 PT 上下文”的局部或全局/TLS 状态标志 `wink_pt_in_context()`。**具体实现在协作式调度器计划 T5 阶段落地**。
    - **与 ADR-0016 协同**：此调试机制与 ADR-0016 引入的 `s_sim_in_isr` 调试标记互为多维度并发防护体系。在 Host/WASM 仿真层下：
      - `s_sim_in_isr` 用于卡口 Task/ISR 临界区入口误用；
      - `s_sim_in_pt`（即 `wink_pt_in_context()`）用于卡口协程内误调阻塞 API。

    延后理由(M0 决策):第三层依赖当前不存在的 PT 上下文检测机制;`WINK_STRICT_NONBLOCKING` 符号剔除已覆盖协作式构建路径,第三层是冗余兜底;协作式调度器 T5 天然引入 PT context,那里加检测最经济。

#### 通用应用规则

6. **哪些 API 应挂 `WINK_BLOCKING`**:
    - 单次调用 busy-wait > 一个 runtime tick(10ms)的;
    - 硬件轮询未主动 yield 的;
    - 违反 ADR-0007 协作式执行契约的。

    当前唯一应用点:`dal_ultrasonic_read`(worst-case ≈60ms)。未来应用点(候选):DHT11 温湿度读取、RFID 卡响应轮询、慢速 SPI Flash 擦除等阻塞路径。

7. **谁负责挂载**:新增 DAL/PAL API 时,若属于上述特征,**必须**由该 API 的 owner 在头文件加 `WINK_BLOCKING` 属性 + `#ifndef WINK_STRICT_NONBLOCKING` 包围。Code Review 卡口。

8. **禁止的实现路径(红线)**:
    - 🚨 **禁止**只加 `WINK_BLOCKING` 而不加 `#ifndef` 包围(仅编译警告不够,AI 会绕过)。
    - 🚨 **禁止**只加 `#ifndef` 而不加 `WINK_BLOCKING`(严格模式下编译失败信息不足,非严格模式下无警告)。
    - 🚨 **禁止**只提供 blocking 版本、不提供非阻塞 alternative——所有挂载 `WINK_BLOCKING` 的 API **必须**在同头文件提供非阻塞替代路径的引导注释(如 `@see dal_ultrasonic_request_measurement`)。

---

## 后果与约束（Consequences & Constraints）

### 正面后果

- ✅ AI Codegen 生成误用代码在编译期(warning) / 链接期(严格模式 undefined ref) 两层拦截,大幅降低"两端不同源事故"概率。
- ✅ `WINK_BLOCKING` 属性成为项目级通用机制,未来所有慢速传感器/慢速外设 API 一致模式接入。
- ✅ 协作式调度器 T5 落地时,构建路径默认开启严格模式,`dal_ultrasonic_read` 从符号表消失,WDT 饿死风险从**可能**降到**不可能**(除非人肉去掉 `-DWINK_STRICT_NONBLOCKING`)。
- ✅ 保留过渡期能力:host 单测(`test_host_pal.c` 依赖 `dal_ultrasonic_read` 触发 echo 时间推进)在非严格模式下继续可用。

### 负面后果 / 约束

- ⚠️ **过渡期需扫描 host 单测**:严格模式下 `dal_ultrasonic_read` 消失,若某个 test 依赖它,链接失败。Track E Task E-3 需前置扫描所有 test 中的 blocking API 使用(计划 R-004);host 默认构建**不**开严格模式,严格模式仅对协作式调度器构建路径开启。
- ⚠️ 严格模式与非严格模式的 diff 会被 CI 拉两次构建;构建时间 +~30%,可接受。
- ⚠️ 第三层(PT panic assert)在 M3 内**不交付**,由协作式调度器 T5 落地——期间协作式构建路径已由第二层保护,无 gap;但需 T5 阶段兑现,防止长期挂账。
- ⚠️ 非 GCC/Clang/MSVC 编译器上 `WINK_BLOCKING` 退化为空宏,失去编译警告能力——这是主流编译系统的成熟做法,非主流编译器上开发者靠 `#ifndef` 严格模式兜底。

### Code Generation 指南

Codegen 生成慢速传感器代码时:

```c
/* ✅ 推荐:使用非阻塞状态机 API */
static struct dal_ultrasonic dev;

WINK_PT_DECL(pt_measure);
static WINK_PT_STATE(pt_measure) {
    WINK_PT_BEGIN(pt_measure);

    (void)dal_ultrasonic_request_measurement(&dev);
    WINK_PT_WAIT_UNTIL(pt_measure,
        dal_ultrasonic_get_cached_distance(&dev, &d) == WINK_OK);

    /* ...使用 d... */

    WINK_PT_END(pt_measure);
}

/* ❌ 禁止:调用 blocking API,严格模式下直接链接失败 */
void app_loop(...) {
    float d;
    wink_status_t s = dal_ultrasonic_read(&dev, &d);  /* -Wdeprecated warning
                                                        + 严格模式 undefined symbol */
}
```

未来 codegen prompt few-shot 应包含此模式(见 Track C Task C-4)。

---

## 遵循与后续（Compliance & Follow-up）

### Accepted 后立即执行

1. 启动实施计划 §Track E(M3,1.5 天,两层交付),按 Task E-1 → E-3 执行。
2. **回写至 `02-wink-micro-os/03-device-abstraction-layer.md`** DAL API 稳定性章节:说明 `WINK_BLOCKING` 属性用法与协作式调度构建约束。
3. **回写至 `07-platform-governance/*`(若有 API 稳定性章节)**:把 `WINK_BLOCKING` 属性纳入 API 稳定性正式约束。
4. **回写至 `.claude/skills/embedded-best-practice/`**:新增"blocking API 硬隔离模式"条目,作为未来慢速外设 API 参考。
5. 更新 codegen prompt few-shot(若接入点确定):增加"禁止使用 `WINK_BLOCKING` 属性 API"约束示例。

### 未来推进(协作式调度器 T5 阶段)

6. 完成 third 层——PT 上下文检测 + runtime panic assert。该步骤已授权,不需另开 ADR。

### 与其他 ADR 的关系

- **ADR-0007**(协作式执行模型):本 ADR 是 ADR-0007 契约在 API 层面的强制执行工具——从"约定"升级为"编译期防线"。
- **ADR-0011**(Protothread 深度防御):第三层 PT 上下文检测与 ADR-0011 的深度防御精神一致,协作式调度器 T5 阶段延续该思路。
- **ADR-0015 / ADR-0016**(本次同批提议):三份 ADR 都是 Q3 优化包的部分,ADR 与代码在同一或相邻 PR 内落地(实施计划 R-6 红线)。

### 与协作式调度器计划的协同

本 ADR 的严格模式默认开启**必须**与 `2026-07-01-sim-cooperative-scheduler-plan.md` T5 落地同一 PR 或**相邻 PR**——避免调度器上线时 sample 编译失败(计划 R-8 红线)。M3 完成后本 ADR 的两层保护已就绪,T5 只需追加 `-DWINK_STRICT_NONBLOCKING=1` 到相关 target 即可生效。

---

*本 ADR 状态变更请在此记录:*
- 2026-07-01:Proposed(伴随 PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3 提出;M3 交付前两层,第三层延后到协作式调度器 T5)
- 2026-07-02:Accepted(架构委员会通过；用户 review 补充 MSVC `__declspec(deprecated(msg))` 分支与 T5 阶段统一拦截宏 `WINK_ASSERT_NONBLOCKING()` 设计,避免未来慢速设备驱动重复实现 assert 逻辑；同 commit 内回写至 `02-wink-micro-os/01-dal-device-abstraction.md` §DAL 稳定性章节)

