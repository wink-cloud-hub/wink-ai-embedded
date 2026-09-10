# MCS-51 仿真后端责任划分：C++ Proxy 可解 / ISS 必需 / 仿真永解不了

| 元数据项 | 说明 |
| :--- | :--- |
| **文档编号** | MCS51-BACKEND-2026-09-10 |
| **创建日期** | 2026-09-10 |
| **所属模块** | `wink-micro-os/frameworks/mcs51/`（Native C++ Proxy 功能级后端）、规划中 ISS 指令级后端、HIL/量产兜底 |
| **状态** | **Draft / 待评审**（todolist，执行前需迁移为 Layer-② 技术设计或 Layer-③ 实施计划，涉及时钟/中断语义的补 ADR）。本篇为分类框架，不直接立项；具体整改项仍以 `MCS51-GAP-2026-09-10` 为准，整合关系见 §6 |
| **审计基线** | master @ e473f35；对照 `MCS51-GAP-2026-09-10`（GAP-01~25）、`2026-09-08-mcs51-simulation-vs-silicon-fidelity-and-test-limits.md` §2~§5、`mcs51_health_pot/docs/DESIGN.md` §8 |
| **关联决策** | ADR-0012（契约诚实）、ADR-0070（C++ 拦截层）、ADR-0071（数据面代理）、ADR-0072（双时钟域与 Trap 红线）、ADR-0073（CMS8S ADC 真实寄存器图与 0 周期即时）、ADR-0076（Native/ISS 双后端路线）、ADR-0077（准双向口） |
| **关联文档** | [仿真与硅片保真度及测试方法论](../zh/tech-designs/mcs51/2026-09-08-mcs51-simulation-vs-silicon-fidelity-and-test-limits.md)、[缝隙审计与整改任务清单](./2026-09-10-mcs51-sim-vs-silicon-gap-todolist.md) |
| **目标受众** | mcs51 框架维护者、仿真引擎开发者、CI/HIL 工程师、AI 代码生成 Agent 维护者 |

---

## 0. 背景与划分原则

回答一个问题：上一轮分析中列出的「仿真绿、真机挂」缝隙，哪些在现有 C++ Proxy 后端内可修，哪些必须等 ISS 指令级解释器，哪些连 ISS 也解不了？

划分原则（对齐 ADR-0076）：

* **A 类 — C++ Proxy 可解**：不需要指令流水线，只需在 `SFR hook / poll / microstep / lint / 构建门禁` 里加**同步记账 + 语义校验 + 故障注入 + 配置声明**。成本低（天级），无性能塌陷，不违反保真度文档 §3.1（不给 Native 加异步定时器）。
* **B 类 — 必须 ISS**：需要**逐指令推进时间 + 任意指令边界异步抢占 + SDCC 真机器码语义**。Native 天生做不到，做leading indicator只能缓解不能根治。成本高（解释执行慢 1~2 数量级），按 ADR-0076 按需触发。
* **C 类 — 仿真永远解不了**：模拟/电气/机械/工艺离散性与 meter 级物理。ISS 也只能逼近，必须 HIL/量产标定/原理图核对兜底。

> 方法论红线：Native 后端内**禁止打脆弱的硬件延时补丁**（异步定时器 + 纯内存轮询 = 死锁，见保真度文档 §3.1）。A 类允许的唯一时间手段是**同步记账**（hook 内 `charge_us` + 同步完成），时间前进了、调用链不断、 fiber 不挂。

---

## 1. 分类总表

| 编号 | 类别 | 标题 | 代表 app / 证据 | 对应 GAP |
| :--- | :--- | :--- | :--- | :--- |
| [A-01](#a-01-uart-txrx-链路就绪校验) | **A** | UART TX/RX 链路就绪校验（波特率源/mux/模式/REN） | `uart_hello/echo/health_pot` 遥测；`mcs51_uart.cpp:43-59` 无条件出总线+即时 TI | GAP-02、GAP-01 |
| [A-02](#a-02-adc-同步记账分频敏感基准链) | **A** | ADC 同步记账（分频敏感）+ 基准/外电路/mux 建模 | `analog_threshold/health_pot` median-of-3；`cms8s_adc.cpp` 0 周期穿透 | GAP-05（含滤波不可证伪声明） |
| [A-03](#a-03-uart-tx-按波特率同步记账) | **A** | UART TX 按波特率同步记账（整字节发送时间 charge） | `health_pot` 22B≈23ms 真机阻塞、仿真 ~110µs | GAP-02 修复项 3、GAP-07 前置依赖 |
| [A-04](#a-04-wdt-超时复位ta-窗口收窄) | **A** | WDT 超时复位粗模型 + TA 窗口收窄 | `health_pot` 10ms 喂狗；`cms8s_sys.cpp:81-88` 自述未建模复位 | GAP-07 |
| [A-05](#a-05-gpio-方向上下拉驱动与模拟数字互斥) | **A** | GPIO 方向/上下拉/驱动参与行为 + 模拟脚数字读屏蔽 | 按键/继电器/`P00CFG=AN0`；`mcs51_gpio.cpp:99-101` 固定强度 | GAP-08、GAP-25 |
| [A-06](#a-06-复位种子foscxram-按型号描述符装载) | **A** | 复位种子/Fosc/XRAM aperture 按 MCU 型号描述符装载 | `CKCON=0x07`、`12MHz vs 24MHz`、`8KB vs 1KB`；`mcs51_context.cpp` | GAP-04、GAP-13、GAP-09、GAP-22 家族门控 |
| [A-07](#a-07-未建模寄存器-tripwire-与-shim-审计门禁) | **A** | 未建模 SFR/XSFR tripwire + shim 全量 diff + C51/SDCC 编译门禁 + STRICT 判决 | `XBYTE[0xF120]` 静默；`mcs51_shim_audit.py`；`cleanup --target=sdcc` 无消费 | GAP-23、GAP-03、GAP-10、GAP-11、GAP-12、GAP-20、GAP-21 |
| [A-08](#a-08-timer-重载开销stop-唤醒movx-总线sbuf-复写等粗粒度补齐) | **A** | Timer/功耗/总线等粗粒度补齐（重载开销、STOP 唤醒源、at89 总线占用、SBUF 复写计数） | `health_pot` T0 0xB1E0；`mcs51_pcon.cpp`；at89 `XBYTE` | GAP-14、GAP-17'、GAP-24、GAP-25、GAP-12、GAP-15 |
| [B-01](#b-01-纯内存轮询推进时间) | **B** | 纯内存轮询推进时间（`while(!flag);` 无 SFR 即冻结） | 保真度文档 §3.1 `g_adc_done` 死锁原型 | 已文档化，不在 GAP 立项 |
| [B-02](#b-02-任意指令边界异步抢占与嵌套) | **B** | 任意指令边界异步抢占、优先级嵌套、ISR latency/压栈、`tick_flag/mailbox` 撕裂 | `button_led_int`、`uart_echo`、`health_pot` ISR/主循环共享 | 已文档化（同步栈内派发），GAP-22 为其向量表前提 |
| [B-03](#b-03-亚微秒与逐周期外设时序) | **B** | 亚微秒与逐周期外设时序（bit-bang、PWM、重载抖动、外部脉冲计数） | `ADC0832`、BUZ、T2/3/4、Mode1 重载延迟 | GAP-12、GAP-14（Native 侧明确不建模） |
| [B-04](#b-04-keil-存储器与代码模型语义) | **B** | Keil 存储器与代码模型语义（`bit/using/overlay`、双 DPTR、栈深、库重入、int/char 宽度） | AI 生成代码；`health_pot` 已人工规避 | GAP-11、GAP-25（文档+门禁先行，语义级根治靠 ISS） |
| [C-01](#c-01-模拟电气链) | **C** | 模拟电气链（VREF/LDO、NTC RC、COM 驱动、继电器/蜂鸣器功率链） | `health_pot` DESIGN §8 全表 | GAP-05/GAP-06 的物理侧、§5 checklist |
| [C-02](#c-02-工艺离散与环境机械) | **C** | 工艺离散与环境机械（容差、温漂、老化、抖动分布、热惯量、EMC/ESD） | 干烧 25/60s vs 量产 120~900s；WDT RC 容差 | GAP 清单 §9.6 残余盲区 |
| [C-03](#c-03-烧录与板级事实) | **C** | 烧录与板级事实（CONFIG 字、startup 初始化、BOM/爬电/隔离、Flash 寿命） | CONFIG 选项字节；`startup.A51` | GAP-06、GAP-03（map 侧）、§5 checklist |

---

## 2. A 类详项（C++ Proxy 可解）

### A-01. UART TX/RX 链路就绪校验

* **现状**：`on_sbuf_write` 不看 TR1/TH1/`FUNCCR` 波特率源（Timer1/BRT/TMR2/TMR4）、SCON 模式、TXD mux（`P14/P22 CFG=0x03`）；RX 不看 REN/RI/mux 即灌 FIFO。
* **Proxy 解法**：写 SBUF 前跑 TX 就绪检查（4 种源全枚举 + SCON∈{1,3} + TXD CFG）；REN=1 时校验 RXD mux + `PS_RXD`；未就绪 STRICT 断言 / Release 单次告警 + 计数器暴露（喂给 GAP-10 runner 判决）。
* **accept**：TR1=0 / mux 缺失 / 模式 0 写 SBUF 触发断言；`health_pot` 遥测配置完整仍全绿。

### A-02. ADC 同步记账（分频敏感）+ 基准链

* **现状**：0 周期即时完成；`DIV_2~256` 不敏感；`ADCLDO/LDOEN/VSEL`、通道 mux、`P00CFG` 全不参与码值。
* **Proxy 解法**：`ADC_GO` hook 内按 DIV `charge_us(10~160µs)` 后同步完成（**同步记账，不是异步定时器**，无 §3.1 死锁）；码值 `raw = norm×4095×(Vrail/Vref)`，Vref 取 `ADCLDO.VSEL+LDOEN`，Vrail 由 device-tree 声明；`ADEN/LDOEN/mux` 缺失 STRICT 断言；median-of-3 抗扰在保真度文档显式声明不可证伪，靠 `mcs51_adc_set_value` 三次异值注入单测。
* **accept**：同 ratio 下 `VSEL=3V/Vrail=VDD(3.3V)` 码值不同且手算一致；DIV 改档连采节拍变化。

### A-03. UART TX 按波特率同步记账

* **现状**：22B 遥测仿真 ~110µs，真机 9600bps 约 23ms；WDT/调度风险全隐身。
* **Proxy 解法**：每字节按当前波特率 `charge_us(≈1ms@9600)` 后置 TI；`while(!TI)` 忙等自然消耗真实发送时间（忙等体内有 `_nop_`，符合 §3.1 约束，需 ADR 确认后实施）。
* **accept**：遥测帧在虚拟时间上占 23ms 量级；**本项为 A-04 的前置依赖**。

### A-04. WDT 超时复位/TA 窗口收窄

* **现状**：只验 TA 序列，不模拟复位；TA 无超时、可被打断。
* **Proxy 解法**：记录 `WDTCLR` 虚拟时间 + `WTS` 档位，microstep/catch-up 检查溢出 → 仿真复位（或 STRICT 断言 + 计数）；TA 记录 `0xAA` 时间戳 + 两 TA 间出现其他 SFR 写即失效。
* **accept**：喂狗超 `WTS` 间隔被复位/断言；`health_pot`（10ms 喂狗）不触发；最长阻塞段（含帧长/波特率）< WTS 写入 DESIGN 硬约束。

### A-05. GPIO 方向/上下拉/驱动与模拟数字互斥

* **现状**：`TRIS/UP/OD/DR/LEDSDR` 无 hook；写 1 恒 WEAK、写 0 恒 SUPPLY；`AN0` 脚仍可数字读。
* **Proxy 解法**：TRIS 门控对外通知；HiZ+上拉缺省读 1；开漏写 1 无强驱；AN mux 置位后数字读返回锁存/告警；强度携带 `DR/LEDSDR`。
* **accept**：TRIS=输入写锁存无对外通知；删 `P0UP` 配置出现显式告警。

### A-06. 复位种子/Fosc/XRAM 按型号描述符装载

* **现状**：`CKCON` 种子错（GAP-04）、时钟 12MHz 兜底跨型号污染（GAP-13）、XRAM 8KB≠1KB（GAP-09）、CMS8S 模型对 at89 无条件注册（GAP-22 家族门控）。
* **Proxy 解法**：建 per-MCU seed 描述符（`mcu` 字段索引：`AT89C52=12MHz/CKCON=0/无XSFR`、`CMS8S78xx=24MHz/CKCON=0x07/1KB`…），`mcs51_context_reset` 统一装载；外设表按型号选模型集合；XRAM aperture 从 board 注入。
* **accept**：reset 后两家族种子正确；`XBYTE[0x0400]` 在 CMS8S 配置下 STRICT 断言。

### A-07. 未建模寄存器 tripwire 与 shim 审计门禁

* **现状**：未声明寄存器 29+111 个静默落影子（GAP-23）；`cleanup --target=sdcc` 无消费（GAP-03）；Release warning 无判决（GAP-10）；`int/char` 语义未入手册（GAP-11）。
* **Proxy 解法**：XDATA 写路径加 XSFR tripwire（未注册地址计数 + STRICT 断言 + 外设名提示）；`mcs51_shim_audit.py` 四面硬比对（地址/向量/mux/复位值）进 CI；SDCC `-std-c89` 编译门禁；STRICT 双跑或 runner 计数判决；cleanup 产出 transform manifest（GAP-20）+ 实例独立回归钉防（GAP-21）。
* **accept**：`XBYTE[0xF120]` 触发断言/计数；`health_pot` 既有 XSFR 零计数；C99 中位声明被门禁拦下。

### A-08. Timer 重载开销/STOP 唤醒/MOVX 总线/SBUF 复写等粗粒度补齐

* **处置**：Mode1 重载每 tick 扣固定 `5~10µs` 或文档声明不建模（GAP-14）；STOP 补 GPIO 端口中断唤醒、WUT/LSE 标不支持（GAP-17'）；at89 下 XBYTE 占用 P0/P2/P3.6/7 告警 + IAP 粗持久化或 unsupported（GAP-24）；SBUF 未清 TI 再写计数、递归禁令、栈余量断言（GAP-25）；T2/3/4 公式参数化、重复向量告警、P0EXTIF W0C、整端口通知契约注释（GAP-12/15）。
* **accept**：各子项单测/文档合入，现有场景无回归。

---

## 3. B 类详项（必须 ISS）

### B-01. 纯内存轮询推进时间

* **机理**：Native 无 8051 流水线，非 SFR 访问恒 0µs；保真度文档 §3.1 `while(!g_adc_done);` 为原型。给 Native 加异步定时器即死锁，无解。
* **ISS 根治**：逐指令解释，`SJMP $` 亦耗机器周期，定时器事件按周期排序派发。
* **过渡**：红线手册禁纯内存忙等（必须 SFR/`_nop_` 可观测点）； lint 检出即警告。

### B-02. 任意指令边界异步抢占与嵌套

* **机理**：Proxy 是写 hook 调用栈内同步嵌套（`ADC_GO()` 内 ISR 已跑完）；优先级严格大于、RETI 抑制、`in_service` 深度均为借机派发近似。`tick_flag/echo_pending/disp_digits` 撕裂、uart burst overrun、高优先级打断低优先级永不可见。
* **ISS 根治**：指令边界查 `IE/IP + flag`，硬件压栈/弹栈，latency 建模。
* **过渡**：共享变量强制 `volatile` +临界区 `EA` 规范；A-07 门禁先行。

### B-03. 亚微秒与逐周期外设时序

* **机理**：`5µs` 微步抹掉 `0.4/0.8µs` 脉冲；T2/3/4 硬编码倒数；Mode1 重载零延迟；`C/T` 外部脉冲直调函数。
* **ISS 根治**：12T/1T 周期精确模型 + 离散事件队列。
* **过渡**：A-08 粗补 + 文档声明精度边界；WS2812/FOC 类业务直接路由 ISS。

### B-04. Keil 存储器与代码模型语义

* **机理**：Host `int=32/char=signed` vs C51 `int=16/char=unsigned`；`bit/using/overlay/双DPTR/248B栈/libc重入` 全是宿主语义。
* **ISS 根治**：SDCC 编真机器码再解释。
* **过渡**：GAP-11 定宽类型规范 + cleanup lint；GAP-03 SDCC 门禁；GAP-25 递归/库/栈清单。

---

## 4. C 类详项（仿真永远解不了）

### C-01. 模拟电气链

VREF 去耦（100nF+1µF）、NTC RC（1k+100nF）、电源纹波、ADC INL/DNL、LDO 温漂、COM 150mA 压降与三极管选型、BUZ 限流/续流、继电器拉弧寿命与线圈反电动势（1N4148）、220V 爬电（≥6.5mm）与双金属/TCO 双保险。仿真只给逻辑量，声压/亮度/烧蚀曲线永无模型。

### C-02. 工艺离散与环境机械

HSI ±1%、WDT RC 容差、晶振温漂、NTC 老化、按键真实抖动分布、水壶热惯量（容积/电压/敞盖）、环境温湿度、EMC/ESD、批次 errata、Flash 寿命。干烧判据仿真 25/60s、量产 120~900s 的差值只能靠标定。

### C-03. 烧录与板级事实

CONFIG 选项字节（HSI 路径/LVR/调试脚）、`startup.A51` 初始化、Keil `.map` 预算（16KB/256B/1KB）、BOM/原理图接法（上拉接 LDO OUT 还是 VDD）、烧录校验流程。属工程流程面，仿真无从得知。

---

## 5. 整改路线（与 GAP 清单的衔接）

| 阶段 | 本篇条目 | GAP 对应 | 产出 |
| :--- | :--- | :--- | :--- |
| 热修（已部分落地 §执行记录） | A-06、A-07（审计脚本） | GAP-01/22/04/13 + §9.5 | 直接 PR + 单测 |
| 门禁 | A-07 | GAP-03/10/09/21/23/20/11 | Layer-③ 实施计划 + CI |
| 模型保真（A 类主体） | A-01~A-05、A-08 | GAP-02/05/08/07/06/17'/24/25/12/14/15 | Layer-② 技术设计（时钟语义补 ADR） |
| 呈现与文档 | A-07（报告/AI 提示词） | GAP-19/20 | runner + 前端/codegen 改动 |
| ISS 按需 | B-01~B-04 | 保真度 §5 / ADR-0076 | ISS 通道规划（WS2812/FOC 先行） |
| HIL 兜底 | C-01~C-03 | §5 checklist、阶段 5 | 真机同源冒烟规范 |

---

## 6. 与 GAP 清单的整合说明（待你确认后执行）

* 整合结论（2026-09-10 已执行）：**GAP 主、分类辅**。GAP-01~25 编号稳定；GAP 清单 §1 总表新增 `后端归属` 列、§6 路线表新增 `后端映射` 列，本篇为标签定义源（§1 总表为反向映射）。正文不复制，保持单源。
* 已知重叠：A-01↔GAP-02、A-02↔GAP-05、A-03↔GAP-02 修 3、A-04↔GAP-07、A-05↔GAP-08/25、A-06↔GAP-04/13/09/22、A-07↔GAP-23/03/10/11/12/20/21、B 类↔保真度已文档化差异 + GAP-12/14/22 前提、C 类↔§5 checklist + §9.6。
