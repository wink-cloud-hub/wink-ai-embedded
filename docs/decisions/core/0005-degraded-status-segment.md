# ADR-0005：可恢复降级状态码段（-50s）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-06-23（提议并拍板） |
| 触发 | embedded-best-practice skill Round 2 评审 §问题 2（`WINK_WARN_CONFIG_CORRUPT` 引入了体系从未定义的 warning 段） |
| 影响范围 | 错误码体系（`07-platform-governance/02-error-fault-model.md` §2/§7）、`wink_status.h`（尚待创建）、DAL 初始化 / NVS 加载降级路径、BAL 降级判定、skill `error-codes.md` / `grilling.md` / `architecture.md` |
| 决策者 | 项目负责人（2026-06-23 拍板采纳方案 B） |

---

## 背景（Context）

[ADR-0001](./0001-error-code-sign-convention.md)（方案 C，Accepted）把错误码体系定为**绝对二分**：`0 = WINK_OK`、负数 = 错误，并按 `-1..-11`（通用可恢复）/ `-20s`（安全可恢复）/ `-30s·-40s`（致命）/ `-99`（panic）分区。**整个体系没有「成功带警告 / 降级运行」这一段。**

但运行期存在一类真实状态——「**失败但可安全降级、系统应继续运行**」：

1. NVS / 持久配置损坏 → 用编译期安全默认值继续，而非 halt（见 [grilling.md](../../.claude/skills/embedded-best-practice/references/static-dispatch/grilling.md) 问题 6）。
2. 某器件 init 失败 → 隔离该器件，系统其余部分继续（见 [architecture.md](../../.claude/skills/c-runtime-polymorphism-reading/references/runtime-polymorphism/architecture.md) 三阶段初始化降级策略）。

skill 文档里凭空出现了 `WINK_WARN_CONFIG_CORRUPT`（`WARN` 前缀、语义「损坏但仍降级运行」），隐含**正数**（因为按现有约定负数即错误，而它「不算错误、要继续」）；而 `WINK_ERR_FAILED_INIT` 又被按名引用却无码位。这导致两个被 skill 反复推荐的检查写法对这个值**语义都不对**：

| 检查写法 | 对隐含正数 `WARN` 的行为 | 问题 |
|---------|--------------------------|------|
| `if (status < 0)`（头号推荐） | 当成 `WINK_OK` 成功 | 配置损坏这类安全事件被静默吞掉 |
| `if (status != WINK_OK)` | 当成错误 | 无法走「带警告正常降级」路径，只能走错误恢复 |

同时 `WARN`/`ERR` 前缀在同 docset 里不一致，AI 无法从前缀推断检查方式。需要一个**自洽**的降级状态表达。

---

## 方案比选（Options）

### 方案 A：正式定义 `>0` = 成功带警告段（degraded success）
新增「`WINK_OK = 0`，`>0` = 成功但带警告」段，`WINK_WARN_CONFIG_CORRUPT = +1` 之类。

- **优点**：语义上「非错误」最直观。
- **缺点**：**破坏 ADR-0001 的绝对二分根假设**——`if (status < 0)` 会把 warning 当成功吞掉。必须同步更新全体系每一条 `< 0` vs `!= WINK_OK` 检查指引、CI 正则、lint 规则、迁移指南，工作量大且极易引入新歧义。AI 生成代码的「负数即错」心智模型被打破。
- **判定**：不推荐。代价远超收益。

### 方案 B（推荐）：新增**负数**降级段 `-50s`
保持「负数 = 非成功」不变，新增 `-50..-59`「**可恢复降级（degraded but operational）**」段：

```c
/* 可恢复降级：操作未完全成功，但系统已安全降级、应继续运行（非 halt） */
WINK_ERR_CONFIG_CORRUPT_DEGRADED = -50,   /* NVS/配置损坏 → 用安全默认值继续 */
WINK_ERR_FAILED_INIT             = -51,   /* 器件 init 失败 → 器件隔离，系统继续 */
```

- **优点**：
  - `if (status < 0)` 语义**绝对不变**，仍能捕获降级状态——安全事件不会被静默吞掉。
  - BAL 用 `status == WINK_ERR_CONFIG_CORRUPT_DEGRADED` / `== WINK_ERR_FAILED_INIT` 特判走保守降级分支，其余 `< 0` 走常规错误恢复——两套路径清晰共存。
  - 零迁移成本：现有检查写法、CI 正则、lint、迁移指南全部无需改动。
  - 统一 `ERR_*` 前缀，**废止 `WARN_*` 前缀**——前缀即符号，AI 可从前缀推断检查方式。
- **缺点**：「降级」与「错误」同为负数，需靠具体码值区分策略——但策略区分本就应由 BAL 按码值决定，码段分区（`-50s` 降级 vs `-1..-11` 通用 vs `-30s` 致命）已提供清晰分类。
- **判定**：**推荐**。

### 方案 C：不新增码，降级语义完全交给器件 health 字段承载
不在返回码体系里表达降级，只靠 `dal_health_t`（OK/DEGRADED/FAULTED）字段。

- **优点**：错误码体系零改动。
- **缺点**：丧失「**用返回码向上层即时发降级信号**」的能力——BAL 必须每次调用后额外查询 health 才知道是否降级，时序与一致性更复杂。配置损坏这类「调用本身降级完成」的事件最适合用返回码表达。
- **判定**：作为方案 B 的**补充**（health 字段承载持续状态，返回码承载瞬时信号），不单独采用。

---

## 推荐决策（Recommendation）

**采纳方案 B**（负数 `-50s` 降级段），并以方案 C 的 health 字段为补充。理由：

1. 保持 ADR-0001 绝对二分根假设不变，`if (status < 0)` 对降级状态依然正确捕获——消除「正数 warning 被 `< 0` 吞掉」的安全风险。
2. 零迁移成本，全体系检查指引 / CI / lint / 迁移指南无需改动。
3. 统一 `ERR_*` 前缀，废止 `WARN_*`，前缀与符号一致，AI 可推断。
4. 码段分区（`-50s` 降级）与既有 `-1..-11 / -20s / -30s / -99` 并列，fail-safe 分类清晰。

## 后果（Consequences）

- **错误码体系**：`-50..-59` 段定为「可恢复降级」。码值：`WINK_ERR_CONFIG_CORRUPT_DEGRADED = -50`、`WINK_ERR_FAILED_INIT = -51`；`-52..-59` 预留。
- **回写**：`07-platform-governance/02-error-fault-model.md` §2 枚举加 `-50s` 段、§7 fail-safe 表加 `-50s` 行（器件隔离/保守逻辑，系统继续）。
- **BAL 降级判定**：`status == -50/-51` → 保守降级（隔离器件 / 用默认值）；其余 `< 0` → 常规错误恢复；二者都不静默。
- **health 字段（补充）**：init 返回 `-50/-51` 时置 `dal_health_t = DEGRADED/FAULTED`，承载持续状态（见 skill lifecycle.md 器件健康状态机）。
- **命名**：废止 `WINK_WARN_*` 前缀；`grilling.md` / `architecture.md` 等存量 `WARN`/无名引用改为 `WINK_ERR_*_DEGRADED` / `WINK_ERR_FAILED_INIT`。
- **wink_status.h（待创建）**：落地该头文件时，`-50s` 段与 ADR-0001 方案 C 一并纳入。

## 遵循与后续（Compliance）

- 拍板后：回写 `07-platform-governance/02-error-fault-model.md` §2（枚举加段）、§7（fail-safe 表加行）、§2 顶部 note 补 ADR-0005 指向。
- 拍板后：skill `_embedded-shared/error-codes.md` 布局表加 `-50s` 段 +「无正数 warning 段、统一 `ERR_*`」声明。
- 拍板后：skill `grilling.md:56` `WINK_WARN_CONFIG_CORRUPT` → `WINK_ERR_CONFIG_CORRUPT_DEGRADED`；`architecture.md` `WINK_ERR_FAILED_INIT` 注明值 `-51`。
- 联动：器件 health 模型（lifecycle.md）的 DEGRADED/FAULTED 置位接本 ADR 的 `-50/-51`。

---

*本 ADR 状态变更请在此记录：*
- 2026-06-23：Proposed（embedded-best-practice skill Round 2 评审 §问题 2 触发）。
- 2026-06-23：Accepted——项目负责人拍板采纳方案 B（负数 `-50s` 降级段，保持 ADR-0001 绝对二分不变）；结论已回写 `07-platform-governance/02-error-fault-model.md` §2/§7 与 skill `error-codes.md` / `grilling.md` / `architecture.md`。
