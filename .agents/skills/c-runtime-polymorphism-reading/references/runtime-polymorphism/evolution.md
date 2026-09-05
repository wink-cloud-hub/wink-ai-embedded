# 06 - 演化路径

> 从复制粘贴到四层架构，5 个阶段学习路径。

---

## 演化路径总览

```
Stage 1  复制粘贴       3 个 LED = 3 份代码
   ↓
Stage 2  struct + me    一份函数 + 不同 me 指针
   ↓
Stage 3  继承 + ops     GPIO/PWM 共享接口
   ↓
Stage 4  向上转型       应用只拿 LedBase*
   ↓
Stage 5  链接自动注册   main 零引用
```

---

## Stage 1：复制粘贴（ch01）

### 问题

```c
/* 3 个 LED，3 份代码 */
static void s1_red_on(void)
{
    write_reg(13, 1);
}

static void s1_green_on(void)
{
    write_reg(14, 1);
}

static void s1_blue_on(void)
{
    write_reg(15, 1);
}
```

**痛点：**
- 重复代码
- 加 LED 要复制粘贴
- 改一个要改所有

### 代码

> 源码：[`oop-in-c/code/01-struct-me/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/01-struct-me/)

### 目标

理解问题：为什么需要抽象？

---

## Stage 2：struct + me 指针（ch01）

### 解决方案

```c
/* 一份函数，不同 me 指针 */
struct s2_led {
    uint8_t pin;
    bool    is_on;
};

void s2_led_on(struct s2_led *me)
{
    me->is_on = true;
    write_reg(me->pin, 1);
}

/* 使用 */
struct s2_led red   = { .pin = 13 };
struct s2_led green = { .pin = 14 };
struct s2_led blue  = { .pin = 15 };

s2_led_on(&red);
s2_led_on(&green);
s2_led_on(&blue);
```

**优点：**
- 一份函数
- 加 LED 加一个 struct

**局限：**
- 所有 LED 只能用 GPIO
- 换 PWM 要重写

### 目标

理解封装：数据 + 行为归位。

---

## Stage 3：继承 + ops 表 + 多态（ch06-ch11）

### 解决方案

```c
/* 父类 */
struct s3_led_base {
    const struct s3_led_ops *ops;
    const char              *name;
};

/* ops 表 */
struct s3_led_ops {
    void (*on)(struct s3_led_base *me);
};

/* GPIO 子类 */
struct s3_led_gpio {
    struct s3_led_base base;
    uint8_t            pin;
};

/* PWM 子类 */
struct s3_led_pwm {
    struct s3_led_base base;
    uint8_t            channel;
};

/* 父类统一接口 */
void s3_led_on(struct s3_led_base *me)
{
    me->ops->on(me);  /* 多态 dispatch */
}

/* GPIO ops */
static void s3_gpio_on(struct s3_led_base *me)
{
    struct s3_led_gpio *self = (struct s3_led_gpio *)me;
    write_reg(self->pin, 1);
}

static const struct s3_led_ops gpio_ops = {
    .on = s3_gpio_on,
};

/* PWM ops */
static void s3_pwm_on(struct s3_led_base *me)
{
    struct s3_led_pwm *self = (struct s3_led_pwm *)me;
    pwm_enable(self->channel);
}

static const struct s3_led_ops pwm_ops = {
    .on = s3_pwm_on,
};
```

**优点：**
- GPIO/PWM 共享接口
- 加硬件类型加一个 ops 表
- 应用代码不动

### 目标

理解多态：同一接口，不同实现。

### 源码

> [`oop-in-c/code/06-inheritance/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/06-inheritance/) ~ [`oop-in-c/code/11-polymorphism-dispatch/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/11-polymorphism-dispatch/)

---

## Stage 4：向上转型 + 全局句柄（ch12-ch15）

### 解决方案

```c
/* 子类对象：static */
static struct s3_led_gpio g_gpio;
static struct s3_led_pwm  g_pwm;

/* 全局句柄：父类指针 */
static struct s3_led_base *g_led_red;
static struct s3_led_base *g_led_status;

void led_board_init(void)
{
    /* 构造子类对象 */
    g_gpio.base.ops  = &gpio_ops;
    g_gpio.base.name = "RED";
    g_gpio.pin       = 13;

    g_pwm.base.ops   = &pwm_ops;
    g_pwm.base.name  = "STAT";
    g_pwm.channel    = 1;

    /* 向上转型 */
    g_led_red    = &g_gpio.base;
    g_led_status = &g_pwm.base;
}

/* 应用层只调接口 */
s3_led_on(g_led_red);
s3_led_on(g_led_status);
```

**优点：**
- 应用层只拿 `LedBase*`
- 换硬件只改 `led_board_init`
- 应用代码零修改

### 目标

理解向上转型：子类 → 父类。

### 源码

> [`oop-in-c/code/12-upcasting/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/12-upcasting/) ~ [`oop-in-c/code/15-interface/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/15-interface/)

---

## Stage 5：链接自动注册（ch17）

### 解决方案

```c
/* initcall.h */
typedef int (*initcall_t)(void);

#define MODULE_INIT(fn)                                         \
    static initcall_t __initcall_##fn                           \
        __attribute__((used, section("my_initcall"))) = fn

/* startup.c */
extern initcall_t __start_my_initcall[];
extern initcall_t __stop_my_initcall[];

void do_initcalls(void)
{
    for (initcall_t *fn = __start_my_initcall;
         fn < __stop_my_initcall; fn++)
        (*fn)();
}

/* main.c */
int main(void)
{
    do_initcalls();    /* 不知道有哪些 init，但都会被调到 */
    while (1) { /* 业务循环 */ }
}

/* drv_led.c */
static int led_init(void)
{
    led_board_init();
    return 0;
}
MODULE_INIT(led_init);   /* 一行宏，自动注册 */
```

**优点：**
- main 零引用
- 加驱动加一个文件 + 一行宏
- 链接器自动收集

### 目标

理解自动注册：链接器魔法。

### 源码

> [`oop-in-c/code/17-initcall/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/17-initcall/)

---

## 完整演化代码

```c
/* Stage 1：复制粘贴 */
void s1_red_on(void)   { write_reg(13, 1); }
void s1_green_on(void) { write_reg(14, 1); }
void s1_blue_on(void)  { write_reg(15, 1); }

/* Stage 2：struct + me */
struct s2_led { uint8_t pin; };
void s2_led_on(struct s2_led *me) {
    write_reg(me->pin, 1);
}

/* Stage 3：继承 + ops + 多态 */
struct s3_led_base { const struct s3_led_ops *ops; };
struct s3_led_ops { void (*on)(struct s3_led_base *me); };
void s3_led_on(struct s3_led_base *me) {
    me->ops->on(me);
}

/* Stage 4：向上转型 + 句柄 */
struct s3_led_base *g_led_red;
g_led_red = &s_led_gpio.base;
s3_led_on(g_led_red);

/* Stage 5：链接自动注册 */
MODULE_INIT(led_init);
```

---

## 学习建议

### 按顺序学

```
1. Stage 1 → Stage 2
   理解：为什么需要封装

2. Stage 2 → Stage 3
   理解：为什么需要多态

3. Stage 3 → Stage 4
   理解：为什么需要向上转型

4. Stage 4 → Stage 5
   理解：为什么需要自动注册
```

### 动手实践

每阶段：

1. **看代码** → 理解原理
2. **跑 demo** → 看输出
3. **改代码** → 加功能
4. **写代码** → 从零实现

### 常见错误

| 阶段 | 常见错误 | 解决 |
|------|---------|------|
| Stage 2 | me 指针传错 | 检查 struct 地址 |
| Stage 3 | ops 表填错 | 检查函数签名 |
| Stage 4 | 向上转型错 | 检查父类首字段 |
| Stage 5 | section 名错 | 检查链接脚本 |

---

## 代码路径

| 阶段 | 目录 |
|------|------|
| Stage 1-2 | [`oop-in-c/code/01-struct-me/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/01-struct-me/) |
| Stage 3 | [`oop-in-c/code/06-inheritance/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/06-inheritance/) ~ [`11-polymorphism-dispatch/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/11-polymorphism-dispatch/) |
| Stage 4 | [`oop-in-c/code/12-upcasting/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/12-upcasting/) ~ [`15-interface/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/15-interface/) |
| Stage 5 | [`oop-in-c/code/17-initcall/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/17-initcall/) |
| 全景 | [`oop-in-c/code/18-roadmap/`](https://github.com/ZhaoChengBo/zhaoming-embedded/tree/master/oop-in-c/code/18-roadmap/) |

---

## 学习检验

学完后能回答：

- [ ] 为什么需要封装？
- [ ] 为什么需要多态？
- [ ] 为什么需要向上转型？
- [ ] 为什么需要自动注册？
- [ ] container_of 原理是什么？
- [ ] ops 表是如何工作的？
- [ ] 链接器如何收集 init 函数？

---

## 下一步

学完 5 阶段，继续：

1. **Platform 层** → 换芯片不改代码
2. **层次化状态机** → 复杂逻辑不用 if 嵌套
3. **事件驱动** → 模块间不互调
4. **Linux 内核** → 工业级驱动模型

> 详见 [01-architecture.md](./01-architecture.md) 第 7 节「工业级架构：三座山」。
