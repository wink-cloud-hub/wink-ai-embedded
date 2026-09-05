# ADR-0015：PAL GPIO Read/Write 升级为 `wink_status_t + out-param`（消除 `bool/void` 遗留签名）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-01（提议）；2026-07-02（采纳） |
| 触发 | [2026-07-01 外部综合评审批判性核验](../../reviews/core/2026-07-01-external-comprehensive-review-critique.md) §一.2.3 / §二.2；[PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3](../../implementation-plans/core/2026-07-01-wmos-code-optimization-q3-plan.md) Track B |
| 影响范围 | `pal/include/hal/pal_hal.h`；`targets/{host,wasm,esp32}/pal_hal_*_gpio.c` / `pal_hal_wasm.c` / `pal_hal_host.c` / `pal_hal_ultrasonic.c`；`dal/src/{input/dal_button.c, output/dal_led.c}`；`pal/include/hal/pal_hal_rmt.h` doxygen 示例；`test/**` 中所有 `pal_gpio_read/write` 直接调用点；`samples/oled_dashboard/`, `samples/devkitc_smoke/` 中相关注释 |
| 决策者 | 待定（架构委员会评审） |
| 关联评审 | [2026-07-01-external-comprehensive-review-critique](../../reviews/core/2026-07-01-external-comprehensive-review-critique.md) |
| 关联实施计划 | [PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3](../../implementation-plans/core/2026-07-01-wmos-code-optimization-q3-plan.md) §Track B（M4） |
| 关联既有 ADR | [ADR-0001 错误码符号约定](0001-error-code-sign-convention.md)、[ADR-0012 契约诚实优于静默降级](0012-contract-honesty-over-silent-degradation.md)（Proposed）、[ADR-0004 静态分发](0004-static-dispatch-vs-runtime-ops.md) |
| 关联设计规范 | `02-wink-micro-os/02-pal-platform-abstraction.md`（Accepted 后回写 GPIO API 章节） |

---

## 背景（Context）

WinkMicroOS 的 PAL/HAL 层在 2026-06-30 之前经历了一轮"契约诚实"改造（ADR-0012、v2.2 中断子系统重构），把 `pal_gpio_init`、`pal_pwm_init`、`pal_pwm_set_duty`、`pal_gpio_enable_interrupt_ex` 等接口统一升级到 `wink_status_t` 返回码 + `WINK_WARN_UNUSED_RESULT` 属性。**但两处遗漏未同步**（`pal_hal.h:61,63`）：

```c
void pal_gpio_write(wink_pin_t pin, bool level);       /* 无返回值 */
bool pal_gpio_read(wink_pin_t pin);                    /* 布尔电平 */
```

### 现况带来的三类问题

1. **`pal_gpio_write` 无法上报失败**：写未 claim 引脚 / mux 冲突 / 越界引脚 / 硬件故障，一律静默丢失。`WINK_WARN_UNUSED_RESULT` 无法覆盖 `void` 返回，AI Codegen 生成的调用代码即使写"检查错误码"的规范风格也无处下手。
2. **`pal_gpio_read` 用 `bool` 混淆电平与错误**：读失败（浮空/短路/mux 冲突/越界引脚）返回的 "false" 与真实的低电平**不可区分**。DAL 层 `dal_button.c:28` `bool raw = pal_gpio_read(dev->config.pin);` 就活在这个盲区里。
3. **API 一致性外观破损**：`pal_gpio_init` 已升 `wink_status_t`，同头文件同模块的 `read/write` 反其道而行，AI Codegen 会**继承这种不对称模式**——生成的样例出现"init 处理错误、read/write 不处理错误"的写法，日后无法反向纠正。

### 为什么现在必须解决

Track B 是跨 target + 跨 DAL 的公共 API 变更。当前直接调用点仅 **2 个 DAL 源文件**（`dal_button.c:28`、`dal_led.c:28`）+ 若干 target 内部逻辑 + 一批 test 文件。随着协作式调度器（`2026-07-01-sim-cooperative-scheduler-plan.md`）落地会**批量生成新样例**，此后每 sprint 迁移成本指数级增长。**这是最后的窗口**。

---

## 方案比选（Options）

### 选项 A：双 API 并存（新签名 + 旧签名 deprecated 别名）

- ✅ 优点：现有调用者无须一次性迁移；迁移可以温和推进。
- ❌ 缺点（决定性）：与本项目"AI Codegen 友好"北极星直接冲突——AI 从旧样例 grep 到 deprecated 别名会**继续复用**（AI 抓不住 `@deprecated` 注释语义，就像 Track E `dal_ultrasonic_read` 现况一样）。这会把 §2 里刚要消灭的问题重新种回来。
- ❌ 缺点：两套 API 并存 = 头文件复杂度翻倍 + doxygen 分裂 + 内部 sample 各种写法混跑，样例代码 SSOT 崩坏。
- ❌ 缺点：本项目并非对外发布的商业库，无外部客户 lock-in 需要迁移期，成本仅在项目内消化。

### 选项 B：硬切换到 `wink_status_t + out-param`（推荐）

新签名：

```c
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_write(wink_pin_t pin, bool level);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level);
```

- ✅ 优点：与 `pal_gpio_init` / `pal_pwm_*` 完全对称，AI Codegen prompt 只需说"所有 `pal_*` 函数返 `wink_status_t`",无例外条款。
- ✅ 优点：`WINK_WARN_UNUSED_RESULT` 强制返回值检查，DAL 层错误直接透传（`s = pal_gpio_read(pin, &raw); if (wink_status_is_error(s)) return s;`）。
- ✅ 优点：读失败与低电平**可辨**——`out_level` 只在 `WINK_OK` 时有意义,错误路径 `out_level` 保持未写入（doxygen 契约）。
- ⚠️ 代价：一次性迁移 4 个 target 实现 + 2 个 DAL + 4~5 个 test 文件 + samples 若干注释。经计划 §3.1.2 逐条排查,调用点数量可控（估计 20~30 处）,可在一个 sprint 内完成。
- ⚠️ 代价：`pal_hal_esp32_gpio.c:352,359` 中 `pal_gpio_wait_level` 内部循环需处理 read 错误码（wait 失败提前退出返 `WINK_ERR_IO`）——这是本次改动**唯一有语义决策的点**，属于 §决策落地规则 4。

### 选项 C：引入 `result_t { status, value }` 结构体返回（result monad）

```c
typedef struct { wink_status_t status; bool value; } pal_gpio_read_result_t;
pal_gpio_read_result_t pal_gpio_read(wink_pin_t pin);
```

- ✅ 优点：调用点写法紧凑（无 out-param）。
- ❌ 缺点（决定性）：**与 `wink_status_t` 生态不兼容**。本项目现有 PAL 层几十个 `WINK_WARN_UNUSED_RESULT wink_status_t` 接口都不用 result monad，独此一处例外会形成认知负担与命名分裂。
- ❌ 缺点：AI Codegen 需要额外样例学习 `.status` / `.value` 字段访问模式,与 out-param 相比无收益。
- ❌ 缺点：C 语言 ABI 下小结构体返回值的调用约定与目标编译器耦合（xtensa / wasm32 / x86-64 各不同），排错难度上升。

### 选项对比小结

| 维度 | A. 双 API 并存 | B. 硬切换（推荐） | C. Result monad |
|-----|---------------|-----------------|------------------|
| AI Codegen 友好 | ❌ 训练数据混乱 | ✅ 一致 | ⚠️ 需额外样例 |
| 迁移成本 | 低（分批） | 中（一次到位） | 高（+ ABI 风险） |
| 与现有生态一致性 | ⚠️ 双写 | ✅ 完全对称 | ❌ 独此例外 |
| 长期维护成本 | ❌ 长尾债务 | ✅ 一次结清 | ⚠️ 命名分裂 |
| 与 ADR-0012 "契约诚实"契合度 | ⚠️ 头文件说"deprecated"= 契约模糊 | ✅ 强 | ✅ 强 |

---

## 决策结论（Decision）

**采纳选项 B**：硬切换 `pal_gpio_read/write` 到 `wink_status_t + out-param`。

### 落地规则

1. **签名重构（`pal_hal.h`）**：

    ```c
    WINK_WARN_UNUSED_RESULT
    wink_status_t pal_gpio_write(wink_pin_t pin, bool level);

    WINK_WARN_UNUSED_RESULT
    wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level);
    ```

2. **错误码语义（预定义,须与 ADR-0017 的错误码手册对齐）**：
    - `WINK_OK`：操作成功；`read` 时 `*out_level` 已写入。
    - `WINK_ERR_INVALID_ARG`：`pin` 越界 / `read` 的 `out_level == NULL`。
    - `WINK_ERR_INVALID_STATE`：`pin` 未通过 `pal_resource_claim` 登记。**前置约束**：在 host/wasm 仿真层下必须强校验，未登记引脚直接读写必须返回此错，以便在 Host 单测阶段及早暴露引脚冲突或配置漏写。
    - `WINK_ERR_IO`：硬件故障（真机；host/wasm 通常不返回）。

3. **`out_level` 契约与防御性安全设计**：
    - **契约定义**：`read` 返 `WINK_OK` 时 `*out_level` 的写入值有效；返错时，其写入值在契约上被视为无效/未定义。这确保了静态分析工具（如 scan-build）可以通过“未初始化局部变量的使用”检测出漏检返回值的情况。
    - **防御性实现**：为了防止在运行时由于某些原因避开了 `WINK_WARN_UNUSED_RESULT` 校验或忽略了返回值，而在局部变量未初始化的状态下直接使用 `out_level` 触发未定义行为（UB），`pal_gpio_read` 在实现时的**最开始处**（或在返回非 `WINK_OK` 前），必须将 `*out_level` 显式写入安全默认值 `false`。

4. **`pal_hal_esp32_gpio.c:352,359` 中 `pal_gpio_wait_level` 内部循环**：将 `while (pal_gpio_read(pin) != level)` 重构为:

    ```c
    bool cur;
    while (1) {
        wink_status_t s = pal_gpio_read(pin, &cur);
        if (wink_status_is_error(s)) { return WINK_ERR_IO; }
        if (cur == level) { break; }
        /* ... timeout check ... */
    }
    ```

    read 错误视为"wait 失败提前退出"，返 `WINK_ERR_IO`（不返 read 的原码,统一 wait 语义）。

5. **`pal_hal_host.c` 副作用不变**：host 侧 `pal_gpio_read` 现有的"echo 边沿虚拟时间推进"副作用（`pal_hal_host.c:43-58,491`）**必须原样保留**。这是过渡期 `dal_ultrasonic_read` blocking API 在 host 下的时间推进机制,已被 `test_host_pal.c:63-64` 依赖。签名重构后行为不变、只是返回码路径新增,回归靠 `test_host_pal.c` 兜底。

6. **禁止的实现路径（红线）**：
    - 🚨 **禁止保留 deprecated 别名**（对应 §方案比选 A）。全量迁移或不做。
    - 🚨 **禁止签名 + 兼容 wrapper 并存**（如 `static inline bool pal_gpio_read_legacy(pin)`）。

7. **迁移执行范围**：所有直接与间接调用点(依据实施计划 §3.1.2 台账),包括:
    - 3 target 实现:`pal_hal_host.c` / `pal_hal_wasm.c` / `pal_hal_esp32_gpio.c`（含 `ESP_PLATFORM=0` stub）
    - 2 DAL 直接调用:`dal_button.c` / `dal_led.c`
    - PAL 内部间接调用:`pal_hal_ultrasonic.c` / `pal_hal_esp32_gpio.c` 内部 `pal_gpio_wait_level`
    - 测试代码:`test/test_button_debounce_e2e.c` / `test/test_host_pal.c` / `test/test_sim_physical.c` / `test/wasm/test_button_debounce_e2e_wasm.c` / `test/wasm/test_debounce_middleware.c`
    - 文档注释:`pal_hal_rmt.h:33,35` doxygen 示例；samples 中相关注释

8. **ESP32 物理硬件层适配细节**：
    - 由于 ESP-IDF 原生的 `gpio_get_level` 只返回 `int`（0 或 1，无错误码返回），在 ESP32 目标上的 `pal_gpio_read` 必须手动调用 `GPIO_IS_VALID_GPIO(pin)` 宏校验引脚的物理合法性，如果不合法则返回 `WINK_ERR_INVALID_ARG`。
    - `pal_gpio_write` 内部则应捕获 `gpio_set_level` 返回的 `esp_err_t` 并对应转换（如 `ESP_ERR_INVALID_ARG` 转换为 `WINK_ERR_INVALID_ARG`）。

---

## 后果与约束（Consequences & Constraints）

### 正面后果

- ✅ PAL 层 GPIO 接口一致性完全对齐:所有 `pal_gpio_*` 接口 100% 返回 `wink_status_t`；`WINK_WARN_UNUSED_RESULT` 编译期强制覆盖。
- ✅ AI Codegen prompt 可将"所有 PAL 接口返 `wink_status_t`"作为无例外的类型规则,不再需要针对 read/write 写额外条款。
- ✅ DAL 层可在错误路径优雅退出（透传状态码到 wink_app callback）,不再吞掉硬件异常。
- ✅ 与 Track A（`pal_resource` 接线闭环）协同——未 claim 引脚的 read/write 可直接返 `WINK_ERR_INVALID_STATE`,把"引脚 mux 冲突"从真机偶发提前到 host 单测阶段暴露。

### 负面后果 / 约束

- ⚠️ **一次性迁移**成本集中在一个 sprint（M4,5 天）,期间涉及 4 个 target + 5+ 个 test 文件 + 2 个 DAL 直接调用点。
- ⚠️ 迁移期若某调用点被漏掉,`-Werror` 编译会直接失败——这是**故意的**（Track B-4 集成回归卡口）,但需要 M4 起步时全量 grep 台账（Task B-1）作为前置任务。
- ⚠️ 迁移期 `pal_gpio_wait_level` 语义微调:read 错误被"提升"为 wait 错误。历史上 read 从未真正失败过（host/wasm 无失败路径,ESP32 底层 gpio_get_level 也无返回码）,故实际行为等价,但需 `test_pal_gpio_wait_level`（若存在）回归验证。
- ⚠️ ADR-0012 未 Accepted 前,本 ADR 关联条目为"Proposed",不影响本 ADR 自身生效。

### Code Generation 指南

Codegen 生成 GPIO 调用代码时:

```c
/* ✅ 推荐写法（read） */
bool level;
wink_status_t s = pal_gpio_read(dev->config.pin, &level);
if (wink_status_is_error(s)) { return s; }
/* ...使用 level... */

/* ✅ 推荐写法（write） */
wink_status_t s = pal_gpio_write(dev->config.pin, true);
if (wink_status_is_error(s)) { return s; }

/* ❌ 禁止写法：忽略返回值 */
pal_gpio_write(pin, level);          /* WINK_WARN_UNUSED_RESULT 编译警告 */
bool raw;
pal_gpio_read(pin, &raw);            /* 同上 */

/* ❌ 禁止写法：错误路径读取 out_level */
bool level;
pal_gpio_read(pin, &level);
if (level) { ... }                    /* 未检查 status，且 level 在出错时无效（虽然实现会防御性写入 false，但契约上不可用且漏检了错误状态） */
```

---

## 遵循与后续（Compliance & Follow-up）

### Accepted 后立即执行

1. 启动实施计划 §Track B（M4,5 天）,按 Task B-1 → B-4 执行。
2. **回写至 `02-wink-micro-os/02-pal-platform-abstraction.md`** §GPIO API 章节:更新 `pal_gpio_read/write` 签名文档,与 `pal_gpio_init` 章节风格对齐。
3. **回写至 `.claude/skills/embedded-best-practice/`**:在"PAL/HAL 接口返回值约定"条目下,把 GPIO read/write 加入"已完成升级"清单。
4. 更新 codegen prompt few-shot(若接入点确定,参考 Track C Task C-4):增加 GPIO read/write 新签名示例。

### 与其他 ADR 的关系

- **ADR-0001**:本 ADR 是 ADR-0001 "负数错误码"约定在 GPIO 层的具体应用。
- **ADR-0012**（Proposed）:本 ADR 遵循 ADR-0012 "契约诚实"原则——不选择"静默丢弃写失败"的降级路径,而是显式错误码上报。
- **ADR-0016 / ADR-0017**(本次同批提议):三份 ADR 都是 Q3 优化包的部分,ADR 与代码在同一或相邻 PR 内落地(实施计划 R-6 红线)。

### 兼容性说明

- 本 ADR 落地后,`bool pal_gpio_read(...)` / `void pal_gpio_write(...)` 签名从 PAL 头文件中**完全消失**（无 deprecated 别名）。
- 项目内所有引用一次性迁移;项目外部无引用者(项目未作为 SDK 对外发布)。
- 迁移期 CI 卡口:`-Werror` 编译 + 全量 grep `bool pal_gpio_read\|void pal_gpio_write` 命中数必须为 0。

---

*本 ADR 状态变更请在此记录:*
- 2026-07-01:Proposed(伴随 PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3 提出)
- 2026-07-02:Accepted(架构委员会通过；用户 review 补充"防御性 out_level 初始化"、"host/wasm 强校验未登记引脚"、"ESP32 `GPIO_IS_VALID_GPIO` 校验 + `esp_err_t → wink_status_t` 显式转换"三处落地细节；同 commit 内回写至 `02-wink-micro-os/02-pal-platform-abstraction.md` §GPIO API 章节)

