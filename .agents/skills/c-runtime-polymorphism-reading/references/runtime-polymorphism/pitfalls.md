# 04 - 常见陷阱

> 避坑指南，代码审查重点。

---

## 陷阱 1：container_of 用错

### 错误示例

```c
/* ❌ 父类指针不是从子类来的 */
struct led_base *base = malloc(sizeof(struct led_base));
struct led_gpio *gpio = container_of(base, struct led_gpio, base);
/* 崩溃！base 不是 led_gpio 的一部分 */
```

### 正确示例

```c
/* ✓ 从子类对象取 base 地址 */
struct led_gpio gpio;
struct led_base *base = &gpio.base;
struct led_gpio *gpio2 = container_of(base, struct led_gpio, base);
/* 正确 */
```

### 检查方法

```c
/* 添加断言 */
static int gpio_on(struct led_base *me)
{
    struct led_gpio *self = container_of(me, struct led_gpio, base);
    assert(self->pin < 256 && "container_of may be wrong");
    ...
}
```

---

## 陷阱 2：ops 表忘记清空

### 错误示例

```c
/* ❌ deinit 后 ops 仍指向已释放的函数 */
void some_deinit(void)
{
    free(s_led);
    s_led = NULL;
    /* 但 g_led_error->ops 仍指向 s_led 的函数表！ */
}

/* 之后调用会崩溃 */
led_on(g_led_error);  /* 崩溃！ */
```

### 正确示例

```c
/* ✓ deinit 时清空 ops */
void led_board_deinit(void)
{
    s_led_err.base.ops = NULL;  /* 清空指针 */
    g_led_error = NULL;
}
```

### 防御措施

```c
/* 公共接口检查 ops */
int led_on(struct led_base *me)
{
    if (!me || !me->ops) return -1;  /* 防止 ops 为 NULL */
    ...
}
```

---

## 陷阱 3：中断里调多态接口

### 错误示例

```c
/* ❌ 中断里调多态，可能重入 */
void ISR(void)
{
    led_on(g_led_error);  /* 危险！ */
}
```

### 真正的问题（先澄清一个常见误解）

> **澄清**：`container_of` 是编译期算偏移，运行时只展开成**一条减法指令**（SUB），
> 原子的、ISR 安全的——本系列架构篇已讲过它「零开销」。多态 dispatch
> （`me->ops->on(me)`，两次指针解引用 + 一次间接调用）本身也只是若干内存读，
> 没有非原子的写竞争。**所以「中断里不能用多态」不是因为 container_of 或 dispatch
> 不原子**。zhaoming 参考资料也从未做过这类论断。

真正不能在 ISR 里随便调的原因，全在「**被调函数做了什么**」：

1. **可能重入 / 不可重入**——`gpio_on` 等实现可能读改写共享状态，任务上下文与 ISR
   上下文同时进入会撕裂数据。
2. **可能阻塞**——一旦实现里调了带阻塞的 mutex / 信号量 / `osDelay`，在 ISR 上下文
   调用直接死锁或断言失败（RTOS 不允许 ISR 阻塞）。
3. **优先级反转 / 关中断过长**——若用「关中断」把整段 dispatch 包起来，中断延迟被
   拉长，伤害实时性。
4. **违反 RTOS 上下文约束**——很多 API 有 `FromISR` 专用变体，任务上下文版本在 ISR
   里调用是未定义行为。

一句话：**风险来自「被调函数做了什么」，而不是「多态机制本身」。**

### 正确做法

```c
/* ✓ 首选：ISR 只发信号，实活交给任务上下文（延迟处理） */
void ISR(void)
{
    /* ISR 尽可能短：释放信号量，用 FromISR 变体，绝不阻塞 */
    BaseType_t higher_priority_woken = pdFALSE;
    xSemaphoreGiveFromISR(g_led_sem, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}

/* 高优先级工作线程在任务上下文里跑多态——可以用任何 RTOS / 框架 API */
void led_worker(void)
{
    for (;;) {
        xSemaphoreTake(g_led_sem, portMAX_DELAY);
        led_on(g_led_error);     /* 任务上下文，安全 */
    }
}
```

```c
/* ✓ 次选：ISR 里直接调「已知安全、可重入、不阻塞」的具体实现 */
/* 前提：你已确认该函数只做寄存器写、不碰共享状态、不阻塞 */
void ISR(void)
{
    platform_gpio_write(s_led_err.pin, s_led_err.on_level);
}
```

```c
/* ⚠ 不推荐：用关中断把整段多态「包」起来 */
/* 技术上能避免重入，但拉长中断延迟、伤害实时性，还掩盖了「被调函数不安全」的真问题 */
void ISR(void)
{
    irq_lock();
    led_on(g_led_error);        /* 若 led_on 实现阻塞，照样死锁 */
    irq_unlock();
}
```

> 对应 zhaoming `hardware-interaction.md` 的「ISR → osSemaphoreRelease → 高优先级
> 工作线程」模式：ISR 最小化，回调在任务上下文执行。详见
> [concurrency.md](../../../_embedded-shared/concurrency.md)。

---

## 陷阱 4：父类不放第一个字段

### 错误示例

```c
/* ❌ 父类不在第一个字段 */
struct led_gpio {
    uint8_t pin;              /* ← 错误！父类不在第一个 */
    struct led_base base;
};

/* 向上转型不是零开销 */
struct led_gpio gpio;
struct led_base *base = &gpio.base;  /* 地址不对 */
```

### 正确示例

```c
/* ✓ 父类放第一个字段 */
struct led_gpio {
    struct led_base base;     /* ← 第一个字段 */
    uint8_t pin;
};

/* 向上转型零开销 */
struct led_gpio gpio;
struct led_base *base = &gpio.base;  /* 地址正确 */
```

### 为什么？

```c
/* C99 保证：struct 首字段地址 == struct 本身地址 */
assert((void *)&gpio == (void *)&gpio.base);

/* 父类不在第一个字段，地址不等 */
assert((void *)&gpio != (void *)&gpio.base);  /* 失败 */
```

---

## 陷阱 5：忘记 deinit

### 错误示例

```c
/* ❌ 有 init 无 deinit */
int some_init(void)
{
    s_led = malloc(sizeof(struct led_gpio));
    return led_gpio_init(s_led, ...);
}

/* 程序退出时内存泄漏 */
```

### 正确示例

```c
/* ✓ 有 init 必须有 deinit */
int some_init(void)
{
    s_led = malloc(sizeof(struct led_gpio));
    return led_gpio_init(s_led, ...);
}

void some_deinit(void)
{
    led_gpio_deinit(s_led);  /* 先析构 */
    free(s_led);              /* 再释放 */
    s_led = NULL;
}
```

---

## 陷阱 6：多线程下 ops 表被修改

### 错误示例

```c
/* ❌ 线程 A 在读 ops */
void thread_a(void)
{
    led_on(g_led_error);  /* 读 me->ops */
}

/* 线程 B 在改 ops */
void thread_b(void)
{
    s_led_err.base.ops = &new_ops;  /* 改 ops */
}

/* 可能崩溃：线程 A 读到一半被线程 B 修改 */
```

### 正确做法

**先澄清一个常见误用**：`const struct led_ops *ops` 只是「指针指向的表是 const」
（表内容不可改），**指针本身仍可被重写**——所以它**挡不住**上面线程 B 的
`s_led_err.base.ops = &new_ops`。要真正锁死指针，得让「指针也是 const」：

```c
/* ✓ 方案1：ops 表 + 指针都 const，编译期挡住重写 */
struct led_base {
    const struct led_ops * const ops;   /* 指针本身 const：不可再赋值 */
};

/* ops 表自然是 const */
static const struct led_ops gpio_ops = { .on = gpio_on, .off = gpio_off };
```

```c
/* ✓ 方案2（更常见的工程约定）：构造期赋值一次，之后永不改 */
/* ops 不加顶层 const 也行——靠「只在 init 里写」的纪律保证 */
int led_gpio_init(struct led_gpio *me, ...)
{
    me->base.ops = &gpio_ops;   /* 构造期一次性绑定 */
    /* 之后任何代码都不再写 me->base.ops */
    /* ... */
}
/* 约定：ops 是「构造期绑定、运行期只读」的不变量 */
```

```c
/* ✓ 方案3：若 ops 真的会在运行期变，读写都必须在同一把锁内 */
void thread_a(void)
{
    mutex_lock(&g_led_mutex);
    led_on(g_led_error);            /* 读 me->ops 与调用必须原子 */
    mutex_unlock(&g_led_mutex);
}
void thread_b(void)
{
    mutex_lock(&g_led_mutex);
    s_led_err.base.ops = &new_ops;  /* 改 ops 也在同一把锁内 */
    mutex_unlock(&g_led_mutex);
}
```

> 经验法则：**ops 表本应是不可变的**（`const` + 构造期绑定）。一旦你发现需要运行期
> 改 ops，多半说明该换模式（策略模式 / 多实例），而不是改一个共享对象的虚表。

---

## 陷阱 7：内存对齐问题

### 错误示例

```c
/* ❌ 强制转换，忽略对齐 */
struct led_gpio gpio;
struct led_base *base = (struct led_base *)&gpio;
/* 某些架构上可能崩溃（对齐不一致） */
```

### 正确示例

```c
/* ✓ 父类放第一个字段，编译器保证对齐 */
struct led_gpio {
    struct led_base base;     /* 编译器自动对齐 */
    uint8_t pin;
};

struct led_base *base = &gpio.base;  /* 安全 */
```

---

## 陷阱 8：返回栈上对象地址

### 错误示例

```c
/* ❌ 返回栈上对象地址 */
struct led_base *create_led(void)
{
    struct led_gpio led;
    led_gpio_init(&led, "ERR", 10, true);
    return &led.base;  /* 错误！led 是栈上变量 */
}

/* 使用时已销毁 */
struct led_base *p = create_led();
led_on(p);  /* 崩溃！led 已销毁 */
```

### 正确示例

```c
/* ✓ 方案1：返回静态对象 */
struct led_base *get_led(void)
{
    static struct led_gpio s_led;
    led_gpio_init(&s_led, "ERR", 10, true);
    return &s_led.base;
}

/* ✓ 方案2：调用者分配内存 */
struct led_base *create_led(struct led_gpio *ptr)
{
    led_gpio_init(ptr, "ERR", 10, true);
    return &ptr->base;
}

/* 调用者负责管理内存 */
struct led_gpio *p = malloc(sizeof(struct led_gpio));
struct led_base *base = create_led(p);
```

---

## 陷阱 9：循环依赖

### 错误示例

```c
/* xxx_base.h */
#include "xxx_subtype.h"  /* ❌ 循环依赖 */

/* xxx_subtype.h */
#include "xxx_base.h"
```

### 正确示例

```c
/* xxx_base.h */
#ifndef _XXX_BASE_H_
#define _XXX_BASE_H_

/* 前向声明，不包含完整定义 */
struct xxx_base;
struct xxx_ops;

#endif

/* xxx_subtype.h */
#include "xxx_base.h"  /* 只在这里包含 */
```

---

## 陷阱 10：忘记检查返回值

### 错误示例

```c
/* ❌ 不检查返回值 */
void app_init(void)
{
    led_gpio_init(&s_led, "ERR", 10, true);  /* 失败也不知道 */
    platform_gpio_init(10, OUTPUT);          /* 失败也不知道 */
}

/* 使用时可能未初始化 */
led_on(&s_led.base);  /* 可能崩溃 */
```

### 正确示例

```c
/* ✓ 检查返回值 */
int app_init(void)
{
    int rc;

    rc = led_gpio_init(&s_led, "ERR", 10, true);
    if (rc != 0) {
        printf("led init failed: %d\n", rc);
        return rc;
    }

    rc = platform_gpio_init(10, OUTPUT);
    if (rc != 0) {
        printf("gpio init failed: %d\n", rc);
        return rc;
    }

    return 0;
}
```

---

## 代码审查检查清单

代码审查时重点检查：

- [ ] container_of 使用是否正确
- [ ] 父类是否放第一个字段
- [ ] ops 表是否用 const
- [ ] deinit 是否清空 ops
- [ ] 中断里是否调多态接口
- [ ] 是否有 init 无 deinit
- [ ] 是否返回栈上对象地址
- [ ] 是否检查返回值
- [ ] 是否有循环依赖
- [ ] 多线程下 ops 表是否安全

---

## 调试技巧

### 1. 打印 ops 地址

```c
printf("ops: %p, ops->on: %p\n", me->ops, me->ops->on);
```

### 2. 检查对象地址

```c
printf("base: %p, gpio: %p\n", &gpio.base, &gpio);
assert((void *)&gpio == (void *)&gpio.base);
```

### 3. 添加魔数

```c
struct led_gpio {
    struct led_base base;
    uint32_t magic;  /* 0xDEADBEEF */
    uint8_t pin;
};

/* 检查魔数 */
assert(self->magic == 0xDEADBEEF && "corrupted object");
```

### 4. 使用 Valgrind

```bash
valgrind --leak-check=full ./app
```

### 5. 使用 AddressSanitizer

```bash
gcc -fsanitize=address -g app.c -o app
./app
```
