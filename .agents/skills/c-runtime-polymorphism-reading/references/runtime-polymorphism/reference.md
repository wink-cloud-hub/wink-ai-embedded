# 05 - 参考对照

> Linux 内核、C++、框架对比，深入理解。

---

## Linux 内核对照

### ops 表对应

内核里到处都是「一个 struct 装函数指针，不同驱动填不同实现」。你写的 `struct led_ops`
就是这套模式的最小形态，与下面每一行同构：

| 子系统 ops 表 | 用途 | 关键函数指针 |
|---|---|---|
| `file_operations` | 文件系统 · 读 / 写 / 打开 / 关闭 | `.read` `.write` `.open` |
| `i2c_algorithm` | I2C 总线 · 数据传输 | `.xfer` |
| `spi_controller` | SPI 控制器 · 数据传输 | `.transfer_one` |
| `gpio_chip` | GPIO 控制器 · 引脚操作 | `.get` `.set` `.direction` |
| `net_device_ops` | 网络设备 · 发包 / 收包 | `.ndo_start_xmit` |

> 注意：上表每行是**各自独立**的子系统，不要把不同行的结构体两两配对（旧版文档曾把
> `i2c_algorithm` 与 `gpio_chip` 错误地配在一行——它们毫无派生关系）。

### 多态 dispatch 对照

```c
/* LED 代码 */
led_on(me)
  → me->ops->on(me)
  → gpio_on(me)

/* Linux I2C */
i2c_transfer(adap)
  → adap->algo->xfer(adap)
  → rk3x_i2c_xfer(adap)

/* Linux VFS */
vfs_read(file)
  → file->f_op->read(file)
  → ext4_read(file)
```

### container_of 对照

```c
/* LED 代码 */
struct led_gpio *self = container_of(me, struct led_gpio, base);

/* Linux 内核（出现数万次） */
#define container_of(ptr, type, member) ({          \
    const typeof(((type *)0)->member) *__mptr = (ptr);    \
    (type *)((char *)__mptr - offsetof(type, member)); })
```

### 模块注册对照

```c
/* LED 代码 */
MODULE_INIT(led_init);
/* 启动时自动调用 */

/* Linux 内核 */
module_init(led_init);
module_platform_driver(led_driver);
```

### 四层架构对照

```
LED 代码:         Linux 内核:
① 应用层         vfs_read / i2c_transfer
② 抽象层         file_operations / i2c_algorithm
③ 实现层         ext4_read / rk3x_i2c_xfer
④ 注册层         module_init / module_platform_driver
```

---

## C++ 对照

### 封装对照

```c
/* C 语言 */
struct led_gpio {
    struct led_base base;
    uint8_t pin;
};

void led_gpio_init(struct led_gpio *me, uint8_t pin)
{
    me->pin = pin;
}

void led_gpio_on(struct led_gpio *me)
{
    platform_gpio_write(me->pin, true);
}
```

```cpp
/* C++ */
class LedGpio : public LedBase {
private:
    uint8_t pin;

public:
    LedGpio(uint8_t p) : pin(p) {}

    void on() {
        platform_gpio_write(pin, true);
    }
};
```

### 继承对照

```c
/* C 语言 */
struct led_gpio {
    struct led_base base;   /* 嵌套父类 */
    uint8_t pin;
};

/* 向上转型 */
struct led_base *base = &gpio.base;
```

```cpp
/* C++ */
class LedGpio : public LedBase {  /* 继承 */
    uint8_t pin;
};

/* 向上转型（隐式） */
LedBase *base = &gpio;
```

### 多态对照

```c
/* C 语言 */
struct led_ops {
    int (*on)(struct led_base *me);
};

struct led_base {
    const struct led_ops *ops;
};

int led_on(struct led_base *me)
{
    return me->ops->on(me);  /* 两次跳转 */
}
```

```cpp
/* C++ */
class LedBase {
public:
    virtual int on() = 0;  /* 纯虚函数 */
};

int led_on(LedBase *me)
{
    return me->on();  /* 编译器自动查 vtable */
}
```

### vtable 对照

```c
/* C 语言：手动构建 vtable */
static const struct led_ops gpio_ops = {
    .on  = gpio_on,
    .off = gpio_off,
};

me->ops = &gpio_ops;
```

```cpp
/* C++：编译器自动生成 vtable */
class LedGpio : public LedBase {
public:
    int on() override;
    int off() override;
};

/* 编译器自动生成 vtable 和 vptr */
```

### 向下转型对照

```c
/* C 语言：container_of */
struct led_gpio *self = container_of(me, struct led_gpio, base);
/* 编译期算偏移，运行时一条减法，零开销 */
```

```cpp
/* C++：dynamic_cast */
LedGpio *self = dynamic_cast<LedGpio *>(me);
/* 运行时查 type_info，有开销 */

/* C++：static_cast（不安全） */
LedGpio *self = static_cast<LedGpio *>(me);
/* 编译期转换，无检查，危险 */
```

### 构造/析构对照

```c
/* C 语言：手动调用 */
int led_gpio_init(struct led_gpio *me, ...);
void led_gpio_deinit(struct led_gpio *me);

led_gpio_init(&gpio, ...);
/* 使用 */
led_gpio_deinit(&gpio);
```

```cpp
/* C++：自动调用 */
class LedGpio {
public:
    LedGpio(...);     /* 构造函数 */
    ~LedGpio();       /* 析构函数 */
};

LedGpio gpio(...);   /* 自动调用构造函数 */
/* 作用域结束自动调用析构函数 */
```

### 信息隐藏对照

```c
/* C 语言：static */
static int gpio_on(struct led_base *me);  /* 本文件可见 */
```

```cpp
/* C++：private/private */
class LedGpio : public LedBase {
private:
    int gpio_on();  /* 只有类内可见 */
};
```

---

## 框架对比

### Zephyr

```c
/* Zephyr GPIO */
static const struct gpio_callback gpio_cb;

int gpio_init(void)
{
    gpio_add_callback(GPIO_PORT, &gpio_cb);
    gpio_pin_configure(GPIO_PORT, PIN, GPIO_OUTPUT);
}
```

**相似点：**
- ops 表机制
- 设备树支持
- 自动注册

**差异点：**
- Zephyr 有设备树
- Zephyr 有电源管理
- Zephyr 有完整的驱动框架

### RT-Thread

```c
/* RT-Thread 设备驱动 */
static rt_err_t gpio_init(rt_device_t dev)
{
    /* 初始化 */
    return RT_EOK;
}

static struct rt_device_ops gpio_ops = {
    gpio_init,
    gpio_open,
    gpio_close,
    gpio_read,
    gpio_write,
    gpio_control,
};
```

**相似点：**
- rt_device_ops ≈ led_ops
- 设备注册机制
- 统一设备接口

**差异点：**
- RT-Thread 有完整的设备管理
- RT-Thread 有线程调度
- RT-Thread 有丰富的组件

### Linux 内核

```c
/* Linux GPIO */
static int gpio_probe(struct platform_device *pdev)
{
    struct gpio_chip *chip;
    chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
    chip->request = gpio_request;
    chip->free = gpio_free;
    chip->get = gpio_get;
    chip->set = gpio_set;
    gpiochip_add(chip);
    return 0;
}

static struct platform_driver gpio_driver = {
    .probe = gpio_probe,
    .driver = {
        .name = "gpio",
        .of_match_table = gpio_of_match,
    },
};
module_platform_driver(gpio_driver);
```

**相似点：**
- platform_driver ≈ MODULE_INIT
- device + driver 分离
- ops 表机制

**差异点：**
- Linux 有设备树
- Linux 有完整的电源管理
- Linux 有丰富的子系统

---

## 选型建议

### 用本架构

- 小型裸机项目
- 快速原型
- 学习理解 OOP
- 资源受限 MCU

### 用 Zephyr

- 需要丰富驱动
- 需要蓝牙/网络
- 需要电源管理
- 需要设备树

### 用 RT-Thread

- 需要完整 OS
- 需要丰富组件
- 需要商业支持
- 国内项目

### 用 Linux

- 应用处理器
- 需要丰富生态
- 需要用户空间
- 服务器场景

---

## 性能对比

| 操作 | C 本架构 | C++ | Zephyr | Linux |
|------|---------|-----|--------|-------|
| dispatch | 2 次跳转 | 2 次跳转 | 2 次跳转 | 2 次跳转 |
| 向下转型 | 1 条减法 | RTTI 查表 | container_of | container_of |
| 内存开销 | 最小 | vptr | 最小 | 较大 |
| 代码体积 | 小 | 较小 | 中 | 大 |

---

## 学习路径

```
1. 本架构
   ↓ 理解 ops 表、container_of
2. C++ 虚函数
   ↓ 理解编译器如何实现多态
3. Zephyr/RT-Thread
   ↓ 理解完整 OS 架构
4. Linux 内核
   ↓ 理解工业级驱动模型
```

---

## 与静态分发（本项目实际采用）的对照

> 本节回答「既然这么好，本项目为什么不这么写」。完整对比与决策见 [index.md](../index.md)。

| 维度 | 运行期多态（本参考基线） | 静态分发（wink-micro-os / chigo-micro 实际采用） |
|------|--------------------------|--------------------------------------------------|
| 调用 | `me->ops->on(me)` 两次跳转 | `dal_servo_set_angle(&dev, ...)` 直调 |
| 对象 | base 嵌入 + ops 指针 | 扁平 POD，无函数指针 |
| 子类恢复 | `container_of` | 不需要，编译期类型已知 |
| 注册 | `MODULE_INIT` 链接段 | `device_tree.c` codegen 静态绑定 |
| 热插拔 / 运行期换实现 | ✅ 原生 | ❌ non-goal |
| 统一句柄（数组/链表遍历） | ✅ | ❌ |
| AI 生成友好度 | ❌ 指针强转易幻觉 | ✅ 命名确定、可静态校验 |
| Wasm 仿真 | ❌ `call_indirect` 破坏优化 | ✅ 可旁路直通、零跳转 |
| RAM 开销 | 每实例 + ops 指针 | 0 |

本项目（WinkMicroOS / chigo-micro）**有意偏离**本参考基线，采用静态分发——决策依据是 ADR-0004（`docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md`）。写本项目代码时请使用 `embedded-best-practice`，请勿套用本文件夹的 vtable / container_of 范式。

---

## 总结

| 方面 | 本架构 | C++ | Zephyr | Linux |
|------|--------|-----|--------|-------|
| 学习曲线 | 低 | 中 | 中 | 高 |
| 代码体积 | 小 | 小 | 中 | 大 |
| 运行开销 | 最低 | 低 | 低 | 中 |
| 功能完整性 | 基础 | 基础 | 完整 | 非常完整 |
| 生态支持 | 无 | 无 | 丰富 | 非常丰富 |
| 适用场景 | 裸机 | 裸机 | RTOS | Linux |

**本架构是基础，理解后再学其他框架会更容易。**
