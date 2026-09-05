# ADR-0026：FOC 电机（SimpleFOC 与 VESC）的 DAL/BAL 职责划分与依赖注入设计

| 项 | 内容 |
|---|---|
| 状态 | **Superseded-in-part（部分被 supersede）** — 积木拆分 / 设备树拓扑仍有效；**ISR 宿主、DI 主路径、`pal_hwtimer`、数值/FPU、仿真快环、VESC scope 由 [ADR-0047](0047-foc-isr-layering-and-pal-hwtimer.md) supersede** |
| 日期 | 2026-07-08 |
| 触发 | 讨论智能运动执行器（FOC 电机驱动）的接入路线与硬件接线未知场景下的 DAL 设计 |
| 影响范围 | 新增 FOC 相关的 DAL 元器件驱动定义、BAL 层 SimpleFOC 依赖注入模式、CodeGen 设备树静态组装规范、仿真端动力学模拟标准 |
| 决策者 | 项目 Owner, AI 助手 |
| 关联 ADR | [ADR-0003 仿真保真度边界](../unisim/0003-simulation-fidelity-boundary.md)、[ADR-0004 静态分发 vs 运行期 ops](0004-static-dispatch-vs-runtime-ops.md)、[ADR-0023 BAL（业务抽象层）正式分层建立](0023-bal-business-abstraction-layer.md)、[ADR-0024 Fault 三阶段模型与 DAL Deinit 契约](0024-fault-three-phase-model-and-dal-deinit-contract.md)、[ADR-0047 FOC ISR 分层与 pal_hwtimer](0047-foc-isr-layering-and-pal-hwtimer.md)（部分 supersede）、[ADR-0048](0048-actuator-control-semantic-naming.md)（`dal_bldc` 命名） |
| 关联设计规范 | [01-dal-device-abstraction.md](../../zh/design/02-wink-micro-os/01-dal-device-abstraction.md) |

---

## 背景（Context）

随着 WinkMicroOS 在机器人与运动控制领域的演进，系统需要支持高性能运动执行器（如无刷 FOC 电机）。这类执行器在物理拓扑上主要分为两类：

1. **外部总线型智能驱动器 (VESC / ODrive)**：
   * 物理上为一块独立的智能驱动板（板载 MCU 跑 FOC），主控 MCU 仅通过 CAN/UART 协议发送控制帧（如速度、位置目标）并读取反馈。
2. **本地算法型驱动器 (SimpleFOC 模式)**：
   * 物理上驱动板为“裸”驱动桥（如 L6234, DRV8302，仅包含三相逆变电路、栅极驱动和采样电阻）。
   * FOC 核心算法（坐标变换、SVPWM 产生和 PID）必须运行在主控 MCU 本身，通过 MCU 的三相 PWM 通道、ADC 电流采样引脚和传感器读取接口（AS5600/磁编码器）直接控制电机。

对于 **外部智能驱动器**，硬件接线和通信协议是内聚且固定的，DAL 层只需实现一个简单的 `dal_vesc` 协议驱动即可。

然而对于 **本地算法型驱动器 (SimpleFOC 模式)**，存在以下架构痛点：
* **硬件方案爆炸**：用户设计的驱动板五花八门。驱动桥可能是 3-PWM 或 6-PWM 控制；角度传感器可能是 I2C 接口（AS5600）、SPI 接口（AS5047D）、正交编码器（ABI）或模拟量；电流采样可能是在线采样、低侧采样或无采样。
* **分层红线冲突**：如果把 SimpleFOC 作为一个整体放进 DAL 层，DAL 会被迫塞入复杂的数学算法与控制器，使其变得无比臃肿，且只要硬件引脚接法一变，DAL 驱动就需要重写。如果把 SimpleFOC 整个放进 BAL 层，算法层又需要直接感知物理引脚与 PWM 通道，破坏了 [ADR-0023](0023-bal-business-abstraction-layer.md) 规定的“BAL 层绝不能引入 PAL 物理引脚/通道头文件”的分层红线，且阻碍了算法的同源仿真。

---

## 方案比选（Options）

### 方案 A：整体黑盒 DAL 驱动化（不区分组件，写统一的 `dal_simplefoc`）

将 SimpleFOC 算法、PWM 驱动、传感器读取、电流采样硬编码在一起，为每种特定的驱动板+传感器组合编写一个专用的 DAL 驱动（如 `dal_simplefoc_l6234_as5600`）。

* ❌ **严重的硬编码与代码膨胀**：硬件接线稍微调整（如使能引脚换了个 GPIO，或者 AS5600 换成 AS5047D），就必须新增或大幅修改 DAL 驱动，无法复用。
* ❌ **违反分层职责**：将 PID、Clarke/Park 变换等高频数学控制算法塞进本应是“哑器件”的 DAL 层，污染了 DAL 的纯粹性。
* ❌ **仿真代价极高**：由于算法与具体的引脚时序高度耦合，在 Wasm/Host 仿真端进行算法闭环验证变得非常困难。

### 方案 B：分体式 DAL 积木 + BAL 控制器 + CodeGen 依赖注入（**推荐**）

将复杂的本地 FOC 驱动拆解为 **“多个单一职责的 DAL 硬件积木”** 与 **“一个纯软件控制算法的 BAL 控制器”**。通过设备树静态配置，在编译期实现依赖注入。

1. **DAL 层提供“硬件积木”**（仅负责物理量搬运，无 FOC 算法认知）：
   * **逆变器/驱动桥积木**：如 `dal_bldc_driver_3pwm`（3路PWM通道 + 使能引脚）和 `dal_bldc_driver_6pwm`。
   * **转子传感器积木**：如 `dal_magnetic_sensor_i2c` (AS5600)、`dal_magnetic_sensor_spi` (AS5047D)、`dal_encoder_abi`（正交脉冲）。
   * **电流采样积木**：如 `dal_current_sense_inline`（ADC 通道 + 放大倍数）。
2. **BAL 层提供“大脑控制器”**：
   * 实现一个纯软件的 `bal_simplefoc` 算法控制器。其配置结构体中只包含算法参数（如极对数、PID 参数），以及**上述 DAL 器件的实例句柄指针**。
3. **设备树静态配置与 CodeGen 组装**：
   * 用户在 `wink-app.json` 中分别声明传感器实例、驱动桥实例，并在 `bal_simplefoc` 实例中通过名称引用它们。
   * CodeGen 扫描该拓扑，在生成的 `device_tree.c` 中生成依赖绑定代码。

* ✅ **职责极度清晰**：DAL 器件继续保持“无脑搬运物理信号”的特征，BAL 专注于纯数学计算，完全不引入 `pal_hal.h`，符合分层红线。
* ✅ **积木式弹性组合**：硬件方案变更时，只需在 JSON 中更换绑定的 DAL 节点类型，BAL 层的 SimpleFOC 算法**一行代码都不需要改**。
* ✅ **双模同源仿真友好度 100%**：在 Host/Wasm 仿真端下，`dal_bldc_driver` 仿真分支**仅旁路最低物理量**（输入 3PWM 占空比 $\rightarrow$ 调 `targets/common/wink_sim_physical` plant $\rightarrow$ 回灌模拟编码器/电流采样值）；plant 方程**不得**写在 DAL `#ifdef SIMULATION` 内。由此 BAL 层 SimpleFOC 算法代码**无需任何修改地在浏览器中闭环跑起来**（见 [01-dal §8.3](../../zh/design/02-wink-micro-os/01-dal-device-abstraction.md)）。

---

## 决策结论（Decision）

采纳 **方案 B**。具体规范要求如下：

### 1. 职责划分与接口规范

* **DAL 驱动积木命名与接口**：
  * **Driver (驱动桥)** 须暴露设置物理电压/占空比接口：
    ```c
    wink_status_t dal_bldc_driver_3pwm_set_voltages(dal_bldc_driver_3pwm_t *dev, float u_a, float u_b, float u_c);
    ```
  * **Sensor (传感器)** 须暴露获取物理角度接口：
    ```c
    float dal_magnetic_sensor_i2c_get_angle(dal_magnetic_sensor_i2c_t *dev);
    ```
* **BAL 控制器依赖注入结构**：
  * `bal_simplefoc` 通过 Codegen **静态绑定**到具体 `dal_*` 实例与命名 API（编译期别名 / 直接调用）。**运行期 fn 表（`get_angle_fn` / `set_voltage_fn`）已被 [ADR-0047](0047-foc-isr-layering-and-pal-hwtimer.md) 否决为主路径（R-005）**，勿照抄旧 ops 表形态：
    ```c
    /* 示意：codegen 钉死具体 DAL 类型；无运行期函数指针表 */
    typedef struct {
        dal_magnetic_sensor_i2c_t *sensor;   /* 编译期绑定的传感器实例 */
        dal_bldc_driver_3pwm_t *driver;      /* 编译期绑定的驱动桥实例 */

        int pole_pairs;                   /* 电机极对数 */
        float voltage_limit;              /* 电压限制 */
        // ... PID 状态参数
    } bal_simplefoc_config_t;

    /* 或由 codegen 生成的编译期别名（示意） */
    #define bal_simplefoc_read_angle(cfg) \
        dal_magnetic_sensor_i2c_get_angle((cfg)->sensor)
    #define bal_simplefoc_write_voltages(cfg, ua, ub, uc) \
        dal_bldc_driver_3pwm_set_voltages((cfg)->driver, (ua), (ub), (uc))
    ```

### 2. 静态设备树拓扑关系（JSON 示例）

在 `wink-app.json` 中通过实例名声明依赖：

```json
{
  "devices": [
    {
      "name": "motor_sensor",
      "type": "dal_magnetic_sensor_i2c",
      "pins": { "sda": 21, "scl": 22 }
    },
    {
      "name": "motor_driver",
      "type": "dal_bldc_driver_3pwm",
      "pins": { "pwm_a": 12, "pwm_b": 13, "pwm_c": 14, "en": 15 }
    },
    {
      "name": "my_foc_controller",
      "type": "bal_simplefoc",
      "properties": {
        "sensor": "motor_sensor",
        "driver": "motor_driver",
        "pole_pairs": 7,
        "voltage_limit": 12.0
      }
    }
  ]
}
```

### 3. CodeGen 静态初始化顺序

在编译生成的 `device_tree.c` 中，初始化必须保证严格的拓扑序：
1. 先初始化底层总线（如 I2C 总线）。
2. 初始化 DAL 传感器 `dal_magnetic_sensor_i2c_init(&motor_sensor, ...)`。
3. 初始化 DAL 驱动桥 `dal_bldc_driver_3pwm_init(&motor_driver, ...)`。
4. 将具体 DAL 实例指针静态填入 `bal_simplefoc_config_t`（codegen 静态绑定；无运行期 fn 表，见 ADR-0047）。
5. 初始化 BAL 算法控制器 `bal_simplefoc_init(&my_foc_controller, &cfg)`.

### 4. 实时性隔离（前后台隔离）

* FOC 控制环中，电流环/估算器需要以 $10\text{kHz}+$ 频率在硬件定时器中断中执行。**ISR 宿主**为 DAL/target `foc_isr_trampoline`（不进 BAL 公共头）；ISR 可调用 BAL `control/` 纯快环函数，经 codegen 静态绑定读写 DAL（完整边界见 [ADR-0047](0047-foc-isr-layering-and-pal-hwtimer.md)）。
* 顶层 App 和 Runtime 协作式主循环不直接执行高频中断，而是通过非阻塞 API 以低频（如 $50\text{Hz}$）修改控制目标（例如速度目标），并读取遥测数据（例如估算出来的实际转速、母线电压），从而避免拖垮协作式调度器。

### 5. 资深电机控制工程师的补充设计约束（高级硬件与控制特性）

若要实现商用级、工业级的电机控制系统，仅靠上述简易积木划分是**不够**的。在真实 FOC 系统中，以下 5 个高级硬件与算法约束必须在 DAL 和 PAL 层予以显式支持：

#### A. PWM 与 ADC 的硬件级同步触发（Hardware-Triggered ADC Sampling）
* **痛点**：相电流采样必须在功率管开关动作产生的噪声衰减之后进行。对于单电阻/双电阻/三电阻低侧采样，ADC 转换必须精确对齐到 PWM 载波的对称中心点（例如计数器的下溢点/Underflow，即所有低侧功率管全部导通的时刻）。若通过软件中断触发 ADC，抖动（Jitter）将导致采样数值极度失真，无法闭环。
* **决策**：PAL 层必须提供 **PWM-ADC 硬件联动机制**。初始化 `dal_current_sense` 时，其底层的 ADC 转换必须绑定到 `dal_bldc_driver` 关联的 PWM 定时器硬件触发源（如 STM32 TIMx_TRGO 或 ESP32 MCPWM Sync Event），实现零 CPU 干预的硬件级同步触发。

#### B. 低侧电流采样的扇区窗口与相重构（Sector-based Phase Reconstruction）
* **痛点**：在双/三电阻低侧采样中，当某相占空比接近 100% 时，该相的低侧功率管导通时间极短（小于 ADC 的采样保持与转换时间，即进入“盲区”）。此时无法直接读取该相电流。
* **决策**：BAL 层的 FOC 算法必须在每个 PWM 周期根据当前的 SVPWM 扇区（Sector），动态选择两个导通时间最长的相进行 ADC 采样，并通过 $i_a + i_b + i_c = 0$ 公式重构出第三相电流。因此，`dal_current_sense` 驱动必须支持**高速的 ADC 转换通道重映射 API**，或者在初始化时由 DAL 预配置好硬件序列转换，以配合 BAL 的快速扇区切换。

#### C. 毫秒级硬件刹车与故障联动保护（Hardware Brake & nFAULT Protection）
* **痛点**：当栅极驱动芯片（如 DRV8301/8305）检测到过流、欠压或过温时，会拉低硬件 `nFAULT` 引脚。若通过软件中断或轮询来关闭 PWM，MOSFET 可能会在数微秒内烧毁。
* **决策**：
  * **硬中断清场**：`dal_bldc_driver` 初始化时必须绑定 `nFAULT` 的 GPIO 外部中断，一旦触发，中断服务程序（ISR）必须绕过所有中间软件层，直接调用 PAL 寄存器接口瞬间关断 PWM。
  * **硬件刹车支持**：在支持硬件刹车的 SoC 上（如 STM32 的 BRK 输入或 ESP32 MCPWM Fault Detector），应直接通过硬件引脚路由锁死 PWM 输出（High-Z 或三相低侧短路刹车），确保系统安全性达到汽车级/工业级标准。

#### D. 智能栅极驱动芯片的控制与诊断配置（Smart Gate Driver Configuration）
* **痛点**：现代驱动芯片（如 DRV8305 / DRV8323）需要通过 SPI 接口配置栅极驱动电流（控制开关损耗与 EMI）、过流保护阀值、死区时间等，并读取片上诊断寄存器。
* **决策**：`dal_bldc_driver` 不仅要抽象三相 PWM，对于智能驱动芯片，还必须支持 SPI 控制字传输的辅助通道。其 DAL 结构体应可选地绑定一个 `pal_spi` 接口，以便在 `init` 阶段配置芯片寄存器并执行启动自检。

#### E. 电磁隔离与母线电压（Vbus）/温度补偿
* **痛点**：
  * 母线电压 Vbus 的波动会直接改变逆变器的等效控制电压增益，必须进行电压前馈补偿（Voltage Feedforward）。
  * 电机温度上升会导致绕组电阻增加、永磁体磁链衰退，需要温度保护。
* **决策**：`dal_current_sense` 或独立的监控 DAL 器件必须提供低频的 `vbus` 和 `temperature` 遥测接口。BAL 层的 FOC 算法需要周期性读取 `vbus` 用于调节 SVPWM 的调制比幅值，并在电压/温度超过阈值时触发系统降级（Limp Mode）或安全停机。

---

## 影响（Consequence）

* **积极影响**：
  * **高内聚低耦合**：硬件引脚配置、芯片协议与 FOC 算法完全解耦。
  * **硬件零成本替换**：支持“积木式”组装，极大减轻了 AI 进行硬件迁移 and 连线变更时的代码改动工作量。
  * **高保真同源仿真**：电机 plant 收敛至 `targets/common/wink_sim_physical.{h,c}`；DAL `SIMULATION` 分支只做最低物理量旁路与 sim 注入（对标 `dal_ultrasonic`），令上层算法以行为级保真在 PC/Web 闭环调试。仿真快环须虚拟时间确定性步进（R-009，见 [01-dal §8.3.2](../../zh/design/02-wink-micro-os/01-dal-device-abstraction.md)）；完整 ISR/DI 边界以 **[ADR-0047](0047-foc-isr-layering-and-pal-hwtimer.md)** 为准。
* **消极影响 / 开销**：
  * CodeGen 代码生成器需要新增分析引用关系（Dependency Reference）的逻辑，生成**静态绑定**（编译期别名 / 直接 `dal_*` 调用），增加了构建工具链的开发量。
  * 换传感器/驱动桥类型须由 codegen 重绑定，不能运行期换 fn 表；ISR 路径依赖静态分发内联（ADR-0004 / ADR-0047）。

---

*本 ADR 状态变更请在此记录：*
- 2026-07-08：Proposed（FOC DAL/BAL 积木拆分与 DI 方向）
- 2026-07-28：Superseded-in-part（积木拆分保留；ISR/DI/`pal_hwtimer`/数值与仿真快环由 ADR-0047 supersede；§1 去运行期 fn 表示例）

