# ADR-0006：ESP-IDF v6.x I2C 驱动兼容性适配

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-06-27 |
| 触发 | ESP-IDF v6.0 对 I2C 驱动进行不兼容架构重构（旧 `driver/i2c.h` 标记 EOL，v7.0 计划完全移除），需保证 `wink-micro-os` 在 v5.1.3 与 v6.x 下同源编译 |
| 影响范围 | `targets/esp32` PAL HAL（I2C 部分）；关联 [ADR-0002](../unisim/0002-dual-target-compilation.md)（双 target 同源）、[ADR-0004](./0004-static-dispatch-vs-runtime-ops.md)（编译期静态分发） |
| 决策者 | 内核负责人 |

---

## 背景（Context）

ESP-IDF v6.0 重构了 I2C 驱动模型：

- **旧 API**（`driver/i2c.h`）：单级 `port` 编号模型，`i2c_param_config()` → `i2c_driver_install()` → `i2c_master_write_read_device(port, addr, ...)`。已标记 **EOL**。
- **新 API**（`driver/i2c_master.h`）：**总线-设备二级句柄模型**，`i2c_new_master_bus()` → `i2c_master_bus_add_device()` → `i2c_master_transmit_receive(dev_handle, ...)`。设备地址在 `add_device` 时绑定到 handle，而非每次传输传入。
- **v7.0** 计划**完全移除**旧 API。

`wink-micro-os` 的 ESP32 PAL 层此前仅使用 v5.x 旧 API。若不适配，v6.x 环境下会产生 deprecation warning，v7.x 下将无法编译——直接威胁 [ADR-0002](../unisim/0002-dual-target-compilation.md) 的"双 target 同源编译"根基。

PAL 公开 API `pal_i2c_transfer(port, dev_addr, write_buf, write_len, read_buf, read_len)` 是跨平台稳定契约（DAL/App 依赖），**签名不可变更**。所有版本差异必须封闭在 `targets/esp32/pal_hal_esp32.c` 内部。

---

## 方案比选（Options）

### 轴 1：版本分发机制

| 方案 | 优点 | 缺点 |
|---|---|---|
| **A. 编译期静态门控**（`ESP_IDF_VERSION` 宏 + `#if`） | 零运行期开销；符合 [ADR-0004](./0004-static-dispatch-vs-runtime-ops.md) 静态分发；双版本同源 | 两套实现共存于同一文件，代码量增加 |
| B. 运行时函数指针分发 | 单一二进制可跨版本 | 引入运行期间接，违背 ADR-0004；ESP-IDF 版本编译期已知，运行期分发属过度设计 |
| C. 仅支持 v6、放弃 v5 | 实现最简 | 破坏 v5.1.3 LTS 用户兼容性；违背 ADR-0002 同源承诺 |

**裁决：A**。版本在编译期完全确定，静态门控与项目既有架构原则一致。

### 轴 2：v6 二级句柄模型的设备 handle 管理

| 方案 | 优点 | 缺点 |
|---|---|---|
| **A. 懒加载设备句柄缓存 + FIFO 替换**（MVP 4 slot/port） | 首次访问后零开销；内存可控（静态数组） | 超过 4 设备需淘汰（有 warning 日志） |
| B. 每次传输 `add_device`/`rm_device` | 无需缓存 | 每次传输额外开销，且 `rm_device` 语义复杂 |
| C. 静态预注册所有设备 | 运行时最快 | 需预先知道所有设备地址，违背 PAL "传输时才传 addr" 契约 |

**裁决：A**。MVP 阶段每总线 4 设备覆盖典型场景（OLED + 传感器群），FIFO 替换提供溢出降级。详见技术设计 §3.2。

### 轴 3：ESP-IDF 错误码映射粒度

| 方案 | 优点 | 缺点 |
|---|---|---|
| **A. 精细映射**（`ESP_ERR_TIMEOUT`→`WINK_ERR_TIMEOUT` 等 7→6 映射） | 上层可按语义做重试/断连策略 | 映射表需维护 |
| B. 统一映射为 `WINK_ERR_HARDWARE` | 实现最简 | 丢失错误语义，上层无法区分超时/NACK/断连 |

**裁决：A**。与 [07-platform-governance/02](../../zh/design/07-platform-governance/02-error-fault-model.md) 错误码模型对齐，赋能 DAL 重试策略。

### 轴 4：应急回退与前向保护

- **回退开关**：采纳 `CONFIG_WINK_I2C_FORCE_V5_API` Kconfig 项——即使检测到 v6.x，也允许强制走 v5.x 路径，应对 v6.x 驱动潜在 bug。
- **v7.0 前向保护**：`#if ESP_IDF_VERSION >= VAL(7,0,0)` `#error`——v7 API 未验证，编译期阻断而非静默崩溃。

---

## 决策结论（Decision）

采纳 **A\* = 轴1-A + 轴2-A + 轴3-A + 轴4 全采纳**，全部封闭在 `targets/esp32/pal_hal_esp32.c`：

1. **编译期版本门控**：`ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)` 选择 `i2c_master.h`（v6 路径）或 `i2c.h`（v5 路径）。
2. **设备句柄懒加载缓存**：`s_i2c_dev_cache[port][I2C_MAX_DEVICES]`，首次访问地址时 `i2c_master_bus_add_device`，命中则直接复用；缓存满按 FIFO 淘汰 slot 0。
3. **精细错误码映射**：`pal_i2c_map_esp_err()` 将 7 种 ESP err 映射到 6 种 `wink_status_t`。
4. **静态互斥锁并发保护**：`xSemaphoreCreateMutexStatic` 保护初始化与缓存操作（数据传输在临界区外，由 ESP-IDF 内部锁处理总线并发）。
5. **强制回退开关**：`CONFIG_WINK_I2C_FORCE_V5_API=y` 强制 v5 路径。
6. **v7.0 前向保护**：检测到 v7.x 时 `#error`，提示先更新本 ADR 与技术设计。
7. **PAL API 零变更**：`pal_i2c_transfer()` 签名与语义完全不变，DAL/App 无感知。

CMake 侧：`targets/esp32/CMakeLists.txt` 的 `REQUIRES` 增加 `esp_driver_i2c` 组件依赖（保留 `driver` 供 GPIO/RMT/LEDC）。

---

## 后果与约束（Consequences & Constraints）

- **正向**：v5.1.3 与 v6.0.x/v6.1 同一份代码同源编译，零 deprecation warning；为 v7.0 迁移预留了受控升级路径。
- **约束**：
  - `pal_hal_esp32.c` I2C 部分代码量约翻倍（双路径并存），这是静态分发的固有代价，可接受。
  - MVP 固定 SDA/SCL 引脚映射（I2C0: 21/22, I2C1: 33/32），引脚可配置留作 Phase 3（技术设计 §3.3 FIXME）。
  - 每总线设备数上限 4（`I2C_MAX_DEVICES`），超出走 FIFO 淘汰并告警。
- **代码生成约束**：DAL 不得直接调用 `driver/i2c*.h`，只能经 `pal_i2c_transfer`；版本门控仅存在于 `targets/esp32`。

---

## 遵循与后续（Compliance & Follow-up）

| 动作 | 状态 |
|---|---|
| 代码实现（`pal_hal_esp32.c` I2C 重写 + CMake 依赖） | ✅ 已完成（commit `0ba0e1a`） |
| 回写设计规范 `02-wink-micro-os/02-pal-platform-abstraction.md` §4.1（I2C 双版本兼容说明） | ✅ 已回写 |
| 技术设计归档 `tech-designs/pal-i2c-v6-compatibility.md` | ✅ 已归档 |
| **ESP-IDF v6.0.1 真机验证（DevKitC 空总线扫描）** | ✅ 已验证（2026-06-27） |
| SDA/SCL 引脚可配置（Phase 3） | ⏳ 推迟 |
| 公开 `pal_i2c_deinit()` API（Phase 5） | ⏳ 推迟 |
| v7.0 发布前更新本 ADR + 技术设计，移除 v7 `#error` 前向保护 | ⏳ 待 v7 |

---

*本 ADR 状态变更请在此记录：*
- 2026-06-27：Accepted（代码已实现并验证编译；编译期门控 + 设备句柄缓存 + 精细错误码 + 强制回退开关 + v7 前向保护全部落地；技术设计归档至 `tech-designs/`，PAL 规范已回写）。
- 2026-06-27：**Hardware Verified**（ESP32 DevKitC 真机 v6.0.1 空总线扫描 3 地址 NACK 正确，驱动无 panic；详见 `reviews/2026-06-27-devkitc-smoke-hardware-verification.md`）。

