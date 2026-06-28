# 静态分发架构

> 本项目标准。源出 ADR-0004 + `wink-micro-os/` 设计文档 + chigo-micro 实际代码。

---

## 分层（wink-micro-os）

```
App/BAL  →  DAL  →  PAL  →  Targets (wasm / esp32 / stm32)
（应用+算法）（器件语义抽象）（平台抽象：HAL + OSAL）（平台实现）
```

依赖**严格向下**，无向上调用：

- **App**（应用层）：codegen 生成的业务逻辑（`app_init`/`app_loop`），只调 DAL 语义 API 或 BAL 算法，不碰 GPIO/I2C。
- **BAL**（Business Algorithm Layer）：可复用算法库（PID、卡尔曼、滤波…），独立仓库维护，由 App 调用，亦可直调 DAL 语义 API。
- **DAL**（Device Abstraction Layer）：器件语义驱动（舵机、超声波…）+ 逻辑句柄结构。
  「时序翻译器」——硬依赖 PAL 总线/OS API，**不含芯片寄存器代码**。
- **PAL**（Platform Abstraction Layer）：分 **HAL**（`pal_hal.h`：GPIO/PWM/I2C）与
  **OSAL**（`pal_osal.h`：时间/mutex）。只定义契约，无实现。
- **Targets**：每平台一份 `.c` 适配（`targets/wasm/`、`targets/esp32/`、`targets/stm32/`）。

> chigo-micro（**外部对照仓库**，非本仓库子目录）对照：`comms/ → control/ → driver/ → sensor/ → platform/`，`platform/` 是叶子，
> 同样「依赖向下、无跨层 HAL 直调」。

---

## 铁律

- 器件结构是**纯 POD**：`dal_servo_t { pwm_channel; current_angle; ... }`，**无函数指针、
  无 `ops`、无 `vptr`、无父类嵌入**。
- 调用是**命名式 API 直调**：`dal_servo_set_angle(&dev, angle)` → 直接进函数体，无查表。
- 子类恢复**不需要**——编译期类型已知，你拿的就是具体类型指针，没有「父类指针反推子类」。
- 注册靠**编译期绑定**（codegen 静态全局 / CMake 文件链接），不靠运行期 `MODULE_INIT`。

---

## 4 种静态分发形态

> 「静态分发」不是只有一种写法。本项目里有 4 种合法形态，按层选用。

### 形态 1：POD + 命名 API（主形态 · DAL / 驱动 / 传感器）

器件实例是 POD 结构，公共 API 是命名自由函数，首参传实例指针。

```c
/* wink-micro-os DAL */
typedef struct { uint8_t pwm_channel; float current_angle;
                 float min_pulse_ms; float max_pulse_ms; } dal_servo_t;
wink_status_t dal_servo_set_angle(dal_servo_t *dev, float angle);   /* 目标签名 */

/* chigo-micro driver（同一 idiom） */
typedef struct { encoder_t encoders[NUM_JOINTS]; motor_mode_t mode; ... } motor_driver_t;
void motor_driver_set_outputs(motor_driver_t *drv, const float *outputs);
```

### 形态 2：文件级 CMake 源切换（平台 HAL 形态 · chigo-micro platform）

`platform.h` 是一组命名自由函数，**无实例结构**；多个 `.c` 实现同名符号，构建系统
**二选一链接**。

```
platform.h（接口，不变）
   ├── platform_esp32.c   ← 真机（idf_component_register 选它）
   └── sim/platform_sim.c ← PC 仿真（sim/CMakeLists.txt 显式排除 esp32）
```

换平台 = 换编译哪个 `.c`，零运行期开销。

### 形态 3：设备实例静态实例化（codegen 形态 · wink-micro-os device_tree）

前端拓扑 → codegen → `device_tree.c` 里静态全局结构 + `device_tree.h` extern 导出。
**编译期用 C 初始化器绑定配置与 API**，替代传统 OS 的运行期注册。

```c
/* device_tree.c（codegen 生成，仓库尚无此文件——见 README 偏差框） */
dal_ultrasonic_t front_radar = { .trig_pin = 4, .echo_pin = 5, .last_distance = 0.0f };
dal_servo_t      neck_servo  = { .pwm_channel = 0, .current_angle = 90.0f, ... };
```

PAL 侧的编译期路由则是 CMake `-DTARGET_PLATFORM=<wasm|esp32|stm32>` 静态链接。

### 形态 4：受控的 vtable 例外（局部策略模式 · chigo-micro control_algo）

**唯一的例外**：当需要在**同一抽象**下切换**多种算法实现**（如 PID ↔ 级联 PID）时，可用
一张函数指针表——但**仅限「策略」语义，绝不可用于器件抽象**。

```c
/* control_algo_t —— 受控的、局部的策略 vtable，不是器件模型 */
typedef struct {
    void *(*create)(void);
    void  (*destroy)(void *inst);
    void  (*update)(void *inst, float *out, const float *setpt);
    void  (*reset)(void *inst);
} control_algo_t;
```

> 这正是 ADR-0004「局部多态化退出路径」的合法用法：**不破坏 App/BAL 静态 API 契约**，
> 多态**封装在单个器件/模块内部**。详见 [evolution.md](./evolution.md)。

---

## 与运行期多态的一句话对照

运行期多态（`me->ops->on(me)` + `container_of`）支持运行期热插拔 / 统一句柄管理，代价是
AI 生成不友好 + Wasm 性能损失。本项目拓扑在编译期确定，不需要运行期切换，故选静态分发。
完整对比见 [../index.md](../index.md)。

---

## 静态 Codegen 初始化顺序与依赖拓扑排序

由于本项目舍弃了运行期的 `MODULE_INIT` 动态注册链，所有的器件初始化调用必须在编译期显式排列妥当。为保障初始化顺序的安全与确定，工具链和代码生成器（Codegen）遵循以下物理依赖图排序与执行机制：

### 1. 三阶段串行初始化流程
在 `device_tree.c` 中，由 Codegen 自动生成的 `device_tree_init_all()` 必须严格遵循以下三阶段顺序：
1. **CPU & 核心初始化 (PHASE_CPU_INIT)**：时钟树配置、看门狗启用、基础中断向量表设置。
2. **物理总线与 PAL 抽象初始化 (PHASE_PAL_INIT)**：GPIO 配置、I2C 物理控制器初始化、SPI 总线启动、操作系统多任务环境 (OSAL) 开启。
3. **逻辑器件 DAL 驱动初始化 (PHASE_DAL_INIT)**：超声波、舵机等应用外设逻辑实例的依次初始化。

### 2. DAG（有向无环图）拓扑排序
DAL 层设备实例的初始化顺序不是固定的，而是由 Codegen 在编译前对设备关系拓扑图进行拓扑排序决定的：
* **节点 (Nodes)**：代表各层实例（如 `front_radar`、`pal_i2c_bus0`、`pal_gpio_4`）。
* **有向边 (Edges)**：代表物理与驱动级依赖。例如，`front_radar`（HC-SR04）依赖 `trig_pin` (GPIO4) 和 `echo_pin` (GPIO5)；OLED 屏 `front_oled` 依赖 `i2c_bus0`。
* **排序算法**：Codegen 对图执行标准拓扑排序。如果检测到环形依赖（例如 A 依赖 B，B 依赖 A），Codegen 必须在编译前**直接报 Fatal Error** 并中断构建流程，绝不允许环形依赖进入固件。
* **扁平代码输出**：拓扑排序结束后，输出为扁平线性调用的 `device_tree_init_all()`，消除了任何运行期检索的开销。

### 3. 初始化失败时的处理与降级策略
由于是在引导时执行线性调用，一旦某个初始化环节发生故障，必须具备明确的行为归宿：
* **PAL / 总线级初始化失败**（如 `PHASE_PAL_INIT` 返回 `WINK_ERR_PAL_FAIL`）：
  * **行为**：定义为**致命错误 (Fatal Failure)**。
  * **策略**：系统应立即停止后续初始化，进入安全死循环（如触发 LED 警示闪烁），或触发看门狗复位。
* **DAL / 器件级初始化失败**（如 `front_radar` 未能拉高 Trig 引脚，返回 `WINK_ERR_TIMEOUT`）：
  * **行为**：定义为**局部非致命错误 (Degraded Failure)**。
  * **策略**：不中断系统整体启动。该器件的状态字段应标记为 `WINK_ERR_FAILED_INIT`（-51，ADR-0005）。App（业务逻辑）读取时能捕获该状态，并启动防跌落、停机等降级控制策略，其余正常外设（如控制状态 LED）仍可继续工作。

---

## 4. 上电自检序列（POST, Power-On Self-Test）

安全关键固件上电后应按序对关键器件自检、隔离故障件、向 App/BAL 暴露自检报告。**AI 不会主动加自检**，必须由本规范明确要求——漏掉 POST 的固件，器件半失效时仍被 App/BAL 当正常器件用，是功能安全漏洞。

### 必检器件类

| 类别 | 举例 | 自检手段 |
|------|------|----------|
| 执行类（带反馈回路） | 电机驱动（过流回路）、舵机 | 回读电流 / 位置，确认驱动链导通 |
| 感知类（有应答） | 超声波（echo 通路）、IMU / 温度（WHO_AM_I 寄存器） | 读芯片 ID / 触发一次回波 |

### 自检失败处理

- POST 失败**不中断整体启动**：置该器件 `health = DAL_HEALTH_FAULTED`（见 lifecycle.md §6），App/BAL 隔离该器件、其余继续。
- 仅 `PHASE_CPU_INIT` / `PHASE_PAL_INIT` 失败才是致命（见上 §3）。

### 插入点与接口

- 自检在 `device_tree_init_all()` 的 `PHASE_DAL_INIT` 内：每个 `dal_xxx_init` 成功后调用 `dal_xxx_self_test(dev)`；其返回码映射到 `health`（`-50/-51` → DEGRADED / FAULTED，ADR-0005）。
- 向 App/BAL 暴露 `device_tree_get_post_report()`，返回各器件 init / self_test 结果，供降级决策与可观测性（[02-error-fault-model.md](../../../../../docs/design/07-platform-governance/02-error-fault-model.md) §8 错误可观测性）。
