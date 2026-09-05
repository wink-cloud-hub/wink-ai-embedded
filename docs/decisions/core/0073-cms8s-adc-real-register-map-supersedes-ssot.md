# ADR-0073：CMS8S78xx 片内 ADC 采用真实寄存器图（取代无夹具期理想化 SSOT 图）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-08-29）** |
| **日期** | 2026-08-29 |
| **触发** | M5 开工前用户提供了原厂夹具（`docs/vendors/Cmsemicon/`：CMS8S78xx 参考手册 V1.1.1 第 22 章 ADC + DemoCode V2.0.2 设备头 `cms8s78xx.h` 与 StdDriver `adc.c/adc.h`）。对照发现：数据面 SSOT §6.2 在无夹具阶段臆测的 CMS8S ADC 寄存器图（ADCON @0xE1、启动位 bit5、结果 @0xE3/0xE4……）与真实硅片完全不符，按 SSOT 实现的模型无法运行任何原厂/Keil 风格用户代码。 |
| **影响范围** | `frameworks/mcs51/include/REG_CMS8S.H`（新）、`mcs51_xsfr.hpp`（新）、`include/mcs51_adc.h` + `src/mcs51_adc.cpp`（注入轨加宽 12-bit / 32 通道）、`src/mcs51_xdata.cpp` + `include/absacc.h`（XSFR 窗口）、`include/wink_mcs51_isr.h`（向量表 8→28）、`src/cms8s_adc.cpp` + `include/cms8s_adc.h`（新模型）、`src/mcs51_bridge.cpp`；文档：数据面 SSOT §6.2、MCU 兼容计划 §3.12、用户兼容手册 §4.4、Layer-① 拦截规格。 |
| **决策者** | 项目 Owner（2026-08-29 AskUserQuestion 三项拍板） |
| **关联 ADR** | [ADR-0070](0070-mcs51-zero-code-simulation-interception-layer.md)（umbrella）、[ADR-0071](0071-sfr-proxy-rmw-edge-data-plane.md)（SFR 代理数据面）、[ADR-0072](0072-dual-clock-domain-and-quota-catchup.md)（0µs 即时外设语义） |
| **关联计划** | [`2026-08-27-mcs51-zero-code-simulation-plan.md`](../../implementation-plans/core/2026-08-27-mcs51-zero-code-simulation-plan.md)（M5 核心） |
| **关联技术设计** | `docs/tech-designs/mcs51/2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md`（数据面 SSOT §6.2 已回写） |

---

## 1. 背景（Context）

M0–M4 期间无任何 CMS8S 原厂资料，数据面 SSOT §6.2 基于「标准 8051 + 常见 SAR ADC」的一般惯例给出了一幅理想化寄存器图（ADCON/ADCF 等），并明确标注为「待夹具核对」。M5 夹具到位后，以原厂设备头 `cms8s78xx.h` 的 SFR 表、参考手册第 22 章寄存器描述、StdDriver `adc.c` 的实际读写三处交叉验证，确认真实图为：

| SFR | 真实地址 | SSOT §6.2 旧图 | 关键位（真实） |
|---|---|---|---|
| ADCON0 | **0xDF** | 0xE1 | bit1 **ADGO**（写 1 启动；硬件完成后**自清零为 0**，故该位即忙标志，轮询 `while(ADCON0&0x02)`）；bit6 **ADFM**（0=左对齐，1=右对齐）；bit5:2 ANACH（AN63 内部子通道） |
| ADCON1 | **0xDE** | （并入 0xE1） | bit7 **ADEN** 模块使能；bit6:4 ADCKS 时钟分频 |
| ADCCHS | **0xD9** | 0xE2 | bit5:0 通道选择（0~25=AN0~AN25；0x3F=AN63 内部） |
| ADRESH | **0xDD** | 0xE3 | 结果高字节（只读） |
| ADRESL | **0xDC** | 0xE4 | 结果低字节（只读） |
| EIE2 | **0xAA** | — | bit4 **ADCIE** 转换完成中断使能 |
| EIF2 | **0xB2** | — | bit4 **ADCIF** 完成标志（软件清零，锁存，同 UART TI 先例） |
| 中断向量 | Keil **interrupt 19**（向量地址 0x9B） | 未列 | EA+ADCIE 均置位时 EOC 派发 |
| ADCLDO | XSFR **0xF692**（MOVX/xdata 空间） | 未列 | bit7 LDOEN、bit6:5 VSEL、bit4 LDOOUTEN |
| PxxCFG | XSFR **0xF000..0xF033** | 未列 | 引脚功能复用 |

结果装载公式（原厂 `ADC_GetADCResult` 逐字）：右对齐 `0xFFF & ((ADRESH<<8)|ADRESL)`（ADRESH 低 4 位 = D11..D8）；左对齐 `0xFFF & ((ADRESH<<4)|(ADRESL>>4))`。

两个旧图未覆盖的硬阻塞：
1. **向量 19**：框架 ISR 表 `WINK_MCS51_NUM_VECTORS = 8`，`set_isr` 对 n≥8 **静默丢弃**，ADC 中断永远无法派发。
2. **XSFR 野指针**：原厂头以 `#define ADCLDO *(volatile unsigned char xdata *)0xF692` 声明 XSFR；清洗 pass 擦除 Keil `xdata` 关键字后，该表达式退化为宿主进程 `*(volatile unsigned char*)0xF692` 野指针解引用，必须以代理对象取代。

## 2. 方案比选（Options）

| 方案 | 描述 | 优 | 劣 | 结论 |
|---|---|---|---|---|
| A. 沿用 SSOT 理想化图 | 按 0xE1/bit5/0xE3/0xE4 实现 | 不改动已写文档 | 任何原厂 StdDriver/Keil 用户代码都无法编译运行；仿真价值归零，违背「零侵入跑真实工程代码」立项目标 | ❌ |
| B. 双图并存 | 理想图 + 真实图各一套模型，按宏切换 | 旧文档不失效 | 双倍维护、语义分叉、用户无从选择；无任何真实代码消费理想图 | ❌ |
| C. **按真实图实现 + 修订 SSOT（本 ADR）** | 模型/REG_CMS8S.H/掩码宏全部对齐原厂；SSOT §6.2 等三处文档回写；ADR 留痕 | 原厂 `adc.c` 未来可直接编译；用户 Keil 工程零改动运行；单一事实来源 | 文档返工；向量表扩表；需新增 XSFR 代理与窗口 | ✅ **采纳** |

XSFR 处理子决策：
- C1. **xdata 开 XSFR 窗口** `[0xF000, 0x10000)` 合法（与 XRAM 孔径 `[0, WINK_MCS51_XDATA_SIZE)` 并列；64KB 影子数组本就覆盖，STRICT 双态语义不变）——对比「拒绝一切 xdata 访问」会把 PxxCFG/ADCLDO 全部打成不支持，原厂 GPIO/ADC 驱动无法运行。
- C2. **WinkXsfr 代理取代宏**：`REG_CMS8S.H` 把 `ADCLDO/PxxCFG` 定义为 `inline WinkXsfr(addr)` 常量初始化代理，全部读写/RMW 走 `wink_mcs51_xdata_read/write(addr, …, kind=XSFR)` 受检 C-ABI——杜绝宿主野指针，且 OOB 双态（STRICT assert+abort / release 告警丢弃）一致。

注入轨宽度子决策：
- C3. **模拟注入轨加宽到 12-bit**（0~4095；通道表 8→32 覆盖 AN0~AN25 + AN63 余量）。8-bit ADC0832 的两个消费点（`mcs51_adc0832.cpp` 取值与 `mcs51_adc.h` inline shim）均 `& 0xFF` 掩码，加宽对 ADC0832 零影响（M4 全部测试回归为证）。对比「双轨 8/12-bit 并存」只会复制 rail 与注入 API。

## 3. 决策结论（Decision）

- **D1. 真实寄存器图为唯一事实**。模型、`REG_CMS8S.H`（sfr 声明 + XSFR 代理 + 原厂逐字掩码宏 `ADC_ADCON0_*`/`ADC_ADCON1_*`/`ADC_ADCLDO_*`/`IRQ_EIE2_ADCIE_Msk`/`IRQ_EIF2_ADCIF_Msk`/`ADC_CH_*`/`ADC_RESULT_*`/`ADC_VREF_*`/`ADC_IS_BUSY`/`ADC_GO()`）全部以原厂 `cms8s78xx.h` + 手册第 22 章为准。SSOT §6.2 的 0xE1 图**废弃**。
- **D2. 0 周期穿透语义不变**（ADR-0072 D1：即时外设 0µs）。ADCON0 写 hook 在写语句内同步完成：拉取 12-bit 码值 → 按 ADFM 装载 ADRESH/ADRESL → 影子中自清 ADGO → 按 ADCIE 锁存 ADCIF、按 EA 派发向量 19。`while(ADCON0&0x02)` 首次迭代即退出。
- **D3. 向量表扩至 28**（核心 0~7；CMS8S 扩展 8~27；ADC=19）。表/计数/派发全部从 `WINK_MCS51_NUM_VECTORS` 宏派生，仅头文件改动。
- **D4. XSFR 窗口 + WinkXsfr 代理**（C1/C2）。xdata 合法孔径 = XRAM  aperture ∪ `[0xF000,0x10000)`；窗口内落 64KB 影子，窗口外保持 STRICT/release 双态 OOB。
- **D5. 12-bit 统一注入轨**（C3）。`MCS51_ADC_RAW_MAX=4095`；ADC0832 shim 掩码低 8 位。
- **D6. v1 明确收窄**（计划内偏差，非缺陷）：AN63 内部通道（BGR/温度/VDD）转换返回码值 0；ADCLDO VSEL 不影响满量程（固定满幅，注释声明）；ADRESH/ADRESL 不挂读 hook（结果寄存器只读、由模型装载）；完整 ADC_Ldo 例程（tier-c，需 system.h/gpio.h/19 个 ISR 桩）延后 M6。
  - **tier-b 已收割（2026-08-29，M5 收尾）**：原厂 StdDriver `adc.c` **未修改**编译并在 host 沙箱运行通过（`test_mcs51_cms8s_vendor`）。机制：committed shim `frameworks/mcs51/include/cms8s78xx.h` 置于 include 路径首位，遮蔽原厂 Keil 设备头（其重定义 stdint/sfr、用野指针 `#define ADCLDO *(volatile unsigned char xdata *)0xF692`，无法被宿主 C++ 编译）；shim 仅 `#include "REG_CMS8S.H"` 提供全部 SFR/XSFR 代理与逐字掩码。原厂 `adc.c/adc.h` 为 GBK，`mcs51_cleanup.py` 新增 UTF-8 优先 / GBK 回退解码与 `--transcode` 模式，在构建树内规范化为 UTF-8（源文件只读、永不入库）。多 TU 经 C++17 `inline WinkSfr/WinkXsfr` ODR 安全共享。夹具缺失时 CMake 优雅跳过（plain-C/CI 检出）。tier-c（完整 ADC_Ldo 例程 + 真实工程 #3）仍延后 M6。

## 4. 后果与约束（Consequences & Constraints）

- **正向**：Keil 风格 CMS8S ADC 用户代码（轮询或中断 19）零改动在 host/wasm 仿真运行；原厂 StdDriver `adc.c` 未来仅需补 system/gpio 层 shim 即可整体编译；通道-3 模拟面（NTC/旋钮/LDR 插件）对 ADC0832 与片内 ADC 同源生效。
- **约束**：
  - 任何新增 CMS8S SFR 必须先在原厂头/手册核对地址与位定义，禁止再凭惯例臆测；`REG_CMS8S.H` 掩码宏保持原厂逐字命名以兼容 StdDriver。
  - XSFR 代理只可经 `wink_mcs51_xdata_*` 受检路径访问，**禁止**在清洗后代码中出现裸 `*(unsigned char xdata *)` 宿主指针（cleanup 擦除 `xdata` 后即野指针）。
  - 向量号 ≥8 的 ISR 依赖扩表后的 28 项表；新增扩展向量时在 `REG_CMS8S.H`/文档登记。
  - ESP32 真机零增量：mcs51 框架整体在 `if(ESP_PLATFORM) return()` 门控之后，新文件不进入固件链接图。
- **测试证据（2026-08-29 M5）**：MSVC host mcs51 ctest 16/16、MinGW host 16/16、wasm/Node 6/6（含 `test_mcs51_cms8s_adc` 单元测试 11 组断言、`test_mcs51_cms8s_adc_e2e` 与 `wasm_mcs51_cms8s_adc_test`）；STRICT 抽测（窗口内合法、0xE000 OOB assert+abort）；`wink lint --pack layering --pack api` 无发现。
- **测试证据（2026-08-29 tier-b 收割）**：未修改原厂 StdDriver `adc.c` 编译运行 —— MSVC host mcs51 ctest **23/23**（17 host 含新 `test_mcs51_cms8s_vendor` + 6 wasm）、MinGW host **17/17**；vendor exe 直跑 PASS（ADC_* config/start/GO/result、向量 19 EOC 中断、XSFR LDO、compare/trig/AN63 smoke）；`wink lint` 无发现。构建注记：REG_CMS8S.H 的枚举重定义宏采用原厂逐字 token 间距（GCC 无 `-Wmacro-redefined`，仅逐字一致才静默；Clang/MSVC 另以 `/wd4005`、system include 抑制第三方头告警）。

## 5. 遵循与后续（Compliance & Follow-up）

- [x] SSOT §6.2 重写为真实图（`2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md`）。
- [x] MCU 兼容计划 §3.12、用户兼容手册 §4.4 同步。
- [x] Layer-① 设计规格 `docs/design/02-wink-micro-os/07-mcs51-simulation-interception.md` 回写 CMS8S ADC 小节。
- [x] ~~编译原厂 StdDriver `adc.c`（tier-b）~~ —— **2026-08-29 收割完成**（见 D6 tier-b 小节）：未修改原厂 `adc.c` 经 `cms8s78xx.h` shim + GBK→UTF-8 transcode 在 host 编译运行，`test_mcs51_cms8s_vendor` 通过。
- [ ] M6：补 system.h/gpio.h shim 与 19 个 ISR 桩，编译完整 ADC_Ldo 例程（tier-c，真实工程 #3）；AN63 内部通道模型（BGR/温度/VDD）按需开启；ADCLDO VSEL 对满量程的影响建模。

---

*本 ADR 状态变更请在此记录：*
- 2026-08-29：Proposed → Accepted（M5 实现 + 测试矩阵全绿后由 Owner 拍板三项子决策；真实寄存器图取代 SSOT 理想化图）。
