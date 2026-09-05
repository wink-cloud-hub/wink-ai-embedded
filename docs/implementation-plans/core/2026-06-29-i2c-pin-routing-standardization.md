# I2C Pin 路由标准化实施计划

| 项 | 内容 |
|---|---|
| **计划状态** | ✅ 已完成（`8a9b3ae feat(pal): declare pal_i2c_pin_map extern for board-level routing` + esp32 target 弱默认 + `samples/avoidance_car`、`samples/devkitc_smoke` `board_config.c` 强覆盖已落地；事后回填：2026-07-03） |
| 创建日期 | 2026-06-29 |
| 关联 ADR | ADR-0002 双 target 同源、ADR-0006 ESP-IDF v6 I2C 兼容 |
| 关联设计规范 | `02-wink-micro-os/02-pal-platform-abstraction.md`（PWM/I2C pin_map 弱默认+强覆盖模式） |

> **For agentic workers:** Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 I2C SDA/SCL 引脚配置从硬编码迁移至 `board_config.c` 模式，与 `pal_pwm_pin_map` 架构一致，支持多板卡灵活配置。

**Architecture:** 复用 PWM 已验证的弱默认 + 强覆盖链接模式：
- `pal_hal.h` extern 声明 `pal_i2c_pin_map`
- `pal_hal_esp32.c` 在 `ESP_PLATFORM` 条件块内提供 `__attribute__((weak))` 默认值
- `samples/*/board_config.c` 可选择性提供强覆盖定义
- **host/wasm 无需 stub**：extern 声明存在但不被引用，链接器不报错

**Tech Stack:** C (ESP-IDF), CMake, MinGW GCC (host CI)

## 全局约束

- **零破坏变更**：无 `board_config.c` 强覆盖时，链接回落至 esp32 target 弱默认（保持现有引脚映射）
- **三 target 同源**：host/wasm/esp32 必须同时编译通过（host/wasm 忽略 pin_map，不链接，且无需 Stub）
- **双版本兼容**：ESP-IDF v5.x / v6.x 双版本 I2C API 必须同时支持
- **编译零警告**：`-Wall -Wextra -Werror`，且确保全局声明在 host/wasm 不被引用时的静态检查完全通过

---

## Task 1: PAL 头文件 extern 声明

**Files:**
- Modify: `wink-micro-os/pal/include/hal/pal_hal.h:31-32` (紧邻 pal_pwm_pin_map 之后)

**Interfaces:**
- Produces: `extern const uint16_t pal_i2c_pin_map[PAL_I2C_PORTS][2];` — [port][0]=SDA, [port][1]=SCL

- [ ] **Step 1: 定义 PAL_I2C_PORTS 常量**

在 `pal_hal.h:14` 之后添加：
```c
#ifndef PAL_I2C_PORTS
#define PAL_I2C_PORTS 2
#endif
```

- [ ] **Step 2: extern 声明 pin_map**

在 `pal_hal.h:31` 之后添加：
```c
/* I2C 物理引脚路由：[port][0] = SDA, [port][1] = SCL */
extern const uint16_t pal_i2c_pin_map[PAL_I2C_PORTS][2];
```

- [ ] **Step 3: Commit**
```bash
git add wink-micro-os/pal/include/hal/pal_hal.h
git commit -m "feat(pal): declare pal_i2c_pin_map extern for board-level routing"
```

---

## Task 2: ESP32 target 弱默认实现与宏重构

**Files:**
- Modify: `wink-micro-os/targets/esp32/pal_hal_esp32.c`

**Interfaces:**
- Consumes: `PAL_I2C_PORTS` from `pal_hal.h`
- Produces: Weak `pal_i2c_pin_map[PAL_I2C_PORTS][2]` = {{21, 22}, {33, 32}} (当前硬编码值)

- [ ] **Step 1: 在平台条件块内添加弱默认 pin_map**

在 `pal_hal_esp32.c:208` 的 `#endif` **之前**（即紧邻 `pal_pwm_pin_map` 定义之后）添加，确保符号只在 ESP32 平台下定义且符合静态分析逻辑：
```c
/* I2C 引脚弱默认：无 board_config.c 强覆盖时使用。
 * I2C0: SDA=21, SCL=22; I2C1: SDA=33, SCL=32 */
__attribute__((weak)) const uint16_t pal_i2c_pin_map[PAL_I2C_PORTS][2] = {
    {21, 22},
    {33, 32}
};
```

- [ ] **Step 2: 重构本地宏 `I2C_PORTS`**

删除 `pal_hal_esp32.c:299` 本地冗余宏定义：
```c
#define I2C_PORTS            2  /* DELETE */
```
将文件中所有使用 `I2C_PORTS` 的静态资源数组声明及边界检查全局替换为 `PAL_I2C_PORTS`，保持数组大小一致且约束统一。

- [ ] **Step 3: 删除硬编码常量并替换为 pin_map 引用**

删除 `pal_hal_esp32.c:457-458`：
```c
static const int i2c_sda_map[I2C_PORTS] = {21, 33};  /* DELETE */
static const int i2c_scl_map[I2C_PORTS] = {22, 32};  /* DELETE */
```
直接在总线初始化中引用全局 `pal_i2c_pin_map`，无需添加冗余的 `(void)pal_i2c_pin_map;` 警告消除代码。

更新 `pal_hal_esp32.c:463-464` (v6 API)：
```c
.sda_io_num = pal_i2c_pin_map[port][0],
.scl_io_num = pal_i2c_pin_map[port][1],
```

更新 `pal_hal_esp32.c:480-481` (v5 API)：
```c
.sda_io_num = pal_i2c_pin_map[port][0],
.scl_io_num = pal_i2c_pin_map[port][1],
```

- [ ] **Step 4: 删除 FIXME 注释**

删除 `pal_hal_esp32.c:17` 和 `pal_hal_esp32.c:455` 的 `FIXME` 注释，替换为完成说明。

- [ ] **Step 5: ESP-IDF 编译验证**
```bash
# 激活 EIM Profile 后执行
cd <esp-idf-project>
idf.py build
```
**Expected:** 0 error, 0 warning

- [ ] **Step 6: Commit**
```bash
git add wink-micro-os/targets/esp32/pal_hal_esp32.c
git commit -m "feat(esp32): weak pal_i2c_pin_map default, refactor I2C_PORTS to PAL_I2C_PORTS"
```

---

## Task 3: 更新 samples board_config.c 强覆盖示例

**Files:**
- Modify: `wink-micro-os/samples/avoidance_car/board_config.c`
- Modify: `wink-micro-os/samples/devkitc_smoke/board_config.c`

**Interfaces:**
- Consumes: `PAL_I2C_PORTS` from `pal_hal.h`
- Produces: Strong definition example for board-level override

- [ ] **Step 1: avoidance_car 添加 I2C pin_map**

在 `board_config.c:11` 之后添加：
```c
/* avoidance_car I2C 路由：I2C0 接 OLED（SDA=21, SCL=22），I2C1 预留 */
const uint16_t pal_i2c_pin_map[PAL_I2C_PORTS][2] = {
    {21, 22},
    {33, 32}
};
```

- [ ] **Step 2: devkitc_smoke 添加相同 pin_map**

与 Step 1 相同，保持 DevKitC 默认硬件映射。

- [ ] **Step 3: 编译验证（双 target）**
```bash
# host unit tests (验证 host 下编译无 linker 缺失错误)
powershell -NoProfile -File python wink-tools/wink.py test

# ESP-IDF firmware build
idf.py build
```
**Expected:** Both pass, no multiple-definition error

- [ ] **Step 4: Commit**
```bash
git add wink-micro-os/samples/avoidance_car/board_config.c
git add wink-micro-os/samples/devkitc_smoke/board_config.c
git commit -m "feat(samples): add strong pal_i2c_pin_map in board_config.c examples"
```

---

## Task 4: 设计规范文档更新与验收

**Files:**
- Modify: `docs/design/02-wink-micro-os/03-directory-architecture.md:131` (board_config.c 说明)

**验收标准 Checklist:**
- [ ] **Step 1: 更新架构文档**

更新 `03-directory-architecture.md` 中 `board_config.c` 的说明，从仅 PWM 扩展为 PWM + I2C：
```markdown
| **`board_config.c`** | 开发板引脚路由 | **开发板物理引脚强覆盖映射（可选）**。<br>为特定硬件板卡提供底层引脚路由的强定义（如覆盖 `pal_pwm_pin_map`、`pal_i2c_pin_map`），解耦通用外设与具体硬件的映射。 | 🛠️ 板卡级固件包/系统工程师提供 |
```

- [ ] **Step 2: Host CI 全量回归**
```bash
powershell -NoProfile -File python wink-tools/wink.py test
```
**Expected:** 16/16 passed

- [ ] **Step 3: ESP-IDF 编译 + 冒烟验证**
```bash
idf.py build
idf.py flash monitor
# 验证 I2C bus scan 正常工作（与修改前行为一致）
```

- [ ] **Step 4: 文档 Commit**
```bash
git add docs/design/02-wink-micro-os/03-directory-architecture.md
git commit -m "docs(arch): update board_config.c description to include I2C pin_map"
```

---

## 风险与回退

| 风险 | 概率 | 影响 | 缓解措施 |
|---|---|---|---|
| `GPIO_NUM_NC (-1)` 隐式强转为 `0xFFFF` 导致引脚配置失效 | 中 | 引脚配置出错或穿崩 | 提示开发人员 I2C 物理引脚必须有效连接，本阶段不支持悬空/置空；未来统一重构引脚类型为 `int16_t` |
| 弱/强符号冲突导致 multiple definition | 低 | 链接失败 | esp32 target 必须使用 `__attribute__((weak))`，board_config 不带 weak |
| CMake target 未正确链接 board_config.c | 低 | 回落至弱默认但无报错 | Task 3 Step 3 显式验证双定义链接 |

---

## 验收通过标准

实施完成后必须满足：

1. ✅ `python wink-tools/wink.py test` 16/16 全通过（零警告）
2. ✅ `idf.py build` 零错误零警告
3. ✅ `board_config.c` 存在强定义时链接优先使用强定义
4. ✅ 无 `board_config.c` 时链接回落至 esp32 target 弱默认（行为不变）
5. ✅ I2C v5/v6 双 API 均正确使用 pin_map
6. ✅ host/wasm target 无链接错误（**无需 stub 代码**）
