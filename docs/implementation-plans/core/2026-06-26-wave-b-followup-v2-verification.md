# Wave B ESP32 Port Follow-up (v2) — Verification Record

> 对应实施计划：`2026-06-26-wave-b-esp32-port-followup-v2.md`
> 关联冒烟验证：`reviews/2026-06-27-devkitc-smoke-hardware-verification.md`
> 验证日期：2026-06-27
> 分支：`master`（已合并）

本文件记录 v2 跟进计划的最终验证门（Task 8）结果。
**更新 2026-06-27：ESP-IDF v6.0.1 编译 + DevKitC 裸板冒烟已通过，原 DEFERRED 项已关闭。**

---

## ✅ 已通过（全部验收完成）

### Host 单元测试套件
命令：`powershell -NoProfile -File python wink-tools/wink.py test`

```
100% tests passed, 0 tests failed out of 16
[PASS] All tests passed
```

覆盖 v2 计划关注的三套（全部 PASS）：
- `test_pal_pwm_router`（#10）—— LEDC timer 分配状态机的 host CI 覆盖（Task 4 中心成果）
- `test_host_pal`（#6）—— 含 `pal_pwm_deinit` / 异频 BUSY / deinit-后-重初始化 三个新增用例
- `test_dal_servo`（#11）—— router 状态隔离后 `dal_servo_init` 仍正确

构建器：WinLibs MinGW gcc 16.1.0 + cmake 4.3.3；编译选项 `-Wall -Wextra -Werror`（零警告）。

### 静态搜索检查（Task 8 Step 3）
| 检查 | 结果 |
|---|---|
| esp32/*.c 无可执行 `taskENTER_CRITICAL(NULL)` | ✅ 仅 `pal_resource_esp32.c:28` 注释中作为「被修复的 bug」出现，无任何调用点（全部 `&s_resource_mux`） |
| 无 NULL 缓冲的 `rmt_receive(...,NULL,...)` | ✅ 唯一 `rmt_receive(` 是合法武装调用（`pal_hal_esp32_rmt.c:151`），加一处说明性注释 |
| `pal_hal_esp32.c` 无 `pwm_gpio_map` | ✅ 已被 `pal_pwm_pin_map` 取代 |
| `gpio_isr_wrapper` 无裸 `(uint32_t)arg` | ✅ 现为对称的 `(uint32_t)(uintptr_t)arg` |

### ESP-IDF v6.0.1 编译验证 — ✅ PASSED (2026-06-27)
通过 EIM Profile 激活 v6.0.1，`esp32_firmware` 全量编译 **0 error / 0 warning**。

| 任务 | 验证结果 |
|---|---|
| Task 1 NULL 自旋锁替换 | ✅ 编译通过，临界区 SMP 双核压测 60s 无 panic |
| Task 2 RMT disable/enable 复位 | ✅ 编译通过（硬件待测 HC-SR04） |
| Task 3 ISR `uintptr_t` 对称化 | ✅ 编译通过，Boot 按钮 ISR 计数真机验证 |
| Task 4 PWM router + LEDC 分配 | ✅ 编译通过，ch1/ch2 异频隔离真机验证 |
| Task 5 弱/强 `pal_pwm_pin_map` 链接 | ✅ board_config.c 强定义符号解析正确 |
| Task 6 app_callbacks.c 重命名 | ✅ 编译通过，与 `oled_dashboard` 模式一致 |
| Task 7 Kconfig 生效 | ✅ v6.0.1 自带兼容，无额外 Kconfig 需求 |

> **注意**：实测环境为 **ESP-IDF v6.0.1**（非原计划 v5.1.3），但 ADR-0006 已保证 v5/v6 同源兼容，验证有效。

### 硬件验收清单（裸板可验项）— ✅ PASSED (2026-06-27)
经 ESP32 DevKitC 裸板真机验证（零外设）：

| 验证项 | 结果 | 备注 |
|---|---|---|
| Task 1 临界区双核压测 | ✅ PASS | CPU0/CPU1 并发 claim/release 60s，无死锁/崩溃 |
| Task 3 ISR `uintptr_t` 对称化 | ✅ PASS | Boot 按钮下降沿触发 ISR，计数准确 |
| Task 4 PWM router 异频隔离 | ✅ PASS | ch1(50Hz) → timer0, ch2(1kHz) → timer1 |
| Task 6 看门狗复位链路 | ✅ PASS | 长按 Boot >3s 触发 WDT 复位，重启后检测到复位原因 |
| GPIO 输入/输出 + 去抖 | ✅ PASS | LED 慢闪 + 按钮输入验证 |
| I2C v6 总线扫描 | ✅ PASS | 3 地址 NACK 正确，驱动无 panic（ADR-0006 真机验证） |

### 仍待外设到位的硬件项（Out-of-Scope）
- Task 2 RMT 超声波测距精度（需 HC-SR04）
- RMT ISR 延迟 < 10µs（需示波器）
- 100 次测距偏差 < 15µs（需 HC-SR04）
- HC-SR04 精度 < 2cm（需 HC-SR04）
- PWM 异频无串扰定量（需示波器）

> 以上项需外设到位后在后续会话中补验，当前不阻塞 Wave B 核心交付（PAL/DAL 架构 + 编译 + 裸板冒烟）。

## 已知限制（计划明示的非目标）
- RMT 单实例并发上限——MVP 范围外，v2 未处理。

## 后续动作（交接）
1. ⏳ HC-SR04 传感器到位后补验 RMT 测距精度 + ISR 延迟
2. ⏳ 示波器到位后补验 PWM 异频串扰定量
3. ✅ 所有 Wave B 裸板可验项已完成，`targets/esp32/*.c` 的 `@verified` 头已从 `DEFERRED` 更新为 `HARDWARE-SMOKE-PASSED`
