# ADR-0065：PAL 独占硬件生命周期 RAII 资源所有权模型

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已通过）** |
| 日期 | 2026-08-24 |
| 影响范围 | `pal_resource.h`、`pal_gpio.h`、`pal_pwm.h`、`pal_i2c.h`、14 个 DAL 外设驱动 |
| 决策者 | 架构委员会 / 核心 PAL 维护组 |
| 关联 ADR | [ADR-0012](0012-contract-honesty-over-silent-degradation.md)（合约诚实）；[ADR-0024](0024-fault-three-phase-model-and-dal-deinit-contract.md)（DAL 生命周期契约）。 |

---

## 背景（Context）

当前仓库内存在严重的**双重 Claim 冲突（Double-Claim Bug）**与所有权边界混乱：
1. `pal_hal_pwm_esp32.c:53,60` 在 PAL 初始化时自动执行 `pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL)` 与 `PAL_RESOURCE_GPIO_PIN`；
2. GPIO 驱动在 PAL 内部不 claim，反而是由上层 DAL 驱动手动 claim（如 `dal_led.c`、`dal_relay.c`、`dal_buzzer.c`、`dal_ultrasonic.c`、`dal_load_cell.c`、`dal_encoder.c` 等 14 个文件共 30 处）；
3. Arduino Shim 层又在 `Common.cpp` 中执行了一次 claim。

这种割裂导致：
- 若 DAL 与 PAL 均对引脚进行 claim，同一引脚会被二次 claim 导致 `WINK_ERR_RESOURCE_BUSY` 假死；
- 若新驱动漏写 claim，则两个设备占用同一物理引脚时系统静默冲突（违反 ADR-0012）。

---

## 决策（Decision）

1. **确立 PAL 独占硬件生命周期的 RAII 模型**：
   - 凡属于**物理硬件单元**（`PAL_RESOURCE_GPIO_PIN`、`PAL_RESOURCE_PWM_CHANNEL`、`PAL_RESOURCE_I2C_PORT`、`PAL_RESOURCE_SPI_BUS`、`PAL_RESOURCE_ADC_CHANNEL`），**统一由 PAL API 在 Init 时自动 Claim，在 Deinit 时自动 Release**。
   - 所有 PAL 初始化接口（`pal_gpio_init*`、`pal_pwm_init*`、`pal_i2c_bus_init` 等）在成功配置硬件前必须先锁定对应资源，若资源已被占用则立即返回 `WINK_ERR_RESOURCE_BUSY`。
   - 所有 PAL 注销接口（`pal_gpio_deinit`、`pal_pwm_deinit`、`pal_i2c_bus_deinit` 等）在释放硬件前必须释放对应资源。

2. **DAL 层禁止直接声明物理引脚所有权**：
   - 批量裁撤现有 14 个 DAL 驱动中约 30 处冗余的 `pal_resource_claim(PAL_RESOURCE_GPIO_PIN, ...)`。
   - DAL 层只允许管理**协议/总线级逻辑资源**（例如 I2C 从机地址 `PAL_RESOURCE_I2C_ADDR`、SPI 片选设备句柄 `PAL_RESOURCE_SPI_CS`）。

3. **统一 Owner 标识符**：
   - PAL 内部 claim 统一使用模块标准名称，如 `"pal_gpio"`、`"pal_pwm"`、`"pal_i2c"`。

---

## 影响（Consequences）

- **正向收益**：彻底消除了 DAL 与 PAL 之间的资源抢占死锁，DAL 驱动代码大幅精简，引脚冲突在底层 PAL 调用处立即 Fail-Loud。
- **迁移要求**：需在 Phase 3 批量清理 DAL 驱动中的手动 claim/release 调用。
