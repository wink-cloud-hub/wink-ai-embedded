# Stage1：ADC 物理引脚纠偏与 E-02 根除（立即生效）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S1-ADC` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
| **优先级** | 🔴 P0（阻塞性行为失真） |
| **关联 CPL** | CPL-01（32+ch）、CPL-02（ADCLDO）、CPL-17（契约）、CPL-22（测试双轨）、CPL-23（ABI 双读） |
| **前置依赖** | stage0（`caps_cache` 快照；双读告警码可先用硬编码，描述符字段就绪后转接） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 0. 版本与开关命名锁定（本阶段冻结，后续阶段只引用）

- **引脚语义版本**：`v1` = 合成通道（`32 + ch`，废弃中）→ `v2` = 板级物理 Pin（目标）。`SimTraceSpecV2` 主版本不动，本升版记录于 ABI 版本附录。
- **双读开关**：芯片层 `cms8s_adc_dual_read_synth`（默认 ON，本阶段；stage7 删除整块逻辑）。
- **可观测计数**：`cms8s_adc_synth_redirect_count`（合成通道重定向次数，单测断言旧用例告警且通过）。
- 发版顺序：仿真后端 → 前端插件 → stub/文档；回滚即关开关回 `v1` 语义（开关独立 commit）。

## 1. 目标

- ✅ 通用 rail 只收物理 Pin，废除 `32 + ch` 假定。
- ✅ `cms8s_adc.cpp` 以 `AN_TO_PIN[26]` 常表闭环（P3 段 24~27），ADCLDO 基准下沉芯片层。
- ✅ ABI 升版 + 芯片层双读一版（合成通道告警 + 重定向），发版顺序后端→前端→stub/文档。
- ✅ 测试拆 `core-tests` / `cms8s-tests`，迁移期双绿。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `frameworks/mcs51/src/mcs51_adc.cpp`、`include/mcs51_adc.h` | ✏️ | 物理 Pin 接口；注释去 `32+ch` |
| `frameworks/mcs51/src/cms8s_adc.cpp` | ✏️ | `AN_TO_PIN` 常表 + ADCLDO + 双读兼容 |
| `frameworks/mcs51/src/mcs51_uni_bridge.cpp` | ✏️ | host 回退数组双轨 |
| `test/` 下 ADC 相关 | ✏️ | 拆 core/cms8s 两组 |

架构红线：通用 rail 禁出现 AN 语义与 ADCLDO；公式法禁入（必须查表）。

## 3. 任务拆分

### Task S1-1：通用 rail 物理 Pin 化 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：`mcs51_adc_get_value` 改为收物理 Pin；`js_pal_adc_read_norm(pin)` 直透；删 `32u + ch`。
- [ ] **Step 2**：`mcs51_adc_set_vref/vrail` 转为芯片层调用的通用 rail 参数（通用头去 ADCLDO 注释）。
- [ ] **Step 3**：验证养生壶冷启动 NTC 室温读数，E-02 消失。

### Task S1-2：CMS8S 闭环 + 双读兼容 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：新增 `AN_TO_PIN[26] = {0..7, 8..15, 16..21, 24..27}` + 越界钳位 + `static_assert(AN_TO_PIN[22]==24)`。
- [ ] **Step 2**：合成通道（32+ch）保留一版：告警 + 重定向到物理 Pin，计数可观测。
- [ ] **Step 3**：ADCLDO.VSEL 基准计算迁入芯片层。

### Task S1-3：契约升版 + 测试双轨 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：C-ABI 引脚语义升版记录（版本号/发版顺序/回滚步骤）。
- [ ] **Step 2**：测试按 `core`（物理 Pin、标准向量）与 `cms8s`（AN 映射、扩展向量）分组，双绿。
- [ ] **Step 3**：host 回退数组随版本双轨。

## 4. 验收

- L0/L1：双轨全绿；旧合成通道用例告警但通过。
- L2：养生壶场景冷启动 NTC≈25℃，无 E-02。
- L3：ABI 版本记录归档。
- L4：`mcs51_adc.*` 零 `32 +` 残留。

## 5. 风险与回滚

- R-01（版本错配）：双读 + 发版顺序缓解；回滚：关闭双读重定向回旧语义（开关单独提交）；Git revert 本阶段 commit。
