# ⚠ 运行期多态 —— 外部参考基线（非本项目标准）

> **这不是本项目的编码标准。** wink-micro-os / chigo-micro 采用静态分发，
> 有意偏离本基线（见 `docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md`）。
> 本文件夹存在的目的：**对照理解** + **阅读 Linux/Zephyr 源码**。

---

## 什么时候读这里

- 你在读 **Linux 内核 / Zephyr / RT-Thread** 的驱动源码，想看懂 `file_operations`、
  `i2c_algorithm`、`container_of` 这些东西。
- 你想理解 **C 怎么手搓 OOP**（封装 / 继承 / 多态的底层机制）。
- 你在做**面试准备**，要把 `container_of`、函数指针、平台抽象讲到底层原理。

## 什么时候**不要**读这里

- 你在 **wink-micro-os / chigo-micro 里写代码**。本项目器件是 POD + 命名 API，
  禁用 `struct xxx_ops` 虚表与 `container_of`。请使用 `embedded-best-practice`。

> 即便本 skill 被触发，也请先确认你在写**本项目代码**还是**读内核源码**——前者改用
> `embedded-best-practice`，后者留这里。

---

## 这个范式是什么

经典 Linux 内核设备模型风格：**ops 虚表 + `container_of` 向下转型 + 运行期多态 dispatch**。

```c
struct led_ops { int (*on)(struct led_base *me); int (*off)(struct led_base *me); };
struct led_base { const struct led_ops *ops; const char *name; bool is_on; };

int led_on(struct led_base *me) { return me->ops->on(me); }   /* 两次跳转 */

static int gpio_on(struct led_base *me) {
    struct led_gpio *self = container_of(me, struct led_gpio, base);  /* 向下转型 */
    platform_gpio_write(self->pin, self->on_level);
    return 0;
}
```

四层架构（应用 → 抽象 → 实现 → 注册）+ Platform 隔离。加器件 = 加文件 + 一行注册，
应用层零修改。这套机制**确实强大**——本项目只是因 P0 约束（AI 生成 + Wasm）选择了不采用。

---

## 出处与致谢

- **源文档**：[ZhaoChengBo/zhaoming-embedded](https://github.com/ZhaoChengBo/zhaoming-embedded)，
  书《C 语言面向对象编程·嵌入式实战》。本文件夹由其 ch18「全书地图」蒸馏而来。
- **配套代码**：`oop-in-c/code/15-platform/` 等（PC 可直接 `gcc` 跑）。
- **视频**：B 站搜「兆鸣嵌入式」。

### 参考基线作者的工业数据（出处：zhaoming ch18）

| 指标 | 数值 |
|------|------|
| 业务代码 | 11.2 万行 |
| 业务文件 | 625 个 |
| 事件发布点 | 144 个 |
| 硬件抽象接口 | 8 种 |
| 最深状态嵌套 | 8 层 |
| 跨项目共享 Platform | 5 套产品 · 一套抽象层 |

> 这些是**运行期多态范式**在真实产品里的规模数据，证明该范式工业可行——只是本项目
> 选择了不同的权衡。

---

## 本文件夹内容（已修正若干技术错误）

| 文档 | 内容 |
|------|------|
| [architecture.md](./architecture.md) | 4 层架构 + ops vtable + 向上/向下转型 + 调用链 |
| [templates.md](./templates.md) | vtable 骨架模板 |
| [pitfalls.md](./pitfalls.md) | 10 陷阱（**陷阱 3/6 已修正**：container_of 原子性、const 指针语义） |
| [evolution.md](./evolution.md) | 5 阶段 OOP 演化路径 |
| [reference.md](./reference.md) | Linux/C++/框架对照（**ops 表已修正为 3 列**）+ 与静态分发对照 |

> 范式无关的工程纪律（错误码、内存、并发、清单）在 [../../../_embedded-shared/](../../../_embedded-shared/)，两 skill 共用，同样适用。
