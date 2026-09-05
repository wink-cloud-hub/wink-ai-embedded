# 4.8 仿真一致性场景清单 (Simulation Consistency Checklist)

| 项 | 内容 |
|----|------|
| 创建日期 | 2026-07-31 |
| 文档层级 | ① 设计规范（`04-wasm-simulation/`） |
| 状态 | **Active** |
| 关联 | [05-simulation-consistency-and-fidelity-spec.md](./05-simulation-consistency-and-fidelity-spec.md)、[ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)、[ADR-0009](../../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md)、[ADR-0013](../../../decisions/unisim/0013-sim-cooperative-scheduler.md)、[ADR-0014](../../../decisions/unisim/0014-sim-single-virtual-core.md)、[ADR-0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md)、[ADR-0042](../../../decisions/unisim/0042-sim-execution-modes.md)、[ADR-0045](../../../decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md)、[07-scheduler-model.md](./07-scheduler-model.md) |

> **定位**：本清单是 [05 仿真一致性与高保真度架构规范](./05-simulation-consistency-and-fidelity-spec.md) 的**场景化对照表（索引）**——按大类/子场景回答「现在能不能验 / 缺口是什么」。
>
> - **读场景能不能验** → 读本清单。
> - **读问题定义、保障方案、验收预言、边界** → 读 [05](./05-simulation-consistency-and-fidelity-spec.md)（**原理与子场景契约 SSOT**）。
> - **产品边界** → [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)。
>
> 「计划解法」只保留 **一行摘要 + 链接**；禁止复述 05 子场景正文。

---

## 0. 阅读约定与 SSOT 分工

### 0.1 支持程度

| 标记 | 含义 |
|------|------|
| ✅ **已支持** | 可验证该场景核心正确性；已知限制见「残余缺口」 |
| 🟡 **部分支持** | 覆盖子集或仅近似；真机仍可能逃逸 |
| ❌ **不支持** | 当前基本验不到；依赖后续 Phase 或真机 |
| 🚫 **刻意不保** | 产品边界外；只做近似或不做 |
| — **N/A** | 当前产品未暴露该 API/路径 |

### 0.2 解法类型

| 类型 | 含义 |
|------|------|
| **A** | 约束写法（lint / 编译期 / JSON 门禁） |
| **B** | 引擎建模（虚拟时钟、延迟、同源、混沌…） |
| **C** | 观测门禁（Sanitizer、软 WDT、影子内存、溢出 Fault） |
| **真机** | HIL / 实机兜底 |

### 0.3 文档指针

* 子场景五字段模板、C1～C25 全文 → [05 §0.1 / §2](./05-simulation-consistency-and-fidelity-spec.md#01-场景文档模板每个子场景必须写齐)
* 原理（虚拟时钟 / Co-Sim / 零 Yield）→ [05 §1](./05-simulation-consistency-and-fidelity-spec.md#1-仿真一致性底层原理概述)
* Phase 里程碑 → [05 §3.2](./05-simulation-consistency-and-fidelity-spec.md#32-演进阶段里程碑)

---

## 1. 大类总览矩阵（C1～C25）

| ID | 一致性大类 | 现状 | 主解法 | Phase / 口径 | 优先级 |
|----|------------|------|--------|--------------|--------|
| C1 | 业务因果 / 状态机 | ✅ | B (+A) | 基线 | 高 |
| C2 | 虚拟微秒逻辑时序 | ✅ | B | Phase 1 | 高 |
| C3 | 共享状态竞态 | ❌ | B+C | Phase 4 | **最高** |
| C4 | 临界区与中断抢占 | 🟡 | B+C | Phase 4 | **最高** |
| C5 | 阻塞 / 饿死 / WDT | 🟡 | A+B+C | Phase 4 前置 | 高 |
| C6 | 栈 / 堆 / 内存安全 | ✅ | A+C | Phase 2 | 高 |
| C7 | 总线协议 / CRC | 🟡 | B+A | Phase 3 | 中高 |
| C8 | DMA / 异步传输窗口 | ❌ | B | Phase 3 | 中（驱动高） |
| C9 | 多核 SMP | ❌ | B 近似 / 真机 | ADR-0014 | 高 |
| C10 | 快环 ISR / FOC | 🟡 | B 近似 / HIL | ADR-0047 | 中高 |
| C11 | 电气 / 模拟 | 🚫 | B 查表近似 | ADR-0003 | 视产品 |
| C12 | CPU / ABI 指令级 | ❌ | C 双轨 | Phase 5 | 低（难查） |
| C13 | 生命周期 / 复位 | 🟡 | B+C | Phase 1 补强 | **最高** |
| C14 | 快进 / Co-Sim 步进 | 🟡 | B+C | Phase 1+ | **最高** |
| C15 | Host↔Wasm 边界 | 🟡 | A+C | 基线 | **最高** |
| C16 | OS 同步原语语义 | 🟡 | B+A | Phase 4 前置 | **最高** |
| C17 | 外设资源互斥 / 时基 | 🟡 | A+C | 持续 | 高 |
| C18 | 总线故障态机 | ❌ | B | Phase 3 扩展 | 中高 |
| C19 | DMA / 缓冲生命周期 | ❌ | B+C | Phase 3 扩展 | 中（驱动高） |
| C20 | 回调重入 / 下半部 | 🟡 | A+C | Phase 4 | 高 |
| C21 | 时间与计数回绕 | 🟡 | A+C | 持续 | 高 |
| C22 | 电源 / 低功耗 / 时钟域 | 🚫 | 真机 | 非目标为主 | 中 |
| C23 | 持久化 / NVS | 🟡 | B | 按需 | 中 |
| C24 | 缓存 / DMA RAM | 🚫 | 真机 / C12 | 非目标为主 | 中 |
| C25 | 浮点 / 数值 UB | 🟡 | C | Phase 2 | 中 |

---

## 2. 关键子场景速查（现状 · 缺口 · 链到 05）

> 完整子场景列表与方案正文在 05；此处只列**高逃逸/高优先级**子项，避免 08 膨胀成第二份规范。未列出的子项默认「跟大类现状走」，细节以 05 为准。

### C1 — 业务因果
| 子项 | 现状 | 残余缺口（摘要） | 链到 05 |
|------|------|------------------|---------|
| C1.1 同源状态迁移 | ✅ | — | [C1.1](./05-simulation-consistency-and-fidelity-spec.md#c11-同源-appbal-状态迁移) |
| C1.2 Bypass 收窄 | 🟡 | 残留整层 `#ifdef` | [C1.2](./05-simulation-consistency-and-fidelity-spec.md#c12-dal-bypass--ifdef-simulation-收窄) |
| C1.3 故障/超时路径 | ✅ | 覆盖面随器件扩展 | [C1.3](./05-simulation-consistency-and-fidelity-spec.md#c13-故障--超时--断线异常路径) |
| C1.4 重试风暴 | 🟡 | 缺统一命令计数预言 | [C1.4](./05-simulation-consistency-and-fidelity-spec.md#c14-幂等恢复与重试风暴) |

### C2 — 虚拟时序
| 子项 | 现状 | 残余缺口 | 链到 05 |
|------|------|----------|---------|
| C2.1 sleep 快进 | ✅ | 非晶振漂移 | [C2.1](./05-simulation-consistency-and-fidelity-spec.md#c21-sleep--定时唤醒快进) |
| C2.2 脉宽零 Yield | ✅ | 与 C14.2 交叠 | [C2.2](./05-simulation-consistency-and-fidelity-spec.md#c22-脉宽测量零-yield-环回) |
| C2.3 去抖/RC | ✅ | 噪声为注入参数 | [C2.3](./05-simulation-consistency-and-fidelity-spec.md#c23-去抖--rc-低通锚定虚拟时钟) |
| C2.4 周期任务过理想 | 🟡 | 缺受控抖动默认开启 | [C2.4](./05-simulation-consistency-and-fidelity-spec.md#c24-单中断友好采样周期) |

### C3 — 竞态
| 子项 | 现状 | 残余缺口 | 链到 05 |
|------|------|----------|---------|
| C3.1 Task↔Task 无锁 | ❌ | 待混沌+TSan | [C3.1](./05-simulation-consistency-and-fidelity-spec.md#c31-无锁共享读写tasktask) |
| C3.2 Task↔ISR | ❌ | 待多点插 ISR | [C3.2](./05-simulation-consistency-and-fidelity-spec.md#c32-taskisr-无锁交叉) |
| C3.3 多字段撕裂 | ❌ | 待影子内存字段级 | [C3.3](./05-simulation-consistency-and-fidelity-spec.md#c33-多字段结构体撕裂) |
| C3.4 发布序/flag | ❌ | 弱内存不全日间 | [C3.4](./05-simulation-consistency-and-fidelity-spec.md#c34-发布-订阅顺序假设) |

### C4 — 临界区 / 中断
| 子项 | 现状 | 残余缺口 | 链到 05 |
|------|------|----------|---------|
| C4.1 临界区门禁 | 🟡 | 加深断言 | [C4.1](./05-simulation-consistency-and-fidelity-spec.md#c41-临界区门禁enterexit) |
| C4.2 Poll 投递 | 🟡 | 非任意刺入 | [C4.2](./05-simulation-consistency-and-fidelity-spec.md#c42-调度点-isr-投递poll-模型) |
| C4.3 优先级嵌套 | ❌ | Phase 4+ / 真机 | [C4.3](./05-simulation-consistency-and-fidelity-spec.md#c43-优先级嵌套) |
| C4.4 FromISR 误用 | ❌ | 待上下文门禁 | [C4.4](./05-simulation-consistency-and-fidelity-spec.md#c44-fromisr--非-isr-safe-api-误用) |
| C4.5 IRQ 队列溢出 | 🟡 | 需 Fail-Loud 强化 | [C4.5](./05-simulation-consistency-and-fidelity-spec.md#c45-pending-中断队列溢出) |

### C5 — 阻塞 / WDT
| 子项 | 现状 | 残余缺口 | 链到 05 |
|------|------|----------|---------|
| C5.1 STRICT_NONBLOCKING | ✅ | 间接调用见 C5.4 | [C5.1](./05-simulation-consistency-and-fidelity-spec.md#c51-strict_nonblocking-编译期隐藏阻塞-api) |
| C5.2 软 WDT | ❌ | 待实现 | [C5.2](./05-simulation-consistency-and-fidelity-spec.md#c52-软-wdt虚拟时间未喂狗) |
| C5.3 饿死统计 | ❌ | 待实现 | [C5.3](./05-simulation-consistency-and-fidelity-spec.md#c53-就绪任务饿死) |
| C5.4 间接阻塞 | 🟡 | 运行时门禁不全 | [C5.4](./05-simulation-consistency-and-fidelity-spec.md#c54-动态间接阻塞) |
| C5.5 优先级反转 | ❌ | 继承未承诺 | [C5.5](./05-simulation-consistency-and-fidelity-spec.md#c55-优先级反转) |

### C6 — 内存
| 子项 | 现状 | 残余缺口 | 链到 05 |
|------|------|----------|---------|
| C6.1 堆配额 | ✅ | 碎片几何≠真机 | [C6.1](./05-simulation-consistency-and-fidelity-spec.md#c61-静态堆配额耗尽) |
| C6.2 碎片 | 🟡 | 压力模式可选 | [C6.2](./05-simulation-consistency-and-fidelity-spec.md#c62-堆碎片化) |
| C6.3 ASan/UBSan | ✅ | Wasm 日间不全开 | [C6.3](./05-simulation-consistency-and-fidelity-spec.md#c63-asan--ubsanuaf越界未对齐溢出) |
| C6.4 NO-MALLOC-APP | ✅ | — | [C6.4](./05-simulation-consistency-and-fidelity-spec.md#c64-app-禁裸-malloc) |
| C6.5 Per-task 栈 | 🟡 | 与 FreeRTOS 栈模型差 | [C6.5](./05-simulation-consistency-and-fidelity-spec.md#c65-per-task-栈溢出) |

### C7 — 协议
| 子项 | 现状 | 残余缺口 | 链到 05 |
|------|------|----------|---------|
| C7.1 CRC 同源 | 🟡 | 残留 Bypass | [C7.1](./05-simulation-consistency-and-fidelity-spec.md#c71-同源协议帧与-crc) |
| C7.2 ACK/重试 | 🟡 | — | [C7.2](./05-simulation-consistency-and-fidelity-spec.md#c72-ack-超时与重试) |
| C7.3 JSON 门禁 | ✅ | — | [C7.3](./05-simulation-consistency-and-fidelity-spec.md#c73-json-语义仿真门禁) |
| C7.4 Bypass 审计 | 🟡 | 持续清零 | [C7.4](./05-simulation-consistency-and-fidelity-spec.md#c74-残留-bypass-清零审计) |

### C8 — DMA 窗口
| 子项 | 现状 | 残余缺口 | 链到 05 |
|------|------|----------|---------|
| C8.1 传输耗时挂起 | ❌ | 同步 API 残留 | [C8.1](./05-simulation-consistency-and-fidelity-spec.md#c81-粗粒度传输耗时挂起) |
| C8.2 完成 IRQ 序 | ❌ | 依赖 C8.1 | [C8.2](./05-simulation-consistency-and-fidelity-spec.md#c82-完成中断与任务唤醒序) |
| C8.3 sync 残留清单 | 🟡 | 需标注迁移 | [C8.3](./05-simulation-consistency-and-fidelity-spec.md#c83-同步-api-残留) |

### C9～C12（边界清晰类）
| 子项 | 现状 | 说明 | 链到 05 |
|------|------|------|---------|
| C9.1 单核边界 | 🚫/❌ | ADR-0014 | [C9](./05-simulation-consistency-and-fidelity-spec.md#c9--多核-smp-真实并发) |
| C9.2 混沌近似 | ❌ | Phase 4 | 同上 |
| C10.1 软步进 | 🟡 | 非硬实时 | [C10](./05-simulation-consistency-and-fidelity-spec.md#c10--快环-isrfoc--硬定时器) |
| C10.2 PWM-ADC 同步 | 🚫 | HIL | 同上 |
| C11.* 电气 | 🚫 | 查表近似除外 | [C11](./05-simulation-consistency-and-fidelity-spec.md#c11--电气--模拟电路特性) |
| C12.1 日间轨 | ✅ 功能 | 非指令级 | [C12](./05-simulation-consistency-and-fidelity-spec.md#c12--cpu--abi-指令级一致性) |
| C12.2 夜间双轨 | ❌ | Phase 5 | 同上 |

### C13 — 生命周期（P0）
| 子项 | 现状 | 残余缺口 | 链到 05 |
|------|------|----------|---------|
| C13.1 冷启动复位清单 | 🟡 | 清单需钉死并测 | [C13.1](./05-simulation-consistency-and-fidelity-spec.md#c131-冷启动bss--静态初值--外设默认电平) |
| C13.2 复位原因注入 | ❌ | 待模型 | [C13.2](./05-simulation-consistency-and-fidelity-spec.md#c132-热重启--软复位原因) |
| C13.3 再 init | 🟡 | 幂等契约待统一 | [C13.3](./05-simulation-consistency-and-fidelity-spec.md#c133-外设-deinit--再-init) |
| C13.4 Zombie/UAF | 🟡 | 依赖 Sanitizer | [C13.4](./05-simulation-consistency-and-fidelity-spec.md#c134-任务对象生命周期与-zombie-gc) |

### C14 — 快进 / 锁步（P0）
| 子项 | 现状 | 残余缺口 | 链到 05 |
|------|------|----------|---------|
| C14.1 禁止双重步进 | 🟡 | 需 CI 守门强化 | [C14.1](./05-simulation-consistency-and-fidelity-spec.md#c141-时钟单一写入--禁止双重步进) |
| C14.2 快进不丢边沿 | 🟡 | 全局最小事件时间 | [C14.2](./05-simulation-consistency-and-fidelity-spec.md#c142-快进跨越边沿--半窗-debounce) |
| C14.3 Plant 锁步 | 🟡 | 禁墙钟 plant | [C14.3](./05-simulation-consistency-and-fidelity-spec.md#c143-plantos-锁步漂移) |
| C14.4 Pin 队列溢出 | 🟡 | Fail-Loud | [C14.4](./05-simulation-consistency-and-fidelity-spec.md#c144-pin-event-queue-溢出--丢失) |
| C14.5 观测/注入序 | 🟡 | 序契约测试 | [C14.5](./05-simulation-consistency-and-fidelity-spec.md#c145-观测与注入竞态) |

### C15 — Host↔Wasm（P0）
| 子项 | 现状 | 残余缺口 | 链到 05 |
|------|------|----------|---------|
| C15.1 Asyncify 契约 | 🟡 | 错误覆盖回归要全 | [C15.1](./05-simulation-consistency-and-fidelity-spec.md#c151-asyncify-挂起契约) |
| C15.2 Poll 非 Push | ✅ | — | [C15.2](./05-simulation-consistency-and-fidelity-spec.md#c152-中断-pushpoll--重入) |
| C15.3 bigint ABI | ✅ | 第三方插件风险 | [C15.3](./05-simulation-consistency-and-fidelity-spec.md#c153-bigint--指针-abi) |
| C15.4 Bypass 泄露 | 🟡 | 探针可加强 | [C15.4](./05-simulation-consistency-and-fidelity-spec.md#c154-语义-bypass-泄露) |
| C15.5 Worker 隔离 | ✅ | — | [C15.5](./05-simulation-consistency-and-fidelity-spec.md#c155-worker-隔离与主线程-starve) |

### C16 — OS 语义（P0）
| 子项 | 现状 | 残余缺口 | 链到 05 |
|------|------|----------|---------|
| C16.1 Mutex/超时/递归 | 🟡 | 对照表+测试钉死 | [C16.1](./05-simulation-consistency-and-fidelity-spec.md#c161-mutex-锁--超时--递归) |
| C16.2 Queue 满策略 | 🟡 | 与真机逐项对齐 | [C16.2](./05-simulation-consistency-and-fidelity-spec.md#c162-queue--ringbuf-满与覆盖策略) |
| C16.3 事件vs超时序 | 🟡 | 需钉死胜者 | [C16.3](./05-simulation-consistency-and-fidelity-spec.md#c163-阻塞等待与超时唤醒序) |
| C16.4 事件组等 | — | 未暴露则 N/A | [C16.4](./05-simulation-consistency-and-fidelity-spec.md#c164-任务通知--事件组若暴露) |
| C16.5 死锁检测 | ❌ | 可选门禁 | [C16.5](./05-simulation-consistency-and-fidelity-spec.md#c165-死锁检测) |

### C17 — 资源互斥
| 子项 | 现状 | 残余缺口 | 链到 05 |
|------|------|----------|---------|
| C17.1 引脚冲突 | 🟡 | codegen 门禁加强 | [C17.1](./05-simulation-consistency-and-fidelity-spec.md#c171-引脚复用冲突) |
| C17.2 定时器独占 | 🟡 | 资源表不全 | [C17.2](./05-simulation-consistency-and-fidelity-spec.md#c172-定时器--pwm-通道独占) |
| C17.3 APB 时钟副作用 | 🚫 | 真机 | [C17.3](./05-simulation-consistency-and-fidelity-spec.md#c173-apb--外设时钟变更副作用) |
| C17.4 PWM 更新毛刺 | 🟡 | 可选模型 | [C17.4](./05-simulation-consistency-and-fidelity-spec.md#c174-pwm-占空比更新毛刺--相位连续性) |
| C17.5 总线争用 | 🟡 | 重叠窗口检测 | [C17.5](./05-simulation-consistency-and-fidelity-spec.md#c175-共享总线所有权多驱动争用) |

### C18～C21
| 子项 | 现状 | 残余缺口 | 链到 05 |
|------|------|----------|---------|
| C18.1 I2C NACK/stretch/hang | ❌ | Phase 3 扩展 | [C18](./05-simulation-consistency-and-fidelity-spec.md#c18--总线故障态机超越-crc丢包) |
| C18.2 UART framing/FIFO | ❌ | 同上 | 同上 |
| C18.3 SPI 模式/CS | ❌ | 同上 | 同上 |
| C19.1 半传输/双缓冲 | ❌ | 依赖 C8 | [C19](./05-simulation-consistency-and-fidelity-spec.md#c19--dma--缓冲生命周期) |
| C19.2 传输中复用 buffer | ❌ | 同上 | 同上 |
| C20.1 回调内 yield | 🟡 | 上下文门禁 | [C20](./05-simulation-consistency-and-fidelity-spec.md#c20--回调重入--延迟下半部) |
| C21.1 uint32 回绕 | 🟡 | 缺专项快进用例 | [C21](./05-simulation-consistency-and-fidelity-spec.md#c21--时间与计数回绕) |

### C22～C25（边界 / 按需）
| 子项 | 现状 | 说明 | 链到 05 |
|------|------|------|---------|
| C22.* 低功耗/时钟域 | 🚫 | 极简 stub 可选 | [C22](./05-simulation-consistency-and-fidelity-spec.md#c22--电源--低功耗--时钟域) |
| C23.1 NVS 撕裂 | 🟡 | 按需注入 | [C23](./05-simulation-consistency-and-fidelity-spec.md#c23--持久化--nvs--磨损) |
| C24.* cache/DMA RAM | 🚫 | 真机 | [C24](./05-simulation-consistency-and-fidelity-spec.md#c24--缓存--内存属性--dma-ram) |
| C25.1 UBSan 溢出等 | 🟡 | Host CI | [C25](./05-simulation-consistency-and-fidelity-spec.md#c25--浮点--数值与编译器-ub) |

---

## 3. 场景侧阅读顺序（非排期 SSOT）

嵌入式关注优先级（实现排期以工程计划为准）：

| 次序 | 项 | 建议动作 |
|------|----|----------|
| 1 | C14 + C15 | 快进/锁步契约与宿主边界 Fail-Loud |
| 2 | C13 + C16 | 冷启动清单 + OS 语义对照表钉死 |
| 3 | C5.2/C5.3 | 软 WDT + 饿死统计 |
| 4 | C8 + C18/C19 | 异步 DMA + 总线故障 + buffer 生命周期 |
| 5 | C3 + C4 | 混沌调度 + TSan + FromISR 门禁 |
| 6 | C17 | 引脚/定时器资源冲突门禁 |
| 7 | C7.4 | Bypass 清零审计持续 |
| 8 | C12 | 夜间 ABI 双轨 |
| — | C9/C10/C11/C22/C24 | 真机 / HIL / 🚫 口径 |

---

## 4. 明确不在仿真一致性承诺内的事项

即使未来 Phase 完成、仿真轴 **A～F** 全部落地，也**不应**对外宣称「仿真已等价真机」或「可免真机/HIL 放行」。完整口径见 [05 §0.4](./05-simulation-consistency-and-fidelity-spec.md#04-af-完备后的生产口径与残余不一致) 与目录 [README](./README.md)。

即使未来 Phase 完成，也**不应**对外宣称「仿真已等价真机」：

1. 电路级 SPICE / 电源完整性全仿真（C11）
2. 浏览器内 Xtensa/RISC-V 微架构周期精确仿真（日间轨）（C12）
3. ESP32 双核 + 缓存一致性完整复刻（C9/C24）
4. 完整时钟树 / APB 变更全副作用（C17.3/C22）
5. deep sleep 电流与唤醒延时波形（C22）
6. Flash 物理磨损均衡（C23）
7. 以仿真结果替代认证级 EMC / 功能安全评估

对外口径遵循 [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)：**行为/虚拟时序高保真；时序并发与电气级以真机为准；仿真用于尽早抓逃逸，而非证明无逃逸。** A～F 完备 = 生产级**预检流水线**，≠ 虚实恒等。

---

## 5. 维护规程

1. **场景支持度变更**：只改本文件矩阵/子项「现状」。
2. **问题定义 / 方案 / 预言变更**：只改 [05](./05-simulation-consistency-and-fidelity-spec.md)。
3. **新增子场景**：先写 05 五字段，再在本文件加一行链接。
4. **禁止双写**技术方案长文。

---

## 修订记录

| 日期 | 说明 |
|------|------|
| 2026-07-31 | 初稿：C1～C12 |
| 2026-07-31 | SSOT 去重：方案交 05，本文件为索引 |
| 2026-07-31 | 对齐 05：扩展至 C1～C25；关键子场景速查表；修复旧 §4.1/§5.2 死链 |
| 2026-08-02 | §4 链回 05 §0.4 / README：A～F 完备 ≠ 免真机放行 |

