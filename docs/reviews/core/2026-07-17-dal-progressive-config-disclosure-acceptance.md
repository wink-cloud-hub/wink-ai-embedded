# ADR-0034 Progressive Config Disclosure — 实施后复评 / 验收记录

> **评审对象：**
> - ADR：[`decisions/0034-dal-progressive-config-disclosure.md`](../../decisions/core/0034-dal-progressive-config-disclosure.md)（Accepted）
> - 技术设计：[`tech-designs/2026-07-16-dal-progressive-config-disclosure.md`](../../tech-designs/core/2026-07-16-dal-progressive-config-disclosure.md)
> - 实施计划：[`implementation-plans/2026-07-16-dal-progressive-config-disclosure-plan.md`](../../implementation-plans/core/2026-07-16-dal-progressive-config-disclosure-plan.md)
> - 设计期评审（只读基线）：[`2026-07-17-dal-progressive-config-disclosure-review.md`](./2026-07-17-dal-progressive-config-disclosure-review.md)
>
> **评审日期：** 2026-07-17（复评收口）  
> **性质：** 实施后验收快照（Layer ④）；不修改设计期评审正文。  
> **结论：** **Accept with deferred HIL** —— 软件门禁与 wasm 行为验证通过；ESP32 真机 HIL 因缺 OLED/舵机等外设暂缓，不阻塞合入。

---

## 1. 设计期 P0 闭环核对

| 设计期 P0 | 落地状态 | 证据 |
|-----------|----------|------|
| P0-1 Router 按完整 profile（freq+bits+clock） | ✅ | `pal_pwm_router.*`；`test_pal_pwm_router` 含同频不同 bits 不复用 |
| P0-2 POD ABI bump | ✅ | `VERSION 0.2.0` / `ABI=2`；Binary host package + consumer smoke |
| P0-3 时钟契约 `STABLE_REQUIRED`→ESP32 REF_TICK；host/wasm `UNSUPPORTED` | ✅ | esp32 / host / wasm `pal_pwm_init_ex`；单测覆盖 |
| P0-4 floating `INPUT` 未注入 → `WINK_ERR_DISCONNECTED` | ✅ | host/wasm `pal_gpio_read`；`test_dal_button` NONE 路径 |
| P0-5 DAL 不泄漏 `pal_*` 类型 | ✅ | `dal_button.h` / `dal_servo.h` 自有枚举；`.c` 内映射 |

设计期 **Major Revision** 所列阻断项均已在实现中关闭；本文件不再重复否决理由。

---

## 2. 验收矩阵（Task 9）

| 门禁 | 结果 | 备注 |
|------|------|------|
| Host 单测（PWM Router / config / Button / Servo） | ✅ PASS | 含 progressive-config 新增用例 |
| Codegen `unittest discover` | ✅ PASS | 默认 golden 字节级不变；`test_advanced_validate` 覆盖 advanced |
| Wasm App 行为（`oled_dashboard` / `avoidance_car` 等） | ✅ PASS | **Owner 本机 wasm 实测通过**（2026-07-17） |
| Binary SDK host package + `binary_sdk_smoke` | ✅ PASS | `wink-micro-os-sdk-binary-v0.2.0.tar.gz` |
| Binary SDK wasm package | ⏸ 未验证本波次 | 登记缺口；不挡 Accept-with-deferred-HIL |
| ESP32 App **build** | ⏸ 可选补跑 | 与 HIL 分开：有 IDF 可另补零错误 build |
| ESP32 **HIL**（按键 / OLED / 舵机） | ⏸ **未验证 HIL** | Owner 缺 OLED、舵机等外设；**明确不阻塞合入** |
| `pal_hal.h` 进 Binary SDK 包 | ⚠ 治理缺口 | packer 仍 auto-scan；与 ADR-0028「excluded」声明冲突 — 另跟，不扩本功能范围 |

---

## 3. 明确不冒充的项

1. **不得**以 wasm / host 通过代替 ESP32 电气行为（上下拉、LEDC bits/`REF_TICK`）。  
2. **不得**以 ESP32 **build** 通过代替 HIL。  
3. HIL 补测建议（有外设后）：`devkitc_smoke` 板载键；`oled_dashboard`；`avoidance_car` 舵机角度观测。

---

## 4. 残余风险（可接受）

| 风险 | 等级 | 处置 |
|------|------|------|
| ESP32 LEDC + REF_TICK 未上板 | 中 | 记 HIL backlog；默认路径仍为 13-bit + AUTO（与今日兼容） |
| wasm Binary SDK 未 pack | 低 | host Binary 已验；wasm pack 独立补 |
| `pal_hal.h` 误进公共包 | 低/治理 | 单独修订 ADR-0028 或 packer 白名单 |

---

## 5. 结论与后续

- **实施计划 Task 0–8：** 完成。  
- **Task 9：** 软件 + wasm 验收完成；HIL / wasm Binary / ESP32 build 记延迟项。  
- **计划状态：** ✅ 已完成（Accept with deferred HIL）。  
- **后续（非本功能阻塞）：** HIL 抽测；wasm Binary pack；`pal_hal.h` 打包治理。

---

*本记录为时间点快照；归档后以追加新评审文件方式修订，不改写设计期评审正文。*

