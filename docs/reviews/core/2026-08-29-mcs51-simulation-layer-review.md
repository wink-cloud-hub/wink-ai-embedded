# MCS-51/8051 零侵入仿真拦截层 验收评审

**评审日期**：2026-08-29
**评审对象**：
- `wink-micro-os/frameworks/mcs51/`（host/wasm 编译的 8051 仿真拦截层：SFR 代理、数据面、双时钟域、ADC0832、CMS8S78xx 片内 ADC、codegen 缝）
- `wink-micro-os/test/mcs51/`（samples / unit / wasm / apps 闭环 e2e）
- wink-tools（兄弟仓）codegen：`mcs51_board_config.py` / 模板 / `boards/mcs51_devboard.json`
- CI：`.github/workflows/pr.yml`（host 矩阵 + wasm job）

**评审视角**：资深嵌入式架构师，静态分发范式（ADR-0004）、零侵入沙箱（ADR-0035/0036）
**关联决策**：[ADR-0070](../../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md)（umbrella）、[ADR-0071](../../decisions/core/0071-sfr-proxy-rmw-edge-data-plane.md)（SFR 数据面）、[ADR-0072](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md)（双时钟域）、[ADR-0073](../../decisions/core/0073-cms8s-adc-real-register-map-supersedes-ssot.md)（CMS8S 真实寄存器图）
**关联设计/计划**：[总纲 SSOT](../../tech-designs/mcs51/2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md)、[实施计划](../../implementation-plans/core/2026-08-27-mcs51-zero-code-simulation-plan.md)、[Layer-① 活规范](../../design/02-wink-micro-os/07-mcs51-simulation-interception.md)
**前序评审**：[2026-07-28 8051 target 可移植性评审](2026-07-28-8051-target-portability-feasibility-review.md)（方向重评：wink 本体不跑 8051 真机 → 仿真拦截层）

---

## 一、总体结论

**通过（轨 A 验收）**。M0–M6 轨 A 全部落地：未修改 Keil C51 用户源码经正则清洗 → C++17 沙箱，在 MSVC/MinGW host 与 emcc/wasm+Node 三端编译运行；SFR 影子代理数据面、双时钟域配额补账、ADC0832 3 线 FSM、CMS8S78xx 片内 ADC 真实寄存器图、板级 codegen 缝与 NTC 闭环 + 开路/短路安全态均有端到端证据。架构红线全部守住，ESP32 真机零增量。

轨 B（UniSim `thermal_heater_plate` 插件连续热平衡、夜间长跑）**不阻塞本验收**，另立后续。

**测试证据（2026-08-29）**：MSVC host mcs51 **18/18**、MinGW host **18/18**、emcc/wasm+Node **7/7**（`-sERROR_ON_UNDEFINED_SYMBOLS=1`）；`wink lint --pack layering --pack api` 无发现；tier-b 未修改原厂 StdDriver `adc.c` 编译运行通过。

---

## 二、六条架构红线核验

| # | 红线 | 核验 | 证据 |
|---|---|---|---|
| 1 | **源码零侵入**：Keil `.c` 不改一字，方言经清洗 + REGX52.H 擦除 | ✅ | `mcs51_cleanup.py` 仅在构建树生成 `.cpp`（源只读不入库）；7 个样例（blinky…iron_ntc）+ tier-b 原厂 `adc.c` 均未修改编译；`interrupt N`→`WINK_ISR(N)` 正则重写 |
| 2 | **ESP32 真机零增量** | ✅ | `frameworks/mcs51/CMakeLists.txt:15` 顶部 `if(ESP_PLATFORM) return()`；全部新增为 test/sample + 已门控的 frameworks，不入固件链接图 |
| 3 | **静态分发 / 无虚表** | ✅ | 外设实例为 POD 结构 + 命名 API（`mcs51_adc0832_init(...)`）；4 大 C 边界 `extern "C"`；无 vtable / `container_of`（ADR-0004/0035） |
| 4 | **无 `-fpermissive` / 无硬编码 GBK** | ✅ | 沙箱 `-std=c++17` 严格编译；源 UTF-8；GBK 仅作 vendor 夹具读入到 UTF-8 构建副本的回退解码（`read_source`/`--transcode`） |
| 5 | **Trap 四红线**（零延时 / 禁 yield / 纯状态机 / 时钟解耦） | ✅ | ADC0832/边沿 trap 在引脚写读语句内同步迁移，0µs、不推进虚拟时间、不调度；ISR 上下文充时间但绝不让出（catch-up 重入死锁防护） |
| 6 | **SIMULATION 隔离置于最底层** | ✅ | 仿真分叉在 PAL/scheduler 层（`SIMULATION=1` 选 fiber 路径）；协议/用户代码同源被测试；mcs51 树整体 sim-only |

分层门禁：`wink lint --pack layering --pack api` **无发现**。

---

## 三、三个 Spike 结论落地核验

| Spike | 结论 | 落地 |
|---|---|---|
| **S1 yield API** | 裸 `while(1){}` 冻结宿主；须协作让出 | `_nop_()`→`wink_mcs51_microstep()` 充 5µs 并 yield；配额片 10,000µs（一个 100Hz 主 tick）耗尽 duration-0 让出；catch-up 补账保 1:1 时间守恒（ADR-0072）。blinky/timer0/iron_ntc 紧超循环长时运行不冻结、无 8002 |
| **S2 三编译器方言链** | MSVC / GCC(MinGW) / emcc 均可吃清洗后 TU | C++17；`inline WinkSfr/WinkXsfr` ODR 安全；GCC 无 `-Wmacro-redefined`（原厂逐字 token 间距）、vendor 头 SYSTEM include 抑警、MSVC `/wd4005`；三端全绿 |
| **S3 codegen/thermal 契约** | 固件期只固化静态常量；热参数属 device-tree | `mcs51_board_config.h` 只出引脚/通道/设定点（C4）；bridge `__has_include` + `MCS51_HAS_ADC0832` 编译期自动绑定；热动力学 tau/watts/beta/R25 不入固件（轨 B 插件） |

---

## 四、七条验收标准核验（总方案 §8）

1. **零改动三端编译**：未修改 Keil 源码在 MSVC/MinGW/emcc 三端编译运行 —— ✅ 18/18 + 18/18 + 7/7。
2. **死循环不冻结**：协作 fiber + 配额让出 + catch-up —— ✅ blinky/timer0/iron_ntc 超循环跨多次 `wink_runtime_run()` 正常推进。
3. **RMW 零虚假边沿**：Read-Latch vs Read-Pin 隔离、diff=old^val 边沿分发 —— ✅ `test_sfr_rmw_latch_integrity`、`test_sfr_edge_dispatch_accuracy`、`test_sfr_operators_coverage`、`test_mcs51_rmw_isolation` 全绿；外设不写影子锁存。
4. **电热闭环 + 安全态**：NTC 码→查表→继电器翻转，开路/短路安全 —— ✅ `iron_ntc` 经 codegen 缝自动绑定 ADC0832（驱动不调 `mcs51_adc0832_init`）；冷(200)→加热 ON、热(20)→OFF、开路(255)→fault1 断加热、短路(0)→fault2 断加热，host 与 Node 双跑 PASS。（注：连续热平衡 = 轨 B。）
5. **CMS8S 穿透**：片内 12-bit ADC 真实寄存器图 0 周期模型 + 原厂 StdDriver tier-b —— ✅ `test_cms8s_adc_instant`（自清/装载/向量 19/ADEN/AN25/AN63/XSFR）、cms8s e2e、`test_mcs51_cms8s_vendor`（未修改原厂 adc.c）全绿。
6. **ESP32 零增量**：见红线 2 —— ✅。
7. **STRICT 双态**：不支持特性 release 告警一次 / STRICT assert+abort —— ✅ `test_mcs51_shims` 覆盖 release warn-once；XSFR/xdata OOB STRICT 抽测 assert+abort。

---

## 五、遗留与后续（不阻塞本验收）

- **轨 B**：UniSim `thermal_heater_plate` 插件（连续热积分 `step(dtUs)`，`analog_knob` 为结构模板）、wasm 热平衡验证、夜间长跑、故障上电锁存。
- **tier-c**：完整原厂 ADC_Ldo 例程（需 system.h/gpio.h shim + 19 个 ISR 桩）、AN63 内部通道（BGR/温度/VDD）模型、ADCLDO VSEL 对满量程影响。
- **工具链发版协调**：`mcs51_board_config.py` 已在 wink-tools 源码（commit 22697fed）；需随 winkcli 发版后 CI wasm job 的 iron_ntc 用例才激活（当前 CMake 以生成器 `EXISTS` 夹具门控，缺失则优雅跳过，其余 wasm 测试照常绿）。

---

*本评审为 Layer-④ 时间点快照，归档后只读。*
