# i2c_eeprom — 24C 系列 I²C EEPROM（首版 AT24C256）

T2.1 交付：`wink-ai-embedded` I2C/SPI 死锁计划 Phase 2 的虚拟 EEPROM 插件。

## 能力

- **容量/分页**：默认 32 KiB（256 Kbit）、页 64 B；页写按 24C 语义 **页内回卷（page roll-over）**，跨页继续写不回滚全帧。
- **地址**：默认 7-bit `0x50`（`A0..A2` 由 `address` 属性声明），16-bit 字地址。
- **协议**：Current Address Read / Random Read（写地址 + repeated START + 读）/ Sequential Read；写数据在 **STOP** 时落盘。
- **tWR 写周期 NACK 窗口**：`writeCycleUs > 0` 时，STOP 后到期的写周期内 `onAddressPhase` 返回 NACK，引擎将 `addr_nack=1`（ADR-0085/0086）。默认 0 = 即时模式。
- **线级会话**（ADR-0086）：实现 `onTransactionStart/onAddressPhase/onWriteByte/onReadByte/onTransactionEnd`，支持逐命令控制器（`REPEATED START`、逐字节 ACK）；同时保留整帧 `onTransfer` 供旧 `js_pal_i2c_transfer` 使用（写周期内整帧失败会返回粒度 `nackBits`）。

## 属性

| 属性           | 默认    | 说明                                    |
| -------------- | ------- | --------------------------------------- |
| `address`      | `0x50`  | 7-bit 从机地址                          |
| `sizeBytes`    | `32768` | 存储容量（8 的倍数）                    |
| `pageSize`     | `64`    | 页写回卷边界                            |
| `writeCycleUs` | `0`     | 写周期长度（µs）；>0 开启地址 NACK 窗口 |

## 状态通道

- `addressPointer`：当前字地址指针（读递增/写提交后更新）
- `busy`：`writeCycleUs` 窗口内为 1
- `writeCount`：已提交的写事务计数

## 快照

`serializeState()` 输出 `addressPointer` / `busyUntilUs` / 基址 64 编码的存储镜像；`onReset()` 保留存储（EEPROM 非易失）但复位指针与忙碌窗口。
