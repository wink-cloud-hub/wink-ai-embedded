# 03 - 代码模板

> 完整可运行骨架，复制即用。

---

## 最小可运行骨架

### 目录结构

```
project/
├── xxx_base.h          # 父类定义
├── xxx_base.c          # 父类实现
├── xxx_subtype.h       # 子类定义
├── xxx_subtype.c       # 子类实现
├── xxx_board_init.c    # 板级初始化
├── xxx.h               # 应用接口
├── app.c               # 应用层
├── platform.h          # 硬件抽象
└── platform_pc.c       # PC 模拟实现
```

---

## 父类模板

### xxx_base.h

```c
#ifndef _XXX_BASE_H_
#define _XXX_BASE_H_

#include <stdbool.h>
#include <stddef.h>

/* ops 表 ≡ C++ vtable */
struct xxx_ops {
    int (*doSomething)(struct xxx_base *me);
    int (*reset)(struct xxx_base *me);
};

/* 父类 struct */
struct xxx_base {
    const struct xxx_ops *ops;    /* 第一个字段 */
    const char           *name;
    bool                  is_initialized;
};

/* 父类接口 */
int xxx_base_init(struct xxx_base *me, const char *name,
                  const struct xxx_ops *ops);
int xxx_doSomething(struct xxx_base *me);
int xxx_reset(struct xxx_base *me);

#endif
```

### xxx_base.c

```c
#include "xxx_base.h"
#include <assert.h>

/* 父类构造函数 */
int xxx_base_init(struct xxx_base *me, const char *name,
                  const struct xxx_ops *ops)
{
    if (!me || !name || !ops) return -1;

    me->ops            = ops;
    me->name           = name;
    me->is_initialized = true;

    return 0;
}

/* 多态 dispatch */
int xxx_doSomething(struct xxx_base *me)
{
    if (!me || !me->ops) return -1;
    if (!me->ops->doSomething) return -2;  /* 不支持 */
    return me->ops->doSomething(me);
}

int xxx_reset(struct xxx_base *me)
{
    if (!me || !me->ops) return -1;
    if (!me->ops->reset) return -2;
    return me->ops->reset(me);
}
```

---

## 子类模板

### xxx_subtype.h

```c
#ifndef _XXX_SUBTYPE_H_
#define _XXX_SUBTYPE_H_

#include "xxx_base.h"

/* 子类 struct */
struct xxx_subtype {
    struct xxx_base base;   /* 父类，放第一个字段 */
    int specific_field;
};

/* 子类构造/析构 */
int xxx_subtype_init(struct xxx_subtype *me, const char *name,
                      int specific_field);
void xxx_subtype_deinit(struct xxx_subtype *me);

#endif
```

### xxx_subtype.c

```c
#include "xxx_subtype.h"
#include "platform.h"
#include <assert.h>

/* 子类实现 */
static int subtype_doSomething(struct xxx_base *me)
{
    struct xxx_subtype *self = container_of(me, struct xxx_subtype, base);
    assert(self && "container_of failed");

    /* 使用 self->specific_field */
    platform_do_something(self->specific_field);
    me->is_initialized = true;
    return 0;
}

static int subtype_reset(struct xxx_base *me)
{
    struct xxx_subtype *self = container_of(me, struct xxx_subtype, base);
    platform_reset(self->specific_field);
    return 0;
}

/* ops 表 */
static const struct xxx_ops subtype_ops = {
    .doSomething = subtype_doSomething,
    .reset       = subtype_reset,
};

/* 子类构造函数 */
int xxx_subtype_init(struct xxx_subtype *me, const char *name,
                      int specific_field)
{
    /* 1. 调父类构造 */
    int rc = xxx_base_init(&me->base, name, &subtype_ops);
    if (rc != 0) return rc;

    /* 2. 填子类字段 */
    me->specific_field = specific_field;

    /* 3. 硬件初始化 */
    platform_init(specific_field);

    return 0;
}

/* 子类析构函数 */
void xxx_subtype_deinit(struct xxx_subtype *me)
{
    if (me) {
        platform_deinit(me->specific_field);
        me->base.ops = NULL;  /* 清空指针 */
        me->base.is_initialized = false;
    }
}
```

---

## 板级初始化模板

### xxx_board_init.c

```c
#include "xxx_subtype.h"
#include "xxx.h"

/* 子类对象：static，外部不可见 */
static struct xxx_subtype s_xxx_dev1;
static struct xxx_subtype s_xxx_dev2;

/* 全局句柄：应用层只拿到父类指针 */
struct xxx_base *g_xxx_dev1;
struct xxx_base *g_xxx_dev2;

int xxx_board_init(void)
{
    int rc;

    /* 构造子类对象 */
    rc = xxx_subtype_init(&s_xxx_dev1, "DEV1", 100);
    if (rc != 0) return rc;

    rc = xxx_subtype_init(&s_xxx_dev2, "DEV2", 200);
    if (rc != 0) return rc;

    /* 向上转型：子类指针 → 父类指针 */
    g_xxx_dev1 = &s_xxx_dev1.base;
    g_xxx_dev2 = &s_xxx_dev2.base;

    return 0;
}

void xxx_board_deinit(void)
{
    if (g_xxx_dev1) {
        struct xxx_subtype *p = container_of(g_xxx_dev1, struct xxx_subtype, base);
        xxx_subtype_deinit(p);
        g_xxx_dev1 = NULL;
    }

    if (g_xxx_dev2) {
        struct xxx_subtype *p = container_of(g_xxx_dev2, struct xxx_subtype, base);
        xxx_subtype_deinit(p);
        g_xxx_dev2 = NULL;
    }
}
```

---

## 应用接口模板

### xxx.h

```c
#ifndef _XXX_H_
#define _XXX_H_

#include "xxx_base.h"

/* 只暴露父类指针，不暴露子类定义 */
extern struct xxx_base *g_xxx_dev1;
extern struct xxx_base *g_xxx_dev2;

/* 应用层只调父类接口，已在 xxx_base.h 声明 */
/* int xxx_doSomething(struct xxx_base *me); */

#endif
```

---

## 应用层模板

### app.c

```c
#include "xxx.h"
#include <stdio.h>

void app_do_something(void)
{
    int rc;

    /* 调接口，不知道底层是什么 */
    rc = xxx_doSomething(g_xxx_dev1);
    if (rc != 0) {
        printf("dev1 failed: %d\n", rc);
    }

    rc = xxx_doSomething(g_xxx_dev2);
    if (rc != 0) {
        printf("dev2 failed: %d\n", rc);
    }
}

int main(void)
{
    /* 板级初始化 */
    xxx_board_init();

    /* 业务逻辑 */
    app_do_something();

    /* 清理 */
    xxx_board_deinit();

    return 0;
}
```

---

## Platform 层模板

### platform.h

```c
#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <stdint.h>
#include <stdbool.h>

/* Platform 抽象接口 */
void platform_init(int id);
void platform_deinit(int id);
void platform_do_something(int id);
void platform_reset(int id);

#endif
```

### platform_pc.c（PC 模拟）

```c
#include "platform.h"
#include <stdio.h>

void platform_init(int id)
{
    printf("[PC] init id=%d\n", id);
}

void platform_deinit(int id)
{
    printf("[PC] deinit id=%d\n", id);
}

void platform_do_something(int id)
{
    printf("[PC] do_something id=%d\n", id);
}

void platform_reset(int id)
{
    printf("[PC] reset id=%d\n", id);
}
```

---

## Makefile 模板

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = app

SRCS = xxx_base.c \
       xxx_subtype.c \
       xxx_board_init.c \
       platform_pc.c \
       app.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)
```

---

## 完整示例：LED

### led_base.h

```c
#ifndef _LED_BASE_H_
#define _LED_BASE_H_

#include <stdbool.h>

struct led_ops {
    int (*on)(struct led_base *me);
    int (*off)(struct led_base *me);
};

struct led_base {
    const struct led_ops *ops;
    const char           *name;
    bool                  is_on;
};

int led_base_init(struct led_base *me, const char *name,
                  const struct led_ops *ops);
int led_on(struct led_base *me);
int led_off(struct led_base *me);

#endif
```

### led_base.c

```c
#include "led_base.h"
#include <assert.h>

int led_base_init(struct led_base *me, const char *name,
                  const struct led_ops *ops)
{
    if (!me || !name || !ops) return -1;
    me->ops  = ops;
    me->name = name;
    me->is_on = false;
    return 0;
}

int led_on(struct led_base *me)
{
    if (!me || !me->ops || !me->ops->on) return -1;
    return me->ops->on(me);
}

int led_off(struct led_base *me)
{
    if (!me || !me->ops || !me->ops->off) return -1;
    return me->ops->off(me);
}
```

### led_gpio.h

```c
#ifndef _LED_GPIO_H_
#define _LED_GPIO_H_

#include "led_base.h"

struct led_gpio {
    struct led_base base;
    uint8_t pin;
    bool    on_level;
};

int led_gpio_init(struct led_gpio *me, const char *name,
                  uint8_t pin, bool on_level);

#endif
```

### led_gpio.c

```c
#include "led_gpio.h"
#include "platform.h"
#include <assert.h>

static int gpio_on(struct led_base *me)
{
    struct led_gpio *self = container_of(me, struct led_gpio, base);
    platform_gpio_write(self->pin, self->on_level);
    me->is_on = true;
    return 0;
}

static int gpio_off(struct led_base *me)
{
    struct led_gpio *self = container_of(me, struct led_gpio, base);
    platform_gpio_write(self->pin, !self->on_level);
    me->is_on = false;
    return 0;
}

static const struct led_ops gpio_ops = {
    .on  = gpio_on,
    .off = gpio_off,
};

int led_gpio_init(struct led_gpio *me, const char *name,
                  uint8_t pin, bool on_level)
{
    int rc = led_base_init(&me->base, name, &gpio_ops);
    if (rc != 0) return rc;

    me->pin      = pin;
    me->on_level = on_level;
    platform_gpio_init(pin, PLATFORM_GPIO_OUTPUT);
    platform_gpio_write(pin, !on_level);
    return 0;
}
```

### led_board_init.c

```c
#include "led_gpio.h"
#include "leds.h"

static struct led_gpio s_led_red;
static struct led_gpio s_led_green;

struct led_base *g_led_red;
struct led_base *g_led_green;

int led_board_init(void)
{
    led_gpio_init(&s_led_red,   "RED",  10, true);
    led_gpio_init(&s_led_green, "GREEN", 11, true);

    g_led_red   = &s_led_red.base;
    g_led_green = &s_led_green.base;

    return 0;
}
```

### leds.h

```c
#ifndef _LEDS_H_
#define _LEDS_H_

#include "led_base.h"

extern struct led_base *g_led_red;
extern struct led_base *g_led_green;

#endif
```

### app.c

```c
#include "leds.h"
#include <stdio.h>

int main(void)
{
    led_board_init();

    led_on(g_led_red);
    led_on(g_led_green);

    led_off(g_led_red);

    return 0;
}
```

---

## 使用步骤

1. **复制模板**
   ```bash
   cp -r templates/ my_project/
   cd my_project
   ```

2. **替换命名**
   ```bash
   # xxx → 你的模块名
   # subtype → 具体实现名
   ```

3. **实现 Platform 层**
   ```c
   // platform_stm32f1xx.c
   void platform_gpio_write(uint8_t pin, bool value)
   {
       HAL_GPIO_WritePin(...);
   }
   ```

4. **编译运行**
   ```bash
   make
   make run
   ```

---

## 模板检查清单

使用模板前确认：

- [ ] 父类放第一个字段
- [ ] ops 表用 const
- [ ] container_of 反推子类
- [ ] 有 init 必须有 deinit
- [ ] 公共接口检查 NULL
- [ ] 头文件只暴露父类指针
- [ ] Platform 层隔离硬件
