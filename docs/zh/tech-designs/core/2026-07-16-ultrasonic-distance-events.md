# 超声波测距完成事件技术设计规格

| 项 | 内容 |
|---|---|
| **文档版本** | v1.0 |
| **设计日期** | 2026-07-16 |
| **状态** | ✅ **Implemented**（host MVP；ESP32 HIL / FAULT 事件后置） |
| **关联 ADR** | [ADR-0033](../../decisions/core/0033-ultrasonic-distance-events.md) |
| **关联实施计划** | [2026-07-16-ultrasonic-distance-events-plan.md](../../implementation-plans/core/2026-07-16-ultrasonic-distance-events-plan.md) |
| **关联样板** | `wink-micro-app/avoidance_car`（目标 L1） |

---

## 1. 目标

```c
WINK_TRY(front_radar_enable_distance_events());  /* JSON auto_poll_ms */

void app_on_event(const wink_event_t *evt) {
    if (evt->device == &front_radar &&
        evt->type == WINK_EVENT_DISTANCE_READY) {
        float cm = evt->param / 10.0f;
        /* … */
    }
}
```

持续检测 = 周期测距 + **每次完成一次** `DISTANCE_READY`。

---

## 2. 时序（MVP soft_poll）

```text
enable_distance_events(period_ms>=50)
        │
        ▼
   periodic tick ──► phase NEED_TRIGGER
        │                 │
        │                 ▼
        │          request_measurement
        │                 │
        │                 ▼
        │            phase WAITING
        │                 │
        │          get_cached_distance
        │            ├─ BUSY → 等下一次 tick
        │            ├─ OK   → post DISTANCE_READY(param=cm*10)
        │            │         → phase NEED_TRIGGER
        │            └─ else → 回 NEED_TRIGGER（MVP 不投 FAULT）
        ▼
disable → stop periodic，清空 slot
```

完成边沿：仅在 `get_cached == OK` 时 post 一次，然后回到 NEED_TRIGGER；禁止对同一未更新缓存在 WAITING 内重复灌队列。

---

## 3. API

```c
typedef struct {
    uint32_t period_ms;
} wink_ultrasonic_distance_event_config_t;

wink_status_t wink_ultrasonic_enable_distance_events(
    dal_ultrasonic_t *dev,
    const wink_ultrasonic_distance_event_config_t *cfg);

void wink_ultrasonic_disable_distance_events(dal_ultrasonic_t *dev);

bool wink_ultrasonic_distance_events_is_enabled(const dal_ultrasonic_t *dev);
```

互斥：`enable` 时若 `wink_sonar_helper_is_running(dev)` → `INVALID_STATE`；`sonar_helper_start` 时若 distance-events enabled → `INVALID_STATE`。

---

## 4. 事件载荷

| 字段 | 值 |
|------|-----|
| `type` | `WINK_EVENT_DISTANCE_READY` |
| `device` | `&dev` |
| `param` | `(uint32_t)(distance_cm * 10.0f + 0.5f)` |
| `timestamp` | `pal_os_get_ms()` |

队列满：`WINK_WARN_DISTANCE_EVENT_QUEUE_FULL`（9401，挂在 ultrasonic 段）。

---

## 5. Codegen

- Role：`enable_distance_events` / `disable_distance_events` + 既有 C 类读距动词。  
- Header：`wink_ultrasonic_distance_events.h`。  
- `auto_poll_ms` 缺省 50；`< 50` → codegen ERROR。  
- Servo Role：`set_angle(float)` → `dal_servo_set_angle`（void 包装，L1）。

---

## 6. 非目标（本规格）

- `DISTANCE_FAULT` 投递  
- 超声波完成 IRQ / RMT 回调直 post  
- 合并删除 `wink_sonar_helper`

