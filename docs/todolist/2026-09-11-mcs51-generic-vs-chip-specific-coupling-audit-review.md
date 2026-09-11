SSOT 终态穿透评审结论：方向对，有 3 个必断的 P0 洞
验证方法：把 §4/§4.1 的每一行落到现网 33 个源码文件的实际耦合点上（读了 bridge/uni_bridge/adc/adc0832/cms8s_adc/cms8s_sys/gpio/timer/uart/extint/xdata/sfr/pcon/clock/trap/peripheral 全部关键段 + tools 脚本 + 跨仓引用）。好的部分先确认，再说洞。
先确认：终态中被代码证实可行的部分
- 描述符门控已是既成模式：mcs51_has_cms8s_io()（mcs51_gpio.cpp:70）、uart_notready_mask_impl（mcs51_uart.cpp:133）、xsfr_window_present/classic_external_bus（mcs51_xdata.cpp:62,119）、has_wdt（cms8s_sys.cpp:91）全部经 mcs51_family_desc() 门控。stage4/5 的"换 caps/描述符"只是换 predicate，机制风险低。
- Hook 机制现成：sfr_write_hooks[256] + trap 注册 API（mcs51_trap.h:39-74）已存在，stage3"TA 改钩子派发"可行（注意保序：TA 通知必须在逐地址 hook 之前，mcs51_bridge.cpp:102 的 GAP-07 注释是约束）。
- Peripheral 表 core 三件套就是 timer/uart/extint（mcs51_peripheral.cpp:40-67），与终态树一致。
- cms8s_adc.cpp:147 已有 get_adc_priv(ctx) 缝，stage2 soc_priv 迁移有现成下刀点。
P0-1（最严重）：§4 的"纯物理 Pin rail"会治好养生壶、同时 ,"治死" iron_ntc——stage1 按现文执行必断
证据链（逐行实测）：
- cms8s_adc.cpp:190：mcs51_adc_get_value(ch)，ch=AN 通道 → mcs51_adc.cpp:73 加 32 → 读虚拟 Pin → E-02 根因，确认。
- mcs51_adc0832.cpp:88：同样调 mcs51_adc_get_value(ch)，ch=0/1（CH0/CH1）。而 test_mcs51_iron_ntc_e2e.c:10 + board_config 证明 iron_ntc 板的 NTC 走的就是 ADC0832 外挂路径。
- stage1 现文："通用 rail 只收物理 Pin""删 32u + ch"——若照做，ADC0832 CH0 改读物理 Pin 0（无驱动→0V），iron_ntc 从"读数正常"变成"误报故障"，E-02 在另一块板上复活。stage1 变更表根本没列 mcs51_adc0832.cpp。
更深一层：ADC0832 是板级外挂，它的 CH0/CH1 在 MCU 上根本没有对应引脚，"物理 Pin 直通"原则对它无定义——这是 CPL-17/终态 §4 双双回避的契约真空。而代码里早有答案：mcs51_uni_bridge.cpp:44 的 host 回退数组是 [64]（031 物理 + 3263 合成），s_host_ext_pin 是 [32]（纯物理）——原作者早就分区了，只是 on-chip 路径 잘못用了合成区。
补丁（改 SSOT §4 层 1 + stage1 任务，由此 stage1 风险大降）：rail 定为双空间分区——0~31 为 MCU 物理 Pin（片上外设经芯片层映射后传入），3263 保留为板级器件通道空间（ADC0832 的 CHx 沿用 32+ch，名正言顺）。stage1 改为：cms8s 传映射后 pin（027），adc0832 一行不改；契约文档写明分区。备选（不推荐）：ADC0832 自建 pull 通道，那是 stage3 devices/ 的量级，别塞进 P0 的 stage1。
附带：test_mcs51_cms8s_adc_e2e.c:34-36 用通道号 0/1/25 注入——rail 语义一变，此用例注入点须同步改为 pin 0/1/27（AN25→P3.3=27），这是 CPL-22 的具体着弹点，stage1 S1-3 应点名这三行，否则执行者必猜错。
P0-2："外设自注册"只有口号，没有机制——stage4 无法执行
终态只写"芯片包自注册""按 target 链入"。但 ADR-0004 禁 weak 符号，core 的三处循环（mcs51_context.cpp reset、mcs51_bridge.cpp:77 microstep、mcs51_pcon.cpp:18 next-event）不能反向引用 chips 符号。唯一满足"单向依赖"的可行机制是：core 自有有界 BSS 注册表 + mcs51_peripheral_register()，芯片包提供 cms8s78xx_register()/at89c52_register() 由 app/board（wink-app.json mcu 注入的选择逻辑） mobil 在 framework init 前调用。调用者身份、注册表上限、重复注册语义，三者 SSOT 全没写。补丁：在 §4 层 2/3 之间加"注册协议"小节（约 10 行），stage4 首个 Task 落mcs51_peripheral_register + 注册表 + ² app 注入调用点。
同理 GpioTraitHooks（§4 层 2 点名的 may_drive/is_analog/pullup）无物理落位。好消息：hook 是无状态代码指针，file-static 存放即多 context 安全（状态走 soc_priv），配 caps_cache 短路——这是我能给出的标准答案，SSOT 应写明"hook 表 file-static + 状态 soc_priv"，而不是留一个"能力钩子层"让人去找不存在的目录。补丁：层 2 注明"逻辑层，物理归属 core（mcs51_trap.h 扩展注册 API）"，stage2 加"定义 hook 表结构（纯声明，零行为）"Task，stage4 只做挂载。
P0-3：T2 与 extbus 的归属写错了，stage2/stage4 会拆错
- T2 不是标准件：mcs51_timer.cpp:36 的 SFR_T2IF=0xC9、RLDL=0xCA、CCEN=0xCE 是 local 常量——标准 8052 的 T2 用 T2CON（0xC8）的 TF2/EXF2，没有 0xC9。终态框图写"标准 Timer0/Timer1"是对的，但 stage4 计划写"通用 Timer0/1/2"——执行者第一天就会问"T2 留多少"。补丁：SSOT 明确"T2 拆分——标准 T2（T2CON/TL2/TH2/RCAP2）留 core，T2IF/RLDL/CCEN/捕获比较划入 cms8s_timer.cpp"；mcs51_timer_init（855~906 行）里那一串 T2IF/EIF2/T34MOD/TL3/CCLx 注册必须跟着搬（stage4 现文只说"承接逻辑"，漏了注册点）。
- extbus 不该搬：classic_external_bus()（mcs51_xdata.cpp:119）判据是 xram_size==0——这是通用概念（任何无片上 XRAM 的 51 家族，包括未来的 STC/N76E003 都有外部 MOVX 总线）。搬去 at89_bus.cpp 之后，mcs51_gpio.cpp:244,277 的两处 mcs51_classic_bus_notify_gpio 调用就变成 core→chips 链接反向依赖，stage6 拆分即断（此即我昨天 P0-5 的 mechanized 版本）。补丁：notify 逻辑与 gpio_bus_bits 留 core，搬的只有状态结构体；且 classicBus/mcs51_classic_bus_* 改名 extbus（"classic"是家族命名，留着就是前缀门禁的漏网——门禁规则也要把 mcs51_* 符号纳入，不止 wink_mcs51_*）。RAM 收益仅 ~8B，但正确性收益是终态自洽。
P1（应改，不阻塞开工）
1. stage3"上移 wink_mcu.h"严重低估范围：实测 6 个 wink-micro-app/mcs51_* 应用直引它，cleanup 工具重写头为它，tools/sdcc_gate/ 有按家族分发的同名头，test/CMakeLists:753,956 有 facade 测试 + shim 抑制。三行计划装不下，变更表须列：6 app、cleanup、sdcc_gate、test/CMakeLists、design 02/03 文档。且目的地 wink-micro-os/include/ 不存在，新建+构建评估要写进 Step。
2. stage5 漏了审计脚本自身：mcs51_shim_audit.py:161 硬编码读 include/REG_CMS8S78XX.H，还交叉校验 mcs51_isr.cpp 的 13 项向量表——stage3 搬头、stage5 缩表会同时打断它和 test_mcs51_xsfr_allowlist_fresh。变更表加这两处。
3. stage2 漏了 extint 反方向：CPL-12 只点了 timer 的 {8,8,6,4}，实测 extint 用的是 {8,8,8,8}（mcs51_extint.cpp:62）——对 CMS8S 过宽（P2.6-7/P3.4-7 本无端口中断却被接受）。Step 4 只写 timer，须加"extint 同读描述符（含收紧侧单测）"。
4. stage6 变更表漏 4 处：test/CMakeLists.txt（board_config 缝 453-496）、design 02/03（bridge __has_include 机制文档）、wink-micro-app/mcs51_health_pot/CMakeLists.txt、codegen boards/mcs51。以及 tools/sdcc_gate/ 整目录在终态树无归位（建议：留位，manifest 驱动）。
5. 终态树补 5 个遗漏文件归位：mcs51_xsfr.hpp（类通用、注释去厂商味，留 core）、mcs51_proxy.hpp（通用，留）、wink_mcs51_pwm_meter.h（host 测量，留）、wink_mcs51_classic_bus.h（见 P0-3 改名）、mcs51_pcon.h（已有 brace 覆盖，确认即可）。
6. has_wdt 用错 predicate：cms8s_sys.cpp:91 以 has_xsfr 代理 WDT 存在性——stage0 冻了精准的 wdt_present，stage4 加一行切换任务。
7. 命名词表：at89c52目录 / at89target / classic.yaml / "AT89C52/classic"四名并存，且 SDCC 门禁已有 stc89c52→at89c52 别名先例——总纲钉一张对照表，否则 manifest 对应规则无法执行。
8. RAM 预算与我 stage0 的 +4B：§8"任何阶段不得净增"字面已破，改"终态不净增、中间增量入账"，+4B 记 stage2 表首行。
对已落地 stage0 的影响：零返工
- irq_vector_table（向量白名单）+ 芯片 reset 逐项装载（复用 wink_mcs51_set_irq_map_entry）的组合 unchanged，我的代码即最终形态。
- port_pin_masks 按 counts 实现，与 CPL-12 的 {8,8,6,4} 字面一致，终态"掩码"二字钉死为 counts 即可。
- wdt_present/uart_count/timer_caps 已有精准字段，P1-6 只是消费它们。
建议
SSOT 现在是"85 分的终态"：分层与依赖方向正确，缺的是可执行性细节（注册协议、hook 落位、rail 双空间、T2/extbus 归属、上移影响面）。补上 P0-1~P0-3（约 40 行文档）后再动计划；P1 可边执行边收敛。要我现在按上述补丁改审计 SSOT（它是只读归档？不——它是 todolist 发现源，计划系列才是执行态；按你 established 的流转规则，这类修正应进计划系列 + 回写审计附录，我会分开处理）并同步修正 stage1/2/4/5/6 的任务行吗？