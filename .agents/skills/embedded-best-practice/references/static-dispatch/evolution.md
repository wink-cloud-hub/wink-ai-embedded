# 静态分发演化与迁移

> 两条路径：① 代码向 ADR 目标迁移（短期 delta）；② 静态→运行期多态的退出条件（长期）。

---

## 一、代码迁移 delta（bool/float → wink_status_t）

wink-micro-os 现有代码处于 ADR-0001 / ADR-0004 落地前。迁移对照表：

| 现状 | 迁移到 | 动作 |
|------|--------|------|
| `bool dal_rc_servo_set_angle(...)` | `wink_status_t dal_rc_servo_set_angle(...)` | 改返回类型，失败返回负码 |
| `float dal_ultrasonic_get_distance(...)`（哨兵 `-1.0f`） | `wink_status_t dal_ultrasonic_read(dev, float *out)` | 拆成「状态 + 出参」，消除哨兵歧义 |
| `dal_*_get_distance` | `dal_*_read` | 按 Registry 改名 |
| `js_sim_*` 三种签名 | 以 Registry 为准的单一签名 | 统一，删除他处声明 |
| `#ifdef SIMULATION` 整函数重复 | 只旁路最低物理信号层 | 重构共享上层 |
| 无 `device_tree.c` | codegen 生成 | 实现 codegen（设计已就绪） |
| `if (status)` 检查 | `if (status < 0)` | 全局搜替换 + lint |

迁移**不得静默改变**物理引脚、默认电压、DAL API 语义（`07-platform-governance/01` §8）。

---

### 1.1 典型迁移重构示例 1：Callback 签名上下文改造

在旧代码中，中断或异步回调往往不带上下文指针，导致回调函数内不得不强行读取全局变量，丧失了封装性：

```c
/* BEFORE (旧版回调设计：全局状态耦合，不利于多实例) */
typedef void (*dal_gpio_isr_cb)(void);
static dal_gpio_isr_cb s_button_cb = NULL;

void gpio_isr_handler(void) {
    if (s_button_cb) s_button_cb(); // 无法得知是哪个按键触发的
}

/* AFTER (新版契约设计：携带静态 user_data 上下文) */
typedef void (*dal_gpio_isr_cb)(void *user_data);

typedef struct {
    uint16_t pin;
    dal_gpio_isr_cb callback;
    void *user_data; // 静态绑定对应的设备实例指针 (如 &emergency_stop_btn)
} dal_gpio_btn_t;

void gpio_isr_handler_new(dal_gpio_btn_t *btn) {
    if (btn->callback) {
        btn->callback(btn->user_data); // 传入上下文，解耦逻辑
    }
}
```

---

### 1.2 典型迁移重构示例 2：消除 float 哨兵值返回值，改用带状态指针

旧代码经常为了图省事，让 API 直接返回测量物理量（如 `float`），并规定 `-1.0f` 代表超时错误。这在噪声较大或边界值测量时极易引起逻辑判断失误：

```c
/* BEFORE (旧版设计：物理量与状态混合，哨兵值存在二义性风险) */
float dal_ultrasonic_get_distance(dal_ultrasonic_t *dev) {
    if (dev == NULL) return -1.0f;
    uint32_t echo_time = measure_echo();
    if (echo_time == 0) return -1.0f; // 超时错误
    return (float)echo_time * 0.017f;
}

/* AFTER (新版契约设计：返回值仅表达状态，数据由指针出参带回) */
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm) {
    if (dev == NULL || distance_cm == NULL) return WINK_ERR_INVALID_ARG;
    
    uint32_t echo_time = 0;
    wink_status_t status = measure_echo(&echo_time);
    if (status < 0) {
        return status; // 返回具体的错误代码 (如 WINK_ERR_TIMEOUT)
    }
    
    *distance_cm = (float)echo_time * 0.017f;
    dev->state.last_distance = *distance_cm;  /* 目标形态：state 区；现状扁平 dev->last_distance 见 README 偏差框 */
    return WINK_OK;
}
```

---

### 1.3 典型迁移重构示例 3：收窄 `#ifdef SIMULATION` 条件编译范围

旧的 DAL 驱动粗暴地将整个读写函数在 Wasm 和真机下完全分立成两个实体，导致大量的中间协议校验与数据转换逻辑无法被仿真端覆盖（同源性低）：

```c
/* BEFORE (旧版隔离：全函数隔离，逻辑完全分叉) */
#ifdef SIMULATION
wink_status_t dal_sensor_read(dal_sensor_t *dev, float *out) {
    *out = js_sim_get_sensor_raw();
    return WINK_OK;
}
#else
wink_status_t dal_sensor_read(dal_sensor_t *dev, float *out) {
    uint8_t raw[2];
    pal_i2c_read(dev->i2c_port, raw, 2);
    *out = ((float)(raw[0] << 8 | raw[1])) * 0.1f;
    return WINK_OK;
}
#endif

/* AFTER (新版隔离：只隔离最低层物理电平/总线 IO，数据换算同源) */
static wink_status_t read_raw_bytes(dal_sensor_t *dev, uint8_t *buf) {
#ifdef SIMULATION
    // 旁路直通物理电平：向仿真环境索取模拟的 I2C 原始数据包
    return js_sim_get_i2c_payload(dev->i2c_port, buf, 2);
#else
    // 物理 I2C 传输
    return pal_i2c_read(dev->i2c_port, buf, 2);
#endif
}

wink_status_t dal_sensor_read(dal_sensor_t *dev, float *out) {
    uint8_t raw[2] = {0};
    wink_status_t status = read_raw_bytes(dev, raw);
    if (status < 0) return status;
    
    // --- 共享同源换算逻辑：不管是仿真还是真机，校验和转换在同一段代码运行 ---
    *out = ((float)(raw[0] << 8 | raw[1])) * 0.1f; 
    dev->state.last_value = *out;  /* 目标形态：state 区；现状扁平见 README 偏差框 */
    return WINK_OK;
}
```

---

### 1.4 典型迁移重构示例 4：扁平结构体 → config/state 显式分离

现状（`dal_ultrasonic.h` / `dal_rc_servo.h`）是扁平字段、非 `const`：

```c
/* BEFORE（现状扁平）：配置与状态平级，字段可被任意篡改 */
typedef struct {
    uint16_t trig_pin;       /* 配置，但未用 const 锁定 */
    uint16_t echo_pin;
    float    last_distance;  /* 运行期状态，与配置平级 */
} dal_ultrasonic_t;
```

```c
/* AFTER（目标：const 配置区 + struct state{} 可变区分离，见 lifecycle.md §2） */
typedef struct {
    const uint16_t trig_pin;     /* const 锁定物理引脚，防篡改 */
    const uint16_t echo_pin;

    struct {
        float         last_distance;  /* 运行期可变状态 */
        wink_status_t last_status;
    } state;
} dal_ultrasonic_t;
```

迁移要点：所有读写点 `dev->last_distance` → `dev->state.last_distance`；Codegen 实例化时 `.trig_pin` / `.echo_pin` 由 `const` 初始化器锁定，`.state` 由运行期驱动填充。

---

## 二、ADR-0004 局部多态化退出路径（何时 + 怎么回退运行期多态）

**触发条件**：一个**具体器件抽象**需要「多种硬件实现在运行期并存且切换」。
（例：距离传感器抽象同时挂 HC-SR04 与 VL53L0X，运行期按配置选实现。）

若不满足（拓扑编译期确定、单一实现），**保持静态分发**，不要为假想的灵活性引入虚表。

**回退时的约束（不可破坏）：**

1. **App/BAL 静态 API 契约不变**——上层/AI 生成看到的仍是 `dal_ultrasonic_read(&dev, &d)`。
2. **多态封装在 DAL 该器件内部**，两种合法手法：
   - **微型 ops 虚表**：在 `dal_ultrasonic.c` 内部定义一张 `static const struct { ... } ops`，
     按实例配置字段（如 `sensor_type`）选择；**仅此一处**间接调用。
   - **静态 `switch-case`**：按 `dev->sensor_type` 在 API 实现内分发到具体硬件路径。

```c
/* 受控的局部多态：封装在 dal_ultrasonic.c 内部，App/BAL 无感知 */
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *out)
{
    switch (dev->sensor_type) {
    case SENSOR_HC_SR04:  return read_hc_sr04(dev, out);
    case SENSOR_VL53L0X:  return read_vl53l0x(dev, out);
    default:              return WINK_ERR_INVALID_PARAM;
    }
}
```

> 关键：上层（App / BAL / 仿真）**完全不知道**内部多态。这既保留了静态命名的生成
> 友好性，又在局部获得了运行时灵活性。器件抽象**整体**仍是 POD + 命名 API。

---

## 三、项目演化方向（ch18 「几座山」在静态分发下的落地）

运行期多态参考基线里讲的 HSM（层次状态机）、Active Object、事件驱动发布订阅，**在静态
分发范式下同样适用且推荐**——它们是「行为组织」与「模块解耦」，与「dispatch 机制」正交：

- 复杂逻辑 → **层次化状态机**（chigo-micro 的 safety 状态机雏形）。
- 跨模块协调 → **事件驱动 + 发布订阅**（不互调，只通过事件）。
- 这些**不依赖 vtable**——状态转移表、事件订阅表都可以是静态数据表。

> 即：本项目「偏离运行期多态」≠「不高级」。复杂业务照样上 HSM / 事件驱动，只是 dispatch
> 走静态分发、器件是 POD。这是本项目与运行期多态基线的**真正分野**。

---

## 演进检验

- [ ] 现有 `bool`/`float` 返回的 DAL API 已迁移到 `wink_status_t`？
- [ ] 无 `if (status)` 残留？
- [ ] `js_sim_*` 签名统一到 Registry？
- [ ] `#ifdef SIMULATION` 已收窄到最低物理信号层？
- [ ] 任何引入的 vtable 都封装在模块内部、App/BAL 无感知？
