# CMS8S78xx Anti-False-Green Phase 2 (EPWM) 评审与验收报告

- **评审对象**: CMS8S78xx 增强型 PWM (EPWM) 与硬件刹车标杆防假绿深度改造（Phase 2: 4 个标杆 App）
- **依据规范**: [TECH-20261010-CMS8S78XX-EPWM-AFG-PHASE2](../../zh/tech-designs/mcs51/2026-10-10-cms8s78xx-anti-false-green-phase2-epwm-design.md)
- **执行计划**: [PLAN-20261010-CMS8S78XX-AFG-PHASE2-EPWM](../../implementation-plans/mcs51/2026-10-10-cms8s78xx-anti-false-green-phase2-epwm-plan.md)
- **评审结论**: **PASSED (合格交付)**
- **评审日期**: 2026-10-10

---

## 1. 交付目标回顾与验收矩阵

本阶段针对 CMS8S78xx EPWM 外设的历史弱断言、死角假绿隐患（死角 1～6）以及仿真超时挂死问题进行系统性治理，完成了 4 大标杆应用的物理波形与硬件状态机高保真仿真闭环：

| 标杆应用 | 原厂配置特征 | 理论参数与计算 | 物理断言与步骤 | 仿真性能 (Virtual / Wall) | 阳性状态 |
|---|---|---|---|:---:|:---:|
| **`epwm_down_count`** | 下计数, $PERIOD=4800$, $CMP=2400$, 互补, 24MHz | $T = (4800+1)/24\text{MHz} = 200.04167\,\mu\text{s}$<br>$f = 4998.96\,\text{Hz}$，占空比 50.01% | 1. 断言 PG0(P2.0) 频率 $4998.96 \pm 5\,\text{Hz}$<br>2. 断言 PG1(P2.1) 频率一致且与 PG0 严格互补反相<br>3. 关键点采样：$10050\mu\text{s}$ PG0=1/PG1=0；$10150\mu\text{s}$ PG0=0/PG1=1 | 100ms / 148ms | 🟢 **PASS (7/7)** |
| **`epwm_updown_count`** | 中心对称增减计数, $PERIOD=4800$, $CMP=2400$, 24MHz | $T = 2 \times 4800 / 24\text{MHz} = 400.0\,\mu\text{s}$<br>$f = 2500.00\,\text{Hz}$，脉冲居中对称 | 1. 断言 PG0(P2.0) 频率 $2500 \pm 2\,\text{Hz}$<br>2. 下计数伪装拦截点：在 $T/8 = 50\mu\text{s}$ 采样断言 PG0=0（若下计数此时为 1 必红）<br>3. PG1 互补反相闭环 | 100ms / 134ms | 🟢 **PASS (7/7)** |
| **`epwm_brake_fb`** | 外部 FB0(P0.6) 刹车, Recover 模式, 24MHz | 故障注入立即冻结输出；撤销后于次个过零加载点自动恢复 | 1. 0~20ms：断言 PG0~PG3 正常载波波形<br>2. 20ms：注入 P0.6=1 触发刹车<br>3. 20~40ms：`stableLevel` 长窗口断言 PG0/1 持续冻结为 1、PG2/3 持续冻结为 0（100 周期稳态）<br>4. 40ms：撤销 P0.6，断言加载点后波形恢复<br>5. 80ms：二次注入 P0.6 验证复现性 | 100ms / 137ms | 🟢 **PASS (14/14)** |
| **`epwm_brake_stop`** | 软件刹车触发, Stop 模式, 24MHz | Stop 模式硬件清零 `PWMCNTE`，撤销后维持停止；需软件写 `PWMBRKC[3]=1` 并重新置位使能方可重启 | 1. 0~20ms：断言 PG0 5000Hz 载波与 P3.2 2500Hz 零点中断<br>2. 26ms：断言 P3.3 故障中断翻转与 PG 引脚安全锁定<br>3. 30~50ms：软件撤销刹车后，断言 PG0 持续 20ms 冻结为 1，且断言 P3.2 翻转频率为 0 Hz（杜绝后台偷跑）<br>4. 52~72ms：固件手动重启后，断言 PG0 恢复 5000Hz 振荡 | 100ms / 134ms | 🟢 **PASS (11/11)** |

---

## 2. 变异防御套件与防假绿实证 (S4 Mutation Matrix)

为彻底杜绝断言器退化或由于偶合造成的假绿假阳，构建了全自动变异防御审计工具：
`wink-micro-os/frameworks/mcs51/tools/verify_epwm_anti_false_green.py`。
该工具独立调度 5 大模型变异，强校验进程退出码（`exit_code != 0`，严格要求为 1）、排除任何 Crash/Panic/超时，并正则核验断言器输出的错误签名：

| 变异编号 | 变异注入操作 | 目标 App / 场景 | 拦截判定特征 | 实测结果 | 变异防御结论 |
|---|---|---|---|:---:|:---:|
| **`MUT-PG-DISABLE`** | 驱动中注释屏蔽物理引脚输出 `js_pal_gpio_write()` | `epwm_down_count` | 频率计算检测到 0 个边沿，断言失败 | 🔴 Exit=1 (Match: `Step #1`, 11.58s) | **DEFENDED (有效拦截)** |
| **`MUT-COMP-INVERT`** | 驱动中互补模式强制同相输出 (`mode_levels[1] = mode_levels[0]`) | `epwm_down_count` | `ASSERT_POINT` 在 $10050\mu\text{s}$ 拦截到 `gpio:17` 为 1（预期 0） | 🔴 Exit=1 (Match: `ASSERT_POINT`, 11.56s) | **DEFENDED (有效拦截)** |
| **`MUT-FB-IGNORE`** | 驱动中强制忽略 FB0 外部刹车引脚电平 | `epwm_brake_fb` | 20ms 长窗口 `stableLevel` 判定信号仍在振荡，最大连续保持时间不足 | 🔴 Exit=1 (Match: `Step #4`, 11.47s) | **DEFENDED (有效拦截)** |
| **`MUT-CLOCK-FIXED`** | 驱动中二级分频旁路被篡改（主频折半 / 分频错位） | `epwm_down_count` | 载波频率变为 2499Hz，超出 $4998.96 \pm 5\text{Hz}$ 容差 | 🔴 Exit=1 (Match: `Step #1`, 11.50s) | **DEFENDED (有效拦截)** |
| **`MUT-STOP-NO-CNTE`** | 驱动中 Stop 模式跳过硬件清零 `PWMCNTE` | `epwm_brake_stop` | P3.2 (pin 26) 在刹车维持期检测到持续过零中断（2500Hz vs 预期 0Hz） | 🔴 Exit=1 (Match: `Step #10`, 11.49s) | **DEFENDED (有效拦截)** |

> **关键反假绿发现**:
> 在初始设计中，Stop 模式维持期仅检查了 PG 引脚电平（由于输出仍受 `brake_latched` 钳位，跳过 `PWMCNTE=0` 仍能呈现高电平假绿）。通过运行变异测试，工具第一时间捕获此假绿隐患（`FALSE_GREEN` 报警）。团队随即在场景中补充阶段 3 **P3.2 过零中断彻底停止（0 Hz）**的波形断言，彻底封死了后台计数器偷跑的假绿通道！

---

## 3. 核心技术突破与工程解决

1. **Keil C51 空延时循环转译优化 (`transpile_app_keil_c51.py`)**:
   - 原厂 `epwm_brake_stop/main.c` 使用双层嵌套 `for(i=65530;i>0;i--) for(j=100;j>0;j--);`。在 Native Wasm 中由于缺乏 SFR 交互，空转 655 万次耗时超 10.5 秒 CPU，导致虚拟时间冻结在 0，计数器无法推移；
   - 增强转译工具匹配嵌套空延时循环，精准重写为虚拟时间推进调用 `wink_mcs51_delay_ms(...)` 与 `wink_delay_us(...)`；
   - 标杆运行时间从 **10,454 ms 直降至 134 ms（近 80 倍性能飞跃）**，彻底消除挂起风险，且 100% 保持原厂源码零修改。
2. **多协程 Fiber 10ms 时间片对齐规范**:
   - 明确了 Wasm cooperative fiber 调度切片为 `10000µs` (10ms)。波形断言的时间窗口边界必须与 10ms 整数倍对齐（如 `["50ms", "70ms"]`），消除了因时间片跨度截断导致的虚假频率读数（如 6666Hz 伪超频）。
3. **硬件语义与寄存器对齐 (`REG_CMS8S78XX.H` / `cms8s_epwm.cpp`)**:
   - 修正 CMS8S78xx 硬件 4 通道 (`PG0`~`PG3`) 架构；
   - 严格映射 `PWMMODE[5:4]=01` 互补分发、`PWM0DIV=0xFF` 主频直通旁路、`PWMCON.6` PWMRUN 运行逻辑；
   - 彻底将 CTest 单测改为通过 `wink_mcs51_host_set_ext_pin()` 物理注入引脚电平，废除直接篡改 `sfr_shadow` 的非物理方式。

---

## 4. 全量质量门禁验证记录 (Quality Gates)

| 门禁项 | 命令 / 工具 | 判定标准 | 实测结果 | 结论 |
|---|---|---|---|:---:|
| **QG-1 Host C++ 单元测试** | `ctest --test-dir build/test -R test_mcs51` | 71/71 测试通过，包含 `test_mcs51_cms8s_epwm` | 71/71 Passed (20.07s) | 🟢 **PASS** |
| **QG-2 Python 工具套件回归** | `python -m unittest discover -s test/core` | 82/82 测试通过，覆盖转译器与硬件容量门禁 | 82/82 Passed (2.32s) | 🟢 **PASS** |
| **QG-3 标杆 Wasm 波形场景** | `wink.py sim run --app ... --scenarios ...` | 4 个标杆用例 100% Steps Passed，零误差超标 | 4/4 Passed (全部 <150ms) | 🟢 **PASS** |
| **QG-4 SDCC 硬件编译门禁** | `gate_app_hardware_capacity.py` | CODE < 16KB，Stack free > 32B | 4/4 Passed (Code ~8.8KB, Stack free 223B) | 🟢 **PASS** |
| **QG-5 Upstream 0 篡改审计** | `audit_vendor_mirror.py` | 148 文件 0 drift，原厂 C 代码 0 变更 | 148/148 reverified, 0 findings | 🟢 **PASS** |
| **QG-6 分层架构与 API 门禁** | `winkcli lint --pack layering --pack api` | Layering, DAL/PAL 契约规则零违规 | 0 findings, 100% Clean | 🟢 **PASS** |
| **QG-7 开源许可地图门禁** | `check_license_map.py` | 满足 ADR-0083/0084 分层许可合规要求 | Satisfied (LGPL/GPL/Apache) | 🟢 **PASS** |
| **QG-8 自动化变异防御套件** | `verify_epwm_anti_false_green.py` | 5 大故障变异 100% 精准拦截且签名匹配 | 5/5 Intercepted (0 Crash, 0 Timeout) | 🟢 **PASS** |

---

## 5. 后续规划 (Next Phase Recommendations)

随着 Phase 2 EPWM 标杆全面达成防假绿交付，MCS-51 仿真与硬件同源治理已建立成熟的工业级防线。建议后续规划如下：
1. **Phase 3（系统保护与模拟闭环）**:
   - `wdt` / `wwdt` 看门狗硬件超时复位因果闭环；
   - `acmp` 片内模拟比较器动态迟滞与直连 EPWM 硬件刹车联动闭环；
2. **Phase 4（高速总线交互）**:
   - `spi` 与 `i2c` 硬件外设的从机双向交互闭环。
