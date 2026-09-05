# DAL mono_oled 外设重构与 Wokwi 拓扑解耦实施计划

| 项 | 内容 |
|---|---|
| **文档名称** | DAL mono_oled 外设重构与 Wokwi 拓扑解耦实施计划 |
| **文档路径** | `docs/implementation-plans/frontend/05-display-mono-oled-refactor-plan.md` |
| **版本** | v1.1.0 (SSOT v2.3.3 全面精修规范版) |
| **日期** | 2026-08-08 |
| **上级计划** | [`00-master-execution-plan.md`](00-master-execution-plan.md), [`00.1-category-type-variant-wokwi-ssot.md`](00.1-category-type-variant-wokwi-ssot.md) |

---

## 1. 重构背景与问题诊断

在现网 DAL C 驱动及 Codegen 架构中，`mono_oled` 外设存在以下分类与接口冲突：

1. **变体分类轴混淆**：
   - 现网 `dal_mono_oled.h` 将 `variant` 定义为控制器芯片型号（`DAL_MONO_OLED_VARIANT_SSD1306` vs `DAL_MONO_OLED_VARIANT_SH1106`）。
   - 这打破了 SSOT §4.1 不变量：若 `variant` 为芯片型号，则同个 `variant`（如 `SSD1306`）同时支持 I2C (4 Pin) 和 SPI (7 Pin) 两种物理拓扑，导致前端拖拽面板及 Codegen 无法根据 `variant` 静态确定引脚映射。
2. **总线拓扑与控制芯片二维解耦**：
   - **变体（Variant）**：仅用于表达物理引脚拓扑变异（`affects_pins: true`），即 `ssd1306_i2c` (4Pin) 与 `ssd1306_spi` (7Pin)。
   - **控制器型号（Panel IC）**：作为 `dal_mono_oled_ic_t` 枚举归入 `dal_mono_oled_config_t.panel_ic`（`affects_pins: false`）。SSD1306 与 SH1106 共享引脚拓扑，仅在 `init()` / `flush()` 算法层面区分寻址模式（页寻址 vs 水平寻址）与 0x02 列偏移。
3. **wink-tools 平铺配置与 -1 Sentinel 适配**：
   - 采用平铺配置结构体 `dal_mono_oled_config_t`，利用 `wink-tools` (Python Codegen `emit_config.py`) 的 `variant_fields` 机制：在选择 `ssd1306_i2c` 变体时，未使用的 SPI 引脚（`pin_clk`, `pin_mosi` 等）由工具链自动裁剪并初始化赋值为 `-1`，确保 ABI 稳定性与驱动层零风险识别。
4. **SSOT §5.1 零 `wokwi_binding` 约束**：
   - 遵照 SSOT §5.1 规则 4，Driver YAML 严禁包含 `wokwi_binding` 字段，Wokwi 仿真拓扑绑定由前端 TypeScript 外设包 (`peripherals/builtin/.../variants.ts`) 统一以 SSOT 大表 `element` 列为依据掌控，解耦 C Codegen 配置层与前端渲染层。

---

## 2. 详细设计规范

### 2.1 C API 头文件设计 (`dal_mono_oled.h`)

#### (1) Variant 与 IC 枚举定义 (严格对齐 SSOT §5.1 双重静态断言)
```c
/**
 * @brief Monochrome OLED physical interface variant (affects pinout)
 */
typedef uint8_t dal_mono_oled_variant_t;
enum {
    DAL_MONO_OLED_VARIANT_SSD1306_I2C = 0,  /**< I2C mode: [VCC, GND, SCL, SDA] 4-Pin */
    DAL_MONO_OLED_VARIANT_SSD1306_SPI = 1,  /**< SPI mode: [VCC, GND, CLK, MOSI, CS, DC, RES] 7-Pin */
};
#define DAL_MONO_OLED_VARIANT_COUNT 2

_Static_assert(sizeof(dal_mono_oled_variant_t) == 1, "variant must stay 1 byte");
_Static_assert(DAL_MONO_OLED_VARIANT_COUNT == 2, "Variant count mismatch with SSOT §2 and codegen YAML");
_Static_assert(DAL_MONO_OLED_VARIANT_SSD1306_SPI + 1 == DAL_MONO_OLED_VARIANT_COUNT, "Sequential variant ordering error");

/**
 * @brief Monochrome OLED display controller IC variant (algorithm only, affects_pins: false)
 */
typedef uint8_t dal_mono_oled_ic_t;
enum {
    DAL_MONO_OLED_IC_SSD1306 = 0,  /**< Standard SSD1306 (horizontal/page addressing) */
    DAL_MONO_OLED_IC_SH1106  = 1,  /**< SH1106 (page addressing with 0x02 column offset) */
};
#define DAL_MONO_OLED_IC_COUNT 2

_Static_assert(sizeof(dal_mono_oled_ic_t) == 1, "panel_ic must stay 1 byte");
```

#### (2) Config 结构体平铺设计 (含显式内存 Padding 字节)
```c
/**
 * @brief Monochrome OLED configuration struct (Flat layout with sentinel trimming)
 */
typedef struct {
    const char             *owner;      /**< Instance owner static string (4 bytes on 32-bit, 8 bytes on 64-bit) */
    dal_mono_oled_variant_t variant;    /**< Interface variant (determines pinout, 1 byte) */
    dal_mono_oled_ic_t      panel_ic;   /**< Controller IC (determines flush algorithm, 1 byte) */
    uint8_t                 i2c_port;   /**< Logical I2C bus index (1 byte) */
    uint8_t                 padding0;   /**< Explicit byte alignment padding (1 byte) */
    uint16_t                i2c_addr;   /**< 7-bit I2C address (0x3C or 0x3D, 2 bytes) */
    uint16_t                width;      /**< Display width in pixels (e.g., 128, 2 bytes) */
    uint16_t                height;     /**< Display height in pixels (e.g., 64, 2 bytes) */

    /* SPI fields (Active when variant == DAL_MONO_OLED_VARIANT_SSD1306_SPI, trimmed to -1 on I2C) */
    int16_t                 pin_clk;    /**< SPI Clock pin (2 bytes) */
    int16_t                 pin_mosi;   /**< SPI MOSI pin (2 bytes) */
    int16_t                 pin_cs;     /**< SPI Chip Select pin (-1 if bus dedicated, 2 bytes) */
    int16_t                 pin_dc;     /**< SPI Data/Command pin (2 bytes) */
    int16_t                 pin_res;    /**< SPI Reset pin (-1 if hardwired, 2 bytes) */
    int16_t                 padding1;   /**< Explicit 2-byte alignment padding (2 bytes) */
} dal_mono_oled_config_t;

/* ABI layout validation assertions */
#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, WASM32 */
_Static_assert(sizeof(dal_mono_oled_config_t) == 24, "ABI break: config size changed on 32-bit target");
#else                         /* LP64: 64-bit host */
_Static_assert(sizeof(dal_mono_oled_config_t) == 28, "ABI break: config size changed on 64-bit host");
#endif
```

### 2.2 Codegen Driver YAML 契约 (`mono_oled.yaml`)

```yaml
codegen_schema: "1.1"
type: mono_oled
category: display
source_stem: mono_oled

fields:
  - name: variant
    type: enum
    enum: [ssd1306_i2c, ssd1306_spi]
    map:
      ssd1306_i2c: DAL_MONO_OLED_VARIANT_SSD1306_I2C
      ssd1306_spi: DAL_MONO_OLED_VARIANT_SSD1306_SPI
    affects_pins: true
    variant_fields:
      ssd1306_i2c: [i2c_port, i2c_addr, panel_ic, width, height]
      ssd1306_spi: [pin_clk, pin_mosi, pin_cs, pin_dc, pin_res, panel_ic, width, height]

  - name: panel_ic
    type: enum
    enum: [ssd1306, sh1106]
    default: ssd1306
    map:
      ssd1306: DAL_MONO_OLED_IC_SSD1306
      sh1106: DAL_MONO_OLED_IC_SH1106

  - name: i2c_port
    type: int
    default: 0
  - name: i2c_addr
    type: int
    default: 60 # 0x3C

  - name: pin_clk
    type: int
    default: -1
  - name: pin_mosi
    type: int
    default: -1
  - name: pin_cs
    type: int
    default: -1
  - name: pin_dc
    type: int
    default: -1
  - name: pin_res
    type: int
    default: -1

  - name: width
    type: int
    default: 128
  - name: height
    type: int
    default: 64
```

### 2.3 驱动层算法与硬件传输时序规范

#### (1) SSD1306 与 SH1106 `flush()` 算法差异
- **SSD1306**：支持水平自动递增寻址模式（Horizontal Addressing Mode, 命令 `0x20, 0x00`）。在设置 Column（0..127）与 Page（0..7）范围后，可以单次连续 DMA/I2C/SPI 发送全量 1024 字节 Framebuffer 数据。
- **SH1106**：只支持页寻址模式（Page Addressing Mode）。在 `flush()` 中必须循环 8 页，每页显式发送：
  1. Set Page Start Address: `0xB0 + page`
  2. Set Lower Column Address (带 0x02 列偏移): `0x02`
  3. Set Higher Column Address: `0x10`
  4. 数据流传输：连续发送当前页 128 字节 Framebuffer 字节。

#### (2) 7-Pin SPI 管脚控制时序规范
- **`pin_res` 硬复位**：`dal_mono_oled_init()` 初始化阶段，若 `pin_res != -1`，拉低 RESET 脚 10ms，再拉高并延迟 10ms，确保控制器复位状态机就绪。
- **`pin_dc` 数据/命令控制**：写初始化命令/寻址命令时拉低 `pin_dc`；发送 Framebuffer 像素字节时拉高 `pin_dc`。
- **`pin_cs` 片选控制**：若 `pin_cs != -1`，每次 Write 事务开始拉低 CS，结束时拉高 CS。若 `pin_cs == -1`，表示总线硬连接地线独占，驱动跳过 CS 控制。

---

## 3. 修改计划与任务拆解

### 任务 1：C DAL 驱动层更新
- [ ] 修改 `wink-micro-os/dal/include/display/dal_mono_oled.h`
  - 增加 `dal_mono_oled_ic_t` 枚举及 `uint8_t` 类型定义
  - 更新 `dal_mono_oled_variant_t` 枚举为 `SSD1306_I2C` 与 `SSD1306_SPI`
  - 添加 SSOT §5.1 双重 `_Static_assert` 门禁与 `dal_mono_oled_config_t` padding 对齐字节
  - 更新 32-bit 与 64-bit 的 ABI 尺寸断言
- [ ] 修改 `wink-micro-os/dal/src/display/dal_mono_oled.c`
  - 更新 `init()` 与 `flush()` 分发逻辑
  - 为 SH1106 补充页寻址及 0x02 列偏移实现
  - 补充 7-Pin SPI 模式（Bit-banging / PAL SPI）与 DC/RES 管脚控制逻辑

### 任务 2：Codegen YAML 契约与 Python 工具链适配
- [ ] 在 `wink-tools/tools/codegen/drivers/` 补充/更新 `mono_oled.yaml`
- [ ] 补全所有 SPI 引脚字段的 `default: -1` 声明
- [ ] 确保 `variant_fields` 校验与 `emit_config.py` -1 剪裁完美匹配

### 任务 3：SSOT 文档全面纠偏
- [ ] 更新 `00.1-category-type-variant-wokwi-ssot.md` 中的 §2.5 `mono_oled` 表格条目（Row 19）
- [ ] 更新 §3 代码落地状态表中的 #19 `mono_oled` 标记（由 `🚧 Refactor Planned` 推进更新为 `✅ Completed`）

---

## 4. 验证计划

1. **单元测试与 ABI 校验**：
   - 编译 DAL `dal_mono_oled`，运行头文件中的 `_Static_assert` 门禁断言。
   - 更新并运行 [test_dal_abi_freeze.c](../../../../../wink-micro-os/test/unit/dal/test_dal_abi_freeze.c) ABI 冻结测试。
2. **Mock 总线命令输出测试**：
   - 运行 [test_dal_mono_oled.c](../../../../../wink-micro-os/test/unit/dal/test_dal_mono_oled.c)，验证 `panel_ic == DAL_MONO_OLED_IC_SH1106` 时输出了带有 `0xB0` 与 `0x02` 列偏移的 Flush 指令流。
3. **Codegen 生成测试**：
   - 运行 `pytest tools/codegen/tests/`，生成 I2C 和 SPI 模式代码，校验非活跃引脚被自动填为 `-1`。
4. **Wokwi 仿真测试**：
   - 在 WASM/Wokwi 仿真环境中验证 128x64 文本渲染与图像 flush。

