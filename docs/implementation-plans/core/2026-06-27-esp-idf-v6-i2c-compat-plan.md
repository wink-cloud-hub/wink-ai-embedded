# ESP-IDF v6.x I2C 驱动兼容实施计划

| 项 | 内容 |
|----|------|
| **计划创建日期** | 2026-06-27 |
| **计划状态** | ✅ 已执行 |
| **关联 Wave** | Wave B 后续 |
| **关联技术设计** | [`../../tech-designs/core/pal-i2c-v6-compatibility.md`](../../tech-designs/core/pal-i2c-v6-compatibility.md) |
| **验证操作手册** | 见本文 **附录 A**（原独立 verification-guide 已按 ③ 层规范并入） |
| **关联设计规范** | `02-wink-micro-os/02-pal-platform-abstraction.md` |
| **目标版本** | ESP-IDF v5.1.3 LTS + v6.0.x/v6.1 |

---

## 1. 背景与目标

### 1.1 问题陈述

ESP-IDF v6.0 对 I2C 驱动进行了不兼容的架构重构：
- 旧 API (`driver/i2c.h`) 已标记为 **EOL（生命周期终止）**
- 新 API (`driver/i2c_master.h`) 采用 **总线-设备 二级句柄模型**
- v7.0 计划**完全移除**旧 API

wink-micro-os 的 ESP32 PAL 层当前仅使用 v5.x 旧 API，需要升级以支持 v6.x 并为 v7.0 做准备。

### 1.2 目标

✅ **双版本兼容**：同一份代码在 v5.1.3 和 v6.0.x/v6.1 下均可编译运行
✅ **零上层变更**：PAL 公开 API `pal_i2c_transfer()` 保持不变
✅ **零警告**：v6.x 下无 deprecation warning
✅ **可验证**：包含完整的验证清单

---

## 2. 变更范围

### 2.1 文件清单

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `wink-micro-os/targets/esp32/CMakeLists.txt` | 修改 | 添加 `esp_driver_i2c` 组件依赖 |
| `wink-micro-os/targets/esp32/pal_hal_esp32.c` | 修改 | I2C 部分重写，添加版本门控 |

### 2.2 接口影响

- **PAL 公开 API**：无变更（`pal_i2c_transfer()` 签名保持不变）
- **DAL 层**：无感知
- **Application 层**：无感知

---

## 3. 详细任务拆分

### Task 1：CMakeLists.txt 组件依赖更新

**预估工作量**：5 分钟

**步骤**：
- [ ] 在 `REQUIRES` 列表中添加 `esp_driver_i2c`
- [ ] 保留 `driver` 依赖（GPIO/RMT/LEDC 仍在使用）

**验证**：
- [ ] v5.1.3 下 `idf.py build` 无错误
- [ ] v6.0.x 下 `idf.py build` 无错误

---

### Task 2：pal_hal_esp32.c I2C 重写

**预估工作量**：30 分钟

**步骤**：
- [ ] 添加 `#include "esp_idf_version.h"`
- [ ] 添加版本条件编译的头文件包含：
  - v6.x: `#include "driver/i2c_master.h"`
  - v5.x: `#include "driver/i2c.h"`
- [ ] 添加 v6.x 专用的静态变量：
  - `s_i2c_bus[I2C_PORTS]` 总线句柄数组
  - `s_i2c_dev_cache[port][index]` 设备句柄缓存（4 个设备）
- [ ] 实现 `pal_i2c_get_or_create_device()` 静态辅助函数
- [ ] 重写 `pal_i2c_transfer()` 初始化分支：
  - v6.x: 使用 `i2c_new_master_bus()`
  - v5.x: 保留现有 `i2c_param_config() + i2c_driver_install()`
- [ ] 重写 `pal_i2c_transfer()` 传输分支：
  - v6.x: 使用 `i2c_master_transmit_receive()`
  - v5.x: 保留现有 `i2c_master_write_read_device()`

**代码关键点**（详见技术设计文档 §3-§6）：
- 使用 `ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)` 做版本门控
- 设备句柄懒加载 + FIFO 替换策略（4 slot 缓存，见 §3.2）
- 错误码精细映射（7 种 ESP err → 6 种 wink_status，见 §4.2）
- v7.0 前向保护编译断言（见 §2.2）
- 并发安全：静态互斥锁保护初始化与缓存操作（见 §6.2）

**Task 2 子任务拆分**：
- 2.1：添加版本门控头文件包含 + v7.0 前向保护
- 2.2：实现静态数据结构（总线句柄数组 + 设备缓存）
- 2.3：实现 `pal_i2c_get_or_create_device()` 懒加载函数
- 2.4：实现 `pal_i2c_map_esp_err()` 精细错误码映射
- 2.5：重写初始化分支（v6.x `i2c_new_master_bus()` 路径）
- 2.6：重写传输分支（`transmit`/`receive`/`transmit_receive` 三分支）
- 2.7：添加静态互斥锁，保护初始化与缓存操作的并发安全

---

### Task 3：编译验证

**预估工作量**：15 分钟 / 每个环境

#### v5.1.3 环境验证：
- [ ] 执行 `idf.py -C esp32_firmware fullclean`
- [ ] 执行 `idf.py -C esp32_firmware build`
- [ ] 确认 0 error, 0 warning
- [ ] 确认无 I2C 相关 deprecation warning
- [ ] 验证回滚开关：`CONFIG_WINK_I2C_FORCE_V5_API=y` 下编译正常

#### v6.0.x/v6.1 环境验证：
- [ ] 执行 `idf.py -C esp32_firmware fullclean`
- [ ] 执行 `idf.py -C esp32_firmware build`
- [ ] 确认 0 error, 0 warning
- [ ] 确认无 I2C 相关 deprecation warning
- [ ] 验证 v7.0 保护断言：手动改版本号到 7.0，确认编译报错

---

### Task 4：主机单元测试

**预估工作量**：10 分钟

**步骤**：
- [ ] 执行 `powershell -NoProfile -File python wink-tools/wink.py test`
- [ ] 确认 16/16 全部通过
- [ ] 特别确认 `test_host_pal` I2C 相关测试（如有）

---

### Task 5：真机验证

**预估工作量**：30 分钟

**硬件要求**：
- ESP32 DevKit
- SSD1306 OLED 屏幕（I2C 接口）

**验证步骤**：
- [ ] 烧录 OLED Dashboard 示例
- [ ] 确认屏幕正常显示（证明 I2C 写操作正常）
- [ ] 如果有 I2C 传感器，确认读取操作正常

---

## 4. 验收标准（Go/No-Go）

| 验收项 | 通过标准 |
|--------|---------|
| v5.1.3 编译 | ✅ 0 error, 0 warning |
| v6.0.x/v6.1 编译 | ✅ 0 error, 0 warning |
| Host 单元测试 | ✅ 16/16 全部通过 |
| OLED 真机显示 | ✅ 屏幕显示正常 |
| PAL API 兼容性 | ✅ 无任何上层代码变更 |

---

## 5. 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| v6.x I2C 驱动存在已知 bug | 🟡 中 | 🟠 中 | 编译验证时关注 ESP-IDF GitHub issues，必要时锁定小版本 |
| 设备缓存溢出（>4 个 I2C 设备） | 🟢 低 | 🟡 中 | MVP 阶段 4 个设备足够，后续可扩展为动态分配 |
| v7.0 API 再次变更 | 🟡 中 | 🟡 中 | 提前关注 v7.0 release notes，及时适配 |

---

## 6. 后续动作

- [x] 创建 `tech-designs/pal-i2c-v6-compatibility.md` 归档技术设计（已迁移自原 `02-wink-micro-os/03-pal-i2c-v6-compat-design.md`）
- [x] 更新 `02-wink-micro-os/02-pal-platform-abstraction.md` 添加 I2C 版本兼容说明（见 §4.1）
- [x] 补充 ADR 记录 → [ADR-0006](../../decisions/core/0006-esp-idf-v6-i2c-compatibility.md)
- [x] 验证操作指南并入本计划附录 A（原 `-verification-guide.md` 已删除）
- [ ] 创建验证报告 `2026-06-xx-esp-idf-v6-i2c-compat-verification.md`（④ 评审层，待真机验证后归档）

---

## 7. 参考资料

- ESP-IDF v5.2 迁移指南（I2C 部分）：`docs/zh_CN/migration-guides/release-5.x/5.2/peripherals.rst`
- ESP-IDF v6.0 迁移指南：`docs/zh_CN/migration-guides/release-6.x/6.0/peripherals.rst`
- Wave B ESP32 Port Follow-up v2 计划：`2026-06-26-wave-b-esp32-port-followup-v2.md`

---

## 附录 A：验证操作手册

> 原独立文件 `2026-06-27-esp-idf-v6-i2c-compat-verification-guide.md`，按五层文档体系规范（[docs-adr §1.1](../../../../.claude/rules/docs-adr.md)，③ 实施计划层命名 `-plan.md`）并入本计划作为验证执行细则。关联 [ADR-0006](../../decisions/core/0006-esp-idf-v6-i2c-compatibility.md)、技术设计 [`../../tech-designs/core/pal-i2c-v6-compatibility.md`](../../tech-designs/core/pal-i2c-v6-compatibility.md)。

### A.1 环境准备

#### 激活 ESP-IDF 环境（Windows PowerShell）

```powershell
# v6.0.1 环境
. "D:\software\embedded\esp\v6.0.1\esp-idf\export.ps1"

# 验证版本
idf.py --version
# 预期输出应该包含 ESP-IDF v6.0.1

# 验证编译器
xtensa-esp32-elf-gcc --version
```

> 💡 **提示**：如果 export.ps1 报 Python venv 错误，请先运行 ESP-IDF 安装脚本：
> ```powershell
> cd D:\software\embedded\esp\v6.0.1\esp-idf
> .\install.ps1
> ```

### A.2 编译验证

#### A.2.1 v6.0.x 环境验证（主要目标）

```powershell
# 进入项目根目录
cd D:\workspaces\ai-coding\wink-ai\wink-ai-embedded

# 进入固件目录
cd esp32_firmware

# 清理旧构建
idf.py fullclean

# 执行构建
idf.py build 2>&1 | Tee-Object -FilePath build.log
```

**✅ 通过标准：**
- ✅ `0 error`
- ✅ `0 warning`
- ✅ **无 I2C deprecation warning**（不会出现 `driver/i2c.h is deprecated` 字样）

检查是否使用了新的 v6.x API：
```powershell
Select-String -Path "build.log" -Pattern "i2c_master"
# 预期：不会出现任何与 i2c_master.h 相关的警告（因为是正确使用）
```

#### A.2.2 v5.1.3 环境验证（向后兼容）

如果机器上有 v5.1.3 环境：

```powershell
# 激活 v5.1.3 环境（路径根据实际安装调整）
. "D:\software\embedded\idf-v5.1.3\export.ps1"

# 清理并重新构建
idf.py fullclean
idf.py build 2>&1 | Tee-Object -FilePath build-v5.log
```

**✅ 通过标准：**
- ✅ `0 error`
- ✅ `0 warning`
- ✅ 可以正常使用旧 API

#### A.2.3 强制回退开关验证（应急方案）

验证 `CONFIG_WINK_I2C_FORCE_V5_API` 开关是否生效：

```powershell
# 在 sdkconfig 或组件配置中启用强制回退
# 或临时修改 CMakeLists.txt 添加编译定义
echo 'CONFIG_WINK_I2C_FORCE_V5_API=y' >> sdkconfig

# 重新构建
idf.py rebuild
```

验证编译输出：
```powershell
Select-String -Path "build.log" -Pattern "forced to use v5.x"
# 预期：找到 "Wink I2C: forced to use v5.x compatible API per Kconfig"
```

### A.3 真机功能验证

#### A.3.1 硬件准备

- ESP32 DevKitC 或同等开发板
- SSD1306 128x64 OLED 屏幕（I2C 接口）
- 接线：
  - SDA → GPIO 21
  - SCL → GPIO 22
  - VCC → 3.3V
  - GND → GND

#### A.3.2 烧录并运行 OLED 示例

```powershell
# 烧录固件 + 监视串口
idf.py -p COM3 flash monitor
# （将 COM3 替换为实际串口号）
```

**✅ 通过标准：**
- ✅ OLED 屏幕正常点亮
- ✅ 显示 Dashboard 内容（速度、距离、舵机角度等）
- ✅ 无 I2C 传输超时错误日志
- ✅ 连续运行 5 分钟无死机/复位

#### A.3.3 多设备场景验证（可选）

如果有多个 I2C 设备（传感器 + OLED）：
- 连接 2-4 个 I2C 设备到同一总线
- 验证所有设备都能正常通信，不会触发设备缓存溢出警告

### A.4 性能基准对比（可选，进阶验证）

#### A.4.1 启用性能日志

在 `pal_hal_esp32.c` 中取消注释性能测量代码，或添加：
```c
#define I2C_PERF_BENCHMARK 1
```

#### A.4.2 运行对比测试

在 v5.x 和 v6.x 环境下分别：
1. 测量单次 I2C 写入延迟（1 字节）
2. 测量整页 OLED 刷新延迟（128 字节）
3. 测量空函数调用 overhead

**✅ 通过标准：**
- ✅ v6.x 性能退化 ≤ 10%（相对于 v5.x）

### A.5 常见问题排查

#### A.5.1 编译错误：`driver/i2c_master.h: No such file or directory`

**原因**：ESP-IDF 版本 < v5.2，或者组件依赖未正确配置

**解决**：
```powershell
# 验证 ESP-IDF 版本
python -c "import idf_version; print(idf_version.__version__)"

# 检查 CMakeLists.txt 是否包含 esp_driver_i2c
Select-String -Path "../wink-micro-os/targets/esp32/CMakeLists.txt" -Pattern "esp_driver_i2c"
```

#### A.5.2 运行时错误：`I2C mutex timeout`

**原因**：高并发场景下互斥锁竞争

**解决**：
- 检查是否有任务频繁调用 `pal_i2c_transfer`
- 考虑增大超时值 `I2C_TRANSFER_TIMEOUT_MS`

#### A.5.3 运行时警告：`device cache full, evicting addr 0xXX`

**原因**：同一总线上超过 4 个 I2C 设备

**解决**：
- 性能可接受的话可忽略
- 如需更多设备，修改 `I2C_MAX_DEVICES` 宏定义（默认 4）

#### A.5.4 OLED 不显示但无报错

**原因**：I2C 地址不对或接线错误

**解决**：
```powershell
# 在 monitor 中启用 debug 日志
# 设置 ESP_LOG_LEVEL=DEBUG
# 查看 I2C 设备地址探测结果
```

### A.6 验证完成清单

执行完所有验证后，请在此处勾选：

| 验证项 | v5.1.3 | v6.0.1 | 日期 | 验证人 |
|--------|--------|--------|------|--------|
| 编译 0 error | ⏸ 延期¹ | ✅ | 2026-06-27 | Claude Code |
| 编译 0 warning | ⏸ 延期¹ | ✅ | 2026-06-27 | Claude Code |
| 无 I2C deprecation | N/A | ✅ | 2026-06-27 | Claude Code |
| Host 单元测试 16/16 | N/A² | ✅ | 2026-06-27 | Claude Code |
| OLED 正常显示 | ⏸ 需真机 | ⏸ 需真机 | | |
| 连续运行 5 分钟稳定 | ⏸ 需真机 | ⏸ 需真机 | | |
| 强制回退开关有效 | [ ] | [ ] | | |

> **v6.0.1 编译验证结论**（2026-06-27，EIM 安装 + `PYTHONUTF8=1` 激活）：`idf.py -C esp32_firmware build` 成功，exit 0，**0 error / 0 warning / 无 I2C deprecation**；产物 `wink_esp32_firmware.bin`（0x2b1b0 B，分区剩 83%）。完整日志 `build-v6.log`。
> ¹ 本机仅装 v6.0.1（EIM），无可用的 v5.1.x 框架；v5 旧 API（`driver/i2c.h`）路径按 `ESP_IDF_VERSION` 门控编译有效，但本地未验证。
> ² Host 单测（`TARGET_PLATFORM=host`）与 IDF 版本无关，16/16 通过。

**最终签名**

验证负责人：Claude Code（自动编译验证；真机项待人工会签）
日期：2026-06-27

### A.7 回滚方案

如 v6.x 适配出现问题，可通过以下任一方式回退：

1. **快速回退**：在 `sdkconfig` 中设置 `CONFIG_WINK_I2C_FORCE_V5_API=y`
2. **版本回退**：`git revert 0ba0e1a`（撤销本次提交）

