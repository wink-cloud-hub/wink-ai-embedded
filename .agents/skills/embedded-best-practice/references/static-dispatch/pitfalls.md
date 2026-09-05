# 静态分发常见陷阱

> 写 wink-micro-os / chigo-micro 代码时的避坑指南。范式无关的陷阱（内存/并发）见
> [../../../_embedded-shared/](../../../_embedded-shared/)。

---

## 陷阱 1：用 `if (status)` 检查错误码

```c
wink_status_t s = dal_ultrasonic_read(&radar, &d);
if (s) { /* ❌ 负数错误码是 truthy，这里把失败当成功 */ }
if (s < 0) { /* ✓ */ }                  /* 正确 */
if (s != WINK_OK) { /* ✓ */ }           /* 正确 */
```

详见 [../../../_embedded-shared/error-codes.md](../../../_embedded-shared/error-codes.md)。这是 AI 代码生成头号雷。

---

## 陷阱 2：API 命名漂移（`get_distance` vs `read`）

代码用 `dal_ultrasonic_get_distance`，Device Registry / 设计文档用 `dal_ultrasonic_read`。
**以 Registry 为准**——它被 codegen、仿真、真机驱动 8 条路径共用。写新代码用 `dal_xxx_read`
这类动词语义，不要用 `get_xxx`（`get` 暗示纯查询、无副作用，与「读硬件」语义不符）。

---

## 陷阱 3：`js_sim_*` 旁路导入签名三处冲突

`js_sim_get_ultrasonic_distance` 在代码 / DAL doc / Registry 里有三种不同签名（参数类型、
返回类型都不同）。这是「SSOT 未强制」的活样本。**写新旁路导入时，签名一律抄 Registry
里的声明**，不要自己另起一份。详见 `07-platform-governance/01-device-model-registry.md`。

---

## 陷阱 4：`#ifdef SIMULATION` 隔离过宽

wink-micro-os 现有 `dal_ultrasonic.c` 把**整个函数体**在 `#ifdef` 下重复了两份——这是
ADR-0003 要废弃的旧形态。正确做法（ADR-0002 item 4 前置）：

**只旁路最低层物理信号源**，协议解析 + CRC + 错误检测在仿真与真机间**共享**。隔离越靠下，
被同源测试覆盖的代码越多。

```c
/* ✓ 只在最底层 fork：真机读寄存器 / 仿真读 renderer 距离，上层换算共用 */
static float read_raw_distance(dal_ultrasonic_t *dev) {
#ifdef SIMULATION
    return js_sim_get_ultrasonic_distance(...);   /* 仅此一行旁路 */
#else
    /* 真机：发触发脉冲 + 测 echo 时序 */
#endif
}
/* 共享：物理量换算、滤波、错误判定 —— 仿真真机同源 */
```

---

## 陷阱 5：`vdl_` vs `dal_` 前缀混用

wink-micro-os README 里残留 `vdl_[device]_[action]` 说法，但代码与其他文档统一用 `dal_`。
**一律 `dal_`**（Device Abstraction Layer）。

---

## 陷阱 6：依赖 Wasm 的 PAL mutex 保证正确性

Wasm 单线程，`pal_mutex_create` 返回常量、lock/unlock 是空操作——**假锁**。仿真里没有真实
竞争，不要依赖 PAL mutex 在仿真环境保证跨任务正确性。真机并发正确性靠 xtensa target 的
真实 OSAL。详见 [../../../_embedded-shared/concurrency.md](../../../_embedded-shared/concurrency.md)。

---

## 陷阱 7：把器件抽象写成 vtable

静态分发的铁律是器件用 POD + 命名 API。如果你发现自己在写 `struct xxx_ops { ... }` +
`container_of` 来抽象一个**器件**——停。那是运行期多态范式（见 `c-runtime-polymorphism-reading`），本项目有意不用。器件就该是 `dal_xxx_t` + `dal_xxx_read/set/...`。

> vtable 仅在「同抽象需切换多算法」（策略模式，如 `control_algo_t`）时合法，且封装在模块内部。

---

## 陷阱 8：以为 `device_tree.c` 已存在

它**尚未生成**（codegen 是设计态）。现在写代码引用 `front_radar` 这类实例会编译失败——
要么手写临时静态实例，要么等 codegen 落地。见 [README 偏差框](./README.md)。

---

## 陷阱 9：在器件层追求「统一句柄 / 数组遍历」

静态分发**刻意放弃**了「把不同外放进同一数组统一初始化/遍历」的能力（这是运行期多态的
优势）。若业务真需要「遍历所有距离传感器统一采样」，不要硬塞 vtable——先确认是否真的需要
运行期多态（见下）。

---

## 何时应回退到运行期多态（ADR-0004 退出条件）

仅当一个**具体器件抽象**确实需要「**多种硬件实现在运行期并存且切换**」（如距离传感器抽象
要同时支持 HC-SR04 与 VL53L0X 并按配置切换）时，才考虑回退。且**不破坏 App/BAL 静态 API 契约**，
多态封装在 DAL 该器件内部（微型 ops 表或 `switch-case`）。详见 [evolution.md](./evolution.md)。
