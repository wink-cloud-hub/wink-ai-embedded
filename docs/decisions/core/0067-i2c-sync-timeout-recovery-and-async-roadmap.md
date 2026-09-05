# ADR-0067：I2C 同步超时恢复与异步 DMA 演进路线

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已通过）** |
| 日期 | 2026-08-24 |
| 影响范围 | `pal_i2c.h`、`pal_hal_i2c_esp32.c`、`pal_hal_i2c_host.c`、`pal_wasm_ch2_bus.c`、DAL OLED/EEPROM/I2C 驱动 |
| 决策者 | 架构委员会 / 核心 PAL 维护组 |
| 关联 ADR | [ADR-0012](0012-contract-honesty-over-silent-degradation.md)（合约诚实）；[ADR-0017](0017-blocking-api-hard-isolation.md)（阻塞 API 隔离）。 |

---

## 背景（Context）

1. **接口分裂**：原 `pal_i2c.h` 仅包含 `pal_i2c_bus_init/deinit`，而传输与扫描接口散落在 `pal_hal.h` 中；
2. **总线挂死与超时缺失**：I2C 从机设备极易在掉电、复位或总线干扰下将 SDA 线永久拉低（SDA stuck low）。原 `pal_i2c_transfer` 缺乏超时控制参数（ESP32 底层写死 1000ms），一旦从机死锁，调用任务将无限期卡顿；且单纯超时返回 `WINK_ERR_TIMEOUT` 无法解除硬件死锁。
3. **属性标记冲突**：`WINK_BLOCKING` 带有面向协作运行时的弃用警告，将其施加在正常的 RTOS 任务上下文同步 I2C 接口上会导致不必要的编译器报警。

---

## 决策（Decision）

1. **整合统一的内聚头文件 `hal/pal_i2c.h`**：
   将总线初始化、注销、传输、扫描统一收敛在 `hal/pal_i2c.h` 中，并将引脚参数由 `uint8_t` 修正为标准 `wink_pin_t`。

2. **支持显式 Wall-Clock 超时与默认回退**：
   ```c
   WINK_WARN_UNUSED_RESULT
   wink_status_t pal_i2c_transfer_timeout(uint8_t port, uint16_t dev_addr,
                                          const uint8_t *write_buf, uint32_t write_len,
                                          uint8_t *read_buf, uint32_t read_len,
                                          uint32_t timeout_ms);
   ```
   - `timeout_ms = 0` 时自动使用 `PAL_I2C_DEFAULT_TIMEOUT_MS (1000ms)`；
   - `timeout_ms = UINT32_MAX` 表示无限期等待；
   - 移除在同步 Task 上下文 API 上的 `WINK_BLOCKING` 误用，承认其在 RTOS 任务模型中的合法地位。

3. **提供硬件级总线死锁恢复接口（SCL 9-Pulse Recovery）**：
   ```c
   WINK_WARN_UNUSED_RESULT
   wink_status_t pal_i2c_bus_recover(uint8_t port);
   ```
   - 严格实现 NXP 规范：将 SCL 翻转 9 个时钟脉冲，令卡死的从机移位寄存器释放 SDA，最后发送 STOP 条件恢复总线为高阻空闲态；
   - 当 `pal_i2c_transfer_timeout` 触发超时返回 `WINK_ERR_TIMEOUT` 时，内部自动触发一次 `bus_recover`。

4. **异步 I2C 演进路线**：
   本次重构聚焦于同步 I2C 健壮性加固。非阻塞/异步 I2C（仿照 `pal_spi_transfer_dma` 的回调模型）纳入 Stage 1 路线图，保持 PAL HAL 设计梯次推进。

---

## 影响（Consequences）

- **正向收益**：消灭了 I2C 驱动中因从机死锁导致的任务永久挂起，统一了头文件内聚性。
- **迁移要求**：旧 `pal_i2c_transfer` 声明为向后兼容 inline wrapper，新驱动优先调用带超时控制的接口。
