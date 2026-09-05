# ADR-0070：MCS-51/8051 兼容采用「零代码仿真拦截层」（frameworks/mcs51）而非真机 Port

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-08-29）** |
| **日期** | 2026-08-27（Accepted 2026-08-29） |
| **触发** | 8051 生态兼容需求（经典 Keil C51 教学工程、小家电温控固件）与 [2026-07-28 8051 可移植性审计](../../reviews/core/2026-07-28-8051-target-portability-feasibility-review.md) 的方向重评：原审计按「wink 真机 port 到 8051」推进，结论需 C90 大降级；架构重评定论 **wink 本体不跑 8051 真机**。 |
| **影响范围** | 新增 `wink-micro-os/frameworks/mcs51/`（host/wasm 编译的仿真拦截层）、`wink-micro-os/test/mcs51/`；wink-tools（兄弟仓）codegen/lint/CLI；CI 矩阵；不触碰 PAL/DAL 公开 API 与任何真机 target。 |
| **决策者** | 项目 Owner（架构评审收敛，AD-1~AD-18） |
| **关联 ADR** | [ADR-0035](0035-arduino-compat-polymorphism-sandbox.md)（Arduino 兼容沙箱，同构范式）、[ADR-0036](0036-cpp-subset-compilation-policy.md)（C++ 子集编译策略）、[ADR-0004](0004-static-dispatch-vs-runtime-ops.md)（POD + 静态分发）、[ADR-0057](0057-pal-adc-subsystem-and-channel-3-analog-contract.md)（通道 3 模拟量 Pull）、[ADR-0012](0012-contract-honesty-over-silent-degradation.md)（不支持清单须显式报错）；卫星 ADR：[ADR-0071](0071-sfr-proxy-rmw-edge-data-plane.md)（SFR 代理数据面）、[ADR-0072](0072-dual-clock-domain-and-quota-catchup.md)（双时钟域与配额补账）、[ADR-0073](0073-cms8s-adc-real-register-map-supersedes-ssot.md)（CMS8S78xx 片内 ADC 真实寄存器图） |
| **关联计划** | [`2026-08-27-mcs51-zero-code-simulation-plan.md`](../../implementation-plans/core/2026-08-27-mcs51-zero-code-simulation-plan.md) |
| **关联技术设计** | `docs/tech-designs/mcs51/2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md`（总纲 SSOT）、同域 SFR/时钟/用户手册三份规格书 |

---

## 1. 背景（Context）

WinkMicroOS 的 MCU 兼容存在两条正交轴：

| 轴 | 目录 | wink 本体由谁编译 | 跑什么 | 现有实现 |
|---|---|---|---|---|
| **A. 真机 port** | `targets/<mcu>/` + `osal/<port>/` | MCU 原厂工具链（C99/C11） | wink 真跑在芯片上 | esp32 / host / wasm / baremetal |
| **B. 仿真拦截层** | `frameworks/<eco>/` | 宿主 / emscripten（C++17） | 外国生态的用户代码跑在 wink 之上 | `frameworks/arduino/` |

8051（Keil C51）语言基线是 ANSI C89/C90 + 大量方言（`sfr/sbit/data/xdata/interrupt/using/_at_/code`），无 `<stdint.h>`、无 `inline`、无 64 位整数；PAL 是 32 位导向的丰富 HAL（mcpwm/pcnt/rmt/dma）。若走轴 A 把 wink 移植到 8051，需全仓 C90 降级且资源根本不可行。

而业务真实诉求是：**让海量现有/AI 生成的 8051 业务代码在浏览器/宿主 UniSim 中高保真运行**（教学演示、小家电温控仿真），不是在 8051 芯片上跑 wink。

## 2. 方案比选（Options）

| 方案 | 描述 | 优 | 劣 | 结论 |
|---|---|---|---|---|
| **A. 真机 port（targets/mcs51 + SDCC/Keil）** | wink 本体经 C90 兼容层编到 8051 | 真机能跑 | 需手写 stdint/stdbool、去 64 位、去 float、PAL 大面积砍；8051 资源（256B RAM/数 KB Flash）不可能承载 wink 内核 | ❌ 否决（原审计方向错误） |
| **B. 仿真拦截层（frameworks/mcs51）** | 对标 arduino 层：用户 51 代码 `-x c++` 编进宿主/wasm，方言由宏 + C++ 代理 + Fiber 承接 | 零侵入、零工具链负担、复用 UniSim 全部物理设施；wink 本体一行不改 | 只做功能级时序，不模拟 12-T 指令周期；亚微秒协议不保真 | ✅ **采纳** |
| C. 要求用户手工移植到 wink API | 改写业务代码 | 无新层 | 违背零代码/低代码定位，教学工程不可行 | ❌ |

关键洞察：**C90 是现代 C/C++ 的严格子集**。用户 Keil C 代码（C90 + 方言）在方言被宏与代理对象映射后，于宿主编译器下天然合法——方向是 shim 把用户方言**向上提升**，而非把 wink **降级**到 C90。

## 3. 决策结论（Decision）

### D1. 双轴模型与非目标
- 8051 支持 = 轴 B 新建 `frameworks/mcs51/`，与 `frameworks/arduino/` 同构：host/wasm 编译为 `wink_mcs51_compat` 静态库；**ESP_PLATFORM 下整树不编入**（真机固件零增量，CI size 守卫）。
- **非目标**：wink 不跑 8051 真机；不引入 SDCC/Keil toolchain；不做 C90 降级。轴 A 真机加固（caps 自注入、`PAL_HAS_*`、原子三档、静态 ringbuf，为 STM32/Cortex-M0 铺路）剥离至独立姊妹计划，与本决策零依赖。

### D2. 芯片范围分阶段（AD-3）
- 首版仅支持经典 **AT89C52**（通用基础）与中微 **CMS8S**（小家电片内 12-bit ADC 专项，AD-9）。STC8H 等增强 8051 后续另立。

### D3. 真·源码级零侵入（AD-1）
- 用户源码保持 `sbit P1_0 = P1^0;`、`void Timer0_ISR(void) interrupt 1` 原样。
- 唯一自动变换：CMake 构建期**正则清洗 Pass** 将 `void <name>(void) interrupt <num>` 替换为 `WINK_ISR(<num>)`；严格模式匹配，不匹配则 FATAL_ERROR 提示；清洗副本输出到 build dir，**严禁原地改源码**。
- `sbit name = 0xXX` 绝对位地址语法封禁（展开退化为 int 静默失效），由 CMake Linter 报错并提示改用 `REG^n`。

### D4. C++17 沙箱 + 全量 .cpp + 四大 extern "C" 边界（AD-4/AD-11，依 ADR-0035/0036）
- 用户 51 C 源码以 `-x c++ -std=c++17` 编译（支撑操作符重载与 C++17 inline 变量消除 ODR 冲突；C++14 环境可平替 `constexpr` 内部链接）。
- `frameworks/mcs51/src/` **全量 `.cpp`**，杜绝 .c/.cpp 混排。
- 仅四处 C 语言边界：① `main` 重映射（`#define main wink_mcs51_user_main` + extern "C" 前向声明，dcl.link 赋 C 链接）；② `WINK_ISR(n)` 静态注册器；③ `mcs51_trap.h` 的 C-ABI POD 陷阱表；④ `wink_app_get_callbacks()` + `mcs51_adc.h` 物理注入契约。
- 编译标志：GCC/Clang/emcc 用 `-Wno-write-strings -Wno-pointer-sign`；**严禁 `-fpermissive`**；MSVC 用 `LANGUAGE CXX` 属性 + 分编译器标志（实施计划 Spike-S2 裁决）。禁硬编码 `-finput-charset=GBK`，编码由 wink-tools 前置无损探测转 UTF-8。

### D5. 运行模型：入口重映射 + Fiber 协程
- 用户 `void main(void)` 内的裸机 `while(1)` 经 main 重映射后，由 `mcs51_runtime.cpp` 在 `app_init` 经 `sim_scheduler_register()` 注册为独立 fiber；`app_loop` 驱动协作式调度。
- 隐式让出：`_nop_()`、延时、超时状态位读触发让出；紧凑空轮询由时间片配额强制切出（详见 ADR-0072）。

### D6. 仿真时序精度：功能级（AD-2）
- 仅模拟 GPIO 状态、定时器 ms/µs 级中断周期与外设功能行为；**不模拟 12-T 指令机器周期**。亚微秒 bit-bang 协议（WS2812/1-Wire）、RC 充放电测温、PSW ALU 标志、计算地址访问 SFR、`#pragma asm`、`_at_` 绝对定位、generic 3 字节指针宽度假设均列入**不支持清单**：`WINK_SIM_STRICT` 下 assert，release 下降级 `pal_log_w` 告警（ADR-0012 诚实契约）。完整清单见用户手册。

### D7. 串口双落点（AD-5）
- SBUF/SCON 模型：host 重定向 stdout；wasm 经 Emscripten JS 桥接浏览器 Console/虚拟终端。

### D8. CI 门禁（AD-6）
- CI 矩阵必须包含 `test_mcs51_host`（GCC + MSVC）与 `test_mcs51_wasm`（emcc + Node），每次 MR 自动验证 blinky 与 iron_ntc；nightly 加 esp32 零增量 size 守卫。

### D9. 外接 ADC 首版锁定 ADC0832（AD-7）
- 经典 89C52 片内无 ADC，小家电教学套件普遍用 ADC0832（8-bit 串行）。首版仅支持 ADC0832；PCF8591 等 I2C ADC 遵循 YAGNI 后续按需。
- ADC0832 支持 4 线独立与 **3 线 DIO 并联复用**（DI/DO 共线），由输入/输出阶段隔离状态机保证双向时序（AD-15，细节见时钟域规格书 §3）。

### D10. 板级描述 SSOT + 构建期 Codegen（AD-10）
- 应用硬件拓扑单一事实来源为应用目录 `wink-app.json`；wink-tools `wink build sim` 双向派发：① 固件编译期生成 `mcs51_board_config.h`（静态包含，零运行时开销、零 JSON 依赖）；② 仿真运行期导出 `unisim-assets/device-tree.json` 供 UniSim 前端加载。

### D11. 模拟量通道 3 标准 Pull 双轨（AD-8，依 ADR-0057）
- 生产（wasm）：`mcs51_adc_get_value(ch)` 底层调 `js_pal_adc_read_norm(pin)` 拉取 [0,1] 归一化值折算码值（ADC0832 0~255，CMS8S 0~4095），零专有 JS 胶水。
- 测试（host/CI）：`mcs51_adc_set_value(ch, raw)` 注入轨；保留 `mcs51_adc0832_set_value` inline shim。用户业务代码零感知。

### D12. 数据面与时钟面决策
- SFR 影子内存、WinkSfr/WinkSfrBitProxy 代理、diff 边沿感知、Read-Latch vs Read-Pin、线性引脚映射即时通知（AD-12/16/18）→ **见 [ADR-0071](0071-sfr-proxy-rmw-edge-data-plane.md)**。
- 双时钟域 1:1 映射、配额守恒、Catch-Up 补账、即时外设 0µs（AD-14/17）、Trap 四红线（AD-13）→ **见 [ADR-0072](0072-dual-clock-domain-and-quota-catchup.md)**。

### AD-1~18 追溯表

| AD | 决议 | 落点 |
|---|---|---|
| AD-1 | 源码零侵入 + 正则清洗 | 本 ADR D3 |
| AD-2 | 功能级时序精度 | 本 ADR D6 |
| AD-3 | 芯片范围分阶段（AT89C52 + CMS8S） | 本 ADR D2 |
| AD-4 | C++17 `-x c++` | 本 ADR D4 |
| AD-5 | 串口 host/wasm 双落点 | 本 ADR D7 |
| AD-6 | CI 双矩阵门禁 | 本 ADR D8 |
| AD-7 | 外接 ADC 首版仅 ADC0832 | 本 ADR D9 |
| AD-8 | 通道 3 Pull 双轨模型 | 本 ADR D11 |
| AD-9 | 增强 51 首发 CMS8S | 本 ADR D2 + 总纲 §6.2 |
| AD-10 | wink-app.json SSOT + Codegen | 本 ADR D10 |
| AD-11 | 全量 .cpp + 4 大 C 边界 | 本 ADR D4 |
| AD-12 | diff 边沿感知 + Read-Latch | ADR-0071 |
| AD-13 | Trap 四红线 | ADR-0072 |
| AD-14 | 1:1 硬实时时钟映射 | ADR-0072 |
| AD-15 | ADC0832 3 线 DIO 状态机 | 本 ADR D9（细节时钟规格书 §3） |
| AD-16 | inline WinkSfr + 花括号初始化 | ADR-0071 |
| AD-17 | 空转配额切出 Catch-Up 补账 | ADR-0072 |
| AD-18 | 线性引脚映射 + GPIO 即时通知 | ADR-0071 |

## 4. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| 8051 业务代码零改动在 host/wasm 高保真仿真，复用 UniSim 物理引擎与前端动画 | 需维护 frameworks/mcs51 沙箱层（SFR 代理、定时器/UART/ADC 模型）与 wink-tools codegen |
| wink 本体、PAL/DAL、ESP32 固件零影响；不引入任何 8 位工具链 | 时序仅功能级：亚微秒协议、RC 测温、PSW 标志等明确不支持，须显式报错而非静默错误 |
| 与 arduino 层同构，架构模式可复用于未来其他 MCU 家族（Avr/STM32 duino） | 用户源码须为标准 C89/C99（K&R、隐式原型不支持）；GBK 工程需前置转 UTF-8 |
| 静态分发 POD 陷阱表，无虚表、无动态分配，符合 ADR-0004 | Trap 开发须守四红线（零延时/禁 yield/纯状态机/时钟解耦） |

## 5. 遵循与后续（Compliance & Follow-up）

- 实施按 [`2026-08-27-mcs51-zero-code-simulation-plan.md`](../../implementation-plans/core/2026-08-27-mcs51-zero-code-simulation-plan.md) M0~M6 执行；M0 三 Spike（yield API、三编译器方言链、codegen/thermal 契约）先行退役风险。
- Accepted 后回写 Layer-①：`docs/design/02-wink-micro-os/` mcs51 仿真拦截小节、`docs/design/03-app-codegen/` codegen 小节（M6 填实）。
- 验收：总方案 §8 七条标准（零改动三端编译、死循环不冻结、RMW 零虚假边沿、电热闭环 + CMS8S 穿透、ESP32 零增量、STRICT 双态）。

**验收证据（2026-08-29，M0–M6 轨 A 完成）**：未修改 Keil C51 用户源码经正则清洗 → C++17 沙箱，在 MSVC/MinGW host 与 emcc/wasm+Node 三端编译运行；host mcs51 ctest **18/18**、wasm/Node **7/7** 全绿（含 M1 blinky、M2 Timer0 ISR、M3 UART/GPIO/shims、M4 ADC0832 + RMW 数据面、M5 CMS8S78xx 片内 ADC + 原厂 StdDriver tier-b、**M6 iron_ntc NTC 闭环**：板级 codegen 缝自动绑定 ADC0832，冷/热 bang-bang 翻转继电器 + 开路/短路安全态——验收 #4）；`wink lint --pack layering --pack api` 无发现；mcs51 整树在 `if(ESP_PLATFORM) return()` 门后，ESP32 固件零增量。轨 B（`thermal_heater_plate` 插件热平衡、夜间长跑）延后。

---

*本 ADR 状态变更请在此记录：*
- 2026-08-27：Proposed（架构评审 AD-1~18 收敛，随实施计划 M0 提交；待 M6 验收后转 Accepted 并回写 Layer-①）
- 2026-08-29：Proposed → Accepted（M0–M6 轨 A 实现与三端测试矩阵全绿——host 18/18、wasm 7/7、arch lint 无发现、ESP32 零增量；iron_ntc 闭环 + 开路/短路安全态满足验收 #4。轨 B 热插件平衡另立后续）。
