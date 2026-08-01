# DAL API 一致性规范 — 评审建议与补充

> 📦 **已归档 (ARCHIVED — 2026-08-01)**：本文 R-01 ~ R-12 全部已整合进 [`dal-api-consistency-spec.md`](./dal-api-consistency-spec.md) v2.1.0，后续以规范正文为准。本文件仅作评审记录留存，不再更新。逐条处置见文末 [§7 裁决回执](#7-裁决回执归档)。

| 项 | 内容 |
|----|------|
| **文档性质** | 评审意见（非规范正文，已归档） |
| **评审对象** | [`dal-api-consistency-spec.md`](./dal-api-consistency-spec.md) v2.0.0 (Draft) |
| **评审日期** | 2026-08-01 |
| **评审人视角** | 嵌入式架构师 / 规范可执行性审阅 |
| **状态** | **已归档** — R-01~R-12 已于 2026-08-01 全部整合进规范 v2.1.0 |

---

## 0. 结论

规范整体质量高（A−），动词模型、ISR/并发契约、stub 与裁剪纪律、合规矩阵与迁移策略都已立住，且具备进 CI 的形态。本文档不重复已合格的部分，只记录**契约缺口、可执行性风险与打磨建议**，供规范作者逐条裁决。

裁决结果建议三选一：

1. **采纳并回写规范**（标注对应规则 ID）；
2. **立项 ADR** 后再纳入（涉及架构取舍的项）；
3. **拒绝并记录理由**。

> 已在 2026-08-01 直接修复的三处硬伤（头部 ADR 坏链、`WINK_ISR_SAFE` 未定义、host 64 位 `sizeof` 断言）见规范正文，不在此重复。

---

## 1. 优先级总览

| 编号 | 议题 | 严重度 | 建议处置 |
|------|------|--------|---------|
| R-01 | 缺少 task-to-task 并发的默认契约 | 高 | 回写规范（MUST） |
| R-02 | lint 覆盖率/实施状态不透明 | 高 | 回写合规矩阵 |
| R-03 | `poll` 等返回值强制检查产生噪音 | 中 | 回写规范（豁免/改 void） |
| R-04 | `read` 与 `read_blocking` 命名双轨无裁决 | 中 | 回写规范（选择规则） |
| R-05 | 错误返回时出参状态未定义 | 中 | 回写规范（MUST） |
| R-06 | `owner` 资源仲裁语义悬空 | 中 | 补跨引用或立项 ADR |
| R-07 | 缺函数级 deprecation/退役策略 | 中 | 回写规范 |
| R-08 | DAL-L-011 deinit 措辞自相矛盾 | 低 | 文字打磨 |
| R-09 | `safe_off` "别名"术语不精确 | 低 | 文字打磨 |
| R-10 | Init 零能量与 Init-to-Ready 边界 | 低 | 补半句澄清 |
| R-11 | 单位后缀对 IMU/ADC 类器件预留不足 | 低 | 扩展后缀表 |
| R-12 | 规范变更治理力度偏弱 | 低 | 附录 C 提级 |

---

## 2. 高优先级契约缺口

### R-01. 缺少 task-to-task 并发的默认契约

**现状**：§6 对 ISR / 跨核共享、volatile、多字段快照讲得很细，但没有规定**同一个 `dal_xxx_t *dev` 实例能否被两个 task 并发调用**。Contract 模板有 `Thread-safe: ★ No/Yes`，却没有默认值。

**风险**：这是嵌入式 HAL 最经典的扯皮来源。驱动作者按"实例非线程安全，调用方串行化"实现，调用方却按"线程安全"使用；或者反过来，驱动里到处加锁导致优先级反转。没有默认值，Contract 字段就会被随意填写且无人对账。

**建议**（提为 MUST）：

> DAL 实例默认**非线程安全**：同一 `dal_xxx_t *dev` 的方法调用、以及 init/deinit 与其他方法之间，MUST 由调用方在外部串行化（同一 mutex / 同一 task / 消息队列）。仅当驱动在头注释显式声明 `Thread-safe: Yes` 并说明锁/无锁机制时，才允许并发调用。`Thread-safe` 字段缺失时默认按 `No` 解释，并由 lint 对公开 API 缺该字段报 warning。

这条应同时补进 §6（新增小节）和 §15 Contract 模板，并与 R-02 的 lint 覆盖关联。

### R-02. lint 规则覆盖率与实施状态不透明

**现状**：全文约 80 条 MUST，但 §17.3 只给了规则 ID → lint 名的命名约定（`dal.<snake_case_rule>`），没有说明哪些规则真正在 `wink-tools/lint/rules/*.yaml` 里实现了。CLAUDE.md 要求改 C 代码后跑 `wink lint --pack layering --pack api`，但本规范没说 DAL 规则落在哪个 pack。

**风险**：规范与实现之间的 gap 不透明，规范会逐渐"愿望化"——条目越来越多，但 CI 实际只拦得住其中一小部分，评审时又无法快速区分"已长牙"和"待实现"。

**建议**：

1. 在 §17 合规矩阵或新增小节增加一张**规则实施表**，每条 MUST/SHOULD 标注：
   - `Enforced by: api-lint / layering-lint / review / pending`；
   - 所属 pack（`api` / `layering` / 未来的 `dal` pack）。
2. 对 `pending` 规则登记为 issue 并在表中给出跟踪号，避免"永远 pending"。
3. 对纯语义、无法静态检测的规则（如"越界饱和还是报错"），明确标 `review`，不要假装 lint 能覆盖。

这是让规范真正可执行的关键一步，优先级与 R-01 并列。

---

## 3. 中优先级问题

### R-03. `poll`（及 `toggle`）的返回值强制检查

**现状**：DAL-F-004 要求除 `safe_off` 外所有返回 `wink_status_t` 的公开 API 都标 `WINK_WARN_UNUSED_RESULT`。

**问题**：`poll` 是每 tick 调用的状态机推进函数，绝大多数调用点有意忽略其返回值（"推进一下，失败了下次再推"），强制检查会在每个事件循环里制造告警噪音，最终诱导调用方加 `(void)` 或干脆关闭该警告。`toggle` 等纯翻转函数同理。

**建议二选一**：

- 方案 A（推荐）：把 `poll` 列入 DAL-F-004 的豁免白名单，并规定 `poll` 的错误通过 `get_status` 查询（异步状态机本就有 status 通道）；
- 方案 B：将 `poll` 的返回类型改为 `void`（推进本身不失败，失败落在状态机 ERROR 态）。

若选方案 B，需同步检查现存 `dal_button_poll` / `dal_eeprom_poll` / `dal_gps_poll` 的返回值使用情况。

### R-04. `read` 与 `read_blocking` 命名双轨缺裁决规则

**现状**：DAL-B-001 允许"后缀 `_blocking` **或** 名字本身暗示阻塞（如 `read`）"。结果是 `dal_ultrasonic_read`（裸 `read`）与 `dal_eeprom_read_blocking`（带后缀）并存。

**问题**：没有裁决规则，新增驱动时 codegen 和人都无法判断该用哪个，命名会继续碎片化——这正是 §1 背景里点名要解决的问题。

**建议**（补一条选择规则）：

> 当同一器件**同时**提供阻塞与非阻塞（request/poll）两种读取形态时，阻塞变体 MUST 使用 `_blocking` 后缀以与非阻塞 API 明确区分；当该操作**仅存在阻塞形态**时，允许使用裸 `read`（名字本身即契约）。

按此规则：ultrasonic 既有 `read`（阻塞）又有 `request_measurement`（非阻塞），严格说应改名为 `read_blocking`——是否值得做破坏性改名需评估存量，可借迁移期（§17.2）一起裁决。

### R-05. 错误返回时出参状态未定义

**现状**：§4 规定了出参命名和 `const` 约定，但没有规定函数返回非 `WINK_OK` 时 `*out_val` 的状态（保持原值 / 清零 / 未定义）。

**风险**：调用方在错误路径读到脏 out 值是高频 bug，尤其在"先声明变量、调用失败后继续用旧值"的循环里。

**建议**（提为 MUST，加入 §4.2 或 §14）：

> 公开 API 返回非 `WINK_OK` 时，所有 `out_*` 出参 MUST 保持调用前的值不变（MUST NOT 清零或写入半成品数据）。调用方 MUST NOT 在错误返回路径读取 out 参数。

实现上很容易达成：出参只在函数末尾、确定返回 `WINK_OK` 前一次性写入。

### R-06. `owner` 的资源仲裁语义悬空

**现状**：DAL-S-001/002 强制 `owner` 为首字段且指向静态存储期字符串，用途写的是"静态资源冲突检测"，但全文没有说明：冲突由谁检测、在 init 还是 device_tree/codegen 阶段、冲突返回什么错误码。

**风险**：`owner` 目前更像配置约定，真正的 claim/仲裁逻辑在下层（resource claim 机制）。规范承诺了"冲突检测"却没指向实现，读者无法验证它是否真的发生。

**建议**：二选一——

1. 补一条规则 + 错误码：`init` 在 claim 失败时返回 `WINK_ERR_BUSY` 或专门的 `WINK_ERR_RESOURCE_CONFLICT`，并 cross-ref 到资源仲裁模块的活规范/ADR；
2. 若冲突检测完全在 codegen/device_tree 期完成（运行期不仲裁），则把 DAL-S-001 的用途描述改为"供 device_tree/codegen 静态资源冲突检测与日志归因"，不要让读者误以为运行期有仲裁。

建议同时核对 ADR-0046（驱动 Registry SSOT）是否已覆盖 claim 语义，避免重复立约。

### R-07. 缺函数级 deprecation / 退役策略

**现状**：§13 讲了结构体布局兼容性（追加字段、只追加不重排），但没有**函数级**废弃与下线规则。项目已有先例：button BAL API rename 采用了"deprecated 两个 minor 版本"的窗口约定（见 memory `adr-0032-abc-naming`）。

**建议**（补入 §13）：

> 公开 DAL 函数废弃 MUST：
> 1. 用 `WINK_DEPRECATED_MSG("use dal_xxx_new() instead; will be removed in v<N+2>")` 标注旧函数；
> 2. 在同版本提供替代 API，并在头注释 `@deprecated` 指向新 API；
> 3. 至少保留两个 minor 版本窗口后才可删除；
> 4. 删除动作本身 MUST 走 ADR 并在 changelog 列出。

需要确认 `wink_status.h`/compiler 头是否已有 `WINK_DEPRECATED_MSG`，若无则与 `WINK_ISR_SAFE` 一起在配套 ADR 里落地。

---

## 4. 低优先级打磨

### R-08. DAL-L-011 deinit 顺序措辞自相矛盾

原文："释放硬件资源 → `memset` 清零 → 置 `initialized=false`"。

`memset` 整个句柄后 `initialized` 已经是 0，第三步多余；真正的 hazard 是 memset 时 ISR 仍可能引用该结构——这一点已被 DAL-L-012 覆盖。

**建议**：DAL-L-011 改为"禁用中断/ISR → 等待 in-flight 回调结束（见 DAL-L-012）→ 释放硬件资源 → 清零句柄（含 `initialized=false`）"，把竞态细节统一 cross-ref 到 L-012，避免两处措辞不一致。

### R-09. `safe_off` "别名"术语不精确

§8.1 把 led 的 safe_off 描述为"`dal_led_off`（别名）"。实际 `dal_led.h` 里**没有** `dal_led_safe_off` 符号，是 YAML `safe_off_fn: dal_led_off` 的映射。

**建议**：区分两个概念——
- **具名 `safe_off` API**：驱动暴露 `dal_xxx_safe_off`（如 dc_motor）；
- **YAML 绑定的关断函数**：驱动无独立 safe_off 符号，由 `config.safe_off_fn` 指向某个已有原语（如 led → `dal_led_off`）。

两者都是 DAL-E-001 意义上的"绑定到具体关断原语"，但代码形态不同，规范应在 §3.2 / §8.1 用统一术语描述，避免读者去头文件里找不存在的 `dal_led_safe_off`。

### R-10. Init 零能量 vs Init-to-Ready 的边界

DAL-BC-001 已用例外条款处理了"执行器 init 后零能量"，但没明说零能量**不代表需要额外 arm/enable**。

**建议**补半句：

> 执行器 init 成功后即**立即接受控制指令**（Init-to-Ready），零能量只是默认输出值而非额外的使能闸门。MUST NOT 出于安全考虑再引入 `enable()`/`arm()` 前置调用——否则破坏 Init-to-Ready。安全关断由 `safe_off` 承担，而非 init 后的待使能态。

### R-11. 单位后缀对 IMU/ADC 类器件预留不足

§9.1 后缀表覆盖了距离/时间/频率/角度/速度，但即将到来的 IMU、ADC 类器件会用到：

- 角速度 `_deg_per_s`（或 `_dps`）、加速度 `_mps2`（m/s²）；
- 电压 `_mv`、原始 ADC 计数 `_raw`；
- 温度 `_c`（摄氏度）。

另外 `_norm` 无法区分 `[0,1]` 与 `[-1,1]`（dc_motor speed 是 `[-1,1]`）。

**建议**：补充上述后缀；并明确"`_norm` 后缀不编码正负号区间，正负有符号归一化以 Range 注释为权威"，或引入 `_snorm`（signed normalized, [-1,1]）与 `_unorm`（[0,1]）区分。后者更利于 codegen 做值域校验，但属于新增约定，建议小 ADR 裁决。

### R-12. 规范变更治理力度偏弱

附录 C 仅写"本规范的变更条款被 ADR Accepted 后，SHOULD 回写到活规范文档"。本规范大量条款是 codegen 的规范性输入，改动它本质是架构变更，SHOULD 偏弱。

**建议**改为：

> 本规范的 MUST 条款新增、删除或语义变更 MUST 走 ADR；SHOULD/MAY 条款变更可由维护者评审合入。所有 Accepted 变更 MUST 同步回写 `01-dal-device-abstraction.md` 活规范，并在本文件变更历史记录。

---

## 5. 已修复项（2026-08-01，留档）

以下三处硬伤已直接改入规范正文，此处仅留档，无需再裁决：

1. **头部 ADR 坏链**：`0001` / `0017` / `0024` / `0043` 链接文件名更正为仓库实际名称（`error-code-sign-convention` / `blocking-api-hard-isolation` / `fault-three-phase-model-and-dal-deinit-contract` / `yaml-driven-layer-lint`）。
2. **`WINK_ISR_SAFE` 未定义**：DAL-C-021 改为"计划标注、当前尚未在 `wink_status.h` 定义、待配套 ADR 落地"，在此之前以 Contract 注释声明为准。
3. **host 64 位 `sizeof` 断言**：§2.3 改为首选 `offsetof` 断言（位宽无关）；含指针结构体的整体 `sizeof` 断言按 `INTPTR_MAX` 分 ILP32/64 两档，DAL-BC-010 同步更新。

---

## 6. 建议的裁决顺序

1. **先做 R-01 + R-02**：这两条决定规范能否真正在 CI 长牙，且互相依赖（默认契约需要 lint 兜底）。
2. **再做 R-03 ~ R-07**：API 形态与错误契约，影响新增驱动与 codegen，应在 v2.1 迁移期开始前定稿，否则存量迁移会带着歧义走。
3. **R-08 ~ R-12 随 v2.1 打磨**：纯文字与术语修正，可批量处理。
4. 涉及新增宏（`WINK_ISR_SAFE`、`WINK_DEPRECATED_MSG`）、`_snorm`/`_unorm`、`owner` 运行期仲裁的项，分别立项小 ADR，不要塞进规范直接拍板。

---

## 7. 裁决回执（归档）

2026-08-01 复核：R-01 ~ R-12 已全部整合进规范 v2.1.0，处置如下。

| 编号 | 议题 | 处置 | 落地位置（v2.1.0） |
|------|------|------|-------------------|
| R-01 | task-to-task 并发默认契约 | ✅ 采纳（MUST） | §6.0 DAL-C-040~043，§15 Contract 模板 Thread-safe 默认 No |
| R-02 | lint 覆盖率不透明 | ✅ 采纳 | §17.3 独立 `dal` pack；§17.3.1 规则实施状态表（lint/review/pending 三级） |
| R-03 | `poll` 等返回值强制检查 | ✅ 采纳（方案 A：豁免属性，保留返回类型） | §4.1 DAL-F-004 豁免白名单（safe_off/poll/deinit） |
| R-04 | `read` vs `read_blocking` 裁决 | ✅ 采纳 | §7.1 DAL-B-001a 命名选择规则 |
| R-05 | 错误返回时出参状态 | ✅ 采纳（MUST） | §4.3 DAL-F-020~022 |
| R-06 | `owner` 仲裁语义悬空 | ✅ 采纳 | DAL-S-001 明确仲裁下沉 PAL resource claim，返回 `WINK_ERR_BUSY`/`WINK_ERR_RESOURCE_EXHAUSTED` |
| R-07 | 函数级 deprecation 策略 | ✅ 采纳（MUST） | §13.2 DAL-BC-020~023，引用现存 `WINK_DEPRECATED_MSG`/button 先例 |
| R-08 | DAL-L-011 deinit 措辞 | ✅ 采纳 | §3.1 DAL-L-011 改为 cross-ref L-012，"清零句柄（含 initialized=false）" |
| R-09 | safe_off "别名"术语 | ✅ 采纳 | §8.1 区分"具名 safe_off API"与"YAML 绑定的关断函数"两种形态 |
| R-10 | Init 零能量 vs Init-to-Ready | ✅ 采纳 | §8.3 补充"立即接受控制指令，禁止 enable()/arm() 闸门" |
| R-11 | 单位后缀预留 | ✅ 采纳（取 Range 注释为权威方案） | §9.1 增 `_dps`/`_mps2`/`_mv`/`_raw`/`_c`；明确 `_norm` 不引入 `_snorm`/`_unorm` |
| R-12 | 变更治理提级 | ✅ 采纳 | 附录 C 变更治理表：MUST 变更走 ADR，架构契约 Accepted 后回写活规范 |

**第二轮复核（同日）追加修复**：ultrasonic_read 退役标注与代码对齐、基线版本迁移表对齐 v2.1.0、存量驱动数 10→9、`WINK_ERR_NO_FIX` 改为待定义示例、§6.0 规则 ID 与 volatile 段重号（000/001a-c → 040~043）、pending 规则补跟踪列。

至此本文所有建议关闭，归档留存。
