# MCS-51 框架通用内核与芯片专属逻辑（CMS8S78xx）耦合混杂审计与解耦迁移清单

| 元数据项 | 说明 |
| :--- | :--- |
| **文档编号** | MCS51-COUPLING-2026-09-11 |
| **创建日期** | 2026-09-11 |
| **所属模块** | `wink-micro-os/frameworks/mcs51/`（通用 8051 仿真内核与 CMS8S78xx 芯片模型） |
| **状态** | **Draft / 待执行迁移核对清单** |
| **审计基线** | master @ e0bce2b + 工作区已落地补丁；对照 CMS8S78xx 原厂手册与标准 8051/8052 规范 |
| **关联文档** | [可维护性审查清单](./2026-09-11-mcs51-maintainability-todolist.md)（M1~M7）、[缝隙审计与整改任务清单](./2026-09-10-mcs51-sim-vs-silicon-gap-todolist.md)、[后端责任划分](./2026-09-10-mcs51-sim-backend-responsibility-classification.md) |
| **目标受众** | 框架维护者、内核架构师、后续迁移执行者 |

---

## 0. 审计背景与核心痛点

在 `frameworks/mcs51/` 的演进历史中，最初仅针对标准 8051（AT89C52）与外挂 ADC0832 设计。随后为了支持复杂商业家电（如中微 CMS8S78xx 养生壶 demo），团队引入了片上外设。

然而在增量开发过程中，为了快速跑通场景，大量**厂商/芯片特定硬件细节（中微 CMS8S78xx 专有的 XSFR、引脚重映射、1T 时钟分频、特殊中断向量）直接硬编码塞进了通用的 `mcs51_*.cpp` 和 `mcs51_*.h` 文件中**。

### 产生的直接危害：
1. **行为失真爆雷**：通用 `mcs51_adc.cpp` 假定所有 8051 只有虚拟通道 `32 + ch`，导致接在物理 Pin 0（P0.0）上的片上 NTC 探头在线仿真读出 0V，**上电即报 E-02 假短路**。
2. **违反开闭原则（OCP）**：通用文件随处可见 `if (has_cms8s_io)`、`if (has_xsfr)` 或硬编码地址。后续若接入宏晶（STC8H/STC89）、新唐（N76E003）或沁恒（CH552），通用代码将沦为 `if-else` 泥潭。
3. **架构名不副实**：表面上叫 `mcs51_gpio`、`mcs51_uart`、`mcs51_bridge`，骨子里却与 CMS8S78xx 强耦合，经典 8051 反而成了“二等公民”。

---

## 1. 混杂耦合全景审计清单（迁移核对总表：24 项完整版）

在初次抽样审查外设驱动的基础上，我们进行了**第二轮穿透式全工程白盒深度审查**（覆盖 Core 数据结构、公共 Include 头文件、CMake 构建依赖、Python 工具链/清洗脚本、门禁 Lint 以及 UniSim 前后端契约），并在资深嵌入式可维护性复审中追加了**第三轮系统级盲区补查**（复位播种路径、上下文结构体残留、Family 描述符 schema 完备性、板级器件归属、测试资产与跨仓 ABI 治理）。

审查确认：原先记录的 10 项仅覆盖了“外设逻辑实现层”，在**内核数据结构污染、公共头文件越权、引脚物理规格削足适履、构建系统强绑及工具链硬编码**等系统级层面存在严重的**次生隐蔽耦合**；复审进一步确认在**复位路径播种、定时器/中断状态结构体残留、描述符 schema 欠配、ADC0832 板级归属错位、测试资产耦合、跨仓 ABI 版本治理、通用 API 前缀污染**上仍有 7 项遗漏。

以下整理为 **24 项全量白盒审计清单**，并划分为 8 大架构维度，作为后续彻底解耦迁移的**逐项勾销凭据（Checklist）**：

| 编号 | 架构维度 | 现存通用文件与行号 | 混杂的芯片专属逻辑（硬编码私货） | 架构不合理性与现实危害 | 建议解耦迁移目标 | 迁移完成标记 |
| :--- | :--- | :--- | :--- | :--- | :--- | :---: |
| **CPL-01** | 外设模拟轨 | [mcs51_adc.cpp:L73](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_adc.cpp#L73)<br>[mcs51_adc.h:L21](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_adc.h#L21) | 通用 ADC 硬编码 `js_pal_adc_read_norm((uint16_t)(32u + ch))` 虚拟引脚偏移假定。 | 经典 8051 无片上 ADC；CMS8S 片上 AN0~AN25 映射在物理 P0.0~P3.3。导致开发板接在 Pin 0 的探头在线仿真读出 0V，**上电即报 E-02 假短路**。 | **迁移至 `cms8s_adc.cpp`**：通道到引脚映射（AN0->Pin 0）由 CMS8S 片上模型闭环，通用 ADC 只收真实物理引脚。 | ⬜ 待办 |
| **CPL-02** | 外设参考源 | [mcs51_adc.h:L58-L64](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_adc.h#L58-L64) | 通用 ADC 声明了 `mcs51_adc_set_vref_mv`，注释写着 `Vref from ADCLDO.VSEL (mV)`。 | 把中微专属的内部基准源（ADCLDO）寄存器动态选择逻辑直接污染到通用 8051 模拟轨头文件中。 | **移入 `cms8s_adc.cpp`**：内部基准计算由中微驱动自理，通过通用抽象告知模拟参考电平。 | ⬜ 待办 |
| **CPL-03** | 数字 IO 机制 | [mcs51_gpio.cpp:L63-L72](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_gpio.cpp#L63-L72)<br>[mcs51_gpio.cpp:L115-L140](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_gpio.cpp#L115-L140) | 1. 显式函数 `bool mcs51_has_cms8s_io(ctx)`。<br>2. 硬编码中微专有 XSFR：`0xF000`（PxxCFG 模拟复用）、`0xF004`（PxTRIS 方向）、`0xF008`（PxUP 上拉）、`0xF00C`（开漏）、`LEDSDR`。 | 标准 8051 端口为准双向弱上拉。中微推挽/开漏/上拉/模拟复用等增强特性直接写死在读写总路径，导致经典 8051 每次读写被无效判断拖累。 | **剥离新建 `cms8s_gpio.cpp`**：通用 GPIO 仅保留标准准双向模型，通过 `GpioTrait` 钩子表挂载扩展 IO 属性。 | ⬜ 待办 |
| **CPL-04** | 串行通信 | [mcs51_uart.cpp:L38-L52](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_uart.cpp#L38-L52)<br>[mcs51_uart.cpp:L135-L170](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_uart.cpp#L135-L170) | 1. 硬编码 `SFR_FUNCCR = 0x91`（CMS8S 专属串口时钟源选择：Timer1/2/4/BRT）。<br>2. 硬编码 `XSFR_PS_RXD = 0xF69F` 引脚重映射及 `P14CFG/P22CFG` 复用。 | 标准 8051 串口引脚固定（P3.0/P3.1），波特率时钟固定来自 Timer1/2。硬编码多时钟源与引脚重映射使通用串口模型失去通用性。 | **剥离新建 `cms8s_uart.cpp`**：通用 UART 仅通过时钟回调获取时钟，引脚重映射与时钟选择由 CMS8S 扩展模型拦截。 | ⬜ 待办 |
| **CPL-05** | 定时器机制 | [mcs51_timer.cpp:L125](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_timer.cpp#L125)<br>[mcs51_timer.cpp:L865-L906](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_timer.cpp#L865-L906) | 1. `CKCON.T0M/T1M`（1T Fsys/4 分频）。<br>2. `T2IF/EIF2` 写0清零（W0C）专属中断标志。<br>3. 注册中微专有 `T34MOD/TL3/TH3/TL4/TH4/CCEN/CCL1..3/CCH1..3`。 | 经典 8051/8052 仅有 12T Timer0/1/2，无 Timer3/4 和捕获比较单元。通用定时器代码膨胀至 1000 多行，且混入特定厂商的写0清零规则。 | **定时器多芯片解耦**：分频基准由芯片家族描述符驱动；Timer3/4 与捕获单元移入 `cms8s_timer.cpp`。 | ⬜ 待办 |
| **CPL-06** | 中断向量表 | [mcs51_isr.cpp:L27-L50](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_isr.cpp#L27-L50) | 全局默认中断向量表 `s_default_irq_map` 绑死中微布局：<br>• ADC 绑死在向量 19<br>• PWM/I2C/SPI 绑死在向量 18/21/22<br>• UART1 标为 0xFF（中微无串口1） | 标准 8052 仅有 0~5 号向量；STC 芯片的 ADC（向量5）和 PWM 向量与中微截然不同。通用 ISR 彻底丧失跨芯片复用性。 | **表驱动化（Table-Driven）**：通用 ISR 只保留标准 0~5 向量；其余扩展向量从芯片家族描述符动态装载。 | ⬜ 待办 |
| **CPL-07** | 外部中断扩展 | [mcs51_extint.cpp:L59-L75](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_extint.cpp#L59-L75) | 硬编码中微专属寄存器：<br>• `SFR_P0EXTIE = 0xAC`<br>• `PORT_EICFG_BASE`（`0xF080`~`0xF098`）<br>• `XSFR_PS_INT0 / XSFR_PS_INT1`（`0xF6C0` 等）<br>• `PORT_VECTORS[4] = {7, 8, 9, 10}` | 标准 8051 外部中断仅有 INT0（P3.2）和 INT1（P3.3）。将中微全端口电平变化中断与引脚重映射硬塞入通用外部中断模型。 | **剥离端口中断**：标准模型仅保留 INT0/INT1；端口中断与引脚选择作为 CMS8S 专有外设模型挂载。 | ⬜ 待办 |
| **CPL-08** | XDATA / XSFR 寻址 | [mcs51_xdata.cpp:L43-L65](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_xdata.cpp#L43-L65)<br>[mcs51_xdata.cpp:L97-L102](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_xdata.cpp#L97-L102) | 1. 硬编码 `KIND_XSFR = 2u`。<br>2. 强包含 `mcs51_xsfr_allowlist.h`，以二分查找验证中微专有 93 个 XSFR 地址。 | 经典 8051 的 MOVX 是外部物理总线。XSFR 是中微专有的扩展 SFR 映射。直接在通用 XDATA 寻址模型中内联中微的白名单校验。 | **总线窗口参数化**：XRAM 大小与 XSFR 窗口区间完全由 `McuFamilyDescriptor` 定义，通用模型不出现任何厂商白名单头文件。 | ⬜ 待办 |
| **CPL-09** | 框架桥强耦合 | [mcs51_bridge.cpp:L8-L10](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_bridge.cpp#L8-L10)<br>[mcs51_bridge.cpp:L102](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_bridge.cpp#L102) | 1. 直接 `#include "ADC0832.H"` 与 `#include "cms8s_adc.h"`。<br>2. 每次通用 SFR 写入无条件直接调用 `cms8s_sys_notify_sfr_write(ctx, addr);`。 | 通用桥直接强依赖具体器件头文件。仿真经典 AT89C52 时，每次 SFR 访问都在跑中微专属的看门狗 TA 时序保护检查。 | **消除裸包含与硬调用**：外设初始化通过自注册链表驱动；TA 保护窗口通过通用 SFR 写钩子动态挂载。 | ⬜ 待办 |
| **CPL-10** | 外设静态列表硬编码 | [mcs51_peripheral.cpp:L69-L93](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_peripheral.cpp#L69-L93) | 通用外设静态注册表 `g_mcs51_peripherals` 中写死声明并引用 `cms8s_adc`、`cms8s_buzzer`、`cms8s_sys`。 | 通用核心必须编译并链接全部中微源文件，无法支持轻量级经典 8051 或新增第三方芯片（如 STC8H）。 | **外设模块静态注册解耦**：采用芯片包模块化自注册（Registration Pattern），各芯片外设仅在对应 target 启用时编译链入。 | ⬜ 待办 |
| **CPL-11** | **核心上下文内生污染**<br>*(深度审查新增)* | [mcs51_context.h:L242-L257](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_context.h#L242-L257) | `Mcu51Context` 结构体明明定义了 `void* soc_priv;`，却在结构体内直接嵌入：<br>• `Mcs51Adc0832State adc0832;`<br>• `Mcs51SysProtState sysProt;`<br>• `Mcs51BuzzerState buzzer;`<br>• `Mcs51Cms8sAdcPriv cms8sAdc;`<br>• `adc_vref_mv / adc_vrail_mv` | 违背面向对象封装精神与纯净架构原则。无论用户仿真 AT89C52 还是 STC89C52，每一个通用上下文实例都在内存中硬抗中微及外挂芯片的私有数据结构。 | **私有状态移入 `soc_priv`**：厂商私有外设状态由芯片自有的上下文接管并挂载到 `soc_priv`，从 `Mcu51Context` 中彻底移除。 | ⬜ 待办 |
| **CPL-12** | **引脚物理规格削足适履**<br>*(深度审查新增)* | [mcs51_timer.cpp:L914, 927, 941](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_timer.cpp#L914) | 通用定时器引脚解析硬编码：<br>`constexpr uint8_t PORT_PINS[4] = {8u, 8u, 6u, 4u};` | `{8, 8, 6, 4}` 是 CMS8S78xx TSSOP-20 封装的物理引脚数（共 26 脚：P2 仅 6 脚，P3 仅 4 脚）。标准 8051 P0~P3 全是 8 脚，若经典程序使用 P3.4/P3.5 计数器，会被硬编码判定为非法引脚而静默失效！ | **引脚规格由芯片描述符供给**：端口引脚有效掩码从 `McuFamilyDescriptor` 读取，禁止在通用算法中硬编码缩水版芯片封装。 | ⬜ 待办 |
| **CPL-13** | **伪装通用的专有寄存器表**<br>*(深度审查新增)* | [mcs51_sfr_map.h:L12-L40](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_sfr_map.h#L12-L40)<br>[mcs51_xsfr_allowlist.h:L9-L22](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_xsfr_allowlist.h#L9-L22) | 在通用 include 目录下建 `mcs51_sfr_map.h`，却把增强/厂商语义寄存器 `P0EXTIF(0xB4)`、`T34MOD(0xD2)`、`EIE2/EIF2` 及 `PS_INT0/PS_T0/PS_ADET` 强行冠以 `MCS51_` 前缀（注：`T2CON(0xC8)` 为标准 8052 寄存器应保留通用；`CKCON(0x8E)` 为多家增强 51 共有但语义各异，地址可提、语义下沉）！ | 概念偷换、鱼目混珠。后来的开发者会误以为这些寄存器是 Intel MCS-51 行业通用标准，导致后续维护极易在经典芯片上引发非法寄存器越权访问。 | **命名归位与头文件下沉**：CMS8S 专有寄存器重命名为 `CMS8S_SFR_*` / `CMS8S_XSFR_*`，移入 `chips/cms8s78xx/` 目录下；标准寄存器保留通用，语义差异由描述符/芯片包承载。 | ⬜ 待办 |
| **CPL-14** | **公共头文件体系厂商混杂**<br>*(深度审查新增)* | [wink_mcs51_wdt.h:L48](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/wink_mcs51_wdt.h#L48)<br>[wink_mcs51_strict.h:L68](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/wink_mcs51_strict.h#L68)<br>[wink_mcu.h:L20-L38](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/wink_mcu.h#L20-L38)<br>[include/REG_CMS8S78XX.H](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/REG_CMS8S78XX.H) | 1. 通用头文件直接导出 `cms8s_sys_notify_sfr_write` API。<br>2. 通用 strict 枚举写死中微 IAP Flash 特性 `MCS51_FEAT_IAP_FLASH`（0xF9..0xFF）。<br>3. `wink_mcu.h` 直接 `#include "REG_CMS8S78XX.H"`，甚至在 51 框架内路由应广 Padauk PFS154。<br>4. 48KB 的 `REG_CMS8S78XX.H` 躺在公共 `include/`。 | 公共头文件体系严重越权，命名空间被彻底污染，架构层次荡然无存。 | **建立厂商隔离 include 目录**：公共 include 仅保留 ISO C51 与通用 8051 标准头文件；厂商头文件下沉至 `chips/`。 | ⬜ 待办 |
| **CPL-15** | **构建系统单体强绑与宏污染**<br>*(深度审查新增)* | [CMakeLists.txt:L27-L50](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/CMakeLists.txt#L27-L50)<br>[test/CMakeLists.txt:L453-L496](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/test/CMakeLists.txt#L453-L496) | 1. `_MCS51_COMPAT_SRCS` 一锅端编译链接所有 CMS8S 源文件与 ADC0832。<br>2. 顶层为 `iron_ntc` 生成的 `mcs51_board_config.h` 强加在静态库 include 路径，使所有 app 构建均被 `MCS51_HAS_ADC0832` 宏污染。 | 破坏模块化与按需编译原则。养生壶是片上 ADC，却在编译期受到外挂 ADC0832 板级配置宏的幽灵干扰。 | **构建目标分层与解耦**：将 `wink_mcs51_compat` 拆分为 `wink_mcs51_core` + `wink_mcs51_cms8s`；板级配置头文件按具体 App 隔离作用域。 | ⬜ 待办 |
| **CPL-16** | **工具链、清洗 Pass 与门禁绑定**<br>*(深度审查新增)* | [mcs51_sdcc_gate.py:L37-L119](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/tools/mcs51_sdcc_gate.py#L37-L119)<br>[mcs51_cleanup.py:L84](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/tools/mcs51_cleanup.py#L84)<br>[lint_sim_compat.py:L159](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/tools/lint/lint_sim_compat.py#L159) | 1. SDCC 编译门禁脚本硬编码中微内存配置 `MEM_LIMITS` 与 StdDriver 链接路径。<br>2. 通用 C 清洗正则写死 `cms8s|cms8s\d*` 匹配。<br>3. 静态门禁 Lint 强绑定 `REG_CMS8S78XX.H`。 | 构建与质检工具链未基于配置驱动（Data-Driven），使得引入新 51 芯片需要大面积修改 Python 核心脚本。 | **工具链配置化**：通过芯片清单（Chip Manifest JSON/YAML）声明各芯片的头文件正则、内存上限与 SDCC 门禁规则。 | ⬜ 待办 |
| **CPL-17** | **前后端引脚仲裁契约断层**<br>*(深度审查新增)* | 前端 `PinArbiter` / Vue 仿真插件 与 后端 `mcs51_adc.cpp` | 前端组件树将传感器绑定到开发板引脚 0（物理 Pin 0/P0.0），并驱动模拟电压；后端却因为缺乏片上引脚映射，私自约定读取虚拟 Pin 32。 | 前端与后端缺乏形式化的引脚映射契约文档，开发者各写各的，导致 UniSim 仿真链路虽然两头都没报错，运行态却产生致命静默断裂。 | **明确仿真引脚契约规范**：在 UniSim 设计规范中确立“板级物理引脚直通”原则，片上外设映射必须在芯片模拟层内部完成换算。 | ⬜ 待办 |
| **CPL-18** | **上下文结构体残留污染**<br>*(复审新增：CPL-11 未覆)* | [mcs51_context.h:L49-L63](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_context.h#L49)（`Mcu51TimerState.t3/t4_*`、`t2_cap_last[4]/cmp[4]`）<br>[mcs51_context.h:L79-L90](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_context.h#L79)（`Mcu51ExtIntState.port_pins[4][8]`）<br>[mcs51_context.h:L189-L193](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_context.h#L189)（`Mcs51ClassicBusState`）<br>[mcs51_context.h:L199-L208](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_context.h#L199)（`isr_table[28]`、`xdata_shadow[65536]`） | Timer3/4 与捕获比较状态、全端口中断采样状态常驻通用结构体；反向还有经典 MOVX 外总线状态常驻；`isr_table[28]` 按 CMS8S 向量数定长（经典仅需 6），`xdata_shadow[65536]` 对 CMS8S 1KB XRAM 超配 64 倍。 | 与 CPL-11 同类但未被覆盖：经典实例为从未使用的 T3/T4/端口中断背负 RAM，CMS8S 实例又为经典外总线背负状态；`sizeof(Mcu51Context) ~68KB` 的 BSS 成本阻碍未来多实例。 | **状态结构体按家族拆分**：T3/T4/捕获比较与端口中断采样移入 `cms8s_*_priv` 经 `soc_priv` 挂载；`classicBus` 移入 `at89_bus_priv`；`isr_table` 容量与 XDATA 影子按 `McuFamilyDescriptor` 裁剪或文档化超配理由。 | ⬜ 待办 |
| **CPL-19** | **复位播种路径无条件 XSFR/参考轨播种**<br>*(复审新增)* | [mcs51_context.cpp:L91-L111](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_context.cpp#L91)（`adc_vref/vrail=3000`、`xdata_shadow[PS_*]=0x7F`）<br>[mcs51_context.h:L29-L30](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_context.h#L29)（`MCS51_XRAM_SIZE_CMS8S78XX`、`XRAM_WINDOW_BASE 0xF000`） | 即使 `family==CLASSIC`，`mcs51_context_reset` 仍播种 `PS_INT0~PS_ADET=0x7F` 与 ADCLDO 源 `adc_vref/vrail` 默认值；通用头直接定义厂商命名的 XRAM 宏与 `0xF000` 窗口基址。 | CPL-02/08 只清理了读写路径，复位路径残留：经典复位后 XDATA 影子携带无意义的 XSFR 复位值，违反“经典物理绝缘”验收标准。 | **播种权下放芯片包**：通用复位仅播种 P0~P3/SP/PCON 等 Intel 标准种子；XSFR 选择器、ADCLDO 参考轨、CKCON/Fosc 由各家族 `reset` 经描述符播种；厂商命名宏下沉至 `chips/cms8s78xx/`。 | ⬜ 待办 |
| **CPL-20** | **Family 描述符 schema 欠配**<br>*(复审新增：迁移前提)* | [mcs51_family.h:L29-L42](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_family.h#L29)（现仅 `fosc/ckcon/xram/xsfr`）<br>[mcs51_family.cpp:L12-L31](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_family.cpp#L12) | 现有描述符撑不起蓝图中的 `port_pin_masks/irq_vector_table/default_pins/gpio_hooks`，更缺 `wdt_present/iap_present/uart_count/timer_caps`。 | CPL-06/12 的修复无处落脚：若不先冻结新 schema，阶段 4/5 的钩子与掩码没有挂载目标，迁移必然返工。 | **先冻结描述符 v2 schema**：补齐端口引脚掩码、中断向量表、WDT/IAP/UART/Timer 能力位；`mcs51_family.cpp` 仅加行，机制文件零修改即支持新家族。 | ⬜ 待办 |
| **CPL-21** | **ADC0832 板级器件归属错位**<br>*(复审新增)* | [mcs51_adc.h:L66-L73](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_adc.h#L66)（`adc0832_*` shim）<br>[mcs51_bridge.cpp:L51-L56](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_bridge.cpp#L51)（`MCS51_HAS_ADC0832` 分支）<br>[mcs51_context.h:L115-L127](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_context.h#L115)（`Mcs51Adc0832State`） | ADC0832 是板级外挂器件，既非通用 `core` 也非 `chips/cms8s` 硅片，却同时寄生在通用 rail 头文件、通用桥、通用上下文三处。 | 按 CPL-15 拆分为 `core+cms8s` 后 ADC0832 仍无家可归；养生壶（片上 ADC）继续受外挂 ADC 板级宏幽灵干扰。 | **下沉至板/器件层**：`mcs51_adc0832.*` 移入 `devices/`，经 trap 注册接入；`core` 仅保留纯净模拟 rail，`chips/` 仅保留片上硅片模型。 | ⬜ 待办 |
| **CPL-22** | **测试资产自身耦合**<br>*(复审新增)* | `test/CMakeLists.txt` 40/40 用例中以合成通道 `32+ch` 断言 CMS8S 行为的用例<br>[mcs51_uni_bridge.cpp:L42-L66](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_uni_bridge.cpp#L42)（host 模拟数组 `[64]` 硬编码合成空间） | 测试用通用 rail API 写法断言厂商行为；host 回退的模拟数组按合成引脚空间定长，与 CPL-01 同源。 | 阶段 1 改 `32+ch` 即破 CI：不是生产代码错而是测试契约错，却无 CPL 项跟踪，迁移安全网自身即污染源。 | **测试分层**：拆 `core-tests`（物理 Pin、标准 0~5 向量）与 `cms8s-tests`（AN 映射、扩展向量）；host 回退数组随契约版本切换双轨，迁移期双绿。 | ⬜ 待办 |
| **CPL-23** | **跨仓 ABI 版本治理缺失**<br>*(复审新增：CPL-17 纵深)* | `wasm_bridge.h` C-ABI（`js_pal_adc_read_norm` 引脚语义）与 `SimTraceSpecV2`、前端视窗 DTO | `32+ch` 改物理 Pin 是 Breaking Change，CPL-17 只写“契约拉齐”，无版本号、无双读过渡、无发版顺序。 | 前端与仿真若各发各的，静默断裂重演；回滚无据。 | **契约版本化过渡**：ABI 引脚语义升版，芯片层兼容双读一个版本（合成通道告警+重定向），按“仿真后端 → 前端插件 → stub/文档”顺序发版，可回滚。 | ⬜ 待办 |
| **CPL-24** | **通用 API 前缀与构建 knob 污染**<br>*(复审新增)* | [wink_mcs51_wdt.h:L24-L48](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/wink_mcs51_wdt.h#L24)（`wink_mcs51_wdt_*` + `cms8s_sys_notify_sfr_write` 同文件）<br>[wink_mcs51_strict.h:L45-L68](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/wink_mcs51_strict.h#L45)（`IAP_FLASH`/`RC_THERMAL` 混入通用枚举）<br>[CMakeLists.txt:L53-L79](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/CMakeLists.txt#L53)（STRICT 双库 + `WINK_MCS51_XDATA_SIZE` 框架级 cache） | 经典 51 无 WDT，`wink_mcs51_wdt_*` 命名暗示通用；STRICT 枚举混入厂商 Flash 与板级 RC 方案；`XDATA_SIZE` 本是板级外部 RAM 尺寸却成框架 cache，STRICT 拆分后将膨胀为 4 库。 | 后续新增芯片必然模仿污染：命名即契约，前缀失守则 lint 失守。 | **前缀即门禁**：`wink_mcs51_*` 仅通用，厂商用 `cms8s_*`、板级用 `board_*`，STRICT 枚举按家族作用域拆分；`XDATA_SIZE` 下沉板级，STRICT 改为 per-target 定义并以 lint 锁死。 | ⬜ 待办 |

---

## 2. 重点案例深度解剖

### 案例 1：CPL-01 虚拟引脚断层导致在线仿真 E-02（最直观的受害者）

```
【前端在线仿真 (embedded-frontend)】
   NTC 探头绑定: P0.0 (开发板线性 Pin 0)
   插件输出: PinArbiter.setAnalogDriver(0, 0.0586) ──> [物理 Pin 0 有电压]
                                                               │
   ┌───────────────────────────────────────────────────────────┴─── 跨仓断层 ───┐
   │                                                                           │
【内核通用 ADC (mcs51_adc.cpp)】                                                │
   mcs51_adc_get_value(ch=0):                                                  │
   调用 js_pal_adc_read_norm(32 + 0) ──> [读取虚拟 Pin 32] ◀───────────────────┘
   Pin 32 无任何驱动 ──> 返回 0.0V (raw=0) ──> 触发 NTC_SHORT_RAW(8) ──> 数码管报 E-02！
```

* **本质原因**：`mcs51_adc.cpp` 越俎代庖，试图代表所有 8051 定义“ADC 该去哪里读”，擅自加了 `32 + ch` 偏置；
* **正规解法**：`cms8s_adc.cpp` 最清楚 AN0 在 P0.0（Pin 0）。引脚计算必须在 `cms8s_adc.cpp` 中闭环，通用 ADC 接口只认物理 Pin。

---

### 案例 2：CPL-03 通用 GPIO 中硬编码私有寄存器（最严重的架构污染）

在 [mcs51_gpio.cpp](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_gpio.cpp) 中，可以看到如下令人费解的代码：
```cpp
inline uint8_t mcs51_tris_addr(uint8_t port) {
    switch (port) {
        case 0: return 0x9Au;  // 这是 CMS8S78xx 的 P0TRIS SFR 地址！
        case 1: return 0xA1u;  // 这是 CMS8S78xx 的 P1TRIS SFR 地址！
        ...
    }
}

inline bool gpio_may_drive(const Mcu51Context* mcu, uint8_t port, uint8_t bit, uint8_t level) {
    if (!mcs51_has_cms8s_io(mcu)) {
        return true;  // 经典 8051 兜底
    }
    // 以下全是 CMS8S78xx 专属逻辑：
    const uint8_t tris = mcu->sfr_shadow[mcs51_tris_addr(port)];
    const uint16_t od = mcs51_od_addr(port);
    ...
}
```
* **本质原因**：把某个具体单片机的寄存器地址字典硬塞进了通用 IO 读取流；
* **正规解法**：经典 8051 只有准双向口；增强型 IO（推挽/开漏/高阻/上拉）作为**扩展接口（GpioTrait）**，由 `cms8s_gpio.cpp` 挂载。通用 GPIO 在驱动引脚前调用 `ctx->gpio_hooks.may_drive(...)`。

---

## 3. 核心落地契约与架构实现硬准则（资深嵌入式专家工程守则）

为确保 24 项迁移清单在执行时不走样、不退化，以下 6 条系统级工程原则必须在落地时严格遵守：

### 准则 1：soc_priv 内存生命周期——按实例 BSS 绑定，严禁全局单例与堆分配
* **痛点防范**：CPL-11/18 引入了 `void* soc_priv;`，但在嵌入式及 Wasm 协程环境中，若在运行时使用 `malloc` / `new` 极易引发内存碎片、不可预测开销或泄漏；若用单个 `static` 全局单例则违背 M2“双 context 永不共享硅片状态”，双实例/单测并行即串扰。
* **硬性约束（二选一，不允许第三种）**：(A) 按实例 BSS 池：`static Mcs51Cms8sPriv s_priv_pool[MCS51_MAX_INSTANCES];`，按 `ctx->instance_index` 分配并 `memset`，`ctx->soc_priv = &s_priv_pool[idx];`；(B) 上移为 `Mcu51Context` 内 `union { Mcs51Cms8sPriv cms8s; Mcs51At89Priv at89; } soc;` 并取地址挂载。全程零堆、零共享，符合 ADR-0036（无异常/无 RTTI）与 ADR-0004。验收：双 context 并跑单测，互相写 XSFR/ADC 状态不串扰。

### 准则 2：ADR-0004 静态分发——以“能力位掩码短路”杜绝 vtable 虚函数性能劣化
* **痛点防范**：GPIO 是单片机仿真中吞吐量最高的数据面通道（每秒数十万次读写）。若每次读写无脑走 `ctx->gpio_hooks.may_drive(...)` 函数指针，会破坏 CPU 分支预测并产生严重的间接调用开销。
* **硬性约束**：字段定义一次、两处落地：① `mcs51_family.h` 的 `McuFamilyDescriptor` 增 `uint32_t capabilities`（如 `MCS51_CAP_ENHANCED_IO`）；② `Mcu51Context` 增 `uint32_t caps_cache`，在 `mcs51_context_reset` / `set_family` 时从 descriptor 快照一次。热路径只读 `ctx->caps_cache`：`if (!(ctx->caps_cache & MCS51_CAP_ENHANCED_IO)) return true;` 标准单片机零函数指针开销，增强型才走钩子。

### 准则 3：CMS8S78xx 原厂 AN 通道到物理引脚查表闭环（禁止算术公式）
* **换算事实源**：依据原厂数据手册，CMS8S78xx 的 26 路片上模拟输入直接映射到标准端口引脚：
  • AN0 ~ AN7   ──> P0.0 ~ P0.7（物理 Pin 0 ~ 7）
  • AN8 ~ AN15  ──> P1.0 ~ P1.7（物理 Pin 8 ~ 15）
  • AN16 ~ AN21 ──> P2.0 ~ P2.5（物理 Pin 16 ~ 21）
  • AN22 ~ AN25 ──> P3.0 ~ P3.3（物理 Pin 24 ~ 27，注意 P3 段不连续，线性公式 `((ch>>3)<<3)|(ch&0x07)` 对 AN22+ 会错算为 22~23，必须弃用）
* **硬性约束**：阶段 1 在 `cms8s_adc.cpp` 中以 `static const uint8_t AN_TO_PIN[26]` 常表闭环（P3 段显式填 24~27），附 `static_assert(AN_TO_PIN[22]==24)` 类断言与越界钳位，彻底根除虚拟 Pin 32 造成的 `E-02` 假短路。

### 准则 4：wink-app.json 到 CMake Target 依赖注入协议闭环
* **构建闭环机制**：阶段 6 拆分 `wink_mcs51_core` 与 `wink_mcs51_cms8s` 后，应用程序构建链通过读取 `wink-app.json` 的 `"mcu"` 字段（如 `"mcu": "cms8s78xx"`），由 CMake 自动解析注入目标库：
  `target_link_libraries(wink_simulator PRIVATE wink_mcs51_core wink_mcs51_${WINK_MCU})`。
  板级器件（如 ADC0832）通过板型描述按需注入 `wink_mcs51_adc0832`，彻底杜绝不同芯片或外挂器件在编译期的宏污染。

### 准则 5：低功耗与协程休眠唤醒事件流中枢统一
* **一致性保证**：单片机执行 `PCON |= 0x01` 进入 IDLE 省电模式时，协程挂起于 `ctx->wake_event`。芯片专有外设（如中微 WDT、片上 ADC 完成、端口外部电平变化中断）必须统一向核心 `mcs51_raise_irq(src)` 汇聚派发，严禁芯片包绕过核心事件中枢私自操作协程挂起，保证多源低功耗唤醒的严格保真。

### 准则 6：历史架构残余清理——`wink_mcu.h` 上移，Padauk 路由剥离 51 框架
* **架构归位**：`frameworks/mcs51/include/wink_mcu.h` 当前兼任全平台 MCU 门面（含 PFS154/PMS150C 分支），属于放错层。阶段 3 将其上移至 `wink-micro-os/include/`（或等效公共门面层），`frameworks/mcs51/` 内仅保留 51 家族路由（classic/cms8s78xx）；Padauk 分支随门面上移归位，不在 51 框架内残留。

---

## 4. 终态解耦目标架构设计（Target Architecture Blueprint）

解耦后的标准分层架构（内核、接口、芯片包、构建、工具链立体解耦）：

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│ 1. 通用内核核心层 (wink_mcs51_core - 严禁出现任何 vendor 名字/专有寄存器)           │
│    ├── mcs51_context.cpp/h   : 核心寄存器、PC/ACC/PSW、时钟、通用状态机，私有挂载 soc_priv│
│    ├── mcs51_sfr.cpp         : 标准 128B/256B SFR 读写分发与影子镜像               │
│    ├── mcs51_gpio.cpp        : 标准 P0~P3 准双向口读写锁存 (无 TRIS/PxxCFG)        │
│    ├── mcs51_timer.cpp       : 标准 Timer0/Timer1 模式 0~3 计数模型 (无 Timer3/4/W0C)│
│    ├── mcs51_uart.cpp        : 标准 UART0 模式 0~3 串行通信 (固定引脚与时钟源)      │
│    ├── mcs51_isr.cpp         : 标准 0~5 号向量中断仲裁器 (INT0, T0, INT1, T1, UART0)│
│    └── mcs51_adc.cpp         : 纯物理模拟轨直通转换 (接收物理 Pin 编号，无 32+ 偏置) │
└──────────────────────────────────────┬──────────────────────────────────────────┘
                                       │ 装载 (Table-Driven / Capabilities)
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│ 2. 芯片家族描述符与能力钩子层 (McuFamilyDescriptor v2 & Trait Hooks · CPL-20 前置)│
│    ├── McuFamilyDescriptor v2 : xram_size, xsfr_base/size, fosc_default,         │
│    │                           irq_vector_table + irq_count, port_pin_masks,     │
│    │                           default_pins, wdt_present, iap_present,           │
│    │                           uart_count, timer_caps                            │
│    ├── GpioTraitHooks        : may_drive_hook, is_analog_hook, pullup_hook      │
│    └── SfrWriteHookTable     : 按需注册各芯片专有 SFR/WDT 保护拦截器              │
└──────────────────────────────────────┬──────────────────────────────────────────┘
                                       │ 模块化静态注册 / 按 Target 编译
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│ 3. 厂商芯片自治扩展包 (chips/ - 各自独立命名空间与头文件)                         │
│    ├── chips/cms8s78xx/ (编译为 wink_mcs51_cms8s)                               │
│    │   ├── include/cms8s_sfr_map.h : 专有寄存器 (原 mcs51_sfr_map.h 归位)        │
│    │   ├── include/REG_CMS8S78XX.H : 原厂芯片头文件与 WinkXsfr 代理下沉          │
│    │   ├── src/cms8s_adc.cpp       : 12-bit SAR ADC + AN0..25 引脚映射 (解决E-02)│
│    │   ├── src/cms8s_buzzer.cpp    : BUZDIV/BUZCON 硬件蜂鸣发生器               │
│    │   ├── src/cms8s_gpio.cpp      : PxxCFG 模拟复用 / PxTRIS / PxUP / 开漏     │
│    │   ├── src/cms8s_uart.cpp      : FUNCCR 多时钟源 / PS_RXD 引脚重映射        │
│    │   ├── src/cms8s_timer.cpp     : Timer3/4 扩展计数器 / W0C 中断标志拦截     │
│    │   └── src/cms8s_sys.cpp       : 内部看门狗 WDT / TA 保护窗口 / CLKDIV 分频 │
│    └── chips/at89c52/ (编译为 wink_mcs51_at89)                                  │
│        └── src/at89_bus.cpp        : MOVX 外部物理总线与 P0/P2 占用模拟         │
├─────────────────────────────────────────────────────────────────────────────────┤
│ 4. 板级器件层 (devices/ · CPL-21：外挂器件不进 core/chips)                        │
│    └── devices/adc0832/            : 外挂 ADC0832 状态机，经 trap 注册接入       │
├─────────────────────────────────────────────────────────────────────────────────┤
│ 5. 契约与测试治理 (CPL-22/23/24：与代码迁移同版本发版)                           │
│    ├── core-tests vs cms8s-tests 分层；host 回退双轨                             │
│    ├── wasm C-ABI 引脚语义版本化，双读过渡一版                                    │
│    └── wink_mcs51_* 前缀门禁 + STRICT 枚举按家族作用域拆分                       │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 4.1 终态整体目录结构（可直接 `mkdir` 落地）

> 约定：顶层 `include/ + src/` 即 `core` 本体（= `wink_mcs51_core`），避免全仓大改名；
> `chips/*`、`devices/*` 各自独立命名空间与构建目标；`tools/manifests` 为唯一工具链事实源。

```
wink-micro-os/frameworks/mcs51/                # ← 本审计所属模块根
├── CMakeLists.txt                             # ESP_PLATFORM 守卫；定义 wink_mcs51_core / _cms8s / _at89 / _adc0832（STATIC EXCLUDE_FROM_ALL）；STRICT 改 per-target；WINK_MCS51_XDATA_SIZE 下沉板级（CPL-15/24）
│
├── include/                                   # ★ 通用 core 公共头：严禁 vendor 名/0xFxxx/ADCLDO/FUNCCR/PS_xx（CPL-13/14/24）
│   ├── mcs51_context.h                        # 仅标准核状态 + void*soc_priv；无 adc0832/sysProt/buzzer/cms8sAdc、无 T3/T4、无 port_pins、无厂商命名 XRAM 宏（CPL-11/18/19）
│   ├── mcs51_family.h                         # McuFamilyDescriptor v2 schema：fosc/ckcon/xram/xsfr + irq_table+count/port_pin_masks/wdt_present/iap_present/uart_count/timer_caps（CPL-20）
│   ├── mcs51_peripheral.h / mcs51_trap.h       # 通用注册/钩子机制；注释不点名厂商（CPL-10）
│   ├── mcs51_sfr_map.h                        # ★ 缩水为 Intel 标准 SFR（P0/P1/P2/P3/SP/PCON/TCON/TMOD/TL/TH/SBUF…）；CMS8S 专有全部下沉（CPL-13）
│   ├── mcs51_adc.h                            # 纯物理 Pin 模拟 rail：无 32+ch、无 ADCLDO、无 adc0832 shim（CPL-01/02/21）
│   ├── wink_mcs51_{gpio,uart,timer,isr,extint,clock,pcon,edge_queue}.h
│   ├── wink_mcs51_strict.h                    # 仅通用枚举；IAP/RC 等按家族拆分（CPL-24）
│   ├── wink_mcu.h                             # 仅路由，不直含 REG_CMS8S78XX.H（CPL-14）
│   └── reg51.h / REGX52.H / absacc.h / intrins.h   # 通用 8051 标准头保留
│
├── src/                                       # ★ 通用 core 实现：grep cms8s/CMS8S/0xF0 /ADC0832/BUZ/WDT-TA 必须零命中（阶段4验收）
│   ├── mcs51_context.cpp                      # 仅 Intel 标准种子（P0~P3/SP/PCON）；XSFR/ADCLDO/CKCON/Fosc 下放芯片包（CPL-19）
│   ├── mcs51_family.cpp                       # 两行：classic + cms8s78xx 描述符行；加新芯片只加行（CPL-20）
│   ├── mcs51_gpio/uart/timer/isr/extint/xdata.cpp  # 标准模型；XSFR/重映射/W0C/T3T4/端口中断全部经钩子/描述符外包（CPL-03~08）
│   ├── mcs51_adc.cpp                          # 物理 Pin 直通；无 Vref/Vrail 缩放私货（缩放归芯片层）（CPL-01/02）
│   ├── mcs51_bridge.cpp / mcs51_uni_bridge.cpp # 无 ADC0832/cms8s 裸 include；TA 硬调用改钩子派发；host 回退数组随契约版本双轨（CPL-09/22）
│   ├── mcs51_peripheral.cpp                   # 仅 core 三件套（timer/uart/extint）；cms8s/* 由芯片包自注册（CPL-10）
│   └── mcs51_{sfr,pcon,clock,edge_queue,pwm_meter,unsupported}.cpp
│
├── chips/                                     # ★ 厂商芯片自治包：允许出现 vendor 名/专有寄存器（CPL-09/10/13/14）
│   ├── cms8s78xx/                             # 编译为 wink_mcs51_cms8s；经典 target 不链接
│   │   ├── include/
│   │   │   ├── cms8s_sfr_map.h                # 原 mcs51_sfr_map.h 归位：P0EXTIF/CKCON/T34MOD/EIE2/EIF2/PS_* → CMS8S_ 前缀
│   │   │   ├── cms8s_xsfr_allowlist.h         # 原 mcs51_xsfr_allowlist.h 下沉
│   │   │   ├── REG_CMS8S78XX.H / cms8s78xx.h / cms8s_adc.h / cms8s_buzzer.h
│   │   │   └── cms8s_priv.h                   # Mcs51SysProt/Buzzer/Cms8sAdc/T3T4/端口采样状态，经 soc_priv 挂载（CPL-11/18）
│   │   └── src/
│   │       ├── cms8s_adc.cpp                  # AN0..25→物理 Pin 闭环 + ADCLDO.VSEL 基准（CPL-01/02）
│   │       ├── cms8s_gpio.cpp                 # PxxCFG/PxTRIS/PxUP/开漏（CPL-03）
│   │       ├── cms8s_uart.cpp                 # FUNCCR/PS_RXD（CPL-04）
│   │       ├── cms8s_timer.cpp                # Timer3/4 + W0C（CPL-05）
│   │       ├── cms8s_extint.cpp               # 端口中断 + 引脚选择（CPL-07）
│   │       └── cms8s_sys.cpp                  # WDT/TA/CLKDIV/IAP（CPL-09）
│   └── at89c52/                               # 编译为 wink_mcs51_at89；CMS8S target 不链接
│       ├── include/at89_priv.h
│       └── src/at89_bus.cpp                   # MOVX 外总线 + P0/P2 占用；classicBus 状态归此（CPL-18）
│
├── devices/                                   # ★ 板级外挂器件：既不进 core 也不进 chips（CPL-21）
│   └── adc0832/
│       ├── include/adc0832.h
│       └── src/mcs51_adc0832.cpp              # 经 trap 注册接入；MCS51_HAS_ADC0832 作用域限本目录/app
│
├── tools/
│   ├── manifests/chips/
│   │   ├── classic.yaml  / cms8s78xx.yaml      # 头正则/内存上限/SDCC 门禁/清洗规则唯一事实源（CPL-16）
│   │   └── schema.json
│   ├── mcs51_sdcc_gate.py / mcs51_cleanup.py  # 改为读 manifests；禁硬编码 MEM_LIMITS/cms8s 正则（CPL-16）
│   └── lint/                                  # 前缀门禁（wink_mcs51_*/cms8s_*/board_*）+ 通用头 vendor 残留扫描（CPL-24）
│
└── test/                                      # 迁移安全网本身先解耦（CPL-22）
    ├── core/                                  # 物理 Pin、标准 0~5 向量、标准 SFR；禁合成通道/扩展向量
    └── cms8s78xx/                             # AN 映射、扩展向量、XSFR 窗口、WDT/TA/CLKDIV
```

依赖方向（单向，禁回指）：`devices/chips → core（含 family/trap）`；`core → PAL/运行时`；`tools/test → manifests + 公共头`；`core` 永不反向依赖 `chips/devices` 任一符号。

---

## 5. 建议执行顺序与分阶段迁移计划（全量 24 项路线图）

为了稳妥推进且不破坏现有的 40/40 单元测试与 UniSim 无头场景，建议按以下 **8 个阶段分步迁移**：

### 阶段 0：描述符与命名门禁先行（CPL-20, CPL-24 前置，无生产行为变更）
* **涉及编号**：`CPL-20`, `CPL-24`
* **改动范围**：
  1. 冻结 `McuFamilyDescriptor v2` schema（`port_pin_masks/irq_vector_table/wdt_present/iap_present/uart_count/timer_caps`），`mcs51_family.cpp` 只加行；
  2. 立 `wink_mcs51_*` 仅通用、`cms8s_*` 归厂商、`board_*` 归板级的前缀规则，STRICT 枚举按家族作用域拆分，以 lint 锁死。
* **验收标准**：新增字段有单测覆盖；lint 对越权前缀/枚举 fail；机制文件零行为变更，40/40 全绿。

### 阶段 1：紧急纠偏——解决在线仿真假短路与引脚错位（立即生效）
* **涉及编号**：`CPL-01`, `CPL-02`, `CPL-17`, `CPL-22`（测试双轨）, `CPL-23`（ABI 双读）
* **改动范围**：
  1. `mcs51_adc.cpp` 提供接收物理 Pin 的纯净接口，废除 `32 + ch` 假定；
  2. `cms8s_adc.cpp` 严格按原厂手册自主完成 `AN 通道 -> 芯片引脚` 闭环计算（AN0->Pin 0），芯片层兼容双读一版（合成通道告警+重定向），向前兼容通道 32 旧单元测试；
  3. UniSim 前后端物理引脚契约升版拉齐，按“仿真后端 → 前端插件 → stub/文档”顺序发版；
  4. 测试拆 `core-tests` 与 `cms8s-tests`，迁移期双绿。
* **验收标准**：养生壶在 `embedded-frontend` 在线仿真中冷启动正确读出 NTC 室温阻值（约 25℃），数码管不再报 `E-02`；旧合成通道用例告警但仍通过。

### 阶段 2：数据结构纯净化——解耦 `Mcu51Context`
* **涉及编号**：`CPL-11`, `CPL-12`, `CPL-18`, `CPL-19`
* **改动范围**：
  1. 将 `Mcu51Context` 中的 `adc0832`, `sysProt`, `buzzer`, `cms8sAdc` 私有状态移出核心结构体，通过 `soc_priv` 动态/静态挂载；
  2. 将 `Mcu51TimerState` 中的 T3/T4/捕获比较状态、`Mcu51ExtIntState` 中的全端口采样状态移入 `cms8s_*_priv`，`classicBus` 移入 `at89_bus_priv`，`isr_table` 容量与 XDATA 影子按描述符裁剪或文档化超配理由；
  3. 修复 `mcs51_timer.cpp` 中硬编码的 `{8, 8, 6, 4}` 封装引脚限制，从 `McuFamilyDescriptor.port_pin_masks` 获取；
  4. 通用复位仅保留 Intel 标准种子，XSFR 选择器/ADCLDO 参考轨/CKCON/Fosc 下放芯片包播种，厂商命名 XRAM 宏下沉。
* **验收标准**：通用核心头文件 `mcs51_context.h` 中彻底杜绝任何厂商专有结构体声明与厂商命名宏；经典 8051 实例占用最简纯净内存；经典复位后 XDATA 影子无 XSFR 残留。

### 阶段 3：头文件体系与命名空间归位
* **涉及编号**：`CPL-09`, `CPL-13`, `CPL-14`, `CPL-21`（ADC0832 下沉至 `devices/`）, `CPL-24`（前缀门禁落地）
* **改动范围**：
  1. 建立 `chips/cms8s78xx/include/`，将 `REG_CMS8S78XX.H`、`cms8s78xx.h`、`cms8s_adc.h`、`cms8s_buzzer.h` 移入；`wink_mcu.h` 上移出 `frameworks/mcs51/`，51 框架内仅保留 51 家族路由；
  2. 将伪装成通用的 `mcs51_sfr_map.h` 拆解重命名为 `cms8s_sfr_map.h`，`MCS51_XRAM_SIZE_CMS8S78XX` 等厂商命名宏同步下沉（`T2CON` 等标准寄存器保留通用，`CKCON` 地址可提、语义下沉）；
  3. `wink_mcs51_wdt.h` 移除 `cms8s_sys_notify_sfr_write`，改为通用 `sfr_write_hooks` 派发，`wink_mcs51_wdt_*` 仅保留真通用语义或改名下沉；
  4. `mcs51_bridge.cpp` 移除对器件头文件的直接 include；
  5. `mcs51_adc0832.*` 与 `adc0832_*` shim 下沉至 `devices/`，经 trap 注册接入。
* **验收标准**：公共 `include/` 下仅保留通用 8051 规范头文件，编译 `mcs51_bridge.cpp` 不再强依赖特定芯片/器件头文件；前缀 lint 全绿。

### 阶段 4：外设增强机制与私有外设剥离
* **涉及编号**：`CPL-03`, `CPL-04`, `CPL-05`, `CPL-07`, `CPL-10`
* **改动范围**：
  1. 新增 `cms8s_gpio.cpp`，承接 `PxxCFG`、`PxTRIS`、`PxUP`、`PxOD`，通过 GPIO 钩子接入；
  2. 新增 `cms8s_uart.cpp`，承接 `FUNCCR` 与 `PS_RXD`；
  3. 新增 `cms8s_timer.cpp`，承接 `Timer3/4` 与 `W0C` 标志；
  4. 新增 `cms8s_extint.cpp`，承接端口引脚中断；
  5. `mcs51_peripheral.cpp` 改为支持模块动态/按需注册。
* **验收标准**：`mcs51_gpio.cpp`、`mcs51_uart.cpp`、`mcs51_timer.cpp` 全文搜索不到任何 `cms8s` 或中微寄存器地址。

### 阶段 5：中断与总线寻址表驱动化
* **涉及编号**：`CPL-06`, `CPL-08`
* **改动范围**：
  1. 中断向量表完全由 `McuFamilyDescriptor` 配置化加载，通用 ISR 核心仅提供标准 0~5 向量仲裁；
  2. XSFR 校验与白名单移至芯片模型，`mcs51_xdata.cpp` 仅根据描述符窗口分发，移除 `mcs51_xsfr_allowlist.h` 强包含。
* **验收标准**：切换至 AT89 模式时，中微专有中断与 XSFR 窗口物理绝缘。

### 阶段 6：构建系统与工具链配置化治理
* **涉及编号**：`CPL-15`, `CPL-16`, `CPL-24`
* **改动范围**：
  1. CMake 拆分为 `wink_mcs51_core` 与 `wink_mcs51_cms8s`，解决单体库强制链接与板级宏污染；STRICT 改为 per-target 定义（避免 2×2 四库膨胀），`WINK_MCS51_XDATA_SIZE` 下沉板级；
  2. `mcs51_sdcc_gate.py`、`mcs51_cleanup.py` 改为由芯片描述清单（Manifest）驱动，移除 Python 脚本内的硬编码正则。
* **验收标准**：新增一款国产 51 芯片只需添加描述文件与芯片外设目录，无需修改通用核心任何一行代码与工具链脚本。

### 阶段 7：测试与契约收尾（CPL-22, CPL-23 关闭项）
* **涉及编号**：`CPL-22`, `CPL-23`
* **改动范围**：
  1. 删除合成通道双读兼容层，`core-tests`/`cms8s-tests` 各自全绿后合入主线；
  2. 归档 ABI 版本升级记录（版本号、发版顺序、回滚步骤），`SimTraceSpecV2` 与前端视窗 DTO 同步定稿。
* **验收标准**：无告警兼容代码残留；跨仓契约文档与实现一致，可回滚。
