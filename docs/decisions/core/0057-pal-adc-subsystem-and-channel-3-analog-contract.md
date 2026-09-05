# ADR-0057：PAL ADC 子系统与通道 3（模拟量）仿真契约

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已通过）** |
| 日期 | 2026-08-05 |
| 触发 | Wokwi 全量外设覆盖计划中 `analog_knob`(#1)/`analog_sensor`(#5)/`heart_rate`(#18) 被底层 PAL ADC 完全缺失阻塞；前置计划 [`00.5-pal-adc-subsystem-plan.md`](../../implementation-plans/frontend/00.5-pal-adc-subsystem-plan.md) 初稿存在与仿真 SSOT 冲突的设计（ESP32 衰减枚举跨平台泄漏、JS 桥另造 mV 路径绕过 PinArbiter、运行时探测 Wi-Fi、role 粒度未定）。本 ADR 在编码前定稿契约。 |
| 影响范围 | 新增 `pal/include/hal/pal_adc.h` + 三 target HAL 实现（esp32/wasm/host；当前仓库无 baremetal HAL target，见决策 5）；`pal_resource.h` 增 ADC 资源；`wasm_bridge.h` + TS `WasmImports` + `PAL_WASM_ABI_HASH`；`codegen/roles/analog_input.yaml`；board 元数据 `wink-tools/tools/codegen/boards/*.json`；仿真文档 `08-channel-routing.md §2.4` 由 Planned 升级；设计规范 `02-wink-micro-os/02-pal-platform-abstraction.md`。 |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0002](../unisim/0002-dual-target-compilation.md)（双 target 同源）；[ADR-0004](0004-static-dispatch-vs-runtime-ops.md)（POD + 静态命名 API，无 vtable）；[ADR-0009](../unisim/0009-physical-behavior-simulation-fault-injection.md)（双域退化 + 确定性 PRNG）；[ADR-0040](0040-device-tree-as-single-source-of-truth.md)（配置 SSOT + Fail-Loud）；[ADR-0056](0056-cross-profile-quantity-ab-class-and-scaled-integers.md)（A/B 量纲分类、封闭后缀、禁弱 typedef）。 |
| 关联计划 | [implementation-plans/2026-08-05-wokwi-dal-type-coverage-plan/00.5-pal-adc-subsystem-plan.md](../../implementation-plans/frontend/00.5-pal-adc-subsystem-plan.md) |

---

## 背景（Context）

通道 3（模拟量）在 [`08-channel-routing.md §2.4`](../../zh/design/04-wasm-simulation/02-mechanisms/08-channel-routing.md) 中长期处于 **Planned**：其架构铁律已写明——保留 DAL 的校准/滤波/阈值/错误码同源，旁路只替换"ADC 原始值来源"，插件注入"绑定到 ADC 通道的物理源"，禁止直接 `return temperature_c`。PinArbiter 也已具备模拟通道能力（`setAnalogDriver`/`readAnalog → [0,1]`，见 [`07-peripheral-registry.md §4.3`](../../zh/design/04-wasm-simulation/02-mechanisms/07-peripheral-registry.md)），退化引擎已有 `adc_noise_v` setter 与 `wink_phys_rc_lowpass`/`wink_phys_warmup_check` 算法。

但 PAL 层至今没有任何 ADC API：`pal_hal.h` 仅有 GPIO/PWM/I2C，`pal_resource_type_t` 无 ADC，`wasm_bridge.h` 无 ADC 导入，`codegen/roles/` 无模拟输入 role。前置计划初稿试图一次性补齐，但引入了四个相互关联、需在编码前定稿的设计分歧：

1. **跨平台抽象形态**：初稿用 ESP32 特有的 `atten(0/2.5/6/11dB)` 枚举作为 PAL 公共配置，host/wasm 无此语义；
2. **Wasm 数据路径**：初稿让 JS 桥把百分比换算成 mV 后 `js_pal_adc_read_mv(pin)` 返回，绕过已有的 PinArbiter 模拟通道 SSOT，并把满量程硬编码进 JS；
3. **ADC2/Wi-Fi 冲突门禁归属**：初稿让 `pal_adc_init()` 运行时"检查系统是否已初始化 Wi-Fi"，造成 PAL 反向依赖网络栈；
4. **Role 粒度与量纲**：初稿让 knob/sensor/heart_rate 共用一个含 `read_promille`+`read_mv` 的 role，三者物理语义与精度需求并不一致。

## 方案比选（Options）

### 决策一：PAL ADC 配置模型

| 方案 | 描述 | 取舍 |
|---|---|---|
| ① ESP32 衰减枚举（初稿） | `pal_adc_atten_t{0/2.5/6/11dB}` 进公共头，所有 target 可见 | **否决**。把单一芯片的输入档概念泄漏进跨平台 PAL（违反 ADR-0002）；wasm/host 要硬塞无意义枚举；衰减本质是 target 选择满量程的实现细节。 |
| ② 目标无关的满量程 + target hint（采纳） | 公共配置只表达 App 可移植关心的量（**满量程 mV、分辨率位、引脚**），衰减/增益作为可选 target hint 由 esp32 实现内部消化 | **采纳**。App 写"我要 0~3300mV、12bit"即可，不绑芯片；esp32 实现据此选最接近的 atten；wasm/host 无 atten 概念。 |

### 决策二：Wasm 模拟数据路径

| 方案 | 描述 | 取舍 |
|---|---|---|
| ① JS 桥返回 mV（初稿） | `js_pal_adc_read_mv(pin) → uint16`，前端把百分比 ×3300 注入 | **否决**。绕过 PinArbiter 模拟 SSOT，产生第二条并行模拟路径；满量程/衰减被硬编码进 JS 桥（JS 不该知道 PAL 满量程）；换算逻辑落在 JS，撕裂同源（raw↔mv 换算本应在 C 被 DAL 复用与测试）。 |
| ② PinArbiter 归一化 + C 侧换算（采纳） | JS 桥只产出 `[0,1]` 归一化物理源 `js_pal_adc_read_norm(pin) → float`（读 `PinArbiter.readAnalog`）；C 侧 `raw = norm × ((1<<bits)-1)`、`mv = norm × full_scale_mv` | **采纳**。PinArbiter 是唯一模拟电气 SSOT；JS 只替换物理量来源（符合通道 3 铁律）；raw/mv 换算在同源 C 路径，DAL/BAL 可复用、可单测；满量程由 PAL per-channel 状态持有，不进 JS。 |

### 决策三：ADC2/Wi-Fi 冲突门禁

| 方案 | 描述 | 取舍 |
|---|---|---|
| ① PAL 运行时探测 Wi-Fi（初稿） | `pal_adc_init()` 检查"系统是否已初始化 Wi-Fi"，是则返回 BUSY | **否决**。PAL 反向依赖 Wi-Fi/网络栈，层属倒挂；FreeRTOS 下初始化时序判断脆弱（Wi-Fi 可能在 ADC init 之后才启动，探针给虚假安全感）；把静态拓扑约束错误地实现成运行时检查。 |
| ② Codegen 静态门禁 + PAL 能力返回（采纳） | board 元数据标注 ADC 脚的 `unit`(1/2) 与 `wifi_conflict`；codegen 在设备树生成期，若 App 启用 Wi-Fi 却把模拟外设接到 ADC2 脚则 **Fail-Loud 报错**（ADR-0040）；运行时 PAL 不探 Wi-Fi，仅在该 target 确实无法兑现时返回 `WINK_ERR_UNSUPPORTED` | **采纳**。拓扑错误在生成期拦截，不占运行时；PAL 保持对网络栈无知；与既有 board 元数据/codegen 校验体系一致。 |

### 决策四：`analog_input` Role 契约遵从 Role SSOT

> **修正（2026-08-05）**：本决策初稿曾自行设计一个"最小 role（read_raw/read_mv）"并质疑 `read_promille` 精度与 `heart_rate` 适用性。复核已 Accepted 的 [`dal-role-architecture-spec.md`](../../../wink-micro-os/docs/dal-development-guide/dal-role-architecture-spec.md) v1.0.0（Role 面板 17-role SSOT，2026-08-05）后，确认 `analog_input` 已在该规范中定稿，本 ADR **遵从而非重定义**。初稿观点作废。

| 方案 | 描述 | 取舍 |
|---|---|---|
| ① 重新自拟最小 role（初稿） | 以 `read_raw`/`read_mv` 为公共面，质疑 promille 丢精度、heart_rate 无千分比语义 | **否决**。与已 Accepted 的 Role SSOT 冲突；且未领会 `analog_input` 的定位——它是"连续标量**原始**模拟输入"能力面，只暴露归一化值与原始 mV；BPM/重量/温度等业务量由上层（App/BAL 或 `environment_sensor`/`motion_sensor` 等专用 role）解释，本就不该塞进基础 role。 |
| ② 遵从 Role SSOT（采纳） | 严格按规范 §3.2-5 落地 `analog_input` 三动词：`read_promille`(convenience,uint16)、`read_mv`(convenience,uint16)、`read_promille_status`(normal,wink_status_t)；映射 type 按规范 §4 表（analog_knob / analog_sensor / load_cell / heart_rate，及 encoder 备选） | **采纳**。精度由分层消化：promille 是 App 便利面（≈10bit），需更细用 `read_mv`；已校准物理量走专用 role（B 类 float，ADR-0056）。role = codegen 内联门面（非 BAL），模板在 DAL 之上调 PAL。 |

## 决策结论（Decision）

### 1. PAL ADC 公共 API（目标无关）

新建 `pal/include/hal/pal_adc.h`，POD 配置 + 命名式静态 API（ADR-0004），全函数返回 `wink_status_t`（ADR-0001）：

```c
typedef uint8_t pal_adc_channel_t;   /* [0, PAL_ADC_CHANNELS) */

typedef struct {
    wink_pin_t pin;                /* 映射 GPIO；用 int16_t 语义兼容 NC(-1) */
    uint16_t  full_scale_mv;       /* 期望满量程 mV；0 = 平台默认(ESP32 11dB≈3100, wasm/host=3300) */
    uint8_t   resolution_bits;     /* 0 = 平台默认(12) */
} pal_adc_config_t;

wink_status_t pal_adc_init(pal_adc_channel_t ch, const pal_adc_config_t *cfg);
void          pal_adc_deinit(pal_adc_channel_t ch);

/* 正向/反向引脚映射，对称于 pal_pwm_channel_pin / pal_i2c_port_pins */
wink_status_t pal_adc_channel_pin(pal_adc_channel_t ch, wink_pin_t *out_pin);
wink_status_t pal_adc_pin_channel(wink_pin_t pin, pal_adc_channel_t *out_ch);

/* 单次采样，两个 getter 复用同一次采样结果（raw 与 mv 时刻一致） */
wink_status_t pal_adc_read_raw(pal_adc_channel_t ch, uint16_t *out_raw);
wink_status_t pal_adc_read_mv (pal_adc_channel_t ch, uint16_t *out_mv);
wink_status_t pal_adc_full_scale_mv(pal_adc_channel_t ch, uint16_t *out_mv);
```

- **逻辑 Channel 句柄抽象**：`pal_adc_channel_t` 为扁平逻辑通道索引（`0..PAL_ADC_CHANNELS-1`）。`pal_adc_init(ch, cfg)` 内部依据 `cfg->pin` 查 Board 元数据自动映射至目标平台的物理 ADC 单元与通道（例如 ESP32 的 ADC1_CH0 或 ADC2_CH3），上层驱动无需关心硬件通道重叠与映射细节。
- **采样时刻一致性与缓存**：`pal_adc_read_raw` 与 `pal_adc_read_mv` 共享通道结构体中的 `last_raw` / `last_sample_us` 缓存。`read_raw` 触发物理采样并刷新缓存，`read_mv` 复用同一时刻的缓存 Raw 值进行校准换算（或在同一次采样窗口内完成），保证两者时刻完全一致且不重复触发多余的硬件采样。
- **片上 ADC 范围界定（≤16-bit）**：`pal_adc.h` 严格限定于 **MCU 片上/内置 ADC**（分辨率 ≤ 16-bit，`uint16_t` 不溢出）。对于 24-bit 高精度外部 ADC（如 HX711 称重模块），其走专用总线/GPIO 驱动（如 Bit-bang GPIO / SPI 协议），不强行挤入 `pal_adc.h`。
- **不导出 `pal_adc_atten_t`**。衰减/增益是 esp32 target 内部细节：esp32 实现按 `full_scale_mv` 选最接近的硬件 atten 并做 eFuse 曲线校准；若 App 显式要求非标准档，留 target-private 扩展头 `<pal_adc_esp32.h>`，不污染公共头。
- `pal_adc_read_raw/mv` 为单次 oneshot 转换（esp32 上 µs 级忙等），公共头标 `WINK_BLOCKING`（最坏 = 一次转换时间），不引入连续采样/DMA 契约（连续 ADC + PWM 硬件触发属 ADR-0047 `pal_hwtimer` 范畴，不在本 ADR）。
- `PAL_ADC_CHANNELS` 默认 16，可 `-D` 覆盖。

### 2. 资源治理

`pal_resource_type_t` 增 `PAL_RESOURCE_ADC_CHANNEL = 6`。

**Claim 主体 = DAL（不是 PAL）**：严格遵循既有资源治理原则（`02-pal-platform-abstraction.md §4.1`，2026-07-02 落地）——**资源所有权登记归 DAL，PAL HAL 不参与 owner 表登记，只做硬件初始化**。理由：PAL 若以固定 owner 自 claim，会与 DAL 的 device-owner claim 二次冲突恒返 BUSY，且掩盖真正冲突者身份。这与 `pal_gpio_init`/`pal_pwm_init_ex` 自身不 claim、由 `dal_button`/`dal_rc_servo` 等 DAL claim 的现状一致。

因此本决策中：

- **`pal_adc_init` 内部不调用 `pal_resource_claim`**，仅做硬件配置。
- 消费 ADC 的 **DAL 驱动**（`dal_analog_knob`/`dal_analog_sensor`/`dal_heart_rate`/`dal_load_cell`，在各自子计划 #1/#5/#18/#19 落地）在 `pal_adc_init` 前后负责 claim/release。
- **ADC 通道与其 GPIO 脚的双重身份**：一个脚被配为 ADC 后不得再被数字 GPIO/PWM 占用。DAL init 须**同时 claim `PAL_RESOURCE_ADC_CHANNEL, ch` 与 `PAL_RESOURCE_GPIO_PIN, pin`**（同一 owner），deinit 同步释放两者；任一 claim 失败须回滚（对齐 §4.1 约束 2 多资源回滚）。跨子系统引脚冲突由此被既有资源表统一拦截。

> **现状注记（2026-08-05 核对）**：既有 PWM 路径（`dal_rc_servo.c:60`）当前**只 claim PWM_CHANNEL、未 claim 对应 GPIO pin**。本决策要求 ADC DAL 做更严格的双重 claim（ADC 脚与数字脚互斥是真实冲突）。PWM 是否补齐 GPIO_PIN claim 属独立一致性整改，不在本 ADR 范围，记为 follow-up。

### 3. Wasm Target 数据路径（通道 3 落地）

- 新增导入 `extern float js_pal_adc_read_norm(uint16_t pin);`（返回 `[0,1]`），**不新增** `js_pal_adc_read_mv`。JS 实现读 `PinArbiter.readAnalog(pin)`（电位器等元件已通过 `setAnalogDriver` 写入）。
- C 侧 `pal_wasm_adc.c`：拿 channel→pin，调 `js_pal_adc_read_norm`，在 C 内换算 raw/mv，并**经退化引擎**施加 RC 低通 + 高斯噪声 + 预热/采样间隔判定：
  - 复用 `wink_phys_rc_lowpass(ctx, target, now_us, rc_tau_s, adc_noise_v, &seed)`；
  - 复用 `wink_phys_warmup_check(...)`（预热内返回 `WINK_ERR_BUSY`，采样过近返回 `WINK_ERR_TIMEOUT`）；
  - 每通道持有独立 `wink_phys_rc_ctx`（按 pin/channel 索引，静态 BSS，越界直通，范式同 `pal_wasm_physical.c`）。
- JS 侧只产理想归一化物理量，不做 mV 换算、不知满量程。
- **PRNG 隔离与确定性**：退化噪声计算采用 **Per-Channel 独立 PRNG 种子**（由 `pin`/`channel` 索引确定性派生 `seed = hash(pin)`）。如此将随机数消耗限定在 ADC 通道内部，既确保模拟量噪点仿真的确定性，又**避免干扰非模拟量外设的全局 Golden 向量**。
- TS 侧必须同步：`WasmImports` 增 `js_pal_adc_read_norm`、`createUnisimImports`/`installUnisimBridge` 接线、`ssotAlignment.test.ts` 解析头文件比对、bump `PAL_WASM_ABI_HASH`（C 与 TS 各一份）。

### 4. ESP32 Target

- 基线 **ESP-IDF v6.0.1**（非 v5.x）。用 `esp_adc/adc_oneshot.h` + `adc_cali.h`；校准 scheme 名称以 v6 头文件为准（编码前核对，避免 v5→v6 改名导致 implicit-declaration）。
- **校准句柄生命周期管理**：`pal_adc_init` 依据 eFuse 状态初始化 `adc_cali_handle_t` 曲线校准句柄，`pal_adc_deinit` 必须显式释放该句柄，防止反复 init/deinit 导致 ESP-IDF 堆内存泄露。
- ADC 单元/通道映射由 board 元数据驱动；esp32 实现不内置硬编码全量表。
- **不运行时探测 Wi-Fi**。ADC2 在 Wi-Fi 下不可用的约束由决策三的 codegen 静态门禁拦截；若运行时确遇硬件不支持，返回 `WINK_ERR_UNSUPPORTED`。

### 5. Host / Baremetal Target

- **host**：实现确定性注入 API `pal_host_adc_inject_raw(ch, raw)` / `pal_host_adc_inject_mv(ch, mv)`（注入后两个 getter 一致返回），供 CTest 断言；不依赖 PinArbiter/JS。
- **baremetal（面向将来）**：当前仓库**没有 `targets/baremetal/` HAL target**（`TARGET_PLATFORM` 仅 wasm/host/esp32；baremetal 仅有 `osal/baremetal/`）。故本次只交付 esp32/wasm/host **三**个 HAL 实现。将来若新增 baremetal HAL target，其 ADC 实现为返回 `WINK_ERR_UNSUPPORTED` 的 stub，并遵守零编译污染（不得引入 `wink_sim_physical` 符号，对齐 physical-degradation §9）。记为 follow-up，不阻塞本 ADR。

### 6. Board 元数据与 Codegen 静态门禁

- ADC 能力元数据写入真实路径 `wink-tools/tools/codegen/boards/*.json`（**不是** `wink-micro-os/boards/`，该目录不存在）。在现有结构内增 `adc` 段：
  ```jsonc
  "adc": {
    "pins": {
      "36": { "channel": 0, "unit": 1 },
      "39": { "channel": 3, "unit": 1 },
      "25": { "channel": 8, "unit": 2, "wifi_conflict": true }
      // ...仅列该 board 实际可用脚，不伪造全量
    },
    "default_full_scale_mv": 3100,
    "default_resolution_bits": 12
  }
  ```
- codegen 新增校验：设备树中模拟外设所接引脚若在 `adc.pins` 缺失 → Fail-Loud；若 `wifi_conflict:true` 且 App 启用 Wi-Fi → Fail-Loud。这是**新增 codegen 能力**，须在前置计划中单列任务（读取点、规则、报错信息），不得隐含在"扩展 JSON"里。
- 芯片变体（ESP32/S3/C3）通道映射不同，由各自 board json 表达，不在代码里写死"全量 ESP32 表"。

### 7. `analog_input` Role 契约（遵从 Role SSOT v1.0.0）

`codegen/roles/analog_input.yaml` 按真实 schema 1.1（`id` + `verbs[].{id,error_class}`，**不是**初稿的 `role_name/return_type/params`）落地，**动词集严格等于** [`dal-role-architecture-spec.md`](../../../wink-micro-os/docs/dal-development-guide/dal-role-architecture-spec.md) §3.2-5 的定义，不自创：

```yaml
codegen_schema: "1.1"
id: analog_input
verbs:
  - id: read_promille
    error_class: convenience     # -> uint16_t，归一化 [0,1000]
  - id: read_mv
    error_class: convenience     # -> uint16_t，引脚电压 mV
  - id: read_promille_status
    error_class: normal          # -> wink_status_t，诚实 out 指针读
```

- `analog_input` 定位为"连续标量**原始**模拟输入"能力面：归一化值 + 原始 mV。`read_promille` 由 DAL 调 PAL `read_raw` 换算（`raw * 1000 / ((1<<bits)-1)`），role 模板只做单行内联转发，不含业务逻辑（规范 §5 行数约束）。
- 业务物理解释**不在本 role**：温度/光照/BPM/重量等由 App/BAL 基于 `read_mv` 换算，或由 `environment_sensor`/`motion_sensor` 等专用 role 承担（规范 §4 已把 DHT22/MPU6050 等映射到那些 role）。`heart_rate`(光电容积波)、`load_cell`(HX711 原始力) 的 default_role 即 `analog_input`，上层再解释为 BPM/重量——这是规范既定切分，本 ADR 遵从。
- 精度分层遵循 ADR-0056：promille 为 App 便利面（≈10bit），需更细读 `read_mv`；已校准物理量走专用 role 的 B 类 float（Full profile）/定点（Micro，binding 吸收）。
- PAL 仍提供 `pal_adc_read_raw` + `pal_adc_read_mv` 作为底层原语；role/DAL 构建于其上，但 role 不直接调 PAL（role 是 DAL 之上的 codegen facade）。

## 后果与约束（Consequences & Constraints）

| 正面 | 负面 / 缓解 |
|---|---|
| 通道 3 由 Planned 首次落地，且天然符合 channel-routing §2.4 同源铁律（DAL 校准/滤波同源，旁路仅替换原始源） | 需新增 codegen ADC 校验能力，比初稿"只改 JSON"工作量大；但这是真实的静态门禁，不能省 |
| wasm 模拟路径复用 PinArbiter 模拟 SSOT + 既有退化引擎，不产生第二条数据路径 | ADC 开始消费 PRNG → 必须重基 golden（ADR-0055）；计划中显式列为任务 |
| 公共 PAL API 目标无关，wasm/host 不背负 ESP32 衰减概念；需要非标准档时走 target-private 扩展头 | esp32 需维护 full_scale_mv→atten 的内部映射；一次性成本，封装在 target 内 |
| ADC2/Wi-Fi 拓扑错误生成期 Fail-Loud，不占运行时、不引入 PAL→Wi-Fi 反向依赖 | board json 需按芯片变体分别维护；与既有 per-board 元数据体系一致 |
| `analog_input` role 直接遵从已 Accepted 的 Role SSOT v1.0.0，零新设计、零分歧；verb 集（read_promille/read_mv/read_promille_status）与 17-role 面板对齐 | 初稿曾自拟 role，已作废；实现时须照规范而非初稿 plan |
| 三 target（esp32/wasm/host）构建矩阵保持完整 | baremetal HAL target 当前不存在，记为 follow-up |

## 遵循与后续（Compliance & Follow-up）

Accepted 后必须：

- [x] 回写 `02-wink-micro-os/02-pal-platform-abstraction.md`：新增 §2.3 PAL ADC 子系统（API、目标无关配置、三 target 矩阵 + baremetal 面向将来、codegen 门禁），§4.1 资源表增 `PAL_RESOURCE_ADC_CHANNEL`（claim 主体为 DAL，非 PAL） — 2026-08-05
- [x] 回写 `04-wasm-simulation/02-mechanisms/08-channel-routing.md §2.4`：通道 3 由 Planned → Partial，补 `js_pal_adc_read_norm` + PinArbiter 数据路径、raw/mv C 侧换算、per-channel PRNG；同步选型表与 §5.1 缺口表；同步 `10-wasm-js-bridge-abi.md §3.1` 导入表与 `07-peripheral-registry.md §4.3` — 2026-08-05
- [x] 据本 ADR 修订前置计划 `00.5-pal-adc-subsystem-plan.md`（role schema 1.1、board 真实路径、`read_norm` 而非 `read_mv`、TS ABI/hash、`pal_adc_channel_pin`、ESP-IDF v6.0.1、噪声/RC 接线、baremetal stub、codegen 校验单列任务；claim 主体修正为 DAL） — 2026-08-05
- [ ] `analog_input.yaml` 按决策 7 的 1.1 schema 落地，动词集严格对齐 `dal-role-architecture-spec.md` §3.2-5（read_promille / read_mv / read_promille_status），不自创 verb；
- [ ] 编码落地后：host 单测覆盖 raw/mv 一致性、注入、满量程；wasm 覆盖 norm→raw/mv 换算 + 噪声/RC 确定性 + PRNG 消费序；esp32 v6.0.1 build 0 warn 0 error；`wink lint --pack layering --pack api` 通过；
- [ ] golden 重基并在 PR 记录 PRNG 消费序变更。

> 本 ADR 定稿契约，不含具体编码排期；排期由前置计划 00.5 与各外设子计划承担。

---

*本 ADR 状态变更请在此记录：*
- 2026-08-05：Proposed（基于前置计划 00.5 初稿评审，定稿四个决策点：目标无关满量程配置 / PinArbiter 归一化路径 / codegen 静态门禁 / role 契约遵从 Role SSOT）
- 2026-08-05：修订决策四——复核 `dal-role-architecture-spec.md` v1.0.0（2026-08-05 Accepted Role SSOT）后，作废自拟的"最小 role"，改为严格遵从其 `analog_input` 定义（read_promille / read_mv / read_promille_status）；决策一/二/三不变
- 2026-08-05：Accepted（包含采样缓存时刻一致性约束、片上 ADC ≤16bit 范围界定、Per-Channel 独立 PRNG 隔离、ESP32 校准句柄生命周期管理与扁平逻辑 Channel 映射）
- 2026-08-05：回写规范时修正 §2 资源治理——claim 主体由"`pal_adc_init` 内部 claim"改为"**DAL claim、PAL 不 claim**"，对齐既有 `02-pal §4.1` 原则（PAL 不参与 owner 表登记；与 `pal_gpio_init`/`pal_pwm_init_ex` 现状一致）；同步移除 `pal_adc_config_t.owner` 字段。决策一/二/三/四不变

