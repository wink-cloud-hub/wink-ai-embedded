# embedded-best-practice 深度评审（Round 2）

> 评审对象：`.claude/skills/embedded-best-practice/`
> 评审视角：资深嵌入式架构师。Round 1（`todolist/embedded-best-practice-review.md`）的 10 条 P0/P1 建议已全部落地（契约模板 / lifecycle / simulation / grilling 等），本轮聚焦 Round 1 没覆盖到的**第二层问题**：内部自相矛盾、示例与现状脱节、体系缺口、结构性技术债。
> 评审方法：通读全部 18 个文档 + 核验 `wink-micro-os/` 实际代码 + 对照 ADR 与 c-code.md。

---

## 严重性分级说明

| 级别 | 含义 |
|------|------|
| 🔴 **致命** | 会直接导致 AI 生成的代码编译失败 / 运行时崩溃，或文档自相矛盾到无法自洽 |
| 🟠 **高** | 体系裂缝或工程化缺口，不修会在规模化后持续踩坑 |
| 🟡 **中** | 增强项 / 结构债，影响可维护性与 AI 引导精度 |

---

# 第一部分：必须修的真问题（有据可查，会误导 AI）

---

## 🔴 问题 1：示例代码引用了实际结构体里**不存在**的 `state` 嵌套字段

### 位置
- `references/static-dispatch/lifecycle.md:32-53`（config/state 分离设计 + codegen 实例化）
- `references/static-dispatch/simulation.md:44`（`dev->state.last_distance = *distance_cm;`）
- `references/static-dispatch/evolution.md:80, 126`（`dev->state.last_distance` / `dev->state.last_value`）

### 现状证据

skill 把「`const` 配置区 + `struct state {}` 可变区」当作目标形态反复示范：

```c
/* lifecycle.md:25-36 —— 目标形态 */
typedef struct {
    const uint8_t  pwm_channel;
    const float    min_pulse_ms;
    const float    max_pulse_ms;
    struct {
        float         current_angle;
        wink_status_t last_status;
    } state;
} dal_servo_t;
```

但**实际头文件是扁平结构，既无 `state` 嵌套，字段也非 `const`**：

```c
/* dal_ultrasonic.h:18-22（实际） */
typedef struct {
    uint16_t trig_pin;
    uint16_t echo_pin;
    float last_distance;        /* 顶层字段，不是 state.last_distance */
} dal_ultrasonic_t;

/* dal_servo.h:19-24（实际） */
typedef struct {
    uint8_t pwm_channel;
    float current_angle;        /* 顶层字段，不是 state.current_angle */
    float min_pulse_ms;
    float max_pulse_ms;
} dal_servo_t;
```

### 内部矛盾

更严重的是 **skill 内部三处文档对同一结构体自相矛盾**：

| 文档 | 描绘的形态 |
|------|-----------|
| `architecture.md:48-49, 76-79` | 扁平（与实际一致） |
| `lifecycle.md` / `simulation.md` / `evolution.md` | `state.*` 嵌套（与实际不符） |

### 影响

这是一个以「**AI 生成友好**」为 P0 卖点的 skill，却给出了 AI 照抄会**直接编译失败**的示例——AI 读到 `evolution.md` 的「迁移 good 示例」去改 `dal_ultrasonic.c`，写 `dev->state.last_distance = ...` 会立刻 `error: no member named 'state'`。这恰恰违背了 skill 存在的全部意义。

此外，`README.md:46-58` 的「代码现状 vs 目标」偏差框**漏列了这一项**——它只列了返回类型、API 命名、device_tree、SIMULATION 四项 drift，没有「结构体未做 config/state 分离」这一项最直观的 drift。

### 建议修复（二选一或组合）

**方案 A（推荐，改动小）：给示例加现状标注**
1. `README.md` 偏差框补一行：
   `| 结构体形态 | 扁平字段（`last_distance` 在顶层，字段非 const） | const 配置区 + `struct state{}` 可变区分离 | lifecycle.md |`
2. 在 `lifecycle.md` / `simulation.md` / `evolution.md` 所有 `dev->state.*` 示例上方加一行注释：`/* 目标形态；现状为扁平字段 dev->last_distance，迁移见 evolution.md */`
3. `evolution.md` 补一个「扁平 → config/state 分离」的 before/after 迁移示例（与已有的「callback 改造」「消除 float 哨兵」并列）。

**方案 B（彻底）：把 `state.*` 示例全部改成与现状一致的扁平写法**，把 config/state 分离降级为 evolution.md 里的「未来演进方向」，不当现行模板。

---

## 🔴 问题 2：`WINK_WARN_CONFIG_CORRUPT` 引入了正数 warning，但错误码体系从未定义 warning 段

### 位置
- `references/static-dispatch/grilling.md:56`：`"返回带有降级标记的 WINK_WARN_CONFIG_CORRUPT"`
- 相关根定义：`references/shared/error-codes.md:9, 43-44`、`.claude/rules/c-code.md` §1、ADR-0001

### 现状证据

skill 的核心错误码语义是**绝对二分**：

```text
error-codes.md:9   —— "所有可能失败的函数返回状态码：0 = 成功，负数 = 错误"
error-codes.md:43  —— wink-micro-os 布局：0=WINK_OK；-1..-11 常见可恢复；
                                          -20..-29 功能安全可恢复；-30..-49 致命；-99=PANIC
c-code.md §1       —— "0 = Success, Negative = Errors"
```

**整个体系里没有「正数 = 警告」这一段。** 但 grilling.md 凭空引入了 `WINK_WARN_CONFIG_CORRUPT`——从命名（`WARN` 而非 `ERR`）和语义（「配置损坏但仍降级运行」）看，它隐含是**正数**（因为按现有约定，负数就是错误，而这里明确「不算错误、要继续运行」）。

### 影响（真实裂缝）

这会同时打破 skill 反复强调的两种正确检查方式：

| 检查写法 | 对 `WINK_WARN_*`（假设为正数）的行为 | 问题 |
|---------|--------------------------------------|------|
| `if (status < 0)`（skill 头号推荐） | 把 warning 当成 **WINK_OK 成功** | 配置损坏这种安全相关事件被静默吞掉 |
| `if (status != WINK_OK)` | 把 warning 当成 **错误** | App 无法走「带警告正常降级」路径，只能走错误恢复 |

两种被推荐的写法对这个值**语义都不对**。而 `error-codes.md` 通篇没有告诉 AI「warning 段存在、该如何检查」。

同时 `architecture.md:135` 提到的 `WINK_ERR_FAILED_INIT` 用 `ERR` 前缀（应为负数），与 `WINK_WARN_*` 的 `WARN` 前缀（应为正数）在同一 docset 里**前缀约定不一致**——AI 无法从前缀推断检查方式。

### 建议修复

二选一，但必须**先把 warning 语义说清**：

**方案 A（推荐，保持根假设不变）：可降级状态也归入负数段**
- 新增 `-5xx` 或 `-6xx` 段为「可恢复降级警告」，如 `WINK_ERR_CONFIG_CORRUPT_DEGRADED = -50`。
- 优势：`if (status < 0)` 语义保持绝对统一，不破坏 CI 正则 / AI 禁令 / 迁移指南对这个根假设的全面依赖。
- App 用「`status == -50` → 降级但继续」做特判，其余 `<0` 走错误恢复。

**方案 B（扩展体系）：正式定义 `>0` = 成功带警告段**
- 在 `error-codes.md` 新增「`WINK_OK = 0`，`>0` = 成功但带警告（degraded success）」段。
- **必须同步更新**所有 `< 0` vs `!= WINK_OK` 的使用指引，明确：
  - `if (status < 0)` → 是否考虑了 warning？
  - 何时该用 `if (status <= WINK_OK)` 之类。
- 工作量大，且容易引入新的歧义，不推荐。

**无论哪种方案**，都要修 grilling.md 的用词，使 `WARN`/`ERR` 前缀与所选段位一致。

---

## 🟠 问题 3：`__attribute__((warn_unused_result))` 缺便携化封装

### 位置
- `SKILL.md:146`（AI 禁令 3）：`"所有返回 wink_status_t 类型的 API 必须使用 __attribute__((warn_unused_result)) 进行修饰"`

### 现状证据

`__attribute__((warn_unused_result))` 确实被 GCC（xtensa/ESP-IDF）和 clang（emcc）同时支持，**技术上不破坏 ADR-0002 双 target**。但直接在业务头文件里裸写 `__attribute__` 有两个张力：

1. 与 `clean-code.md` / `tooling.md:31` 反复强调的「**禁用 clang-only / GCC-only 特性、两套工具链都过**」精神有张力——虽然是公共特性，但裸 `__attribute__` 仍是「编译器特有语法」。
2. 项目若未来引入 MSVC/MSVC-clang 工具链（如 Windows 原生 host 单测），裸 `__attribute__` 直接编译失败。

### 影响

中等。功能层面目前能用，但属于「文档示范了一个不够干净的写法」，且 AI 会照抄裸 `__attribute__` 扩散到全代码库，后续治理成本累积。

### 建议修复

1. 在 `wink_status.h` 定义便携宏（若尚无）：
   ```c
   #if defined(__GNUC__) || defined(__clang__)
       #define WINK_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
   #else
       #define WINK_WARN_UNUSED_RESULT
   #endif
   ```
2. 在 `contracts.md` 或 `lifecycle.md` 给出宏定义模板作为标准做法。
3. `SKILL.md:146` 措辞从「使用 `__attribute__((warn_unused_result))`」改为「使用 `WINK_WARN_UNUSED_RESULT` 宏修饰」。

---

## 🟠 问题 4：CI 正则 `struct\s+\w+_ops\b` 的白名单是空的，抓不到本项目唯一的合法 vtable

### 位置
- `references/shared/tooling.md:57-60`（CI 正则门禁表 + 例外白名单）

### 现状证据

tooling.md 的正则与白名单：

| 禁止模式 | 合法例外 |
|---------|---------|
| `struct\s+\w+_ops\b`（器件抽象 ops 虚表） | `control_algo_t` 策略层（封装在模块内） |

问题在于**白名单和正则对不上**：

- `control_algo_t` 在 `templates.md:151-157` 是这样定义的：
  ```c
  typedef struct {
      void *(*create)(void);
      ...
  } control_algo_t;       /* 匿名 struct，无 struct tag 名 */
  ```
- 它**没有** `struct xxx_ops` 这样的形式（匿名结构体 + `control_algo_t` 类型名，类型名不含 `_ops`）。
- 因此正则 `struct\s+\w+_ops\b` **永远不会命中** `control_algo_t`——白名单这一行是**空操作（vacuous）**，写了等于没写。

更本质的问题：**运行期多态的真正危险信号不是「类型名带 _ops」，而是「实例里有 ops 指针字段 + 通过 `->ops->` 间接调用」**。当前正则只盯类型名，漏掉了字段名和调用点。

### 影响

正则门禁是 skill 自称的「防 AI 翻车的第一道闸」，但这条对真正的器件层 vtable（字段叫 `ops`、通过 `dev->ops->xxx()` 调用）**抓不住**，却给了「我有白名单」的虚假安全感。

### 建议修复

1. **扩大正则覆盖类型名 + 字段 + 调用点**：
   ```text
   (\bstruct\s+\w+_ops\b|\b\w+_ops_t\b|\.\s*ops\s*->|->\s*ops\s*->)
   ```
2. **白名单从「文件名 / 类型名」改为「行级显式标注」**，更精确且可审计：
   ```c
   h->algo->update(h->instance, out, setpt); /* lint-allow: strategy-vtable */
   ```
3. 在 tooling.md 表里注明：合法 vtable（策略模式）**必须**带 `lint-allow` 行级豁免，否则 CI fail。

---

# 第二部分：体系性缺口（当前 skill 的盲区）

---

## 🟡 问题 5：缺「器件健康状态（health）统一模型」

### 位置
- `references/static-dispatch/architecture.md:128-135`（init 失败降级策略）
- `references/static-dispatch/lifecycle.md:57-63`（init 幂等性，零散 `bool initialized`）

### 现状证据

architecture.md 描述了降级机制：DAL init 失败 → 标记 `WINK_ERR_FAILED_INIT` → App 读到后启动防跌落/停机降级。但**没有定义 App 如何统一查询「这个器件现在是 OK / DEGRADED / FAULTED」**。

lifecycle.md 只有零散的 `last_status`、`bool initialized` 字段，每个器件各自为政。静态分发下做统一巡检的标准做法（一个 `dev->state.health` 枚举 + X-macro 遍历表）在 skill 里是缺的。

讽刺的是，`templates.md` 形态 5（X-macro 批量遍历）已经给了「遍历所有超声波统一 init」的骨架，只差「遍历时查询 health」这一步——基础设施有了，语义模型没补。

### 影响

中。当器件数量上来（grilling.md 问题 1 就假设 8 个超声波），App 无法用统一代码做「扫描所有器件、跳过故障的、对降级的走保守逻辑」——只能为每个器件写特判，违背 skill 自己宣讲的「表驱动 / DRY」。

### 建议修复

在 `lifecycle.md` 补一节「器件健康状态机」：

```c
typedef enum {
    DAL_HEALTH_OK        = 0,   /* 初始化成功、运行正常 */
    DAL_HEALTH_DEGRADED  = 1,   /* 部分功能受限（如 NVS 损坏用默认值、单通道失效） */
    DAL_HEALTH_FAULTED   = 2,   /* init 失败或运行期故障，App 必须降级/停用 */
} dal_health_t;

/* 每个器件 state 区统一含： */
dal_health_t health;
```

并给出 App 统一巡检入口（复用 X-macro 形态 5）：

```c
#define X_CHECK_HEALTH(name) \
    if (name.state.health >= DAL_HEALTH_FAULTED) { app_quarantine(&name); }
ULTRASONIC_DEVICES(X_CHECK_HEALTH)
#undef X_CHECK_HEALTH
```

---

## 🟡 问题 6：缺「上电自检（POST）」环节

### 位置
- `references/shared/safety-checklist.md:35`（阶段 8 硬件交互，仅单次验证）

### 现状证据

安全关键嵌入式系统的标配是 POST（Power-On Self-Test）：上电后按序对关键器件做自检、隔离故障件、向 App 暴露自检报告。但 skill 里：

- `safety-checklist.md` 阶段 8 讲的是「验证到寄存器级 / 引脚配置匹配」这种**单次硬件操作验证**，不是 POST 序列设计。
- `grilling.md` 问题 6 沾了 NVS 配置损坏的边，但没有成体系的 POST。
- 没有文档说明：哪些器件 init 时**必须**自检（如电机驱动必须检过流回路、超声波必须检 echo 通路）、自检失败如何隔离、自检报告如何向 App 暴露、自检在 `device_tree_init_all()` 三阶段（architecture.md:116-119）的哪一阶段插入。

### 影响

中偏高。对「**AI 生成固件**」尤其致命——AI 不会主动加自检，除非 skill 明确要求。漏掉 POST 的固件，器件半失效时仍被 App 当正常器件用，是功能安全漏洞。

### 建议修复

在 `architecture.md` 三阶段初始化后，或新增 `lifecycle.md` 一节「POST 自检序列」：

1. 定义哪些器件类属 POST 必检（执行类：电机 / 舵机有反馈回路；感知类：超声波 / IMU 有回波 / WHO_AM_I）。
2. POST 失败 → 置 `health = DAL_HEALTH_FAULTED`（接问题 5），不中断整体启动。
3. `device_tree_init_all()` 的 `PHASE_DAL_INIT` 内，每个 init 成功后调用 `dal_xxx_self_test()`。
4. 向 App 暴露 `device_tree_get_post_report()`。

---

## 🟡 问题 7：Wasm `Asyncify` 栈税没有门禁

### 位置
- `references/shared/realtime-hardware.md:107`（点名 `ASYNCIFY_STACK_SIZE` 风险）
- `references/shared/tooling.md:100-105`（栈与资源门禁）

### 现状证据

realtime-hardware.md 明确列出已知风险：
> `Asyncify 栈税（ASYNCIFY_STACK_SIZE 深嵌套可能不够）`

但 tooling.md 的栈门禁**只覆盖真机任务栈**：
- `-fstack-usage` 生成 `.su` 文件、CI 校验最大栈帧 < 阈值；
- FreeRTOS `uxTaskGetStackHighWaterMark`。

**没有任何门禁覆盖 Wasm Asyncify 栈预算**。对一个反复声称「Wasm 仿真是命门」的项目，这是具体的可补 CI 闸。

### 影响

中。Asyncify 栈溢出表现为「深调用链时仿真静默挂起 / 行为异常」，极难定位——因为它不像真机栈溢出那样有明确 reset reason。

### 建议修复

在 tooling.md 栈门禁一节补 Wasm 专项：

1. **栈预算声明**：在 wasm target 构建脚本里把 `ASYNCIFY_STACK_SIZE` 显式声明为项目常量，文档化其与最深调用链的关系。
2. **编译期断言**：对已知深嵌套路径（如 `App → DAL → pal_delay_ms → Asyncify 栈帧`）用 `static_assert` 或构建期脚本校验调用链深度。
3. **CI 跑 wasm target 时**，对最深的同步阻塞路径做栈用量回归。

---

## 🟡 问题 8：`js_sim_*` 直接按地址写 Wasm 线性内存，是沙箱边界隐患

### 位置
- `references/static-dispatch/simulation.md:80-91`（TS 桩 `writeWasmFloat32(distancePtr, ...)`）

### 现状证据

simulation.md 的错误注入 TS 桩：

```typescript
function js_sim_get_ultrasonic_distance(trigPin: number, distancePtr: number): number {
    ...
    writeWasmFloat32(distancePtr, simDistance); // 将距离写入 Wasm 内存
    return 0;
}
```

`writeWasmFloat32(distancePtr, ...)` 是**按裸地址直接写 Wasm 线性内存**——这绕过了 Emscripten 的类型边界。一旦 Wasm 线性内存布局变化（增删全局变量、改导出顺序），`distancePtr` 计算若不同步更新就会**野写到相邻内存**，且无任何运行时检查。

这其实属于 `docs/design/07-platform-governance/03-security-sandbox.md` 的安全沙箱范畴，但 skill 完全没把它连上。

### 影响

中。当前 demo 阶段不会暴露，但当 `js_sim_*` import 数量增多、多线程/异步路径加入后，裸地址写内存是典型越界写入源。

### 建议修复

在 `simulation.md` §3 给 `js_sim_*` import 补一条契约：

1. **优先用 Emscripten 提供的类型化 API**：`ccall` / `cwrap` 返回值，或 `Module.setValue(ptr, value, 'float')` / `Module.HEAPF32.set(...)`——它们至少走 Emscripten 的 heap 抽象，而非裸指针算术。
2. **禁止** `js_sim_*` 侧自行用 `Module.HEAP8.buffer` + 偏移做地址运算。
3. 在 simulation.md 顶部标注：`js_sim_*` 边界属于安全沙箱范畴，完整约束见 `07-platform-governance/03-security-sandbox.md`。

---

## 🟡 问题 9：chigo-micro 引用已过时（路径不再存在于本仓库）

### 位置
- `references/static-dispatch/architecture.md:24, 53-54`（`chigo-micro/...` 路径）
- `references/shared/concurrency.md:122-125`
- `references/shared/realtime-hardware.md:105`
- `references/static-dispatch/README.md:3, 71`

### 现状证据

`MEMORY.md` 记录（且 system-reminder 已注入）：

> chigo-micro 已迁出，现位于 `D:\workspaces\ai-coding\chigo\chigo-micro`，**不再是 wink-ai-embedded 子目录**。

但 skill 正文仍把 chigo-micro 当**本仓库子目录路径**引用，如：
- `architecture.md:24`：`chigo-micro 对照：comms/ → control/ → driver/ → sensor/ → platform/`
- `architecture.md:53`：`/* chigo-micro driver */`
- `concurrency.md:122`：`chigo-micro（FreeRTOS...）` + `message_parser.h`
- `README.md:71`：`chigo-micro/project/embedded/`

AI 抄这些相对路径在本仓库里**找不到文件**。

### 影响

中。AI 会尝试 `Read` / `Grep` 这些不存在的路径，浪费轮次；或误以为本仓库有 chigo-micro 而去改错地方。

### 建议修复

1. 在 `index.md` 或 `README.md` 顶部加一句声明：
   `chigo-micro 为外部对照仓库（绝对路径 D:\workspaces\ai-coding\chigo\chigo-micro，见 MEMORY），非本仓库子目录；本文 chigo-micro 示例仅作对照/溯源。`
2. 正文凡引用 chigo-micro 路径处，统一标注「（外部对照仓库，非本仓库路径）」。

---

# 第三部分：结构性 / 可维护性技术债

---

## 🔴 问题 10：`shared/` 在两个 skill 间整份重复拷贝 —— 最大结构债

### 位置
- `embedded-best-practice/references/shared/`（8 个文件）
- `c-runtime-polymorphism-reading/references/shared/`（8 个文件，Glob 确认为独立拷贝）

### 现状证据

Glob 结果确认，两个 skill 各有一份 `shared/`：

```
embedded-best-practice/references/shared/clean-code.md
embedded-best-practice/references/shared/concurrency.md
...
c-runtime-polymorphism-reading/references/shared/clean-code.md
c-runtime-polymorphism-reading/references/shared/concurrency.md
...
```

这是**两份独立拷贝**，不是符号链接、不是共享引用。

### 影响

🔴 高（结构债）。这**直接违反 skill 自己反复宣讲的 SSOT（单一事实来源）原则**——`clean-code.md`、`memory-safety.md`、`concurrency.md`、`error-codes.md` 这些「范式无关工程纪律」本应是两 skill 共享的同一份。现状下，改一条并发规则要同步两个文件，必然漂移；一旦漂移，两个 skill 会对同一条规则给出冲突指引。

讽刺的是，skill 文档里 `pitfalls.md` 陷阱 3（`js_sim_*` 签名三处冲突）正是批判「SSOT 未强制」——而 `shared/` 的重复本身就是同一类问题。

### 建议修复

任选其一：

**方案 A（推荐）：提升 `shared/` 到公共位置**
- 移到 `.claude/skills/_embedded-shared/references/`（下划线前缀表示非用户直接调用的 skill）。
- 两个 SKILL.md 用相对路径 `../_embedded-shared/references/shared/xxx.md` 指过去。
- `static-dispatch/` 与 `runtime-polymorphism/` 仍各自保留（这是两范式真正分野的部分）。

**方案 B：合并成一个被两者依赖的 `embedded-c-shared` skill**
- 通过 SKILL.md 的 `description` 让两个主 skill 在需要工程纪律时引导到 shared skill。
- 代价：Claude Code skill 间不能硬依赖，只能靠路由描述，引导精度不如方案 A。

---

## 🟡 问题 11：SKILL.md 偏长（225 行），信息冗余

### 位置
- `SKILL.md`（全文）

### 现状证据

Claude Code skill 的最佳实践是 **progressive disclosure**：SKILL.md 当「精简路由器」，细节进 references，按需加载。当前 SKILL.md 内嵌了：

- 硬性规则速查表（`SKILL.md:100-117`）——与 `clean-code.md:10-18` 的硬限表**重复**；
- AI 生成禁令（`SKILL.md:140-149`）——与 tooling.md CI 正则、各 pitfalls **重复**；
- 好坏例子（`SKILL.md:175-204`）——与 `pitfalls.md`、`error-codes.md` **重复**；
- SOLID / Clean Code 速查（`SKILL.md:208-212`）——与 `clean-code.md:42-48` **重复**。

### 影响

中。冗余不只是篇幅问题——**两份内容会漂移**。例如硬限表若在某处更新（如函数长度 80→100 行），SKILL.md 和 clean-code.md 不同步就会矛盾。而且长 SKILL.md 每次会话全量加载，浪费 token。

### 建议修复

SKILL.md 只保留四块「路由级」内容：
1. 第 0 步边界判定（写本项目 vs 读外部 OOP）；
2. 任务 → 文档路由表（已有，保留）；
3. 编辑后安全审查协议（已有，保留）；
4. 核心原则 + 术语表（精简）。

下沉内容：
- 硬规则表 → `clean-code.md`（已在）；
- AI 禁令 → 新增 `shared/ai-guards.md` 或并入 tooling.md；
- 好坏例子 → 已在各 pitfalls，删 SKILL.md 重复版。

---

## 🟡 问题 12：缺一个「AI 改完代码的 30 秒极简自检」

### 位置
- `SKILL.md:65-86`（编辑后安全审查协议，引用 12 阶段清单）

### 现状证据

`12 阶段 safety-checklist`（`safety-checklist.md`）很全但很重，适合高风险改动。AI 改完**每一处**代码最缺的是一个 3-5 条致命项的**秒级** checklist，用于低/中风险改动的快速自检，而不是每次都跑完整 12 阶段。

当前 SKILL.md 的风险分级表（低/中/高）虽然区分了阶段范围，但即便是「低风险」也要跑阶段 1/10/12，仍偏重；且没有一句「最常翻车的 5 个点」速查。

### 影响

中。AI 在低风险改动上要么过度审查（浪费），要么干脆跳过（漏检）。一个秒级 checklist 能覆盖 80% 的常见翻车点。

### 建议修复

在 SKILL.md 末尾（或核心原则后）加一个极简自检，与 12 阶段形成「轻 / 重」两档：

```text
改完代码 30 秒自检（致命 5 项）：
□ 返回 wink_status_t 且用 if(status<0) 检查？（非 bool / 非 float 哨兵）
□ 业务层没直接碰 PAL / 寄存器？（走 DAL 命名 API）
□ 没扩大 #ifdef SIMULATION 范围？（只旁路最低物理信号层）
□ 没发明未在契约 / Registry 注册的 API？
□ 没在实时路径 malloc / 没吞错误码？

任一项不过 → 转完整 12 阶段清单。
```

---

# 优先级汇总

| 优先级 | 问题 | 性质 | 改动量 |
|--------|------|------|--------|
| 🔴 **P0** | #1 `state.*` 示例与实际结构脱节 | 会致 AI 编译失败 | 小（加标注 + 迁移示例） |
| 🔴 **P0** | #2 `WINK_WARN_*` warning 段未定义 | 错误码体系裂缝 | 小（定义段位 + 统一用词） |
| 🟠 **P1** | #3 `warn_unused_result` 便携宏 | 工程化缺口 | 小（加宏定义） |
| 🟠 **P1** | #4 CI 正则失配 + 空白名单 | 门禁形同虚设 | 小（改正则 + 行级豁免） |
| 🟠 **P1** | #9 chigo-micro 过时引用 | AI 找不到路径 | 小（加声明 + 标注） |
| 🔴 **P1** | #10 `shared/` 两 skill 重复拷贝 | 最大结构债 / SSOT 违反 | 中（提公共位置 + 改引用） |
| 🟡 **P2** | #5 器件 health 统一模型 | 体系增强 | 中 |
| 🟡 **P2** | #6 POST 自检序列 | 功能安全增强 | 中 |
| 🟡 **P2** | #7 Asyncify 栈门禁 | Wasm 稳定性 | 小-中 |
| 🟡 **P2** | #8 `js_sim_*` 沙箱契约 | 安全边界 | 小 |
| 🟡 **P2** | #11 SKILL.md 瘦身 | 可维护性 / token | 中 |
| 🟡 **P2** | #12 极简自检 checklist | AI 引导精度 | 小 |

---

# 建议执行顺序

1. **先做 P0 两条**（#1、#2）：改动小、收益大、风险低，且都是「文档与代码 / 体系自洽」问题，不涉及架构取舍，可直接动手。
2. **再做 P1 结构债**（#10 优先）：`shared/` 去重是后续一切维护的基础，越早做迁移成本越低。
3. **P1 其余**（#3、#4、#9）可与 P0 同批，都是局部修正。
4. **P2 按需**：#5/#6（health + POST）建议成对做（health 模型是 POST 的前提）；#11/#12（SKILL.md 瘦身 + 极简自检）建议成对做。

---

> 本评审基于 2026-06-23 的文档与代码快照。所有「位置」标注的行号以当时文件为准。
