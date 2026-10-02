# DAL 外设执行分计划：sensor/imu (惯性测量单元外设)

| 项 | 内容 |
|---|---|
| **计划名称** | `sensor/imu` (惯性测量单元) 跨平台驱动、Codegen 与仿真插件全栈落地计划 |
| **所属批次** | P2 批次 (IoT/进阶传感与存储总线外设，序号 #16) |
| **映射 Wokwi 组件** | `wokwi-mpu6050` (🟢 Native 原生匹配) |
| **驱动文件路径** | `wink-micro-os/dal/include/sensor/dal_imu.h`<br>`wink-micro-os/dal/src/sensor/dal_imu.c` |
| **Codegen 描述** | `wink-micro-os/codegen/drivers/imu.yaml`（**ADR-0051：YAML 为 SSOT**） |
| **Role 契约规范** | `six_axis_imu` / `accel_sensor`（[`dal-role-architecture-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-role-architecture-spec.md) §3 & §4） |
| **前端仿真插件** | `wink-plugin-peripherals/builtin/imu/` (UniSim 8 引脚高保真运动学模型) |
| **单测文件路径** | `wink-micro-os/test/unit/dal/test_dal_imu.c`（Unity C 测试套件） |
| **关联规范** | [`00-master-execution-plan.md`](./00-master-execution-plan.md) (#16 `imu`), [`00.1-category-type-variant-wokwi-ssot.md`](./00.1-category-type-variant-wokwi-ssot.md) (§2.4 #18), [`dal-api-consistency-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md) (v3.4.3), [ADR-0056](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md) (量纲封闭后缀), [ADR-0051](../../decisions/tools/0051-scannable-codegen-extension-roots.md) |
| **前置依赖** | **已就绪**：PAL I2C (`pal_i2c_*`) 与 GPIO 中断 (`pal_gpio_irq_*`) 已就绪 |
| **计划状态** | 🆕 规划落地版（经三维抽象、物理运动学模型、全生命周期与 5 大 Guard 全量硬化） |

---

## 1. 架构定位与“三维抽象设计心法”

### 1.1 独立确立 `Type = imu` 的架构法理依据

依据 [`00-master-execution-plan.md`](./00-master-execution-plan.md) §1.2「三维抽象设计心法（DAL 架构护城河）」与 [`00.1-category-type-variant-wokwi-ssot.md`](./00.1-category-type-variant-wokwi-ssot.md) §2.4 第 18 项：

1. **`type` = 驱动护城河（绝对禁止以具体芯片命名）**：
   * 惯性测量单元外设统一归属于 **`category: sensor`** 下的顶级独立 **`type: imu`**。
   * 严禁将 `mpu6050` 或 `mpu9250` 等具体芯片型号立为独立 `type`。一旦以芯片型号立为 type，将导致 C 侧 `app_codegen` 识别失败、破坏 DAL 设备树一致性、并在真机固件与前端仿真间引入概念漂移。
2. **`variant` = 同族避风港（收拢芯片拓扑）**：
   * MPU 家族芯片差异在 `type: imu` 内部以细分 `variant` 枚举收拢：
     - `mpu6050_i2c`：6 轴（3 轴加速度 + 3 轴陀螺仪），I2C 接口，`WHO_AM_I` 默认 `0x68`；
     - `mpu6500_i2c`：6 轴，I2C 接口，`WHO_AM_I` 默认 `0x70`；
     - `mpu9250_i2c`：9 轴（6 轴 IMU + AK8963 3 轴磁力计），I2C 接口，`WHO_AM_I` 默认 `0x71`；
     - `mpu6500_spi`：6 轴，4 线硬件 SPI，支持高速 DMA 读取；
     - `mpu9250_spi`：9 轴，4 线硬件 SPI。
   * 具体芯片型号名仅作为 Codegen 别名（`aliases` / `ic_to_variant_map`）存在，对外暴露的 C API（`dal_imu_*`）和 Role 契约保持 100% 稳定一致。
3. **原生组件对齐（Native Wokwi Match）**：
   - 完美对齐 Wokwi 原生 WebComponent `<wokwi-mpu6050>`，实现 1:1 引脚拓扑、动态 3D 姿态/加速度可视化以及 Class 4 高速总线物理流传输。

---

## 2. 硬件拓扑与 Variant 变体分类

### 2.1 变体枚举与引脚拓扑 (`dal_imu_variant_t`)

依据 SSOT §1.1 变体设计规范与 Class 4（高速硬件总线流传输类）要求，穷举底层硬件变体：

| 变体枚举名称 | 物理引脚拓扑 | `affects_pins` | 底层总线与时序特征 | 仿真适配 (Wokwi) | 落地阶段 / 状态 |
|---|---|:---:|---|:---:|---|
| **`DAL_IMU_VARIANT_MPU6050_I2C`** (0) | `[VCC, GND, SCL, SDA, AD0, INT, XCL, XDA]` (8Pin) | `false` | **标准硬件 I2C**：Class 4，地址由 `AD0` 决定 (`0x68`/`0x69`)，`XCL/XDA` 赋 `-1` 哨兵 | 🟢 Native (`wokwi-mpu6050`) | **Phase 1 全功能落地** |
| **`DAL_IMU_VARIANT_MPU6500_I2C`** (1) | `[VCC, GND, SCL, SDA, AD0, INT]` (6Pin) | `true` | **标准硬件 I2C**：Class 4，`WHO_AM_I` 默认 `0x70` | 🟢 Native | **Phase 1 全功能落地** |
| **`DAL_IMU_VARIANT_MPU9250_I2C`** (2) | `[VCC, GND, SCL, SDA, AD0, INT, XCL, XDA]` (8Pin) | `false` | **I2C + 内部 AK8963 Bypass**：Class 4，支持 9 轴融合访问 | 🟢 Native | **Phase 1 全功能落地** |
| **`DAL_IMU_VARIANT_MPU6500_SPI`** (3) | `[VCC, GND, SCLK, SDI, SDO, CS, INT]` (7Pin) | `true` | **4 线硬件 SPI**：Class 4，支持高达 20MHz 读取与 DMA 批量传输 | 🔴 Custom | 🟡 预留枚举 (Phase 2) |
| **`DAL_IMU_VARIANT_MPU9250_SPI`** (4) | `[VCC, GND, SCLK, SDI, SDO, CS, INT]` (7Pin) | `true` | **4 线硬件 SPI**：Class 4，支持 9 轴高频采集 | 🔴 Custom | 🟡 预留枚举 (Phase 2) |

### 2.2 市面主流 IMU 芯片全量盘点与 Codegen 别名表 (`aliases`)

在 `wink-micro-os/codegen/drivers/imu.yaml` 中建立标准芯片别名库：

| 工业芯片型号 | 轴数 | 通信协议 | 默认从机地址 | 满量程量程范围 | 典型应用领域 | 映射变体 (`variant`) |
|---|:---:|:---:|:---:|:---:|---|---|
| **`MPU-6050`** | 6 轴 | I2C | `0x68` / `0x69` | ±2g~±16g, ±250~±2000 dps | 平衡车、开源四轴无人机、万向节稳定器 | `mpu6050_i2c` |
| **`MPU-6000`** | 6 轴 | I2C / SPI | `0x68` / CS | ±2g~±16g, ±250~±2000 dps | 穿越机飞控 (经典防震高采样率) | `mpu6050_i2c` / `mpu6500_spi` |
| **`MPU-6500`** | 6 轴 | I2C / SPI | `0x70` / `0x71` | ±2g~±16g, ±250~±2000 dps | 智能穿戴、VR 手柄、机器人关节 | `mpu6500_i2c` / `mpu6500_spi` |
| **`MPU-9250`** | 9 轴 | I2C / SPI | `0x71` / `0x72` | 6轴 + ±4800 µT 磁场 | 室内导航、姿态参考系统 (AHRS)、天向追踪 | `mpu9250_i2c` / `mpu9250_spi` |
| **`ICM-20602`** | 6 轴 | I2C / SPI | `0x68` / `0x69` | 低噪声高带宽 | 工业机器人、云台相机 | `mpu6500_i2c` (兼容模式) |

Codegen 别名映射配置：
```yaml
aliases:
  mpu6050:
    variant: mpu6050_i2c
    i2c_addr: 104
  mpu6500:
    variant: mpu6500_i2c
    i2c_addr: 112
  mpu9250:
    variant: mpu9250_i2c
    i2c_addr: 104
```

---

## 3. 5 大资深架构底线 Guard 落地硬化方案

依据 [`00-master-execution-plan.md`](./00-master-execution-plan.md) §1.3，本驱动强制实施 5 道运行时质量防线：

1. **🛡️ Guard A: 低功耗预留 (`enable_pin` 与 `safe_sleep`)**：
   - `dal_imu_config_t` 首部硬件参数统一预留 `int16_t enable_pin` (默认 `-1` 为未绑定)，支持外接 LDO 使能脚硬件级完全断电；
   - 软件层面实现 `dal_imu_sleep(dev)` 与 `dal_imu_wakeup(dev)`，操作 `PWR_MGMT_1` 的 `SLEEP` (bit 6)，休眠静态电流 $< 5\,\mu\text{A}$。
2. **🛡️ Guard B: I2C Bus-Owner 总线共享与复用**：
   - `init()` 必须遵守总线复用契约：首先探测该 `i2c_port` 是否已由 PAL 打开；若已打开则增加引用计数复用句柄，绝不强制独占关闭或重配波特率破坏其他 I2C 从机。
3. **🛡️ Guard C: 零值即默认 (Zero-as-Default)**：
   - `i2c_hz == 0` $\rightarrow$ 自动推导为 `400000` (400kHz Fast Mode)；
   - `accel_fs == 0` $\rightarrow$ 自动推导为 `0` (±2g 默认)；
   - `gyro_fs == 0` $\rightarrow$ 自动推导为 `0` (±250 dps 默认)；
   - `sample_rate_hz == 0` $\rightarrow$ 自动推导为 `100` (100Hz 经典姿态解算周期)。
4. **🛡️ Guard D: 高频事件底层中断托底 (Anti-Polling-Loss)**：
   - 驱动支持配置 `int_pin` 并直接挂接 `pal_gpio_irq_*` EXTI 边沿捕获；在 Data Ready 中断到来时唤醒采集任务，杜绝 App 纯软件空等轮询。
5. **🛡️ Guard E: 高带宽读取异步化与 DMA 预留 (Anti-CPU-Blocking)**：
   - 为 SPI 变体预留 FIFO 批量突发读取 DMA 接口契约（`request_read` / `is_read_done`），单次突发传输 14 字节（6 轴 + 温度）不阻塞 CPU。

---

## 4. 数据结构与 C API 规范设计 (`dal_imu.h`)

### 4.1 跨 Profile 量纲决策 (遵循 ADR-0056 与封闭后缀表)

* **B 类传感器测量分类**：遵循 ADR-0056，禁止使用裸 `float` 或非法后缀（如 `_val` / `_norm`），全仓统一使用封闭量纲整型：
  1. **`accel_mg`（毫 g，`int16_t`）**：$1\,\text{g} = 1000\,\text{mg}$。静态水平放置时 $A_z = +1000$。量程覆盖 $\pm 16000\,\text{mg}$，`int16_t` 零开销存储；
  2. **`gyro_dps`（角速度，度每秒，`int16_t`）**：量程覆盖 $\pm 2000\,\text{dps}$；
  3. **`temp_ddegc`（十分之一摄氏度，`int16_t`）**：$25.4^\circ\text{C}$ 表示为 `254`（51 与嵌入式通用）；Full Profile (ESP32/Wasm) 额外提供 `temp_degc`（`float`）；
  4. **`_raw`（原始 ADC LSB 补码，`int16_t`）**：直接返回 16 位未缩放原始数据，供上层校准算法使用。

### 4.2 错误码契约 (严格对齐 ADR-0001 与 `wink_status.h`)

* `WINK_OK = 0`：操作成功；
* `WINK_ERR_INVALID_ARG = -1`：传入空指针或引脚配置越界；
* `WINK_ERR_NOT_INITIALIZED = -11`：句柄未初始化；
* `WINK_ERR_COMM_FAIL = -14`：I2C/SPI 总线通信失败（NACK 或总线仲裁丢失）；
* `WINK_ERR_CHIP_ID_MISMATCH = -15`：`WHO_AM_I` 读取值与变体预期不匹配（器件离线或损坏）；
* `WINK_ERR_TIMEOUT = -18`：等待中断或数据转换超时。

### 4.3 头文件完整契约 (`wink-micro-os/dal/include/sensor/dal_imu.h`)

遵循 `dal-api-consistency-spec.md` 及 `DAL-S-001/006/011/014`，严格遵守 `DAL-HDR-NO-HAL`：

```c
// SPDX-License-Identifier: LGPL-3.0-only
#ifndef DAL_IMU_H
#define DAL_IMU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
/* 严格遵循 DAL-HDR-NO-HAL: 禁止 include "pal_hal.h" */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IMU 变体拓扑枚举 (一等公民)
 */
typedef enum {
    DAL_IMU_VARIANT_MPU6050_I2C = 0, /**< MPU-6050 I2C 6轴 (0x68/0x69) */
    DAL_IMU_VARIANT_MPU6500_I2C = 1, /**< MPU-6500 I2C 6轴 (0x70) */
    DAL_IMU_VARIANT_MPU9250_I2C = 2, /**< MPU-9250 I2C 9轴 (0x71 + AK8963 0x0C) */
    DAL_IMU_VARIANT_MPU6500_SPI = 3, /**< MPU-6500 SPI 6轴 (预留) */
    DAL_IMU_VARIANT_MPU9250_SPI = 4, /**< MPU-9250 SPI 9轴 (预留) */
} dal_imu_variant_t;

/**
 * @brief 加速度计量程枚举
 */
typedef enum {
    DAL_IMU_ACCEL_FS_2G  = 0, /**< ±2g  (16384 LSB/g) */
    DAL_IMU_ACCEL_FS_4G  = 1, /**< ±4g  (8192 LSB/g) */
    DAL_IMU_ACCEL_FS_8G  = 2, /**< ±8g  (4096 LSB/g) */
    DAL_IMU_ACCEL_FS_16G = 3, /**< ±16g (2048 LSB/g) */
} dal_imu_accel_fs_t;

/**
 * @brief 陀螺仪量程枚举
 */
typedef enum {
    DAL_IMU_GYRO_FS_250DPS  = 0, /**< ±250  °/s (131.0 LSB/dps) */
    DAL_IMU_GYRO_FS_500DPS  = 1, /**< ±500  °/s (65.5 LSB/dps) */
    DAL_IMU_GYRO_FS_1000DPS = 2, /**< ±1000 °/s (32.8 LSB/dps) */
    DAL_IMU_GYRO_FS_2000DPS = 3, /**< ±2000 °/s (16.4 LSB/dps) */
} dal_imu_gyro_fs_t;

/**
 * @brief IMU 配置结构体 (POD config_t)
 * 成员对齐降序：指针 -> uint32 -> uint16 -> int16 -> enum -> bool
 */
typedef struct {
    const char *owner;              /**< 资源占用者名称 (DAL-S-001) */
    uint32_t bus_speed_hz;          /**< I2C/SPI 总线时钟 (0 代表默认 400kHz/10MHz) */
    uint16_t sample_rate_hz;        /**< 采样率 (0 代表默认 100Hz) */
    uint8_t i2c_port;               /**< I2C 控制器端口号 */
    uint8_t i2c_addr;               /**< 7位 I2C 从机地址 (0 代表 0x68) */
    int16_t sda_pin;                /**< I2C SDA 引脚 (必填或由 Board 推导) */
    int16_t scl_pin;                /**< I2C SCL 引脚 (必填或由 Board 推导) */
    int16_t int_pin;                /**< 中断引脚 (-1 为未绑定) */
    int16_t enable_pin;             /**< 硬件供电使能脚 (-1 为未绑定, Guard A) */
    dal_imu_variant_t variant;      /**< 拓扑变体 */
    dal_imu_accel_fs_t accel_fs;    /**< 加速度计量程 */
    dal_imu_gyro_fs_t gyro_fs;      /**< 陀螺仪量程 */
} dal_imu_config_t;

/**
 * @brief IMU 句柄结构体 (POD instance_t)
 */
typedef struct {
    dal_imu_config_t config;        /**< 配置副本 (DAL-S-011: 值副本且首成员) */
    int16_t accel_raw[3];           /**< 原始 X/Y/Z 加速度 LSB */
    int16_t gyro_raw[3];            /**< 原始 X/Y/Z 角速度 LSB */
    int16_t temp_raw;               /**< 原始片上温度 LSB */
    uint8_t who_am_i;               /**< 读出的实际器件 ID */
    bool initialized;               /**< 初始化标记 */
    bool is_sleeping;               /**< 当前休眠态 */
} dal_imu_t;

/* DAL-S-014: 首成员偏移断言 */
_Static_assert(offsetof(dal_imu_t, config) == 0,
               "config must be the first member of dal_imu_t");

/* ── 核心驱动 API (DAL-F-*) ────────────────────────────────────────── */

wink_status_t dal_imu_init(dal_imu_t *dev, const dal_imu_config_t *cfg);
wink_status_t dal_imu_deinit(dal_imu_t *dev);

wink_status_t dal_imu_read_accel_raw(dal_imu_t *dev, int16_t out_accel_raw[3]);
wink_status_t dal_imu_read_accel_mg(dal_imu_t *dev, int16_t out_accel_mg[3]);

wink_status_t dal_imu_read_gyro_raw(dal_imu_t *dev, int16_t out_gyro_raw[3]);
wink_status_t dal_imu_read_gyro_dps(dal_imu_t *dev, int16_t out_gyro_dps[3]);

wink_status_t dal_imu_read_temp_ddegc(dal_imu_t *dev, int16_t *out_temp_ddegc);
wink_status_t dal_imu_read_temp_degc(dal_imu_t *dev, float *out_temp_degc);

wink_status_t dal_imu_sleep(dal_imu_t *dev);
wink_status_t dal_imu_wakeup(dal_imu_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DAL_IMU_H */
```

---

## 5. Codegen 驱动描述与 Role 面板映射 (`imu.yaml`)

SSOT 驱动定义位于 `wink-micro-os/codegen/drivers/imu.yaml`：

```yaml
schema_version: "1.1"
type: imu
category: sensor
display_name: "Inertial Measurement Unit (6-Axis/9-Axis)"
description: "6-Axis/9-Axis IMU sensor (accelerometer, gyroscope, temperature) supporting MPU-6050/6500/9250"
c_prefix: dal_imu
header: sensor/dal_imu.h
is_actuator: false
default_role: six_axis_imu

fields:
  owner:
    type: string
    required: false
    description: "Resource owner identifier"
  bus_speed_hz:
    type: integer
    default: 400000
    description: "Bus clock frequency in Hz"
  sample_rate_hz:
    type: integer
    default: 100
    description: "Sample rate in Hz"
  i2c_port:
    type: integer
    default: 0
    description: "I2C port index"
  i2c_addr:
    type: integer
    default: 104
    description: "7-bit I2C address (104 = 0x68)"
  sda_pin:
    type: pin
    required: true
    description: "I2C SDA pin"
  scl_pin:
    type: pin
    required: true
    description: "I2C SCL pin"
  int_pin:
    type: pin
    default: -1
    description: "Data ready interrupt pin"
  enable_pin:
    type: pin
    default: -1
    description: "Power enable pin"
  variant:
    type: enum
    values: [mpu6050_i2c, mpu6500_i2c, mpu9250_i2c, mpu6500_spi, mpu9250_spi]
    default: mpu6050_i2c
    affects_pins: true
  accel_fs:
    type: enum
    values: [2g, 4g, 8g, 16g]
    default: 2g
  gyro_fs:
    type: enum
    values: [250dps, 500dps, 1000dps, 2000dps]
    default: 250dps

config:
  init_fn: dal_imu_init
  deinit_fn: dal_imu_deinit

role_bindings:
  six_axis_imu:
    verbs:
      read_accel: dal_imu_read_accel_mg
      read_gyro: dal_imu_read_gyro_dps
      read_temp: dal_imu_read_temp_ddegc
  accel_sensor:
    verbs:
      read_acceleration: dal_imu_read_accel_mg
```

---

## 6. UniSim 前端外设仿真模型设计 (`builtin/imu/`)

### 6.1 Wokwi 原生 `<wokwi-mpu6050>` 8 引脚拓扑

| 引脚名 | pinType | simRole | 方向 | 说明 |
|---|---|---|:---:|---|
| `VCC` | `power` | `power` | in | 3.3V 供电轨 |
| `GND` | `ground`| `ground`| in | 参考地 |
| `SCL` | `i2c_scl`| `signal`| in | I2C 时钟 |
| `SDA` | `i2c_sda`| `signal`| inout| I2C 数据 |
| `AD0` | `digital_in`| `signal`| in | 地址选通 (低=0x68, 高=0x69) |
| `INT` | `digital_out`| `signal`| out | 中断输出 (Read-to-Clear) |
| `XCL` | `digital_out`| `signal`| out | 辅助时钟 (变体赋 `-1`) |
| `XDA` | `bidirectional`| `signal`| inout| 辅助数据 (变体赋 `-1`) |

### 6.2 3D 重力投影运动学模型与大端序拼装

模型内部维护空间姿态（Roll $\phi$, Pitch $\theta$, Yaw $\psi$）：
$$A_x = -16384 \cdot \sin(\theta)$$
$$A_y = 16384 \cdot \sin(\phi) \cos(\theta)$$
$$A_z = 16384 \cdot \cos(\phi) \cos(\theta)$$
所有 16 位传感器寄存器（`ACCEL_X/Y/Z`, `TEMP`, `GYRO_X/Y/Z`）按 **Big-Endian（高字节 `0x3B`，低字节 `0x3C`）** 打包。

### 6.3 完整生命周期与虚拟时间节流

- 实现 `onBound`, `onPinChange`, `onReset`, `onPropertyChange`, `serializeState`, `deserializeState`, `onPowerOff`, `onDestroy`；
- 状态通道采用 `createThrottlePublish({ intervalUs: 33_000n })`（30fps 节流），消除事件风暴。
- 变体 `mpu9250_i2c` 支持在 `INT_PIN_CFG` 的 `BYPASS_EN` 置位时直通虚拟 AK8963 磁力计（地址 `0x0C`）。

---

## 7. 三仓闭环验证与质量门禁卡点 (Quality Gates)

| 卡点编号 | 检查项目 | 验收指标与执行命令 |
|:---:|---|---|
| **Gate-1** | 插件 Vite 构建与 Manifest 校验 | `node .../vite.js build --config vite.config.sim.ts`<br>`bun scripts/emit-sim-manifests.mjs`<br>产出 `dist/simulation.js` 与 `dist/manifest.json`，无 TS 编译报错。 |
| **Gate-2** | DeviceTree 自动化生成验证 | `wink-app.json` 声明 `type: "imu"`，验证 `winkcli build sim` 派生出标准 `unisim-assets/device-tree.json`。 |
| **Gate-3** | ESP-IDF 原厂示例验收 | 运行 `run_esp32_headless_evidence.ps1 -App i2c_basic -Reporter json`，原厂代码正确读出 `WHO_AM_I = 71` 并通过 `ASSERT_BUS_PAYLOAD`。 |
| **Gate-4** | Canary 变异测试 (Fail-Loud) | 篡改 `matcher` 为非 `7571`，断言引擎必须 Fail-Loud 报错拒绝；恢复后转绿。 |
| **Gate-5** | 全量治理门检通过 | 运行 `python wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py`，Gate 1~4 全检 0 错误 0 警告退出码 0。 |
| **Gate-6** | DAL C 驱动 6 Pack Lint 全通 | `python wink-tools/wink.py lint --pack layering --pack api --pack drivers --pack dal --pack abi --pack user_surface` 零新增告警。 |
