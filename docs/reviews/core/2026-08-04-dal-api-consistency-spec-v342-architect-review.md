# DAL API 一致性规范 v3.4.2 外部架构师评审

| 项 | 内容 |
|----|------|
| **评审对象** | `wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md` v3.4.2 (Active) |
| **评审日期** | 2026-08-04 |
| **评审者** | 资深嵌入式架构师（外部视角） |
| **评审性质** | 架构级深度评审（与驱动级合规评审 [2026-08-01-dal-api-consistency-spec-review.md](2026-08-01-dal-api-consistency-spec-review.md) 互补） |
| **结论** | **质量第一梯队**，但存在 6 处架构级盲区与若干 Lint 可执行性缺陷；建议补强 P0 四项后晋升为 **v4.0 稳定基线** |
| **关联 ADR** | ADR-0001, ADR-0004, ADR-0017, ADR-0024, ADR-0043, ADR-0046, ADR-0048, **ADR-0056** |
| **建议动作** | (1) 新增 `DAL-EC-005` 错误码分段宏；(2) 合并 §1.4 + §17.3.1 为单表；(3) 增 `DAL-L-030/031` safe_off↔deinit 顺序；(4) 增 `DAL-B-026` 异步三段式 ERROR 态恢复；(5) 提 `DAL-U-030` 至 Lint 强管控 |

---

## 1. 总体评价

本规范是嵌入式驱动 API 领域**第一梯队**水准的作品。三大支柱架构清晰、决策论证扎实、工程姿态务实：

1. **Profile 分级 + 跨 Profile 同源 YAML**（§1.3）— 把"同源"承诺收敛到 codegen binding 层，而非强行让 32/8 位 C 签名完全相同。
2. **A/B 量纲两分类 + 全 Profile 定标整数**（§9.3~9.4, ADR-0056）— 在 8 位端消除软浮点、在 32 位端消除"伪精度"。
3. **Lint 分级标注 + AI 阅读策略**（§术语）— 用元规范管理规范本身，并指导 AI 助手按行跳过 100% Lint 化规则。

与 Zephyr / Linux 工业实践相比，本规范**在三个具体点上做得更透**：A 类量纲统一、A/B 分类治理、ABI 探针自动化。

但作为"体系稳定基线"仍存在**架构级盲区**与**Lint 可执行性缺口**。本评审聚焦于"体系本身"的健壮性，与现有合规评审互补。

---

## 2. 亮点（值得保留并对外宣传的工程实践）

### 2.1 A/B 量纲两分类（§9.3 + ADR-0056）

**核心论点**：A 类（执行器命令）硬件终态是离散寄存器整数（PWM CCR/ARR），在 32 位 Full Profile 用 `float 0.8f` 写寄存器前还要强转 `(uint32_t)(0.8f * max_duty)`，**纯属多余**。

**工业对比**：
- Zephyr 的 `<zephyr/dt-bindings/pwm/pwm.h>` 用周期 + 占空比纳秒数，**没有量纲分类概念**。
- Arduino 的 `analogWrite(0~255)` 隐藏刻度，跨硬件不可移植。
- 本规范的 `int16_t speed_promille` 全 Profile 同刻度、`uint32_t position_um` 长行程首选——**是真同源、真零软浮点**。

### 2.2 `safe_off` 未初始化返回 `WINK_OK` 的设计论证（§3.2）

罕见的"**用真实故障消费链路反推 API 语义**"案例——五步论证：
1. 主消费方是 `wink_actuator_safe_off_all`（系统级批量关断）
2. 未初始化在该路径下是合法态（启动早期 / deinit 后未摘除）
3. 返回错误码会制造假阳性
4. 与 `deinit` 语义族对齐
5. 不承担"检测忘记 init"职责

这是教科书级的 API 语义论证范例，**应作为后续新 API 设计的论证范式**。

### 2.3 Lint 分级标注（`[LINT-ENFORCED]` / `[LINT-PARTIAL]` / `[MANUAL-REVIEW]`）

用元规范管理规范本身，且明确告知 AI 助手「按行阅读时可跳过 100% Lint 化规则」——工程上很务实。

### 2.4 ABI 探针自动化（§2.3.1）

解决 "手填 `offsetof` 常数 vs 实际编译" 的经典坑。`gcc -m32` + `gcc` 各编一次从 `.s` 产物中解析 `v_<name>` 标号初值——思路虽简但极有效。

### 2.5 DAL-S-006 引脚类型按可选性区分

把 "对 `uint16_t` 写 `pin < 0` 是恒假死代码" 固化为 SHOULD，是**用编译器告警反推 API 形态**的好例子。实证来源是 led v3.4.0 合规整改中"必填 `uint16_t` 引脚做 `<0` 检查是恒假死代码"。

### 2.6 `WARN_UNUSED_RESULT` 豁免白名单（§4.1）

把"应急路径不强制检查"的工程现实显式化（`safe_off` / `poll` / `deinit`），避免与 lint 教条冲突。同时明确"白名单仅豁免 `WARN_UNUSED_RESULT` 属性，不改变返回类型"——**细致**。

---

## 3. 架构级盲区（建议补强）

### 盲区 1：§14.1 错误码分段方案是"待落地"状态 〔**P0**〕

**现状**：数值分段（-1~-99 全局 / -100~-199 DAL / -200~-299 器件特有…）**仅在文档里**，未与 `wink_status.h` 实际头文件强绑定。规范要求器件特有错误码 "MUST 落在预留分段内"，但**当前 `wink_status.h` 没有定义任何错误码值，更没有分段元数据**。

**风险**：未来真正落地时必然引起规范条款与头文件不一致；Linter 也无法静态校验（无法区分"违规"与"未定义"）。

**建议**：
1. 把分段定义本身做成 `wink_status.h` 内的 `WINK_ERR_RANGE_DAL_MIN/MAX` 等宏 + Doxygen 注释，从规范级降到编译期可校验级。
2. 配套 Lint：`dal.quantity.error_code_in_range`，扫描整型字面量（如 `return -456;`）是否落在合法分段内。
3. 实施状态 `DAL-EC-004` 当前为 `pending`（#WINK-DAL-022），**应在 v4.0 前 Close**。

### 盲区 2：8 位 Micro Profile 规范被"独立"出去但缺乏返回主规范的桥梁 〔**P2**〕

**现状**：主体规范多次 `see dal-micro-profile-spec.md`，但**没有总览图**说明「主规范 X 章节在 Micro 视角下对应哪个子规范章节」。这给读者带来很大跳转成本。

**建议**：在主规范首部加一张 **Profile Cross-Reference Matrix**，列：主规范章节 → Micro 子规范章节 / 不适用 / 8 位特有补充。

### 盲区 3：§11 裁剪（`WINK_USE_xxx`）与 §6 并发模型未交叉 〔**P2**〕

**现状**：`WINK_USE_<TYPE>` 关闭某个驱动后，该驱动的实例句柄**根本不存在**，那么 `wink_actuator_registry`（§8.2 safe-off-all）遍历时如何跳过？**这条规则没有说明裁剪态下的 actuator_registry 行为**。

**建议**：新增规则 `DAL-E-011`：`WINK_USE_xxx=OFF` 时 MUST 不注册到 `wink_actuator_registry`，或提供编译期空 stub。

### 盲区 4：§3.1.2 `deinit` 顺序未覆盖"调 `safe_off` 后再 deinit"路径 〔**P0**〕

**现状**：§3.1 给出清场顺序（禁中断 → 等 in-flight → 释放资源 → 清零句柄），但**用户代码层 `safe_off` → `deinit` 之间的幂等性 / 顺序约束**没有规则。

**举例**：
- 用户在 watchdog 路径已 `safe_off` 成功，再正常 `deinit` 是不是该幂等成功？（应该是，但规则没说）
- 反之 `deinit` 后 `safe_off`？（`safe_off` 内部有 `initialized` 检查，会变成 no-op）

这两种正常组合**目前依赖隐式行为**，建议显式化为 `DAL-L-030/031`：

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-L-030 | MUST | deinit 后调用的 safe_off MUST 为 no-op（句柄已清零状态）且不返回错误 |
| DAL-L-031 | MUST | safe_off 后调用 deinit MUST 仍能完成 deinit 语义（即使硬件已断电） |

### 盲区 5：§5.2 `read` / `get` / `get_state` 三元区分未触达"out param 是否缓存" 〔**P1**〕

**现状**：规范说 `get_*` 不碰硬件、读缓存。但**缓存是谁的、由谁失效？** 形如 `dal_dc_motor_get_speed` 读 `dev->current_speed` — 这个缓存是驱动自己维持的，但**没有规则约束缓存的失效时机**。

**举例**：
- 硬件寄存器被外部（PAL ISR）直接修改时，缓存何时刷新？
- deinit 后 `get_*` 返回的缓存值是否仍合法？

**建议**：新增 `DAL-V-020`：`get_*` / `get_cached_*` 出参在 `deinit` 后 MUST 视为失效，驱动 MAY 主动清零缓存字段。

### 盲区 6：§7.4 异步三段式没有「状态机崩溃态」恢复路径 〔**P0**〕

**现状**：状态机定义只有 IDLE / BUSY / DONE / ERROR 的良性迁移。**没有"ERROR 状态如何回到 IDLE"的契约**。常见做法是 `request_*` 检测到 ERROR 时自动重置，但规范没说。

**真实故障路径**：
- I2C 总线恢复失败（§14.5 提到）→ 状态机进 ERROR
- 上层调 `request_*` 期望重试
- 没有规则定义此行为，可能"返回 `WINK_ERR_BUSY` 永不退出"或"重新尝试触发新传输"

**建议**：新增 `DAL-B-026` 规定 `request_*` 在 ERROR 态下的语义：

> 推荐语义：进入 ERROR 后 `request_*` 触发自动恢复迁移 `ERROR → IDLE → BUSY`；恢复失败再次进 ERROR；调用方通过 `get_status` 查询状态。

---

## 4. Lint 可执行性与一致性问题

### 4.1 §1.4 Lint 索引矩阵与 §17.3.1 实施状态存在「覆盖洞」 〔**P0**〕

**现状**：
- 索引矩阵列了 `dal_struct.py` / `dal_api_shape.py` / `dal_quantity.py` 等 6 个 pack；
- 实施状态表又引用了 `api pack: STATUS-NOT-BOOL-PUBLIC`、`layering pack` 等**不在矩阵中的 pack**。

读者要交叉对照才能知道"DAL-F-001 到底由哪个 pack 拦截"——两份表本应合一。

**建议**：把 §1.4 矩阵的「主要覆盖规则」列扩展为「所有规则 ID → pack 路径 → 实施状态」的三栏总表，作为唯一真相源。建议在 §17.3 增补：

```markdown
### 17.3.2 规则权威索引（合并 §1.4 + §17.3.1）

| 规则 ID | 级别 | Lint 状态 | 所属 Pack | 引擎 Rule ID | 实施状态 | 跟踪 Issue |
|---------|------|----------|----------|-------------|----------|----------|
| DAL-S-001 | MUST | LINT-ENFORCED | dal_struct.py | dal.config_owner_first | lint-enforced | #WINK-DAL-001 |
| ...      |      |           |            |               |          |           |
```

### 4.2 §2.3 ABI 探针依赖 gcc，对 MSVC/IAR 闭源工具链失效 〔**P1**〕

**现状**：探针实现是 `gcc -m32 -S` + `gcc -S`。"Linux 嵌入式开发"主流是 GCC，但 ESP-IDF 官方文档已支持 MSVC 后端、IAR 仍占据相当市场（尤其汽车级）。`gcc -m32` 在 MSYS2 / Windows + IAR 环境大概率装不上。

**建议**：
1. 探针层增加 `clang -m32` fallback。
2. 在 `wink.py lint --pack abi` 增加 `--compiler {gcc,clang,auto}` 选项。
3. 对 IAR 链应至少声明"不支持"而非静默 `gcc-multilib not found` 后让 LP64 通过、ILP32 跳过。

### 4.3 §5.3 黑名单的"禁用词"无 Lint 自动化 〔**P1**〕

**现状**：`turn_on` / `enable_output` / `spin` / `start_pwm` / `clean_screen` / `refresh_display` / `fetch_data` / `sample_now` / `get_dist` 都是 **Lint 极易做正则字符串扫描的"坏味道"**，但 `[MANUAL-REVIEW]` 状态意味着无人强制拦截。

**建议**：加 `dal.api.blacklist_verb` 规则——Linter 扫 `dal_<type>_<verb>` 模板时检查 verb ∈ 黑名单 → 报 error。Pack: `dal_api_shape.py`。

### 4.4 §6.2 多字段快照一致性只给了"推荐方案"列表，无硬性规则 〔**P1**〕

**现状**：三种推荐方案（读序契约 / 单原子快照 / seqlock）**地位完全平等**，Linter 无法校验。但这是 SMP 正确性核心，**选错方案是隐蔽 bug 来源**。

**建议**：将"读序契约"提升为 MUST（"至少声明读序契约，且 ISR 写端遵守对称写序"），其他两种作为可选项。

### 4.5 §9.4.5 Setter/Getter 同表示（DAL-U-030）目前是 `[MANUAL-REVIEW]` 〔**P0**〕

**现状**：防止"半整型化撕裂"是 A/B 分类落地的**关键护栏**，应升为 Lint 强制。

**建议**：`dal_quantity.py` 可通过：扫描同一 YAML quantity 在 setter/getter/cache 字段的类型签名必须三处一致。

### 4.6 §11.2 Stub `WINK_ERR_UNSUPPORTED` 缺少 Lint 防退化 〔**P2**〕

**现状**：`DAL-P-012` 要求 Stub MUST NOT 置 `initialized = true`，但这是 review-enforced——某天有人手抖写成 `dev->initialized = true;` 后无人察觉。

**建议**：加 `dal.stub.no_initialized_true` Lint 规则，扫 `if (stub_mode) { dev->initialized = true; }` 模式。

---

## 5. 命名 / 一致性小问题

| 位置 | 问题 | 建议 |
|------|------|------|
| §3.3 `get_state` / §5.2 `get_status` | 规范把它们合并为"查询状态机"，但**类型语义不同**（`state` = 设备侧状态 IDLE/BUSY/...，`status` = 操作结果/健康度）。 | 明确区分：建议 §5.2 表加 "**`state` ∈ {IDLE,BUSY,...} 设备侧状态机；`status` ∈ {OK,FAIL,...} 操作健康度**"，并新增 `DAL-F-015` 约束命名。 |
| §5.3.1 `set` 动词 | 范式 `(dev, bool on)` 与 `set_<property>` 的 `set` **语义冲突**：`set(dev, true)` vs `set_speed(dev, val)` 的 `set` 究竟是"开/关"还是"赋值"？ | 把 `set` (bool) 重命名为 `set_enabled` 或 `set_active`，与 `set_<property>`（属性赋值）不撞名。需走 ADR + deprecation 轨道。 |
| §5.3.2 `calibrate` / `zero` | 同一行混了两个动词，语义差很多（calibrate 是测零点、zero 是直接清零）。 | 分两行或加一句精确说明何时用哪个。 |
| §13.4 API 版本宏 | 给出 `0xMMmmPP` 格式但**字段宽度未明确**（`0x030300` 看着是 MM=03, mm=03, PP=00，但 v3.10.0 就装不下 8 位 mm）。 | 定死 `MAJOR:8, MINOR:8, PATCH:16`，并在头注释显式声明。 |
| §3.3 `self_test` | 说"MAY"，但又规定"MUST 标 `WINK_BLOCKING`"——**MAY 实现的 API 不存在"必须标什么"**，规则动词层级错位。 | 改为："若实现 self_test，签名/标记/注释 MUST 满足..." |
| §17.1 黄金参考动态迁移 | "待 `dal_led_8b` 在 8051 CI 上成功构建后，双 Profile 标杆将迁移至 `led` 驱动" — **这是 Open Issue 写进 spec**，会与具体 Issue 编号脱节。 | 要么用 Issue 编号引用（`#WINK-DAL-040`），要么从规范正文移除、改在 `implementation-plans/` 里跟踪。 |

---

## 6. 文档组织建议

1. **§17.1 合规矩阵应与 §1.4 Lint 索引合并**：分立两份导致「规则 ID 在矩阵里」「包名在索引里」互相割裂。建议合到「规则权威索引」一节，按规则 ID 排序单表列出：
   - 规则 ID、级别、Lint 状态、所属 pack、是否依赖 ABI 探针、是否 Micro Profile 适用、对应 ADR、当前实施状态、跟踪 Issue

2. **"Golden Reference 样板驱动"**应在 spec 里给一个**永久 URL 锚点**（如 `docs/dal-development-guide/golden-ref/dc_motor.md`）而非散落引用 — 避免 driver 重构时链接断。

3. **A/B 两分类决策树**（§9.3 表格）建议加一张**判定流程图**：
   - "数据方向？" → 硬件→App → B
   - "硬件终态是离散寄存器？" → 是 → A
   - 例外：位置 / 角度 / 速度 即使是 App→硬件也是 A（终态是计数器/CCR 整数）

---

## 7. 可量化的「下一步」清单

| 优先级 | 项 | 工作量估计 |
|------|----|----------|
| P0 | 把 §14.1 错误码分段落地到 `wink_status.h` + 配套 Lint | 0.5d |
| P0 | 增 §1.4 + §17.3.1 合并为「规则权威索引」单表 | 0.5d |
| P0 | §3.x 增加 `safe_off ↔ deinit` 顺序约束 | 0.2d |
| P0 | §7.4 补 `DAL-B-026` 规定 `request_*` 在 ERROR 态下的行为 | 0.2d |
| P1 | §6.2 提升"读序契约"为 MUST | 0.2d |
| P1 | §5.3 黑名单词加 Lint 自动化 | 0.5d |
| P1 | 增 §9.5 "缓存失效时机" 规则（DAL-V-020） | 0.2d |
| P1 | §2.3 探针加 clang fallback / `--compiler` 选项 | 0.3d |
| P2 | §11 裁剪态与 `wink_actuator_registry` 交叉规则 | 0.2d |
| P2 | §1.5 增 Profile Cross-Reference Matrix | 0.2d |
| P2 | §5.3.1 `set` (bool) 改名 `set_enabled` 并附 deprecation | 0.3d |

---

## 8. 最后一点观察

本规范最值得外部借鉴的不是某条规则，而是它**对"工程现实 vs 教条"的处理姿态**：

- 允许 `void *dev` 存在但**显式标记为已知技术债**（§4.4 + §17.4）
- 允许稳定驱动的 `float set_speed` 保留但**明确说明它是迁移前现状、不构成新驱动背书**（§17.1 注释）
- 允许 `safe_off` 内部调用非 ISR-safe API，**但要求该 API MUST NOT 标 ISR-safe**（DAL-C-021）

这种"承认不完美 + 把妥协显式化为待办"的工程文化，比一份"什么都 MUST"的规范更有生命力。新增的 §1.4 / §1.5 / §9.3 A/B 两分类、§2.3 ABI 探针、§3.2 safe_off 论证，都是这种文化的具体体现。

**核心建议**：补强 P0 四项后，本规范可以视为 WinkMicroOS DAL 体系的稳定基线 v4.0 发布候选。

---

## 附录 A. 与现有合规评审的分工

| 评审维度 | [2026-08-01-dal-api-consistency-spec-review.md](2026-08-01-dal-api-consistency-spec-review.md) | 本评审 |
|----------|----------|----------|
| 视角 | 内部维护者 / 规则实施 | 外部架构师 / 体系健壮性 |
| 颗粒度 | 规则条逐条对照 | 盲区 + 体系级一致性 |
| 输出 | 合规矩阵 + 实施细则 | 架构师补充建议 + 优先级 |
| 受众 | 维护者、Reviewer | 架构师、决策者、长期演进 |

---

参考文件：
- [dal-api-consistency-spec.md v3.4.2](../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md)
- [ADR-0056 Cross-Profile Quantity AB Class and Scaled Integers](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)
- [ADR-0043 Yaml-Driven Layer Lint](../../decisions/tools/0043-yaml-driven-layer-lint.md)
- [2026-08-01-dal-api-consistency-spec-review.md (上一轮合规评审)](./2026-08-01-dal-api-consistency-spec-review.md)

