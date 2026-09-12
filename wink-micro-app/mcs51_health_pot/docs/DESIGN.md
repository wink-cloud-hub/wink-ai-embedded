# mcs51_health_pot —— CMS8S78xx 商用养生壶仿真与固件设计规范

> 平台：WinkMicroOS · CMS8S78xx 1T 8051 硬件全特性平台（Axis-B，ADR-0070~0078）  
> MCU 模型：中微半导体 CMS8S78xx 1T 高速 8051 内核（Flash 16KB, SRAM 1KB, 片内 12-bit SAR ADC, 硬件蜂鸣器发生器 BUZCON/BUZDIV, 150mA 高驱动 COM 口）  
> 仿真通道：CH1 GPIO（4COM-8SEG 动态扫描、按键、双冗余 LED、加热继电器）· CH2 UART（遥测）· CH3 模拟量（NTC→片内 12-bit ADC）· BUZCON 硬件无源蜂鸣器 PWM  
> 固件：`health_pot.c`（Keil C51 风格，经 `transpile_app_keil_c51.py` 转译为 C++17 原生/wasm 编译，支持双 target 同源编译）  

---

## 1. 功能概述

本应用基于中微半导体 **CMS8S78xx** 芯片进行全栈商用级重构，充分挖掘其芯片专属硬件外设能力（片内 12-bit SAR ADC、硬件无源蜂鸣器发生器 BUZCON、大电流驱动 GPIO），实现一款产品化、专业级养生壶小家电控制固件。

| 功能 | 说明 |
|---|---|
| **烧水模式（HEAT）** | 支持 **55 ℃ 直热温水**（`MODE_DIRECT_55`）、**80 ℃ 泡茶**（`MODE_DIRECT_80`）与 **100 ℃ 沸腾**（`MODE_BOIL_100`，98 ℃ 沸腾确认 3 s 后转入保温）三大经典档位。加热过程中**允许按 FUNC 键动态变轨**：在 55℃ -> 80℃ -> 100℃ -> 55℃ 之间秒级循环切换；向下切换时若水温已超标则立即切断发热盘转入保温；数码管实时显示水温与目标标识（100 ℃ 显示 `"XXb0"`，55 ℃ 显示 `"XX55"`，80 ℃ 显示 `"XX80"`，小数点仅在继电器吸合加热期间以 1 Hz 呼吸闪烁）；沸腾/到温完成播放 4 音和弦旋律，平滑转入保温 |
| **保温模式（WARM）** | 目标温度 55 / 60 / 80 / 90 ℃ 四档，按 FUNC 键循环切换；得益于 12-bit ADC 高精度，采用 **±1 ℃ 窄带迟滞 bang-bang 控温**（`WARM_HYST_C = 1`），相比传统 8-bit 的 ±3 ℃ 大幅提升水温稳定性；数码管显示 `"XXYY"`（前两位实时水温，后两位设定水温） |
| **关机待机（OFF）** | 任意工作状态按 ON/OFF 键安全关断发热盘，回到待机状态；数码管默认显示 `" -- "`（若预选 55 ℃ 显示 `"-55-"`，若预选 80 ℃ 显示 `"-80-"`），外设进入待机节能节拍。**待机时按 FUNC 即可三档预选加热模式**。**传感器故障（E-01/E-02）下 ON/OFF 同样有效**：人工确认后进入「静音待机」（显示 `" -- "`、停止声光报警、继电器保持断开，故障码仍经遥测 F 字段上报），探头未恢复时再次按 ON/OFF 开机会被**拒绝并重新弹出故障**，读数连续 300 ms 有效后自动清除为普通待机 |
| **按键交互与极速响应** | ON/OFF（P0.4）、FUNC（P0.5），**10 ms 边沿即时响应架构（<10 ms 零感延迟）**：按下的同一 tick 内立即执行模式切换并刷新显示缓冲区，解决传统 100 ms 慢轮询的迟钝感；支持边沿锁存滤波，杜绝网页仿真端鼠标快速连击被吞键；待机与加热中按 FUNC 支持 55℃->80℃->100℃ 循环变轨；任意时刻按 ON/OFF 一键急停关机 |
| **多音调硬件声学（BUZCON/BUZDIV）** | 充分利用 CMS8S78xx 片内专属硬件无源蜂鸣器发生器（P0.3，BUZCON 分频控制与 BUZDIV 频率寄存器），实现零 CPU 占用、免定时器中断占用的多音调声效：<br>① **按键音（Key Click）**：4000 Hz, 30 ms<br>② **功能升调（Step Up）**：2000 Hz (40 ms) → 3000 Hz (40 ms)<br>③ **沸腾完成旋律（Boil-Done Chord）**：C6(1042 Hz) → E6(1320 Hz) → G6(1562 Hz) → C7(2106 Hz) 4 音上行分解和弦<br>④ **探头恢复提示（Recover Chime）**：G6(1562 Hz, 100 ms)<br>⑤ **故障急促警报（Warble Alarm）**：3000 Hz / 2000 Hz 急促交替双音报警（进入 1 s + 每秒 100 ms 短促警示，**60 s 超时自动静音**，避免扰民）<br>⑥ **无效操作否定音（Busy Blip）**：~800 Hz / 30 ms 短促低音，FUNC 在待机/加热态按下或传感器故障下尝试开机被拒时给出 |
| **4COM-8SEG 数码管显示** | 利用 CMS8S78xx 强驱动口（COM0..COM3 在 P3.0..P3.3 灌电流，SEG a..dp 在 P1.0..P1.7 推挽）实现 4 位 8 段共阴数码管分时动态扫描（Timer0 10 ms ISR 轮巡）：<br>• 待机态：`" -- "`<br>• 加热态：`"25b0"`（水温 25 ℃，b0 沸腾加热，dp 点 1 Hz 闪烁）<br>• 保温态：`"9860"`（水温 98 ℃，目标设定 60 ℃）<br>• 故障态：国际家电标准故障码 `"E-01"`（开路）、`"E-02"`（短路）、`"E-03"`（干烧）、`"E-04"`（超温） |
| **双冗余状态指示灯** | 独立保留 3 颗板载 LED（加热红灯 P0.1、保温绿灯 P0.2、故障红灯 P0.6），与数码管形成工业级双重冗余视觉指示；故障灯以 1 Hz 闪烁警示 |
| **片内 12-bit SAR ADC 测温** | 废除过时外置 ADC0832 3 线 bit-bang 方案，全面切换到 CMS8S78xx 片内 12-bit SAR ADC（AN0/P0.0），配置片内 3.0V 高精 LDO 参考电压；内置 **中值滤波（median-of-3）**，有效滤除发热盘通断引起的电源纹波尖峰；兼容标称输入与硬件 12-bit 原始码 |
| **继电器 Dwell 触点保护** | 加热器**任何再吸合路径**（保温 bang-bang、快速 OFF→ON 电源重启、沸腾后再热）均受 `RELAY_DWELL_SECONDS = 3s` 门控（真机典型 30~60 s），防止机械继电器在短间隔内频繁拉弧烧蚀；**所有断开边沿（关机/故障/沸腾到温）一律 0 延迟即时切断** |
| **五级安全故障防护** | ① **NTC 开路**（E-01）：12-bit 原始码 ≥ 3900（分压轨拉至满量程）<br>② **NTC 短路/脱落**（E-02）：12-bit 原始码 ≤ 8（约 ≥120 ℃ 的零码带）；任何运行态下短路即切断，恢复目标为 OFF（**绝不自动重启加热**）<br>③ **干烧保护一级**（E-03）：加热器连续通电 25 s 水温仍 < 45 ℃（仿真加速值，真机 120~180 s）<br>③' **干烧保护二级 / 沸腾超时**（E-03）：加热器连续通电 60 s 水温始终未达 98 ℃（覆盖敞盖散热、低电压、半壶水等越过 45 ℃ 后停滞的工况；仿真加速值，真机标定 600~900 s）<br>④ **超温保护**（E-04）：连续 10 s 原始码 ≤ 16（水温 > 105 ℃）；越界码期间**强制断开继电器且不允许完成沸腾确认**，杜绝"超温假沸腾"<br>**热故障 Sticky 锁存机制**：热故障（E-03/E-04）一旦锁存优先级最高，后续任何读数（含探头短路/开路）都不得降级或自清除，必须人工按 ON/OFF 键解除；同类传感器故障码允许实时刷新（开路↔短路）；探头故障（E-01/E-02）可由人工 ON/OFF 确认进入静音待机，或在读数连续 300 ms 有效后自动恢复至 OFF 态 |
| **隐式 POST 上电自检** | 控制任务在每次 100 ms 周期中，**探头安全检查先于状态机事件转移**；若探头在上电前已损坏，首个控制周期即进入 FAULT 并关断继电器，消除上电误开机安全盲区 |
| **串口遥测输出** | 格式：`T=<温度>C,S=<状态>,H=<继电器>,F=<故障码>\n`，兼具 headless `ASSERT_BUS_PAYLOAD` 自动化测试与上位机产测标定 |

---

## 2. 硬件清单（BOM）与 CMS8S78xx 外设映射

| # | 器件名称 | 型号 / 规格 | 硬件功能 | CMS8S78xx 硬件外设 | 仿真映射机制 |
|---|---|---|---|---|---|
| 1 | **主控 MCU** | **CMS8S78xx**（LQFP48 / TSSOP28） | 核心控制 | 1T 8051 内核，24 MHz，16KB Flash，1KB SRAM | `cms8s78xx_devboard` + `mcu: "cms8s78xx"` |
| 2 | **温度采样** | NTC 热敏电阻（10kΩ @ 25℃，B=3950） | 水温检测 | 片内 12-bit SAR ADC + 片内 3.0V LDO 基准 | CH3 模拟量轨物理 Pin 0（AN0，v2 rail key），内部寄存器 `ADCON0/1` 拦截 |
| 3 | **数码显示** | 4 位 8 段共阴数码管（0.36 英寸） | 温度与状态显示 | P3.0..P3.3（高灌电流 COM0..3）+ P1.0..P1.7（SEG a..dp） | `seg_display` 插件（`direct_gpio_4d`，含视觉暂留 POV 解码） |
| 4 | **声音单元** | 压电式无源蜂鸣器（Passive Buzzer） | 多音调按键/旋律/警报 | 片内专用硬件蜂鸣器发生器（`BUZCON` + `BUZDIV`，输出脚 P0.3） | `buzzer` 插件（`passive_pwm`，频率/占空比动态分析） |
| 5 | **发热执行** | 发热盘（1000W）+ 机械继电器 / 固态 SSR | 煮水与保温执行 | GPIO P2.0 推挽输出（高电平吸合） | `led` 插件（`heater_relay`，`active_high: true`） |
| 6 | **状态指示** | 高亮贴片 LED ×3（红、绿、红） | 加热 / 保温 / 故障冗余指示 | GPIO P0.1 / P0.2 / P0.6（低电平点亮） | `led` 插件（`led_heat / led_warm / led_fault`） |
| 7 | **操作按键** | 轻触开关 ×2（开关接地，内部上拉） | ON/OFF 电源、FUNC 功能切换 | GPIO P0.4 / P0.5（低电平有效） | `button` 插件（`btn_onoff / btn_func`） |
| 8 | **通信遥测** | UART 接口（TXD=P2.2，RXD=P2.1，引脚重映射） | 遥测与调试 | 片内 UART0 + Timer1 mode-2 波特率发生器（T1M=Fosc/4，SMOD=1，TH1=0xD9 → 9615 bps） | CH2 UARTBus TX 时间线捕获（SBUF 拦截，与引脚无关） |

---

## 3. 引脚分配（Pinout）

引脚分配遵循 CMS8S78xx 推荐小家电开发板引脚布局：

| 引脚名称 | 线性号 | 方向 | 硬件外设功能 | 有效电平 | 功能描述 |
|---|---|---|---|---|---|
| **P0.0 / AN0** | 0 | 模拟输入 | 片内 ADC 通道 0 | 0~3.0 V | NTC 热敏电阻测温输入 |
| **P0.1** | 1 | 推挽输出 | GPIO | **低有效** | 加热指示灯 LED_HEAT（0=亮） |
| **P0.2** | 2 | 推挽输出 | GPIO | **低有效** | 保温指示灯 LED_WARM（0=亮） |
| **P0.3 / BUZ** | 3 | 硬件输出 | 硬件蜂鸣器发生器 | 50% 矩形波 | 无源蜂鸣器硬件 PWM 输出 |
| **P0.4** | 4 | 准双向上拉 | GPIO / EXTINT | **低有效** | ON/OFF 电源按键 |
| **P0.5** | 5 | 准双向上拉 | GPIO / EXTINT | **低有效** | FUNC 功能选择按键 |
| **P0.6** | 6 | 推挽输出 | GPIO | **低有效** | 故障指示灯 LED_ERR（1 Hz 闪烁） |
| **P1.0..P1.7** | 8..15 | 推挽输出 | 强驱动 SEG 端口 | **高有效** | 数码管段码 a, b, c, d, e, f, g, dp |
| **P2.0** | 16 | 推挽输出 | 继电器驱动 | **高有效** | 发热盘加热继电器 HEATER（1=吸合） |
| **P2.1 / RXD** | 17 | 数字复用 | UART0 RXD（引脚重映射） | TTL | 串口接收（预留产测下行） |
| **P2.2 / TXD** | 18 | 数字复用 | UART0 TXD（引脚重映射） | TTL | 串口遥测（9615 bps，误差 0.16%） |
| **P3.0..P3.3** | 24..27 | 开漏/强灌 | 高灌电流 COM 端口（专用，不与 UART 复用） | **低有效** | 数码管位选 COM0, COM1, COM2, COM3 |

---

## 4. 软件架构设计

### 4.1 任务调度模型（超级循环 + Timer0 硬件时基）

```
Timer0 ISR（10 ms 周期）             主循环 main while(1)
┌──────────────────────────┐        ┌───────────────────────────────────────────────┐
│ 1. 重装 TH0/TL0 (0xB1E0) │        │ _nop_() 协作让步（Wasm 协程推进与事件汇聚）    │
│ 2. 消影：P3 COM 全灭      │ ──置位──▶ │ 每 10 ms：按键扫描消抖 button_scan()           │
│ 3. 输出段码/选通当前 COM  │ tick_flag │ 每 10 ms：蜂鸣音序器 buzzer_task()（前台推进） │
│ 4. scan_idx 前进 0..3    │        │ 每 100 ms：control_task() 测温 + 主状态机转移 │
│ 5. tick_flag = 1 唤醒前台│        │ 每 500 ms：blink_toggle；每 1 s：看门狗+遥测 │
└──────────────────────────┘        │ 每 10 ms：输出刷新 outputs_refresh() + 喂狗    │
                                    └───────────────────────────────────────────────┘
```

- **Timer0 重装校准（CMS8S78xx 硅真对齐）**：代码显式清零 `CKCON.T0M`，Timer0 走 Fsys/12 = 24 MHz/12 = 2 MHz（0.5 µs/计数）；10 ms = 20000 计数，重装值 `65536-20000 = 0xB1E0`（`TH0 = 0xB1, TL0 = 0xE0`），不依赖硅片复位默认（T0M=1, Fsys/4）。
- **周期调度不依赖 16 位自由计数**：100 ms / 500 ms / 1 s 任务由独立的 8 位分频器（10/50/100）产生，相位固定且与运行时长无关，避免旧实现中 `tick10ms` 16 位回绕（每 10.9 min）把某一秒截短为 360 ms 的计时漂移。
- **数码管动态扫描**：Timer0 ISR 中每 10 ms 切换一位 COM 并输出对应段码，4 位轮询周期 40 ms（25 Hz 全场刷新率），在视觉暂留（POV）模型下呈现无闪烁稳定显示。
- **无阻塞蜂鸣器音序器**：主循环前台每 10 ms 推进 `buzzer_task()`，以 10 ms 为基准递减 `melody_ticks`，自动步进 `tone_step_t` 结构体数组；Timer0 ISR 只负责数码管扫描，不触碰蜂鸣音序器。前台业务逻辑仅需调用 `play_melody()` 注册音序指针，零 CPU 阻塞。进入故障态时立即清空当前旋律，急促 warble 在下一个 10 ms tick 必响。

---

### 4.2 状态机状态转移图

```mermaid
stateDiagram-v2
    [*] --> OFF : 上电初始化（发热盘切断）

    OFF --> HEAT : ON/OFF 按下且探头读数有效 / 切换为 XXb0
    OFF --> FAULT : 探头开路(E-01) / 短路(E-02)（POST 或运行中监测）
    OFF --> FAULT : 静音待机下按 ON/OFF 但探头仍故障（拒绝启动）

    HEAT --> OFF : ON/OFF 按下 / 发热盘切断（清零全部热计时）
    HEAT --> WARM : 首次达到 98℃ 并保持 3s / 转入 XXYY 保温
    HEAT --> FAULT : 探头异常 / 干烧(E-03) / 超温(E-04)

    WARM --> OFF : ON/OFF 按下 / 回到待机
    WARM --> FAULT : 探头异常 / 保温超温(E-04)

    FAULT --> OFF : 探头故障按 ON/OFF 人工确认 / 进入静音待机
    FAULT --> OFF : 探头故障连续 300 ms 读数有效 / 自动恢复
    FAULT --> OFF : 热故障 (必须人工按 ON/OFF 键解除)
    FAULT --> FAULT : 故障保持 (发热盘强制断开 / 警报)

    OFF --> OFF : 静音待机：读数连续 300 ms 有效 → 清除 F 码

    note right of WARM
        FUNC 键循环切换保温设定：
        55 -> 60 -> 80 -> 90 -> 55 ℃
        播放阶梯升调(2k->3kHz)
        ±1℃ 窄带迟滞控制：
        T 低于 set-1 开启加热（受 dwell 门控）
        T 高于 set+1 关闭加热
    end note

    note right of OFF
        待机状态支持 FUNC 键预选模式：
        100℃ 沸腾模式（显示 " -- "）
        55℃ 直热模式（显示 "-55-"）
        按 ON/OFF 立即以选定模式启动
        「静音待机」（传感器故障人工确认后）：
        显示 " -- "、声光报警停止、继电器锁定断开；
        遥测 S=0 但 F 仍为 1/2；
        探头未修好时按 ON/OFF 拒绝开机并重新弹故障；
        读数连续 300ms 有效自动清除 F 码。
    end note

    note right of FAULT
        故障代码规范：
        E-01: NTC 探头开路 / 脱落
        E-02: NTC 探头短路 / 击穿
        E-03: 干烧报警 (25s 未过 45℃ / 60s 未沸腾)
        E-04: 超温报警 (水温越界失控)
        E-03/E-04 优先级最高，任何后续读数不得降级
    end note
```

---

### 4.3 状态输出与外设真值表

| 运行状态 | 发热继电器 | 4COM-8SEG 数码管 | 加热灯 (P0.1) | 保温灯 (P0.2) | 故障灯 (P0.6) | 硬件蜂鸣器 (P0.3) |
|---|---|---|---|---|---|---|
| **OFF（待机）** | 0 | `" -- "` / `"-55-"` | 灭 | 灭 | 灭 | 按键 4 kHz 30 ms；FUNC 预选升调 |
| **OFF（静音待机，探头故障已人工确认）** | **0（锁定）** | `" -- "` | 灭 | 灭 | 灭 | warble 停止；F 字段保留 1/2；按 ON/OFF 开机被拒时重新显示 E-01/E-02 报警；读数恢复 300 ms 后播 G6 并清除 |
| **HEAT（加热中）** | 1（**任何再吸合均受 3 s Dwell 门控**） | `"XXb0"` / `"XX55"`（dp 1Hz 闪烁） | **常亮** | 灭 | 灭 | 按键 4 kHz 30 ms；FUNC 动态变轨升调 |
| **HEAT（沸腾确认 3s）** | 0 | `"98b0"`（dp 熄灭） | **常亮** | 灭 | 灭 | 保持结束播放 4 音和弦 |
| **WARM（T < set - 1）** | 1（受 Dwell 门控） | `"XXYY"`（当前+目标） | 灭 | **常亮** | 灭 | 档位切换阶梯升调 |
| **WARM（T > set + 1）** | 0 | `"XXYY"`（当前+目标） | 灭 | **常亮** | 灭 | — |
| **FAULT（故障）** | **0（硬件强制）** | `"E-01"` ~ `"E-04"` | 灭 | 灭 | **1 Hz 闪烁** | 急促双音报警（60s 超时静音） |

---

### 4.4 4COM-8SEG 数码管显示编码规范

采用共阴极接线方式，段码映射遵循标准 `dp g f e d c b a`（高位至低位）：

```
    a
  ┌───┐
f │   │ b
  ├───┤ ‹-- g
e │   │ c
  └───┘
    d    . dp
```

| 字符 | 亮起段 | 16进制编码 | 字符 | 亮起段 | 16进制编码 |
|---|---|---|---|---|---|
| `'0'` | a, b, c, d, e, f | `0x3F` | `'8'` | a, b, c, d, e, f, g | `0x7F` |
| `'1'` | b, c | `0x06` | `'9'` | a, b, c, d, f, g | `0x6F` |
| `'2'` | a, b, d, e, g | `0x5B` | `'b'` | c, d, e, f, g | `0x7C` |
| `'3'` | a, b, c, d, g | `0x4F` | `'O'` | a, b, c, d, e, f | `0x3F`（POV 呈现为 '0'） |
| `'4'` | b, c, f, g | `0x66` | `'E'` | a, d, e, f, g | `0x79` |
| `'5'` | a, c, d, f, g | `0x6D` | `'-'` | g | `0x40` |
| `'6'` | a, c, d, e, f, g | `0x7D` | Blank | 无 | `0x00` |
| `'7'` | a, b, c | `0x07` | 小数点 | dp | 与对应位或 `0x80` |

---

### 4.5 CMS8S 硬件蜂鸣器发生器声学设计

CMS8S78xx 芯片集成专属硬件蜂鸣器外设，通过设置系统分频器（Prescaler=64 @ 24MHz 时基输入，基准频率 $F_{base} = 24\text{MHz} / 64 / 2 = 187.5\text{kHz}$），其蜂鸣器频率公式为：
$$F_{buz} = \frac{187500}{\text{BUZDIV}} \text{ Hz}$$

固件预置的声学音效配置：

1. **按键触觉点击（TONE_KEY）**：`BUZDIV = 47`（$\approx 3989\text{ Hz}$），持续 30 ms。高频短促脉冲，消除小家电按键发闷感。
2. **档位切换升调（TONE_STEP）**：
   - Step 1: `BUZDIV = 94`（$\approx 1994\text{ Hz}$），持续 40 ms；
   - Step 2: `BUZDIV = 63`（$\approx 2976\text{ Hz}$），持续 40 ms。二段式上扬音，强化档位提升感。
3. **沸腾完成分解和弦（TONE_BOIL_DONE）**：
   - Note 1: C6（1042 Hz，`BUZDIV = 180`，100 ms）
   - Note 2: E6（1320 Hz，`BUZDIV = 142`，100 ms）
   - Note 3: G6（1562 Hz，`BUZDIV = 120`，100 ms）
   - Note 4: C7（2106 Hz，`BUZDIV = 89`，200 ms）
   - 优美愉悦的大三和弦上行，给用户清晰的烹煮完成提示。
4. **探头故障恢复提示（TONE_RECOVER）**：G6（1562 Hz，`BUZDIV = 120`，100 ms）单声提示。
5. **故障警报（急促交替 Warble）**：在 FAULT 态下由控制任务驱动，以 3000 Hz 与 2000 Hz 进行双频交替，持续 60 s 后静音，符合小家电安全扰民控制标准。进入故障瞬间会切断正在播放的旋律，warble 在下一个 10 ms tick 立即起响，不再被按键音/旋律残留覆盖。
6. **无效操作否定音（TONE_BUSY）**：`BUZDIV = 235`（$\approx 798\text{ Hz}$），持续 30 ms。待机/加热态按 FUNC（档位选择仅在保温态有效）或静音待机下探头未恢复时按 ON/OFF（开机被拒）时播放，明确区分"死机无响应"与"操作不被接受"。

---

### 4.6 片内 12-bit SAR ADC 与温度插值

- **寄存器配置**：
  ```c
  ADC_ConfigRunMode(ADC_MODE_CONTINUOUS);
  ADC_EnableChannel(ADC_CH_0);
  ADC_EnableLDO();
  ADC_ConfigADCVref(ADC_VREF_3V);
  ADC_Start();
  ```
- **中值滤波（Median-of-3）**：连续采样 3 次并使用 3 元素极小比较网络取中值，彻底剔除偶发噪声脉冲。
- **物理标定 LUT**：温度查表直接运行在 12-bit 原始码域（0..4095），锚点按设备树声明的 NTC 物理模型（R25 = 10 kΩ，B = 3950，160 kΩ 上拉分压，$V/V_{ref}=R_{NTC}/(R_{NTC}+160k)$）离线计算：5/10/20/25/40/50/60/80/90/98/105 ℃ 对应原始码 571/458/298/241/131/89/63/32/24/19/16，相邻锚点间线性插值；数码管交互滑块、无头场景注入与硬件真机共用同一码域。

---

## 5. 仿真通道映射（对照 UniSim ABI Catalog）

| 功能模块 | UniSim 通道 | ABI 符号 / 机制 | Headless 注入与断言方式 |
|---|---|---|---|
| **4COM-8SEG 数码管** | CH1 GPIO 输出 | P3.0..P3.3 (COM) + P1.0..P1.7 (SEG) 动态扫描 | `ASSERT_POINT target: "plugin:display/text" matcher: "25b0"`（视觉暂留 POV 解码） |
| **硬件蜂鸣器** | CH1 专用外设 | `BUZCON`/`BUZDIV` 寄存器拦截 → 调频 PWM 仿真 | `buzzer` 插件状态监听 / 频率占空比断言 |
| **发热继电器** | CH1 GPIO 输出 | P2.0 输出电平变化 | `ASSERT_POINT target: "plugin:heater_relay/on"` |
| **状态指示灯** | CH1 GPIO 输出 | P0.1 / P0.2 / P0.6 低有效驱动 | `ASSERT_POINT target: "plugin:led_heat/on"` 等 |
| **按键输入** | CH1 GPIO 输入 | P0.4 / P0.5 准双向弱上拉输入 | `INPUT_PLUGIN_EVENT targetPluginId: "btn_onoff" action: "SET_PRESSED"` |
| **NTC 测温** | CH3 模拟量输入 | 片内 ADC AN0（物理 Pin 0，v2 rail key）采样 | `INPUT_ANALOG adcChannel: 0 valueNorm: <0.0~1.0>` |
| **UART 遥测** | CH2 串口总线 | 片内 SBUF 发送拦截 → `js_pal_uart_write` | `ASSERT_BUS_PAYLOAD target: "bus:0:tx" matcher: "S=1,H=1"` |

---

## 6. Headless 自动化测试场景矩阵

工程配套 15 个全自动化 Headless 验证场景，涵盖快乐路径、边界保护、触点延寿、全状态电源安全与新增的外设交互：

| 场景文件 | 验证重点与覆盖路径 | 核心断言结果 |
|---|---|---|
| `health-pot-display-buzzer.scenario.json` | **CMS8S 专属外设验证**：待机 `" -- "` → 开机加热 `"25b0"` → 沸腾 `"98b0"` → 保温 `"9860"` → 探头故障 `"E-02"` 字符解码与继电器联动 | 🟢 100% PASS（验证 POV 数码管字符解码与声学） |
| `health-pot-boil-warm.scenario.json` | **快乐路径**：OFF → 98℃ 烧水 → 3s 沸腾确认 → 转入 WARM → FUNC 切换 80/90/60℃ → 迟滞再加热 → ON/OFF 关机 | 🟢 100% PASS（继电器切换、指示灯、UART 遥测） |
| `health-pot-boil-confirm-dip.scenario.json` | **沸腾确认期温度回落防护**：98℃ 关加热器后，水温降至 60℃ 绝不提前重热，必须完整倒数 3s 转保温 | 🟢 100% PASS（消除 98℃ 边界振荡活锁） |
| `health-pot-fault-guard.scenario.json` | **NTC 探头短路自恢复路径**：加热中短路 → FAULT（继电器即时断开）→ 探头恢复经 300 ms 去抖 → 自动回 OFF | 🟢 100% PASS（安全关断，去抖恢复） |
| `health-pot-ntc-open.scenario.json` | **NTC 探头开路自恢复路径**：加热中拔出探头（分压轨满量程，原始码 4095）→ 触发 E-01 → 恢复经 300 ms 滤波去抖 → 自动回 OFF | 🟢 100% PASS（开路对称性覆盖） |
| `health-pot-dryfire.scenario.json` | **干烧一级 Sticky 锁存验证**：25s 未过 45℃ 触发 E-03 → 即使探头被干烧拖入短路区也不降级 → 探头恢复不自动清除 → 必须人工按键解除 | 🟢 100% PASS（热故障优先级锁定与手动清除） |
| `health-pot-overtemp.scenario.json` | **保温超温保护验证**：保温状态下注入过热码持续 10s 触发 E-04 → 继电器强制断开 → 必须人工按键清除 | 🟢 100% PASS（过热失控看门狗） |
| `health-pot-relay-dwell.scenario.json` | **继电器 Dwell 触点保护验证**：断开边沿 0 延迟即刻生效；再吸合边沿受 3s 最小停歇时间门控 | 🟢 100% PASS（继电器触点防频繁拉弧） |
| `health-pot-uart-telemetry.scenario.json` | **UART 遥测链路**：待机首帧 + 加热态连续周期帧，验证波特率发生器初始化后轮询发送不死锁 | 🟢 100% PASS（`ASSERT_BUS_PAYLOAD` 帧内容与周期） |
| `health-pot-display-cold-hold-clamp.scenario.json` | **显示标定边界**：5℃ 冷端锚点、沸腾确认期 dp 恒灭（segMask 精确断言）、105℃ 钳位 99 不乱码 | 🟢 100% PASS（`text` + `segMask` 双通道断言） |
| `health-pot-fault-blink-1hz.scenario.json` | **故障灯 1Hz 闪烁**：三点位相位采样验证半周期 500ms（防回归为 0.5Hz） | 🟢 100% PASS（LED 相位断言） |
| `health-pot-sensor-fault-powermute.scenario.json` | **任意状态电源安全（传感器故障可确认）**：上电即开路 → E-01 → 按 ON/OFF 进入静音待机（显示 `" -- "`、报警停、F 保留）→ 探头未恢复再按 ON/OFF **拒绝开机**并重新报警 → 恢复 300ms 自动清除 → 正常开机加热 | 🟢 100% PASS（电源键永不失灵 + 拒绝盲加热） |
| `health-pot-power-cycle-dwell.scenario.json` | **电源快速重启 Dwell**：HEAT 中关机再立即开机，断开边沿 0 延迟；再吸合仍须等满 3s 最小停歇 | 🟢 100% PASS（再吸合路径统一门控，无零间隔拉弧） |
| `health-pot-dryfire-stall.scenario.json` | **二级干烧（沸腾超时）**：水温越过 45℃ 后停滞在 55℃（敞盖/低电压），一级 25s 看门狗不触发；持续加热 60s 未沸腾 → E-03 锁存，手动解除 | 🟢 100% PASS（填补 45℃ 以上保护盲区） |
| `health-pot-overtemp-heat.scenario.json` | **HEAT 态越界码防护**：加热中直接注入 raw≈15（未经过 98℃ 沸腾带），继电器在 10s 资格期强制断开、**不得假沸腾转 WARM**；10s 后 E-04 锁存手动解除 | 🟢 100% PASS（杜绝超温假沸腾旋律/误转保温） |

**一键测试命令**：
```powershell
python D:\workspaces\ai-coding\wink-ai\wink-ai\packages\wink-tools\wink.py sim run `
  --mode headless --app mcs51_health_pot `
  --scenarios wink-micro-app\mcs51_health_pot\unisim-scenarios
```

---

## 7. 代码与工程目录结构

```
wink-micro-app/mcs51_health_pot/
├── health_pot.c                       # 核心商用固件（CMS8S78xx 专用，全外设集成驱动）
├── CMakeLists.txt                     # 跨 target 构建配置，链入 wink_mcs51_core
├── wink-app.json                      # CMS8S78xx 硬件设备清单（4COM-8SEG、buzzer、继电器、LED、按键）
├── DESIGN.md                          # 本设计文档
├── unisim-assets/
│   ├── device-tree.json               # 运行时 UniSim 设备树定义
│   ├── wink_simulator.js              # 编译导出的 WebAssembly 胶水层
│   └── wink_simulator.wasm            # 编译导出的 WebAssembly 仿真内核
└── unisim-scenarios/
    ├── health-pot-display-buzzer.scenario.json     # CMS8S 专属数码管与蜂鸣器验证
    ├── health-pot-boil-warm.scenario.json         # 快乐路径基准测试
    ├── health-pot-boil-confirm-dip.scenario.json   # 沸腾确认回落防护
    ├── health-pot-fault-guard.scenario.json       # NTC 短路防护
    ├── health-pot-ntc-open.scenario.json          # NTC 开路防护
    ├── health-pot-dryfire.scenario.json           # 干烧一级 Sticky 锁存
    ├── health-pot-dryfire-stall.scenario.json     # 干烧二级（45℃ 以上停滞）
    ├── health-pot-overtemp.scenario.json          # 保温超温失控防护
    ├── health-pot-overtemp-heat.scenario.json     # 加热越界不假沸腾 + E-04
    ├── health-pot-power-cycle-dwell.scenario.json # 快速重启再吸合 Dwell
    ├── health-pot-sensor-fault-powermute.scenario.json # 传感器故障电源可确认
    └── health-pot-relay-dwell.scenario.json       # 继电器 Dwell 延寿门控
```

---

## 8. 仿真与量产真机差异分析及工程建议

| 特性维度 | UniSim 仿真环境行为 | CMS8S78xx 量产硬件行为 | 工程师量产落地指导 |
|---|---|---|---|
| **12-bit SAR ADC 基准** | 软件映射至 3.0V 标称轨 | 片内 LDO 输出 3.0V 至 VREF 引脚 | 必须在 PCB 上的 VREF 引脚就近放置 100nF + 1µF 陶瓷去耦电容，并在 NTC 分压输入点串联 1kΩ + 100nF RC 低通滤波 |
| **4COM-8SEG 驱动能力** | 虚拟逻辑仿真，无功耗与压降 | COM 口最大 150mA 灌电流，SEG 口 32.7mA 推挽 | CMS8S78xx 可直驱小尺寸数码管无需外挂三极管；若驱动大尺寸或超高亮数码管，COM 口仍建议增加 S8550/PNP 三极管驱动 |
| **硬件蜂鸣器 BUZCON** | 提取频率与占空比事件 | P0.3 输出硬件方波，直驱无源压电陶瓷蜂鸣片 | 蜂鸣器引脚需串联限流电阻（如 100Ω），并反向并联续流二极管或 10k 泄放电阻防止反电动势击穿 MCU 引脚 |
| **看门狗（WDT）** | cms8s_sys 建模 TA 保护（含窗口收窄：AA/55 间插入其他 SFR 写或超 ~100us 虚拟时间窗口则解锁失效、受保护写入回滚）+ WDT 超时粗模型（CKCON.WTS 间隔内无 WDTCLR 喂狗则 STRICT 断言 / Release 计数判 FAIL，不做整机复位） | 片内 WDT：`CKCON.WTS=0x06` 选择 2^24 Tsys ≈ 0.70s 溢出（24MHz）；WDTRE 使能与 WDTCLR 喂狗均须 TA 保护时序（`TA=0xAA; TA=0x55; WDCON`，中间禁插其他 SFR 访问） | **硬约束：最长阻塞段（含遥测帧长/波特率，当前 22B@9600bps ≈ 23ms）< WTS 间隔（0.70s）**；主循环每 10ms 喂狗；改波特率/加长遥测帧前重核算该不等式 |
| **干烧保护时间** | 25 s（< 45 ℃）+ 60 s（未沸腾）加速测试 | 一级 120 ~ 180 s；二级沸腾超时 600 ~ 900 s（视水壶容积与功率而定） | 量产前将 `DRYFIRE_SECONDS` / `BOIL_TIMEOUT_SECONDS` 修改为真实壶体测试标定值，或引入温升斜率 $\Delta T / \Delta t$ 动态算法 |
| **强电电气隔离与安规** | 仅逻辑量控制 | 220V 强电发热盘与弱电主控板共存 | 继电器驱动线圈必须反向并联 1N4148 续流二极管；PCB 强弱电爬电距离不小于 6.5 mm；加热回路必须串联双金属温控器与一次性温度保险丝（TCO）实现机械级硬件双保险 |
