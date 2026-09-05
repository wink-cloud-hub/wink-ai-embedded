# 4.1 Wasm 运行沙箱生命周期、Web Worker 隔离与 Asyncify 异步调度

在 Web 仿真环境中，如何安全、高效、不卡顿地运行用 C 语言编写的嵌入式主循环与任务调度，是平台设计的核心技术难点。本文件详细解析了 Wasm 仿真沙箱生命周期、Web Worker 线程隔离、以及基于 Emscripten Asyncify 与 Wasm Table 的异步调度实现。

---

## 1. Web Worker 线程隔离设计

### 1.1 为什么必须采用 Web Worker？
嵌入式 C 代码主循环通常是一个不退出的 `while(1)` 或由 FreeRTOS 任务调度器控制的抢占式死循环。
*   如果将 WebAssembly 直接加载在浏览器的主线程（UI 渲染线程）运行，Wasm 的高 CPU 占用将瞬间占满单线程，导致浏览器无法响应用户交互（点击、拖拽均无响应），UI 停止刷新（卡死），并抛出“网页无响应”系统警告。
*   **解决方案**：平台采用 **Web Worker** 启动独立的后台线程运行 Wasm 沙箱，通过 `postMessage` 机制与前端主线程进行状态和数据的低频交互，确保主线程 UI 渲染永远维持在 60 FPS。

### 1.2 线程交互架构与生命周期数据流

```text
  [ 前端 UI 主线程 (Vue 3) ]                       [ Web Worker Wasm 沙箱线程 ]
             │                                                 │
             │ ─── 1. POST: { type: 'start', wasmBytes } ───►  │
             │                                                 ├─ 2. WebAssembly.instantiate()
             │                                                 ├─ 3. 调用 main() -> 初始化 OSAL 调度器
             │                                                 │
             │                                                 │ (C 逻辑调用 pal_gpio_write)
             │ ◄── 4. POST: { type: 'pin_write', pin, lvl } ───┤
             │                                                 │
      (用户点击虚拟按键)                                         │
             │ ─── 5. POST: { type: 'pin_input', pin, lvl } ──►│
             │                                                 ├─ 6. 覆写 Wasm 对应虚拟引脚状态
             │                                                 │
             │ ─── 7. POST: { type: 'pause' } ──────────────►  ├─ 8. 挂起 Wasm 调度协程
             │ ─── 9. POST: { type: 'stop' } ───────────────►  └─ 10. 销毁 Wasm 实例与 Worker
```

### 1.3 wasm 二进制产出与 App 注入路径

从 `wink-micro-os/` 顶层执行：

```bash
# 一次性激活 emsdk（本机路径示例）
& 'D:\software\embedded\emsdk\emsdk_env.ps1'

# 构建（默认 App = samples/avoidance_car）
emcmake cmake -S . -B build-wasm -DTARGET_PLATFORM=wasm
cmake --build build-wasm

# 换 App 变体（AI 生成的 App 或其它 sample）
emcmake cmake -S . -B build-wasm -DTARGET_PLATFORM=wasm \
    -DWINK_APP_DIR=<absolute-path-to-app>
```

**产出**：
- `build-wasm/wink_simulator.wasm` — 唯一二进制（未压缩，交给 Workbench 前端仓的 Worker 加载）；
- `build-wasm/wink_simulator.js` — MODULARIZE 胶水（`WasmSandbox` UMD 导出），已注入默认 `js_*` 桩实现（见 §2.2.2）。

**App 注入契约**：由 [02-wink-micro-os/03-directory-architecture §6.1](../../02-wink-micro-os/03-directory-architecture.md) 约束 3 定义——App CMakeLists 用 `set(WINK_APP_SOURCES ... PARENT_SCOPE)` 向顶层导出源列表；wasm target 是"所有 App 变体共用一个二进制 target"的沙箱模型（与 host 分支的"每 App 一个可执行"模型对称）。

### 1.4 本仓 Node 侧烟测（不替代前端 Workbench）

`wink-micro-os/targets/wasm/wink_sim_stub.js` 是**编译期契约门禁**，不是宿主替代品：

- 静态解析 `wink_simulator.wasm` 的 `env.js_*` imports 集合，与预期集合比对——漂移即 fail；
- 在 `worker_threads.Worker` 里加载 `wink_simulator.js`，等 `onRuntimeInitialized` 到达即判定 PASS。

⚠ **必须走 worker**：Emscripten 6.x Asyncify 的 `unwind → rewind` 循环与 Node 主线程 event loop 同居会 starve 其它 timer 并导致长跑 OOM（`setTimeout(resolve, 10ms)` + rewind 同步链会形成 tight loop）。这个观察**同样适用于浏览器**——Workbench 前端必须把 wasm 隔离到 Web Worker，主 UI 线程只做消息驱动。参见 §1.1。

---


## 2. Emscripten Asyncify 协程挂起机制

### 2.1 阻塞延时冲突
嵌入式代码中存在大量阻塞延时调用（如 `pal_delay_ms(100)`）。在单线程的 Worker 内部，直接空转死等会使 Worker 线程同样无法处理 `onmessage` 队列中的事件（如无法处理主线程传来的 `pin_input` 按键点击消息）。

### 2.2 Asyncify 解决方案
**Asyncify** 是 Emscripten 编译器的核心特性。它能够在 Wasm 代码调用 JS 异步函数（如 `setTimeout`、`Promise`）时，**挂起当前的 Wasm 执行栈（保存当前寄存器和栈帧），将 CPU 执行权交还给浏览器事件循环**。等 JS 异步回调完成后，Asyncify 会**重新恢复 Wasm 的执行栈，并从暂停位置继续向下执行**。

> ⚠ **两端契约（缺一不可，见 ADR-0019）**：Asyncify 挂起由 **C/链接侧** 与 **JS 侧** 共同保证：
> 1. **C/链接侧**：`-s ASYNCIFY_IMPORTS=[...]` 声明哪些 JS import 是异步挂起点（合法设置名为 `ASYNCIFY_IMPORTS`，非限制插桩范围的 `ASYNCIFY_ONLY`/`ASYNCIFY_ADD`）；
> 2. **JS 侧（`--js-library` 场景）**：库函数必须在 `addToLibrary({...})` 中同时提供 **函数体返回 `Promise`** 与 **`<symbol>__async: 'auto'` 元数据**。emcc 6.x 的 `jsifier.mjs` 只在 `__async === 'auto'` 时自动包 `Asyncify.handleAsync`；`__async: true` **不生效**，Promise 会被丢弃、wasm 侧调用立即返回（ADR-0019 spike 证伪）。
> 3. **JS 侧（手写 `handleSleep` 场景）**：不使用 `--js-library` 时，import 实现须显式 `return Asyncify.handleSleep((wakeUp) => {...})`，且 `wakeUp` 必须被调用且仅调用一次。
> 二者缺一即坏：仅 C 侧声明 IMPORTS 而 JS 侧未 handleAsync/handleSleep → 静默失效（wasm 直接跑过 sleep）；JS 侧包了但 C 侧未列入 IMPORTS → wakeUp 抛 `invalid Asyncify state`。C 侧见 §2.2.1，JS 侧见 §2.2.2。

#### 2.2.1 C 侧桥接声明 (`pal_hal_wasm.c`)
```c
#include "pal_osal.h"

// 声明外部 JS 侧导入的异步挂起函数
extern void js_pal_delay_ms(uint32_t ms);

void pal_delay_ms(uint32_t ms) {
    // 静态映射，Asyncify 会拦截此外部调用并挂起
    js_pal_delay_ms(ms);
}
```

#### 2.2.2 JS 侧异步延时拦截实现 (Worker 内部)

**本仓的默认实现（ADR-0019 落地形式）**：`wink-micro-os/targets/wasm/wink_sim_js.js` 通过 `emcc --js-library=...` 编译期注入所有 `wasm_bridge.h` 声明的 `js_*` 符号默认桩。每个符号采用 **wrapper 模式**：库函数体先查 `Module.js_xxx` 是否被宿主覆盖，命中则委托，否则跑默认实现。`sleep_ms` / `busy_wait_us` 使用 `__async: 'auto'`（**不是** `true`——见下方⚠️），emcc 自动用 `Asyncify.handleAsync` 包装它们的 Promise 返回值。

**覆盖机制契约**：Workbench 前端仓拿到 `wink_simulator.js` 后**不需要**再声明这批符号，只需在 factory config 或 post-factory 实例上给 `Module.js_*` 赋值即可覆盖默认桩（wrapper 每次调用都查 Module 属性，两种时机等价）：

```typescript
// 方式 A：factory config（推荐——首次调用前就已生效）
const module = await WasmSandbox({
  // 覆盖默认 no-op：把 GPIO 写通知给主 UI 线程
  js_pal_gpio_write: (pin: number, level: boolean) => {
    self.postMessage({ type: 'pin_write', pin, level });
  },
  js_pal_i2c_transfer: (port, addr, wbuf, wlen, rbuf, rlen) => {
    // 覆盖默认桩，走 Virtual Peripheral Registry 分发
    return dispatchI2c(port, addr, wbuf, wlen, rbuf, rlen);
  },
  // sleep_ms 保留默认 setTimeout 实现即可；若需精细虚拟时间步进则覆盖，
  // 但必须返回 Promise（见下方 Asyncify 契约）
});

// 方式 B：post-factory（必须在 wasm 首次调用该 import 前完成）
const module = await WasmSandbox({});
module.js_pal_gpio_write = (pin, level) => postToUI(...);
```

⚠ **Emscripten 6.x 关键前提（ADR-0019 §背景，spike 已证伪三种"愿望"）**：
- **`Module.js_* = fn` 顶层挂 property 单独不生效**——若库函数体是硬编码 `function(pin, level) {}`（无 wrapper），wasm-loader 编译期把默认实现固化进 `wasmImports.env`，运行时给 Module 赋值只是在 Module 对象上加属性，wasm 侧不看。**本仓通过在 `wink_sim_js.js` 每个符号里加 Module 查找 wrapper 才使覆盖生效**（ADR-0019 选项 B）。
- **`__async: true` **不**触发 Asyncify 自动包装**——emcc 6.x `src/jsifier.mjs:482` 只识别 `'auto'`，`true` 是元数据标记。写错会导致 wasm 侧 `pal_os_sleep_ms(N)` 立即返回（Promise 被丢弃），且**无编译期或运行时诊断**。
- **Wasm 符号只能"覆盖"，不能"新增"**——glue 内部把每个未在 `--js-library` / `--pre-js` 提供的 `js_*` 直接编译成 `abort('missing function: ...')`。若前端仓需要新增桥接符号，必须走：加 `wasm_bridge.h` extern 声明 → 加 `wink_sim_js.js` 默认桩（含 wrapper）→ 重编 wasm，两步不可跳过。

**Asyncify Promise 契约（面向 Workbench / Phase B）**：

Host 覆盖 `js_pal_os_sleep_ms` / `js_pal_os_busy_wait_us` 时**必须返回 Promise**。返回同步值（`undefined` / 数字 / 字符串）会导致 Asyncify 陷入 unwind→rewind 死循环，main 剩余代码被反复执行，**无任何编译期或运行时诊断**（ADR-0019 spike #8 证实）。

**唯一防线**：Phase B `types/wasm/imports.ts` 的 `WasmImports` 接口把这两个符号标为 `Promise<void>` 返回类型，TS 覆盖实现在编译期就会被强制 `async` / 显式 `Promise`。

```typescript
// ✅ 正确
Module.js_pal_os_sleep_ms = (ms: number): Promise<void> => {
  return new Promise((resolve) => {
    // 虚拟时钟推进到 wake_at 时再 resolve（Phase B §5.3）
    scheduleWakeAt(clock.getUs() + BigInt(ms) * 1000n, resolve);
  });
};

// ❌ 错误——sync 返回，触发 Asyncify 死循环
Module.js_pal_os_sleep_ms = (ms) => {
  clock.advance(BigInt(ms) * 1000n);
  // 无 return —— TS 若未声明 Promise<void> 类型，此错误可能溜过
};
```

**手写 `handleSleep` 的等价形式**（仅当不使用 `--js-library` 时；本仓走 `--js-library`+wrapper，此段仅供参考）：

```typescript
const wasmImports = {
  env: {
    js_pal_os_sleep_ms: (ms: number) => {
      return Asyncify.handleSleep((wakeUp) => {
        setTimeout(wakeUp, ms);
      });
    },
    js_pal_os_busy_wait_us: (us: number) => {
      return Asyncify.handleSleep((wakeUp) => {
        setTimeout(wakeUp, Math.max(0, us / 1000));
      });
    }
  }
};
```

**契约验收要求（交付前端/Workbench 仓实现方）：**
1. **不可**写成「`setTimeout` 后直接 `return`」——那不会恢复 Wasm 栈，下次调用即 `invalid Asyncify state`。若走 `--js-library`+wrapper 路径，覆盖函数体**必须** `return new Promise(...)`。
2. `wakeUp` / `resolve` 必须**被调用且仅调用一次**；重复/遗漏调用即 `StateError`。
3. **入口与调度**：须用 `Module.callMain()`（**非** `Module._main()`）启动——`MODULARIZE=1` + `ASYNCIFY=1` 下只有 `callMain` 正确处理被插桩的 main；且 `main` 是 `wink_runtime_run(cb, 0)` 的永不返回驱动循环（靠内部 `delay` 反复挂起-唤醒推进），**JS 不得 `await callMain()` 等其结束**，否则偶发挂起/重入异常。
4. **必须在 Web Worker 里加载 wasm**：Asyncify unwind→rewind 循环与主线程 event loop 同居会 starve 其它 timer 甚至 OOM（本仓 node stub 首次跑通时观察到 20s 内堆爆），主 UI 线程只做消息驱动，不 host wasm。

#### 2.2.3 编译参数配置
在云编译时，必须指示 Emscripten 编译器启用 Asyncify 并注册异步挂起点函数列表（与 `wink-micro-os/CMakeLists.txt` 一致，SSOT 闭合）：
```bash
emcc main.c pal_osal_wasm.c -o simulator.js \
  -s ASYNCIFY=1 \
  -s ASYNCIFY_IMPORTS=["js_pal_delay_ms", "js_pal_delay_us"] \
  -s ASYNCIFY_STACK_SIZE=65536 \
  -s STACK_OVERFLOW_CHECK=2 \
  -s ASSERTIONS=1
```

> ⚠ **`ASYNCIFY_IMPORTS` 仅含真正挂起点（ADR-0003 / D2 裁决）**：
> - `js_pal_delay_ms` / `js_pal_delay_us`：经 `pal_osal_wasm.c` 的 `pal_delay_ms/us` 调用，是真实 Asyncify 挂起点。
> - `js_pal_get_ms/us`：同步读，不挂起，**不**列入。
> - `js_pal_i2c_transfer`：**同步零拷贝**（`pal_hal_wasm.c` 直接返回其 bool，见 §3），**非挂起点，禁列入 IMPORTS**——旧文档误列，已裁决移除。
> - `STACK_OVERFLOW_CHECK=2` + `ASSERTIONS=1`：dev/debug 构建，溢出/重入时 abort 而非静默栈损坏（亦为 §4 中断重入的早期报警）；生产 profile 的体积/性能权衡留作后续。
> - `ASYNCIFY_STACK_SIZE=65536`：起步值；最终须以**最深 AI 生成调用链**实测压到刚好不触发恢复失败的下限（业务逻辑递归深度不可预测）。

---

## 3. WASM 共享内存与零拷贝数据读取 (Shared Memory Pointer)

在 I2C 屏幕刷新、UART 字节数据通信等高频数据交换场景中，如果频繁序列化和反序列化数据会产生巨大开销。我们通过直读 **Wasm 共享内存（Memory Buffer）** 来实现零拷贝传输。

*   当 Wasm 侧的 C 代码发起 `pal_i2c_transfer(port, dev_addr, write_buf, write_len, read_buf, read_len)` 时，发送缓冲区 `write_buf` 和接收缓冲区 `read_buf` 实际为 WASM 内部线性内存的**地址指针（偏移量数字）**。
*   在 JS 侧，可以通过访问 `wasmInstance.exports.memory.buffer` 直接在对应地址上建立 `TypedArray` 视图：

```typescript
// JS 侧拦截并直读 Wasm 内存
function js_pal_i2c_transfer(
  port: number,
  dev_addr: number,
  write_buf_ptr: number,
  write_len: number,
  read_buf_ptr: number,
  read_len: number
): boolean {
  const wasmMemory = wasmInstance.exports.memory.buffer;
  
  // 1. 建立共享视图，零拷贝读取 Wasm 内部的数据包
  if (write_len > 0 && write_buf_ptr !== 0) {
    const writeData = new Uint8Array(wasmMemory, write_buf_ptr, write_len);
    // 将 writeData 投递给前端虚拟 I2C 设备解析 (如虚拟 OLED SSD1306)
    virtualI2CBus.write(dev_addr, writeData);
  }
  
  // 2. 仿真外设产生的数据，零拷贝直接写回 Wasm 内存对应的接收地址中
  if (read_len > 0 && read_buf_ptr !== 0) {
    const responseData = virtualI2CBus.read(dev_addr, read_len);
    const readView = new Uint8Array(wasmMemory, read_buf_ptr, read_len);
    readView.set(responseData);
  }
  
  return true;
}
```

---

## 4. 硬件中断的 Wasm Table 函数指针路由机制（方案 C：Poll 模型）

在物理单片机中，硬件中断通过中断向量表中的函数指针 `pal_gpio_isr_t` 处理。Wasm 沙箱安全限制 **禁止 JS 侧直接执行 Wasm 内部的任意内存地址**，因此须通过 Table 索引映射间接路由。

> ⚠ **架构变更（方案 C 落地）**：旧版 §4 使用「Push 模型」——JS 随时调用 Wasm 导出的 `_trigger_wasm_interrupt`，会在 Asyncify sleeping 窗口触发确定性重入崩溃（D1）。**本节描述的是已落地的 Poll 模型**，`_trigger_wasm_interrupt` 已从 Wasm 导出中永久移除。

### 4.1 Wasm Table 索引基础

Wasm 编译器把所有 C 函数指针统一保存在一个叫做 **Table** 的数组中。C 的 `pal_gpio_isr_t` 函数指针在 Wasm 内部实际是该 **Table 数组的索引值（Index）**：

```text
  [ Wasm Table (函数指针表) ]
  ┌───────┬──────────────────────────┐
  │ Index │ 实际 C 中断处理函数指针    │
  ├───────┼──────────────────────────┤
  │   0   │ NULL                     │
  │   1   │ my_button_press_handler  │ ◄─── 注册的 callback 索引为 1
  │   2   │ sensor_data_ready_isr    │
  └───────┴──────────────────────────┘
```

### 4.2 Poll 模型中断路由（三步契约）

#### 步骤 1：C 中断注册（`pal_hal_wasm.c`）

C 侧注册中断时将函数指针 cast 为 Table 索引，并连同 `arg_ptr` 传给 JS。JS 侧**只存映射表，不执行任何 Wasm 回调**：

```c
// 摘自 pal_hal_wasm.c pal_gpio_enable_interrupt
uint32_t callback_index = (uint32_t)(uintptr_t)callback;  // C 函数指针 → Table 索引
uint32_t arg_ptr        = (uint32_t)(uintptr_t)arg;        // wasm32 线性内存偏移
js_pal_register_interrupt(pin, callback_index, arg_ptr);   // 告知 JS 侧存入映射表
```

#### 步骤 2：JS 侧 GPIO 事件到达时，只写 pending 队列（Worker 内部）

```typescript
// JS 侧 pending 队列（FIFO，容量由 PAL_WASM_INTERRUPT_QUEUE_SIZE 配置，默认 16）
const pendingInterrupts: Array<{ callbackIndex: number; argPtr: number }> = [];
const MAX_PENDING = PAL_WASM_INTERRUPT_QUEUE_SIZE; // 与 pal_wasm_internal.h 保持一致

const wasmImports = {
  env: {
    // C 注册 ISR 时调用——只存映射表
    js_pal_register_interrupt: (pin: number, callbackIndex: number, argPtr: number) => {
      interruptRegistry.set(pin, { callbackIndex, argPtr });
    },
    js_pal_deregister_interrupt: (pin: number) => {
      interruptRegistry.delete(pin);
    },
    // C 每 tick 边界主动调用——返回一个 pending 中断（无则返回 0）
    js_pal_poll_interrupt: (outCallbackIndexPtr: number, outArgPtr: number): number => {
      if (pendingInterrupts.length === 0) return 0;
      const intr = pendingInterrupts.shift()!;
      const memView = new Uint32Array(Module.HEAPU8.buffer);
      memView[outCallbackIndexPtr >> 2] = intr.callbackIndex;
      memView[outArgPtr >> 2]           = intr.argPtr;
      return 1;
    },
  }
};

// GPIO 事件到达入口——只写 pending 队列，绝不直接调用任何 Wasm 导出
function onVirtualPinTrigger(pin: number): void {
  const isrInfo = interruptRegistry.get(pin);
  if (!isrInfo) return;
  if (pendingInterrupts.length < MAX_PENDING) {
    pendingInterrupts.push({ callbackIndex: isrInfo.callbackIndex, argPtr: isrInfo.argPtr });
  } else {
    console.warn(`[WasmBridge] Interrupt queue full, dropping interrupt for pin ${pin}`);
  }
}
```

#### 步骤 3：Wasm tick 边界主动 poll 并分发（`wink_runtime.c` + `pal_hal_wasm.c`）

`wink_runtime_run` 在每个 tick 中，**先** `pal_wasm_dispatch_pending_interrupts()`（drain 所有 pending），**再** `wink_app_delay_ms()`（触发 Asyncify 挂起）：

```
tick N 执行顺序：
  [1] callbacks->loop()                    ← 应用逻辑
  [2] pal_wasm_dispatch_pending_interrupts ← ✅ Wasm 正常运行态，安全分发所有 pending ISR
  [3] wink_app_delay_ms()                  ← Asyncify unwind（sleeping 窗口开始）
       JS 事件到来 → 只写 pending 队列 ↗
  [4] timeout → wakeUp → Rewind
tick N+1 的 [2] drain 上一 tick 积累的新中断
```

```c
// 摘自 pal_hal_wasm.c pal_wasm_dispatch_pending_interrupts（在 SIMULATION 宏下由 runtime 调用）
void pal_wasm_dispatch_pending_interrupts(void) {
    uint32_t callback_index, arg_ptr;
    while (js_pal_poll_interrupt(&callback_index, &arg_ptr)) {
        pal_gpio_isr_t isr = (pal_gpio_isr_t)(uintptr_t)callback_index;
        if (isr != NULL) { isr((void *)(uintptr_t)arg_ptr); }
    }
}
```

### 4.3 安全性分析

| 维度 | 旧 Push 模型 | 新 Poll 模型（方案 C） |
|------|-------------|----------------------|
| Asyncify 重入风险 | ❌ 确定性崩溃（D1） | ✅ 彻底消除 |
| JS 侧实现复杂度 | 低（但不安全） | 低（只写队列） |
| C 侧改动范围 | 无 | `pal_hal_wasm.c`、`wasm_entry.c`、`wink_runtime.c`（仅 SIMULATION 宏） |
| host/esp32 target 影响 | 无 | **无**（`#ifdef SIMULATION` 完全隔离） |
| 与 ESP32 中断语义对称 | ❌ Push 无对应 | ✅ 等效 Bottom-Half 队列消费（ADR-0002） |

### 4.4 中断队列容量配置

容量由 `pal_wasm_internal.h` 中的 `PAL_WASM_INTERRUPT_QUEUE_SIZE`（默认 16）控制，可在构建时覆盖：

```cmake
target_compile_definitions(wink_simulator PRIVATE PAL_WASM_INTERRUPT_QUEUE_SIZE=32)
```

**JS 侧的 `MAX_PENDING` 必须与之保持一致**（跨仓契约，前端仓 Code Review 检查清单）。

### 4.5 跨仓验证清单（须 Emscripten + 前端环境，不阻塞本仓 C 合入）

1. `wasm-objdump -x wink_simulator.wasm | grep trigger` → 无 `_trigger_wasm_interrupt` 符号。
2. sleeping 窗口期间 JS 触发 GPIO 中断 → 全部进入 pending 队列，**不**直接调用任何 Wasm 导出。
3. Wasm 唤醒后、下一次 `delay` 前 → 排队中断按 FIFO 顺序 drain，ISR 正确执行。
4. 全程无 `RuntimeError: invalid Asyncify state` / 无 abort / 无栈损坏。
5. 高频压力：连续 1000 tick + 每 tick 4 次虚拟中断 → ISR 执行次数与触发次数一致（队列不溢出时）。
6. 队列满溢：单 tick 触发超过 `MAX_PENDING` 次中断 → 控制台告警，不静默丢失，不崩溃。

> 两 target 中断模型同源（ADR-0002）：ESP32 = ISR 投递 FreeRTOS Queue、Bottom-Half 任务消费；
> Wasm = JS 写 pending 队列、tick 边界 C 主动 drain。语义对称，均为延迟投递的安全分发模型。

---

## 5. 协作式多任务调度器与虚拟单核仿真

### 5.1 协程上下文抽象（`sim_ctx`）
为了支持 `pal_os_task_create` 创建的多个并发死循环任务，仿真侧废弃了同步直调的退化设计，引入了轻量级物理协程：
* **WASM 侧**：使用 Emscripten 提供的 `<emscripten/fiber.h>`，为每个任务分配独立的数据栈与 Asyncify 栈，利用 Asyncify 在任务挂起点进行安全的堆栈保存与上下文切换。
* **Host 侧**：使用 Win32 Fibers 实现相同语义，保障多平台仿真下行为的高度一致与同源。

### 5.2 确定性协作调度
* **调度点 (Yield Points)**：多任务协作调度仅发生在明确的 Yield 点（如 `pal_os_sleep_ms` 等）。
* **确定性调度 (Deterministic Round-Robin)**：当多个任务同时处于 `READY` 状态时，调度器通过带 Seed 的 PRNG 伪随机数发生器决定轮转顺序，彻底规避并发竞争的时序随机性，保证 CI 测试可 Bit-Exact 100% 重现。
* **自删 GC 机制**：任务自删不允许在其自身的 Fiber 上下文中直接销毁。调度器采用**三段式自删 Zombie 清洗**机制，在切回主调度上下文后安全地释放 Fiber 内存。

