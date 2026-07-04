/**
 * @file pal_hal_i2c_esp32.c
 * @brief ESP32 target 的 I2C 主控实现（ESP-IDF v5.x / v6.x 双 API 兼容）：
 *        pal_i2c_transfer + weak pal_i2c_pin_map 默认 + 设备缓存 LRU + SMP 安全互斥锁。
 *
 * 由 targets/esp32/pal_hal_esp32.c 拆出（PLAN-20260701-PAL-TARGET-P1-MAINT Task 2 Step 5）。
 * 契约不变：仅物理位置调整；见 pal/include/pal_hal.h 的 pal_i2c_transfer 契约。
 *
 * ✅ @verified: HARDWARE-SMOKE-PASSED (DevKitC, 2026-06-27)
 *    - I2C v6 master bus scan (3 addresses NACK, no panic)
 *
 * ✅ R-1：pal_i2c_transfer 公共签名、返回码集合、v5/v6 传输语义均逐字保留。
 * ✅ R-4：全文件仅 1 处最外层 `#if defined(ESP_PLATFORM)`，包住所有 IDF 头文件
 *   与实现；WINK_I2C_USE_V6_API 是外层 guard 内部的版本门控，非平台 guard。
 *
 * MVP status:
 * - v5/v6 双 API 通过 WINK_I2C_USE_V6_API 门控 + CONFIG_WINK_I2C_FORCE_V5_API 覆盖
 * - v7.x 前向保护：编译期 #error 拦截未验证版本
 * - LRU 设备缓存 + 事务安全的 pal_i2c_get_or_create_device()
 * - constructor(101) 静态初始化互斥锁，避免双核懒初始化竞态
 */
#include "pal_hal.h"

#if defined(ESP_PLATFORM)
#include "esp_err.h"
#include "esp_idf_version.h"

/* ─────────────────────────────────────────────────────────
 * I2C 版本门控：ESP-IDF v6.x 使用新的 driver/i2c_master.h
 * ───────────────────────────────────────────────────────── */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    /* v6.x 新 API：总线-设备二级模型 */
    #include "driver/i2c_master.h"
    #define WINK_I2C_USE_V6_API  1
#else
    /* v5.x 旧 API：单级 port 模型 */
    #include "driver/i2c.h"
    #define WINK_I2C_USE_V6_API  0
#endif

/* v7.0 前向保护：检测到未验证的版本时编译报错 */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(7, 0, 0)
    #error "ESP-IDF v7.x I2C API compatibility not verified yet. " \
           "Please update tech-designs/pal-i2c-v6-compatibility.md first."
#endif

/* 强制回退开关：Kconfig 可配置强制使用 v5.x API */
#if defined(CONFIG_WINK_I2C_FORCE_V5_API) && CONFIG_WINK_I2C_FORCE_V5_API
    #undef WINK_I2C_USE_V6_API
    #define WINK_I2C_USE_V6_API  0
    #pragma message "Wink I2C: forced to use v5.x compatible API per Kconfig"
#endif

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "wink_pal_i2c";

/* I2C 引脚弱默认：无 board_config.c 强覆盖时使用。
 * I2C0: SDA=21, SCL=22; I2C1: SDA=33, SCL=32 */
__attribute__((weak)) const wink_pin_t pal_i2c_pin_map[PAL_I2C_PORTS][2] = {
    {21, 22},
    {33, 32}
};

/* ─────────────────────────────────────────────────────────
 * I2C 实现（v5.x / v6.x 双版本兼容）
 * ───────────────────────────────────────────────────────── */

#define I2C_MAX_DEVICES      4    /* MVP：每总线最多 4 个设备 */
#define I2C_TRANSFER_TIMEOUT_MS  1000

static bool s_i2c_initialized[PAL_I2C_PORTS] = {false};

/* 并发安全：静态互斥锁，保护初始化与设备缓存操作
 * ✅ SMP-safe: FreeRTOS 调度器启动前静态初始化，无双核竞态
 * 参见: https://www.freertos.org/xSemaphoreCreateMutexStatic.html */
static SemaphoreHandle_t s_i2c_mutex = NULL;
static StaticSemaphore_t s_i2c_mutex_buf;

/* 在启动时初始化互斥锁，避免双核懒初始化竞态
 * constructor(101) 确保在应用层代码之前执行 */
__attribute__((constructor(101)))
static void pal_i2c_static_init_mutex(void) {
    s_i2c_mutex = xSemaphoreCreateMutexStatic(&s_i2c_mutex_buf);
}

#if WINK_I2C_USE_V6_API
/* v6.x：总线-设备二级模型 */
static i2c_master_bus_handle_t s_i2c_bus[PAL_I2C_PORTS] = {NULL};

typedef struct {
    i2c_master_dev_handle_t handle;
    uint16_t                dev_addr;    /* 0 = slot 空闲 */
} i2c_dev_cache_entry_t;

static i2c_dev_cache_entry_t s_i2c_dev_cache[PAL_I2C_PORTS][I2C_MAX_DEVICES] = {
    {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}},
    {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}}
};

/**
 * @brief ESP-IDF I2C 错误码精细映射
 */
static inline wink_status_t pal_i2c_map_esp_err(esp_err_t esp_err)
{
    if (esp_err == ESP_OK) {
        return WINK_OK;
    }
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
    ESP_LOGD(TAG, "unmapped esp_err: %s", esp_err_to_name(esp_err));
    return WINK_ERR_HARDWARE;
}

/**
 * @brief 将缓存条目移动到队尾（LRU 热更新）
 * @note 每次命中后调用，确保访问频率高的设备不被淘汰
 */
static inline void pal_i2c_lru_touch(uint8_t port, int hit_idx)
{
    i2c_dev_cache_entry_t hit = s_i2c_dev_cache[port][hit_idx];
    for (int i = hit_idx; i < I2C_MAX_DEVICES - 1; i++) {
        if (s_i2c_dev_cache[port][i + 1].dev_addr == 0) {
            s_i2c_dev_cache[port][i] = hit;
            return;
        }
        s_i2c_dev_cache[port][i] = s_i2c_dev_cache[port][i + 1];
    }
    s_i2c_dev_cache[port][I2C_MAX_DEVICES - 1] = hit;
}

/**
 * @brief 获取或创建 I2C 设备句柄（懒加载 + LRU 替换 + 事务安全）
 * @note 必须在持有 s_i2c_mutex 的情况下调用
 * @design
 *   - ✅ LRU: 命中时将条目移到队尾，淘汰时总是淘汰 index 0（最久未用）
 *   - ✅ 事务安全: 先分配临时句柄，仅在完全成功后才修改缓存
 *   - ✅ 无中间状态: 即使创建设备失败，缓存也不会被破坏
 */
static wink_status_t pal_i2c_get_or_create_device(uint8_t port, uint16_t dev_addr,
                                                   i2c_master_dev_handle_t *out_handle)
{
    /* Step 1：线性扫描缓存，查找已存在的设备 */
    int free_slot = -1;
    for (int i = 0; i < I2C_MAX_DEVICES; i++) {
        if (s_i2c_dev_cache[port][i].dev_addr == dev_addr) {
            /* ✅ LRU: 命中时将该条目移到队尾（提高后续访问局部性） */
            pal_i2c_lru_touch(port, i);
            *out_handle = s_i2c_dev_cache[port][I2C_MAX_DEVICES - 1].handle;
            return WINK_OK;
        }
        if (s_i2c_dev_cache[port][i].dev_addr == 0 && free_slot == -1) {
            free_slot = i;
        }
    }

    /* Step 2：先在栈上分配临时句柄，不修改任何缓存状态 */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = 400000,
    };

    i2c_master_dev_handle_t temp_handle = NULL;
    esp_err_t err = i2c_master_bus_add_device(s_i2c_bus[port], &dev_cfg, &temp_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to add I2C device addr 0x%02X: %s",
                 dev_addr, esp_err_to_name(err));
        return pal_i2c_map_esp_err(err);
    }

    /* Step 3: 需要淘汰缓存（仅在设备创建成功后执行） */
    if (free_slot == -1) {
        ESP_LOGW(TAG, "port %d device cache full, LRU evicting addr 0x%02X",
                 port, s_i2c_dev_cache[port][0].dev_addr);

        esp_err_t rm_err = i2c_master_bus_rm_device(s_i2c_dev_cache[port][0].handle);
        if (rm_err != ESP_OK) {
            ESP_LOGW(TAG, "evict device 0x%02X failed: %s (ignoring)",
                     s_i2c_dev_cache[port][0].dev_addr, esp_err_to_name(rm_err));
        }

        for (int i = 0; i < I2C_MAX_DEVICES - 1; i++) {
            s_i2c_dev_cache[port][i] = s_i2c_dev_cache[port][i + 1];
        }
        free_slot = I2C_MAX_DEVICES - 1;
    }

    /* Step 4：唯一的缓存写入点 - 原子性提交 */
    s_i2c_dev_cache[port][free_slot].handle = temp_handle;
    s_i2c_dev_cache[port][free_slot].dev_addr = dev_addr;
    *out_handle = temp_handle;
    return WINK_OK;
}
#else  /* WINK_I2C_USE_V6_API == 0：v5.x 旧 API */
/**
 * @brief v5.x 错误码映射（复用相同的精细映射，v5.x 也受益）
 */
static inline wink_status_t pal_i2c_map_esp_err(esp_err_t esp_err)
{
    if (esp_err == ESP_OK) {
        return WINK_OK;
    }
    if (esp_err == ESP_ERR_TIMEOUT) {
        return WINK_ERR_TIMEOUT;
    }
    if (esp_err == ESP_ERR_INVALID_ARG) {
        return WINK_ERR_INVALID_ARG;
    }
    if (esp_err == ESP_ERR_INVALID_STATE) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    /* v5.x i2c_master_write_read_device 对 NACK 返回 ESP_FAIL */
    if (esp_err == ESP_FAIL) {
        return WINK_ERR_DISCONNECTED;
    }
    ESP_LOGD(TAG, "unmapped esp_err: %s", esp_err_to_name(esp_err));
    return WINK_ERR_HARDWARE;
}
#endif /* WINK_I2C_USE_V6_API */

wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                      const uint8_t *write_buf, uint32_t write_len,
                      uint8_t *read_buf, uint32_t read_len) {
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }

    /* 临界区：初始化 + 设备缓存操作
     * ✅ 互斥锁已在 constructor 中静态初始化，SMP 安全 */
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "I2C mutex timeout");
        return WINK_ERR_BUSY;
    }

    if (!s_i2c_initialized[port]) {
        /* SDA/SCL 物理路由来自 pal_i2c_pin_map（board_config.c 强定义，
         * 缺省时回落至本 TU 的弱默认值）。*/
#if WINK_I2C_USE_V6_API
        /* v6.x：总线初始化 */
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = (i2c_port_t)port,
            .sda_io_num = pal_i2c_pin_map[port][0],
            .scl_io_num = pal_i2c_pin_map[port][1],
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_i2c_bus[port]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "failed to create I2C master bus port %d: %s",
                     port, esp_err_to_name(err));
            xSemaphoreGive(s_i2c_mutex);
            return WINK_ERR_HARDWARE;
        }
#else
        /* v5.x：旧 API 初始化 */
        i2c_config_t cfg = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = pal_i2c_pin_map[port][0],
            .scl_io_num = pal_i2c_pin_map[port][1],
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = 400000,
        };
        esp_err_t err = i2c_param_config((i2c_port_t)port, &cfg);
        if (err != ESP_OK) {
            xSemaphoreGive(s_i2c_mutex);
            return WINK_ERR_HARDWARE;
        }
        err = i2c_driver_install((i2c_port_t)port, cfg.mode, 0, 0, 0);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            xSemaphoreGive(s_i2c_mutex);
            return WINK_ERR_HARDWARE;
        }
#endif /* WINK_I2C_USE_V6_API */
        s_i2c_initialized[port] = true;
    }

    wink_status_t rs = WINK_OK;
    esp_err_t err = ESP_OK;

#if WINK_I2C_USE_V6_API
    /* v6.x：先获取/创建设备句柄（在临界区内） */
    i2c_master_dev_handle_t dev_handle = NULL;
    rs = pal_i2c_get_or_create_device(port, dev_addr, &dev_handle);
    if (wink_status_is_error(rs)) {
        xSemaphoreGive(s_i2c_mutex);
        return rs;
    }
#endif /* WINK_I2C_USE_V6_API */

    /* 实际数据传输：持有 s_i2c_mutex 锁以防止并发时设备被驱逐引致 UAF 漏洞 */
#if WINK_I2C_USE_V6_API
    if (write_buf != NULL && write_len > 0) {
        if (read_buf != NULL && read_len > 0) {
            /* 写+读 组合传输（repeated START） */
            err = i2c_master_transmit_receive(dev_handle,
                                              write_buf, (size_t)write_len,
                                              read_buf, (size_t)read_len,
                                              I2C_TRANSFER_TIMEOUT_MS);
        } else {
            /* 只写 */
            err = i2c_master_transmit(dev_handle, write_buf, (size_t)write_len,
                                      I2C_TRANSFER_TIMEOUT_MS);
        }
    } else if (read_buf != NULL && read_len > 0) {
        /* 只读 */
        err = i2c_master_receive(dev_handle, read_buf, (size_t)read_len,
                                 I2C_TRANSFER_TIMEOUT_MS);
    }
    /* else: 空操作，直接返回 OK */
#else
    /* v5.x：旧 API 传输 */
    err = i2c_master_write_read_device(
        (i2c_port_t)port, dev_addr,
        write_buf, (size_t)write_len,
        read_buf, (size_t)read_len,
        pdMS_TO_TICKS(I2C_TRANSFER_TIMEOUT_MS)
    );
#endif /* WINK_I2C_USE_V6_API */

    xSemaphoreGive(s_i2c_mutex);

    if (err != ESP_OK) {
        return pal_i2c_map_esp_err(err);
    }
    return WINK_OK;
}

#else /* !ESP_PLATFORM: non-IDF stub for static analysis. */

wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                      const uint8_t *write_buf, uint32_t write_len,
                      uint8_t *read_buf, uint32_t read_len)
{
    (void)port; (void)dev_addr; (void)write_buf; (void)write_len;
    (void)read_buf; (void)read_len;
    return WINK_ERR_UNSUPPORTED;
}

#endif /* ESP_PLATFORM */
