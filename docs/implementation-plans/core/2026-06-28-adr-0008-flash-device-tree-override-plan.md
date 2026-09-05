# 实施计划：ADR-0008 Flash(NVS) 动态设备树配置逃生通道 — 核心(host 可测)

## 1. 元数据表

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260628-ADR0008-FLASH-DT-OVERRIDE` |
| **创建日期** | 2026-06-28 |
| **目标平台/SoC** | `host`（主交付）/ `wasm`（降级 stub）/ `ESP32`（NVS 实现） |
| **工具链/SDK版本** | GCC 16.1.0 (MinGW) / cmake 4.3.2 / Ninja 1.13.2（host）；ESP-IDF v6.0.1（esp32，可选验证） |
| **计划状态** | ✅ 已完成 |
| **优先级** | 🟡 P1（重要，解锁真机快速调试反馈环） |
| **计划版本** | v1.0 |
| **关联技术设计** | 无，已并入本计划 |
| **关联设计规范** | `02-wink-micro-os/`（PAL/设备树规范，Accepted 后回写） |
| **关联评审记录** | 无 |
| **关联 ADR** | [ADR-0008](../../decisions/core/0008-dynamic-device-tree-config-flash.md)（Proposed → 本计划收尾转 Accepted） |
| **目标里程碑** | ADR-0008 Wave B 核心 |
| **前置依赖计划** | 无（baseline ctest 17/17 绿） |
| **计划负责人** | 主架构师 |
| **所需子代理技能** | `embedded-best-practice` |

---

## 2. 背景与目标

### 2.1 问题陈述

ADR-0008 旨在消除真机调试痛点：微调一个 GPIO 引脚或舵机脉宽就要触发云端重编译 + 全量烧录（1~2 分钟反馈环）。方案 C = **静态 POD 实例 + Flash 配置动态覆写**：固件静态分配器件，启动时尝试从 Flash 读配置覆写 POD 的 pin/参数字段，失败则静默降级到编译期默认。

**核对真实代码后发现 ADR 是「绿地方案」**——它假设的 5 项基础设施当前一个都不存在：无 `device_tree_init_all()`、无 WebSerial 协议、无 `device_id`、无 Flash 存储、无 Wasm 虚拟 FS。本计划只做**可测核心**。

### 2.2 技术/业务目标

- ✅ 一套 target 无关、host 上完全单测的「覆写机制」：`device_id` 注册表 + blob 解析器(magic/version/CRC, 损坏静默降级) + per-DAL `apply_override` + PAL 存储抽象(NVS/host内存/wasm no-op)
- ✅ 在 `avoidance_car` 样本端到端打通（servo + ultrasonic，覆盖 ADR 全部示例）
- ✅ 零破坏性变更：不改 DAL 公开 init API 签名、不改 POD struct 布局；无 override 时行为等同默认
- ✅ 三 target（host/wasm/esp32）同源编译，wasm/无存储 target 走运行期降级
- ✅ CRC32 跨边界契约钉死（host 实现 = 前端对接权威参考）

### 2.3 成功指标

| 指标 | 通过标准 | 验证方法 |
|------|----------|----------|
| host 单元测试 | 100% 通过（含新增 `test_dev_config`） | `cmake -B build-test -DTARGET_PLATFORM=host` → `ctest` |
| 既有测试回归 | 0 回归（17/17 → 18+/… 全绿） | 同上，全量 ctest |
| PAL 契约门禁 | `test_pal_contract` 绿 | ctest |
| ESP32 NVS 链接 | 0 error（warning 可选验证） | `idf.py build`（WINK_APP=avoidance_car）— 本分支非强制 |
| GCC+MSVC 双链 | 双链 0 error 0 warning | host 构建 |

---

## 3. 变更范围与影响分析

### 3.1 文件变更清单

| 文件路径 | 变更类型 | 说明 |
|----------|----------|------|
| `wink-micro-os/pal/include/wink_dev_config.h` | 🆕 新增 | blob 格式常量 + override 注册表类型 + `wink_dev_config_apply()`；顶部钉死 CRC32 契约 |
| `wink-micro-os/pal/src/wink_dev_config.c` | 🆕 新增 | CRC32(无表 bitwise) + apply（共享核心，target 无关） |
| `wink-micro-os/pal/include/pal_storage.h` | 🆕 新增 | 存储抽象 `read/write/erase/reset` |
| `wink-micro-os/targets/host/pal_storage_host.c` | 🆕 新增 | 内存单槽实现（测试用） |
| `wink-micro-os/targets/esp32/pal_storage_esp32.c` | 🆕 新增 | NVS 实现（namespace `wink`, key `dtcfg`） |
| `wink-micro-os/targets/wasm/pal_storage_wasm.c` | 🆕 新增 | no-op stub：read 返 UNSUPPORTED（运行期降级） |
| `wink-micro-os/test/test_dev_config.c` | 🆕 新增 | Unity 单测 + E2E |
| `wink-micro-os/dal/include/dal_servo.h` + `dal/src/dal_servo.c` | ✏️ 修改 | 新增 `dal_servo_apply_override(void*, params, len)` |
| `wink-micro-os/dal/include/dal_ultrasonic.h` + `dal/src/dal_ultrasonic.c` | ✏️ 修改 | 新增 `dal_ultrasonic_apply_override(void*, params, len)` |
| `wink-micro-os/samples/avoidance_car/{device_tree.h,device_tree.c,app_callbacks.c}` | ✏️ 修改 | 注册表 + `device_tree_apply_flash_config()` + app_init 从结构体读配置 |
| `wink-micro-os/targets/{host,esp32}/CMakeLists.txt` | ✏️ 修改 | 链接新源（照 `pal_pwm_router.c` 先例） |
| `wink-micro-os/CMakeLists.txt` | ✏️ 修改 | wasm 分支 SRCS 加新源 |
| `wink-micro-os/test/CMakeLists.txt` | ✏️ 修改 | 新增 `add_wink_host_test(test_dev_config ...)` |
| `esp32_firmware/main/app_main.c` | ✏️ 修改 | `app_main()` 顶部加 `nvs_flash_init()`（erase-on-corrupt） |
| `docs/decisions/core/0008-dynamic-device-tree-config-flash.md` | ✏️ 修改 | Proposed → Accepted + 实现澄清段 |
| `docs/design/02-wink-micro-os/...` | ✏️ 修改 | Layer ① 回写（ADR Accepted 后） |

### 3.2 接口影响分析

| 接口层 | 是否破坏性 | 影响范围 | 备注 |
|--------|-----------|----------|------|
| PAL 公开 API | ❌ 否 | 新增 `pal_storage_*`（纯新增） | 不动既有 PAL 签名 |
| DAL 层 | ❌ 否 | 新增 `*_apply_override`（纯新增） | 不改 init 签名、不改 POD 布局 |
| 应用层 | ❌ 否 | avoidance_car app_init 改从结构体读配置 | 无 override 时行为等同默认 |
| 构建系统 | ❌ 否 | 三 target CMakeLists 加源 | 照 `pal_pwm_router.c` 先例 |
| 文档 | ❌ 否 | ADR + Layer ① | — |

### 3.3 架构红线

> 🚨 违反即拒绝合入：
> 1. **禁 packed/`#pragma pack` 指针强转**：blob 用 offset+memcpy 逐字段反序列化（规避非对齐访问/别名 UB）。runtime POD 绝不 memcpy 到 wire。
> 2. **绝不 brick**：blob 损坏/版本不符/CRC 错/未命中 → 静默降级到编译期默认，绝不 Panic。
> 3. **三 target 同源**：共享核心 `wink_dev_config.c` 一份代码同时过 host/wasm/esp32；CRC 不用 ESP-IDF 私有实现。
> 4. **零动态分配**：无 malloc；blob 解析走栈缓冲，大小由 `WINK_DEV_CONFIG_MAX_BYTES` 常量约束。

### 3.4 系统资源与并发约束

| 维度 | 变化 | 风险 | 缓解 |
|------|------|------|------|
| ROM/Flash | +解析器+CRC32(无表，<1KB) + 各 target storage 实现 | 极小 | wasm/host/esp32 均富余 |
| RAM(静态) | sample 侧栈缓冲 `MAX_BYTES`(256B) + host 单槽(256B) | 极小 | 静态/栈分配，无运行期扩展 |
| 栈深度 | +1 层 apply 调用链 | 极小 | 解析器浅函数 |
| 堆 | 0 | — | 运行期严禁 malloc |
| 并发 | 启动单线程 app_init 内调用 | 无 | 写→复位→读 串行化，CRC 兜底 torn write |

---

## 4. 关键设计约束（读证所得）

1. **配置当前不从结构体流向 init**（最关键）：`app_callbacks.c:26-29` `servo_cfg` 是局部变量覆盖结构体字段；`:32` `dal_ultrasonic_init(&front_radar, 4, 5)` 用字面量。⇒ 覆写写进结构体字段后，**必须让 app_init 改为从结构体读配置**才生效。
2. **servo config↔dev 字段重复（已知 wart）**：`dal_servo_init` 吃独立 config，app_init 须从 dev 字段重建 config 再喂 init（多一跳，避免改 DAL API）。
3. **hook 点**：`device_tree_init_all()` 不存在，实插在每个 sample `app_init` 最顶部。
4. **复用资产**：`WINK_ERR_CHECKSUM(-8)`/`WINK_ERR_CONFIG_CORRUPT_DEGRADED(-50)`（闲置）；`servo_safe_off_thunk(void* ctx)` thunk 范式；`add_wink_host_test`；ESP-IDF 默认分区表已含 16KB `nvs`，**无需改 partitions.csv**；仓内无现成 CRC（自写 bitwise 正确）。

---

## 5. 跨边界契约与安全边界

> ADR §4.3「优雅降级、绝不 brick」的落地依据；前端 Codegen 将来对齐的契约。**host 单测实现 = 权威参考。**

### CRC32：完整性闸门（只防损坏，不防语义）

- **算法钉死**：**CRC-32/ISO-HDLC**（zlib/PNG/zip 同款），多项式反射值 `0xEDB88320`、init `0xFFFFFFFF`、final XOR `0xFFFFFFFF`、输入/输出 reflected。
- **覆盖范围**：`header(8B) + items(N×20B)`，不含末尾 4 字节 CRC 自身。
- **实现**：共享核心 `wink_dev_config.c` 自写**无表 bitwise**；不用 `esp_rom_crc32`（防双 target 漂移）。
- **写进头注释**：`wink_dev_config.h` 顶部把 4 参数写死成注释 = 前端对接唯一契约。
- **能挡**：torn write、Flash 比特翻转/老化、首次启动 `0xFF…` 垃圾。
- **挡不了**：语义错误但字节完好（前端写错格式）→ 由 §4.2 前端 linter + 版本门控/轻校验兜底。

### device_id ↔ params 布局：版本门控

- `device_id` = codegen 稳定 uint32；注册表绑定 `(id, 类型化 dev*, 类型化 apply_fn)` 三元组，固件侧类型安全。
- 兜底：① apply 轻校验 + `dal_*_init` 再校验（纵深）；② params 布局变更必 bump `version`，旧 version 一律降级。

### 其它硬约束

- **小端假定**：所有 target 均 LE，f32 memcpy 安全；头注明 LE 假定。
- **`HAS_FLASH_CONFIG_ESCAPE`**：核心阶段统一运行期降级，**不定义/不引用**；留作未来低资源 target 编译期裁剪开关。

---

## 6. Blob 格式

```
启动 app_init 顶部:
  device_tree_apply_flash_config()
    ├─ pal_storage_read("dtcfg", buf, cap, &len)   // NVS / host内存 / wasm=UNSUPPORTED
    │     非 OK(EMPTY/UNSUPPORTED/IO) → 返回，静默用编译期默认
    └─ wink_dev_config_apply(buf, len, registry[], n)
          ├─ 校验 长度不变式 / magic / version / count
          ├─ CRC32(header+items) ≠ 末尾 4B → WINK_ERR_CHECKSUM（降级）
          └─ 逐 item: device_id 查注册表 → 命中 apply_override(dev, params,16)
                         未命中/校验失败 → 跳过该项(降级), 不中断
```

**Blob 布局**（小端，offset+memcpy，**不用 packed 指针强转**）：
```
[magic:u32=0x57494E4B "WINK"][version:u16=1][count:u16=N]   // header 8B
[item]×N :  [device_id:u32][params:16B]                      // 每 item 20B
[crc32:u32]                                                  // CRC-32/ISO-HDLC, 覆盖 header+items
```
- **长度不变式**：`len == 8 + count*20 + 4`，不符降级。
- **version 策略**：`version != 1 → CONFIG_CORRUPT_DEGRADED` 降级（不做前向迁移）。
- **count==0**：合法 no-op 成功。
- **MAX_BYTES**：`WINK_DEV_CONFIG_MAX_BYTES`(256) 头常量；超限降级。

**params 布局**（各 DAL 自行反序列化）：
- servo: `pwm_channel:u8 @0`, `min_pulse_ms:f32 @1`, `max_pulse_ms:f32 @5`（≥9B）
- ultrasonic: `trig_pin:u16 @0`, `echo_pin:u16 @2`（≥4B）

---

## 7. 任务拆分与进度

### Task 1：落 Layer ③ 计划文档 ✅

### Task 2：TDD 解析器 `wink_dev_config` ✅
- [x] 写 `test_dev_config.c`：解析器用例（合法/magic错/CRC错/长度不符/buffer过小/version≠1/count==0）+ CRC golden vector
- [x] 实现 `wink_dev_config.h/.c`（CRC32 无表 bitwise + apply）
- [x] 红→绿

### Task 3：TDD PAL 存储抽象 `pal_storage` ✅
- [x] 写 `pal_storage.h` + `pal_storage_host.c` + host 读写/erase/reset 用例 → 绿

### Task 4：per-DAL `apply_override` ✅
- [x] 扩 `test_dal_servo.c`/`test_dal_ultrasonic.c` 用例 → 实现 `dal_*_apply_override` → 绿

### Task 5：avoidance_car 样本端到端打通 ✅
- [x] 改 `device_tree.{h,c}` + `app_callbacks.c`；E2E 用例（内存存储写入→apply→断言字段）→ 绿；回归 `app_avoidance_car_e2e`

### Task 6：三 target 构建接线 ✅
- [x] host/wasm/esp32 三 CMakeLists + `test/CMakeLists.txt`；wasm stub；esp32 NVS + `app_main` init（**不加** `HAS_FLASH_CONFIG_ESCAPE`）

### Task 7：全量回归 ctest ✅
- [x] ctest 全绿（含 `test_pal_contract`）；GCC+MSVC 双链

### Task 8：收尾文档 ADR Accepted + 回写 ✅
- [x] ADR §3.1 init API 修正、§3.2 packed→memcpy + CRC 契约、§4.1 wasm 分阶段、device_id 版本门控、`HAS_FLASH_CONFIG_ESCAPE` 语义；Layer ① 回写

---

## 8. 测试策略（L0–L1）

`test_dev_config.c`（Unity，`add_wink_host_test`）覆盖：
1. **解析器**：合法→逐 item 命中 apply 字段被改写；magic错→`CONFIG_CORRUPT_DEGRADED`；CRC错→`CHECKSUM`；长度不符→降级；buffer过小→`INVALID_ARG`；version≠1→降级；count==0→no-op 成功
2. **per-DAL apply**：servo 合法改 pwm/min/max；min≥max→不写返 INVALID_ARG；ultrasonic 合法改 trig/echo；trig==echo→不写返 INVALID_ARG
3. **E2E（host 内存存储）**：`pal_storage_write` 含覆写 blob → `device_tree_apply_flash_config()` → 断言字段已变；`pal_storage_erase` → apply 返非 OK 且字段保持默认
4. **未命中 device_id**：blob 含注册表无的 id → 该项跳过、其它项仍 apply、不崩溃
5. **CRC 权威参考**：golden vector（借 `zlib.crc32` 生成）固化实现

**L0 编译门禁**：host `ctest` 全绿；esp32 `idf.py build` NVS 链接通过（本分支可选）；GCC+MSVC 双链 0 warning。

---

## 9. 回滚与降级方案

- **方案 1（运行期降级，内置）**：擦除 Flash 的 `dtcfg`（`pal_storage_erase` 或 WebSerial 0x15 后置）→ 下次启动 apply 返 EMPTY → 回编译期默认。无需重编译。
- **方案 2（Git 回退）**：`git revert` 本计划全部 commit → 回 baseline（17/17 绿）。
- **方案 3（编译期裁剪，未来）**：低资源 target 定义 `HAS_FLASH_CONFIG_ESCAPE=0` 编译期剥离解析器+storage（本分支不引入）。

---

## 10. 参考资料

- [ADR-0008](../../decisions/core/0008-dynamic-device-tree-config-flash.md)
- [ADR-0001 错误码符号约定](../../decisions/core/0001-error-code-sign-convention.md)
- [ADR-0004 静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)
- [ADR-0002 双 target 同源编译](../../decisions/unisim/0002-dual-target-compilation.md)

---

### 问题与变更日志

| 日期 | 问题描述 | 解决方案 | 影响范围 |
|------|----------|----------|----------|
| 2026-06-28 | 计划中 host 构建命令缺 `-DTARGET_PLATFORM=host`（否则默认 wasm 跳过 host 测试） | 执行时用 `-DTARGET_PLATFORM=host` + `-G Ninja` + `build-test/` | 构建命令 |
| 2026-06-28 | gcc 16 的 `warn_unused_result` 不能用 `(void)func()` 抑制 | 被忽略的状态码改 `wink_status_t s=func(); (void)s;` | 调用点编码 |

