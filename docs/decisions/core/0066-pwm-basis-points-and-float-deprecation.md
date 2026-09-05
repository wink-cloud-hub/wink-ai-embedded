# ADR-0066：PWM Basis Points (bp) 定点规范与浮点接口下线路线

| 项 | 内容 |
|---|---|
|状态 | **Accepted（已通过）** |
| 日期 | 2026-08-24 |
| 影响范围 | `pal_pwm.h`、`pal_hal_pwm_esp32.c`、`pal_hal_pwm_host.c`、`pal_wasm_ch1b_pwm.c`、`dal_buzzer.c`、`dal_rc_servo.c`、`dal_dc_motor.c` |
| 决策者 | 架构委员会 / 核心 PAL 维护组 |
| 关联 ADR | [ADR-0004](0004-static-dispatch-vs-runtime-ops.md)（静态分发）；[ADR-0012](0012-contract-honesty-over-silent-degradation.md)（合约诚实）；[ADR-0056](0056-cross-profile-quantity-ab-class-and-scaled-integers.md)（定点数与量纲规范）。 |

---

## 背景（Context）

1. **浮点库膨胀问题**：原 `pal_pwm_set_duty(uint8_t channel, float duty)` 接收 `0.0f..1.0f`。在 Cortex-M0/M3、RISC-V 32EC 等无硬件单精度 FPU 的微控制器上，链接器只要扫描到一次浮点运算符号，就会被迫拉入 GCC/Clang 的软件浮点模拟库（`soft-fp`，如 `__aeabi_f2iz`、`__aeabi_fdiv`、`__aeabi_fmul`），导致 Flash 固件代码直接膨胀 2~4 KB，并在中断或硬实时控制环路中消耗额外的数十个 CPU 周期。
2. **概念与命名错误纠正**：占空比以 `0.01%` 为步进精度，全量程 `0..10000`（对应 `0.00%..100.00%`），标准数学与金融量纲术语为 **Basis Points (bp, 万分比 / 基点)**，而非 `permille`（千分比，0..1000）。
3. **高位计算溢出与精度截断**：ESP32-S3 等芯片支持最高 20-bit 定时器（top = 1,048,575）。在 32 位无符号整数下，`1,048,575 * 10,000 = 10,485,750,000` 超过 `UINT32_MAX (4.29G)`，会导致数值溢出反转；同时若无四舍五入，低分辨率下会丢失精度。

---

## 决策（Decision）

1. **全面引入 Basis Points 定点接口**：
   ```c
   WINK_WARN_UNUSED_RESULT
   wink_status_t pal_pwm_set_duty_bp(uint8_t channel, uint16_t basis_points);
   ```
   - 范围严格为 `0..10000`（0 代表 0.00%，5000 代表 50.00%，10000 代表 100.00%）；
   - 超出 10000 立即返回 `WINK_ERR_INVALID_ARG`。

2. **硬件底层计算算法规范（防溢出 + 四舍五入）**：
   各 Target 实现必须采用如下通用四舍五入防溢出算法（使用 `uint64_t` 中间积）：
   ```c
   static inline uint32_t pal_pwm_calc_duty_counter(uint16_t bp, uint32_t top)
   {
       if (bp == 0u) { return 0u; }
       if (bp >= 10000u) { return top; }
       uint64_t product = (uint64_t)bp * (uint64_t)top;
       uint32_t count = (uint32_t)((product + 5000ull) / 10000ull);
       return (count > top) ? top : count;
   }
   ```

3. **浮点接口逐步下线路线图**：
   - **过渡期（Phase 1~2）**：`pal_pwm_set_duty(float)` 保留但打上 `WINK_DEPRECATED_MSG`，内部转为调用 `pal_pwm_set_duty_bp`；
   - **收敛期（Phase 3）**：将全量 DAL（buzzer、servo、motor 等 7 处）与 selftest 迁移至 `_bp` 接口；
   - **下线期（v3.0）**：提供 `PAL_PWM_HIDE_FLOAT_API` 编译宏，默认彻底隐藏 float 声明；通过 `arm-none-eabi-nm` 验证固件中 0 软浮点符号。

---

## 影响（Consequences）

- **正向收益**：在无 FPU 架构上彻底释放 2~4 KB Flash 空间，消除中断控制中的浮点周期开销，消除 20-bit PWM 高位溢出风险。
- **迁移要求**：Buzzer 驱动迁移为 `0/5000/10000 bp`，Servo 驱动角度转万分比，Motor 驱动百分比转万分比。
