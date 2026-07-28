# 内存安全（范式无关）

> 适用范围：两种架构风格共用。

---

## 实时路径禁止 malloc / free（两项目共识）

- **PID 回调、中断、1kHz 快路径、Wasm 仿真热路径**——一律禁用 `malloc/free`。
- 所有结构体**静态分配**，或通过 FreeRTOS 队列 / 事件传递。
- chigo-micro：`c-embedded.md` 明文「实时路径禁止 malloc/free」。
- wink-micro-os：器件实例是 codegen 生成的静态全局（`device_tree.c`），无动态分配。

---

## 堆分配规则（仅在非实时路径允许时）

1. **每次分配都检查 NULL**。
2. **每次分配都配对释放**，并文档化所有权（谁负责释放）。
3. **按相反顺序释放**（A→B→C 分配，则 free C→B→A）。
4. **释放后置空指针**（防 use-after-free / double-free）。
5. **错误路径必须清理**已分配资源——用 `goto` 标签集中清理：

```c
int module_init(module_t *self)
{
    self->buf_a = platform_malloc(BUF_A_SIZE);
    if (self->buf_a == NULL) { return ERR_NO_MEMORY; }
    self->buf_b = platform_malloc(BUF_B_SIZE);
    if (self->buf_b == NULL) { goto fail_b; }
    self->queue = queue_create(DEPTH);
    if (self->queue == NULL) { goto fail_queue; }
    return 0;
fail_queue:
    platform_free(self->buf_b); self->buf_b = NULL;
fail_b:
    platform_free(self->buf_a); self->buf_a = NULL;
    return ERR_NO_MEMORY;
}
```

> ⚠ 嵌入式平台用**非标准 allocator**：`platform_malloc` / `pvPortMalloc` / `os_malloc` /
> `mem_alloc`。**绝不混用不同 allocator 家族**——先搜项目里现有用法再跟随。

---

## 明令禁止的 API（及替代）

| 禁止 | 原因 | 替代 |
|------|------|------|
| **VLA**（变长数组 `int a[n]`） | 栈大小不可控，易爆栈 | 固定大小数组 + 长度参数 |
| **`strcpy`** | 无界写入 | `snprintf(dst, sizeof dst, "%s", src)`（首选）或 `memcpy` + 手动 `dst[n-1]='\0'`；**勿用 `strncpy`**（`strlen(src)≥n` 时不补 `\0`，且 null 填满低效） |
| **`sprintf`** | 无界写入 | `snprintf` |
| **`gets`** | 无界写入 | `fgets` |
| **`alloca`** | 栈分配，不可控 | 静态缓冲或堆 |

- 缓冲区**必须连同长度一起传**；拷贝前校验输入长度 ≤ 目标容量。
- 缓冲区大小定义为宏，禁止魔法数字。

---

## 缓冲区溢出防护

- `memcpy`/`memmove` 的 size 必须 ≤ 目标缓冲区容量。
- chigo-micro：数组拷贝用 `memcpy(dst, src, sizeof(dst))`，而非逐元素。
- 协议帧解析：先校验长度字段再解析（见 chigo-micro CMD/STATE/PARAM 帧 + CRC16）。

---

## 栈溢出防护

- **禁止递归**（或严格有界 + 可证明安全）。
- 评估大型局部变量；栈紧张时改堆。
- 合理的调用链深度。
- FreeRTOS 任务栈按「最深调用链 + 余量」配置；用 `uxTaskGetStackHighWaterMark` 监控。

### 栈用量静态分析（强制）

构建系统必须启用栈用量分析并设置阈值：

```makefile
# GCC/Clang 启用栈用量输出
CFLAGS += -fstack-usage

# 每个函数的 .su 文件生成后，用脚本检查阈值：
# - 普通函数：栈使用 < 256 字节
# - 中断服务程序：栈使用 < 128 字节
# - 热路径（PID 回调）：栈使用 < 64 字节
```

> 超过阈值的函数必须重构：拆分子函数、将大型局部变量改为静态或堆分配。

### 环形缓冲内存屏障（ESP32 多核强制）

多核环境下，环形缓冲读写指针更新前必须加内存屏障：

```c
void ringbuf_write(ringbuf_t *self, uint8_t data)
{
    uint32_t next = (self->write_idx + 1) % self->size;
    
    /* 确保数据写入完成后再更新写指针 */
    __sync_synchronize();  /* 全内存屏障 */
    
    self->buf[self->write_idx] = data;
    self->write_idx = next;
}
```

> ESP32 Xtensa 是宽松内存模型，CPU 可能乱序执行。内存屏障确保可见性和执行顺序。

> 缓冲区 / 栈的运行时校验也出现在 [safety-checklist.md](./safety-checklist.md) 阶段 3、6。

---

## Struct 布局与序列化（硬规）

运行时/DAL structs 是**内存中的状态**，不是 wire/flash 布局。永远不要混淆两者。

### 自然对齐，禁止 packed

DAL/运行时 POD 结构体 **必须自然对齐**——**禁止** `__attribute__((packed))` / `#pragma pack`。
在 ARM/Xtensa 上，packed/未对齐访问会：
- 降低性能（产生额外的加载/存储指令）
- 在某些架构上引发 Alignment Fault / Hard Fault
- 导致未定义行为

让编译器负责填充和对齐。

### 成员排序

按对齐要求**降序排列**成员，以最小化尾部填充并保持布局清晰：

```c
/* ✅ 正确：按对齐大小降序排列 */
typedef struct {
    uint64_t  timestamp_us;    /* 8B */
    double    temperature;     /* 8B */
    uint32_t  sample_count;    /* 4B */
    float     current_angle;   /* 4B */
    uint16_t  status;          /* 2B */
    uint8_t   retry_count;     /* 1B */
    bool      is_valid;        /* 1B */
} sensor_data_t;

/* ❌ 错误：随意排列，产生大量填充且可读性差 */
typedef struct {
    uint8_t   retry_count;     /* 1B + 7B 填充 */
    uint64_t  timestamp_us;    /* 8B */
    bool      is_valid;        /* 1B + 3B 填充 */
    uint32_t  sample_count;    /* 4B */
    float     current_angle;   /* 4B */
    uint16_t  status;          /* 2B */
    double    temperature;     /* 8B */
} sensor_data_bad_t;
```

### 配置区与状态区分离（强制 DAL 器件结构）

所有 DAL 器件结构体必须明确分离 `const` 配置区和可变状态区：

```c
/* ✅ 标准模板：配置区 + 状态区 显式分离 */
typedef struct {
    /* --- 配置区：codegen 初始化后永不修改 --- */
    const uint8_t  pwm_channel;
    const float    min_pulse_ms;
    const float    max_pulse_ms;
    const uint32_t init_timeout_us;
    
    /* --- 状态区：运行时可变 --- */
    struct {
        float      current_angle;
        bool       is_enabled;
        uint32_t   last_update_us;
        uint8_t    retry_count;
    } state;
} dal_rc_servo_t;
```

> 理由：
> 1. `const` 配置区编译器放入 Flash，节省 RAM
> 2. 意外修改配置区会触发编译错误（写入 const）
> 3. 状态区集中管理，序列化时只需持久化这部分
> 4. 便于 codegen 静态初始化配置区

### 分离 Wire/Flash 结构体

跨进程/跨 target 边界的数据结构（网络帧、持久化记录）必须**独立命名和定义**：

```c
/* ✅ 正确：明确的 wire 格式结构体 */
typedef struct {
    uint32_t  version;         /* 格式版本号 */
    uint32_t  data_len;        /* 数据长度 */
    uint16_t  crc16;           /* 校验和 */
    uint8_t   payload[64];     /* 载荷 */
} config_frame_wire_t;

typedef struct {
    uint32_t  magic;           /* 魔术字 */
    uint32_t  version;         /* 版本号 */
    uint32_t  crc32;           /* 校验和 */
    uint8_t   data[256];       /* 数据 */
} config_record_flash_t;
```

**永远不要**将运行时 POD 结构体直接 `memcpy` 到 wire/flash。

### 显式序列化/反序列化

所有跨边界的数据转换必须通过显式的序列化/反序列化函数：

```c
wink_status_t config_serialize(const config_t *runtime,
                                config_frame_wire_t *wire)
{
    ASSERT(runtime != NULL);
    ASSERT(wire != NULL);

    wire->version  = htole32(CONFIG_FORMAT_VERSION);
    wire->data_len = htole32(sizeof(config_t));
    /* ... 字段逐一拷贝，处理字节序 ... */
    wire->crc16 = calculate_crc16(wire->payload, wire->data_len);

    return WINK_OK;
}

wink_status_t config_deserialize(config_t *runtime,
                                  const config_frame_wire_t *wire)
{
    ASSERT(runtime != NULL);
    ASSERT(wire != NULL);

    /* 先校验版本、长度、CRC */
    if (le32toh(wire->version) != CONFIG_FORMAT_VERSION) {
        return WINK_ERR_VERSION_MISMATCH;
    }
    if (!verify_crc16(wire->payload, le32toh(wire->data_len),
                      wire->crc16)) {
        return WINK_ERR_CRC_FAILED;
    }

    /* ... 字段逐一拷贝，处理字节序 ... */
    return WINK_OK;
}
```

这样可以：
- 处理不同编译器的结构体布局差异
- 处理不同端序（big-endian / little-endian）
- 进行版本校验和兼容性处理
- 进行完整性校验（CRC）

---

> **源出（溯源）**：zhaoming `memory-safety.md`、chigo-micro `.claude/rules/c-embedded.md`、
> 原 `rules/c-code.md` §4 Struct 布局与序列化。
