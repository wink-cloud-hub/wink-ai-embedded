# Stage2：`Mcu51Context` 数据结构纯净化

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S2-CONTEXT` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
| **优先级** | 🔴 P0 |
| **关联 CPL** | CPL-11（soc_priv）、CPL-12（引脚掩码）、CPL-18（残留结构/isr/xdata）、CPL-19（复位播种） |
| **前置依赖** | stage0（caps_cache）、stage1（rail 接口已稳） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ `adc0832/sysProt/buzzer/cms8sAdc` 移出通用体，经 `soc_priv` 按实例挂载；GPIO Trait 钩子结构以 per-context 指针预留（定义在本阶段，挂载在 stage4）。
- ✅ T3/T4/捕获比较、端口采样移入 `cms8s priv`；外部总线跟踪改名 `extbus` 留 core（通用概念，判据 `xram_size==0`，搬走即造成 gpio 调用点链接反向依赖，见总纲 §3.2）；`isr_table`/XDATA 按描述符裁剪或文档化超配。
- ✅ 通用复位仅 Intel 标准种子；XSFR/ADCLDO/CKCON/Fosc 下放芯片包；厂商命名宏下沉。
- ✅ timer `{8,8,6,4}` 与 extint `{8,8,8,8}` 双向改读 `port_pin_masks`（前者修复 classic，后者收紧 CMS8S）；`sizeof` 不净增；双 context 不串扰。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `include/mcs51_context.h` | ✏️ | 删厂商/板级状态与厂商宏，加 `instance_index` + 留 `soc_priv(void*)` + 标准状态；禁 include 任何厂商 priv 头 |
| `src/mcs51_context.cpp` | ✏️ | 标准种子 only + caps 快照 + `instance_index` 分配/重绑；XSFR/rail 播种删除 |
| `chips/cms8s78xx/include/cms8s_priv.h` | 🆕 | priv 结构（sys/buzzer/adc/T3T4/端口采样） |
| `chips/at89c52/include/at89_priv.h` | 🆕 | 预留位（当前无 classic 私有状态；外部总线跟踪已确认为通用概念，留 core 改名 extbus） |
| `include/mcs51_trap.h`（+ `mcs51_context.h` 钩子字段） | ✏️ | 新增 GPIO Trait 钩子结构定义（may_drive/is_analog/pullup，per-context 指针，纯声明零行为；挂载在 stage4。file-static 全局指针否决：双 context 分属不同家族时必串扰） |
| `include/wink_mcs51_classic_bus.h` → `wink_mcs51_ext_bus.h` | ✏️/改名 | "classic" 系家族命名，通用概念改通用名；`Mcs51ClassicBusState`/`mcs51_classic_bus_*` 同改；调用点（gpio/xdata）同步改名，逻辑不动 |
| `src/mcs51_timer.cpp` + `src/mcs51_extint.cpp` | ✏️ | 引脚掩码双向改读描述符（timer 修 classic，extint 收紧 CMS8S） |

架构红线：**soc_priv 锁定方案 A（按实例 BSS 池），否决 union 方案**——union 要求通用头 include 厂商 priv 头以确定大小，直接击穿 CPL-11/14；禁全局单例；禁堆。

## 3. 任务拆分

### Task S2-1：soc_priv 挂载（方案 A 锁定） `[状态: ⏳ 待开始]`

- [ ] **Step 0**：通用头加 `uint8_t instance_index`（纯通用字段）+ `MCS51_MAX_INSTANCES` 上限常量；新增 `mcs51_context_init(ctx, idx)`（多实例显式分配，`idx` 越界编译期/运行期断言）；`mcs51_context_reset(ctx)` 签名不变（默认 `idx==0` 即今日单 context 行为，现网 ~40 处调用零改动），reset 内断言 `idx < MAX` 越界熔断；`reset`/`set_family` 按 index 重绑 `soc_priv` + `memset` 对应池槽，family 切换时先解绑旧槽再绑定新槽，classic 显式绑定 `nullptr`（禁残留悬空）。冲突域说明：池按芯片分数组，同家族同 index 才冲突；跨进程（各测试二进制独立进程）天然隔离，危险仅在同进程双 context——Step 3 的串扰单测必须经 `init` 分配不同 idx，否则测的是假阴性。**memset 保序铁律**：`reset` 必须在 `memset` 前把 `instance_index` 存局部变量、`memset` 后立即恢复（沿用现网 `saved_isrs` 惯用法），再绑定 `soc_priv`；绑定与池槽 `memset` 必须赶在任何 `g_mcs51_peripherals[i].init/reset` 调用之前（外设 init 立即解引用 `soc_priv`，否则野指针）。
- [ ] **Step 1**：建 `cms8s_priv.h` / `at89_priv.h`；芯片源内定义 `static Priv s_priv_pool[MCS51_MAX_INSTANCES]`，按 `ctx->instance_index` 分配，`ctx->soc_priv = &pool[idx]`。
- [ ] **Step 2**：通用体删 4 状态 + `adc_vref/vrail`（rail 默认改由芯片 reset 注入，通用 `mcs51_adc_reset` 仅清注入表）。
- [ ] **Step 3**：双 context 串扰单测（互写 XSFR/ADC 状态不互相污染；`instance_index` 越界钳位单测；异家族双 context 各自 hooks 隔离）。
- [ ] **Step 4**：定义 GPIO Trait 钩子结构（`mcs51_trap.h` 扩展注册 API + ctx 内 `gpio_hooks` 字段，纯声明零行为）；`extbus` 改名（头/状态/函数 + gpio/xdata 两处调用点同步改名，逻辑零改动）。

### Task S2-2：残留结构与复位播种 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：T3/T4/比较/端口采样移入 cms8s priv；`classicBus` 就地改名 `extbus` 留 core（判据 `xram_size==0` 为通用概念；`mcs51_xdata.cpp:273-303` 逻辑不动，`gpio.cpp:244,277` 调用点同步改名——搬出 core 即链接反向依赖，否决）。
- [ ] **Step 2**：`xdata_shadow` 保持 64KB 不动（MOVX 地址空间镜像：42 处访问直引 16 位地址，按 xram 裁剪需全站地址换算，否决；书面超配理由即本句）。`isr_table[28]` 同理保留统一下发平面（classic 空槽位既无注册也无派发，88~176B 不值得在中断相邻路径加分支）。预算优化对象是状态字段（见 §4 表），不是镜像容器。
- [ ] **Step 3**：通用 reset 删 `PS_*=0x7F` 与 `3000/3000` 播种；`MCS51_XRAM_SIZE_CMS8S78XX` 与 `MCS51_XRAM_WINDOW_BASE` 一并下沉（后者此前遗漏）。
- [ ] **Step 4**：timer 与 extint 引脚合法性双向改读 `port_pin_masks`（timer 修 classic P3.4/P3.5 可用；extint 收紧 CMS8S P2.6-7/P3.4-7 非法引脚 + 收紧侧单测）。

### Task S2-0：`sizeof` 基线测量与分家族预算 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：基线测量并记录（迁移前唯一一次）：`static_assert` + 单测打印 `sizeof(Mcu51Context)` 及主要成员偏移（`timer/extint/uart/isr_table/xdata_shadow`），写入本计划 §4 预算表（当前约 68KB，实测为准）。
- [ ] **Step 2**：冻结预算：classic 实例与 CMS8S 实例拆分后各自 `sizeof` 不得超过基线；`xdata_shadow`/`isr_table` 若保留超配必须在 §4 写明理由 + 上限。

## 4. 预算表（§4，实测填数）

| 行 | 说明 | `sizeof` | 备注 |
|----|------|----------|------|
| 基线（master，stage0 前） | 迁移前唯一实测 | 待填 | 含约 64KB `xdata_shadow` + 28 项 `isr_table` |
| stage0 后（`caps_cache` 入账） | +数个字节（含对齐，以实测为准），相对 68KB 可忽略 | 待填 | 总纲 §8 已批预算内增量 |
| stage1 后（rail 64 槽入账） | +96B（32 槽 ×（2B injected + 1B flag）） | 待填 | 双空间分区必需容量 |
| stage5 后（irq map 入 ctx 入账） | +104B（13 项 × 8B profile） | 待填 | 绝缘必需；诊断计数器留 file-static 不计入 |
| cms8s priv 池/实例 | 待测（sys+buzzer+adc+T3T4/采样，stage2 落数） | 待填 | BSS 池按实例，"context 外"内存，另行列表不与上表混算 |
| 注册表上限 | 8（3 core + 3 cms8s + 2 余量，stage4 `static_assert` 锁死，namespace 为"项"非字节） | 待填 | 超限编译期失败，逼新外设走 chips 拆分而非 core 堆料 |
| 拆分后 classic | ≤ 基线 | 待填 | extbus 状态（~8B）留 core，已计入 |
| 拆分后 CMS8S | ≤ 基线 | 待填 | soc_priv 池外计（BSS 池按实例，另行列表） |

## 4. 验收

- L1：通用头零厂商结构/宏；§4 预算表填实测数，拆分后两行均 ≤ 基线。
- L2：经典复位后 XDATA 无 XSFR 残留；经典 P3.4/P3.5 计数可用。
- L4：`grep -Ei 'cms8s|adc0832|0xF0' include/mcs51_context.h src/mcs51_context.cpp` 零命中；`grep -Ei '#include.*(cms8s|at89)_priv' include/mcs51_context.h` 零命中（union 穿透回归哨兵）。

## 5. 风险与回滚

- R-02（串扰/超限）：BSS 池 + 预算断言缓解；回滚 `git revert <S2-commit>`，priv 头独立提交可单独回退。

## 6. 阶段自审自我检验清单（Self-Audit Checkpoint）
- [ ] **目录落位**：`chips/cms8s78xx/include/cms8s_priv.h`、`chips/at89c52/include/at89_priv.h`（预留位）已严格按目录树落位；无 `at89_bus.cpp`（extbus 留 core，见 Step 1 否决理由）。
- [ ] **通用纯净度**：`include/mcs51_context.h` 仅包含标准字段、`void* soc_priv` 与 per-context hooks，无厂商私有头包含；`ext_*` 无 `classic` 命名残留。
- [ ] **预算约束**：§4 预算表已填实测数（基线/stage0/拆分后 classic/拆分后 CMS8S 四行），拆分后两行均 ≤ 基线。
- [ ] **双轨状态**：双 context 串扰单测（含异家族 hooks 隔离）全绿，host/wasm 双 target 编译零 warning/error。
