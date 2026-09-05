# 静态分发代码模板

> 复制即用。目标形态（`wink_status_t`），非旧代码的 `bool`/`float` 形态。

---

## 形态 1：DAL POD 器件 + 命名 API（主形态）

### dal_xxx.h

```c
#ifndef DAL_XXX_H
#define DAL_XXX_H

#include "wink_status.h"
#include <stdint.h>
#include <stdbool.h>

/* POD 器件结构：无函数指针、无父类嵌入 */
typedef struct {
    uint16_t pin_a;        /* 物理引脚（codegen 从拓扑填入） */
    uint16_t pin_b;
    float    last_value;   /* 缓存的最近读数 */
} dal_xxx_t;

/* 命名式 API：返回 wink_status_t（0=ok，负数=错误） */
wink_status_t dal_xxx_init(dal_xxx_t *dev);
wink_status_t dal_xxx_read(dal_xxx_t *dev, float *out_value);
wink_status_t dal_xxx_deinit(dal_xxx_t *dev);

#endif /* DAL_XXX_H */
```

### dal_xxx.c

```c
#include "dal_xxx.h"
#include "pal_hal.h"          /* 直接调 PAL，静态分发，无虚表 */

wink_status_t dal_xxx_read(dal_xxx_t *dev, float *out_value)
{
    if (dev == NULL || out_value == NULL) {
        return WINK_ERR_INVALID_PARAM;       /* 外部输入 → 运行时检查 */
    }

    bool hi = pal_gpio_read(dev->pin_a);     /* PAL 命名 API，直调 */
    /* ... 物理量换算 ... */
    float value = hi ? 1.0f : 0.0f;
    dev->last_value = value;
    *out_value = value;
    return WINK_OK;
}
```

> ⚠ **目标形态 vs 现状偏差提示**：
> > 模板展示的是 ADR 目标形态：`wink_status_t` 返回值 + 扁平 POD。
> > wink-micro-os 实际代码：
> > - ✅ POD 结构已到位（无 ops / 无虚表）
> > - ⏳ 返回类型迁移中：部分 API 仍用 `bool`/`float`（见 [evolution.md](./evolution.md) §1）
> > - ⏳ 字段布局待迁移：现状 `dev->last_distance`（扁平），目标 `dev->state.last_distance`
> >   （const 配置区 + state 可变区分离，见 [evolution.md](./evolution.md) §1.4）
> >
> > 写新代码请用目标形态；改旧代码请按 evolution.md 渐进迁移。

---

## 形态 2：device_tree.c / .h（codegen 静态绑定）

### device_tree.h（codegen 生成）

```c
#ifndef DEVICE_TREE_H
#define DEVICE_TREE_H
#include "dal_ultrasonic.h"
#include "dal_rc_servo.h"

/* 只暴露逻辑实例，绝不暴露 SDK 类型 */
extern dal_ultrasonic_t front_radar;
extern dal_rc_servo_t      neck_servo;
#endif
```

### device_tree.c（codegen 生成 · 编译期绑定配置与 API）

```c
#include "device_tree.h"

dal_ultrasonic_t front_radar = { .trig_pin = 4, .echo_pin = 5, .last_distance = 0.0f };
dal_rc_servo_t      neck_servo  = { .pwm_channel = 0, .current_angle = 90.0f,
                                 .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };
```

应用层（App）只拿逻辑实例名，调命名 API：

```c
#include "device_tree.h"
float dist;
if (dal_ultrasonic_read(&front_radar, &dist) == WINK_OK) { /* ... */ }
```

> ⚠ 仓库尚无此文件（codegen 设计态）。实例名须过 C 标识符净化 + 唯一性校验
> （见 `07-platform-governance/01-device-model-registry.md` §7）。

---

## 形态 3：平台文件切换（chigo-micro platform 风格）

### platform.h（接口，不变，无 struct）

```c
#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>
#include <stdbool.h>

void platform_gpio_write(uint16_t pin, bool value);
bool platform_gpio_read(uint16_t pin);
void platform_delay_ms(uint32_t ms);
/* ... 一组命名自由函数，无实例结构 ... */
#endif
```

### CMake 二选一链接

```cmake
# 真机 target —— components/platform/CMakeLists.txt
idf_component_register(SRCS "platform_esp32.c" INCLUDE_DIRS ".")

# 仿真 target —— sim/CMakeLists.txt
# 显式排除 platform_esp32.c，改链 platform_sim.c
set(SIM_SRC ... sim/platform_sim.c ...)
```

```c
/* platform_esp32.c（真机） */
void platform_gpio_write(uint16_t pin, bool value) { gpio_set_level(pin, value); }

/* sim/platform_sim.c（PC 仿真，替换 platform_esp32.c） */
void platform_gpio_write(uint16_t pin, bool value) { printf("[sim] pin %u = %d\n", pin, value); }
```

> 换平台 = 换编译哪个 `.c`，`platform.h` 接口与上层零修改。

---

## 形态 4：局部策略 vtable（受控例外 · control_algo）

> **仅限「算法策略切换」，不可用于器件抽象。** 这是 ADR-0004 局部多态化的合法用法。

### control_algo.h

```c
#ifndef CONTROL_ALGO_H
#define CONTROL_ALGO_H
#include <stdbool.h>

/* 策略 vtable —— 局部、封装在控制层内部 */
typedef struct {
    void *(*create)(void);
    void  (*destroy)(void *inst);
    void  (*update)(void *inst, float *outputs, const float *setpoints);
    void  (*reset)(void *inst);
    bool  (*set_param)(void *inst, int param_id, float value);
} control_algo_t;

/* 句柄：ops 是 const，构造期绑定 */
typedef struct {
    const control_algo_t *algo;
    void *instance;
} control_handle_t;

static inline bool control_handle_update(control_handle_t *h,
                                         float *out, const float *setpt) {
    h->algo->update(h->instance, out, setpt);   /* 仅这一处间接调用，且只在策略层 */
    return true;
}
#endif
```

具体策略（如 `algo_pid.c`）填这张表；App 看到的是静态命名接口，多态被封装在控制层内部。

---

## 形态 5：X-Macros 模式（批量生成与统一迭代）

在没有运行期对象数组的情况下，若需要对同类型的多个全局外设进行批量操作（如统一初始化、统一低功耗睡眠），推荐使用 **X-Macros 模式**。这保持了 C 语言零运行时开销的优势，且能在编译期展开：

### 头文件声明 (例如 `device_tree.h`)

```c
#ifndef DEVICE_TREE_H
#define DEVICE_TREE_H

#include "dal_ultrasonic.h"

/* 定义设备树 X-Macro 元表 */
#define ULTRASONIC_DEVICES(X) \
    X(front_radar) \
    X(back_radar)  \
    X(side_radar)

/* 使用 X-Macro 自动展开 extern 外部声明 */
#define X_DECLARE(name) extern dal_ultrasonic_t name;
ULTRASONIC_DEVICES(X_DECLARE)
#undef X_DECLARE

#endif
```

### 源文件实现 (例如 `device_tree.c`)

```c
#include "device_tree.h"

/* 实例化所有雷达设备 */
dal_ultrasonic_t front_radar = { .trig_pin = 4, .echo_pin = 5 };
dal_ultrasonic_t back_radar  = { .trig_pin = 6, .echo_pin = 7 };
dal_ultrasonic_t side_radar  = { .trig_pin = 8, .echo_pin = 9 };

/* 批量初始化所有超声波传感器 */
wink_status_t device_tree_init_ultrasonics(void) {
    wink_status_t status;
    
    #define X_INIT(name) \
        status = dal_ultrasonic_init(&name); \
        if (status < 0) { \
            return status; /* 链式报错中断 */ \
        }
        
    ULTRASONIC_DEVICES(X_INIT)
    #undef X_INIT
    
    return WINK_OK;
}
```

---

## 形态 6：静态 Observer 模式（解耦事件通知）

静态分发下**禁止**使用动态分配链表来注册观察者（Observer）。对于外设事件发生时通知上层业务（App），推荐使用编译期绑定的**静态回调**：

### 驱动头文件 (例如 `dal_button.h`)

```c
#ifndef DAL_BUTTON_H
#define DAL_BUTTON_H

#include "wink_status.h"
#include <stdint.h>

typedef struct {
    const uint16_t pin;
} dal_button_t;

/* 声明全局静态回调函数，由应用层在 device_tree.c 或业务层实现 */
extern void dal_button_on_press(dal_button_t *dev);

#endif
```

### 业务层/设备树实现绑定 (例如 `device_tree.c`)

```c
#include "dal_button.h"
#include "app_control.h"

extern dal_button_t emergency_stop_btn;
extern dal_button_t mode_switch_btn;

/* 静态分发的回调接口：用 if-else / switch 代替动态回调指针 */
void dal_button_on_press(dal_button_t *dev) {
    if (dev == &emergency_stop_btn) {
        app_emergency_halt(); // 静态直调
    } else if (dev == &mode_switch_btn) {
        app_toggle_work_mode();
    }
}
```

---

## PAL 契约参考（wink-micro-os）

`pal_hal.h`：`pal_gpio_init/write/read/enable_interrupt`、`pal_pwm_init/set_duty`、
`pal_i2c_transfer(port, addr, wbuf, wlen, rbuf, rlen)`。
`pal_osal.h`：`pal_delay_ms/us`、`pal_get_ms/us`、`pal_mutex_t` + create/lock/unlock/destroy。
每平台在 `targets/<arch>/pal_hal_*.c` 实现；wasm target 把每个 PAL 函数转发到 `js_pal_*` 导入。

---

## 编译期断言（static_assert）

> 与本项目「编译期静态分发」哲学一致——零运行时开销，**编译期就红**。双 target 共用：wasm 与
> xtensa 若对布局/枚举有分歧，编译期立刻暴露，比运行时崩好定位。

```c
#include <assert.h>   /* C11: static_assert / _Static_assert */

/* 锁结构体大小——换编译器/平台时暴露对齐/位域漂移（双 target 护栏） */
static_assert(sizeof(dal_rc_servo_t) == 12u, "dal_rc_servo_t layout drifted");

/* 锁枚举与查找表大小一致（错误码表常漏增项） */
static_assert(ARRAY_SIZE(k_error_table) == WINK_ERR_COUNT,
              "error table out of sync with enum");

/* 锁数组边界 / 协议字段宽度 */
static_assert(NUM_JOINTS <= MAX_JOINTS, "joint count exceeds capacity");
static_assert(CMD_FRAME_MAX_LEN <= UINT16_MAX, "len field too narrow");
```

要点：用它锁**结构体大小**（检测位域/对齐漂移）、**枚举与表大小一致**、**数组边界**、**协议字段宽度**。
失败消息必写——`static_assert(cond, "why")`。

---

## 字节序 marshal / unmarshal（双 target + 协议帧）

ADR-0002 禁跨 target 位域与 `#pragma pack`，故协议帧 / 持久存储 / Wasm↔host 边界一律用
**显式字节级 marshal**，不靠结构体直接 `memcpy`（布局依赖编译器/平台）：

```c
/* 显式小端 marshal/unmarshal——禁位域、禁 #pragma pack（ADR-0002）。
 * 字节流布局由代码显式控制，wasm/xtensa 行为一致。 */
static inline void pack_u16_le(uint8_t *buf, uint16_t v) {
    buf[0] = (uint8_t)(v & 0xFFu);
    buf[1] = (uint8_t)((v >> 8) & 0xFFu);
}
static inline uint16_t unpack_u16_le(const uint8_t *buf) {
    return (uint16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
}
/* u32 同理：4 字节移位 + 掩码。CRC16 计算也走字节级，不依赖宿主字节序。 */
```

要点：移位用无符号、掩码到位宽（BARR-C 位操作安全，[../../../_embedded-shared/clean-code.md](../../../_embedded-shared/clean-code.md)）；
这套 helper 属共享层 → host 单测覆盖（[../../../_embedded-shared/testing.md](../../../_embedded-shared/testing.md)）。

---

## 模板检查清单

- [ ] 器件结构是纯 POD（无函数指针 / 无 `ops` / 无父类嵌入）
- [ ] 公共 API 返回 `wink_status_t`（**不是** `bool`/`float`）
- [ ] 调 PAL 用命名 API 直调（无虚表）
- [ ] 外部输入运行时校验 + 错误码；内部契约用断言
- [ ] device_tree 实例名过 C 标识符净化 + 唯一
- [ ] 平台切换走 CMake 文件链接，不改 `platform.h`
- [ ] 若用 vtable（形态 4），仅限策略层、ops 是 const、不暴露给器件抽象
- [ ] 关键结构体大小 / 枚举与表一致性 / 数组边界用 `static_assert` 锁住
- [ ] 跨 target / 跨边界数据用显式 marshal（无位域 / 无 `#pragma pack`）
