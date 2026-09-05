# DAL `encoder` 驱动对照规范 v3.4.1（ADR-0056）全面合规评审

| 项 | 内容 |
|---|---|
| **评审日期** | 2026-08-03 |
| **评审范围** | `dal_encoder.h`、`dal_encoder.c`、`codegen/drivers/encoder.yaml`、`test/unit/dal/test_dal_encoder.c`、`test_dal_abi_freeze.c` 逐规则审计 |
| **基准规范** | [`dal-api-consistency-spec.md` v3.4.1](../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md)（含 [ADR-0056](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)、DAL-S-006） |
| **驱动成熟度** | `experimental: false`（stable，非执行器 `is_actuator: false`） |
| **关联 ADR** | [ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)、[ADR-0024](../../decisions/core/0024-fault-three-phase-model-and-dal-deinit-contract.md)（deinit 清场）、[ADR-0056](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)（量纲分类） |
| **结论状态** | **Resolved（2026-08-03 整改完成）**：6 项已修（F-1~F-6），F-7 按规范"stable 不强制"不改；host 单测 **15/15 PASS**（`-Werror` 零警告，新增 8 个用例覆盖回滚/幂等/pull 变体）；`wink lint` 无 finding |

---

## 1. 评审结论

`encoder` 是正交脉冲解码驱动（x1 模式：A 上升沿采样 B 相），stable。核心 ISR 解码逻辑正确、测试覆盖全面（单相/双向/invert/x2-x4 拒绝），volatile count 与临界区 reset 设计合理。

**优点（保留）：**
- ISR 解码逻辑简洁正确：单相 `count++`、双向采样 B 相、invert 翻转极性
- `volatile int32_t count` + `PAL_CRITICAL_SECTION` reset 符合 DAL-C-001/002
- `get_count` 用 const dev（DAL-F-011），正确标注 `ISR-safe: Yes`（单字 aligned 读）
- x2/x4 变体 fail-closed 返回 UNSUPPORTED，不预留半实现
- deinit 正确先 disable+synchronize ISR 再 reset GPIO
- 可选 pin_b 用 `wink_pin_t`（int16_t）+ `-1` 哨兵，必填 pin_a 同理（均为可选/必填但当前统一 wink_pin_t，见 F-7）

**问题按严重度：**

| # | 严重度 | 规则 | 问题 |
|---|--------|------|------|
| F-1 | **P1 缺陷** | DAL-L-007/008 | init 在 ISR 注册**之前**就置 `initialized=true`；ISR 注册失败时 err_release 路径**不回滚 initialized/config**，留下"initialized=true 但资源已释放"的僵尸句柄 |
| F-2 | P2 | DAL-EC-020/021/023 | 驱动零日志（无 LOG_TAG、无 LOG_I/W） |
| F-3 | P2 | DAL-L-014 | deinit 及 init 回滚中 `pal_resource_release` 失败被 `WINK_IGNORE_UNUSED` 静默，无 LOG_W |
| F-4 | P3 | DAL-BC-010/§2.3 | 缺 32/64 位分档 sizeof/offsetof ABI 断言（仅有 offsetof(config)==0） |
| F-5 | P3 | §15 | init Contract 缺 Side-effects；get_count/reset/deinit 缺 Side-effects；init Range 不完整 |
| F-6 | P3 | DAL-S-015 | config 中 `pal_gpio_mode_t pull` 直接暴露 PAL 类型到 DAL 公开头（PAL 类型泄漏）；与 dc_motor/rc_servo 用 DAL 自有枚举的做法不一致 |
| F-7 | ℹ️→SHOULD | DAL-S-006 | pin_a 是必填引脚但用 `wink_pin_t`（int16_t），按新约定必填用 `uint16_t`；pin_b 可选保留 wink_pin_t 正确。当前 init 已校验 `pin_a < 0`，非 bug，但与 DAL-S-006 不完全一致 |

> **评级图例**：✅ 合规 · ❌ 不合规 · ⚠️ 部分 · ℹ️ 观察 · N/A 不适用
> **末列"是否解决"留空**，整改后回填。

---

## 2. 逐规则合规清单

### 2.1 数据结构与句柄（§2）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-S-001 | config 首成员 owner | ✅ | `dal_encoder.h:33` | |
| DAL-S-002 | owner 静态存储 | ✅ | init 校验非 NULL | |
| DAL-S-003 | 成员按尺寸降序 | ⚠️ | config：ptr(8)→int16×2(4)→enum(int,4)→enum(int,4)→bool(1)，有填充但可接受；句柄 volatile int32→bool×2，有尾填充 | |
| DAL-S-005 | 禁位域/pack | ✅ | 无 | |
| DAL-S-006 | 必填引脚 uint16_t / 可选 wink_pin_t | ⚠️ | **F-7**：pin_a 必填用 wink_pin_t（int16_t），pin_b 可选 wink_pin_t 正确。dc_motor dir_pin_a 同样历史宽松，规范明确"不强制改" | |
| DAL-S-010 | POD | ✅ | | |
| DAL-S-011 | config 首成员 | ✅ | `:45` + `_Static_assert` `:52` | |
| DAL-S-012 | bool initialized | ✅ | `:47` | |
| DAL-S-013 | {0} 零初始化安全 | ✅ | | |
| DAL-S-014 | offsetof 断言 | ✅ | `:52`（缺整尺寸，见 F-4） | |
| DAL-S-015 | init 后 config 不可变 | ✅ | 仅 init `:75` 写 config | |
| DAL-S-015b | **PAL 类型不泄漏到 DAL 公开头** | ❌ | **F-6**：`:8` include `pal_hal.h`，`:36` config 直接用 `pal_gpio_mode_t pull`。dc_motor 用自有 `dal_dc_motor_variant_t`，rc_servo 用 `dal_rc_servo_clock_requirement_t`，encoder 应定义 `dal_encoder_pull_t` 并在 .c 映射到 pal_gpio_mode_t | |
| §2.3/BC-010 | 分档 ABI 断言 | ❌ | **F-4**：仅有 offsetof(config)==0。64 位实测 config=24/handle=32/init@28；32 位推导 config=20/handle=28/init@24 | |
| DAL-S-020 | Full 下不 malloc | ✅ | 无堆 | |

### 2.2 生命周期（§3）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-L-001 | init 校验 dev/cfg 非 NULL | ✅ | `:31-33`，含 owner NULL `:34` | |
| DAL-L-002 | 深拷贝 cfg | ✅ | `memcpy` `:75` | |
| DAL-L-003 | 成功置 initialized=true | ❌ | **F-1**：`:77` 在 ISR 注册 `:81` **之前**置位；若 ISR 注册失败，err_release 不回滚 initialized | |
| DAL-L-004 | 重复 init 返 ALREADY_INITIALIZED | ✅ | `:43` | |
| DAL-L-005 | 最小防御校验 | ✅ | pin_a `<0` `:37`、variant 校验 `:40`、channel 类 N/A | |
| DAL-L-006 | init 后零能量 | N/A | 传感器无输出能量；count=0 `:76` 正确 | |
| DAL-L-007 | 失败回 initialized=false | ❌ | **F-1**：ISR 注册失败路径 initialized 已为 true 且未重置 | |
| DAL-L-008 | init 失败资源回滚 | ⚠️ | claim 失败逆序释放正确；GPIO init 失败 reset+release 正确；**ISR 注册失败** reset GPIO + release claim，但**未重置 dev 状态**（F-1） | |
| DAL-L-010 | deinit 幂等 | ✅ | `:139` 未 init 返 OK | |
| DAL-L-011 | 清场顺序：卸ISR→GPIO reset→释放→memset | ✅ | `:144-165` 顺序正确（disable+synchronize ISR → reset pin_a/pin_b → release → memset） | |
| DAL-L-012 | 用 ISR 时先禁中断等 in-flight | ✅ | `:145-146` disable + synchronize_interrupt | |
| DAL-L-013 | 共享总线只释放自身 | N/A | 独占 GPIO | |
| DAL-L-014 | 清场失败 LOG_W | ❌ | **F-3**：init 回滚和 deinit 中 release 全用 `WINK_IGNORE_UNUSED` 静默 | |
| DAL-L-015 | deinit best-effort + initialized=false | ✅ | memset `:165` | |
| DAL-L-020~025 | safe_off（执行器） | N/A | `is_actuator: false`，无 safe_off | |

### 2.3 签名 / 返回值（§4）

| 规则 ID | 状态 | 证据 | 是否解决 |
|---------|------|------|----------|
| DAL-F-001 返回 wink_status_t | ✅ | 全部 | |
| DAL-F-002 禁 bool 返回 | ✅ | | |
| DAL-F-004 WARN_UNUSED_RESULT + 白名单 | ✅ | init/get_count/reset 标注；deinit 不标（白名单） | |
| DAL-F-010 首参句柄 | ✅ | | |
| DAL-F-011 查询用 const dev | ✅ | `get_count(const dal_encoder_t*)` `:85` | |
| DAL-F-012 修改用非 const | ✅ | init/reset/deinit | |
| DAL-F-013 出参 out_ 前缀 | ✅ | `out_count` `:85` | |
| DAL-F-014 init 第二参 const cfg | ✅ | | |
| DAL-F-020 错误返回时出参不变 | ✅ | get_count 仅成功末尾写 `*out_count` `:114` | |

### 2.4 动词与命名（§5）

| 规则 | 状态 | 说明 | 是否解决 |
|------|------|------|----------|
| `dal_<type>_<verb>` 格式 | ✅ | init/get_count/reset/deinit | |
| 动词在标准库 | ✅ | get_*（缓存读，不碰硬件）/ reset | |
| get_count 不触发硬件采样 | ✅ | 仅读 volatile 缓存 `:114` | |
| 无黑名单动词 | ✅ | | |

### 2.5 并发 / ISR（§6）—— encoder 重点

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-C-001 | volatile 单字宽单写者 | ✅ | `volatile int32_t count` `:46`，ISR 单写者，32 位对齐单字 | |
| DAL-C-002 | RMW 用原子/临界区 | ✅ | reset 用 `PAL_CRITICAL_SECTION` `:127` | |
| DAL-C-003 | 禁空口"无需临界区" | ✅ | 注释解释了单字 volatile 读容忍旧值 | |
| DAL-C-010 | 多字段快照一致性 | ✅ | 仅 count 一个共享字段 | |
| DAL-C-040/042 | 默认非线程安全并标注 | ✅ | 头注释每个 API 标 Thread-safe | |
| DAL-C-020~022 | ISR 上下文/声明 | ✅ | get_count 如实标 `ISR-safe: Yes`；reset 标 `ISR-safe: No`（临界区在 task 上下文） | |
| DAL-C-030/031 | 回调上下文 | N/A | 用 PAL_DEFINE_ISR 宏，非用户回调 | |

### 2.6 阻塞 / DMA（§7）

| 规则 | 状态 | 说明 | 是否解决 |
|------|------|------|----------|
| 全 API 非阻塞 | ✅ | ISR 注册/GPIO 配置无 busy-wait | |
| DAL-BUF-001~003 DMA | N/A | 无 DMA | |

### 2.7 失效安全 / 向后兼容（§8、§13）

| 规则 | 状态 | 说明 | 是否解决 |
|------|------|------|----------|
| DAL-BC-001 Init-to-Ready | ✅ | 无 arm/enable 前置 | |
| 句柄/config 只追加不重排 | ⚠️ | stable，无 apply_override 线序负担 | |

### 2.8 单位、量纲（§9，ADR-0056）

| 规则 | 状态 | 说明 | 是否解决 |
|------|------|------|----------|
| DAL-U-001 物理量带后缀 | ✅ | count 是脉冲计数（无量纲整数），int32_t 合理 | |
| DAL-U-010 Range 声明 | ⚠️ | get_count 注释说明 int32 范围；init 缺 pull/pin Range | |
| DAL-U-022 禁弱 typedef | ✅ | 无 | |
| A/B 分类 | ✅ | encoder count 是 **B 类传感器测量**（硬件→App），Full 用 int32_t 整数（非 float），天然符合"测量量用整数/定点"；Micro 同类型，无跨 Profile 类型分化 | |
| count 溢出/回绕 | ℹ️ | int32_t 在极高速/长时间可能溢出（约 21 亿脉冲）；正交编码器通常有 CPR/转速换算在上层，int32 对绝大多数场景足够。若需溢出保护属上层逻辑，非本驱动 MUST | |

### 2.9 裁剪 / 双 Target（§11、§12）

| 规则 | 状态 | 证据 | 是否解决 |
|------|------|------|----------|
| DAL-P-001 WINK_USE_ENCODER | ✅ | `:116` | |
| DAL-P-002 裁剪 stub WINK_UNAVAILABLE_MSG | ✅ | `:120-127` 全 4 函数 | |
| DAL-P-003 提示启用 | ✅ | WINK_ENCODER_DISABLED_MSG | |
| DAL-T-001 dal/ 无平台宏 | ✅ | `.c` 无 `#ifdef SIMULATION/ESP_PLATFORM` | |
| DAL-T-002 同源进两 target | ✅ | | |
| DAL-T-003 时间走 PAL | N/A | 无时间调用 | |

### 2.10 错误码与日志（§14）

| 规则 ID | 状态 | 证据 | 是否解决 |
|---------|------|------|----------|
| DAL-EC-001/002 通用错误码 | ✅ | INVALID_ARG/UNSUPPORTED/ALREADY_INITIALIZED/NOT_INITIALIZED/BUSY | |
| DAL-EC-020 init 成功 INFO | ❌ | **F-2**：无 LOG_I | |
| DAL-EC-021 init 失败 WARN | ❌ | **F-2**：claim/gpio/ISR 失败直接返回，无 LOG_W | |
| DAL-EC-022 ISR 不打日志 | ✅ | ISR 内无日志（正确） | |
| DAL-EC-023 tag dal_<type> | ❌ | **F-2**：无 `#define LOG_TAG`，未 include pal_log.h | |
| DAL-EC-030 init 防御校验 | ✅ | owner/pin/variant | |

### 2.11 API Contract 注释（§15）

| 函数 | Pre | Post | Range | Blocking | Thread-safe | ISR-safe | Side-effects | Error-codes | 状态 | 是否解决 |
|------|-----|------|-------|----------|-------------|----------|--------------|-------------|------|----------|
| init | ✅ | ✅ | ⚠️ 部分 | ✅ | ✅ | ✅ | ❌ 缺 | ✅ | ❌ F-5 | |
| get_count | ✅ | ✅ | N/A | ✅ | ✅ | ✅ | ❌ 缺 | ✅ | ❌ F-5 | |
| reset | ✅ | ✅ | N/A | ✅ | ✅ | ✅ | ❌ 缺（清零 count） | ✅ | ❌ F-5 | |
| deinit | ✅ | ✅ | N/A | ✅ | ✅ | — | ⚠️ 注释提了 ADR-0024 但无 Side-effects 字段 | ❌ 缺（仅写 idempotent） | ❌ F-5 | |

### 2.12 Codegen YAML（§16）

| 项 | 状态 | 说明 | 是否解决 |
|----|------|------|----------|
| codegen_schema 1.1 | ✅ | | |
| type/category/is_actuator/experimental | ✅ | encoder/sensor/false/false | |
| default_role | ✅ | pulse_counter | |
| fields 声明 | ✅ | pin_a/pin_b/pull/variant/invert/cpr | |
| pin_b default -1 | ✅ | 可选引脚哨兵 | |
| config 映射齐全 | ✅ | c_type/config_type/headers/deinit_fn；safe_off_fn=""（非执行器） | |
| role_binding | ✅ | get_count/reset，int32_t out_count | |
| profiles 显式声明 | ℹ️ | 未写，默认 Full；可补 `[full]` | |
| quantity/quantity_class | N/A | count 是 B 类整数测量，无 A 类标量命令 | |

---

## 3. 不合规项整改清单

| # | 严重度 | 规则 | 整改建议 | 是否解决 |
|---|--------|------|----------|----------|
| F-1 | **P1 缺陷** | DAL-L-003/007/008 | `initialized=true` 移到 ISR 注册成功之后；ISR 失败路径 `memset` 清零暂存的 config，确保不留僵尸句柄 | ✅ 2026-08-03 `dal_encoder.c:141-143`（提交点在全部资源就绪后）；ISR 失败路径 `:136` memset；新增 `test_encoder_init_rollback_releases_pin_a_on_pin_b_conflict` |
| F-2 | P2 | DAL-EC-020/021/023 | `#define LOG_TAG "dal_encoder"` + `pal_log.h`；init 成功 LOG_I（pin_a/pin_b/pull/invert），claim/gpio/ISR 失败 LOG_W | ✅ 2026-08-03（单测可见 `[I] [dal_encoder] init ready`） |
| F-3 | P2 | DAL-L-014 | 抽 `release_gpio_claim_logged(pin, owner)`，init 回滚与 deinit 中 release 失败记 LOG_W 但不中断清场 | ✅ 2026-08-03 `dal_encoder.c:23-33` |
| F-4 | P3 | DAL-BC-010/§2.3 | 补 `#if INTPTR_MAX` 分档断言（64 位实测 24/32/28；32 位推导 20/28/24） | ✅ 2026-08-03 `dal_encoder.h:70-79`；64 位编译验证通过 |
| F-5 | P3 | §15 | 全函数补 Side-effects；init 补完整 Range；get_count 补溢出说明；deinit 补 Postconditions/Error-codes | ✅ 2026-08-03 |
| F-6 | P3 | DAL-S-015b | 新增 `dal_encoder_pull_t{UP=0,DOWN=1,NONE=2}`，config 改用它；`.c` 内 `encoder_map_pull()` 映射到 `pal_gpio_mode_t`；YAML map 改为 DAL 枚举 | ✅ 2026-08-03；与既有 `dal_button_pull_t` 范式一致；非法 pull 返回 INVALID_ARG（新增 `test_encoder_init_rejects_invalid_pull`）；同步 2 个 BAL 测试调用点 |
| F-7 | ℹ️ | DAL-S-006 | pin_a 必填仍用 `wink_pin_t` | ✅ 按规范"stable 历史宽松不强制改"**不改**（已有 `pin_a < 0` 校验） |

---

## 4. 观察项（ℹ️）

| # | 观察 | 建议 |
|---|------|------|
| O-1 | count 为 int32_t，极端长期高速可能溢出 | 上层转速/位置换算处理；本驱动保持 int32 即可，文档注明范围 |
| O-2 | `isr_registered` 字段在 deinit memset 后归零，逻辑正确；但 init 失败路径如果未来在置 initialized 前注册了部分 ISR，需要此字段回滚 | 随 F-1 修复（initialized 延后设置）后更健壮 |
| O-3 | deinit 注释的 ADR-0024 checklist 很完整（注释在 .c 内），值得保留 | 可提升到头文件 Contract |
| O-4 | YAML `cpr`（counts per revolution）字段 `emit: none`，供上层 BAL 使用但不进 config | 设计合理，无问题 |
| O-5 | 单相模式（pin_b=-1）下 invert 被忽略（`:24` 注释 N/A），逻辑正确 | 可在头注释明确 |

---

## 5. 测试覆盖评估

**整改前**（7 个用例）：NULL、未初始化、单相计数、双向计数、显式 x1、invert 极性、x2/x4 拒绝。缺口：init 错误路径回滚、deinit 循环无泄漏、reset 边界。

**整改后**（15 个用例，✅ 全 PASS）新增 8 个：
- `test_encoder_init_rejects_negative_pin_a` — pin_a 必填校验
- `test_encoder_init_rejects_invalid_pull` — 非法 pull 枚举拒绝 + claim 不泄漏
- `test_encoder_get_count_null_out_returns_invalid_arg` — 出参/句柄 NULL
- `test_encoder_double_init_returns_already_initialized` — DAL-L-004 fail-fast
- `test_encoder_init_rollback_releases_pin_a_on_pin_b_conflict` — **DAL-L-008 回滚**（预占 pin_b 造 BUSY，验证 pin_a claim 被释放）
- `test_encoder_deinit_loop_no_resource_leak` — ADR-0024 §4 #8 十轮循环
- `test_encoder_deinit_idempotent` — NULL / 未 init / 已 deinit
- `test_encoder_pull_variants_accepted` — PULL_NONE/PULL_DOWN 映射覆盖

---

## 6. 总评

encoder 驱动**解码核心逻辑质量高**（ISR 简洁正确、volatile/临界区使用规范、const getter、x2/x4 fail-closed、测试覆盖主路径），是稳定的传感器驱动。

唯一的实质缺陷是 **F-1（initialized 置位早于 ISR 注册，失败路径留僵尸句柄）**——这是把"状态提交"放在"资源就绪"之前的经典初始化顺序错误，修复简单（移一行）但重要。其余为日志、ABI 断言、Contract 注释、PAL 类型解耦等工程完备度项，照 led/rc_servo 样板补齐即可。

量纲方面 encoder 无压力：count 是 B 类整数测量，天然无 float，不涉及 ADR-0056 A 类整型化。

**建议执行顺序**：F-1（移 initialized 行 + 补测试）→ F-2/F-3（日志/释放可追溯）→ F-6（DAL 自有 pull 枚举）→ F-4（ABI 断言）→ F-5（Contract）。F-7 不改。

---

## 7. 整改记录（2026-08-03）

**代码变更**
- `dal_encoder.h`：新增 `dal_encoder_pull_t` 枚举（F-6）；32/64 位分档 ABI 断言（F-4）；全函数 Contract 补 Side-effects/Range/Postconditions/Error-codes（F-5）；init 注释明确"initialized 仅在 ISR 注册成功后置位"。
- `dal_encoder.c`：`LOG_TAG` + `pal_log.h`（F-2）；`release_gpio_claim_logged()`（F-3）；`encoder_map_pull()` DAL→PAL 映射（F-6）；**`initialized=true` 移到 ISR 注册成功之后，失败路径 memset 清零暂存 config**（F-1）；deinit ADR-0024 checklist 注释规整。
- `encoder.yaml`：`pull` map 由 `PAL_GPIO_INPUT_*` 改为 `DAL_ENCODER_PULL_*`。
- `test_dal_encoder.c`：pull 值改 DAL 枚举；新增 8 个用例（见 §5）。
- `test_bal_chassis.c` / `test_bal_closed_loop_dc_motor.c`：3 处 encoder config 的 `.pull` 同步为 `DAL_ENCODER_PULL_UP`。

**验证**
- host 单测：**15/15 PASS**（MinGW gcc 16，`-Wall -Wextra -Werror` 零警告）；64 位 ABI 断言实测通过（config=24 / handle=32 / initialized@28）。
- `wink lint --pack layering --pack api`：No findings。
- 两个 BAL 测试的 encoder/pull 相关编译错误已清零（经 `-fsyntax-only` 定向核验）。

**未混入的预存问题（非本次引入）**
- `test_bal_chassis.c` / `test_bal_closed_loop_dc_motor.c` 仍引用 `dal_dc_motor_t.current_speed`，而 dc_motor 此前已重命名为 `current_speed_promille`（会话前改动）。属独立的 dc_motor 重构测试漂移，与 encoder 整改无关，应由 dc_motor 任务同步（同 `test_motor_encoder.py` 那笔漂移）。

---

*本评审为时间点快照（文档第④层），归档后不随代码变动修改；整改结果在 §3 末列回填。*

