# Stage5：中断向量表与 XDATA/XSFR 表驱动化

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S5-IRQBUS` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
| **优先级** | 🟡 P1 |
| **关联 CPL** | CPL-06（向量表）、CPL-08（XDATA/XSFR） |
| **前置依赖** | stage0（schema）、stage4（芯片包可挂载） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ 通用 ISR 仅标准 0~5 向量仲裁，其余从 `irq_vector_table + irq_count` 装载；AT89 模式下 CMS8S 中断物理绝缘。
- ✅ `mcs51_xdata.cpp` 去 `mcs51_xsfr_allowlist.h` 强包含，按 `xsfr_base/size` + 芯片校验分发；经典无窗口。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `src/mcs51_isr.cpp` | ✏️ | 标准仲裁 + 描述符装载 |
| `src/mcs51_xdata.cpp` | ✏️ | 窗口参数化，删 allowlist 包含 |
| `chips/cms8s78xx/` | ✏️ | 向量表行 + XSFR 校验归位 |

## 3. 任务拆分

### Task S5-1：向量表驱动 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：通用 ISR 删 `s_default_irq_map` 中 ADC/PWM/I2C/SPI/UART1 硬编码，改读描述符。
- [ ] **Step 2**：AT89 模式断言扩展向量不可达 + XSFR 窗口关闭的绝缘单测。

### Task S5-2：XSFR 参数化 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：`KIND_XSFR` 改为按 `xsfr_size!=0` 判定；allowlist 头下沉芯片包，通用零包含。
- [ ] **Step 2**：未建模 XSFR tripwire 归芯片模型所有，通用只做窗口分发。

## 4. 验收

- L1：切换 family 后对方中断/XSFR 不可达（绝缘单测）。
- L4：通用 `include/`、`src/mcs51_xdata.cpp` 无厂商白名单引用。

## 5. 风险与回滚

- R：向量号漂移致 ISR 错配 → 缓解：向量表 `static_assert` + dispatch 计数单测；回滚 `git revert <S5-commit>`。
