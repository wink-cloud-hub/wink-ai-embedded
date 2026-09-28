# ESP-IDF 仿真 H6：代际句柄令牌原型与集成边界

| 字段 | 内容 |
|---|---|
| 状态 | **阶段性集成；四类运行时句柄与真实 UniSim 场景已验证，H6 尚未完成** |
| 日期 | 2026-09-28 |
| 范围 | `wink-micro-os/frameworks/esp_idf/` 的 Host/Wasm 仿真句柄；不修改 `ESP_PLATFORM` 真机 ABI |
| 关联实施计划 | [ESP-IDF 仿真基建加固 H6](../../../implementation-plans/esp32/2026-09-28-esp-idf-simulation-hardening-plan.md) |
| 关联决策 | [ADR-0012 契约诚实](../../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0013 协作调度](../../../decisions/unisim/0013-sim-cooperative-scheduler.md)、[ADR-0089 堆能力契约](../../../decisions/core/0089-esp-idf-heap-caps-allocation-contract.md) |
| 原型 | `wink-micro-os/frameworks/esp_idf/test/probes/h6_handle_token_probe.c`、`h6_handle_token_wasm_test.js` |

## 1. 已确认的问题与句柄盘点

静态池槽位删除后可立刻复用。只检查 `used` 的裸池地址无法区分旧引用和新对象；仅在同一槽中增加 generation 字段也不够，因为旧指针仍指向更新后的字段。令牌必须在创建时冻结身份，解析时比较槽位当前身份，不能解引用编码为指针的令牌。

| 家族 | 现行编码与外部持有者 | 删除/复位及失效表现 | H6 集成要求 |
|---|---|---|---|
| Queue / Semaphore / EventGroup | `void *` 指向各自静态池；App/任务可长期保存 | `used=false` 或 `memset` 后旧地址在复用时重新有效；失败多返回 `pdFAIL`/`0` | 优先迁移，解析器须按预期家族验证令牌，阻塞恢复点须再次校验 |
| Task | 现有 `gen:16 + slot:8` 编码为 `TaskHandle_t`；App 与同步原语保存任务 ID | 删除和复位递增 `uint16_t gen`，回绕后跳过 0，但旧令牌在足够多次复用后可碰撞；无效句柄由各 API 按现行值处理 | 与共享序号策略统一，保留 `NULL` 表示当前任务的特殊语义 |
| NVS | `uint32_t` 值为 `slot+1`；App 保存 | close/flash reset 清 `in_use`，同槽重新 open 可使旧值重新有效；现有读写入口多返回 `ESP_ERR_INVALID_ARG`，`nvs_close` 无返回值且只按槽位清状态，`nvs_commit` 当前忽略句柄 | 需要整数句柄专用编码，先固定失效返回契约，再覆盖全部读写/commit/close 入口；不能直接复用指针转换 |
| I2C master/legacy、SPI、GPTimer | 结构体指针或 `void *` 指向模块池；App/驱动保存 | 模块独立复位与删除；旧槽复用可 ABA | 逐模块核对上限、回调借用与错误码，再登记资源家族 |
| Event handler instance、HTTP、MQTT、netif 等 | handler/client/netif 的静态对象地址可能被 App 或异步 fiber 保存 | 注销、destroy、软复位可作废；旧 fiber 与复用槽已有历史缺陷 | 与 H5 事件所有权和生产者停机顺序一起审查，不能用 FreeRTOS 原型直接替换 |

头文件中还声明了未实现或未纳入当前模拟覆盖的句柄类型。上表只记录已发现的主要实现家族；正式 H6 前须从 `esp_idf_sources.cmake` 和覆盖矩阵生成完整逐 API 清单，不能把 7 个原型类型位当成全仓已足够。

## 2. 原型候选与选择

| 候选 | 优点 | 关键代价 |
|---|---|---|
| 池对象地址 + generation 字段 | 现有调用点改动少 | 旧指针读到新 generation，不能解决同槽 ABA |
| 不复用的独立描述符指针 | 公共 `void *` 类型自然 | 每次创建消耗描述符；全模块重建后 Wasm 线性内存地址仍可重复，需外部实例身份 |
| 32 位不可解引用令牌 | Host64/Wasm32 共用编码；静态池无需每次分配描述符；可按家族、槽位、创建序号检查 | 整数转指针行为依赖目标 ABI；序号空间有限；完整模块重建需浏览器侧交接单调序号 |

本原型选择第三种作为 **首批 FreeRTOS/NVS 家族的阶段性实现**，暂不把它提升为全 H6 最终契约。令牌布局为 bit 0 固定 1、bits 1–6 槽位（0–63）、bits 7–9 家族（1–7）、bits 10–31 全局单调序号（1–4,194,303）。`NULL/0` 保留为无效或 API 特殊值。创建先检查槽位空闲与序号未耗尽，转换为 `void *` 再转回 `uintptr_t` 校验，最后才提交状态。解析先把不可信句柄转回整数、核对家族/槽位/序号和池中 `used`，绝不解引用令牌。删除/软复位清存活位，保留全局序号；下次创建得到不同身份。达到最大序号后**拒绝创建**，不回绕、不重用旧值。

全局序号必须跨所有采用此编码的资源家族唯一；若各模块各自从 1 开始，错误家族/同槽可能别名。独立探针的 `h6_init()` 每个实例只能调用一次，创建前必须初始化，防止运行中的实例把序号回滚；运行时的 `esp_sim_handle_issue()` 则用静态单调序号，软复位不清零。完整 Wasm 模块销毁后，浏览器侧仍需把旧模块末尾序号传给新实例，且在交接期间禁止旧实例继续创建。当前运行时尚无该交接 ABI，单纯新建模块并从 0 开始会重用令牌，**不保证跨实例安全**。本次用户接受 H3 阶段性豁免后，已将四类句柄先行接入以取得真实仿真证据；在跨实例所有权、耗尽策略和其他资源家族完成前不得把 H6 标为完成。

## 3. 原型验证

以下命令均在仓库根目录运行；输出文件留在忽略的 `build/` 或 WSL `/tmp`，没有修改运行时源码。

```text
# WSL Ubuntu 24.04 / GCC 13.3 / Host64 / ASan+UBSan
gcc -std=c11 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra -Werror wink-micro-os/frameworks/esp_idf/test/probes/h6_handle_token_probe.c \
  -o /tmp/h6_handle_token_probe
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /tmp/h6_handle_token_probe
# H6 token probe passed: Host64, max_slots=64, max_sequence=4194303

# Windows MinGW GCC 16.2 / Host32
gcc -std=c11 -O2 -Wall -Wextra -Werror \
  wink-micro-os/frameworks/esp_idf/test/probes/h6_handle_token_probe.c \
  -o build/h6_handle_token_probe.exe
build/h6_handle_token_probe.exe
# H6 token probe passed: Host32, max_slots=64, max_sequence=4194303

# Windows Emscripten 6.0.9 / Node 22.23.2 / Wasm32
emcc -std=c11 -O0 -Wall -Wextra -Werror -c \
  wink-micro-os/frameworks/esp_idf/test/probes/h6_handle_token_probe.c \
  -o build/h6_handle_token_probe.o
<emsdk>/upstream/bin/wasm-ld --no-entry --export-all --allow-undefined \
  --import-memory build/h6_handle_token_probe.o -o build/h6_handle_token_probe.wasm
node wink-micro-os/frameworks/esp_idf/test/probes/h6_handle_token_wasm_test.js \
  build/h6_handle_token_probe.wasm
# H6 token probe passed: Wasm32, real module replacement
```

Host C 测试覆盖删除后同槽复用、复位、类型错误、63/64 槽边界、初始化/回滚拒绝及最大序号处拒绝回绕。Node 测试创建三个 `WebAssembly.Instance` 与独立线性内存：前两个验证序号交接后旧令牌不能操作新模块对象，第三个验证耗尽边界。原型直接使用 `wasm-ld` 链接最小模块；本机 `emcc` 的完整 JS 链接长时间无输出，因而这里未验证完整 ESP-IDF Wasm 运行时集成。`--allow-undefined` 链接后检查只有 `env.memory` 导入；正式门禁不能依赖宽松未定义符号设置。

## 4. 剩余设计门

1. 固定完整资源家族清单、各 profile 槽位上限、现行失效返回值和外部持有者；决定超过 7 个家族时的类型命名空间，保证 32 位内不碰撞。
2. 决定谁拥有跨 Wasm 实例的序号、何时交接、同页多实例如何串行化，以及达到 4,194,303 次创建后的 fail-loud 用户语义。若不能证明，改选具有独立实例身份的描述符/桥接方案。
3. 写公开 API 的旧句柄红测，再逐家族迁移解析器；所有阻塞恢复点与异步回调在访问对象前重新解析。完成 Host、真实 Wasm runtime、sanitizer 和 profile size/map 验证后才能标 H6 完成。
4. H3 完整 phase3 POSIX Host sanitizer 仍未通过。本次运行时首批集成沿用用户明确要求的阶段性豁免；H3 正式门禁未解除，H6 结项仍须补齐相应证据或修订门禁。

## 5. 真正仿真验收的 PASS 条件

独立 C/Node 探针仅证明令牌编码可行，**不是 H6 仿真场景 PASS**。正式接入后，使用 `wink-ai/packages/wink-tools/wink.py` 源码入口构建实际 ESP-IDF 仿真 App，并由 UniSim 运行含显式 oracle 的场景。至少保留以下三类证据：

1. `python wink.py build wasm --app <H6-fixture-app>` 和 `python wink.py build sim --app <H6-fixture-app>` 均成功，产出与本次源码一致的 Wasm/UniSim 资产；按需补 Host 构建。构建成功本身只证明可编译。
2. `python wink.py sim run --app <H6-fixture-app> --mode headless --scenarios <H6-scenario-dir> --reporter junit --artifacts <result-dir>` 返回成功，报告零失败/零跳过；场景对旧队列/信号量/事件组/NVS 等已迁移家族分别断言删除后复用、软复位及重新实例化后的失效返回值，同时断言新句柄仍可用。输入、轨迹、JUnit 和产物路径要可定位。
3. `wink.py test --with-wasm` 或等价的定向 CTest 记录发现数、通过数和失败项；真实 Wasm 场景测试与编译检查分别报告。若其他无关工具套件失败，只能报告 H6 专项通过及全量门禁未通过，不能写“整体验收 PASS”。

首批 fixture 与真实场景已创建并执行，证据见 §6。场景目前验证同槽复用；软复位由 Host NVS/Node Wasm runtime 定向测试覆盖，跨完整 Wasm 实例的正式运行时交接仍未实现。因此 H6 保持未完成。

## 6. 首批运行时集成与 UniSim 证据（2026-09-28）

- `esp_sim_handle_issue/decode` 在仿真运行时统一发放并验证令牌；Queue、Semaphore、EventGroup 与 NVS 使用冻结的 token，池槽位复用后旧句柄不能重新生效。Host 公开 API 回归测试 `test_esp_idf_freertos`、`test_esp_nvs` 为 2/2 PASS，NVS 软复位后同槽重新打开仍拒绝旧句柄。
- 阻塞恢复边界另做三条红测：旧队列读取者、信号量等待者、事件组等待者在资源删除后、同槽创建新对象并给新对象数据/信号时，旧等待者曾误读新对象。三个用例均先红；`sync_block` 返回后先用原 token 重新解析，再触碰等待者数组或资源数据后转绿。队列发送、接收、窥视的恢复点共用此检查。
- 阻塞恢复边界另做三条红测：旧队列读取者、信号量等待者、事件组等待者在资源删除后、同槽创建新对象并给新对象数据/信号时，旧等待者曾误读新对象。三个用例均先红；`sync_block` 返回后先用原 token 重新解析，再触碰等待者数组或资源数据后转绿。队列发送、接收、窥视的恢复点共用此检查。
- `wink-micro-app/fixtures/esp_idf_h6_handles/` 是真实 ESP-IDF Wasm App：四个 API 家族分别点亮 GPIO2/4/5/18；只有旧句柄拒绝且新句柄可用才点亮。`wink-ai/packages/wink-tools/wink.py build wasm` 与 `build sim` 均成功；`sim run --mode headless --scenarios ... --reporter junit` 报 1 个场景、4 个断言 PASS，JUnit 为 `build/h6-unisim-artifacts/junit-report.xml`（tests=1, failures=0）。
- 分层/API lint 无发现，许可地图门禁通过。Emscripten 完整链接首次在受限缓存访问下停滞；允许访问工作区外 SDK 缓存后构建在约 8 秒内成功。独立探针的直接 Wasm 链接记录仍是当时的原型证据，不替代本节真实运行。
- Task、I2C/SPI/GPTimer、HTTP/MQTT/netif 等句柄家族尚未迁移。当前场景没有覆盖 `esp_restart` 或完整模块重新实例化，浏览器侧序号交接 ABI 尚待决策和实现；本次仅称“首批四类场景 PASS”。

## 5. 真正仿真验收的 PASS 条件

独立 C/Node 探针仅证明令牌编码可行，**不是 H6 仿真场景 PASS**。正式接入后，使用 `wink-ai/packages/wink-tools/wink.py` 源码入口构建实际 ESP-IDF 仿真 App，并由 UniSim 运行含显式 oracle 的场景。至少保留以下三类证据：

1. `python wink.py build wasm --app <H6-fixture-app>` 和 `python wink.py build sim --app <H6-fixture-app>` 均成功，产出与本次源码一致的 Wasm/UniSim 资产；按需补 Host 构建。构建成功本身只证明可编译。
2. `python wink.py sim run --app <H6-fixture-app> --mode headless --scenarios <H6-scenario-dir> --reporter junit --artifacts <result-dir>` 返回成功，报告零失败/零跳过；场景对旧队列/信号量/事件组/NVS 等已迁移家族分别断言删除后复用、软复位及重新实例化后的失效返回值，同时断言新句柄仍可用。输入、轨迹、JUnit 和产物路径要可定位。
3. `wink.py test --with-wasm` 或等价的定向 CTest 记录发现数、通过数和失败项；真实 Wasm 场景测试与编译检查分别报告。若其他无关工具套件失败，只能报告 H6 专项通过及全量门禁未通过，不能写“整体验收 PASS”。

上述 fixture、场景文件和运行报告在本次原型阶段尚未创建或执行，因此本设计与实施计划继续把 H6 标为未完成。
