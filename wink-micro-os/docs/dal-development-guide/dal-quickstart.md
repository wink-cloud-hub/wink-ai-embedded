# DAL 快速上手

目标：用**已有** DAL 驱动跑通一个最小 App（声明设备 → codegen → 业务回调调用语义 API）。  
若要**新增一种外设类型**，请改读 [adding-peripheral.md](./adding-peripheral.md)。

---

## 1. 心智模型（一分钟）

```text
wink-app.json  ──codegen──►  device_tree.*（全局 POD 实例）
App / Role     ──调用──►  {instance}_{verb}（Role 包装）或 dal_<type>_*（DAL API，无虚表）
DAL / BAL      ──调用──►  pal_*（真机）或 Wasm Bridge / Channel 1~4（仿真旁路，WASM Simulation 3.0）
```

- App **不**直接玩 GPIO/PWM 时序；只调 Role 动词或 `dal_*`。
- 引脚与通道写在 JSON，换板改配置，不改业务调用点。
- 范式：POD + 静态命名函数（[ADR-0004](../../../docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md)）。
- 仿真对齐：DAL 支持语义 Bypass（WASM Bridge Channel 4）或物理 GPIO/PWM 通道仿真，架构详见 [Wasm 仿真 3.0 SSOT](../../../docs/design/04-wasm-simulation-3.0/00-README.md)。

---

## 2. 最小路径

### 2.1 声明设备

在 `wink-micro-app/<your_app>/wink-app.json` 增加实例。`type` 必须与 codegen 插件 `DriverBase.type` **逐字一致**（例：`dc_motor`、`ultrasonic`、`rc_servo`）。

```json
{
  "app_name": "avoidance_car",
  "board": "esp32_devkitc",
  "devices": {
    "left_motor": {
      "type": "dc_motor",
      "pwm_channel": 0,
      "dir_pin_a": 18,
      "dir_pin_b": 19
    }
  }
}
```

字段约定见 [`../wink-app-json-guide.md`](../wink-app-json-guide.md)（引脚名须以 `_pin` 结尾等）。  
实例**必有** `type`（驱动平面）；可选 `role`（App 侧 Role Interface，缺省 `default_role`；**非 BAL**）。  
`variant` / `enable_pin` / `driver_ic` **不是**全外设通用字段。字段分层与 **type vs role** 见 [dal-best-practices.md §3.0](./dal-best-practices.md)。

App 推荐优先调 codegen 生成的 `{instance}_{verb}`（由 `role` 决定），复杂场景再直接调 `dal_*` 或 BAL（逃生 / 算法）。

### 2.2 生成与编译

```text
python wink-tools/wink.py build host --app <your_app>
```

有 `wink-app.json` 时，仅声明到的类型会 `WINK_USE_*=ON`，其余驱动走 stub。

### 2.3 在业务里调用

生成头（如 `device_tree.h`）暴露全局实例。在 `app_callbacks.c`（或等价入口）中：

```c
#include "device_tree.h"
#include "actuator/dal_dc_motor.h"
#include "sensor/dal_ultrasonic.h"

/* A 类执行器命令：全 Profile 统一定标整数（千分比 promille ‰，500 代表 50% 速度） */
WINK_IGNORE_UNUSED(dal_dc_motor_set_speed(&left_motor, 500));

/* B 类传感器测量：32 位 Full Profile 下公开 API 保留并使用 float */
float distance_cm = 0.0f;
WINK_IGNORE_UNUSED(dal_ultrasonic_get_distance_cm(&front_sonar, &distance_cm));
```

> 💡 **量纲与 float 适用边界（[ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)）**：
> - **A 类（执行器命令，如速度 `speed_promille`、占空比 `duty_promille`、舵机角度 `angle_ddeg`）**：硬件终态为离散整数。全 Profile 统一定标整数（传 `500` ‰ 或 `900` ddeg），彻底消除 8位/32位 之间的软浮点开销与寄存器转换。
> - **B 类（传感器测量，如距离 `distance_cm`、温度 `temp_degc`）**：在 32 位 Full Profile (ESP32/WASM) 下公开 API 保留并推荐使用 `float`；在 8 位 Micro Profile (8051) 下退化为定点整型（如 `distance_mm`）。
> - **BAL（物理算法层，如 PID / 滤波）**：数学计算域（Math Domain）完全支持使用 `float`，算法解算完成后转换成 A 类定标整数给 DAL 入参。

具体 API 以对应 `dal/include/.../dal_*.h` 为准。参考应用：`wink-micro-app/avoidance_car/`。

### 2.4 安全关断

执行器若注册了 `safe_off`，故障/反初始化路径会调用它。  
有刷直流电机（ADR-0048）：`dal_dc_motor_safe_off` → **brake**（非 coast）。单方向脚无法制动时返回 `WINK_ERR_UNSUPPORTED`。

---

## 3. 常见「第一次」检查清单

- [ ] `type` 与 registry / 仿真 Manifest 一致  
- [ ] 引脚字段名符合 `*_pin`  
- [ ] 改 JSON 后重新 configure / build  
- [ ] 业务只 `#include` 需要的 `dal_*.h`，不直接依赖 `pal_*`（除既有豁免）  
- [ ] 失败路径检查 `wink_status_t`（0 成功，负数为错）

---

## 4. 下一步

| 需求 | 文档 |
|------|------|
| JSON 字段与板级 | [`../wink-app-json-guide.md`](../wink-app-json-guide.md) |
| 新增 `dal_*` 类型 | [adding-peripheral.md](./adding-peripheral.md) |
| `type` / `role` / 字段分层 | [dal-best-practices.md §3.0](./dal-best-practices.md) |
| 给驱动挂 Role（codegen） | [role-interface-codegen.md](./role-interface-codegen.md) |
| 拓扑 / 芯片扩展与 `HAS_*` 裁剪 | [dal-best-practices.md §3](./dal-best-practices.md) |
| A/B 量纲与定标整数规范 | [ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md) |
| Wasm 仿真 3.0 SSOT | [`04-wasm-simulation-3.0/`](../../../docs/design/04-wasm-simulation-3.0/00-README.md) |
| Role 动词与生成 API（SSOT） | [`01-app-business-logic.md`](../../../docs/design/03-app-codegen/01-app-business-logic.md) |
| 架构全文 | [`01-dal-device-abstraction.md`](../../../docs/design/02-wink-micro-os/01-dal-device-abstraction.md) |
| App 踩坑 | [`../app-coding-gotchas.md`](../app-coding-gotchas.md) |
