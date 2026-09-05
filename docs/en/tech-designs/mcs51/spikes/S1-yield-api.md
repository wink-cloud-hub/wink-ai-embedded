# Spike-S1 结论报告：Fiber 让出 API 与配额切出机制

| 项 | 内容 |
|---|---|
| Spike 编号 | S1（M0-2） |
| 日期 | 2026-08-27 |
| 状态 | ✅ 结论已出（双端实证） |
| 验证环境 | host：MinGW gcc 14.2.0 + Win32 Fiber；wasm：emcc 4.0.5（emscripten fiber + asyncify）+ Node v22.19.0 |
| PoC 资产 | `docs/tech-designs/mcs51/spikes/assets/s1/`（`s1_spike.c` + `shim/pal_osal.h`；`.exe/.js/.wasm` 为构建产物，不入库） |
| 消费里程碑 | M1（runtime fiber 桥接）、M2（虚拟时钟/配额/Catch-Up/定时器步进） |

---

## 1. 问题与裁决

**问题**：51 用户代码在 fiber 中跑裸机 `while(1)`，紧凑忙等（`while(!TF0);`）内无任何睡眠/阻塞调用。协作式调度器无法抢占——fiber 不切回，主循环、物理引擎、wasm 事件循环全冻结。现有 WCET 故障 8002 是**事后墙钟检测**（fiber 返回后才测），救不了已冻结的循环。需要：(1) 一个让出机制；(2) 裁决新增 `sim_ctx_yield()` 还是复用 `pal_os_sleep_ms(0)`；(3) 裁决配额阈值。

**候选方案对比**：

| 方案 | 描述 | 结论 |
|---|---|---|
| A. 新增 `sim_ctx_yield()` 调度器原语 | 在 sim_ctx 层加专用 yield | ❌ 不必要。现有 `sim_scheduler_yield_timed(id, now, 0)` + `sim_ctx_switch(cur, main)` 已完整表达"duration-0 协作让出 + 回主 fiber"，host/wasm 双端 `pal_os_sleep_ms(0)` 就是此封装 |
| B. mcs51 直接复用 `pal_os_sleep_ms(0)` | 对标 Arduino `yield()` | ⚠️ 机制对，但 mcs51 不能直接调——它语义绑定 host `s_time_us`/wasm `s_virtual_us` 主时钟，且 mcs51 需在切出点附带微步计费 + 配额判定 + 切回后定时器步进 |
| **C. mcs51 层自维护虚拟从时钟 + 拦截点微步计费 + 配额切出（复用 duration-0 yield）** | fiber 内每次 SFR 读/`_nop_` 充功能级 µs；累计满配额则 duration-0 yield 回主；主循环 catch-up 步进定时器 + 维护 tick 边界 | ✅ **采纳** |

**裁决一句话**：**不新增调度器 API**；mcs51 层提供自己的 `mcs51_maybe_yield()`（微步计费虚拟从时钟 + 配额判定 + 复用 `sim_scheduler_yield_timed(...,0)`/`sim_ctx_switch` 切出），定时器步进与 tick 边界由 runtime 在切出返回点驱动。

## 2. 关键证据（双端运行输出）

host 与 wasm 输出**逐字节一致**：

```
[A] after 3x1ms delay: virt=3000 us, master_ticks=0
[fiber] poll exited: TF0 at virt=50000 us after 9999 iters, 100 quota yields
[B] tight-poll: ticks=5, quota_yields=100, virt=50000 us, TF0=1
S1 SPIKE PASS: tight poll non-freezing + quota catch-up + 1:1 clock
```

- **场景 A**（定时延时）：3×1ms 虚拟睡眠 → `s_virt_us=3000`，时钟精确。
- **场景 B**（紧凑忙等 `while(!TF0)`，无显式 yield）：9999 次轮询迭代，触发 **100 次配额切出**；主循环推进 **5 个 tick**；mock Timer0 在**恰好 50000µs** 置 TF0（= 5 tick × 10ms，**1:1 时间守恒**）；轮询正常退出，**零冻结**。

**复现命令**（在 `wink-micro-os/` 下）：

```bash
# host (gcc + Win32 fiber)
gcc -std=c11 -Wall -Wextra -g \
  -I targets/common/include -I pal/include -I ../docs/tech-designs/mcs51/spikes/assets/s1/shim \
  ../docs/tech-designs/mcs51/spikes/assets/s1/s1_spike.c \
  targets/common/src/wink_sim_scheduler.c osal/host/sim_ctx_win32_fiber.c \
  -o /tmp/s1_host.exe -lkernel32 && /tmp/s1_host.exe

# wasm (emcc + asyncify fiber + node)
emcc -std=c11 -O1 \
  -I targets/common/include -I pal/include -I ../docs/tech-designs/mcs51/spikes/assets/s1/shim \
  ../docs/tech-designs/mcs51/spikes/assets/s1/s1_spike.c \
  targets/common/src/wink_sim_scheduler.c osal/wasm/sim_ctx_emscripten_fiber.c \
  -s ASYNCIFY=1 -s ENVIRONMENT=node -s ALLOW_MEMORY_GROWTH=1 \
  -o /tmp/s1_wasm.js && node /tmp/s1_wasm.js
```

## 3. 关键机制发现（指导 M2 实现）

1. **鸡生蛋约束（最重要）**：虚拟时钟**不能**由主循环在 fiber 切出后才推进——fiber 内配额判定依赖虚拟时间，若虚拟时间只在主循环涨，则配额条件永不满足、永不切出（PoC 第一版即因此死循环超时，exit 143）。**虚拟时间必须由 fiber 内拦截点微步计费推进**：每次被拦截的 SFR 读 / `_nop_()` 充一个功能级 µs 常数（PoC 用 `MICROSTEP_US=5`，对应 12MHz 下几条指令的功能级近似，M2 校准）。

2. **配额切出 = duration-0 yield**：`sim_scheduler_yield_timed(self, virt_now, 0)` 置 WAITING+`wakeup_us=virt_now`，主循环 `wakeup_by_time()` 用 `<=` 立即重新置 READY——实质协作式轮转让出，主循环获得一次"catch-up 步进定时器 + 维护 100Hz tick 边界"的窗口。

3. **Catch-Up 补账落点**：fiber 微步已计费虚拟时间；切出返回点由 mcs51 runtime 把定时器模型从"上次步进虚拟时刻"推进到"当前 `s_virt_us`"（溢出 → 置 TF0 / 派 ISR），并在累计满 10000µs 时交还一个主 tick。PoC 中 `step_timers()` + tick 边界计数即此逻辑的最小镜像。

4. **纯空 `while(1){}` 无拦截点 = 协程模型硬限制**：体内若无任何 SFR 访问 / `_nop_()` / 延时，则无任何微步计费点，无法切出。真实业务 `while(1)` 体内必有翻转/读 SFR/调延时；`while(!TF0);` 有 SFR 读可拦。**需在用户手册 + `WINK_SIM_STRICT` 声明**：紧凑循环体内必须至少含一次 SFR 访问/`_nop_`/延时，否则由 WCET 8002 墙钟故障捕获（诚实契约，ADR-0012）。

5. **配额阈值与 WCET 阈值正交**：
   - `WINK_SIM_TASK_WCET_THRESHOLD_US=5000` 是**墙钟**事后阈值（真实 CPU 执行耗时，抓真冻结），保持不变。
   - mcs51 配额 `QUOTA_US=500` 是**虚拟时间**协作切出粒度，须远小于主 tick（10000µs）；500µs = 5% tick，PoC 验证 50ms 定时器仅 100 次切出、开销可接受。
   - 两者不冲突：配额保证流畅（主动让出），WCET 兜底（被动抓漏网冻结）。

6. **asyncify 栈**：wasm 端 fiber 走 `emscripten_fiber` + 每 fiber 64KB asyncify 栈（`WINK_SIM_ASYNCIFY_MIN`），PoC 单 fiber 嵌套浅、无溢出；M6 iron_ntc 深嵌套（查表+控温+串口）需在此预算内复验（R-002 保留）。

## 4. 可复用产物（M1/M2 直接消费）

**M1 `mcs51_runtime.cpp` / intrins 最小版**：

```c
/* mcs51 层虚拟从时钟 + 配额切出（复用现有调度器，不新增 API） */
static uint64_t s_virt_us;          /* mcs51 虚拟微秒时钟（微步计费推进） */
static uint64_t s_slice_start;
#define MCS51_QUOTA_US        500u  /* 虚拟时间片配额 */
#define MCS51_MICROSTEP_US      5u  /* 每拦截点功能级计费（M2 校准） */

/* 拦截点统一入口：SFR 读钩子 / _nop_() 内调用 */
static inline void mcs51_microstep(uint32_t us) {
    s_virt_us += us;
    if ((uint64_t)(s_virt_us - s_slice_start) >= MCS51_QUOTA_US) {
        uint32_t self = sim_scheduler_current_id();
        sim_scheduler_yield_timed(self, s_virt_us, 0);   /* duration-0 让出 */
        sim_ctx_switch(sim_scheduler_current_ctx(), mcs51_main_ctx());
        s_slice_start = s_virt_us;                       /* 切回：新片 */
        mcs51_timers_catch_up(s_virt_us);                /* M2：定时器补账 */
    }
}
#define _nop_() mcs51_microstep(MCS51_MICROSTEP_US)
/* WinkSfrBitProxy::operator uint8_t() 读 TF0 等前亦调 mcs51_microstep() */
```

- `delay_ms(n)` → `sim_scheduler_yield_timed(self, s_virt_us, n*1000)` + switch（虚拟时间睡眠，主循环在 wakeup 点唤醒）。
- 主循环 tick 边界 / `step_timers` 镜像见 PoC `run_master()`。

## 5. 回写点（影响计划任务行）

| 任务 | 回写内容 |
|---|---|
| M1 `src/mcs51_runtime.cpp` | fiber 注册用现有 `sim_scheduler_register()`；`_nop_()` 最小版接 `mcs51_microstep()`；**不新增 sim_ctx_yield** |
| M1 `include/intrins.h` | `_nop_()` = 微步 + maybe_yield（非空操作） |
| M2 虚拟时钟/配额/Catch-Up | 按本报告 §3.1~3.3、§4 片段实现；`QUOTA_US=500`、`MICROSTEP_US` M2 校准；定时器在切出返回点 catch-up |
| M2 `test_unisim_clock_mapping` | 直接采用 PoC 场景 B 断言（50ms 定时器 = 5 tick、配额切出 >0、virt=50000） |
| M3 STRICT 双态 | 不支持清单加"紧凑空循环体内须有拦截点"，违例归 WCET 8002 |
| 用户手册 §5.2 | 补微步协程让出纪律：拦截点 = SFR 读 / `_nop_` / 延时 |
| 风险 R-002 | 机制风险退役（双端实证不冻结）；保留 asyncify 深嵌套栈预算一项至 M6 |

---

*Spike 结论在 M2 实现落地后，可将本报告关键片段并入时序面 SSOT §2 并归档。*
