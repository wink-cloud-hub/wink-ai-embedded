# ADR-0069：电热小家电垂直领域 NTC 提升为独立 Type 与安规契约裁决

| 项 | 内容 |
|---|---|
| **状态** | **Accepted（已采纳）** |
| **日期** | 2026-08-26 |
| **触发** | 电热类小家电仿真与固件体系评审：NTC 热敏电阻作为电热水壶、电饭煲、电熨斗、空气炸锅中出货量百亿级的核心外设，现行 SSOT v2.4.0 将其暂列为 `analog_sensor` 变体（仅输出 mV，计算推至 BAL）存在三项严重阻碍：① 开发者体验割裂（业务层强需摄氏度而非无意义的电压）；② 3C / IEC 60335 强制安规故障原语（探头开路断线、探头短路）在通用电压驱动中无法承载；③ 8位 51 单片机急需在驱动内部封装零浮点定点查表插值。 |
| **影响范围** | `00.1-category-type-variant-wokwi-ssot.md`（第 14 行与 §1.1 变体用例回写）；`00-master-execution-plan.md`（P0 表格 #5.1）；`wink-micro-os/dal/{include,src}/sensor/dal_ntc.{h,c}`；`wink-micro-os/codegen/drivers/ntc.yaml`；测试文件 `test_dal_ntc.c`；`18-p0-sensor-ntc-temperature-plan.md` |
| **决策者** | 项目 Owner（确认采纳：NTC 提升为独立 Type，严格遵照平台错误码与 PAL 资源生命周期规范） |
| **关联 ADR** | [ADR-0056](0056-cross-profile-quantity-ab-class-and-scaled-integers.md)（B 类传感器测量量纲与 0.1°C `_ddegc` 映射）；[ADR-0001](0001-error-code-sign-convention.md)（错误码规范）；[ADR-0004](0004-static-dispatch-vs-runtime-ops.md)（POD + 静态分发）；[ADR-0024](0024-fault-three-phase-model-and-dal-deinit-contract.md)（deinit 资源释放契约）；[ADR-0046](0046-dal-driver-registry-ssot.md)（Codegen Schema 1.1 YAML SSOT）；[ADR-0057](0057-pal-adc-subsystem-and-channel-3-analog-contract.md)（PAL ADC 资源治理） |
| **关联计划** | [`18-p0-sensor-ntc-temperature-plan.md`](../../implementation-plans/wokwi-dal-type-coverage-type/18-p0-sensor-ntc-temperature-plan.md) |

---

## 1. 背景（Context）

在 SSOT v2.4.0 的早期规划中，所有单引脚模拟输入设备（NTC、光敏电阻、MQ2 烟雾、火焰传感器）被统一归并在 `category=sensor, type=analog_sensor` 下，API 仅返回原始采样码值（`_raw`）与毫伏电压（`_mv`），并将物理量换算推迟至 BAL 业务抽象层。

然而在电热类小家电（Thermal Appliances）垂直落地与专家架构评审中，暴露出该归类的不可调和矛盾：

1. **领域驱动设计（DDD）与开发者体验（DX）的倒退**：
   家电工程师在开发电水壶、电饭煲、电熨斗时，核心物理控制量是**温度（摄氏度）**。强制 DAL 仅输出电压、要求每个产品在应用层或 BAL 层重复胶水绑定 NTC 换算，严重违背高内聚原则。这与 `input/analog_knob`（底层同样是分压读 ADC，但凭借人机调参一等公民地位独立建 Type 返回归一化比例）和 `sensor/load_cell`（称重核心外设独立建 Type）的架构先例冲突。
2. **IEC 60335 / 3C 家电强制安规原语缺失**：
   电热设备必须具备传感器失效安全防护（防干烧起火）：探头引线脱落（开路）或绝缘破损（短路）必须被底层驱动**确切识别并锁存**。通用 `analog_sensor` 只能输出 0V 或 3.3V，驱动层根本不理解这是安规故障，把致命安全责任完全推给上层是危险的。
3. **8位 51 单片机极限尺寸与零浮点诉求**：
   市售 90% 的低成本小家电采用 8 位 MCU（1T 8051 / 晟矽微 / 中微）。若物理量换算留在通用 BAL 浮点库中，会强行链接 2KB~4KB 的软浮点模拟库，把只有几 KB 的 Flash 撑爆。NTC 必须在驱动内部原生提供 **16位定点整数查表（LUT）插值**，做到零浮点消耗。

---

## 2. 决策（Decision）

### D1. 提升 NTC 为顶级独立 Type (`sensor/ntc`) 并回写 SSOT
- **决策**：正式批准在 `category = sensor` 下独立新建 **`type = ntc`**。
- **架构法理（Boundary A 重释）**：
  按照 SSOT §1.1 边界 A 铁律：*“若控制物理量单位改变或核心 C API 签名遭到破坏，必须新建 Type”*。
  NTC 的核心输出物理量是**温度（`°C` / `0.1°C`）**，而非通用模拟传感器的**电压（`mV`）**。以此为分水岭，NTC 独立建 Type 具备完全的架构自洽性。
- **SSOT 回写约定**：
  - SSOT v2.4.0 映射大表第 14 行从 `analog_sensor / ntc_temperature` 变更为独立行：
    `sensor` $\to$ `ntc`，变体为 `single_ended_adc`（默认单端电阻分压）与 `differential_bridge`（预留差分电桥）；
  - 原 `analog_sensor` 保留光敏电阻、MQ2 烟雾 AO、火焰 AO 等通用模拟量传感器。

---

### D2. 错误码对齐平台规范，禁止私有全局错误码
- **绝不破坏 `wink_status.h` 契约**：
  严禁使用未在枚举中定义的 `WINK_ERR_HW_FAULT` 或 `WINK_ERR_NTC_*`。
- **错误码与状态标志解耦标准**：
  - 当检测到探头短路或探头开路断线时，API 统一返回平台标准错误码：**`WINK_ERR_HARDWARE = -12`**；
  - 当检测到测得温度超出设备物理极值区间（如 $T < -30^\circ\text{C}$ 或 $T > 280^\circ\text{C}$）时，返回 **`WINK_ERR_OUT_OF_RANGE = -4`**；
  - 在 `dal_ntc_t` 句柄中提供细分安规状态字段：
    - `bool fault_open;`（开路故障锁存标志）
    - `bool fault_short;`（短路故障锁存标志）
  - 提供显式故障查询与清除 API：`dal_ntc_clear_faults(dal_ntc_t *dev)`。

---

### D3. 严格落实 PAL ADC 资源生命周期治理
- 严禁 handle 裸读 `dev->adc_channel`。严格遵循 `dal_analog_knob` 的黄金范式：
  1. **`dal_ntc_init`**：
     * 调用 `pal_adc_acquire((wink_pin_t)cfg->ao_pin, &pal_cfg, &ch)` 获取通道并配置默认全量程；
     * 执行双重资源 claim：`pal_resource_claim(PAL_RESOURCE_ADC_CHANNEL, ch, cfg->owner)` 与 `pal_resource_claim(PAL_RESOURCE_GPIO_PIN, cfg->ao_pin, cfg->owner)`；
     * 任何一步失败必须逐级释放资源回滚（Rollback）。
  2. **`dal_ntc_deinit`**：
     * 反查通道：`pal_adc_pin_channel(dev->config.ao_pin, &ch)`；
     * 释放通道：`pal_adc_release(ch)`；
     * 释放资源记录：双重 `pal_resource_release`。
  3. **引脚哨兵**：
     * 必填引脚 `ao_pin` 类型为 `uint16_t`（DAL-S-006）；
     * 可选差分引脚 `diff_neg_pin` 类型为 `wink_pin_t`（`int16_t`），未接必须使用官方常量 **`WINK_PIN_NC`**（非裸 `-1`）。

---

### D4. 跨 Profile 量纲对齐 ADR-0056：`temp_ddegc` 与 `temp_degc`
- **Micro Profile（8051 / 8位）**：
  * API：`dal_ntc_read_ddegc(dal_ntc_t *dev, int16_t *out_ddegc)`；
  * 量纲后缀：`_ddegc`（$1\,\text{LSB} = 0.1^\circ\text{C}$，十分之一摄氏度），$25.4^\circ\text{C} = 254$；
  * 采用 33 点整数查表 + 16位线性插值，耗时 $< 20\mu\text{s}$，零软浮点库引入。
- **Full Profile（ESP32 / Wasm / PC）**：
  * API：`dal_ntc_read_degc(dal_ntc_t *dev, float *out_degc)`；
  * 使用 B 参数经典方程，高精度输出标准浮点物理量。
- **构建系统裁剪门禁**：
  在头文件与源文件中，浮点代码段使用 `#if !defined(WINK_NO_FLOAT) && !defined(WINK_PROFILE_MICRO)` 守卫；在构建系统中正式注入 Profile 宏，保证 8 位目标翻译单元内零 `float`、零 `math.h` 符号引用。

---

### D5. 硬件分压自适应安规与强电 EMI 滤波去抖
- **mV 域极性自适应判定**：
  不依赖 ADC 位宽（禁止硬编码 12 位 4065），统一使用采样电压 $V_{sig}$（mV）与参考电压 $V_{ref}$：
  * `PULL_UP` 拓扑（温升电压降）：
    * $V_{sig} < V_{short\_thresh}$（默认 30mV） $\implies$ **探头短路**（`fault_short = true`）；
    * $V_{sig} > V_{ref} - V_{open\_thresh}$（默认 30mV） $\implies$ **探头开路**（`fault_open = true`）；
  * `PULL_DOWN` 拓扑（温升电压升）：极性自动反转判定。
- **强电 EMI 采样去抖滤波**：
  针对加热盘交流 PWM 与继电器跳火产生的突变尖峰，引入连续 $N$ 次（默认 3 次）超阈值防抖窗口，避免单次瞬态毛刺引发安规停机误报。

---

### D6. Codegen YAML 100% 对齐 Schema 1.1
- 抛弃不合规的手写草案，完全对齐 `load_cell.yaml` 标准格式：
  * `codegen_schema: "1.1"`
  * `quantity_class: sensor_measurement`
  * `fields` 采用 Map 字典形式，变体采用 `fields.variant.enum` 配合 `map:` 映射 C 枚举常量；
  * 提供 `role_bindings`（`temperature_sensor`）规范动词模板。

---

### D7. 差分变体阶段性收敛
- 鉴于当前 PAL ADC 仅提供单端通道 API，暂无 PGA 差分硬件抽象：
  * `DAL_NTC_VARIANT_DIFFERENTIAL_BRIDGE` 枚举保留用于 ABI 占位，但在初始化阶段显式校验并返回 **`WINK_ERR_UNSUPPORTED`**；
  * Phase 1 集中资源把单端 `single_ended_adc`（覆盖 99% 小家电）做到极致坚固。

---

## 3. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| **小家电垂直生态开箱即用**：应用层一行直读摄氏度（`_ddegc` / `_degc`），彻底消灭模板重复代码。 | 需要正式维护 `sensor/ntc` 独立驱动与 Codegen 元数据（Type 计数由 28 增至 29）。 |
| **安规底线加固**：底层原生拦截探头开路与短路，为 3C / IEC 60335 认证提供权威故障原语。 | 驱动层需维护连续采样的去抖窗口与锁存状态，内存增加几个字节。 |
| **51 单片机零浮点保障**：16 位定点查表使 8 位机 CPU 占用率 $<0.06\%$，固件体积减少 3KB+。 | 开发者必须为特定 NTC 材料准备 33 点定点标定表（或使用离线生成脚本）。 |
| **资源冲突防御**：严格的 PAL ADC claim/release 杜绝了两个外设抢占同一 ADC 引脚的隐患。 | 驱动必须严格依赖 PAL ADC 子系统，初始化需多步回滚防护。 |
