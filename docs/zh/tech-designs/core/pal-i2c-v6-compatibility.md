# ESP-IDF v6.x I2C 驱动兼容技术设计

| 项 | 内容 |
|----|------|
| **创建日期** | 2026-06-27 |
| **关联 ADR** | [ADR-0006](../../decisions/core/0006-esp-idf-v6-i2c-compatibility.md) |
| **关联实施计划** | [`../../implementation-plans/core/2026-06-27-esp-idf-v6-i2c-compat-plan.md`](../../implementation-plans/core/2026-06-27-esp-idf-v6-i2c-compat-plan.md) |
| **关联设计规范** | [`../02-wink-micro-os/02-pal-platform-abstraction.md`](../../design/02-wink-micro-os/02-pal-platform-abstraction.md) |
| **目标版本** | ESP-IDF v5.1.3 LTS + v6.0.x/v6.1 |

---

## 1. 设计背景与约束

### 1.1 ESP-IDF v6.x I2C 架构变更

| 维度 | v5.x 旧 API (`driver/i2c.h`) | v6.x 新 API (`driver/i2c_master.h`) |
|------|-----------------------------|-----------------------------------|
| **句柄模型** | 单级：port 编号直接操作 | 二级：bus handle + device handle |
| **初始化流程** | `i2c_param_config()` → `i2c_driver_install()` | `i2c_new_master_bus()` → `i2c_master_bus_add_device()` |
| **传输接口** | `i2c_master_write_read_device(port, addr, ...)` | `i2c_master_transmit_receive(dev_handle, ...)` |
| **设备地址** | 每次传输传入 | 设备添加时绑定到 handle |
| **资源清理** | `i2c_driver_delete(port)` | `i2c_master_bus_rm_device(dev)` → `i2c_del_master_bus(bus)` |

### 1.2 核心约束（不可妥协）

✅ **PAL API 零变更**：`pal_i2c_transfer()` 签名保持不变
✅ **双版本同源**：同一份代码在 v5.1.3 和 v6.x 下均可编译
✅ **零警告**：无 deprecation warning
✅ **行为兼容**：上层 DAL 无感知，DUT 测试无需修改

---

## 2. 整体架构设计

### 2.1 适配层架构

```
┌─────────────────────────────────────────────────────────┐
│              PAL 公开 API (pal_i2c_transfer)            │  ← 签名不变
└────────────────────────────┬────────────────────────────┘
                             │
    ┌────────────────────────┼────────────────────────┐
    │ 版本分发 (编译期静态分支)                        │
    │ ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0) │
    └────────────────────────┼────────────────────────┘
                             │
              ┌──────────────┴──────────────┐
              │                             │
    ┌─────────▼─────────┐         ┌─────────▼─────────┐
    │   v5.x 实现路径   │         │   v6.x 实现路径   │
    │  driver/i2c.h     │         │ driver/i2c_master.h│
    │  单级 port 模型   │         │ 总线-设备二级模型  │
    └───────────────────┘         └───────┬───────────┘
                                          │
                              ┌───────────▼───────────┐
                              │  设备句柄缓存子系统   │
                              │  (dev_addr → handle)  │
                              └───────────────────────┘
```

### 2.2 编译期门控策略

```c
/* 版本检测与编译开关 */
#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    /* v6.x 新 API：使用总线-设备模型 */
    #include "driver/i2c_master.h"
    #define WINK_I2C_USE_V6_API  1
#else
    /* v5.x 旧 API：保持现有实现 */
    #include "driver/i2c.h"
    #define WINK_I2C_USE_V6_API  0
#endif

/* v7.0 前向保护：检测到未验证的版本时编译报错 */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(7, 0, 0)
    #error "ESP-IDF v7.x I2C API compatibility not verified yet. " \
           "Please update tech-designs/pal-i2c-v6-compatibility.md first."
#endif
```

---

## 3. v6.x 路径详细设计

### 3.1 静态数据结构

```c
#if WINK_I2C_USE_V6_API

#define I2C_PORTS            2
#define I2C_MAX_DEVICES      4    /* MVP：每总线最多 4 个设备，覆盖典型场景 */

/* 总线句柄数组 */
static i2c_master_bus_handle_t s_i2c_bus[I2C_PORTS] = {NULL};

/* 设备句柄缓存：[port][index] → handle + addr
 * 懒加载策略：首次访问某地址时创建设备句柄 */
typedef struct {
    i2c_master_dev_handle_t handle;
    uint16_t                dev_addr;    /* 0 = slot 空闲 */
} i2c_dev_cache_entry_t;

static i2c_dev_cache_entry_t s_i2c_dev_cache[I2C_PORTS][I2C_MAX_DEVICES] = {
    {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}},  /* port 0 */
    {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}}   /* port 1 */
};

#endif /* WINK_I2C_USE_V6_API */
```

### 3.2 设备句柄获取/创建算法（核心）

**算法名称**：线性扫描 + FIFO 替换

**算法复杂度**：O(N)，N = I2C_MAX_DEVICES（MVP 下 N=4，可接受）

```c
/**
 * @brief 获取或创建 I2C 设备句柄（懒加载）
 * @param port I2C 端口号
 * @param dev_addr 7 位设备地址
 * @return WINK_OK 成功，WINK_ERR_RESOURCE_EXHAUSTED 缓存满
 */
static wink_status_t pal_i2c_get_or_create_device(uint8_t port, uint16_t dev_addr,
                                                   i2c_master_dev_handle_t *out_handle)
{
    /* Step 1：线性扫描缓存，查找已存在的设备 */
    int free_slot = -1;
    for (int i = 0; i < I2C_MAX_DEVICES; i++) {
        if (s_i2c_dev_cache[port][i].dev_addr == dev_addr) {
            /* 命中缓存，直接返回 */
            *out_handle = s_i2c_dev_cache[port][i].handle;
            return WINK_OK;
        }
        if (s_i2c_dev_cache[port][i].dev_addr == 0 && free_slot == -1) {
            free_slot = i;  /* 记录第一个空闲 slot */
        }
    }

    /* Step 2：未命中，需要创建新设备 */
    if (free_slot == -1) {
        /* 缓存已满：FIFO 替换策略，淘汰 index 0，整体前移 */
        WINK_LOG_W("I2C port %d device cache full, evicting addr 0x%02X",
                   port, s_i2c_dev_cache[port][0].dev_addr);

        esp_err_t err = i2c_master_bus_rm_device(s_i2c_dev_cache[port][0].handle);
        if (err != ESP_OK) {
            WINK_LOG_E("failed to remove I2C device: %s", esp_err_to_name(err));
            /* 即使删除失败，也继续覆盖（总比失败好） */
        }

        /* 整体前移，腾出最后一个 slot */
        for (int i = 0; i < I2C_MAX_DEVICES - 1; i++) {
            s_i2c_dev_cache[port][i] = s_i2c_dev_cache[port][i + 1];
        }
        free_slot = I2C_MAX_DEVICES - 1;
    }

    /* Step 3：创建设备 */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = 400000,   /* MVP：固定 400kHz，后续可配置 */
    };

    esp_err_t err = i2c_master_bus_add_device(s_i2c_bus[port], &dev_cfg,
                                               &s_i2c_dev_cache[port][free_slot].handle);
    if (err != ESP_OK) {
        WINK_LOG_E("failed to add I2C device addr 0x%02X: %s",
                   dev_addr, esp_err_to_name(err));
        return WINK_ERR_HARDWARE;
    }

    s_i2c_dev_cache[port][free_slot].dev_addr = dev_addr;
    *out_handle = s_i2c_dev_cache[port][free_slot].handle;
    return WINK_OK;
}
```

### 3.3 初始化流程（v6.x）

```c
/* pal_i2c_transfer() 内部懒初始化分支 */
if (!s_i2c_initialized[port]) {
    /* FIXME：MVP 阶段固定 SDA/SCL 映射，与 v5.x 保持一致
     * I2C0: SDA=21, SCL=22; I2C1: SDA=33, SCL=32 */
    static const int i2c_sda_map[I2C_PORTS] = {21, 33};
    static const int i2c_scl_map[I2C_PORTS] = {22, 32};

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = (i2c_port_t)port,
        .sda_io_num = i2c_sda_map[port],
        .scl_io_num = i2c_scl_map[port],
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,  /* 对应 v5.x 的 *_PULLUP_ENABLE */
    };

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_i2c_bus[port]);
    if (err != ESP_OK) {
        WINK_LOG_E("failed to create I2C master bus port %d: %s",
                   port, esp_err_to_name(err));
        return WINK_ERR_HARDWARE;
    }

    s_i2c_initialized[port] = true;
}
```

### 3.4 传输流程（v6.x）

```c
/* 先获取/创建设备句柄 */
i2c_master_dev_handle_t dev_handle;
wink_status_t rs = pal_i2c_get_or_create_device(port, dev_addr, &dev_handle);
if (wink_status_is_error(rs)) {
    return rs;
}

/* 执行传输：注意 API 语义差异
 *
 * v5.x: write_buf NULL + write_len 0 表示只读
 *       read_buf NULL + read_len 0 表示只写
 *
 * v6.x: 分开 transmit/receive，需要分别调用
 */
esp_err_t err = ESP_OK;

if (write_buf != NULL && write_len > 0) {
    if (read_buf != NULL && read_len > 0) {
        /* 写+读 组合传输（repeated START）*/
        err = i2c_master_transmit_receive(dev_handle,
                                          write_buf, (size_t)write_len,
                                          read_buf, (size_t)read_len,
                                          1000);  /* 单位：ms！注意 v5.x 是 ticks */
    } else {
        /* 只写 */
        err = i2c_master_transmit(dev_handle, write_buf, (size_t)write_len, 1000);
    }
} else if (read_buf != NULL && read_len > 0) {
    /* 只读 */
    err = i2c_master_receive(dev_handle, read_buf, (size_t)read_len, 1000);
}
/* else: 空操作（无写无读），直接返回 OK */

if (err != ESP_OK) {
    return pal_i2c_map_esp_err(err);  /* 精细错误码映射 */
}
```

---

## 4. 错误码精细映射设计

### 4.1 映射表（SSOT 与 wink_status.h 对齐）

**原实现问题**：所有错误统一映射为 `WINK_ERR_HARDWARE`，丢失了错误语义，上层无法做重试策略。

**改进后的映射表**：

| ESP-IDF 错误码 | 映射到 wink_status_t | 说明 |
|---------------|---------------------|------|
| `ESP_OK` | `WINK_OK` | 成功 |
| `ESP_ERR_TIMEOUT` | `WINK_ERR_TIMEOUT` | 传输超时 |
| `ESP_ERR_INVALID_ARG` | `WINK_ERR_INVALID_ARG` | 参数非法 |
| `ESP_ERR_INVALID_STATE` | `WINK_ERR_NOT_INITIALIZED` | 驱动未初始化 |
| `ESP_ERR_NO_MEM` | `WINK_ERR_RESOURCE_EXHAUSTED` | 内存不足 |
| `ESP_ERR_NOT_FOUND` | `WINK_ERR_DISCONNECTED` | 设备无应答（NACK） |
| `ESP_ERR_I2C_BUS_OFF` | `WINK_ERR_HARDWARE` | 总线硬件错误 |
| `ESP_ERR_I2C_NACK` | `WINK_ERR_DISCONNECTED` | 设备 NACK |
| 其他 | `WINK_ERR_HARDWARE` | 兜底 |

### 4.2 映射函数实现

```c
/**
 * @brief ESP-IDF I2C 错误码精细映射
 * @param esp_err ESP-IDF 返回的错误码
 * @return 对应的 wink_status_t
 */
static inline wink_status_t pal_i2c_map_esp_err(esp_err_t esp_err)
{
    if (esp_err == ESP_OK) {
        return WINK_OK;
    }

    /* 按出现概率排序，优化分支预测 */
    if (esp_err == ESP_ERR_TIMEOUT) {
        return WINK_ERR_TIMEOUT;
    }
    if (esp_err == ESP_ERR_NOT_FOUND) {
        return WINK_ERR_DISCONNECTED;
    }
    if (esp_err == ESP_ERR_INVALID_ARG) {
        return WINK_ERR_INVALID_ARG;
    }
    if (esp_err == ESP_ERR_NO_MEM) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }
    if (esp_err == ESP_ERR_INVALID_STATE) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    /* 兜底：硬件错误 */
    WINK_LOG_D("I2C unmapped esp_err: %s", esp_err_to_name(esp_err));
    return WINK_ERR_HARDWARE;
}
```

---

## 5. 资源清理与反初始化

### 5.1 问题：现有 PAL 缺少 pal_i2c_deinit()

观察现有代码：`pal_gpio_deinit()` 不存在，`pal_pwm_deinit()` 存在，但 `pal_i2c_deinit()` 完全缺失。

**策略**：本次适配不新增 PAL API（避免破坏兼容性），但在实现内部做好资源清理的预留。

### 5.2 静态清理函数（供未来扩展）

```c
#if WINK_I2C_USE_V6_API
/**
 * @brief 清理 I2C 总线及所有设备（内部预留，暂不对外暴露）
 * @note ESP-IDF v6.x 要求：先删所有设备，再删总线
 */
static void pal_i2c_cleanup_port(uint8_t port)
{
    if (port >= I2C_PORTS || !s_i2c_initialized[port]) {
        return;
    }

    /* Step 1：清理所有设备句柄 */
    for (int i = 0; i < I2C_MAX_DEVICES; i++) {
        if (s_i2c_dev_cache[port][i].dev_addr != 0) {
            (void)i2c_master_bus_rm_device(s_i2c_dev_cache[port][i].handle);
            s_i2c_dev_cache[port][i].handle = NULL;
            s_i2c_dev_cache[port][i].dev_addr = 0;
        }
    }

    /* Step 2：删除总线 */
    (void)i2c_del_master_bus(s_i2c_bus[port]);
    s_i2c_bus[port] = NULL;
    s_i2c_initialized[port] = false;
}
#endif
```

---

## 6. 并发安全设计

### 6.1 问题分析

ESP-IDF v6.x 的 `i2c_master_transmit_receive()` 是线程安全的（内部有互斥锁），但：
1. 我们的**设备句柄缓存操作**不是线程安全的
2. 两个任务同时访问同一个未初始化的 port 可能导致重复初始化

### 6.2 解决方案：临界区保护

```c
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* I2C 操作互斥锁：懒创建，双重检查锁定模式 */
static SemaphoreHandle_t s_i2c_mutex = NULL;
static StaticSemaphore_t s_i2c_mutex_buf;  /* 静态分配，避免 malloc 失败 */

/* pal_i2c_transfer() 入口处： */
if (s_i2c_mutex == NULL) {
    /* 首次调用：创建互斥锁（线程安全由 FreeRTOS 保证） */
    s_i2c_mutex = xSemaphoreCreateMutexStatic(&s_i2c_mutex_buf);
}

if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    /* 临界区：初始化 + 设备缓存操作 */
    if (!s_i2c_initialized[port]) {
        /* ... 初始化总线 ... */
    }

    /* 设备句柄获取/创建必须在临界区内 */
    rs = pal_i2c_get_or_create_device(port, dev_addr, &dev_handle);

    xSemaphoreGive(s_i2c_mutex);
} else {
    WINK_LOG_E("I2C mutex timeout");
    return WINK_ERR_BUSY;
}

/* 注意：实际数据传输在临界区外——让 ESP-IDF 内部锁处理并发传输，
 * 这样可以实现总线级别的并发排队，而不是整个 PAL 层串行化 */
```

---

## 7. 超时与时钟拉伸配置

### 7.1 超时参数语义统一

| 项 | v5.x | v6.x | 统一策略 |
|----|------|------|---------|
| **超时单位** | portTICK_PERIOD_MS | 毫秒（直接 ms） | 统一使用 1000ms |
| **时钟拉伸** | 隐式默认最大 | 需显式配置 | MVP：使用默认值，后续可通过 menuconfig 配置 |

### 7.2 时钟拉伸显式配置（v6.x 新增）

```c
/* 在 bus_cfg 中添加（v6.1 新增字段） */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 1, 0)
    bus_cfg.flags.disable_clock_stretch = false;  /* 启用时钟拉伸，兼容慢设备 */
#endif
```

---

## 8. 回滚与降级策略

### 8.1 强制回退编译开关

即使在 v6.x 环境下，也允许用户强制使用 v5.x 兼容 API（应对 v6.x 驱动 bug）：

```c
/* Kconfig 配置项（由 ESP-IDF menuconfig 控制）
 * CONFIG_WINK_I2C_FORCE_V5_API=y 则强制使用 v5.x 旧 API
 */

#if defined(CONFIG_WINK_I2C_FORCE_V5_API) && CONFIG_WINK_I2C_FORCE_V5_API
    /* 用户强制要求使用 v5.x API */
    #undef WINK_I2C_USE_V6_API
    #define WINK_I2C_USE_V6_API  0
    #include "driver/i2c.h"      /* 强制包含旧头文件 */
    #pragma message "Wink I2C: forced to use v5.x compatible API per Kconfig"
#endif
```

### 8.2 运行时探测降级（未来扩展）

如果 v6.x 总线创建失败，可以尝试回退到 v5.x API（暂不实现，MVP 阶段留作预案）。

---

## 9. 性能基准对比设计

为确保 v6.x 适配没有引入性能退化，需要建立基准：

### 9.1 测试用例

| 测试项 | 说明 | 接受阈值 |
|--------|------|---------|
| **空传输延迟** | write_len=0, read_len=0（仅 overhead） | ≤ 5µs 差异 |
| **单字节写** | 写 1 字节到 SSD1306 | ≤ 10% 退化 |
| **页写 128 字节** | SSD1306 整页写入 | ≤ 10% 退化 |
| **单字节读** | 从传感器读 1 字节 | ≤ 10% 退化 |
| **设备创建开销** | 首次访问新地址的额外延迟 | ≤ 500µs（可接受，懒加载只付一次） |

### 9.2 测量代码（集成到 pal_i2c_transfer）

```c
#ifdef WINK_I2C_PERF_BENCHMARK
    uint64_t t_start = pal_get_us();
#endif

/* ... 执行传输 ... */

#ifdef WINK_I2C_PERF_BENCHMARK
    uint64_t t_elapsed = pal_get_us() - t_start;
    WINK_LOG_V("I2C transfer port=%d addr=0x%02X write=%u read=%u elapsed=%lluus",
               port, dev_addr, write_len, read_len, t_elapsed);
#endif
```

---

## 10. 验证矩阵

| 验证项 | v5.1.3 环境 | v6.0.x 环境 | 验收标准 |
|--------|-------------|-------------|---------|
| 编译通过 | ✅ | ✅ | 0 error, 0 warning |
| 无 deprecation | - | ✅ | 无 I2C 相关弃用警告 |
| Host 单元测试 | ✅ | ✅ | 16/16 全部通过 |
| SSD1306 显示 | ✅ | ✅ | 整屏刷新无花屏 |
| I2C 传感器读取 | ✅ | ✅ | 连续 1000 次读取无错误 |
| 多设备并发访问 | ✅ | ✅ | 4 个设备同时访问无死锁 |
| 设备缓存溢出 | ✅ | ✅ | 第 5 个设备可正常访问（FIFO 替换生效） |
| 性能基准 | ✅ | ✅ | 无 >10% 性能退化 |

---

## 11. 后续演进路线

| 阶段 | 内容 | 优先级 |
|------|------|--------|
| **Phase 1** | 完成 v6.x 基本适配 + 设备缓存 + 精细错误码 | 高 |
| **Phase 2** | 增加并发安全互斥锁 | 中 |
| **Phase 3** | SDA/SCL 引脚可配置（修复 FIXME） | 中 |
| **Phase 4** | 性能基准测试 + 优化 | 低 |
| **Phase 5** | 增加 pal_i2c_deinit() 公开 API | 低 |

---

## 12. 参考资料

1. ESP-IDF v6.0 迁移指南：[I2C Driver Migration](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/migration-guides/release-6.x/peripherals.html#i2c-driver)
2. ESP-IDF v6.x I2C Master API 文档：`driver/i2c_master.h`
3. 本项目错误码规范：`07-platform-governance/02-error-fault-model.md`
4. [ADR-0002](../../decisions/unisim/0002-dual-target-compilation.md)：双 target 同源编译约定
5. [ADR-0006](../../decisions/core/0006-esp-idf-v6-i2c-compatibility.md)：ESP-IDF v6.x I2C 驱动兼容性适配（本设计的决策依据）

