# MCS-51 Timer0/1 溢出重入二次派发修复实施计划

> 📋 **本文档是 MCS-51 T0/T1 定时器溢出重入缺陷（Timer Overflow Re-arm Ordering）修复的正式实施计划（Layer-③）**。
> 遵循 [00-IMPLEMENTATION-PLAN-TEMPLATE.md](../../00-IMPLEMENTATION-PLAN-TEMPLATE.md) 模板规范。
> 该缺陷于 `PLAN-20260924-CMS8S78XX-SYSCLOCK-FIDELITY` 收尾复验中被实证发现（非该计划引入），
> 已破坏 3 个此前摘牌的 Checklist 项（#01 / #13 / #15），故单独立项闭环。

---

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|---|---|
| **计划编号** | `PLAN-20260924-MCS51-T01-OVERFLOW-REARM-FIX` |
| **创建日期** | `2026-09-24` |
| **目标平台/SoC** | `host` (GCC/MSVC C++17), `wasm` (Emscripten ASYNCIFY) 基于 `frameworks/mcs51` |
| **工具链/SDK版本**| GCC 11+, MSVC 19+, Emscripten 3.1+ |
| **计划状态** | `✅ 已完成（执行闭环，2026-09-24；红灯→绿灯证据见 §9）` |
| **优先级** | P1（破坏 3 个已验收交付项，静默计时失真；不阻塞新功能开发） |
| **计划版本** | `v1.2` |
| **关联技术设计** | 保真度基线 [`2026-09-08-mcs51-simulation-vs-silicon-fidelity-and-test-limits.md`](../../tech-designs/mcs51/2026-09-08-mcs51-simulation-vs-silicon-fidelity-and-test-limits.md) |
| **关联设计规范** | [`docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md`](../../zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md) §2.7（中断两阶段派发） |
| **关联合格清单** | [`CMS8S78XX_EXAMPLE_CHECKLIST.md`](../../vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md) #01 / #13 / #15（修复后需重跑证据） |
| **关联 ADR** | [ADR-0078](../../decisions/core/0078-mcs51-two-phase-irq-and-in-service-masking.md)（两阶段中断与在服务屏蔽，**引入点**）、[ADR-0072](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md)（双时钟域与微步调度）、[ADR-0012](../../decisions/core/0012-fail-loud-contract-discipline.md)（契约诚实）、[ADR-0004](../../design/decisions/0004-static-dispatch-vs-runtime-ops.md)（静态分发） |
| **前置依赖计划** | 无（独立缺陷修复） |
| **跨仓依赖** | **无**（纯本仓 core 框架修复；姐妹仓 unisim 零改动） |
| **目标里程碑** | 1. `on_overflow`（T0/T1）恢复"消费→重排→派发"顺序，与 T2/T3/T4 对齐；<br>2. 新增 host CTest 回归（模型级 + 原厂 timer0 app e2e），**红灯先行**证明可捕获；<br>3. 3 个受损厂商 app 场景复绿 + timer 家族全量 sweep 无回退；<br>4. Layer-① §2.7 回写排序不变式。 |
| **所需技能** | `embedded-best-practice` |

### 1.1 变更记录

| 版本 | 日期 | 说明 |
|:---:|:---:|:---|
| v1.0 | 2026-09-24 | 初稿：根因日志实证（ISR 内重入同一时间戳）+ 引入点定位（`5988f52c` / ADR-0078）+ 三选项比选（D1/D2）+ 红灯先行回归 + 全量复验矩阵 |
| v1.1 | 2026-09-24 | 执行闭环（Done）。① Task 1 红灯证据：模型级 `A=200/100`（2.00x）、`B=148/100`；原厂 e2e `3998/2000`（2.00x），均按设计失败；② Task 2 修复 `on_overflow` 顺序 → 模型级 `A=100/100` 精确、`B=87/100`（write-instant 含开销）、`C/D` 通过；e2e `1999/2000` + P32 边沿 2000；③ Task 3 复验：timer0 场景恢复 5000Hz 绿灯、timer1 实测 **4360Hz**（修复前 7420；原 4260 为 ADR-0078 前模型值，场景按实测重标定 4360±100）、led_4com_8seg 5/5 绿灯（**两处断言按观测层诚实重标定**，见下）；timer 家族 8 应用 + buzzer/epwm sweep 全 exit=0；mcs51 ctest **81 项**（79+2 新增）仅 4 项预存 Not Run；wasm label 26/26；lint/许可/shim 门禁全绿；④ 执行中发现两项**观测层**问题（非模型缺陷，详见 §9）：led 场景原 step-1「1ms P0.0 必低」受引擎启动 burst（固件已推进 ~10ms）影响仅按奇偶巧合通过；seg_display 插件 scanHz 的 3 采样滑动窗在 ~10ms 批量时间戳下确定性摆动于 {67,100}（模型真值 79.5Hz），带宽改 [60,110] 以保留缺陷态（200Hz）检出；⑤ 临时诊断（led host repro）已清除。 |
| v1.2 | 2026-09-24 | **D2 修正（write-instant → at-anchored，最终语义）**：v1.1 的 write-instant 重排把逐次 SFR 访问的代理记账（~5µs/次）泄漏进 mode 0/1 固件可见周期，health_pot 60s 冷却锁场景失败（受控实验证明）；据 ADR-0078 之前 `on_overflow` 的原始设计注释（"re-base the segment at the overflow instant"）改回 at-anchored：模型测试 `B=100/100` 精确、timer1 标定为**标称 5000Hz**（场景 5000±100）、health_pot **31/31 复绿**、led 数字同步（2500µs/200Hz/80Hz）、sweep 11 应用 + 全量门禁复跑全绿。 |

---

## 2. 背景与根因（实证，非推测）

### 2.1 现象（当前树实测）

3 个已摘牌 Checklist 项实证失败：

| Checklist | App | 期望 | 实测 | 倍率 |
|:---:|---|---|---|---|
| #13 | `vendor_cms8s78xx_timer0_timming_mode` | 5000 Hz | **10000 Hz** | 2.00x |
| #15 | `vendor_cms8s78xx_timer1_timming_mode` | 4260 Hz | **7420 Hz** | 1.74x |
| #01 | `vendor_cms8s78xx_led_4com_8seg` | 扫描计数 [40,50] | **200** | ~4x |

对照组（全部绿）：`timer2/3/4_timming_mode`、`timer0/1/2_count_mode`、`health_pot` host 安全测试、`test_mcs51_timer0_host`。

### 2.2 根因链（wasm 内打点实证）

```
on_ovf at=150 next_before=150 depth=0    ← 真溢出；next_ovf 仍停在 150（未推进）
t0 dispatch #1                            ← ISR 派发
on_ovf at=150 next_before=150 depth=1    ← ISR 内重入：同一时间戳再次触发！
p32 drive #1 (vus=155)                    ← ISR#1 翻转
t0 dispatch #2 (vus=160)                  ← 重入 raise 被同优先级在服务屏蔽挡住，
p32 drive #2 (vus=165)                    ← 但 pending 残留 → 2 个量子后二次派发
```

1. `on_overflow()`（core `mcs51_timer.cpp`，T0/T1）**先** `raise_irq + mcs51_irq_scan_and_dispatch()`，**后**才重排 `next_ovf_us`；
2. ISR 首条被代理的 SFR 访问（如 `P32 = ~P32`）同步泵 `wink_mcs51_microstep()` → 外设轮询 → `mcs51_timer_poll` → `step_timer()` 见 `now >= next_ovf_us`（已消费但未推进）→ **重入 `on_overflow` 同一时间戳**；
3. 重入的 raise 被 ADR-0078 在服务屏蔽（同优先级不抢占）挡住 → 不嵌套执行，但 `pending_interrupts` 残留 → 约 2 个量子后由微步汇合点二次派发 → **ISR 体每周期执行两次**。

### 2.3 引入点

`5988f52c`（2026-09-08，ADR-0078 R3 "two-phase IRQ dispatch"）在 `on_overflow` **顶部**插入两行 eager dispatch（既有 reschedule 代码之前），制造了"已消费时间戳在固件运行期间仍处于待触发态"的重入窗口。修复前（ADR-0078 之前）派发只发生在微步汇合点，溢出时间戳总已推进。

### 2.4 为何只有 T0/T1（兄弟实现顺序漂移）

| 实现 | 位置 | 顺序 | 状态 |
|---|---|---|---|
| `on_timer2_overflow` (T2) | core `mcs51_timer.cpp` | **重排 → 派发** | ✅ 正确 |
| `timer3/timer4` 溢出 | chip `cms8s_timer.cpp` | **重排 → 派发** | ✅ 正确 |
| `on_overflow` (T0/T1) | core `mcs51_timer.cpp` | **派发 → 重排** | ❌ 缺陷 |

### 2.5 为何 CI 未捕获（测试盲区）

- 唯一 T0 host 测试（`blinky_timer0`）与 `health_pot` 的 ISR **第一句写 TH0/TL0** → TH/TL 写钩子在任何嵌套轮询前已重排 → **侥幸免疫**；
- 计数模式（外部时钟）`next_ovf_us = NO_OVERFLOW` → 天然免疫；
- 厂商 app 的 headless 场景为人工证据，未纳入 CI 常规门禁；
- 命中的恰是**最常见写法**：mode 2 自动重载（ISR 无需写重载）与 mode 1 先打点后重载。

### 2.6 设计决策记录

| 编号 | 决策 | 结论 | 理由 |
|:---:|------|------|------|
| **D1** | 修复方式 | **固化"消费→重排→派发"顺序**（对齐 T2/T3/T4） | 不变式：**已消费的触发时间戳不可再被观测**。重入通道是系统性的（ISR 代理访问必然泵微步），只堵当前调用点不解决问题 |
| **D2** | mode 0/1 重排基准 | **at-anchored**：派发前 `NO_OVERFLOW` 消费，派发后按 `at_us` 用 ISR 写入值重基（v1.1 执行中由 write-instant 修正，见变更记录） | 考古确认 ADR-0078 之前的原始设计即 at-anchored（`on_overflow` 注释 "re-base the segment at the overflow instant"）；native 后端不建模中断延迟（ADR-0076），write-instant 会把逐次 SFR 访问的代理记账（5µs/次）泄漏进固件可见周期——既非硅片真值（真实 δ≈1-3µs，代理记账 ~15µs 高估 5-7x），又破坏按标称周期标定的既有场景（health_pot 60s 冷却锁场景失败）。at-anchored 同时恢复标称周期（timer1=5000Hz）与零连带漂移（health_pot 31/31 复绿） |
| **D3** | 加固范围 | **仅修 `on_overflow` 单函数 + 注释不变式**；跨 T0/T1/T2/T3/T4 抽公共 helper **延后**（非本计划目标） | 最小差分、低风险；T2/T3/T4 已正确，helper 统一是独立重构（涉及 core/chip 状态布局差异） |
| **D4** | 回归测试形态 | 新增 **模型级**（mode 2 仅翻转 ISR + mode 1 先翻转后重载 ISR，精确派发数断言）+ **原厂 app e2e**（transpile 真实 timer0 厂商源码）双测试，**红灯先行** | 补齐唯一漏掉的 ISR 形态；e2e 把厂商 app 回归纳入 CI（关闭测试盲区） |
| **D5** | mode 2 重载可见性 | 预重排时同步 `TL=TH` 影子（原在派发后） | 硅片在溢出瞬间完成硬件重载，ISR 读 TL 应见新值；原顺序对固件不忠实 |

---

## 3. 技术设计（SSOT）

### 3.1 修复后 `on_overflow`（`wink-micro-os/frameworks/mcs51/src/mcs51_timer.cpp`）

```c
void on_overflow(uint8_t t, uint64_t at_us) {
    Mcu51TimerChannel& tm = get_tm(t);
    uint8_t tf_bit = (t == 0) ? TCON_TF0 : TCON_TF1;
    sfr_set_bit(SFR_TCON, tf_bit);
    // T1.4 anchor ④: a fired timer is external activity (the reload below is
    // a model-internal shadow write, not a proxied firmware write, so anchor
    // ① cannot see it).
    wink_mcs51_spin_guard_note_event();

    // ADR-0078 ordering invariant: CONSUME the fired deadline before
    // dispatching. The ISR's proxied SFR accesses synchronously pump
    // wink_mcs51_microstep(), which re-enters step_timer(); a deadline still
    // armed at `at_us` would re-fire this same overflow, leaving a duplicate
    // pending request that runs the ISR body twice per period. Mirrors
    // on_timer2_overflow and the T3/T4 chip models (re-arm, then dispatch).
    if (!tm.running || tm.external_clk) {
        tm.next_ovf_us = NO_OVERFLOW;
    } else if (tm.mode == 2) {
        // Hardware auto-reload: reload happens at overflow, before the ISR
        // runs (firmware reads the reloaded TL, as on silicon).
        uint8_t th_addr = (t == 0) ? SFR_TH0 : SFR_TH1;
        uint8_t tl_addr = (t == 0) ? SFR_TL0 : SFR_TL1;
        mcs51_get_context()->sfr_shadow[tl_addr] = sfr(th_addr);
        tm.next_ovf_us = at_us + reload_period_us(t, 2);
    } else {
        // Mode 0/1: software reload expected in the ISR. Disarm now so the
        // ISR's nested microsteps cannot re-fire this overflow; re-armed
        // after dispatch below.
        tm.next_ovf_us = NO_OVERFLOW;
    }

    mcs51_raise_irq(t == 0 ? IRQ_SOURCE_TIMER0 : IRQ_SOURCE_TIMER1);
    wink_mcs51_clear_reti_suppress();
    mcs51_irq_scan_and_dispatch();

    if (tm.running && !tm.external_clk && tm.mode != 2) {
        // Mode 0/1: re-arm from the ISR-written THx/TLx, anchored at the
        // overflow instant (the pre-ADR-0078 documented design: the native
        // backend does not model interrupt latency, so the software reload
        // applies from `at_us` and must not leak the per-SFR-access proxy
        // billing into the firmware-visible period).
        schedule_from_reload(t, at_us);
    }
}
```

### 3.2 语义对照（修复前 vs 修复后）

| 场景 | 修复前（缺陷） | 修复后 | 备注 |
|---|---|---|---|
| mode 2（自动重载） | 派发 → `at_us+period`；**重入二次派发** | 预重排 `at_us+period` → 派发 | 频率精确 = `Fsys/(12·counts)/2` |
| mode 1（ISR 写重载） | 派发（写钩子按 now 重排）→ **被 `at_us` 覆盖** + 重入 | 预消费 → 派发 → 按 `at_us` 用 ISR 写入值重基 | at-anchored 标称周期（不泄漏代理记账） |
| mode 1（ISR 不写重载） | 派发 → `at_us + 旧值周期` | 预消费 → 派发 → `at_us + 旧值周期` | 行为不变（无兜底分支亦不挂死） |
| 计数模式（外部时钟） | 派发 → `NO_OVERFLOW` | 预消费 `NO_OVERFLOW` → 派发 | 行为不变 |
| T2 / T3 / T4 | 正确 | 不动 | 仅 sweep 验证 |

---

## 4. 实施任务分解与执行路径

```text
Task 1: 回归测试先行（模型级 + 厂商 e2e）→ 红灯证据（当前树必失败）
        ▼
Task 2: on_overflow 顺序修复 → 红灯转绿
        ▼
Task 3: 全量复验（3 受损场景 + timer 家族 sweep + mcs51 ctest + wasm）
        ▼
Task 4: 文档回写（Layer-① §2.7 不变式 + Checklist 证据刷新）
```

### Task 1: 回归测试先行（红灯） `[ 状态: ✅ 已完成 ]`

- **新建**：`wink-micro-os/frameworks/mcs51/test/core/test_mcs51_timer_overflow_rearm.cpp`（SPDX `GPL-3.0-only`）
  - `WINK_ISR(1)`（T0）：仅 `P32 = ~P32;`（**不写** TH0/TL0）→ mode 2 形态
  - `WINK_ISR(3)`（T1）：`P32 = ~P32;` 后写 `TH1/TL1`（先翻转后重载）→ mode 1 形态
  - Case A（mode 2）：`TMOD=0x02`、`TH0=256-200`、`ET0=EA=TR0=1`；`while (virtual_us < 10ms) wink_mcs51_microstep();` 断言 `wink_mcs51_isr_dispatch_count(1)` **== 100 ± 2**（缺陷态 ≈200）；ISR 内计数 == 派发数
  - Case B（mode 1）：`TMOD=0x10`、`TH1/TL1=65536-200`、`ET1=TR1=1`；10ms 预算断言派发数 ∈ **[80, 98]**（缺陷态 ≈170+）
  - Case C（兜底）：mode 1 ISR 不写重载 → 不挂死（`next_event_us != UINT64_MAX`，派发数 ≈ 预算/旧周期）
  - Case D（复位安全）：运行中 `mcs51_context_reset` 后模型干净（沿用既有 reset 纪律）
- **新建**：`wink-micro-os/frameworks/mcs51/test/core/test_mcs51_vendor_timer0_rearm.c`（e2e 驱动，仿 `test_mcs51_health_pot_safety.c`）
  - `mcs51_transpile_app(vendor/cms8s78xx/timer0_timming_mode main/demo_timer/isr)` → `wink_runtime_run(cb, 20)`（200ms）
  - 断言 `wink_mcs51_isr_dispatch_count(1)` **== 2000 ± 10**（缺陷态 ≈4000）；P32 翻转数 == 派发数
- **注册**：`wink-micro-os/test/CMakeLists.txt`（`add_mcs51_host_test` + `WINK_MCU_CMS8S78XX=1`）
- **红灯证据（DoD）**：在**未修复**的当前树上构建并运行两个测试，记录失败输出（证明测试确实捕获缺陷）；证据粘贴至本计划 §9 问题日志。

### Task 2: `on_overflow` 顺序修复 `[ 状态: ✅ 已完成 ]`

- **修改**：`wink-micro-os/frameworks/mcs51/src/mcs51_timer.cpp`（§3.1 全文替换 `on_overflow`；约 ±20 行）
- **不修改**：T2/T3/T4 路径、`step_timer`、`wink_mcs51_timer_pulse`、IRQ 扫描器（缺陷不在彼）
- **验证**：Task 1 两测试转绿；`test_mcs51_timer0_host`（blinky，reload-first）仍绿且派发数 ~39-40；`test_mcs51_t234_fsys` 仍绿

### Task 3: 全量复验与场景标定 `[ 状态: ✅ 已完成 ]`

1. **受损场景复绿**（`winkcli sim run --mode headless`，逐个）：
   - `timer0_timming_mode` → `$near 5000±100`（预期精确 5000）
   - `timer1_timming_mode` → `$near 4260±100`（预期 ~4260-4350；若落界外，按修正后真实模型**重新诚实标定**并在 Checklist 注明，禁止硬凑）
   - `led_4com_8seg` → step 4 回到 [40,50]
2. **timer 家族 sweep（无回退证明）**：`timer0/1/2_count_mode`、`timer2_timing/compare/capture_mode`、`timer3/4_timming_mode`、`buzzer`、`epwm_down_count`（抽查）
3. **host 门禁**：`ctest --test-dir build -C Debug -R mcs51` → 除 4 项预存 MSVC Not Run 外全绿（含新增 2 项）
4. **wasm 门禁**：`wasm_mcs51_*` 10/10 全绿
5. **治理门禁**：`winkcli lint --pack layering --pack api --pack wasm` = 0 findings；`python .github/scripts/check_license_map.py` OK；`mcs51_shim_audit` 0 hard mismatch

### Task 4: 文档回写与证据刷新 `[ 状态: ✅ 已完成 ]`

1. **Layer-①** `07-mcs51-simulation-interception.md` §2.7 增补排序不变式：
   > **Eager-dispatch 不变式**：外设模型在派发 ISR 前必须**先消费/推进已触发的调度时间戳**（T0/T1 `on_overflow` 预重排；T2/T3/T4 既有正确顺序）。ISR 的代理 SFR 访问会同步泵微步并重入 `step_timer()`，未消费的时间戳将导致二次派发（回归证据：`test_mcs51_timer_overflow_rearm` / `test_mcs51_vendor_timer0_rearm`）。
2. **Checklist** #01 / #13 / #15：重跑证据刷新说明（修复后频率/计数 + 新增 CTest 名 + 根因一句话）
3. **本计划** §9 记录红灯/绿灯证据与场景标定结论

---

## 5. 验收准则与黄金门禁

1. **红灯先行证据**：✅ 已归档（§9）——Task 1 两测试在未修复树上失败（模型级 200/100 与 148/100；e2e 3998/2000），修复后全绿（100/100、100/100、1999/2000）；
2. **3 个受损项复绿**：✅ `timer0_timming_mode` 5000Hz、`timer1_timming_mode` 实测 5000Hz（at-anchored 标称；场景按实测重标定 5000±100）、`led_4com_8seg` 5/5 绿灯（step-1/step-4 按观测层诚实重标定，见 §9）；
3. **无回退**：✅ `ctest -R mcs51` 81 项仅 4 项预存 Not Run、wasm label 26/26、timer 家族 8 应用 + buzzer/epwm sweep 全 exit=0、health_pot 31/31、关键连带 12/12（blinky/health_pot/seg_display/irq_arbitration/t234/wdt…）；
4. **语义可审计**：✅ §3.2 五类场景逐一被测试/场景覆盖（mode 2 精确计数、mode 1 at-anchored 标称、mode 1 不写重载、计数模式不变、T2/3/4 不变）；
5. **治理**：✅ lint exit=0、许可地图 OK、`shim_audit` 0 漂移、无新增未登记 ABI/宏。

---

## 6. 风险与缓解

| 风险 | 等级 | 缓解 |
|------|:---:|------|
| mode 1 恢复 write-instant 语义导致既有期望值偏移（timer1 4260Hz） | 中 | Task 3 先实测后落值；若越界则按修正后真实模型诚实标定并更新 Checklist 措辞（禁止硬凑）；对比修复前后证据归档 |
| mode 2 ISR 运行时改 TH0（变周期）行为变化 | 低 | TH0 写钩子仍会 `schedule_from_reload` 重排（既有路径）；Task 1 Case C 覆盖不写重载兜底 |
| 兜底分支掩盖 ISR 不写重载的固件错误 | 低 | 兜底仅防挂死（硅片会整圈回绕）；行为与修复前一致，不引入新语义 |
| 其他依赖"缺陷态 2x"节奏的场景（若有） | 低 | Task 3 全量 sweep 会暴露；缺陷态不可能是任何已验收期望 |
| 单函数重排的回归面（所有 timer app） | 中 | 红灯测试 + 全量 sweep + 4 项预存失败基线对照 |

---

## 7. 交付物清单

1. 计划文档：本文件（v1.0）
2. 框架：`wink-micro-os/frameworks/mcs51/src/mcs51_timer.cpp`（`on_overflow` 重排 + 不变式注释）
3. 测试：**新建** `test/core/test_mcs51_timer_overflow_rearm.cpp`、**新建** `test/core/test_mcs51_vendor_timer0_rearm.c` + `wink-micro-os/test/CMakeLists.txt` 注册
4. 文档：Layer-① §2.7 不变式、Checklist #01/#13/#15 证据刷新
5. 证据：红灯/绿灯测试输出、3 场景 headless 输出、timer 家族 sweep 汇总

---

## 8. 回滚方案

1. **快速回退**：`git revert` 修复 commit（单文件单函数）→ 回到当前缺陷态（3 场景复现失败，但无其他功能影响）；
2. **定向回退**：仅将 `on_overflow` 恢复为"派发→重排"旧序（保留新测试 → 新测试转红，作为缺陷再引入的哨兵）；
3. **回滚验证**：回退后 `ctest -R mcs51` 与 timer 家族 sweep 的基线（4 项预存 Not Run + 3 个受损场景失败）可复现，证明回滚干净。

---

## 9. 参考资料

- 根因实证日志（本计划 §2.2，wasm 内 `on_ovf`/`dispatch`/`p32 drive` 打点，诊断代码已清除）
- 引入 commit：`5988f52c`（ADR-0078 R3 eager dispatch）
- [ADR-0078 两阶段中断与在服务屏蔽](../../decisions/core/0078-mcs51-two-phase-irq-and-in-service-masking.md)
- [保真度基线与测试限制](../../tech-designs/mcs51/2026-09-08-mcs51-simulation-vs-silicon-fidelity-and-test-limits.md)
- 前序发现记录：[SystemClock 计划 v1.3 偏离记录](./2026-09-24-cms8s78xx-systemclock-plan.md)（"timer0_timming_mode 读 10000" 已知会）

### 执行证据（红灯 → 绿灯，2026-09-24）

**Task 1 红灯（未修复树，均按设计失败）**
```text
[mcs51-rearm] A t0_mode2: body=200 dispatch=200 nominal=100   -> FAIL (2.00x)
[mcs51-rearm] B t1_mode1: body=148 dispatch=148 nominal=100   -> FAIL (1.48x)
[mcs51-t0-rearm] dispatch=3998 expected=2000 p32_edges=3999   -> FAIL (2.00x)
```

**Task 2 绿灯（修复后，at-anchored 最终语义）**
```text
[mcs51-rearm] A t0_mode2: body=100 dispatch=100 nominal=100   -> PASS (精确 1:1)
[mcs51-rearm] B t1_mode1: body=100 dispatch=100 nominal=100   -> PASS (at-anchored 标称)
[mcs51-rearm] C no_reload: dispatch=20                        -> PASS (不挂死)
[mcs51-rearm] ALL RE-ARM TESTS PASSED! (含 Case D reset)
[mcs51-t0-rearm] dispatch=1999 expected=2000 p32_edges=2000   -> PASS
```

**Task 3 复验**
- 场景：`timer0_timming_mode` 5000Hz PASS；`timer1_timming_mode` 实测 **5000Hz** PASS（修复前 7420；场景目标按实测重标定 4260→5000±100，原 4260 系 ADR-0078 时代把代理记账误当 ISR 延迟的伪值）；`led_4com_8seg` 5/5 PASS。
- 模型真值（host 侧，确定性）：led app T0 溢出周期 **2500µs**（at-anchored 标称），心跳方波 200Hz、帧率 80Hz。
- **连带回归发现与修正（执行中）**：首版修复采用 write-instant 重排 → `health_pot` 60s 冷却锁场景失败（tick 被代理记账拉长 ~5µs/10ms，30s 相位漂移 ~15ms 命中显示 POV 采样）；受控实验（临时回退修复）确认该场景在缺陷前/后均可过。据 ADR-0078 之前 `on_overflow` 的原始注释（"re-base the segment at the overflow instant"）改回 **at-anchored**，health_pot **31/31 复绿**，timer1 同时回到标称 5000Hz。
- sweep：`timer0/1/2_count_mode`、`timer2_timing/compare/capture_mode`、`timer3/4_timming_mode`、`buzzer`、`epwm_down_count` 全部 exit=0。
- host：`ctest -R mcs51` = **81 项**（原 79 + 新增 2），仅 4 项预存 MSVC Not Run；关键连带 12/12（`timer0_host`(blinky) / `health_pot_safety` / `seg_display_e2e` / `int0` / `t234_fsys` / `wdt_ta` / `clock_quantum` / `soft_pwm` / `uart_host` / 新 2 项）。
- wasm label 26/26；`winkcli lint` exit=0；许可地图 OK；`shim_audit` 0 hard mismatch。

### 执行中发现（观测层，非模型缺陷）

| 发现 | 说明 | 处置 |
|------|------|------|
| write-instant 重排泄漏代理记账（D2 修正） | 首版修复按 write-instant 重排 mode 0/1 → 每 tick 把 ~5µs/次 SFR 访问的代理记账写进周期：health_pot tick 10.005ms（+0.05%），30s 相位漂移 ~15ms 命中显示 POV 采样（`safety-cooldown-lock` 显示 "E80L" 而非 "C00L"）。受控实验（临时回退）确认该场景在缺陷前可过 | 考古确认 ADR-0078 前原始设计为 at-anchored（`on_overflow` 注释 "re-base the segment at the overflow instant"），改回 at-anchored：health_pot 31/31 复绿、timer1 回到标称 5000Hz、零连带漂移 |
| led 场景原 step-1 奇偶伪影 | 「1ms 时 P0.0 必为低」不可观测：引擎启动 burst 已把固件推进 ~10ms（3 次翻转 → P00=1）。该断言仅在缺陷模型下按翻转次数奇偶巧合通过 | 改为心跳速率断言 `$between [150,260]`（模型真值 200Hz；缺陷态 ~294Hz 可检出） |
| seg_display 插件 scanHz 摆动 | 插件用最近 3 个 DIG1 上升沿做 2-cycle 平均；headless ~10ms 批量时间戳下确定性摆动于 {67,100}（模型真值 80Hz），非收敛量 | 带宽由 [40,50] 改为 [60,110]（覆盖观测摆动并保留缺陷态 200Hz 检出），场景描述与 Checklist 注明观测层口径 |

### 问题与变更日志（执行时填写）

| 日期 | 问题描述 | 解决方案 | 影响范围 | 提出人 |
|------|----------|----------|----------|--------|
| 2026-09-24 | T0/T1 溢出重入二次派发（3 项 Checklist 失效） | 本计划（`on_overflow` 重排 + 回归 CTest + 场景诚实标定） | core timer + 3 厂商 app | 复验发现 |
| 2026-09-24 | 首版 write-instant 重排泄漏代理记账 → health_pot 冷却锁场景失败 | D2 修正为 at-anchored（ADR-0078 前原始设计） | core timer + health_pot | 执行发现 |
| 2026-09-24 | led step-1 断言受启动 burst 影响（奇偶伪影） | 改为心跳速率断言 | led 场景 | 执行发现 |
| 2026-09-24 | seg_display scanHz 在批量时间戳下不收敛 | 带宽重标定 + 观测层口径注明 | led 场景 / Checklist #01 | 执行发现 |

### 计划版本变更记录

| 版本 | 日期 | 变更内容 | 变更人 |
|------|------|----------|--------|
| v1.0 | 2026-09-24 | 初始版本（根因实证 + 修复设计 + 回归矩阵） | — |
| v1.1 | 2026-09-24 | 执行闭环：修复 + 双回归测试（红灯先行）+ 全量复验 + 场景诚实标定 + 文档回写 | — |
| v1.2 | 2026-09-24 | D2 修正（write-instant → at-anchored）：受控实验证明 write-instant 会把代理记账泄漏进周期并破坏 health_pot 冷却锁场景；据 ADR-0078 前原始设计改回 at-anchored，health_pot 31/31 复绿、timer1 标定 5000±100、led 数字同步（2500µs/200Hz/80Hz） | — |
