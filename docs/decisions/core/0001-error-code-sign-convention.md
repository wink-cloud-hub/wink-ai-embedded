# ADR-0001：wink_status_t 错误码符号约定

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-06-22（提议）/ 2026-06-23（拍板） |
| 触发 | 架构评审 P0 项（见 [2026-06-22 评审报告](../../reviews/core/2026-06-22-architecture-review.md) §2.2） |
| 影响范围 | DAL / PAL / BAL / 静态检查规则 / 全部既有错误处理代码 |
| 决策者 | 项目负责人（2026-06-23 拍板采纳方案 C） |

---

## 背景（Context）

当前 `07-platform-governance/02-error-fault-model.md:25-39` 定义的 `wink_status_t` 采用**正数错误码**：

```c
typedef enum {
    WINK_OK = 0,
    WINK_ERR_INVALID_ARG = 1,
    ...
    WINK_ERR_NOT_INITIALIZED = 11,
    WINK_ERR_INTERNAL = 255        // 12..254 全部空洞
} wink_status_t;
```

这与三处权威基线**全部相反**：

1. 同工作区 `chigo-micro/CLAUDE.md` 编码约定："错误处理：返回 `int`（**0=成功，负数=错误**），禁止忽略返回值"
2. vendor best-practice `02-coding-standards.md`
3. Linux 内核（`-EINVAL=-22`）、POSIX（`-1` + errno）惯例

### 带来的三个实质问题

1. **反向判断 bug 高发**：C 语言约定俗成 `if (status)` 判错。正数方案下 `WINK_OK=0` 为假、错误为真，`if (status) { /* 误把成功分支写这里 */ }` 是 AI 生成代码极易踩的坑。当前文档第 46 行注释"WINK_OK 必须为 0，便于条件判断"反而掩盖了风险——它假设了 `if (status != WINK_OK)` 写法，但实际（尤其 AI 生成）常写成 `if (status)`。
2. **errno 翻译层成为 bug 源**：PAL 底层（HAL/POSIX/ESP-IDF）返回负 errno，DAL 要把负 errno 翻译成正数 `WINK_ERR_IO`，多一层无意义符号翻转，翻译表本身是 bug 源。
3. **错误码体系不完备**：`INTERNAL=255` 导致 12–254 全空洞；**缺失功能安全必备码**——过流、过温、watchdog 超时、数值溢出、assert/panic。而同工作区 `CLAUDE.md` 的 Safety task 明确列了过流/过温/通信超时，这些故障无码位承载。

> 注：错误模型的**意图**完全正确（用枚举状态码替代 `float -1.0f`/`NULL`/`bool` 隐式错误、禁止忽略返回值）。本 ADR 只针对**符号约定**与**体系完备性**两个落地偏差。

---

## 方案比选（Options）

### 方案 A：维持正数错误码（现状）
- **优点**：零迁移成本。
- **缺点**：上述三个问题全部保留；与同工作区既有约定长期分裂；AI 生成代码的反向 bug 风险持续存在。
- **判定**：不推荐。返工成本会随代码量增长而指数上升，越晚改越痛。

### 方案 B：改为负数错误码，与 errno 对齐
```c
typedef enum {
    WINK_OK               = 0,
    WINK_ERR_INVALID_ARG  = -22,   // 对齐 EINVAL
    WINK_ERR_TIMEOUT      = -110,  // 对齐 ETIMEDOUT
    WINK_ERR_IO           = -5,    // 对齐 EIO
    ...
} wink_status_t;
```
- **优点**：`if (status)` 语义正确（0=成功=假，负数=错误=真）；PAL errno 可直接透传无需翻译；与 Linux/POSIX/同工作区约定全一致。
- **缺点**：迁移成本——需重构全部既有错误处理 + 更新静态检查规则；errno 数值对新手不直观（但可用宏名屏蔽）。
- **判定**：推荐方向，但 errno 数值对齐可酌情简化（见方案 C）。

### 方案 C（推荐）：负数错误码 + 语义命名 + 补功能安全码
保留语义化的宏名（不要求记住 errno 数值），但**符号改为负数**，并**补齐功能安全码**：

```c
typedef enum {
    WINK_OK = 0,

    /* 通用可恢复错误（负数，对齐 Linux 惯例） */
    WINK_ERR_INVALID_ARG       = -1,
    WINK_ERR_TIMEOUT           = -2,
    WINK_ERR_DISCONNECTED      = -3,
    WINK_ERR_OUT_OF_RANGE      = -4,
    WINK_ERR_IO                = -5,
    WINK_ERR_BUSY              = -6,
    WINK_ERR_UNSUPPORTED       = -7,
    WINK_ERR_CHECKSUM          = -8,
    WINK_ERR_PERMISSION        = -9,
    WINK_ERR_RESOURCE_EXHAUSTED = -10,
    WINK_ERR_NOT_INITIALIZED   = -11,

    /* 功能安全相关（新增，区分致命/可恢复） */
    WINK_ERR_OVERCURRENT       = -20,   // 过流（可恢复：限流重试）
    WINK_ERR_OVERTEMPERATURE   = -21,   // 过温（可恢复：降频）
    WINK_ERR_WATCHDOG          = -30,   // 看门狗超时（致命：复位）
    WINK_ERR_OVERFLOW          = -40,   // 数值溢出/计算 UB（致命）
    WINK_ERR_PANIC             = -99,   // 不可恢复，需 halt
} wink_status_t;
```
- **优点**：`if (status)` 正确；语义命名可读；功能安全码补齐；码段分区（-1~-11 通用、-20s 安全可恢复、-30s/-40s 致命）便于分类处理。
- **缺点**：与方案 B 相同的迁移成本。
- **判定**：**推荐**。

---

## 推荐决策（Recommendation）

**采纳方案 C**。理由：
1. 符号对齐项目既有约定与 Linux/POSIX 惯例，消除反向 bug 风险。
2. PAL errno 可直接透传，去掉翻译层。
3. 功能安全码补齐，承载过流/过温/watchdog 等真机必备故障。
4. 码段分区支持"致命/可恢复"分类，为 fail-safe 策略提供基础。

## 后果（Consequences）

- **迁移工作量**：重构 DAL/PAL/BAL 全部 `return WINK_ERR_*` 与 `if (status)` 判断；更新 App Safe Codegen 静态检查规则（status 必检逻辑不变，但 lint 规则需适配负数）。
- **AI 生成代码**：AI prompt/模板需更新错误处理范例；好处是 `if (status)` 现在语义正确，AI 生成质量提升。
- **迁移期兼容**：建议在 wink_status.h 顶部保留注释说明历史变更，避免旧代码误用。
- **测试**：需补充错误码边界测试（特别是新增的功能安全码）。

## 遵循与后续（Compliance）

- 拍板后：更新 `07-platform-governance/02-error-fault-model.md` 第 2 节枚举定义（替换正数为方案 C）。
- 拍板后：移除 `07-02` 的决策标注（见本次回写）。
- 联动：fail-safe 姿态表（`07-02 §7`）应按"致命/可恢复"码段分类执行器动作。
- 联动：Golden Trace 的错误事件记录需覆盖新增码位。

---

*本 ADR 状态变更请在此记录：*
- 2026-06-22：Proposed（评审触发）
- 2026-06-23：Accepted——项目负责人拍板采纳方案 C（负数错误码 + 补功能安全码）；结论已回写 `07-platform-governance/02-error-fault-model.md` §2（枚举）、§7（fail-safe 按码段分类）。

