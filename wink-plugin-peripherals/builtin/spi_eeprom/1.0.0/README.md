# spi_eeprom — 25/95 系列 SPI EEPROM（首版 M95256）

T2.2 交付：`wink-ai-embedded` I2C/SPI 死锁计划 Phase 2 的虚拟 SPI EEPROM 插件，遵循 ADR-0087（`packages/unisim/docs/internals/decisions/0087-spi-session-stream-and-cs-edge-abi.md`，私有仓）的 CS 帧契约。

## 能力

- **容量/分页**：默认 32 KiB（M95256，256 Kbit）、页 64 B；页写 **页内回卷**，写周期 `tW` 由 `writeCycleUs` 表示。
- **命令集**：`WREN 0x06` / `WRDI 0x04` / `RDSR 0x05` / `WRSR 0x01`（接受并忽略）/ `READ 0x03` / `WRITE 0x02`。
- **WEL 锁存**：`WREN` 帧在 **CS 上升沿**（帧结束）置 WEL；`WRDI` 清除；`WRITE` 完成后自动清除（器件语义）。
- **WIP 窗口**：`WRITE` 提交后置 WIP 至 `writeCycleUs` 到期；窗口内仅 `RDSR` 被接受，其余帧忽略（真实器件行为）。
- **双契约**：线级 `onExchangeByte`（ADR-0087 会话流）+ 整帧 `onFrame`（一次调用 = 一帧，legacy/`_ex` 路径）。
- **状态通道**：`wel` / `wip` / `writeCount`。

## 属性

| 属性           | 默认         | 说明                                            |
| -------------- | ------------ | ----------------------------------------------- |
| `deviceId`     | `spi_eeprom` | 板级绑定逻辑设备号（ADR-0087 `device_id` 契约） |
| `sizeBytes`    | `32768`      | 存储容量                                        |
| `pageSize`     | `64`         | 页写回卷边界                                    |
| `writeCycleUs` | `0`          | 写周期 tW（µs）；0 = 即时模式                   |

## 快照

`serializeState()` 输出 WEL / WIP 截止时间 / 写计数 / 存储镜像；`onReset()` 保留存储（EEPROM 非易失）并清 WEL 与写窗口。
