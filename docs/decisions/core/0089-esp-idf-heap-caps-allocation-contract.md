# ADR-0089：ESP-IDF 仿真 heap_caps 分配与簿记契约

| 项 | 内容 |
|---|---|
| 状态 | **Accepted**（按加固计划 D1 选择执行） |
| 日期 | 2026-09-28 |
| 触发 | H3 实施前发现模拟器把所有 heap_caps 分配都放入有限簿记表、虚报普通内存水位，并声称所有指针均可直接 libc free；Windows 对齐分配器无法满足此说法 |
| 影响范围 | `wink-micro-os/frameworks/esp_idf/src/core/esp_heap_caps.c`、heap API 行为测试、API 覆盖矩阵和仿真文档 |
| 决策者 | 项目计划已选方案 2；MSVC 专项探针已验证分配器配对及实际堆源码 |
| 关联 ADR | [ADR-0012](0012-contract-honesty-over-silent-degradation.md)（契约诚实）、[ADR-0045](../unisim/0045-simulation-memory-quota-and-fault-policy.md)（仿真配额与故障） |
| 关联计划 | `docs/implementation-plans/esp32/2026-09-28-esp-idf-simulation-hardening-plan.md` §3 H3 / D1 |

## 1. 背景

原实现对普通 `malloc` 尝试最多 16 次以寻找更高对齐地址，再回退到普通 `malloc`；它可能实际没有满足请求的对齐。所有分配均写入固定容量 tracker，tracker 满时仍返回未登记指针；所有释放统一调用 `free`，无法跨 MSVC 的 `_aligned_malloc` 配对释放。普通 malloc 也被虚拟 320 KiB 配额和动态水位误导。

官方 ESP-IDF 的能力堆允许使用标准 `free` 释放其分配。Windows MSVC 的 `_aligned_malloc` 必须由 `_aligned_free` 配对，因此仿真层不能诚实地对所有特殊对齐块承诺 libc `free` 兼容。

## 2. 决策

### 2.1 分配类别

- **普通分配**：不请求 DMA、SPIRAM，也没有高于目标宿主 malloc 基线对齐的显式对齐要求（实现用最大对齐联合体偏移推导该基线）。使用宿主 `malloc/calloc/realloc/free`；`heap_caps_free` 和 libc `free` 均可释放。普通分配不进入有限 tracker，也不扣减虚拟配额。
- **特殊分配**：请求 DMA、SPIRAM，或使用高于目标 `max_align_t` 的显式对齐。使用明确的宿主对齐分配器并记录精确释放器、大小和能力。正式跨平台契约要求通过 `heap_caps_free` 释放；MSVC 的 `_aligned_malloc` 必须配 `_aligned_free`。直接 libc `free` 属于仿真不支持行为。
- DMA 至少 32 字节对齐；显式对齐必须是指针大小以上的 2 的幂。分配器按目标能力保证对齐，不使用随机 malloc 探测和未对齐回退。

### 2.2 统计和配额

- 普通分配不实施仿真配额。DEFAULT/INTERNAL/8BIT 查询返回配置的容量提示，不代表当前空闲量、历史低水位或碎片率；`get_minimum_free_size` 与 `get_largest_free_block` 对普通域同样不声称实测水位。
- 特殊分配使用有限 tracker 和配置配额。分配前先验证溢出、配额和 tracker 槽位，再分配并登记；tracker 满或分配失败时返回 NULL，不泄漏、不增加用量。
- 特殊配额与大小统计仅对经过 `heap_caps_free` 的完整分配/释放配对精确。物理 DMA 可达性、PSRAM 总线和真实碎片行为不由仿真保证。

### 2.3 realloc 与裸指针边界

- `realloc(NULL, n)` 等价于分配；`realloc(p, 0)` 释放并返回 NULL。
- 普通→普通用宿主 `realloc`。特殊→特殊或特殊→普通先分配目标块、复制 `min(old,new)`、成功后释放旧块；失败时旧指针及统计保持有效。
- 普通→特殊返回 NULL 并保留旧指针，因为未登记的普通裸指针没有可移植的旧大小。
- 释放或 reset 后裸指针失效；同地址复用后的旧指针、重复释放和悬空访问属于未定义行为。tracker generation 不能让同地址裸指针变得可区分，不作此保证。

## 3. 验证要求和当前证据

- MinGW Windows 原型：`max_align_t` 为 8 字节；`malloc` 满足基线；`_aligned_malloc/_aligned_free` 成对分配释放成功。
- MSVC 19.51 x64：独立 `_aligned_malloc/_aligned_free` 对齐探针通过；实际 `esp_heap_caps.c` 在 MSVC C11 下直接编译并运行 DMA 32B 对齐、满配额同尺寸 realloc、簿记归零，输出 `MSVC_ACTUAL_HEAP_SOURCE_OK`。此探针隔离了 framework 其余模块，不代表整仓 MSVC 构建通过。
- 同一实际源码探针加 MSVC AddressSanitizer 后，在把 Visual Studio `clang_rt.asan_dynamic-x86_64.dll` 目录加入进程 PATH 后运行通过；覆盖 DMA 对齐分配、满配额 realloc、数据验证和释放，不代表整套 Host 测试的 sanitizer 验证。
- Emscripten Wasm32 原型：`max_align_t` 为 8 字节；`malloc`、`aligned_alloc/free`、`posix_memalign/free` 对齐与释放配对成功。
- 当前宿主安装 Visual Studio 18 / MSVC 19.51；整仓 MSVC Host 构建仍被既有 C11 原子配置、I2C 头文件和告警策略错误阻断，不能据专项探针宣称整仓 MSVC 通过。
- MinGW AddressSanitizer 当前不可用：本机 MinGW 工具链缺少 `libasan`；MSVC ASAN 已通过实际 heap 源码的 focused probe，但整套 Host phase3 sanitizer 还受整仓 MSVC 编译错误阻断。WSL 没有已安装 Linux 发行版，因此原生 POSIX Host 与 Linux sanitizer 仍需 CI/外部 Linux runner 验证。
- 实现不得硬编码普通 malloc 的 16 字节基线；MSVC 不提供该代码所需的 `max_align_t`，实际源码已改用最大对齐联合体偏移推导，并通过直接 MSVC 编译验证。
- 分配器验收包括：普通分配超过 tracker 容量后 libc free 与特殊分配仍成功；特殊块对齐、配额、tracker 满、复位清理、溢出、类别 realloc 和失败回滚。

## 4. 后果

- 仿真不再声称 DMA/SPIRAM 分配完全复刻官方硬件语义，也不把普通 host malloc 描述成有限片上 SRAM。
- 使用特殊对齐/DMA/SPIRAM 的用户代码必须以 `heap_caps_free` 配对释放；这是 MSVC 仿真的显式降级，并由 API 矩阵记录。
- 原“所有 heap_caps 均 libc free 兼容”和“普通堆真实水位 SSOT”测试与文档必须迁移到此契约。
