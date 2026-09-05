# 轴 A 真机 Port 通用加固实施计划（caps 自注入 / PAL_HAS_* / 原子三档 / 静态池）

> **状态：Draft 骨架（独立排期，不阻塞 MCS-51 仿真主线）**
> 本计划从《MCU 兼容方案 v2.1》§4 剥离。8051 仿真是轴 B（host/wasm，见 [MCS-51 仿真计划](2026-08-27-mcs51-zero-code-simulation-plan.md)），与轴 A 零依赖；轴 A 服务于未来 STM32（含 Cortex-M0）真机 port。

## 1. 元数据（待细化）

| 字段 | 内容 |
|------|------|
| 创建日期 | 2026-08-27 |
| 目标平台 | ESP32（回归验证）/ STM32 Cortex-M3+ / Cortex-M0/M0+（未来） |
| 优先级 | 🟡 P1（不阻塞 8051 主线） |
| 关联 ADR | [ADR-0064](../../decisions/core/0064-target-capability-ssot.md)（target capability SSOT）；未来 STM32 port 另立 ADR |
| 背景材料 | [`docs/tech-designs/mcs51/mcu-compat-plan.md`](../../tech-designs/mcs51/mcu-compat-plan.md) §4/§5 |

## 2. 任务范围（源自总方案 §4）

- [ ] **P0.1 caps 自注入（~2 天）**：`pal/include/hal/pal_target_caps.h` 现状为 `ESP_PLATFORM / __wasm__ / else` 硬编码链（L12-31），改为分发各 target 自带头（`targets/<mcu>/pal_target_caps_<mcu>.h` + sim 兜底）。
- [ ] **P0.1 编译期 HAL 子集门控 `PAL_HAS_*`（~含上）**：新增 `PAL_HAS_DMA/MCPWM/PCNT/RMT/HW_FPU/ATOMICS`；ESP32 专属头整组 `#if` 包裹；DAL 裁剪（`cmake/wink_dal_drivers.cmake`）改读 caps；FOC pipeline 门控；无 FPU 时复用 q15/q31 定点（提取公共 `pal_fixedpoint.h`）。
- [ ] **P0.2 `pal_atomic.h` 三档（~1-2 天）**：修现状 `#else → <stdatomic.h>` 真坑（`pal/include/osal/pal_atomic.h` L69-79，非 GCC/Clang/MSVC 即炸）：① 原生原子（Cortex-M3+/ESP32，GCC `__atomic_*`）；② 临界区退化（Cortex-M0/裸机，save/restore IRQ，8/16 位原生、32 位入临界区）；③ 仿真（现有 Win32/pthread）。
- [ ] **P0.2 baremetal ringbuf 静态分配（~1 天）**：`osal/baremetal/pal_osal_baremetal.c` ringbuf 去 malloc，新增 `pal_os_ringbuf_create_static(buf, size)`；有堆 target 保留 `_create`。
- [ ] **（可选，低优）时间 wraparound helper**：`PAL_TIME_AFTER` / `PAL_TIME_ELMSD` 宏，修 `frameworks/arduino/src/Common.cpp:134-140` 截断 unsigned long 71 分钟溢出。

## 3. 验收

- Cortex-M0 最小构建（无原子指令、无 FPU）静态链接通过（MCS-51 总方案验收 #6 由本计划门禁保证）。
- ESP32/host/wasm 现有测试全绿回归。
- `wink lint arch` 全绿。

## 4. 后续

STMicro STM32 真机 port（`targets/stm32/` + osal port，基于 STM32Cube HAL/LL）依赖本计划完成，届时另立技术设计与 ADR（见总方案 §5）。
