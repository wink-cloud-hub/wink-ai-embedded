# ADR-0072：8051 仿真双时钟域——主从 1:1 映射、配额强制切出与 Catch-Up 补账

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-08-28）** |
| **日期** | 2026-08-27 |
| **触发** | MCS-51 拦截层时序面设计：51 用户代码在 fiber 中以裸机 `while(1)` 运行，无 OS 让出；若虚拟时间与宿主 UniSim 物理时间脱节，会出现三大问题——B1 单线复用 DIO 时序撕裂、B2 虚拟时钟与物理时间失真、B3 静态初始化顺序死锁（见时钟域规格书 §1）。 |
| **影响范围** | `frameworks/mcs51/src/mcs51_clock.cpp`（从时钟/配额/Catch-Up 驱动）、`mcs51_timer.cpp`（定时器模型）、`mcs51_bridge.cpp`（拦截点接入）；`targets/common/` fiber 让出 API（Spike-S1 裁决）；外设即时模型（ADC0832/CMS8S）。 |
| **决策者** | 项目 Owner（AD-13/AD-14/AD-17） |
| **关联 ADR** | [ADR-0070](0070-mcs51-zero-code-simulation-interception-layer.md)（umbrella）、[ADR-0007](0007-cooperative-loop-execution-model.md)（协作式循环执行模型） |
| **关联计划** | [`2026-08-27-mcs51-zero-code-simulation-plan.md`](../../implementation-plans/core/2026-08-27-mcs51-zero-code-simulation-plan.md)（M2 核心） |
| **关联技术设计** | `docs/tech-designs/mcs51/2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md`（时序面 SSOT） |

---

## 1. 背景（Context）

宿主侧 Wink 运行在协作式调度器上：app_loop 以 100Hz（dt=10ms）为主时钟驱动 UniSim 物理引擎。51 用户代码假设自己独占 CPU：`delay_ms(100)` 是忙等、`while(!TF0);` 轮询溢出标志、`_nop_()` 耗 1 机器周期。单线程仿真下，若用户 fiber 不主动让出，宿主调度器、物理引擎、wasm 事件循环全部冻结；若用宿主睡眠直接映射延时，虚拟时间又与物理时间失真（热平衡、串口波特率全错）。

## 2. 方案比选（Options）

| 方案 | 描述 | 优 | 劣 | 结论 |
|---|---|---|---|---|
| A. 宿主睡眠直映 | `delay_ms` → `pal_os_sleep_ms` | 简单 | 51 虚拟时钟不存在，定时器/波特率无法在虚拟时间内自推进；紧凑忙等 `while(!TF0)` 无睡眠点仍死锁 | ❌ |
| B. 指令级节拍仿真 | 模拟 12-T 机器周期逐条推进 | 最保真 | 性能不可接受、需完整 8051 核心模型；与 AD-2 功能级精度矛盾 | ❌ |
| C. 双时钟域主从契约 + 配额切出补账 | 宿主 tick 为主、虚拟 µs 为从，1:1 映射；fiber 按时间片配额运行，耗尽强制切出并补账 | 功能级精度下时间守恒；忙等不死锁；即时外设 0µs | 需调度器让出 API 与补账循环（Spike-S1） | ✅ **采纳** |

## 3. 决策结论（Decision）

### D1. 双时钟域主从契约（AD-14）
- **主时钟**：宿主 app_loop，100Hz / dt=10ms 物理时间。
- **从时钟**：mcs51 虚拟微秒计数 `s_virtual_us`，与主时钟 **1ms 物理 : 1ms 虚拟硬实时 1:1 映射**。
- **即时外设耗时 0µs**：ADC0832/CMS8S 等功能级外设转换在当前调用内同步完成（见 ADR-0070 D9/D11），不消耗虚拟时间——功能级精度下外设访问是状态机跳转而非时序等待。
- 每个宿主 tick，fiber 在 10ms 虚拟时间预算内运行；预算耗尽即切出，保证物理热平衡/动画连续。

### D2. 配额强制切出（AD-17 前半）
- 紧凑空轮询（`while(1){}`、`while(!TF0);`）内无任何让出点时，调度器按单次运行微秒配额（提议阈值 500µs，Spike-S1 与现有 `WINK_SIM_TASK_WCET_THRESHOLD_US=5000` 校准）强制上下文切换。
- 隐式让出点：`_nop_()`、延时函数、带超时的状态位读。
- **【M2 落地校准 2026-08-28】配额片 = 10,000µs（一个 100Hz 主 tick），非 S1 PoC 提议的 500µs。** 生产 runtime（`pal_sim_scheduler_run`）把每次主任务 fiber 派发计为一个 tick（PoC 用自定义主循环按时间边界计数）；配额片对齐 tick 才能保持「`delay_ms(100)` = 10 ticks」与 1:1 计费守恒——若片为 500µs，每次让出向主时钟计费 10ms 而从时钟只推进 500µs，守恒破裂。让出动作为 duration-0 协作让出（`pal_os_sleep_ms(0)`），主时钟计费经 `pal_os_busy_wait_us()` 1:1 推进（host `s_time_us`；wasm JS 虚拟时钟桥，Node 测试桩同步调 `pal_wasm_advance_virtual_clock`）。
- **【M2 落地注记】mcs51 层保留自有 s_virtual_us 从时钟**（不直接读主时钟）：虚拟时间必须在 fiber 内拦截点充电推进（S1 §3.1 鸡生蛋约束）；主时钟仅作 1:1 计费接收方。

### D3. Catch-Up 补账（AD-17 后半，时间守恒核心）
- fiber 被强制切出时，**必须补齐当前 tick 未消费的虚拟微秒差额**，并以补账时间驱动定时器步进（溢出 → ISR 派发），再交还执行权。
- 补账循环保证：无论用户代码怎么忙等，`s_virtual_us` 累计推进量与宿主物理时间严格 1:1 守恒——`delay_ms(100)` 恰对应 10 个宿主 tick，定时器周期、波特率、热积分全部正确。
- 配额超额发 `WINK_WARN_WCET_EXCEEDED`（非致命；致命级告警视为验收 #2 失败）。

### D4. Trap 四红线（AD-13，硬实时纪律）
Level 2 即时陷阱内：
1. **零延时**：严禁调用延时/阻塞 API；
2. **禁止让出**：严禁调用 fiber yield（重入即死锁）；
3. **纯状态机**：仅允许修改外设内部状态或读写 SFR 影子；
4. **时钟解耦**：定时推进一律交调度器 fiber tick / 补账循环完成。

### D5. 静态初始化安全三铁律（B3）
1. 核心表（陷阱表、影子、ISR 向量表）全 POD 零初始化 BSS，无动态 ctor 依赖；
2. `WinkSfr` 对象非 const + constexpr 构造，走常量初始化（见 ADR-0071 D4）；
3. 中断派发执行期门控：`s_interrupts_enabled` 在运行期显式使能前，任何静态注册期的伪触发都不派发 ISR。

### D6. ADC0832 3 线 DIO 阶段隔离（AD-15，时序面规格书 §3）
- DI/DO 物理共线时，状态机以 IDLE/PHASE_INPUT/PHASE_OUTPUT 隔离：输入阶段采集 DI 配置位；输出阶段屏蔽 MCU 释放总线的 `DIO=1` 写入，每个 CLK 下降沿把注入值对应位送到 on_read 缓存并**同步镜像锁存器影子位**，保证位读与整端口读结果一致。

## 4. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| 忙等死锁消除：任何 `while(1)` 都不冻结宿主/wasm | 需调度器提供 fiber 让出能力（Spike-S1：新增 `sim_ctx_yield()` 或复用 sleep(0) 路径） |
| 虚拟时间与物理时间 1:1 守恒，定时器/串口/热学闭环全部正确 | 配额阈值需实测校准；补账循环须防饿死（外设即时 0µs 保证有界） |
| 即时外设 0µs 模型简单高性能 | 亚微秒级协议（12-T 精度）明确不支持（ADR-0070 D6） |
| 三铁律杜绝静态初始化 fiasco | 外设开发须守 Trap 四红线，lint/评审把关 |

**测试约束**（时序面规格书 §6）：`test_adc0832_dio_shared`（DIO 共享时序）、`test_unisim_clock_mapping`（100ms↔10 tick、补账守恒）、`test_static_init_safety`（3 TU 跨单元静态注册）；验收 #2（死循环不冻结、无致命 WCET 告警）。

## 5. 遵循与后续（Compliance & Follow-up）

- M0 Spike-S1 裁决让出 API 形态与配额阈值；M2 落地双时钟域、补账与定时器；M4 外设状态机消费时钟契约。
- ~~Accepted 后随 ADR-0070 回写 Layer-①（04-wasm-simulation 时钟/调度小节）。~~ **已提前回写（2026-08-28）**：`02-mechanisms/02-virtual-clock.md` §6（框架级双时钟域：主从分工/1:1 映射/配额/Catch-Up/三铁律）+ `03-scheduler-and-concurrency.md` §3.2（tick 计数口径 + duration-0 让出原语）。0070 umbrella 转 Accepted 时无需重复本决策回写。

---

*本 ADR 状态变更请在此记录：*
- 2026-08-27：Proposed（随 MCS-51 拦截层方案提交；M2 时钟测试全绿后随 umbrella 转 Accepted）
- 2026-08-28：M2 落地完成——D1~D5 全部实现并验证（Timer0/1 模式 0/1/2 溢出派发、虚拟 µs 从时钟、配额切出 + Catch-Up、3-TU 静态初始化安全）；三端 ctest 全绿（MSVC / MinGW GCC / emcc+Node，各 6/6），layering+api lint 无 findings，ESP32 零增量。D2 配额阈值经生产 runtime tick 语义校准为 10,000µs（见 D2 落地校准）。
- 2026-08-28：**Accepted**（Owner 裁决提前独立转正，不随 umbrella 批次）——M2 已端到端证明决策稳定，且 M4 外设 trap 需消费本时钟契约，Layer-① 不应继续缺失真相。已回写 `04-wasm-simulation/02-mechanisms/02-virtual-clock.md` §6 与 `03-scheduler-and-concurrency.md` §3.2。ADR-0070 umbrella 保持 Proposed 至 M6 总验收。
