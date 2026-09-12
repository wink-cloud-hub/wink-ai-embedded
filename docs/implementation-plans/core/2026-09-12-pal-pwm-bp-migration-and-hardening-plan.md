# PAL PWM Basis-Points 迁移与硬化实施计划

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260912-PAL-PWM-BP-HARDENING` |
| **创建日期** | `2026-09-12` |
| **目标平台/SoC** | `host`（MSVC/MinGW）、`wasm`（Emscripten）、ESP32；MCS51/8 位核为未来接入约束 |
| **工具链/SDK** | CMake 3.15+、Python 3.10+、Emscripten、`arm-none-eabi-nm`（W-6 验收用） |
| **计划状态** | 执行中（In Progress，2026-09-12 已执行 W-1~W-6；复审待补） |
| **优先级** | P0（构建红 + wasm ABI parity）、P1（算法硬化） |
| **计划版本** | `v1.0` |
| **关联 ADR** | [`ADR-0066`](../../decisions/core/0066-pwm-basis-points-and-float-deprecation.md)（bp 定点与浮点下线）、[`ADR-0017`](../../decisions/core/0017-blocking-api-hard-isolation.md)、[`ADR-0056`](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)、[`ADR-0012`](../../decisions/core/0012-contract-honesty-over-silent-degradation.md) |
| **关联设计规范** | [`02-pal-platform-abstraction.md`](../../zh/design/02-wink-micro-os/02-pal-platform-abstraction.md) |
| **关联输入** | 2026-09-12 全仓弃用 API 审计（PWM float / WINK_BLOCKING / button events 三族） |
| **影响仓** | `wink-ai-embedded`（主体）、`wink-ai`（wasm ABI 接线，W-5） |
| **前置状态** | wink-ai-embedded 工作区干净（HEAD `e968e7a` 之后）；wink-ai 仓位于同级目录 `../wink-ai` |

---

## 1. 背景与现状

ADR-0066 已将 `pal_pwm_set_duty(float)` 标记弃用，引入 `pal_pwm_set_duty_bp(uint16_t)`（万分比 `0..10000`），并给出防溢出的四舍五入算法。当前落地约 60%，存在 7 类缺口：

| # | 缺口 | 证据 |
|---|------|------|
| P1 | 契约不完整 | ADR 定义的 `pal_pwm_calc_duty_counter()` 仅存在于 ADR 文档；无防手抖 helper（`pal_pwm.h:69` 只有裸 API） |
| P2 | 实现不一致 | esp32 内联 `uint64_t` 公式（`targets/esp32/pal_hal_pwm_esp32.c:132-146`）；host 转 float 记录（`targets/host/pal_hal_pwm_host.c:54-62`）；wasm 转 float 走 float JS import（`targets/wasm/pal_wasm_ch1b_pwm.c:50-64`） |
| P3 | wasm ABI 断链 | `js_pal_pwm_set_duty_bp` 已声明（`wasm_bridge.h:95`）且 JS shim 存在（`wink_sim_js.js:107`），但**无 C 调用者、TS `WasmImports` 未声明**（`packages/unisim/src/types/wasm/imports.ts:19` 仅 float）；unisim ABI catalog 已标 needs-fix |
| P4 | 调用点残留 | `unisim_smoke/app_callbacks.c:53`、`selftest_pwm_router.c:50-51`、`selftest_rmt_loopback.c:123`、`test/unit/pal/test_host_pal.c:36,44,84,158`、`test_pal_nonblocking_strict.c:28` |
| P5 | 测试缺口 | 仅 1 条 bp 用例（`test_pal_pwm_config.c:41`：3750→37.5%），无边界/舍入/溢出/分级分支覆盖 |
| P6 | 构建红 | `build-host` 有 11 个测试目标因 `-Werror` 失败（PWM 弃用 + `dal_ntc_read_*` 弃用 + ignoring-return + unused，见附录 A） |
| P7 | 门禁缺失 | ADR-0066 Phase 3 的 `nm` 零软浮点验收未接入；wink-tools 无"禁止新增 float PWM"规则 |

## 2. 目标与非目标

**目标**
- G1：bp 成为唯一推荐路径；float wrapper 保留（deprecated）直至 v3.0 的 `PAL_PWM_HIDE_FLOAT_API` 口径，不在本计划删除。
- G2：共享分级算法 + 防手抖宏落位 `pal_pwm.h`（SSOT），esp32/host/wasm 三 target 语义对齐。
- G3：wasm bp 全链路可达且 TS ABI parity 归零（catalog 0 needs-fix）。
- G4：新增算法单测矩阵，覆盖边界/舍入/分级分支。
- G5：**PWM 相关目标构建全绿**；并存量的 11 个 `-Werror` 失败目标（W-4）一并清理，恢复全量构建绿。

**非目标**
- 不删除 `pal_pwm_set_duty(float)`、不改 bp 签名与量纲、不引入 Q16/raw 方案（`pal_pwm_set_duty_raw` 专家后门登记为 Backlog B-1，启动需另开 ADR）。
- 不重构 DAL 公共 API（buzzer/servo/motor 已整数化，仅回归）。
- 不新增/删除 wasm ABI 符号（因此 **不 bump `PAL_WASM_ABI_HASH`**；备选方案 D4-B 除外，见 §3）。
- 不在本计划处理 `wink_button_events_start/stop`（已零调用，仅登记）。

## 3. 已冻结决策（执行前确认）

| ID | 决策 | 理由/备选 |
|----|------|-----------|
| D1 | 算法落位 `pal_pwm.h` **static inline**（单 SSOT）；同时提供 `PAL_PWM_DUTY_PCT` 宏常量 | 避免新 include 路径；host 单测可直接打靶；宏支持 C99 结构体静态常量表达式初始化 |
| D2 | 算法提供显式安全阈值常量 `PAL_PWM_TOP_32BIT_MAX (429496u)`，结合编译期上限宏 `PAL_PWM_DUTY_TOP_LIMIT` 裁剪 64 位路径；允许 target 完全覆写 helper | 仅运行时分支不能避免 `__udivdi3` 被链接；小核设为 65535 时必须编译期裁剪。mcs51/8 位核：`top<=65535` 纯 32 位运算，8-bit 可走 LUT（Backlog B-2）；消除魔法数字 |
| D3 | host 保持 float 观测（`host_record_pwm`/`sim_last_pwm_duty` 不动） | x86 无软浮点约束；避免联动 ~6 个 DAL/BAL 测试文件 |
| D4 | wasm：C 端 `pal_pwm_set_duty_bp` 改调 `js_pal_pwm_set_duty_bp`（**备选 D4-B**：仅补 TS 声明、C 保持 float 中继） | 让已声明 ABI 变真；风险用 JS shim 浮点回退对冲（见 T2.4） |
| D5 | 测试策略：正常用例迁 bp；`test_host_pal` 保留 1 条 legacy float 覆盖 + 局部 pragma；strict 存在性检查改 `_bp` | 兼容性覆盖与"零新弃用调用"兼得 |
| D6 | W-4 实行"三层止血法"并独立提交（C2）；豁免必须写明理由 + 指向本计划/Backlog | 保证全量构建绿可复现，且杜绝 NTC 等无关复杂弃用导致工期失控扩散 |

## 4. 工作分解（WBS）

### W-1 契约层算法硬化（wink-micro-os）

- **T1.1** `pal/include/hal/pal_pwm.h`：新增分级 inline 与安全常量定义：
  ```c
  /** 
   * 32位无符号整数乘法安全阈值：(9999u * PAL_PWM_TOP_32BIT_MAX + 5000u) <= UINT32_MAX (4,294,967,295)
   * 消除裸写 429496u 魔法数字，明确数学推导依据
   */
  #define PAL_PWM_TOP_32BIT_MAX 429496u

  #ifndef PAL_PWM_DUTY_TOP_LIMIT
  #define PAL_PWM_DUTY_TOP_LIMIT 0xFFFFFFFFu  /* 全范围：保留 64 位路径 */
  #endif

  static inline uint32_t pal_pwm_calc_duty_counter(uint16_t bp, uint32_t top);
  ```
  - `bp==0 || top==0 → 0`；`bp>=10000 → top`；否则 `(bp*top + 5000) / 10000` 四舍五入，`>top` 时钳位；
  - `PAL_PWM_DUTY_TOP_LIMIT <= PAL_PWM_TOP_32BIT_MAX` 时仅编译 32 位分支（纯 32 位乘除，MCS51 / Cortex-M0 编译期剔除 `__udivdi3`），否则 runtime `top<=PAL_PWM_TOP_32BIT_MAX` 快速路径 + `uint64_t` 满范围路径；
  - 8 位 PWM（如未来 mcs51/PCA，`top<=255`）：可使用 32 位分支，或 256 项 LUT（Backlog B-2）；target 亦可完全覆写本 helper（D2）。
  - **防御性分工**：`pal_pwm_calc_duty_counter` 作为底层算术 helper，对 `bp>=10000` 实行安全钳位；而公共 PAL 接口 `pal_pwm_set_duty_bp` 严格履行 ADR-0012 合约诚实，当 `bp > 10000u` 时返回 `WINK_ERR_INVALID_ARG`。
- **T1.2** 防手抖 helper 与宏常量（宏 + inline 双轨支持）：
  ```c
  /* 常量表达式安全宏（满足 C99 全局/静态配置结构体初值初始化） */
  #define PAL_PWM_DUTY_PCT(p)       ((uint16_t)((uint32_t)(p) >= 100u ? 10000u : (uint32_t)(p) * 100u))
  #define PAL_PWM_DUTY_PERMILLE(pm) ((uint16_t)((uint32_t)(pm) >= 1000u ? 10000u : (uint32_t)(pm) * 10u))
  #define PAL_PWM_DUTY_OFF          (0u)
  #define PAL_PWM_DUTY_HALF         (5000u)
  #define PAL_PWM_DUTY_FULL         (10000u)

  /* 运行时内联辅助函数（带参数类型约束与边界钳位） */
  static inline uint16_t pal_pwm_duty_pct(uint8_t pct) {
      return (pct >= 100u) ? 10000u : (uint16_t)(pct * 100u);
  }
  static inline uint16_t pal_pwm_duty_permille(uint16_t pm) {
      return (pm >= 1000u) ? 10000u : (uint16_t)(pm * 10u);
  }
  ```
  - **小数百分比（如 7.5%）使用规范**：`PAL_PWM_DUTY_PCT` 仅用于纯整数百分比；对于 RC 舵机中位等带小数的占空比（如 7.5% = 1.5ms/20ms），**必须使用千分比宏 `PAL_PWM_DUTY_PERMILLE(75)` 或直接书写 `750u`**。在头文件注释显式警示：严禁传入 `pal_pwm_duty_pct(7.5)`（否则整型隐式截断为 700 bp，引发 9 度舵机机械偏角）。
- **T1.3** 契约注释更新（bp 范围、四舍五入语义、`>10000` 由 API 层返回 `WINK_ERR_INVALID_ARG`）。
- 验收：host 编译零新告警；静态结构体初始化 `PAL_PWM_DUTY_PCT(50)` 无 C99 编译报错；`pal_pwm_calc_duty_counter` 可被 host 单测直接调用。

### W-2 Target 对齐（wink-micro-os）

- **T2.1** `targets/esp32/pal_hal_pwm_esp32.c:132-146`：删除本地公式，改用 T1.1（行为等价：相同公式与舍入）。
- **T2.2** `targets/host/pal_hal_pwm_host.c`：确认保持 D3，不改。
- **T2.3** `targets/wasm/pal_wasm_ch1b_pwm.c:50-64`：bp 路径改调 `js_pal_pwm_set_duty_bp(channel, basis_points)`；内部影子以 bp 为源（`uint16_t s_pwm_duty_bp[]`），`pal_wasm_get_pwm_duty_percent()` 由 bp 派生返回（导出契约不变）。
- **T2.4** `targets/wasm/wink_sim_js.js`：给 `js_pal_pwm_set_duty_bp` shim 增加**浮点回退**（宿主无 override 时转调 `js_pal_pwm_set_duty(channel, bp/100)`），避免跨仓版本错配导致观测断链；`wink_sim_stub.js` 已列出符号，核对即可。
- **T2.5**（约束登记）在 `pal_pwm.h` 注释与 02-pal 规范记录：8/16 位 target 接入 PWM 时必须定义 `PAL_PWM_DUTY_TOP_LIMIT`（mcs51 不得走 uint64 路径）。
- 验收：esp32 无本地公式残留；grep 证明 wasm bp 调用 `js_..._bp`；wasm 单测与 node 冒烟通过。

### W-3 单测与调用点迁移

- **T3.1** 新增 `test/unit/pal/test_pal_pwm_bp_math.c`：
  - 数值矩阵与独立 64 位参考实现比对（见 §6.2），含 `PAL_PWM_DUTY_TOP_LIMIT=65535` 编译分支 smoke；
  - 边界：`bp=0/1/9999/10000`，`top=0/1/255/65535/65536/1048575`，舍入半程（`+5000` 临界）。
- **T3.2** `test/CMakeLists.txt:182` 邻域注册 `add_wink_host_test(test_pal_pwm_bp_math unit/pal/test_pal_pwm_bp_math.c)`。
- **T3.3** 迁移调用点（50%→5000 等）：
  - `runtime/selftest/src/selftest_pwm_router.c:50-51`、`selftest_rmt_loopback.c:123`；
  - `wink-micro-app/unisim_smoke/app_callbacks.c:53`；
  - `test/unit/pal/test_host_pal.c` 的正常用例改 bp（`7.5f→750` 等，断言 `sim_last_pwm_duty` 仍按 host 语义），保留 1 条 legacy float 用例并加局部 `-Wdeprecated-declarations` pragma + 注释；
  - `test_pal_nonblocking_strict.c:28` 改为 `sizeof(&pal_pwm_set_duty_bp)`。
- 验收：上述文件在 `-Werror` 下零弃用告警；新测试全绿。

### W-4 存量 `-Werror` 清理（严格止血法，恢复全量构建绿）

- **T4.1** 告警普查（执行时刷新，附录 A 为 2026-09-12 快照）：
  `cmake --build build-host --parallel 8 -- -k > build.log`，按 `目标 → 文件:行 → 类别` 落表。
- **T4.2** 实行**三层止血实施策略**，防止非 PWM 模块重构拖垮主任务：
  1. **第一层（PWM 直接关联目标，彻底重构）**：`test_pal_pwm_config.c`、`test_host_pal.c`、`test_pal_nonblocking_strict.c`，必须彻底迁入 bp 接口，零遗留告警；
  2. **第二层（琐碎警告，快速包揽闭环）**：`test_dal_load_cell.c:89,111`、`test_pal_i2c_bus.c:29-63`、`test_pal_deferred.c:11` 等 ignoring-return 处，包裹 `WINK_IGNORE_RESULT(...)` 或增补断言；unused 变量加 `WINK_IGNORE_UNUSED(...)`；
  3. **第三层（非 PWM 复杂弃用，局部隔离止血）**：`test_dal_ntc.c:135-257`（13 处 `dal_ntc_read_*`）若迁移为非阻塞/`ddegc` 涉及状态机重构，**严禁在本计划中展开深入业务重写**；采用文件级或局部 `#pragma GCC diagnostic push / -Wdeprecated-declarations` 隔离，注释关联至独立 Backlog B-4，耗时严格压在 0.5h 内。
- **T4.3** 连带目标与构建验证：清理后重新全量编译，确保连带目标（如 `test_wink_selftest`）恢复绿色。
- 验收：`cmake --build build-host --parallel 8` **exit 0**；W-4 整体工期严格控制在 0.5 天内。

### W-5 跨仓 ABI 接线（wink-ai）

- **T5.1** `packages/unisim/src/types/wasm/imports.ts:19` 邻域新增 `js_pal_pwm_set_duty_bp(channel: number, basisPoints: number): void`（注释范围 `[0,10000]`，ADR-0066）。
- **T5.2** `packages/unisim/src/core/bridge/unisim-bridge-factory.ts:124,258`：实现 bp 导入并转百分比后喂既有 `pwmSink`（`pluginHost.notifyDutyChange` 契约不变）。
- **T5.3** `packages/embedded-frontend/src/simulation-kernel/workers/wasm-simulation.worker.ts:225`：类型/注释核对（pwmSink 保持 `duty: percent`）。
- **T5.4** `packages/unisim/scripts/abi-catalog/abi-catalog.yaml:520-525` 状态 needs-fix → implemented；重跑 catalog/parity 校验（0 needs-fix）。
- **T5.5** ABI hash：本计划无符号增删 → `PAL_WASM_ABI_HASH`（`pal_wasm_degradation.c:80`）与 `wasm-physical-bridge.ts:111` 的 `EXPECTED_ABI_HASH` **均不改**；若评审选 D4-B 删声明，则必须双端同步 bump。
- 验收（wink-ai 仓内命令 + 头less 场景）：固件调 `_bp` 时 TS 侧 `pwmSink` 收到对应 duty；`verifyAbiHash` 通过。

### W-6 门禁与文档

- **T6.1** nm 零软浮点/零 64 位软除验收（MCU 目标）：
  `arm-none-eabi-nm -u <fw.elf> | findstr "__aeabi_f __udivdi3"` → 0 命中；同时记录 host `PAL_PWM_DUTY_TOP_LIMIT=65535` 编译 smoke 无 64 位符号。
- **T6.2**（P1）wink-tools lint 新增两条规则（沿用 `tools/lint/packs/` 机制；wink-ai 仓变更）：
  1. 禁止**新增** float PWM 调用（`pal_pwm_set_duty(` 仅允许存量豁免清单）；
  2. 常量字面量启发式告警：`pal_pwm_set_duty_bp(ch, <1..100 的整型字面量>)` → 告警 `Did you mean PAL_PWM_DUTY_PCT(val)?`。
     **防误伤边界**：严格限定为 `1 <= literal <= 100`；**显式排除 `0`**（关断 PWM 极其常见，允许裸传 0）与 `10000`，杜绝滥报。
- **T6.3** 回写设计规范：`docs/zh/design/02-wink-micro-os/02-pal-platform-abstraction.md`（bp 算法分级、`PAL_PWM_DUTY_TOP_LIMIT`、`PAL_PWM_TOP_32BIT_MAX`、helper 宏与常量表达式）。
- **T6.4** ADR-0066 处理：正文只读，不改；在 ① 规范回写并在本计划 §4 记录算法属实现细化（不新开 ADR）。
- **T6.5**（P1）编码/AI 规则约束：更新 `.agents/rules/c-code.md`，明确：
  1. "PWM 占空比必须使用 `PAL_PWM_DUTY_PCT()` 或显式四位万分比字面量（如 `5000u`），关断允许裸 `0`，禁止裸 `1..100`/浮点字面量"；
  2. "小数百分比（如 7.5%）禁止使用 `_PCT`，必须使用 `PAL_PWM_DUTY_PERMILLE(75)` 或 `750u`"；
  3. 在 `AGENTS.md` Critical Patterns 增一行指引（AI 生成代码防线）。

## 5. 改动文件清单

| 仓 | 文件 | 动作 | 归属 |
|----|------|------|------|
| embedded | `wink-micro-os/pal/include/hal/pal_pwm.h` | 改（算法+宏） | W-1 |
| embedded | `wink-micro-os/targets/esp32/pal_hal_pwm_esp32.c` | 改 | W-2 |
| embedded | `wink-micro-os/targets/wasm/pal_wasm_ch1b_pwm.c` | 改 | W-2 |
| embedded | `wink-micro-os/targets/wasm/wink_sim_js.js` | 改（bp shim 回退） | W-2 |
| embedded | `wink-micro-os/test/unit/pal/test_pal_pwm_bp_math.c` | 新增 | W-3 |
| embedded | `wink-micro-os/test/CMakeLists.txt` | 改（注册） | W-3 |
| embedded | `selftest_pwm_router.c` / `selftest_rmt_loopback.c` | 改 | W-3 |
| embedded | `wink-micro-app/unisim_smoke/app_callbacks.c` | 改 | W-3 |
| embedded | `test/unit/pal/test_host_pal.c` / `test_pal_nonblocking_strict.c` | 改 | W-3 |
| embedded | W-4 清单（附录 A 对应文件，~9 文件） | 改 | W-4 |
| embedded | `docs/zh/design/02-wink-micro-os/02-pal-platform-abstraction.md` | 改 | W-6 |
| wink-ai | `packages/unisim/src/types/wasm/imports.ts` | 改 | W-5 |
| wink-ai | `packages/unisim/src/core/bridge/unisim-bridge-factory.ts` | 改 | W-5 |
| wink-ai | `packages/embedded-frontend/src/simulation-kernel/workers/wasm-simulation.worker.ts` | 核对/注释 | W-5 |
| wink-ai | `packages/unisim/scripts/abi-catalog/abi-catalog.yaml` | 改（状态） | W-5 |

## 6. 验收标准

### 6.1 命令级（仓库根目录）

```bash
# 1. 全量构建（W-4 后应 exit 0）
cmake -DTARGET_PLATFORM=host -B build-host
cmake --build build-host --parallel 8

# 2. host 双轨测试全绿（含新增 math 测试与 11 个修复目标）
ctest --test-dir build-host/wink-micro-os/test -E "^wasm_" --output-on-failure

# 3. wasm 轨全绿（需 emcc/node）
ctest --test-dir build-host/wink-micro-os/test -R "wasm_" --output-on-failure

# 4. 定向
ctest --test-dir build-host/wink-micro-os/test -R "test_pal_pwm|test_host_pal|test_pal_nonblocking_strict" --output-on-failure
```

### 6.2 数值矩阵（与独立 64 位参考实现逐值比对）

| bp | top | 期望 counter | 说明 |
|----|-----|--------------|------|
| 0 | 65535 | 0 | 下界 |
| 1 | 65535 | 7 | 0.01% 舍入 |
| 5000 | 65535 | 32768 | 恰为 .0（5000×65535+5000 = 327,680,000） |
| 9999 | 65535 | 65528 | 上界邻近 |
| 10000 | 1048575 | 1048575 | 满量程直返 top |
| 1 | 1048575 | 105 | 20-bit 舍入 |
| 5000 | 1048575 | 524288 | 恰为 .5 → 进位 |
| 9999 | 1048575 | 1048470 | 20-bit 上界邻近 |
| 任意 | 0 | 0 | top 保护 |

补充：随机 `bp∈[0,10000]`、`top∈[0,1048575]` 抽 1000 组与参考实现比对；`PAL_PWM_DUTY_TOP_LIMIT=65535` 编译变体跑同一矩阵（限 `top<=65535`）。

### 6.3 功能级

- 生产 wasm/headless：固件调用 `pal_pwm_set_duty_bp` 后，TS `pwmSink`/`notifyDutyChange` 收到 `bp/100` 百分比；ABI catalog 0 needs-fix。
- `PAL_PWM_HIDE_FLOAT_API` 编译 smoke：隐藏 float 声明的构建无编译错误（wink-micro-os 侧预演，不改变默认）。

## 7. 风险与回滚

| 风险 | 影响 | 缓解 |
|------|------|------|
| wasm C/TS 版本错配 | bp 观测断链（静默） | T2.4 shim 浮点回退 + W-5 与 T2.3 联动提交 |
| 分级只用运行时分支 | 小核仍链 `__udivdi3` | D2 编译期限宏裁剪 + T6.1 nm 验收 |
| host/真机语义差 | 断言口径混乱 | D3 冻结 host 语义；矩阵测试按各 target 语义断言 |
| `_bp(ch, 50)` 语义断层 | 实际 0.5% 无告警 | 三道防线：T1.2 helper/宏、T6.2 lint 字面量告警（排除 0）、T6.5 编码/AI 规则 |
| 小数百分比（如 7.5%）截断 | 舵机产生 9 度机械偏差 | 规范引导：显式声明禁止 `_PCT(7.5)`，推行 `PAL_PWM_DUTY_PERMILLE(75)` 或 `750u` |
| C99 静态配置初始化报错 | 全局/静态配置无法使用 inline | T1.2 提供 `PAL_PWM_DUTY_PCT` 常量表达式宏，满足静态结构体初值需求 |
| W-4 存量弃用导致工期蔓延 | NTC 状态机重构拖累 P0 交付 | T4.2 实行三层止血法：NTC 局部 pragma 豁免 + 登记 B-4，耗时锁定 < 0.5d |
| 常数除法在小核的成本 | SDCC 若不生成魔数乘移位，仍有 32 位软除 | T6.1 nm/disasm 实测；不可接受时 8-bit 走 LUT（Backlog B-2） |
| 豁免扩散 | `-Werror` 被永久静音 | D6：豁免必须注释 + 指向本计划/Backlog；T4.1 每季度复查 |
| ABI 规则违反 | hash/TS 不一致 | 本计划不改符号；若选 D4-B 删声明，双端同步 bump |

**回滚**：按提交粒度 `git revert`；W-5 与 T2.3 必须成对回滚；W-4 独立可回滚。

## 8. 实施顺序、提交拆分与时间线

| 提交 | 内容 | 仓 | 验收 |
|------|------|----|------|
| C1 `feat(pal): ...` | W-1 + W-2 + W-3 | embedded | 定向测试绿；grep 无遗留 float/PWM 弃用新调用 |
| C2 `fix(test): ...` | W-4（告警普查已附） | embedded | 全量 build exit 0；host ctest 全绿 |
| C3 `docs(pal): ...` | W-6.3 | embedded | 规范与代码一致 |
| C4 `feat(unisim): ...` | W-5 | wink-ai | TS 类型/测试 + catalog parity + headless 观测 |

时间线（单人，含缓冲）：D1 W-1/W-2/W-3 → D2 W-4 + 全量构建 → D3 W-5 + 场景 → D4 W-6 + 复审。**合计约 3.5~4 天**。

## 9. 附录 A：2026-09-12 构建失败快照（W-4 输入）

11 个失败目标：`test_dal_load_cell`、`test_dal_ntc`、`test_host_pal`、`test_pal_atomic`、`test_pal_deferred`、`test_pal_i2c_bus`、`test_pal_nonblocking_strict`、`test_pal_pwm_config`、`test_pal_time_safety`、`test_sim_scheduler_headless_jump`、`test_wink_selftest`。

| 类别 | 代表位置 |
|------|----------|
| deprecation（PWM float） | `test_host_pal.c:36,44,84,158`、`test_pal_nonblocking_strict.c:28` |
| deprecation（`dal_ntc_read_*`） | `test_dal_ntc.c:135-257`（13 处） |
| ignoring-return | `test_dal_load_cell.c:89,111`、`test_host_pal.c:85,87,95,100,165`、`test_pal_i2c_bus.c:29-63`、`test_pal_deferred.c:11`、`test_pal_pwm_config.c:21,36,43` |
| unused | `test_pal_atomic.c:37`、`test_pal_time_safety.c:19` |

> 执行 T4.1 时刷新本表，避免遗漏连带目标（如 `test_wink_selftest`）。

## 10. 状态跟踪

- [x] D1~D6 决策确认（按计划冻结决策执行；D4 采用主方案 C 调 `_bp` + T2.4 回退）
- [x] W-1 契约层算法硬化
- [x] W-2 Target 对齐（esp32 / wasm / shim 回退；host 按 D3 保持观测，另修复三端 `pal_pwm_init` 默认 pin 覆盖通道映射缺陷）
- [x] W-3 单测矩阵 + 调用点迁移
- [x] W-4 存量 `-Werror` 清理（12 个目标构建绿；全量构建仅剩 wink-tools codegen 缺失导致的 2 个 e2e app 目标失败，与本计划无关）
- [x] W-5 wink-ai ABI 接线（imports / bridge / catalog；hash 不变；worker 仅核对）
- [x] W-6 文档回写 + lint 规则 + AI 编码规则 + nm 门禁记录（arm-nm 在本机缺失，以 host 65535/全范围对比冒烟代替；ESP32 真机 nm 待 CI 补）
- [ ] 复审记录（docs/reviews/core/）

## 11. Backlog（本计划外，需单独立项）

| ID | 事项 | 触发条件 | 备注 |
|----|------|----------|------|
| B-1 | `pal_pwm_set_duty_raw(channel, raw_ticks)` 专家级原生计数接口 | 出现 20-bit 级精密控制场景（数控电源等） | 属新 ABI 符号：需 ADR + bump `PAL_WASM_ABI_HASH` + TS 声明；与 bp 量纲契约的边界需定义 |
| B-2 | 8-bit PWM 占空比 256 项 LUT / 定点快速换算 | mcs51/PCA 类 8 位 PWM 接入 PAL 且 T6.1 实测除法开销不可接受 | 仅影响 `top<=255` 路径；放 target 实现或 header 编译期分支 |
| B-3 | `.agents/rules/c-code.md` 之外的 codegen 侧防线（wink-ai 模板生成时注入 `pal_pwm_duty_pct()`） | AI 生成应用代码出现裸 bp 字面量回归 | 依赖 wink-tools 模板改造，独立小项 |
| B-4 | `test_dal_ntc.c` 弃用 API 专项重构与 pragma 解除 | 启动 NTC 温度传感器规范收敛任务时 | 彻底将 13 处旧接口替换为非阻塞 `ddegc` 契约，解开 W-4 中的局部豁免 |
