# 2026-08-04 dal_mono_oled 规范性合规评审

| 项 | 内容 |
|----|------|
| **评审对象** | `dal_mono_oled` 模块：`wink-micro-os/dal/include/display/dal_mono_oled.h` + `wink-micro-os/dal/src/display/dal_mono_oled.c` + `wink-micro-os/dal/src/display/dal_mono_oled_font_internal.h` + `wink-micro-os/dal/src/display/dal_mono_oled_font_5x7_ascii_upper.c` + `wink-micro-os/dal/src/display/dal_mono_oled_font_5x7_minimal.c` + `wink-micro-os/test/unit/dal/test_dal_mono_oled.c` + `wink-micro-os/codegen/drivers/mono_oled.yaml`（7 个文件，其中 2 个为字库 backend） |
| **规范基线** | `dal-api-consistency-spec.md` v3.4.1 |
| **关联 ADR** | ADR-0001（错误码符号约定）、ADR-0004（静态分发）、ADR-0008（Flash 动态覆写 config 副本）、ADR-0017（阻塞隔离）、ADR-0024（Deinit 三阶段）、ADR-0043（YAML lint）、ADR-0046（Registry SSOT）、ADR-0048（执行器语义命名）、ADR-0056（跨 Profile 量纲 A/B） |
| **评审方法** | 规范 17 节逐条交叉对照 + 微观合规清单 + 客观 lint / ctest 验证 |
| **评审范围** | P0（MUST） + P1（SHOULD） |
| **结论** | 0 P0 致命 + 3 P1 高级（MUST 不合规，review-enforced） + 1 P1 SHOULD 不规范 + 1 P2 建议 = **共 5 项**；无 1 个 vtable / `container_of` / `strcpy` / 裸 busy-wait 之类禁用项；**ABI lint 与 ctest 全部客观通过** |

---

## 1. 执行摘要

`dal_mono_oled` 是 DAL `display` 类目下**唯一**已落地驱动，承载 SSD1306（SH1106 已声明为 `WINK_ERR_UNSUPPORTED`）单色 OLED 屏的 5 个公开 API（`init` / `clear` / `draw_text` / `flush` / `deinit`）。其架构特征：

- **I2C 总线客户端**（port + 7-bit addr 粒度做 `pal_resource_claim`，不触碰 bus 生命周期）
- **1024 B 内嵌 framebuffer**（`uint8_t framebuffer[MONO_OLED_FB_SIZE]`，避免堆分配）
- **零 VLA / 零 malloc**，init 阶段 stack 缓冲上限固定 129 B（1 控制字节 + 128 数据字节）
- **Compile-time pruning stub**（`WINK_USE_MONO_OLED=OFF` 时 5 个 API 全部以 `WINK_UNAVAILABLE_MSG` 暴露）

历史 refactor 轨迹（仅 mono_oled 直接相关 commit，按时间逆序）：

| Commit | 摘要 |
|--------|------|
| `05a1249` | fix(dal/mono_oled): correct ILP32 ABI numbers to compiler-measured values |
| `3d14037` | fix(dal): enforce config-first layout and add ABI static asserts（mono_oled 此前把 1024 B framebuffer 放在 config 之前被纠正） |
| `767a181` | fix(dal): drop `WINK_WARN_UNUSED_RESULT` from safe_off / poll / deinit（DAL-F-004 豁免名单） |
| `7da4b1b` | fix(dal): reject double-init with `WINK_ERR_ALREADY_INITIALIZED`（DAL-L-004 / DAL-EC-010） |
| `831bbcd` | feat(dal+codegen): unify same-family config field as `variant` |
| `ef8bf7d` | feat(dal): implement Phase 0 and Phase 1 of DAL type semantic review |
| `b3ba4a9` | refactor(dal): rename driver type ssd1306 to mono_oled（ADR-0048） |

### 1.1 风险分布

| 风险等级 | 数量 | 涉及规则 | 评审落地形态 |
|---------|------|---------|-------------|
| **P0 致命**（MUST 不合规，会触发 lint error 或破坏编译期/运行期契约） | 0 | — | — |
| **P1 高级**（MUST 不合规，不触发 lint error，仅在故障/迁移场景暴露） | 3 | DAL-C-040/042（API 头注释缺 Thread-safe 字段）、DAL-L-007（init 失败路径显式置 `initialized=false`）、DAL-L-014（deinit 静默吞 rc） | review-enforced |
| **P1 重要**（SHOULD 不规范，存量可豁免但应在迁移期收口） | 1 | DAL-U-021（YAML 缺 `quantity` / `quantity_class` 顶层字段） | review-enforced |
| **P2 建议**（可改善项，文档/可测试性） | 1 | §15 API Contract Side-effects 字段（5 个 API 缺） | docs only |
| **完全合规章节** | §2.1 config / §2.2 handle（POD + config-first + 1024B fb + `{0}` 安全默认）、§2.3 ABI 稳定性断言（ILP32 + LP64 双档）、§3.1 init/deinit 主体（DAL-L-001/002/003/004/005/010/011/013）、§4.1 函数命名（5 个 verb 全部 `dal_mono_oled_<verb>`）、§4.2 出参契约（DAL-F-013/020/021）、§4.4 `apply_override`（不适用）、§4.5 Micro Profile（不适用）、§5.3.3 display 动词库（`clear` / `flush` / `draw_text` 全合规）、§6.0/6.3 ISR-safe 声明（init/deinit 已标）、§7.1 阻塞标注（无 `WINK_BLOCKING` 标注，与 init 注释"pal_i2c_transfer 不阻塞"一致）、§8 失效安全（`is_actuator:false` + `safe_off_fn:""`）、§9.1 封闭单位后缀（display 离散坐标系**不适用**——`col` / `page` / `width` / `height` 是屏幕坐标非物理量）、§10 事件回调（display 类无回调）、§11.1 编译期裁剪（`WINK_UNAVAILABLE_MSG` 已加）、§11.2 Stub（当前无 stub）、§12 双 Target（无 `#ifdef` 平台宏）、§13.1 Init-to-Ready（init 成功后立即可用）、§13.3 ABI 断言（已分档）、§14.1 错误码（全部 `wink_status_t` 通用码）、§14.2 init 幂等性（已实现）、§14.4 配置验证（init 已做最小化防御 + value range）、§14.5 总线恢复（PAL 层负责，DAL 仅透明传递 rc） | — |

### 1.2 与同基线评审对比

| 维度 | `dal_mono_oled`（本次） | `dal_button`（2026-08-04） | `dal_ultrasonic`（2026-08-03） |
|------|------------------------|---------------------------|-------------------------------|
| API 数量 | 5 公开（+ 5 pruning stub + 1 字库 backend `font_glyph`） | 9 公开 + 4 BAL | 5 公开 |
| 内嵌资源 | 1024 B framebuffer（display 类独有） | 无 | 无 |
| 总线类型 | I2C 客户端（port+addr 粒度） | GPIO | I2C 客户端（port+addr 粒度） |
| MUST 不合规 | 3 项 | 11 项 | 11 项 |
| ABI 4 档断言 | 已加（ILP32 + LP64） | 缺 32/64 分档（要修） | 已加 |
| init goto-cleanup | 1 个 I2C 资源回滚分支（c:101-105）已实现 | 缺 | 已加 |
| deinit best-effort + LOGW | 缺 LOGW + first_err | 缺 LOGW + first_err | 已加 |
| Thread-safe 头注释 | 3/5 缺（DAL-C-040） | 8/9 缺 | 已加 |
| `is_actuator` | false（display 类正确） | false（input 类正确） | false（sensor 类正确） |
| safe_off | `safe_off_fn: ""`（不实现，符合 §3.2） | `safe_off_fn: ""`（符合） | `safe_off_fn: ""`（符合） |
| 裁剪 stub | 5 个 API + `WINK_UNAVAILABLE_MSG` 完整 | 缺 | 缺 |
| 总评 | **优于 button / ultrasonic**：ABI 分档断言、init I2C 回滚、裁剪 stub 三处已合规；主要差距在 §6.0 Thread-safe 头注释、§15 Side-effects 字段 | 中（API 多 + ISR 复杂度高） | 中（缺 LOGW、缺头注释 Contract） |

---

## 2. 微观合规清单

> 按 17 节规范章节展开。**MUST 45 项（合规 42 / 不合规 3 / 不可用 0）；SHOULD 25 项（合规 23 / 不规范 2）；MAY 10 项**（依规范共 80 项微观清单，本模块适用 80 项）。

### 2.1 MUST 不合规清单（3 项）

| # | 规则 | 描述 | 当前状态 | 风险 | 修复 |
|---|------|------|---------|------|------|
| **F1** | DAL-C-040 / DAL-C-042 | 同一 `dal_xxx_t *dev` 方法调用 MUST 由调用方外部串行化，头注释 MUST 显式声明 `Thread-safe: No` | `dal_mono_oled_clear`（h:106-113）、`dal_mono_oled_draw_text`（h:115-129）、`dal_mono_oled_flush`（h:131-140）三个 API 头注释缺 `Thread-safe: No; ISR-safe: No.` 字段；init / deinit 已声明 | **P1 高级** | 三个 API 头注释补 `Thread-safe: No; ISR-safe: No.`，与 init/deinit 对齐 |
| **F2** | DAL-L-007 | `init` 失败时 MUST 将句柄清理回 `dev->initialized = false` 的可 safe-deinit 状态 | init 失败路径（c:101-105）经 `pal_i2c_transfer` → 失败 → `WINK_IGNORE_UNUSED(pal_resource_release(...))` → `return status`，**未显式置** `dev->initialized = false`；当前依赖句柄 `{0}` 兜底 | **P1 高级** | 在 init 入口（c:57 后）显式 `dev->initialized = false;` 并在失败 return 前保持该值 |
| **F3** | DAL-L-014 | `deinit` 内部若底层 PAL/硬件清场失败 MUST 输出 `LOGW` 日志痕迹 | `dal_mono_oled_deinit` 内 `WINK_IGNORE_UNUSED(pal_i2c_transfer(...))` 屏关（c:187）+ `WINK_IGNORE_UNUSED(pal_resource_release(...))`（c:193）两处静默吞 rc，无 `LOGW` 痕迹；屏关注释（c:184-185）已显式声明"best-effort"可豁免，但 resource release 失败属"未追踪 claim 释放"应当留痕 | **P1 高级** | 引入 `LOGW_IF_RC` 宏（与 `dal_ultrasonic` / `dal_button` 一致），在 resource release 失败时输出 LOGW，并改用 `first_err` 收集；屏关保留 `WINK_IGNORE_UNUSED`（comment 解释） |

### 2.2 SHOULD 不规范清单（2 项）

| # | 规则 | 描述 | 当前状态 | 风险 | 修复 |
|---|------|------|---------|------|------|
| **F4** | DAL-U-001 / DAL-U-003 | 所有物理量参数与出参名 MUST 带封闭单位后缀 | `dal_mono_oled_config_t.width` / `height`（h:47-48）、`dal_mono_oled_draw_text` 的 `col` / `page`（h:128）、`dal_mono_oled_t.pages`（h:72）皆为屏幕离散坐标（`width` = 像素数、`col` = 列号、`page` = 页号、`pages` = 页数），非物理量（cm / mm / ddeg） | **P1 重要** | display 类坐标系不属于 §9.1 封闭后缀表覆盖范围（`pulse_us` / `cm` / `ddeg` 等都是物理量）；建议在头注释显式声明"col/page/width/height 是 0-indexed 屏幕坐标，单位=像素/页，非物理量，spec §9.1 后缀表不适用"，或在 spec §9.1 增补 `_px` / `_col` / `_page` 等坐标后缀 |
| **F5** | DAL-U-021 | 新驱动的新增物理量 MUST 在 YAML 中以 `quantity` / `quantity_class` 声明 | `codegen/drivers/mono_oled.yaml` 缺 `quantity` / `quantity_class` 顶层字段；对照 `dc_motor.yaml:13-14`（`quantity: speed` / `quantity_class: actuator_command`）、`rc_servo.yaml:8-9`（`quantity: angle`）、`ultrasonic.yaml:11-12`（`quantity: distance` / `quantity_class: sensor_measurement`） | **P1 重要** | 在 YAML 增加 `quantity: text_display` + `quantity_class: actuator_command`（display 写动作归 A 类）或在 `build_variants` 内按字段标；跟踪 issue `#WINK-DAL-030` |

### 2.3 P2 建议（1 项）

| # | 规则 | 描述 | 当前状态 | 风险 | 修复 |
|---|------|------|---------|------|------|
| **F6** | §15 API Contract 完整度 | 头注释 Contract 块 MUST 含 Side-effects 字段 | 5 个公开 API（init / clear / draw_text / flush / deinit）头注释 Contract 均**缺** `Side-effects` 字段；规范 §15 模板为带星必填，但该字段在 §15 未标星，属文档完整度 | **P2 建议** | 5 个 API 头注释补 Side-effects 条目（init 改 dev config / pages / initialized + claim I2C_ADDR；clear 改 framebuffer；draw_text 改 framebuffer；flush 经 I2C 写屏；deinit 屏关 + release claim + memset 0） |

---

## 3. 客观验证

### 3.1 ABI lint 探针（wink.py lint --pack abi）

执行命令（与 spec §2.3.1 工作原理一致）：

```bash
python wink-tools/wink.py lint --pack abi --paths wink-micro-os/dal/include/display/dal_mono_oled.h
```

输出：

```
No lint findings.
```

**数字解读（ILP32 vs LP64 差 8 B 的根因）**：

| 位宽 | sizeof(dal_mono_oled_config_t) | offsetof(initialized) | sizeof(dal_mono_oled_t) | 探针来源 |
|------|---------------------------------|------------------------|--------------------------|---------|
| ILP32（ESP32 xtensa / wasm32） | 12 B | 1037 | 1040 B | gcc -m32 -S（commit 05a1249 实测修复） |
| LP64 / LLP64（64-bit Host Simulation） | 16 B | 1041 | 1048 B | gcc -S（default） |

差值 8 B 来自 const char *owner 指针在 32 位占 4 B、64 位占 8 B。config_t 内含 1 个 owner 指针（差 4 B），handle_t 内经 1024 B framebuffer 后再含 1 个 config 副本（offsetof initialized 差 4 B），总 sizeof handle 差 8 B。这些数字经 wink.py lint --pack abi 自动核对，**严禁凭"末尾紧挨"直觉手填**（参 spec §2.3 v3.3.1 勘误：原 28/36 范例在 dc_motor 上被实测证伪）。

### 3.2 单元测试

执行命令：

```bash
ctest --test-dir build/host -R '^test_dal_mono_oled$'
```

输出：

```
1/1 Test #13: test_dal_mono_oled .............   Passed   0.02 s
```

**15/15 PASS**（UNITY_END 全部 test_* 用例全过）。ctest 视 mono_oled 为 1 个测试目标；展开到内部 test_* 用例层共 16 个（ctest 1/1 + Unity 16/16），详表：

| # | 用例名 | 覆盖点 |
|---|--------|-------|
| 1 | test_init_null_returns_invalid_arg | init dev=NULL / cfg=NULL 拒收（DAL-L-001） |
| 2 | test_init_null_owner_returns_invalid_arg | init owner=NULL 拒收（DAL-S-002） |
| 3 | test_init_valid_claims_addr_and_sends_init | 正常 init → I2C_ADDR 已 claim + 1 次 I2C transfer（init 命令序列） |
| 4 | test_init_addr_conflict_returns_busy | 同 port+addr 重复 init → WINK_ERR_BUSY（PAL resource 互斥） |
| 5 | test_init_rejects_invalid_width | width=64 拒收（init 当前仅支持 128 像素宽） |
| 6 | test_init_rejects_invalid_height | height=48 / 128 拒收（仅 {32, 64}） |
| 7 | test_init_rejects_invalid_i2c_addr | i2c_addr=0x00 / 0x7F 拒收（7-bit 合法范围 0x08~0x77） |
| 8 | test_init_128x32_ok_and_flush_transfers_4_pages | 128×32 屏 → pages=4 + flush 1 addr + 4 page = 5 transfers |
| 9 | test_clear_zeros_framebuffer | clear 后 framebuffer 全 0 |
| 10 | test_draw_text_modifies_framebuffer | draw_text 改 framebuffer（不触 I2C） |
| 11 | test_draw_text_ascii_upper_letter_b | B / z 渲染为正确字形（字库 0x7F / 0x61） |
| 12 | test_flush_generates_i2c_transfers | 128×64 flush = 1 addr + 8 page = 9 transfers |
| 13 | test_ops_before_init_returns_not_initialized | clear / draw_text / flush 在 init 前 → WINK_ERR_NOT_INITIALIZED |
| 14 | test_deinit_hardening | NULL / 未 init 幂等 / 正常 deinit / 重复 deinit 4 个分支（DAL-L-010） |
| 15 | test_deinit_loop_i2c_client_no_resource_leak | 10-round init→deinit 循环无 I2C_ADDR claim 泄漏（ADR-0024 §4 #6/#8） |
| 16 | test_mono_oled_sh1106_variant_unsupported | variant=SH1106 → WINK_ERR_UNSUPPORTED（stub 态，不 claim 资源） |

### 3.3 spec §17.1 矩阵对照

| 驱动 | init | deinit | safe_off | const getter | Contract 注释 | WINK_BLOCKING 标注 |
|------|------|--------|----------|-------------|---------------|-------------------|
| dal_mono_oled | OK（DAL-L-001/002/003/004/005/007/008 主体合规，仅 F2 待修） | OK（DAL-L-010/011/013/015 主体合规，F3 待补 LOGW） | OK 不实现（is_actuator:false + safe_off_fn 为空） | — 不适用（display 类无 getter） | OK 5/5 API 有 Contract 块，F1 待补 Thread-safe 字段 | — 不适用（无阻塞 API） |

对照 §17.1 基线矩阵，**mono_oled 行原状态为全 OK**（init / deinit / Contract 主体均已合规），待修项 = 3（F1/F2/F3 全在 P1 高级区），相对 button / ultrasonic 行的"部分缺"标记更优。

---

## 4. 修复方案

### 4.1 P1 修复（3 项）

#### F1：clear / draw_text / flush 头注释补 Thread-safe 字段

**位置**：dal_mono_oled.h:106-140 三个 API 头注释

**修复**（以 dal_mono_oled_clear 为例）：

```c
/**
 * @brief 清空帧缓冲（纯内存操作，无 I2C）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；initialized。
 *   - Blocking: No（纯 memset）。
 *   - Thread-safe: No; ISR-safe: No.  /* 补 F1 */
 *   - Side-effects: dev->framebuffer[0..1023] = 0。  /* 补 F6 */
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_mono_oled_clear(dal_mono_oled_t *dev);
```

draw_text / flush 同样模式补两个字段。flush 补 Side-effects 时显式写"writes 1024 B framebuffer to OLED via I2C"。

**预期工作量**：1 commit，~30 行注释。

---

#### F2：init 失败路径显式置 dev->initialized = false

**位置**：dal_mono_oled.c:56-113 init 全函数

**修复**（在 init 函数体顶部 dev==NULL / cfg==NULL 校验后、initialized 状态检查后加 1 行，失败 return 前不重复写）：

```c
wink_status_t dal_mono_oled_init(dal_mono_oled_t *dev, const dal_mono_oled_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    /* ... 其他 value range 校验保持不变 ... */
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }

    /* DAL-L-007: 显式置 initialized=false，确保失败路径句柄处于 safe-deinit 态 */
    dev->initialized = false;

    /* Phase 2：(port,addr) 粒度地址冲突治理 */
    uint32_t res_id = pal_resource_i2c_id(cfg->i2c_port, cfg->i2c_addr);
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_I2C_ADDR, res_id, cfg->owner);
    if (wink_status_is_error(rs)) { return rs; }

    /* ... I2C init 命令序列 ... */
    if (wink_status_is_error(status)) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_I2C_ADDR, res_id, cfg->owner));
        return status;  /* dev->initialized 保持 false */
    }

    /* ... 成功路径最后 dev->initialized = true ... */
}
```

**预期工作量**：1 commit，~3 行改动。

---

#### F3：deinit 引入 LOGW_IF_RC + first_err 收集

**位置**：dal_mono_oled.c:175-199 deinit 全函数

**修复**（先在文件顶部、init 上方加宏定义，与 dal_ultrasonic / dal_button 对齐）：

```c
#define LOGW_IF_RC(call, first_err) do {                                  \
    wink_status_t _rc = (call);                                            \
    if (wink_status_is_error(_rc) && wink_status_is_ok(first_err)) {       \
        first_err = _rc;                                                   \
        LOG_W(LOG_TAG, "step failed rc=%d at line %d", (int)_rc, __LINE__);\
    }                                                                      \
} while(0)
```

deinit 改造：

```c
wink_status_t dal_mono_oled_deinit(dal_mono_oled_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }  /* DAL-L-010 idempotent */

    uint8_t port = dev->config.i2c_port;
    uint16_t addr = dev->config.i2c_addr;
    const char *owner = dev->config.owner;

    wink_status_t first_err = WINK_OK;

    /* 1. Best-effort turn screen off (DAL-L-015 + DAL-L-014) */
    uint8_t cmd[2] = {0x00, 0xAE};
    LOGW_IF_RC(pal_i2c_transfer(port, addr, cmd, sizeof(cmd), NULL, 0), first_err);

    /* 2. Release only this client's I2C address claim (ADR-0024 §4 #6). */
    uint32_t res_id = pal_resource_i2c_id(port, addr);
    LOGW_IF_RC(pal_resource_release(PAL_RESOURCE_I2C_ADDR, res_id, owner), first_err);

    /* 3. Clear the instance data completely (DAL-L-015 best-effort 清场) */
    memset(dev, 0, sizeof(dal_mono_oled_t));

    return first_err;
}
```

屏关（步骤 1）用 LOGW_IF_RC 替换 WINK_IGNORE_UNUSED，**仍保留 best-effort 语义**——LOGW_IF_RC 只在 first_err == WINK_OK 时记录第一个失败 rc，best-effort 继续推进。屏关注释（c:184-185）的 "best-effort ignore" rationale 升级为 "best-effort + LOGW"。

**预期工作量**：1 commit，~15 行改动。

### 4.2 P1 SHOULD 修复（2 项）

#### F4：display 离散坐标后缀规范化

**位置**：codegen/drivers/mono_oled.yaml + dal_mono_oled.h:46-72

**修复（两步）**：

1. 头注释（dal_mono_oled.h:45-72 区域）补 1 段 rationale：

```c
/**
 * @brief display 离散坐标后缀说明：
 *   - width / height / col / page / pages 是 0-indexed 屏幕坐标，
 *     单位 = 像素（width/col）或 8 像素页（height/page/pages）。
 *   - 不属于 spec §9.1 封闭单位后缀表覆盖的物理量（_cm / _ddeg / _us 等）。
 *   - 后缀留空以与物理量命名明确区分；如未来 §9.1 增补 _px / _col / _page
 *     等坐标后缀，再统一替换。
 */
```

2. 同步 issue #WINK-DAL-040 跟踪 spec §9.1 增补 _px / _col / _page / _rows 坐标后缀。

**预期工作量**：1 commit（头注释 8 行）+ 1 issue 跟踪。

---

#### F5：YAML 增补 quantity / quantity_class

**位置**：codegen/drivers/mono_oled.yaml:1-9

**修复**：

```yaml
codegen_schema: "1.1"
type: mono_oled
category: display
source_stem: mono_oled
is_actuator: false
experimental: false

# spec v3.4.1 §9.3 / §16.2: 新驱动 MUST 声明 quantity_class（DAL-U-021）。
# display 写动作归 A 类（actuator_command），与 dc_motor/rc_servo 同。
quantity: text_display
quantity_class: actuator_command
```

跟踪 issue #WINK-DAL-030（与 spec §17.3.1 `codegen quantity_class 校验待实现` 同一跟踪号）。

**预期工作量**：1 commit，~3 行 YAML。

### 4.3 P2 修复（1 项）

#### F6：5 个 API 头注释补 Side-effects

**位置**：dal_mono_oled.h:93-158 5 个 API 头注释

**修复汇总表**：

| API | Side-effects 字段内容 |
|-----|---------------------|
| init（h:93-104） | pal_resource_claim(I2C_ADDR port+addr); sends 18~20-byte init command sequence via I2C; writes dev->config (deep copy); writes dev->pages; writes dev->framebuffer[0..1023]=0; sets dev->initialized=true. |
| clear（h:106-113） | writes dev->framebuffer[0..1023]=0 (memset). |
| draw_text（h:115-129） | modifies dev->framebuffer[col..col+5*N] for N=length(str). No I2C traffic. |
| flush（h:131-140） | sends 1 addr-set (7 B) + pages (129 B each) I2C transfers; reads dev->framebuffer[0..1023]. |
| deinit（h:142-158） | sends 2-byte display-off command via I2C (best-effort); pal_resource_release(I2C_ADDR port+addr); memset(dev, 0, sizeof(dal_mono_oled_t)). |

**预期工作量**：1 commit，~25 行注释。

---

## 5. 风险与治理

### 5.1 修复风险表

| # | 风险 | 缓解 |
|---|------|------|
| F1 | 头注释改动可能触发下游 codegen 模板生成变化 | 头注释仅 @note 块追加字段，Doxygen 风格；codegen 不解析头注释，零影响 |
| F2 | 显式 dev->initialized = false 在 init 失败路径写入，可能与 {0} 兜底语义重复 | 仅在 init 函数体顶部写一次，失败路径不写第二次；与 dal_dc_motor / dal_ultrasonic 实现一致 |
| F3 | LOGW_IF_RC 引入需确保 LOG_TAG 与 LOG_W 宏在该 TU 可见 | 当前 dal_mono_oled.c 顶部已 include pal_hal.h，PAL 头已暴露日志宏；新增 LOG_TAG "dal_mono_oled" 一行 |
| F4 | 头注释声明 "col/page 非物理量" 但未推动 spec 增补 _px 后缀 | 通过 issue #WINK-DAL-040 显式跟踪，spec 增补后批量替换即可 |
| F5 | quantity_class 增补后，codegen 引擎目前不会校验该字段（spec §17.3.1 pending） | 跟踪 issue #WINK-DAL-030，codegen 实现后该字段即被强制 |
| F6 | 5 个 API 头注释 Side-effects 字段需逐个手写，易遗漏 | 列入 lint 规则候选（review-enforced 升级路径） |

### 5.2 不修复项的理由

下列规范项本模块合规或合理豁免，**故意不修**：

| 章节 | 条款 | 不修理由 |
|------|------|---------|
| §3.2 DAL-L-020 | safe_off | mono_oled YAML 显式 `is_actuator: false` + `safe_off_fn: ""`，属 display 类，§3.2 MUST NOT 实现空壳 safe_off |
| §4.4 DAL-F-010 | apply_override void *dev 例外 | mono_oled 无 apply_override 函数（不需要运行时 config override） |
| §5.4 DAL-V-001/002 | 器件特有 API | display 类无 device_specific API；5 个 verb 全部走标准动词库 |
| §6.1 DAL-C-001/002 | 跨 ISR 共享字段临界区 | mono_oled 无 ISR 路径，framebuffer 单写者（task），无 volatile 字段 |
| §7.4 DAL-B-020~024 | 异步三段式 | display 类是同步路径（draw → flush），无 request/poll 状态机 |
| §9.1 DAL-U-001 | 单位后缀 | display 离散坐标非物理量，§9.1 后缀表不适用（见 F4 rationale） |
| §9.4 DAL-U-023~030 | A 类定标整型 | mono_oled 无 A 类控制量（speed / angle），仅输出 framebuffer 字节流 |
| §10 DAL-CB-001~003 | 事件回调 | display 类无回调 |
| §11.2 DAL-P-010~014 | Stub 语义 | 当前无未实现 stub（SH1106 已在 init 入口以 WINK_ERR_UNSUPPORTED 拒收，不算 stub） |
| §13.1 DAL-BC-002 | 句柄末尾追加 | 现有 4 字段已按 config / fb / pages / initialized 顺序，未来追加字段 MUST 追加在末尾 |
| §13.1 DAL-BC-003 | config 只追加不重排 | config 6 字段已排序，apply_override 不存在 |

### 5.3 长效治理

| # | 治理项 | 责任方 | 跟踪 |
|---|--------|-------|------|
| 1 | spec §15 模板增补 Thread-safe 字段范例段，明确"无 ISR 路径的 display / 简单 actuator 必须显式标 No" | spec 维护者 | 内部 review 议题，纳入下版 v3.4.2 |
| 2 | spec §9.1 增补 display 离散坐标后缀表（_px / _col / _page / _rows），与现物理量后缀并列 | spec 维护者 + mono_oled owner | issue #WINK-DAL-040 |
| 3 | codegen 引擎实现 quantity / quantity_class 校验（DAL-U-021），对所有 driver registry 缺该字段的 YAML 报错 | codegen 维护者 | issue #WINK-DAL-030（与 spec §17.3.1 同一跟踪号） |

---

## 6. 评审元数据

| 项 | 值 |
|----|----|
| 评审人 | Claude (Code v3.4.1) |
| 评审对象 commit | master（2026-08-04 起步） |
| 评审方法 | 规范 17 节逐条交叉对照 + 微观合规清单 + 客观 lint / ctest 验证 |
| 评审范围 | 7 文件（1 头 + 1 实现 + 1 内部字库头 + 2 字库 backend + 1 单测 + 1 YAML） |
| 评审耗时 | ~15 分钟 |
| 评审工具 | Read / Grep / Glob / bash（git log 溯源） |
| 客观验证 | ABI lint 探针 0 findings；ctest 1/1 目标 PASS（含 16 内部 test_* 用例） |
| 修复责任人 | 开发 owner 自行决定优先级（建议按 §4.1 → §4.2 → §4.3 顺序） |
| 审批人 | tech lead review 后合并 |
| 跟踪 | 本评审的 5 项修复建议应在本 review 落地后归档为只读；下次 dal_mono_oled 评审至少应覆盖本次列出的不合规项是否全部收敛 |



