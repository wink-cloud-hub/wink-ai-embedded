# 01 - 四层架构详解

> 源代码：[oop-in-c/code/15-platform/](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/15-platform/)

---

## 四层架构定义

```
①  应用层    led_on / i2c_transfer
②  抽象层    LedOps / file_operations / i2c_algorithm
③  实现层    gpio_on / ext4_read / rk3x_i2c_xfer
④  注册层    main / module_init / module_platform_driver
```

---

## 第一层：应用层

### 职责

业务逻辑，只拿 `LedBase*` 句柄，不知道底层是什么硬件。

### 代码示例

```c
/* ===== app.c ===== */
#include "leds.h"    /* 只包含父类头文件 */

void alarm_blink(void)
{
    led_on(g_led_error);     /* 可能是 GPIO LED */
    led_off(g_led_error);
}

void status_indicate(int err_code)
{
    if (err_code == 0)
        led_on(g_led_status);     /* 可能是 PWM LED */
    else
        led_on(g_led_error);      /* 可能是 GPIO LED */
}
```

### 特点

- 只知道 `struct led_base *`
- 不知道 `led_gpio` / `led_pwm` / `led_i2c` 的存在
- 换硬件？改 `led_board_init.c`，这里一行不动

---

## 第二层：抽象层

### 职责

定义「接口长什么样」，统一 dispatch 机制。

### struct 定义

```c
/* ===== led_base.h ===== */

/* ops 表 ≡ C++ vtable */
struct led_ops {
    int (*on)            (struct led_base *me);           /* 必填 */
    int (*off)           (struct led_base *me);           /* 必填 */
    int (*set_brightness)(struct led_base *me, uint8_t brightness); /* 选填 */
};

/* 父类 struct ≡ C++ base class */
struct led_base {
    const struct led_ops *ops;    /* 第一个字段！ops指针 ≡ vptr */
    const char           *name;
    bool                  is_on;
};
```

### 统一接口（dispatch）

```c
/* ===== led_base.c ===== */

/* 多态 dispatch — 一行代码完成分发 */
int led_on(struct led_base *me)
{
    assert(me && me->ops && me->ops->on);
    return me->ops->on(me);     /* base->ops->on(base) 两次跳转 */
}

int led_off(struct led_base *me)
{
    assert(me && me->ops && me->ops->off);
    return me->ops->off(me);
}

/* 选填函数：有实现就调，没有提供默认行为 */
int led_set_brightness(struct led_base *me, uint8_t brightness)
{
    if (me->ops->set_brightness)
        return me->ops->set_brightness(me, brightness);
    /* 默认实现：没有 set_brightness 就用 on/off 模拟 */
    if (brightness > 0) return led_on(me);
    else                return led_off(me);
}
```

### 核心机制：调用链拆解

```
led_on(g_led_error)           ← 应用层调统一接口
    │
    └→ me->ops->on(me)        ← 两次指针跳转
         │    │    │
         │    │    └── 传入自身 me 指针
         │    └── 查 ops 表里的 on 函数指针
         └── me 是 LedBase*，ops 指向具体实现

如果 ops 指向 gpio_ops → 跳到 gpio_on()
如果 ops 指向 pwm_ops  → 跳到 pwm_on()
```

### 为什么 ops 放第一个字段？

```c
struct led_gpio {
    struct led_base base;   /* ← 放第一个字段 */
    uint8_t pin;
};

/* C99 保证：struct 首字段地址 == struct 本身地址 */
/* &led_gpio.base == (void*)&led_gpio */
/* 向上转型零开销 */
```

---

## 第三层：实现层

### 职责

填 ops 表，操作具体硬件，用 `container_of` 反推子类。

### container_of 宏

```c
/* 从父类指针反推子类指针 */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
```

### GPIO 实现

```c
/* ===== led_gpio.h ===== */
struct led_gpio {
    struct led_base base;       /* 父类，放第一个字段 */
    uint8_t         pin;
    bool            on_level;
};

/* ===== led_gpio.c ===== */
static int gpio_on(struct led_base *me)
{
    struct led_gpio *self = container_of(me, struct led_gpio, base);
    platform_gpio_write(self->pin, self->on_level);
    me->is_on = true;
    return 0;
}

static const struct led_ops gpio_ops = {
    .on  = gpio_on,
    .off = gpio_off,
    /* .set_brightness = NULL — 不填，用父类默认 */
};

int led_gpio_init(struct led_gpio *me, const char *name,
                  uint8_t pin, bool on_level)
{
    led_base_init(&me->base, name, &gpio_ops);
    me->pin      = pin;
    me->on_level = on_level;
    platform_gpio_init(pin, GPIO_MODE_OUTPUT);
    return 0;
}
```

### PWM 实现

```c
/* ===== led_pwm.h ===== */
struct led_pwm {
    struct led_base base;
    uint8_t channel;
    uint8_t duty;
};

/* ===== led_pwm.c ===== */
static int pwm_on(struct led_base *me)
{
    struct led_pwm *self = container_of(me, struct led_pwm, base);
    platform_pwm_enable(self->channel);
    platform_pwm_set_duty(self->channel,
                          (uint8_t)((uint32_t)self->duty * 255U / 100U));
    me->is_on = true;
    return 0;
}

static const struct led_ops pwm_ops = {
    .on             = pwm_on,
    .off            = pwm_off,
    .set_brightness = pwm_set_brightness,  /* PWM 支持 */
};
```

### I2C 实现

```c
/* ===== led_i2c.h ===== */
struct led_i2c {
    struct led_base            base;
    struct platform_i2c_client client;
    uint8_t                    reg;
};

/* ===== led_i2c.c ===== */
static int i2c_on(struct led_base *me)
{
    struct led_i2c *self = container_of(me, struct led_i2c, base);
    uint8_t val = 0x01;
    platform_i2c_write(&self->client, self->reg, &val, 1);
    me->is_on = true;
    return 0;
}
```

---

## 第四层：注册层

### 职责

构造子类对象，向上转型，导出句柄给应用层。

### 板级初始化

```c
/* ===== led_board_init.c ===== */
#include "led_gpio.h"
#include "led_pwm.h"
#include "led_i2c.h"

/* 子类对象：static，外部不可见 */
static struct led_gpio s_led_err;
static struct led_pwm  s_led_status;
static struct led_i2c  s_led_net;

/* 全局句柄：应用层只拿到父类指针 */
struct led_base *g_led_error;
struct led_base *g_led_status;
struct led_base *g_led_network;

int led_board_init(void)
{
    /* 构造子类对象：填 ops + 硬件参数 */
    led_gpio_init(&s_led_err,    "ERR",  10, true);
    led_pwm_init (&s_led_status, "STAT", 1,  50);
    led_i2c_init (&s_led_net,    "NET",  i2c_bus, 0x3C, 0x00);

    /* 向上转型：子类指针 → 父类指针 */
    g_led_error   = &s_led_err.base;
    g_led_status  = &s_led_status.base;
    g_led_network = &s_led_net.base;

    return 0;
}
```

### 头文件暴露策略

```c
/* ===== leds.h（给应用层用） ===== */
#include "led_base.h"

/* 只暴露父类指针，不暴露子类定义 */
extern struct led_base *g_led_error;
extern struct led_base *g_led_status;
extern struct led_base *g_led_network;

/* 应用层 #include "leds.h"
 * → 只知道 struct led_base
 * → 不知道 led_gpio / led_pwm / led_i2c 的存在
 */
```

### 链接自动注册

```c
/* ===== initcall.h ===== */
typedef int (*initcall_t)(void);

#define MODULE_INIT(fn)                                         \
    static initcall_t __initcall_##fn                           \
        __attribute__((used, section("my_initcall"))) = fn

/* ===== startup.c ===== */
extern initcall_t __start_my_initcall[];
extern initcall_t __stop_my_initcall[];

void do_initcalls(void)
{
    for (initcall_t *fn = __start_my_initcall;
         fn < __stop_my_initcall; fn++)
        (*fn)();
}

/* ===== main.c ===== */
int main(void)
{
    do_initcalls();    /* 不知道有哪些 init，但都会被调到 */
    while (1) { /* 业务循环 */ }
}

/* ===== drv_led.c ===== */
static int led_init(void)
{
    led_board_init();
    return 0;
}
MODULE_INIT(led_init);   /* 一行宏，自动注册，main 零引用 */
```

---

## Platform 层：工程实践层

> 不在 4 层内，ch15 专门讲。

### 职责

硬件抽象，隔离芯片差异。

### 接口定义

```c
/* ===== platform.h ===== */
void platform_gpio_init(uint8_t pin, uint8_t mode);
void platform_gpio_write(uint8_t pin, bool value);

void platform_pwm_init(int32_t channel);
void platform_pwm_enable(int32_t channel);
void platform_pwm_set_duty(int32_t channel, uint8_t duty);

int  platform_i2c_write(struct platform_i2c_client *client,
                         uint8_t reg, uint8_t *data, size_t len);
int  platform_i2c_read(struct platform_i2c_client *client,
                        uint8_t reg, uint8_t *buf, size_t len);
```

### 芯片实现

```c
/* ===== platform_stm32f1xx.c ===== */
void platform_gpio_write(uint8_t pin, bool value)
{
    HAL_GPIO_WritePin(GPIO_PORT(pin), GPIO_PIN(pin), value);
}

/* ===== platform_esp32.c ===== */
void platform_gpio_write(uint8_t pin, bool value)
{
    gpio_set_level(pin, value);
}
```

**换芯片只换 `platform_*.c` 文件，上面所有层零修改。**

---

## 层间衔接：完整调用链（从应用到硬件）

```
启动阶段（注册层）:
led_board_init()                    ← 启动时调用（或 MODULE_INIT 自动调用）
  │
  ├── led_gpio_init(&s_led_err, ...)   构造子类对象
  │      ├── led_base_init(&me->base, ..., &gpio_ops)  填 ops 表
  │      └── me->pin = 10, me->on_level = true          填硬件参数
  │
  └── g_led_error = &s_led_err.base    向上转型：子类指针 → 父类指针

运行阶段调用:
应用层调:  led_on(g_led_error)
              │
              │  g_led_error 是 struct led_base * 类型
              │  指向 s_led_err 的 base 字段
              ▼
抽象层:    me->ops->on(me)
              │         │
              │         └── me = g_led_error (父类指针)
              └── ops 指向 gpio_ops 表
                   │
                   │  gpio_ops.on 存的是 gpio_on 函数地址
                   ▼
实现层:    gpio_on(me)
              │
              │  container_of(me, struct led_gpio, base)
              │  = 从 base 指针反推 led_gpio* 指针
              │  = base地址 - offsetof(led_gpio, base)
              │  = 一条减法指令，零开销
              ▼
           platform_gpio_write(self->pin, self->on_level)
              │
              │  调 Platform 层抽象接口
              ▼
Platform层: HAL_GPIO_WritePin(...)   ← 具体芯片的寄存器操作
```

### 编译依赖关系

```
app.c
  │  #include "leds.h"  → 只看 LedBase*，不看子类
  ▼
leds.h
  │  #include "led_base.h"  → 暴露父类指针
  ▼
led_base.h  ← 抽象层定义，所有层都依赖它
  ▲
  ├─ led_gpio.c  (实现子类，填 ops 表)
  ├─ led_pwm.c
  ├─ led_i2c.c
  └─ led_board_init.c  (构造对象，向上转型)
        │
        ▼  #include "platform.h"
     platform.h
        ▲
        └─ platform_stm32f1xx.c  (芯片实现)
```

### 运行调用链

```
启动时:  main() → do_initcalls() → led_board_init()
  │                           │
  │                           └── 构造对象 + 向上转型 + 导出 g_led_xxx
  │
  └── while(1) 业务循环
        │
        └── app.c 调 led_on(g_led_error)
                │
                ▼
             led_base.c: me->ops->on(me)  ← dispatch
                │
                ▼
             led_gpio.c: gpio_on()  ← 具体实现
                │
                ▼
             platform_stm32f1xx.c: HAL_GPIO_WritePin()  ← 硬件
```

---

## 层职责表

| 层 | 文件 | 职责 | 换什么只动这层 |
|---|------|------|----------------|
| 应用层 | app.c | 业务逻辑 | 加新功能 |
| 抽象层 | led_base.c | 定义接口 + dispatch | 不动 |
| 实现层 | led_gpio.c | 填 ops + 操作硬件 | 换外设/换电机 |
| 注册层 | led_board_init.c | 构造对象 + 向上转型 | 换硬件配置 |
| Platform层 | platform_*.c | 硬件抽象 | 换芯片 |

**应用层 `app.c` 永远不动。**

---

## 文件目录结构

> 引用：[`oop-in-c/code/15-platform/pc/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/15-platform/pc/)

```
oop-in-c/code/15-platform/pc/
├── led_base.h            ← 抽象层：父类 struct + ops 表定义
├── led_base.c            ← 抽象层：统一接口 + dispatch
├── led_gpio.h            ← 实现层：GPIO 子类 struct
├── led_gpio.c            ← 实现层：gpio_ops + gpio_on/gpio_off
├── led_pwm.h             ← 实现层：PWM 子类 struct
├── led_pwm.c             ← 实现层：pwm_ops + pwm_on/pwm_off
├── led_i2c.h             ← 实现层：I2C 子类 struct
├── led_i2c.c             ← 实现层：i2c_ops + i2c_on/i2c_off
├── led_board_init.c      ← 注册层：构造对象 + 向上转型 + 导出句柄
├── leds.h                ← 应用接口：只暴露 LedBase* 句柄
├── platform.h            ← Platform 层：硬件抽象接口
├── platform_stm32f1xx.c  ← Platform 层：STM32F1 实现
├── platform_esp32.c      ← Platform 层：ESP32 实现
├── app.c                 ← 应用层：业务逻辑
└── Makefile
```

---

## 目标 → 机制映射

| 目标                | 对应机制               | 换的时候改什么文件                 |
|---------------------|------------------------|------------------------------------|
| 换主控芯片，业务零改 | Platform 层隔离        | 只换 `platform_*.c`                |
| 换 LED 驱动方式     | 实现层 + ops 表        | 只换 `led_board_init.c` 里的绑定   |
| 加新 LED 类型       | 实现层加文件           | 加 `led_xxx.h` + `led_xxx.c`       |
| 加新外设驱动        | 注册层链接自动收集     | 驱动文件加一行 `MODULE_INIT`        |
| 换整块主板方案      | 注册层 + Platform 层   | 换 `platform_*.c` + `led_board_init.c` |
