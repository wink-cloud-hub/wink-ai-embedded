# CMS8S78xx I2C 与 SPI 硬件总线轮询死锁分析与双仓协同解决实施计划

> 📋 **本文档是 CMS8S78xx 官方示例中 I2C 与 SPI 硬件总线轮询死锁问题的深度分析与正式实施计划（Layer-③）**。  
> 遵循 [00-IMPLEMENTATION-PLAN-TEMPLATE.md](../00-IMPLEMENTATION-PLAN-TEMPLATE.md) 规范，吸收资深嵌入式架构评审意见，针对 [CMS8S78XX_EXAMPLE_CHECKLIST.md](../../vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md) 第 8 章节（编号 37、38）被标记为 `🚫 Blocked` 的子示例，提出彻底根治方案与双仓协同推进路线。

---

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260921-CMS8S78XX-I2C-SPI-DEADLOCK` |
| **创建日期** | `2026-09-21` |
| **最新修订** | `2026-09-22`（v2.1：全链路收尾——43 app 资产全量重建 + hash 运行时校验、legacy SPI 同帧路径实现、证据聚合脚本修复、HEAD 既存失败基线归因） |
| **目标平台/SoC** | `host` (GCC/MSVC C++17), `wasm` (Emscripten) 基于 `wink-micro-os/frameworks/mcs51` |
| **工具链/运行时** | GCC 11+, MSVC 19+, Emscripten 3.1+, 原厂 `CMS8S78xx_DemoCode_V2.0.2`；UniSim 侧 Bun 1.x |
| **计划状态** | 🟢 Phase 1 已完成；🟢 **Phase 2 已完成**（T2.0–T2.6 全部落地并实证；Checklist 37/38 已正式摘牌） |
| **优先级** | 🔴 P1（Checklist 列级优先级为 P3；两者维度不同，见 T2.5） |
| **计划版本** | `v2.1` |
| **关联技术设计** | 原厂 `CMS8S78xx` 参考手册（SPI/I2C 章节）；[04-wasm-simulation](../../../zh/design/04-wasm-simulation/00-README.md)；`wink-ai/packages/unisim/docs/internals/decisions/0085/0086/0087/0088`（私有仓 ADR） |
| **关联设计规范** | [07-mcs51-simulation-interception](../../../zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md)（§2.9 CH2 会话契约）；[04-wasm-simulation](../../../zh/design/04-wasm-simulation/00-README.md) |
| **关联合格清单** | [`docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md`](../../vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md) §8（编号 37 `I2C_Master_AT24C256`、38 `SPI_Master_95256`，均 `[x]`） |
| **关联 ADR** | [ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)（静态分发）、[ADR-0070](../../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md)（零侵入拦截）、[ADR-0071](../../decisions/core/0071-sfr-proxy-rmw-edge-data-plane.md)（SFR 代理数据面）、[ADR-0072](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md)（双时钟/配额）、[ADR-0075](../../decisions/core/0075-mcs51-production-wasm-target-headless.md)（headless 实证）、[ADR-0076](../../decisions/core/0076-mcs51-sim-backends-native-vs-iss-channel-roadmap.md)（仿真后端路线）、[ADR-0081](../../decisions/core/0081-uart-tx-per-byte-synchronous-charge.md)（整字节同步记账）、[ADR-0082](../../decisions/core/0082-mcs51-reset-semantics-fiber-exit-and-reentry.md)（复位/纤程语义）；跨仓 **ADR-0085**（I2C 整事务状态化 `js_pal_i2c_transfer_ex`，Accepted）、**ADR-0086**（I2C 控制器会话流，Accepted）、**ADR-0087**（SPI 会话流 + CS 边沿，Accepted）、**ADR-0088/ADR-X1**（通用 ABI 返回值模型，Proposed；私有仓归档） |
| **跨仓依赖关系** | **双仓联动**：`wink-ai-embedded`（SFR 拦截与片内模型）+ `wink-ai`（`@wink-ai/unisim` 总线/插件与 ABI 实现） |
| **目标里程碑** | 1. 消除 `while(!flag)` 硬件标志轮询导致的仿真不收敛（业务永不推进），而非"宿主冻结"；<br>2. 在 `wink-ai-embedded` 实现 CMS8S78xx SPI/I2C SFR 拦截、状态机读清锁存与虚拟时钟推进；<br>3. 以 SFR 层自旋防护替代不可实现的 PC 看门狗（防御纵深，非主修复手段）；<br>4. 经 ADR-0085/0086 落地 ABI 后，在 UniSim 实现虚拟 EEPROM 插件并完成端到端数据校验；<br>5. 官方示例源码一行不改，编译输出 Wasm 资产，Headless 测试 100% 绿灯。 |
| **所需技能** | `embedded-best-practice` |

---

## 2. 问题深度剖析（Root Cause Analysis）

### 2.1 阻断现状
在 [`CMS8S78XX_EXAMPLE_CHECKLIST.md`](../../vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md) 的第 8 章节中，以下两项被明确标为 `🚫 Blocked`：
* **No. 37**: `I2C/I2C_Master_AT24C256/code`（硬件 I2C 主机读写 AT24C256，需虚拟从机支持，否则死锁）
* **No. 38**: `SPI/SPI_Master_95256/code`（硬件 SPI 主机读写 M95256，需硬件传输标志自动打拉，否则死锁）

### 2.2 源码级死锁机理
查阅官方原始源码，两个子示例均采用了标准的**同步阻塞轮询硬件中断/完成标志位**模式：

#### ① SPI 传输死循环 (`demo_spi.c`)
```c
uint8_t SPI_Transmit(uint8_t Data) {	
    SPDR = Data;                       // 1. 向数据寄存器写入待发字节 (SFR 0xEE)
    while(!SPI_GetTransferIntFlag());  // 2. 轮询 SPSR & 0x80 (SPSR bit 7: SPISIF 传输完成标志)
    return (SPDR);                     // 3. 读取接收缓冲并自动清标志
}
```
* **物理真机行为**：
  1. 向 `SPDR` (SFR `0xEE`) 写入字节后，硬件移位寄存器接管引脚，以设定波特率产生 SCLK 并串行收发 8 个 bit；
  2. 传输完成后，硬件逻辑自动将 `SPSR.SPISIF`（bit 7，`SPSR` 为 SFR `0xED`）置 1；
  3. CPU 检测到非 0 后跳出循环；
  4. 按厂商 `StdDriver/src/spi.c` 的 `SPI_ClearTransferIntFlag()`（`temp = SPSR; temp = SPDR;`），标准读清时序是**先读 SPSR、再读 SPDR**；执行 `return (SPDR);` 读取数据寄存器时，硬件自动将 `SPSR.SPISIF` 复位为 0（写 SPDR 是否可清无本地证据，不采纳）。
* **当前仿真断层**：在当前 `frameworks/mcs51` 中：
  1. `SPCR` (`0xEC`)、`SPSR` (`0xED`)、`SPDR` (`0xEE`)、`SSCR` (`0xEF`) 甚至未在 `REG_CMS8S78XX.H` 中定义；
  2. 固件执行 `SPDR = Data` 即使存入内存影子，也**没有任何硬件仿真钩子感知**；
  3. 紧随其后的 `while(!SPI_GetTransferIntFlag())` 不断读取 `sfr_shadow[0xED]`，其值恒为 0，陷入死循环！

#### ② I2C 传输死循环 (`demo_i2c.c`)
```c
int16_t At24c256_write_byte(uint16_t addr, uint8_t ch) {
    ...
    I2C_MasterWriteAddr(AT24C256_WRITE);       // I2CMSA (0xF4) = 0xA0
    I2C_MasterWriteBuffer((addr>>8) & 0xff);    // I2CMBUF (0xF6) = high_addr
    I2C_SendMasterCmd(I2C_MASTER_START_SEND);   // I2CMCR (0xF5) = START (bit1) | RUN (bit0)
    while(!(I2C_GetMasterIntFlag()));          // 轮询 I2CMSR (0xF5) & 0x80 (I2CMIF)
    I2C_ClearMasterIntFlag();                  // I2CMSR = 0x00 清除中断标志
    ...
}
```
* **物理真机行为**：
  1. 向 `I2CMCR` (SFR `0xF5`) 写入 `START | RUN` 后，I2C 硬件控制器状态机启动：产生 START 信号，串行发出从机地址并检测从机 ACK；若 ACK 正常，继续发送 `I2CMBUF` 数据字节并等待 ACK；
  2. 事务完成后，硬件置位 `I2CMSR.I2CMIF`（bit 7），同时更新 `ADD_ACK` (bit 2)、`DATA_ACK` (bit 3)；
  3. CPU 轮询命中后跳出循环，并通过软件写 `I2CMSR = 0x00` 清除标志。
* **当前仿真断层**：
  1. `I2CMCR` / `I2CMSR` 虽复用同一个 SFR 地址 `0xF5`（写为控制寄存器，读为状态寄存器），但框架没有安装读写 Trap 钩子；
  2. 固件在写完 `I2CMCR` 后，`I2CMSR` 恒为 0，直接死在第一个字节的 `while(!(I2C_GetMasterIntFlag()))`。

### 2.3 死循环在仿真中的真实后果（v1.2 纠正）
`frameworks/mcs51` **不是指令级解释器**：固件经 Keil C51 转译后 native 编译，执行流只在 **SFR/XDATA 代理访问点**产生 microstep。因此 v1.1 中"紧凑自旋不调用 Wasm 导入、虚拟时钟不前进、宿主事件循环彻底饥饿"的描述与本框架不符，实际链路是：

1. `while(!SPI_GetTransferIntFlag())` 每圈读取 SPSR，经 `mcs51_proxy.hpp` → `wink_mcs51_on_sfr_read()` → `wink_mcs51_microstep()`（`src/mcs51_bridge.cpp`）；
2. 每次 microstep 固定 `wink_mcs51_charge_us(WINK_MCS51_MICROSTEP_US=5µs)`，累计超过 `WINK_MCS51_QUOTA_US=10ms` 配额时 `cooperative_yield()`（`src/mcs51_clock.cpp`，ADR-0072）；
3. 因此宿主事件循环**会被让出**，定时器/ISR 可继续调度；真正的症状是 **业务永不推进、固件卡在该函数、Headless 场景超时**，而不是无声冻结浏览器；
4. 转译器已对**空无限循环**注入 `_nop_()` 保证微步进（`tools/transpile_app_keil_c51.py`），但**有限软件延时循环**（`for(i..)for(j..)`）不含任何 SFR 访问，仿真内核无法在其间插入 microstep——这也是"PC 自旋看门狗"不可实现、且不能误杀延时的根本原因（详见 §5.4 重设计）。

> 结论：本计划的根因是"**SFR 未被拦截 → 标志位恒 0 → 业务不收敛**"，主修复手段是片内外设模型（T1.2/T1.3）；看门狗只是防御纵深。

---

## 3. 核心裁决：修改哪个仓？

### 3.1 两仓职责与能力矩阵对比

| 维度 | `wink-ai-embedded` (WinkMicroOS) | `wink-ai` (UniSim) |
| :--- | :--- | :--- |
| **层级归属** | **芯片内核与片内外设层 (SoC Silicon Layer)** | **板级总线拓扑与片外器件层 (Board/Bus Layer)** |
| **感知范围** | 8051 SFR 读写、中断向量派发、微步进/配额让出、虚拟时钟计费 | WASM imports（`js_pal_*`）、总线/插件契约与虚拟时间线 |
| **已具备能力** | • `REG_CMS8S78XX.H` 寄存器体系（ADC/UART/Timer 等已有模型）<br>• `mcs51_uart` 写入 SBUF 同步 `charge_us` 后置 `TI` 的范式（ADR-0081）<br>• wasm 侧已声明 `js_pal_i2c_transfer` import（catalog: implemented）<br>• `js_pal_spi_transfer` import（catalog: **stub**，未生产可用）<br>• SFR hook 机制 `mcs51_trap_register_sfr_read/write` + `sfr_shadow` 数据面 | • `I2CBus`/`SPIBus` 总线调度（**整事务/整帧级**，插件只见 `onTransfer`/`onFrame`）<br>• `unisim-bridge-factory.ts` 桥接存在（**slice 堆拷贝，非零拷贝**）<br>• Headless 场景断言与时间线捕获引擎 |
| **当前缺失** | • **无 `SPCR`/`SPSR`/`SPDR`/`SSCR` SFR 读写拦截模型**<br>• **无 `I2CMCR`/`I2CMSR` 状态机模型**<br>• 未把片内总线事件转为 `js_pal_*` 调用<br>• host 构建缺 `js_pal_i2c/spi_transfer` fallback（否则 CTest 链接失败）<br>• 缺少基于 SFR 层的自旋防护 | • **缺少片外 AT24C256 / M95256 虚拟器件插件**<br>• **总线只到"整事务"层，缺 repeated START / 逐字节 ACK / CS 边沿可见性**（ADR-0086 目标）<br>• SPI 通道为 stub，`device_id`/CS 映射契约未定 |

### 3.2 裁决结论
1. **解决死锁的第一责任 100% 属于 `wink-ai-embedded` 仓**：
   * 固件读写的是 8051 片内 SFR；不拦截 SFR，`unisim` 连 1 个 bit 的事件都收不到；
   * **单改 `unisim` 绝对无法解除死锁**。
2. **"全保真数据闭环与自动化实证"依赖 ADR-0085/0086 先落地**：
   * `wink-ai-embedded` 负责片内控制器模型与协议封包；
   * `unisim` 需先完成 ABI 扩展（`js_pal_i2c_transfer_ex` / `js_pal_i2c_session_*`）与**引擎线级事件接线**，否则 repeated START 与逐字节 ACK 不可表达，AT24C256 随机读无法保真；
   * Phase 1 仅能达成"运行不挂 + 时序合理"，**Checklist 摘除 Blocked 应以 Phase 2 数据校验为准**（见 T2.5）。

---

## 4. 架构设计与双仓通信拓扑

### 4.1 整体数据流向图

```
┌────────────────────────────────────────────────────────────────────────┐
│                        wink-ai-embedded 仓                             │
│                                                                        │
│  [官方源码 demo_i2c.c / demo_spi.c] (保持一行不改，原厂零侵入)         │
│       │ 写 SPDR / I2CMCR / SSCR   ▲ 读 SPSR / I2CMSR (获取完成标志)    │
│       ▼                           │                                    │
│  [wink-micro-os/frameworks/mcs51/chips/cms8s78xx: cms8s_spi.cpp & cms8s_i2c.cpp] │
│       │ ① 状态机拦截、双步读清锁存跟踪、精准更新状态字                 │
│       │ ② 计算传输开销, 推进虚拟时间 (wink_mcs51_charge_us)            │
│       │ ③ 片选边沿记录（SSCR，Phase 1 仅测试断言，供 SPI 会话 ADR）    │
│       │ ④ SFR 层自旋防护（诊断优先，见 §5.4）                          │
│       ▼                                                                │
│  [Wasm FFI Bridge: targets/wasm/wink_sim_js.js]                        │
│       │ 调用导入: js_pal_i2c_transfer_ex / js_pal_i2c_session_*        │
│       │（Phase 2，依赖 ADR-0085/0086 落地；SPI 通道待独立 ADR）        │
└───────┼────────────────────────────────────────────────────────────────┘
        │ (WASM / JS 指针仅调用期有效；桥接为堆拷贝)
┌───────┼────────────────────────────────────────────────────────────────┐
│       ▼                                                                │
│  [Unisim 桥接层: I2C/SPI 通道转发（非零拷贝）]                          │
│       │                                                                │
│       ▼                                                                │
│  [UniSim Bus Core: I2CBus / SPIBus]                                    │
│       │ 依据总线端口和从机地址 / 片选引脚路由                          │
│       ▼                                                                │
│  [UniSim 虚拟外设插件: i2c_eeprom / spi_eeprom（builtin 插件布局）]    │
│       │ • AT24C256: 32KB 存储, 页写边界, 线级 repeated START 会话      │
│       │ • M95256: CS 帧边界/WEL 锁存 —— 依赖 SPI 线级契约（另开 ADR） │
│                                                                        │
│                         wink-ai 仓 (unisim)                            │
└────────────────────────────────────────────────────────────────────────┘
```

### 4.2 方案分层演进路径

为确保以最小风险快速解除阻塞并稳妥交付全保真验证，分为两阶段：

* **阶段 1：内核解阻断与单仓自洽闭环（Phase 1: Pure Embedded Unblocking）**
  * **修改范围**：仅限 `wink-ai-embedded` 仓；
  * **核心做法**：
    1. 在 SFR hook 层实现硬件传输标志的**同步自动打拉（Synchronous Auto-assert）**与**双步读清时序（Read SPSR → Read SPDR）**，状态镜像写回 `ctx->sfr_shadow`；
    2. 实现 `SSCR` 片选寄存器与 `I2CMCR` 命令状态机（含 STOP 行为级偏差声明）；
    3. Phase 1 Mock **协议无关**：SPI 统一返回 `0xFF`/`0x00`，I2C 默认 ACK；**不在片内模型内置 M95256/AT24C256 协议语义**（片外器件语义归 Phase 2 插件，避免分层越界）；
    4. 增加 **SFR 层自旋防护**（非 PC 看门狗，见 §5.4）；
  * **达标效果**：
    1. 固件 `while(!flag)` 按时序正常跳出；
    2. 官方示例成功编译输出 `unisim-assets` 三件套；
    3. CTest 单元测试覆盖 I2C/SPI 寄存器行为；
    4. Checklist §8 编号 37、38 暂标"运行已解除阻塞（Phase 1 证据）"，**正式摘牌延后到 Phase 2 数据校验**。
* **阶段 2：端到端虚拟器件全保真闭环（Phase 2: Dual-Repo High-Fidelity Loop）**
  * **前置依赖（硬）**：ADR-0085/0086 评审 Accepted → 七步 ABI hash 同步落地（header/TS/引擎）→ catalog `proposed → implemented`；
  * **修改范围**：`wink-ai-embedded` + `wink-ai` (unisim)；
  * **核心做法**：
    1. `wink-ai-embedded` 按 ADR-0085/0086 调用 `js_pal_i2c_transfer_ex`（整事务）与 `js_pal_i2c_session_*`（逐字节/repeated START）；
    2. `unisim` 交付 builtin 插件 `i2c_eeprom`（24C 系列）与 `spi_eeprom`（25/95 系列，CS 帧契约另行立项），并**接线线级事件**（`onTransactionStart/onExchangeByte/onTransactionEnd`）；
    3. 插件经 `wink-app.json` 声明并在运行时生成 `unisim-assets/device-tree.json`；
  * **达标效果**：
    1. 原厂 `At24c256_read_str()` 与 `SPI_M95256_Read_Data()` 读回写入的真实数据；
    2. Headless 断言：I2C 经插件通道或串口输出校验；SPI 因官方 demo 无串口逻辑且 `value` 为读后即死的局部变量（引擎无变量级内存断言），**只允许走插件通道**（如 `plugin:spi_eeprom/...`）校验，100% 绿灯。

---

## 5. 详细实现技术规格（融合 ADR-0085/0086 v1.3 契约）

### 5.1 物理寄存器地址与位掩码基准校验（纠正原稿偏差）

经过对原厂 `cms8s78xx.h` 与参考手册的严格交叉核对，物理地址与位掩码严格定义如下：

```c
// =============================================================================
// SPI 相关 SFR 物理地址定义 (严禁错位)
// =============================================================================
sfr SPCR  = 0xEC; // SPI 控制寄存器 (原稿曾误记为 0xEB，现正本清源)
sfr SPSR  = 0xED; // SPI 状态寄存器 (原稿曾误记为 0xEC)
sfr SPDR  = 0xEE; // SPI 数据寄存器
sfr SSCR  = 0xEF; // SPI 从机选择控制寄存器 (bit 1 控制 NSSO1 / CS 片选)

// SPCR 位掩码
#define SPI_SPCR_SPEN_Msk    (0x40) // bit 6: SPI 模块使能
#define SPI_SPCR_SPR2_Msk    (0x20) // bit 5: 时钟分频高位
#define SPI_SPCR_MSTR_Msk    (0x10) // bit 4: 主机模式使能
#define SPI_SPCR_CPOL_Msk    (0x08) // bit 3: 时钟极性
#define SPI_SPCR_CPHA_Msk    (0x04) // bit 2: 时钟相位
#define SPI_SPCR_SPRn_Msk    (0x03) // bit 1..0: 时钟分频低位

// SPSR 位掩码
#define SPI_SPSR_SPISIF_Msk  (0x80) // bit 7: SPI 传输完成中断标志
#define SPI_SPSR_WCOL_Msk    (0x40) // bit 6: 写冲突碰撞标志
#define SPI_SPSR_SSCEN_Msk   (0x01) // bit 0: NSS 自动控制使能

// SSCR 控制位
#define SPI_SSCR_NSSO1_Msk   (0x02) // bit 1: NSSO1 输出引脚电平控制

// =============================================================================
// I2C 相关 SFR 物理地址定义
// =============================================================================
sfr I2CSADR = 0xF1; // 从机地址
sfr I2CSCR  = 0xF2; // 从机控制
sfr I2CSSR  = 0xF2; // 从机状态
sfr I2CSBUF = 0xF3; // 从机数据
sfr I2CMSA  = 0xF4; // 主机从机地址 (bit 7..1: 7-bit Addr, bit 0: R/W)
sfr I2CMCR  = 0xF5; // 主机控制寄存器 (写)
sfr I2CMSR  = 0xF5; // 主机状态寄存器 (读)
sfr I2CMBUF = 0xF6; // 主机数据缓冲
sfr I2CMTP  = 0xF7; // 主机时钟分频寄存器

// I2CMCR 写控制命令掩码
#define I2C_I2CMCR_RSTS_Msk  (0x80) // 软件复位
#define I2C_I2CMCR_ACK_Msk   (0x08) // 发送/应答 ACK
#define I2C_I2CMCR_STOP_Msk  (0x04) // 产生 STOP 停止条件
#define I2C_I2CMCR_START_Msk (0x02) // 产生 START 起始条件
#define I2C_I2CMCR_RUN_Msk   (0x01) // 启动主机传输

// I2CMSR 读状态位掩码
#define I2C_I2CMSR_I2CMIF_Msk   (0x80) // bit 7: 主机中断标志
#define I2C_I2CMSR_BUS_BUSY_Msk (0x40) // bit 6: 总线忙标志
#define I2C_I2CMSR_IDLE_Msk     (0x20) // bit 5: 主机空闲标志
#define I2C_I2CMSR_ARB_LOST_Msk (0x10) // bit 4: 仲裁丢失
#define I2C_I2CMSR_DATA_ACK_Msk (0x08) // bit 3: 数据字节应答 (0: ACK, 1: NACK)
#define I2C_I2CMSR_ADD_ACK_Msk  (0x04) // bit 2: 地址字节应答 (0: ACK, 1: NACK)
#define I2C_I2CMSR_ERROR_Msk    (0x02) // bit 1: 错误标志
#define I2C_I2CMSR_BUSY_Msk     (0x01) // bit 0: 控制器内部忙
```

**v1.2 语义校正与证据边界（实现前必读）**：

1. **地址/位定义以厂商 `cms8s78xx.h` 为准且已逐位核对一致**；参考手册 Ch.15/Ch.16 的章节号无法在本地核验（PDF 文本不可提取），不得作为唯一证据。
2. **SPISIF 清序只采纳"读 SPSR → 读 SPDR"**：证据是厂商 `StdDriver/src/spi.c` 的 `SPI_ClearTransferIntFlag()`（`temp = SPSR; temp = SPDR;`）。v1.1 中"读写 SPDR 均可清"无本地证据，删除；若日后参考手册证实写入亦可清，再扩展模型。
3. **`SSCR` 复位值 `0x02` 是假设**（demo 的 `SPI_M95256_Stop()` 置高、`Start()` 清 bit1）；实现时按"上电未选中"建模，并在 CTest 中固化。
4. **`0xF2` 从机寄存器必须容忍写入**：厂商 `I2C_EnableMasterMode()` 会写 `I2CMCR = 0x00; I2CSCR = 0x00;`，因此**禁止**对 0xF2 加 `WINK_ASSERT_UNREACHABLE`；Phase 1 只做"接受写入并忽略/记录 + 读回阴影"，从机模式不在本期范围（评审 I5 修正）。
5. **`0xF5` 写入需区分语义**：`I2C_ClearMasterIntFlag()` 写 `I2CMSR = 0x00` 清 `I2CMIF`；`I2C_EnableMasterMode()` 也写 0。写钩子需按"`val==0` 且无 START/STOP/RUN 位 → 清标志"处理，不得误判为命令。
6. **`I2CMSA` 是 8-bit 形式（含 R/W）**：`0xA0` 对应 7-bit 地址 `0x50`；接入 ADR-0085/0086 的 ABI 前必须 `>>1` 转换，不得把 R/W 位带进 `dev_addr`。

---

### 5.2 SPI 外设行为模型（`cms8s_spi.cpp`）——按框架真实 hook 契约重写

#### 1. 框架 hook 契约（纠正 v1.1 的 API 误用）
- 框架**没有**"外设类 + `read_sfr()` 返回值"接口。真实契约：
  - `mcs51_trap_register_sfr_write(addr, fn)` / `mcs51_trap_register_sfr_read(addr, fn)`（`include/mcs51_trap.h`）；
  - hook 签名为 `void(ctx, addr[, old, new])`，**读 hook 无返回值**：模型必须先把要返回的值写入 `ctx->sfr_shadow[addr]`，`WinkSfr` 代理再把它交给固件；
  - 模型私有状态（`spcr_/spsr_/spdr_rx_/sscr_/latched`）在 hook 内同步镜像到 shadow；
  - 注册时机与 UART/ADC 模型一致（peripheral desc 的 reset 阶段，S4-H2 契约）。
- **同步完成范式**（与 ADR-0081 的 UART TI 同构）：`SPDR` 写 hook 内完成传输、按分频计费、置 `SPISIF`；`while(!flag)` 首读即命中。

#### 2. 两步读清时序（read SPSR → read SPDR）
```cpp
static void on_spsr_read(Mcu51Context* ctx, uint8_t) {
    if (g_spi.spsr & SPI_SPSR_SPISIF_Msk) g_spi.spsr_read_latched = true;
    ctx->sfr_shadow[SFR_SPSR] = g_spi.spsr;
}
static void on_spdr_read(Mcu51Context* ctx, uint8_t) {
    if (g_spi.spsr_read_latched) {
        g_spi.spsr &= (uint8_t)~SPI_SPSR_SPISIF_Msk;
        g_spi.spsr_read_latched = false;
    }
    ctx->sfr_shadow[SFR_SPDR] = g_spi.spdr_rx;
}
static void on_spdr_write(Mcu51Context* ctx, uint8_t, uint8_t val) {
    spi_handle_transmit(ctx, val);                 /* 计费 + 完成 + 置 SPISIF */
    ctx->sfr_shadow[SFR_SPSR] = g_spi.spsr;        /* 同步自动打拉 */
}
```
- 语义要点：读 SPSR 只置锁存、不清标志；随后读 SPDR 才真正清 `SPISIF`；写 SPDR **只启动传输**（v1.1 的"写 SPDR 也满足清序"无证据，已删除）。`SPI_Transmit()` 的 `while(!flag)` + `return SPDR` 天然走通。
- `SPSR` 写只允许改 `SSCEN`(bit0)，`SPISIF`/`WCOL` 软件写无效。

#### 3. `SSCR` 边沿跟踪与片选语义
- `SSCR` 写 hook 记录 bit1 边沿（高→低 = 帧开始，低→高 = 帧结束）；
- **Phase 1 的边沿记录仅用于 CTest 断言，不对外输出**：SPI 线级 ABI 尚不存在（ADR-0087 待立项），Phase 2 才把边沿接入会话契约；
- **Phase 1 不在片内模型解析器件命令**：M95256 的 WREN/WRITE/RDSR 帧命令 FSM（评审 I13）属于片外器件语义，归 Phase 2 插件实现。

#### 4. Phase 1 协议无关 Mock
- SPI 传输统一返回 `0xFF`（MISO 空闲高）或测试注入值，**不区分任何 EEPROM 命令**；
- `SPISIF` 同步置位保证跳出；数据正确性不属于 Phase 1 验收（证据分级见 T1.6 / T2.5）。

#### 5. 时钟计费（公式化，替代硬编码 2µs）
- `Fspi = Fsys / SPIClkDiv`（厂商 `spi.c` 注释；demo 使用 `SPI_CLK_DIV_8`）；
- 每字节 8 bit：`byte_us = (8 * 1_000_000 + Fspi - 1) / Fspi`，在 `SPDR` 写 hook 内先 `wink_mcs51_charge_us(byte_us)` 再置 `SPISIF`；
- `Fsys` 从 `cms8s_sys` 模型读取（`SYS_SET_SYSTEM_CLK` 配置），禁止写死常量。

#### 6. host 构建 fallback（T1.2 隐藏前置）
- `mcs51_uni_bridge.cpp` 需为 Phase 2 将使用的 `js_pal_spi_transfer` 提供 `#ifndef __EMSCRIPTEN__` 空实现，否则 host CTest 链接失败。

---

### 5.3 I2C 外设行为模型（`cms8s_i2c.cpp`）——按框架真实 hook 契约重写

#### 1. hook 契约与 `0xF5` 读写分离
- 与 SPI 同构：`0xF5` 需**同时注册读/写 hook**；写 hook 收到的是 `I2CMCR` 命令，读 hook 需先把 `I2CMSR` 镜像写入 `sfr_shadow[0xF5]` 再返回给固件；
- 写 `0xF5 == 0x00` 且无命令位 → 视为 `I2C_ClearMasterIntFlag()`（清 `I2CMIF`）；`I2C_EnableMasterMode()` 同样写 0，模型需幂等处理（见 §5.1 注 5）。

#### 2. 命令状态机（与厂商宏严格对齐）
1. **START + SEND**（`0x03` = `START | RUN`）：START → `I2CMSA`（8-bit 含 R/W）→ 地址 ACK → `I2CMBUF` 字节 → 数据 ACK；置 `I2CMIF`，回填 `ADD_ACK`/`DATA_ACK`，清 `BUSY`/`BUS_BUSY`。
2. **SEND**（`0x01` = `RUN`）：发送 `I2CMBUF` → 数据 ACK；置 `I2CMIF`，回填 `DATA_ACK`。
3. **RECEIVE + ACK/NACK**（`0x01|0x08` / `0x01`）：接收 1 字节入 `I2CMBUF`，按 `I2CMCR.ACK` 在总线上回 ACK/NACK；置 `I2CMIF`。
4. **STOP**（`0x04`，**不带 RUN**）：清 `BUS_BUSY`、置 `IDLE`，释放总线。

**模型侧路由与错误映射（ADR-0086 v1.3）**：
- **重复 START**：模型自持"当前是否有活跃会话"状态；固件在会话中再次下发 `START | RUN` 时必须路由到 `session_restart`（直接 `open` 会得 `WINK_ERR_BUSY`）；
- **地址 NACK 后**：`session_open/restart` 返回 `addr_nack=1` 时，模型**跳过**后续数据命令，进入 `ADDR_NACKED` 映射（保持 `I2CMIF`、不更新 `DATA_ACK`）；
- **非法状态命令**：引擎返回 `WINK_ERR_INVALID_STATE` 时，模型保持 `I2CMIF=1`、不更新 `DATA_ACK`/`ADD_ACK`，并记录诊断，不伪造成功；
- **result 必填**：`open/restart/write/read` 均必须传 `result`（引擎对 NULL 返回 `WINK_ERR_INVALID_ARG`），否则 `I2CMSR.ADD_ACK/DATA_ACK` 无法回填。

**STOP 行为级偏差声明（评审 I12）**：原厂 demo 在 STOP 后**不做任何标志轮询**（直接软件延时），故 Phase 1 模型"STOP 不置 `I2CMIF`"是有意的行为级选择，而非寄存器级精确。若日后参考手册证实硬件在 STOP 后置 `I2CMIF`，且新固件据此轮询，本模型需升级为"STOP 也置位"（升级条件已记录，回归用例 `test_cms8s_i2c.cpp` 的 stop 用例）。

#### 3. 时间计费（I2C，与 SPI 对称，补齐 v1.3 缺失）

- 时钟公式（厂商 `i2c.c` 注释）：`I2CMTP = 0 → SCL = 3*10*Tsys`；`I2CMTP != 0 → SCL = 2*(1+I2CMTP)*10*Tsys`（demo 使用 `I2C_ConfigCLK(2)`）；
- 每条 `I2CMCR` 命令计费：数据字节按 9 个 SCL（8 bit + ACK）、START/STOP 各按 9 个 SCL 估算；在 `0xF5` 写 hook 内 `wink_mcs51_charge_us(cmd_us)` 后再置 `I2CMIF`；
- `Tsys`/`Fsys` 取自 `cms8s_sys` 模型（`SYS_SET_SYSTEM_CLK` 配置），禁止写死常量；demo 注释"400K"与 Fsys 假设的差异在 CTest 中固化为显式断言（与 §5.2 同口径）。

#### 4. ACK/NACK 与 tWR：现状纠偏（评审补充）
- **官方 demo 没有 ACK Polling**：`At24c256_write_byte()` 在 STOP 后使用固定 `2000×200` 次软件延时等待擦写，且全程不检查 `ADD_ACK`/`DATA_ACK`。v1.1 的"ACK Polling 二次死锁"是假想场景，风险等级下调；
- Phase 1 Mock：所有寻址与数据**直接应答 ACK**（`ADD_ACK=0`、`DATA_ACK=0`），任何 ACK 分支都能收敛；
- Phase 2 插件：按真实 24C 时序在 `tWR` 窗口内对寻址回 NACK。若未来固件引入 ACK Polling，需保证"轮询期间虚拟时间单调推进"（C 侧 `charge_us` 主导，ADR-0072），使轮询在有限次内收敛；该场景以独立 CTest/场景用例覆盖。

#### 5. Phase 2 目标：会话流 ABI（ADR-0086）
- 逐命令时序（`START_SEND → SEND → START_RECEIVE_ACK → RECEIVE_ACK/NACK → STOP`）必须映射为 `js_pal_i2c_session_*`（`open/restart/write/read/close`），**不能**逐字节调用整事务 `js_pal_i2c_transfer`；
- 控制器模型逐命令调用时 `len=1`：ACK 字节用 `ack_mode=PAL_I2C_ACK_ALL`，NACK 字节用 `PAL_I2C_NACK_ALL`；
- `I2CMSA` 的 8-bit 地址（`0xA0`）→ ABI 7-bit `dev_addr`（`0x50`）转换在模型内完成。

#### 6. 明确不做（Scope Out）
- 9-Clock 总线恢复（ADR-0067 的 PAL 层能力，不属片内模型；demo 无触发路径）；
- 从机模式寄存器行为（仅容忍写入，见 §5.1 注 4）；
- `I2CMCR.RSTS`（软件复位）不实现专用语义：按"清状态机 + 释放内部会话"最小处理或直接忽略，不得崩溃；
- 多主仲裁、10-bit 地址、SMBus PEC。

---

### 5.4 运行时防护机制（SFR 层自旋防护，替代 v1.1 的 PC 看门狗）

#### 1. 为什么 v1.1 的 PC 看门狗不可实现
框架**没有 PC/寄存器文件，也没有指令步进函数**：固件是转译后的 native 代码，微步进只发生在 SFR/XDATA 代理访问点。因此 `check_instruction(uint16_t pc)`、`mcs51_core.cpp`、`mcs51_watchdog.h` 均不存在且无法落地；且 `mcs51_watchdog.h` 会与既有片上看门狗模型 `include/wink_mcs51_wdt.h` 撞名。

#### 2. 可实现的判据：多地址滑动窗口 + 事件清零 + 虚拟时间预算
```cpp
/* 放在 SFR 读派发路径（mcs51_bridge.cpp / 各外设模型读 hook），非新文件 */
struct SfrSpinGuard {
    uint8_t  last_addrs[3] = {0xFF, 0xFF, 0xFF}; /* 3 地址滑动窗口：覆盖 A,B,A 与 A,B,C 交替轮询 */
    uint64_t window_start_us = 0;
    uint32_t reads = 0;                          /* 仅诊断用 */
};
/* 默认预算 50ms（须大于测试语料最长合法硬件等待）；STRICT/CTest 可收紧 */
static constexpr uint32_t SPIN_GUARD_BUDGET_US = 50000u;
```
- **只统计 SFR 读**：转译器注入的 `_nop_()` microstep 不计入，demo 末尾 `while(1){;}` 不会误杀；
- **窗口清零锚点（必须实现，缺一不可）**：① 任何经代理的 SFR 写；② `mcs51_raise_irq()` 派发；③ `mcs51_edge_queue_drain()` 产生边沿；④ 模型内部写标志（如 UART RX push、定时器重载）；⑤ 场景/量子步边界。这四类"外部事件"是区分"合法长等待"与"真自旋"的关键——仅靠 SFR 写清零会误杀 `while(!RI)`（RX 由 JS 注入、不经代理写）与长周期定时器等待；
- **命中条件**：同一 3 地址集合内连续读 + 窗口内无上述任何事件 + `virtual_us - window_start_us > SPIN_GUARD_BUDGET_US`；
- 虚拟时间由既有 `wink_mcs51_charge_us`（5µs/microstep）自然累积，无需额外时钟接口；
- **已知局限**：>3 个寄存器交替轮询可能漏报；作为防御纵深可接受，不追求完备（真正兜底是场景超时 + T1.2/T1.3 模型）。

#### 3. 触发后的行为
- **默认诊断优先**：记录 SFR 地址、连续读次数、虚拟时间、外设状态，不中止仿真（避免误杀长等待）；
- **STRICT/CTest 硬失败**：经 ADR-0082 的纤程退出（`longjmp`）终止当前用例并标记场景失败——注意**自旋发生在固件 native 代码中，没有调用边界可以"返回 `WINK_ERR_TIMEOUT`"**；不得复用 `wink_mcs51_unsupported()`（语义是"未支持特性"，会造成诊断污染）；
- 预算/开关按构建配置，由 T1.4 用例固化"诊断不误杀"与"STRICT 必失败"两种行为。

#### 4. 定位
本机制是**防御纵深**：主修复仍是 T1.2/T1.3 的外设模型；T1.4 不得作为"解除死锁"的验收依据。

#### 4. 定位
本机制是**防御纵深**：主修复仍是 T1.2/T1.3 的外设模型；T1.4 不得作为"解除死锁"的验收依据。

---

### 5.5 `wink-ai` (unisim) 侧插件实现规格（契约级，依赖 ADR-0085/0086）

> 前置（硬）：ADR-0085/0086 Accepted 且七步 ABI hash 同步落地。本计划只约定**契约与验收**，不指定 UniSim 内部实现文件（遵循 `docs/AGENTS.md` 黑盒隔离）。

#### 1. 插件交付形态与挂载
- 交付形态：builtin 外设插件（`type` + 语义化版本目录 + manifest），可被 `winkcli`/Headless 扫描发现；插件不得直连宿主内部模块。
- 挂载声明：微应用经 `wink-app.json` 声明，构建产物 `unisim-assets/device-tree.json` 固化绑定（I2C：port + 7-bit 地址；SPI：port + CS 引脚/`deviceId` 映射）。
- 观测/断言：Headless 校验**只允许走插件通道**（`plugin:<id>/<channel>`）；`ASSERT_BUS_PAYLOAD` 当前只解析 UART，且 SPI 示例（No. 38）无串口逻辑、其 `value` 为读后即死的局部变量——引擎没有变量级内存断言能力，严禁依赖"内存快照 `value==0x08`"或串口输出。

#### 2. 虚拟 I2C EEPROM 插件（AT24C256）
- **从机地址**：7-bit `0x50`（写 `0xA0` / 读 `0xA1`）；
- **存储**：32 KB；页大小 64 字节，页写跨边界回滚（Page Roll-over）；
- **协议**：Current Address Read / Random Read / Sequential Read；写周期 `tWR` 通过 ADR-0085 `stretch_us` 告知（0=即时模式）；
- **线级要求（ADR-0086）**：插件实现线级回调以观察 repeated START 与逐字节 ACK；仅实现整帧 `onTransfer` 时，引擎对会话调用必须返回 `WINK_ERR_UNSUPPORTED`（不得静默降级）；
- **时序**：`behavioral` 模式不推进虚拟时间，tWR 只在 timing/cycle 模式计入时间线；固件时间由 C 侧 `charge_us` 主导。

#### 3. 虚拟 SPI EEPROM 插件（M95256 / 25LC256）
- **前置（独立 ADR，T2.2a）**：SPI 帧/CS 线级契约未定案前，本插件只做 Mock 级交付，不得宣称保真。需先立项 **ADR-0087**（`js_pal_spi_session_*` + CS 引脚边沿绑定，对标 ADR-0086），并裁决它与 catalog 既有提案 `js_pal_spi_transfer_ex` 的关系、以及 `js_pal_spi_transfer`（`stub`）的升级归属；
- **指令集**（按 M95256 语义实现，供 Phase 2 插件侧使用，片内模型不感知）：
  * `0x06` WREN：CS 低→高跳变时置 `WEL=1`；
  * `0x04` WRDI：清 `WEL`；
  * `0x05` RDSR：返回 `{bit1: WEL, bit0: WIP}`；
  * `0x02` WRITE：仅 `WEL==1` 允许，CS 上升沿提交写入并自动清 `WEL`；
  * `0x03` READ：从 16 位地址连续吐出数据。
- **帧命令 FSM**（`current_cmd_`/`byte_count_`/`frame_state_`，CS 低→高为一帧）在**插件内**实现，状态跨 `SPI_Transmit` 调用保持。

#### 4. 验收口径
- I2C：`At24c256_read_str()` 读回与写入完全一致；随机读/连续读/页写边界均有单测与场景断言；
- SPI：`SPI_M95256_Read_Data()` 读回一致；WREN/WEL 锁存与 WIP 轮询路径有单测；
- 全部通过后才允许 Checklist 37/38 正式摘牌（T2.5）。

---

## 6. 任务拆分与双仓协同推进路线

> 路径约定：`embedded` 侧一律以仓库根为基准（`wink-micro-os/...`）；`wink-ai` 侧只写契约/交付物，不写内部 TS 路径。
> 优先级双维度：**ABI 优先级**（ADR-0085=P0、ADR-0086=P1）与**实施阶段**（Phase 1 / Phase 2）分开表述。

### 阶段一：`wink-ai-embedded` 仓解死锁（实施 P1）

| 序号 | 任务项 | 负责仓 | 涉及文件 | 验收标准 |
| :---: | :--- | :---: | :--- | :--- |
| **T1.1** | 补齐 SPI / I2C SFR 声明与宏定义 | `embedded` | `wink-micro-os/frameworks/mcs51/chips/cms8s78xx/include/REG_CMS8S78XX.H`<br>`.../include/cms8s_sfr_map.h` | SPCR(0xEC)/SPSR(0xED)/SPDR(0xEE)/SSCR(0xEF)、I2CSADR..I2CMTP(0xF1..0xF7)；同步 `mcs51_shim_audit.py` fixture/allowlist；SPDX LGPL-3.0-only |
| **T1.2** | SPI 片内外设模型（hook + 双步读清 + 计费） | `embedded` | `.../chips/cms8s78xx/src/cms8s_spi.cpp`<br>`.../include/cms8s_spi.h`<br>`.../mcs51_sources.cmake`<br>`wink-micro-os/frameworks/mcs51/src/mcs51_uni_bridge.cpp`（host fallback） | 1. 读 SPSR→读 SPDR 真正清 `SPISIF`；<br>2. SSCR 边沿跟踪；<br>3. `charge_us` 按 `Fsys/SPIClkDiv` 公式；<br>4. host 可链接 |
| **T1.3** | I2C 片内外设模型（命令状态机 + 计时 + Mock ACK） | `embedded` | `.../src/cms8s_i2c.cpp`<br>`.../include/cms8s_i2c.h`<br>`.../mcs51_sources.cmake`<br>`wink-micro-os/frameworks/mcs51/src/mcs51_uni_bridge.cpp`（i2c host fallback） | 1. 0xF5 读写 hook 分离（含 `val==0` 清标志）；<br>2. START/RUN/repeated-START/STOP 状态机与 ADDR_NACKED 映射；<br>3. 0xF2 从机写入容忍；<br>4. 按 SCL 公式计费；<br>5. host 可链接 |
| **T1.4** | SFR 层自旋防护（替代 PC 看门狗） | `embedded` | `wink-micro-os/frameworks/mcs51/src/mcs51_bridge.cpp`（读派发路径）<br>（**不新建** `mcs51_core.cpp`/`mcs51_watchdog.h`） | 3 地址窗口 + 事件清零锚点 + 虚拟时间预算；默认诊断不误杀、STRICT 经纤程退出硬失败；有限延时/长周期定时器/RX 等待不误杀 |
| **T1.5** | CTest 单测（读清/命令/计时/防护） | `embedded` | `wink-micro-os/frameworks/mcs51/test/cms8s78xx/test_cms8s_spi.cpp`<br>`.../test_cms8s_i2c.cpp`<br>`.../test_mcs51_sfr_spin_guard.cpp`<br>`wink-micro-os/test/CMakeLists.txt`（`add_mcs51_host_test` 注册） | CTest 100% 绿灯；含读清多重读、repeated START、STOP、计时、防护正反例 |
| **T1.6** | 官方微应用镜像与构建输出 | `embedded` | `wink-micro-app/vendor/cms8s78xx/spi_master_95256/`<br>`wink-micro-app/vendor/cms8s78xx/i2c_master_at24c256/`（ADR-0079 嵌套布局） | 按现有 vendor app scaffold 生成：`CMakeLists.txt` + `wink-app.json` + 拷贝原厂 `main.c/demo_*.c/h/isr.c`（一行不改）+ `unisim-scenarios/`；`winkcli build sim` 产出三件套；运行无超时/不收敛 |
| **T1.7** | 门禁与证据分级 | `embedded` | — | `winkcli lint --pack layering --pack api`、`mcs51_shim_audit.py`、`check_license_map.py` 全绿；Checklist 37/38 仅标注"Phase 1 已解除运行阻塞" |

### 阶段二：ABI 落地与虚拟外设联动（ABI 优先级见左，实施 P2）

| 序号 | 任务项 | 负责仓 | 涉及文件 | 验收标准 |
| :---: | :--- | :---: | :--- | :--- |
| **T2.0** | ADR-0085/0086 评审 + 七步 ABI hash 同步 | 双仓 | `wasm_bridge.h`、`targets/wasm/wink_sim_js.js`（新 import 的 JS 实现）、`targets/wasm/pal_wasm_ch2_bus.c`（测试导出面）、TS imports/exports、`PAL_WASM_ABI_HASH`、`abi-catalog.yaml` | ADR Accepted；catalog `proposed→implemented`；wasm parity 全绿（**T2.3/T2.4 硬前置**） |
| **T2.1** | AT24C256 虚拟插件（线级会话接口） | `wink-ai` | builtin 插件交付物（manifest + 线级回调实现） | 单测覆盖页写回滚、Random/Sequential Read、tWR NACK 窗口 |
| **T2.2a** | SPI 姐妹 ADR 立项与评审 | 双仓 | ADR-0087（`js_pal_spi_session_*` + CS 边沿绑定）；裁决 `js_pal_spi_transfer_ex` 与 `js_pal_spi_transfer`（stub）升级归属 | ADR Accepted；catalog 登记 SPI 会话符号（proposed） |
| **T2.2** | M95256 虚拟插件（CS 帧契约后） | `wink-ai` | builtin 插件交付物 + 线级契约实现 | 单测覆盖 WREN/WEL 锁存、CS 上升沿提交、RDSR/WIP（**依赖 T2.2a**） |
| **T2.3** | 双仓联调（`_ex` + `session_*`） | 双仓 | `cms8s_i2c.cpp`/`cms8s_spi.cpp` ↔ UniSim 桥接/总线 | 事务数据透传；AT24C256 随机读经 repeated START 成功；**不使用**整事务接口逐字节调用 |
| **T2.4** | Headless 场景与数据实证 | 双仓 | 各微应用 `unisim-scenarios/` | **仅插件通道断言**（`plugin:<id>/<channel>`）读写字节一致；`winkcli sim run` 退出码 0 |
| **T2.5** | Checklist 摘牌与归档 | `embedded` | `docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md` | 37/38 `🚫 Blocked → [x]`（以 T2.4 证据为准）；同步索引归档 |
| **T2.6** | Layer-① 回写与 ADR 合流 | `embedded` | `docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md`；ADR-X1 收敛（ADR-0085/0086 规则抽象为跨通道通用规则） | 设计规范含 CH2 I2C/SPI 线级契约与证据口径；无悬空 ADR 引用 |

---

## 7. 风险评估与应对预案

| 风险项 | 严重级 | 影响表现 | 缓解策略与技术对策 |
| :--- | :---: | :--- | :--- |
| **R1. 外设模型未命中导致业务不收敛** | 🔴 高 | 固件卡在 `while(!flag)`，Headless 场景超时（非宿主冻结） | 1. T1.2/T1.3 同步自动打拉 + 双步读清；<br>2. T1.4 SFR 层自旋防护输出诊断；<br>3. CTest 专用不收敛用例（带超时） |
| **R2. 寄存器/宏与厂商头冲突** | 🟡 中 | 编译错误或 shim 漂移 | 1. 以厂商 `cms8s78xx.h` 为基准（已核对）；<br>2. `#ifndef` 守卫 + StdDriver 隔离垫片；<br>3. `mcs51_shim_audit.py` fixture/allowlist 同步，0 漂移 |
| **R3. 虚拟时间漂移** | 🟡 中 | 固件延时与总线时钟不匹配 | 按 `Fspi = Fsys/SPIClkDiv`、`SCL = 2*(1+I2CMTP)*10*Tsys` 公式记账；`Fsys` 取自 `cms8s_sys`；CTest 固化 |
| **R4. 轮询二次死锁（更正）** | 🟢 低 | 未来固件若引入 ACK Polling/WIP 轮询可能不收敛 | 官方 demo 无 ACK Polling（固定延时）；Phase 1 Mock 全 ACK；Phase 2 插件保证 NACK 窗口内虚拟时间单调推进；新增轮询场景用例 |
| **R5. ABI 依赖未落地** | 🔴 高 | T2.3/T2.4 无接口可用，或逐字节误用整事务导致协议错误 | T2.0 为硬前置；ADR-0085/0086 Accepted + 七步 hash 同步后才启动 Phase 2 |
| **R6. host 构建链接失败** | 🟡 中 | CTest 无法链接 `js_pal_*` | T1.2 在 `mcs51_uni_bridge.cpp` 补 `#ifndef __EMSCRIPTEN__` fallback |
| **R7. 插件发现/挂载路径错误** | 🟡 中 | Headless 场景加载不到 EEPROM 插件 | 按 builtin 插件布局与 `wink-app.json → device-tree.json` 流程交付；T2.4 前先做最小挂载冒烟 |
| **R8. 时序模式误用** | 🟡 中 | 默认 behavioral 下 tWR 不推进，误判"保真" | 验收声明明确模式；tWR 断言只在 timing/cycle 模式作为证据 |
| **R9. 门禁/许可遗漏** | 🟡 中 | 合入被 CI 拦截 | T1.7 覆盖 layering/api lint、shim audit、license map、ABI catalog `--check` |
| **R10. 自旋防护误杀合法长等待** | 🟡 中 | 长周期定时器/RX 注入/外部中断等待被误判为死锁，场景假失败 | §5.4 事件清零锚点（IRQ/边沿/模型内部写/量子步）+ 默认诊断优先 + 预算 50ms；STRICT 收紧需有语料依据；T1.4 正反例固化 |
| **R11. ADR 与 catalog 契约漂移** | 🟡 中 | 文档/目录/实现三方不一致（如字段改名未同步、错误码写错） | T2.0 统一走七步 hash + catalog 再生成；评审前跑 `check:abi-catalog`；本计划 v1.4 已清理 `addr_ack`/`WINK_ERR_STATE`/内存快照等残留 |

---

## 8. 验收门禁与签署标准

所有属于本计划的代码与资产在最终归档前必须满足以下硬性准入标准：

1. **零代码修改（Zero-code Modification）**：官方示例源码（`main.c`, `demo_i2c.c`, `demo_spi.c`, `isr.c`）保持逐字一致，禁止修改原厂代码绕过死锁。
2. **真实资产就绪（Assets Ready）**：对应微应用目录下完整生成 `unisim-assets/` 三件套（`device-tree.json`, `wink_simulator.js`, `wink_simulator.wasm`），文件体积与哈希合规。
3. **分层与 ABI 门禁**：
   - `winkcli lint --pack layering --pack api`（Findings 0）；
   - `python wink-micro-os/frameworks/mcs51/tools/mcs51_shim_audit.py`（0 漂移）；
   - `python .github/scripts/check_license_map.py`（许可地图合规）；
   - ABI 变更时：`bun run check:abi-catalog`（wink-ai 侧，md/readonly TS 新鲜度）+ wasm parity。
4. **单测与场景全绿（Tests 100% Green）**：CTest 底座单测通过；Headless 场景退出码 0，数据校验**仅经插件通道**（`plugin:<id>/<channel>`）断言完全通过（SPI 无串口、无变量级内存断言可用）。
5. **证据分级（Evidence Tier）**：
   - Phase 1 证据 = "运行收敛 + 时序合理 + 单测"（Checklist 标注"运行已解除阻塞"）；
   - Phase 2 证据 = "读写数据一致 + repeated START/逐字节 ACK 路径覆盖"，**此时才允许 37/38 正式转为 `[x]`**。
6. **清单闭环（Checklist Resolved）**：[CMS8S78XX_EXAMPLE_CHECKLIST.md](../../vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md) 中编号 37、38 完成上述证据分级后的状态更新。

---

## 9. 变更记录（Plan Change Log）

| 版本 | 日期 | 变更摘要 |
| :--- | :--- | :--- |
| v1.0 | 2026-09-21 | 初稿：SFR 地址纠正、读清锁存、SSCR 边沿、ACK Polling 与看门狗设计 |
| v1.1 | 2026-09-21 | 吸收嵌入式专家评审：物理地址与位掩码核对、双步读清、片选边沿、风险表 |
| v1.2 | 2026-09-21 | 融合 ABI 评审与框架实况：①§2.3 根因改写（SFR proxy/charge/quota，非宿主冻结）；②§3 能力矩阵纠偏（imports/stub/非零拷贝/缺 host fallback）；③§5.1 证据边界与 0xF2/0xF5 语义；④§5.2/§5.3 按真实 hook 契约重写、Mock 协议无关、计费公式化、STOP 行为级偏差声明、ACK Polling 纠偏；⑤§5.4 看门狗重设计为 SFR 层自旋防护；⑥§5.5 插件契约化（ADR-0085/0086 前置）；⑦§6 任务路径/依赖重排（T2.0 硬前置、T1.7 门禁、ADR-0079 嵌套目录）；⑧§7 风险表更新（R5–R9 新增）；⑨§8 门禁命令更正 + 证据分级；⑩ADR 引用链接修正 |
| v1.3 | 2026-09-22 | 融合 ADR-0085/0086 协议契约与实况校准：①§4.2/§5.5/§8 纠正 SPI 验收无串口事实，确立内存变量快照/插件通道断言；②§5.4 SfrSpinGuard 升级为 2 地址滑动窗口抵御多状态寄存器交替防抖；③T2.2 确立 SPI 会话流姐妹 ADR 对标 |
| v1.4 | 2026-09-22 | 与 ADR-0085/0086 v1.3 契约对齐：①错误码定案（`WINK_ERR_DISCONNECTED`/`WINK_ERR_INVALID_STATE`）并清理 catalog 漂移（`addr_ack` 残留、`nack_bits` 写相位、session 必填 result、全局池命名）；②§5.3 补 repeated START 路由、ADDR_NACKED 映射、I2C 时钟计费与 RSTS 范围；③§5.4 防护重写（3 地址窗口 + 事件清零锚点 + 诊断优先/纤程退出，删除不可返回的 `WINK_ERR_TIMEOUT` 表述）；④SPI 断言去内存快照、T2.2a 立项 ADR-0087、T2.0 补 JS/导出实现面、T2.6 Layer-① 回写；⑤§7 新增 R10/R11 |
| v1.5 | 2026-09-22 | Phase 1 执行记录（T1.1–T1.7）：①T1.1 补齐 SPI/I2C SFR、XSFR 引脚选择器与位掩码宏，GAP-23 allowlist 165→171；②T1.2/T1.3 落地 `cms8s_spi.cpp`（同步完成 + SPSR→SPDR 双步读清 + SSCR 边沿 + 分频计费）与 `cms8s_i2c.cpp`（0xF5 命令状态机 + 重复 START 路由 + ADDR_NACKED + SCL 计费 + 理想 ACK Mock），并补 host `js_pal_spi/i2c_transfer` fail-closed 垫片；③T1.4 落地 SFR 自旋防护（3 地址窗口 + 五类事件锚点 + 50ms 预算 + 诊断优先/longjmp 硬失败）；④T1.5 新增三套 CTest（SPI 11 组 / I2C 13 组 / spin-guard 7 组），10 项 mcs51 测试全绿；⑤T1.6 新建 `vendor/cms8s78xx/i2c_master_at24c256` 与 `spi_master_95256`（原厂源码 SHA256 逐字节一致）并生成三件套，headless 场景均 PASS（Virtual 2s ≈ Wall 0.2s）；⑥T1.7 门禁全绿（layering/api lint、shim audit 0 漂移、许可地图）并完成 Checklist 37/38 Phase 1 标注。Phase 2 保持阻塞于 ADR-0085/0086 评审 |
| v1.6 | 2026-09-22 | Phase 2 / T2.0 执行记录（双仓）：①ADR-0085/0086 → **Accepted**（含活规范回写与 changelog）；②embedded 侧七步同步：`wasm_bridge.h` 新增 `pal_i2c_result_t`（8B `_Static_assert` 锁定）＋ `js_pal_i2c_transfer_ex` ＋ `js_pal_i2c_session_open/restart/write/read/close` ＋ `pal_wasm_i2c_transfer_ex`/复位三导出声明，`wink_sim_js.js`/`wink_sim_stub.js` 垫片实现，`PAL_WASM_ABI_HASH → 0x6B769176`，`exported_runtime_functions.json` 同步，`winkcli lint --pack wasm` 0 error；③unisim 侧：`imports.ts`/`exports.ts` 声明、`I2CBus.transferEx` 与 ADR-0086 会话 FSM（单 port 互斥、池 4、ADDR_NACKED、线级回调缺失→`WINK_ERR_UNSUPPORTED`）、`BusDomainHandler` 线级回调接线、`unisim-sdk` 类型扩展、桥接 8 字节 LE 结果编组，catalog `proposed→implemented` 并重新生成 md；④验证：`bun run typecheck` 0 错、`test:gates` 通过、i2c 单测 15 例全绿（core 共 62 例）、embedded wasm 测试重建通过、两应用三件套已按新 hash 重建。余项：T2.1 `i2c_eeprom` 插件、T2.2a ADR-0087、T2.3 控制器模型会话路由与快照/`MODEL_ABI_VERSION`、T2.4 数据实证 |
| v1.7 | 2026-09-22 | T2.1 执行记录：①新增 builtin 插件 `wink-plugin-peripherals/builtin/i2c_eeprom/1.0.0`（AT24C256：32 KiB / 64 B 页写**页内回卷**、Current-Address / Random / Sequential Read、`writeCycleUs` 开启 tWR **地址相位 NACK** 窗口、线级会话回调 + 整帧兼容、`serializeState/deserializeState` 存储镜像），单测 8 例；②`unisim-sdk` 设备契约新增 `onAddressPhase(direction)` 门控并转发于 `attachI2cDevice`/`BusDomainHandler`，stub-host 增补会话助手（`i2cSessionOpen/Restart/Write/Read/Close`）供插件测试；③引擎 `I2CBus.transferEx` 与会话 open/restart 尊重地址门控（ADR-0085 §5.3.4 tWR 承诺落地）；④验证：SDK 与引擎 typecheck 0 错、插件 8 例 + engines i2c 17 例全绿、沙箱 **13/13** bundle 通过、peripherals 全量 117 用例通过；⑤CI 门禁计数 12→13 并纳入新插件单测 |
| v1.8 | 2026-09-22 | T2.2a 执行记录：①**ADR-0087**（SPI 控制器级会话流 + CS 边沿绑定）立项并 Accepted（`packages/unisim/docs/internals/decisions/0087-spi-session-stream-and-cs-edge-abi.md`）：`js_pal_spi_session_open/transfer/close`（单 port 互斥、池 `PAL_SPI_SESSION_POOL_MAX=4`、未知设备 `WINK_ERR_NOT_FOUND`、close 幂等）、开帧=CS 拉低+`onTransactionStart`、关帧=**CS 上升沿提交点**+`onTransactionEnd`、会话仅走 `onExchangeByte` 线级回调（整帧插件 `WINK_ERR_UNSUPPORTED`）；②**裁决** `js_pal_spi_transfer_ex` 为推荐整帧入口（7 参数、8-bit、删除 `data_width`），`js_pal_spi_transfer`（bool/stub）降级 legacy；`device_id` 定为**板级绑定逻辑设备号**（cs_pin 当 device_id 废弃）；③catalog 登记 3 个会话符号（proposed）并订正 `_ex` 签名/`spi_transfer` 注记，md 重新生成（132→135 符号），`check:abi-catalog` 通过；④活规范回写（architecture 设计规范 CH2 SPI 段、C ABI guide 表格），ADR-0086 follow-up SPI 姐妹项闭环。余项：T2.2 `spi_eeprom` 插件、T2.3 双仓联调 |
| v1.9 | 2026-09-22 | T2.2 执行记录：①新增 builtin 插件 `wink-plugin-peripherals/builtin/spi_eeprom/1.0.0`（M95256：32 KiB / 64 B 页写回卷、`WREN/WRDI/RDSR/WRSR/READ/WRITE` 命令 FSM、**WEL 于 WREN 帧 CS 上升沿锁存**、**WRITE 帧 CS 上升沿提交并清 WEL**、WIP 窗口内仅 RDSR 有效、线级 `onExchangeByte` + 整帧 `onFrame` 双契约、`serializeState/deserializeState`），单测 8 例；②`unisim-sdk` stub-host 新增 **SPI 会话助手**（`spiSessionOpen/Transfer/Close`；缺 `onExchangeByte` 的整帧设备返回 `ok:false`，不静默降级）；③两个 EEPROM 插件擦除态对齐为 `0xFF`；④验证：typecheck 0 错、插件 16 例（i2c+spi）与沙箱 **14/14** bundle、peripherals 全量 125 用例通过；⑤CI 计数 13→14 并纳入新插件单测。余项：T2.3 双仓联调（引擎 SPI 会话 FSM + CS 驱动 + 头文件七步同步 + 模型路由 + 快照） |
| v2.0 | 2026-09-22 | **Phase 2 完成执行记录（T2.3–T2.6）**：①**T2.3-A** 引擎 SPI 会话流（`spi-bus.ts` `transferEx`/会话 FSM/CS sink、`BusDomainHandler` 线级回调与 `csPin` 透传、桥接 `js_pal_spi_transfer_ex`+`session_*` 转发与 CS→PinArbiter、headless 补 `spiBus`；`spi-bus` +18 例 / core 71 例全绿）；②**T2.3-B** embedded 七步同步（`wasm_bridge.h` 4 符号 + `pal_wasm_spi_transfer_ex` 导出、`wink_sim_js.js`/`wink_sim_stub.js` 垫片、`pal_wasm_ch2_spi.c` 逻辑设备号、`exported_runtime_functions.json`、**`PAL_WASM_ABI_HASH 0x6B769176 → 0xF402251A`**（C/TS/catalog 三方一致，两应用 wasm 运行时实测）、catalog 4 条 `proposed→implemented`，md 136 符号；两应用资产重建）；③**T2.3-C** 会话表 state-hash（`I2CBus/SPIBus.exportStateBuffer()` + `SimulationPluginHost.exportStateBuffer()` + `HeadlessDomainContext.registerStateHashProviders()`；复位/纤程退出先 close 再清表；**`MODEL_ABI_VERSION → unisim-phase3-diagnostic-l0-v2`**）；④**T2.3-D** MCS51 控制器会话路由（`cms8s_i2c.cpp` `I2CMCR`→`_ex`/`session_*`、`cms8s_spi.cpp` `SSCR.NSSO1`/`SPDR`→会话；`mcs51_bus_abi.h` 导入镜像；`mcs51_uni_bridge.cpp` 可脚本化总线 mock 4+6 fallback，wasm 路径无回落不伪造数据；CTest 含新 Phase 2 用例）；⑤**T2.4** 数据实证（两应用 `wink-app.json` 挂载 EEPROM → `device-tree.json`；插件新增 `readCount/lastReadByte/readbackHex` 通道；场景升级为插件通道断言：I2C 8 步 `readbackHex="3233343536"`、SPI 7 步 `readbackHex="08"`，退出码 0，负向对照验证断言有效；sim-only 插件 `dist/manifest.json` 发射器补齐）；⑥**T2.5** Checklist 37/38 `[-] 🟡 Phase 1` → `[x] ⚡ Level 3`，统计 33→35 完成 / 7→5 暂缓；⑦**T2.6** Layer-① 回写 `07-mcs51-simulation-interception.md` §2.9（命令映射/线级契约/host-wasm 边界/证据口径）+ §0 CH2 表 + §3.1 目录树；ADR-0085/0086/0087 follow-up 勾选 + 变更记录；**ADR-0088/ADR-X1（通用 ABI 返回值模型）Proposed 草案立项** + catalog 注记。门禁：`winkcli lint`（layering/api/wasm）0 error、shim audit 0 漂移、license map OK、CTest **165/165**、unisim typecheck/test:gates/check:abi-catalog 全绿、peripherals 127 例 + 沙箱 14/14。 |
| v2.1 | 2026-09-22 | **全链路收尾闭环**：①**全量资产重建**：43 个 app（appliances/mcs51/native/vendor/PDK 排除）`winkcli build sim` 全部成功，逐一 Node 实例化校验 `pal_wasm_get_abi_hash() = 0xF402251A` **43/43**（旧 hash 资产导致的 `verifyAbiHash` fail-fast 风险清零）；②**legacy `js_pal_spi_transfer`** 按 ADR-0087 以同一帧路径实现（bool 包装 `_ex`，引擎改动 + `spi-legacy-frame` 单测 3 例），catalog `stub → implemented`，C-ABI guide 与 ADR-0087 兼容条目同步；③**证据聚合脚本修复**：`run_mcs51_headless_evidence.ps1` 支持嵌套 app 路径（`mcs51/…`），5/5 载体 PASS；④**全量 headless 回归**：43 app 中 38 PASS；5 个失败经「embedded HEAD + unisim HEAD」双仓基线复现，确认**均非本次引入**（timer0/1 timing 频率偏差、`led_4com_8seg` scanHz、`uart0_rxtx` 回显窗口、`pdk/button_led` `port` 与 manifest/DAL codegen 契约错位），登记为独立后续项；⑤门禁复跑：CTest 165/165、unisim **781 pass**、`winkcli lint` 0 error、shim/license/isolation 全绿、peripherals 127 例 + 14/14 bundle；⑥**ADR-0088 接受包**（通用 ABI 返回值模型）：状态 `Proposed → Accepted`，catalog 17 条 `ADR-X1` 引用统一改指 `ADR-0088`，guide 新增 §4.6 返回值模型并更新 Step 0 失败模式，ADR-0085 合流 follow-up 勾选，catalog 重生成 + `check:abi-catalog` 通过；⑦**format:check 闭环**：新增 `.oxfmtrc.json`（对齐私有仓规范 + 忽略 `**/dist/**` 等生成物），44 个源文件按规范重排，`bun run lint` / `format:check` 双 0，127 例测试与 14/14 bundle 复跑通过；⑧**交接提示词归档**：`docs/implementation-plans/mcs51/2026-09-22-phase2-remaining-handoff-prompt.md`（原文 + 执行结果摘要）；⑨`build-peripherals.ps1` 补齐 oxlint/oxfmt 工具链链接。 |
