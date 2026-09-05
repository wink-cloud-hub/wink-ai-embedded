# Spike-S3 结论报告：Codegen 跨仓契约 + 热学插件落点

| 项 | 内容 |
|---|---|
| Spike 编号 | S3（M0-4） |
| 日期 | 2026-08-27 |
| 状态 | ✅ 结论已出（codegen 契约实证；跨仓改动出草案未应用；私有 fixture 延后） |
| 验证环境 | Python 3（直接驱动兄弟仓 `wink-tools/tools/frontend/runtime_device_tree.py`，**未改动兄弟仓任何文件**） |
| PoC 资产 | `docs/tech-designs/mcs51/spikes/assets/s3/`（`boards/mcs51_devboard.json`、`wink_app_iron_ntc.json`、`s3_codegen_probe.py`、`mcs51_board_config.h.j2.draft`；build 产物 `device-tree.iron_ntc.json` 不入库） |
| 消费里程碑 | M4（板级 codegen、adc0832 数据面）、M6（thermal_heater_plate 插件、iron_ntc e2e） |
| 探测的兄弟仓 | wink-tools `D:\…\wink-ai\packages\wink-tools`；unisim `…\wink-ai\packages\unisim`；插件仓 `wink-ai-embedded\wink-plugin-peripherals` |

---

## 1. 问题与裁决

**问题**：8051 沙箱需要 (1) 固件期静态板级配置（引脚/通道常量）与 (2) 运行期 device-tree（驱动 unisim 元件 + PinArbiter），两者都由 `wink-app.json` SSOT 经 wink-tools codegen 产出；需裁决跨仓改动面、板级 schema、引脚拍平约定，以及 thermal_heater_plate 插件落点与元件 schema。

**裁决一句话**：**运行期 device-tree 发射链已存在且通用——8051 新板/新元件零改动发射器即可产出**（实证）；跨仓硬改动仅 **1 行 target 宏 + 1 个 board JSON + 1 个固件期模板**；thermal 插件按 `analog_knob` 同构落在插件仓 `builtin/thermal_heater_plate/`，热动力学参数只走运行期 device-tree.properties，**不编译进固件**。

### 1.1 关键裁决明细

| # | 裁决 | 证据/理由 |
|---|---|---|
| C1 | **运行期 device-tree 发射器无需为 mcs51 改一行**。现有 `build_runtime_device_tree()` 只依赖 board JSON 的 `headers` + manifest 的 `pins/properties`，与 MCU 无关。 | §2 PoC：喂合成 manifest_index，未改发射器即产出正确 device-tree.json |
| C2 | **引脚拍平约定**：8051 `P0.0~P3.7` → pin index `0..31`，`index = port*8 + bit`；SFR 端口地址 `0x80 + port*0x10`（P0=0x80…P3=0xB0）。board `headers` 用 `"P2.0": 16` 这样的字符串键，经 `$board.headers.P2.0` 引用解析为 int。 | 与 esp32_devkitc.json `headers:{"D2":2,...}` 同机制；PoC 断言 CS/CLK/DIO=16/17/18 |
| C3 | **跨仓硬改动极小**：(a) `config_h.py` target 表加 1 行 `"mcs51": "WINK_TARGET_MCS51_SIM"`；(b) 新增 `codegen/boards/mcs51_devboard.json`；(c) 新增固件期模板 `mcs51_board_config.h.j2`（device_tree.h.j2 的 8051 作用域版）。 | config_h.py L118-124 现状仅 esp32/wasm/host/baremetal；boards/ 仅 esp32_devkitc.json；templates/ 无 mcs51 |
| C4 | **固件期 vs 运行期分层**：`mcs51_board_config.h` 只编译**引脚索引 + ADC 通道 + 设定点**等固件需静态知道的常量；`tau/watts/beta/R25` 等热动力学参数是仿真模型参数，只存在于运行期 device-tree.json 的 `properties`，**不进固件**（固件看不到 NTC 物理，只读 ADC 归一化值）。 | 模板草案注释明示；device-tree properties 原样透传 |
| C5 | **thermal 插件落点**：`wink-plugin-peripherals/builtin/thermal_heater_plate/1.0.0/`，结构对标 `analog_knob`：`package.json` + `1.0.0/src/simulation.ts`（`normalizeManifest` + `BaseSimulationPlugin` 子类 + `ManifestFactory`）+ `tsconfig.json` + `vite.config.sim.ts`。热模型积分/UI 属 unisim 引擎域，插件只供 manifest（pins/properties/stateChannels/timingModel）与行为钩子。 | analog_knob 实证结构；unisim 已迁兄弟仓 `packages/unisim`，插件经 `@wink-ai/unisim` 依赖 |
| C6 | **adc0832 是元件类型不是板载**：3 线 DIO（CS/CLK/DIO）作为 manifest pins，DIO 为 `bidir`；`channel/vrefMv` 为 properties。cms8s 片内 ADC 无外部引脚（M5），不走 device-tree 引脚拍平，走固件期通道常量。 | adc0832 是外置 SPI-like 芯片；cms8s ADC 是 MCU 内部外设 |
| C7 | **私有真实工程 fixture 不在 spike 采集**（许可证：普中/串口Demo/CMS8S原厂源不入库）。语法特性清单沿用 S2 已实证集 + SSOT；真实 GBK 工程转码无损回归（E-003）门禁到 fixture 可用，由用户在仓外提供路径。 | 项目约束：私有 fixture 不提交 |

## 2. 关键证据（发射器零改动产出）

PoC 直接 import 兄弟仓**未修改**的发射器，喂 mcs51 board + 合成 manifest（adc0832/thermal_heater_plate/led）：

```
$ python docs/tech-designs/mcs51/spikes/assets/s3/s3_codegen_probe.py
=== device-tree.json (emitted by UNMODIFIED emitter) ===
{
  "appName": "iron_ntc",
  "board": "mcs51_devboard",
  "devices": {
    "temp_adc": { "type": "adc0832",
      "pinMapping": { "CS": 16, "CLK": 17, "DIO": 18 },
      "properties": { "channel": 0, "vrefMv": 5000 } },
    "heater": { "type": "thermal_heater_plate",
      "pinMapping": { "DRIVE": 8 },
      "properties": { "ntcChannel": 0, "setpointC": 180.0, "thermalTauS": 8.0,
                      "heaterWatts": 40.0, "ntcBeta": 3950, "ntcR25Ohm": 100000 } }
  }
}
[s3] PASS: existing emitter flattens P0.0~P3.7 -> 0..31 and passes
        adc0832 + thermal_heater_plate schema with ZERO emitter change.
```

验证点：
- `$board.headers.P2.0` → int `16`（P2.0 = port2*8+bit0 = 16）；P1.0 → `8`。拍平/别名/`$board.headers` 解析全走现有 `runtime_device_tree.py` 逻辑（L318-345 引脚解析、L484-487 properties camelCase 转换）。
- 热学 6 个 properties 经 `snake_to_camel`（`setpoint_c`→`setpointC`）原样透传，无需发射器认识热学。
- 最终 JSON 形状 `{appName, board, devices:{id:{id,type,pinMapping,properties}}}` 与 unisim 运行期消费契约一致（sim.py 已写 `unisim-assets/device-tree.json`）。

**跨仓引用的现状代码位置**（M4 开工锚点）：
- 发射器：`wink-tools/tools/frontend/runtime_device_tree.py`（`build_runtime_device_tree` L407、`resolve_pin_name` L188、board headers 解析 L318/L471）。
- CLI 发射：`wink-tools/tools/cli/commands/build/sim.py`（`wink build sim` → `unisim-assets/device-tree.json`）。
- target 宏：`wink-tools/tools/codegen/generators/config_h.py` L118-124。
- 板目录：`wink-tools/tools/codegen/boards/`（仅 `esp32_devkitc.json`）。
- 固件模板：`wink-tools/tools/codegen/templates/`（`device_tree.h.j2`/`device_tree.c.j2`/`wink_arduino_bindings.*.j2`，无 mcs51）。
- 插件范式：`wink-plugin-peripherals/builtin/analog_knob/{package.json,1.0.0/src/simulation.ts}`。

## 3. 可复用产物（M4/M6 直接消费）

- **板级 JSON**：`assets/s3/boards/mcs51_devboard.json` → M4 复制进兄弟仓 `codegen/boards/`（headers 全 32 脚 + metadata.mcu="mcs51" + sim_heap_quota_kb=16）。
- **SSOT 样例**：`assets/s3/wink_app_iron_ntc.json` → M6 iron_ntc app 的 wink-app.json 雏形（adc0832 + heater 两元件 + `$board.headers.*` 引用）。
- **探针**：`assets/s3/s3_codegen_probe.py` → M4 回归脚本（合成 manifest 驱动真发射器，断言拍平/透传）。
- **固件期模板草案**：`assets/s3/mcs51_board_config.h.j2.draft` → M4 在兄弟仓落 `templates/mcs51_board_config.h.j2`；含 pin-index 宏、ADC/heater 实例常量、`MCS51_PIN_IDX_PORT/BIT/VALID` 守卫。热学参数不入固件（C4）。
- **config_h.py 补丁（草案，未应用）**：
  ```python
  # wink-tools/tools/codegen/generators/config_h.py  target_macro 表内追加一行
      "mcs51": "WINK_TARGET_MCS51_SIM",
  ```
- **manifest schema 草案**（adc0832 / thermal_heater_plate，见 `s3_codegen_probe.py` 的 `MANIFEST_INDEX`）：
  - adc0832 pins：`CS(sink,digital,aliases cs/nss)`、`CLK(sink,aliases clk/sclk)`、`DIO(bidir,aliases dio/mosi/miso/data)`；properties：`channel`、`vrefMv`。
  - thermal_heater_plate pins：`DRIVE(sink,digital,aliases drive/heat/pwm/gate)`；properties：`ntcChannel`(绑定反馈 ADC 通道)、`setpointC`、`thermalTauS`、`heaterWatts`、`ntcBeta`、`ntcR25Ohm`；`timingModel` 建议 `continuous`（热惯性积分，非纯事件驱动）。
- **thermal 插件骨架契约**（M6）：仿 analog_knob 建 `builtin/thermal_heater_plate/1.0.0/src/simulation.ts`，`createThermalHeaterPlateManifest()` 返回上述 pins/properties + `stateChannels:{plateTempC:{...}, heaterOn:{...}}`；`class ThermalHeaterPlatePlugin extends BaseSimulationPlugin`；`package.json` 依赖 `@wink-ai/unisim`。热 RC 积分（tau）与 NTC β 方程在插件 tick 内算，经 stateChannel 把温度→ADC 归一化值回灌 mcs51 adc 注入轨（边界④ mcs51_adc injection）。

## 4. 回写点（影响计划任务行）

| 任务 | 回写内容 |
|---|---|
| M4 codegen（跨仓） | 发射器**零改动**（C1）；跨仓仅剩 3 件：config_h +1 行、新增 board JSON、新增 mcs51_board_config.h.j2。走外部 PR（E-001/C-001），本仓出草案/diff，不直接改兄弟仓工作树 |
| M4 adc0832 | 元件 manifest 用 CS/CLK/DIO(bidir) + channel/vrefMv；固件侧引脚索引由 board_config.h 常量喂 3 线 DIO 状态机 |
| M5 cms8s ADC | 片内无外部引脚，不走 device-tree 拍平；通道走固件期常量（C6） |
| M6 thermal | 插件落点 `builtin/thermal_heater_plate/1.0.0/`（对标 analog_knob）；热参数只走运行期 properties（C4）；温度经 stateChannel→adc 注入轨闭环；timingModel=continuous |
| R-003（codegen 跨仓契约不清） | **退役**——契约实证：发射器通用、改动面 3 件、引脚拍平约定确定 |
| E-001（thermal/引擎外部排期） | 保留为外部依赖，但落点与 schema 已锁定（C5），插件可在本仓 wink-plugin-peripherals 内先行开发，不阻塞 unisim 引擎改动 |
| E-003（真实 GBK 工程回归） | 保留；fixture 由用户仓外提供，不入库（C7） |

---

*Spike 结论在 M4/M6 落地后，可将引脚拍平约定与固件期/运行期分层并入总纲 SSOT §2.2/数据面 SSOT 并归档。*
