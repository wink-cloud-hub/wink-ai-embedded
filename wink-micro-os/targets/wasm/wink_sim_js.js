/**
 * wink_sim_js.js — Wasm 侧 `extern js_*` 符号的默认 JS-library 实现（ADR-0019 wrapper 模式）
 *
 * 编译期通过 `--js-library=<this>` 由 emcc 注入到 wink_simulator.js 胶水层，
 * 提供 wasm_bridge.h 里所有 `js_pal_*` / `js_sim_*` extern 符号的默认实现，
 * 让本仓的 wink_simulator.wasm/.js 无须外挂 JS 侧胶水即可在 Node / 浏览器
 * 里跑起来（node stub 只用来 smoke，Workbench 前端会**覆盖**这里的实现）。
 *
 * 为什么需要它：Emscripten 6.x 下 wasm `extern` symbol 只能靠
 *   ①`--js-library` 编译期注入，或 ②`--pre-js` 在初始化前覆盖 wasmImports。
 * 未在 `wasmImports.env` 里出现的 `js_*` 直接被 glue 编译成 `abort('missing
 * function: ...')`，Module 顶层属性挂 property 不会被 wasm-loader 感知。
 * 本文件采用方案 ①，让 wasm 侧无论谁做宿主都有一个"跑得起来"的下限。
 *
 * ========================================================================
 * 覆盖机制（ADR-0019 wrapper 模式，spike 已验证）
 * ========================================================================
 * 每个符号的库函数体都是一个 wrapper：
 *   function (args...) {
 *       if (typeof Module !== 'undefined' && typeof Module.js_xxx === 'function') {
 *           return Module.js_xxx(args...);
 *       }
 *       // 默认桩实现
 *   }
 *
 * 这样宿主（Workbench 前端 / 单元测试 mock）只需要在 factory config 或
 * post-factory 实例上给 `Module.js_xxx` 赋值，wrapper 每次调用都查 Module
 * 属性，两种时机等价：
 *
 *   // 方式 A：factory config
 *   const Module = await WasmSandbox({
 *       js_pal_gpio_write: (pin, level) => postToUI({...}),
 *   });
 *
 *   // 方式 B：post-factory（必须在 wasm 首次调用该 import 前完成）
 *   const Module = await WasmSandbox({});
 *   Module.js_pal_gpio_write = (pin, level) => postToUI({...});
 *
 * ⚠️ 历史坑（ADR-0019 §背景 spike）：
 *   1. 之前的实现直接是 `js_pal_gpio_write: function(pin, level){}` 硬编码，
 *      运行时给 `Module.js_pal_gpio_write` 赋值**不生效**——library 被编译期
 *      固化进 wasmImports.env，Module 属性不在其中。头部注释历史版本承诺
 *      "Module 顶层挂 property 即可覆盖" 是错的，wrapper 模式才使承诺兑现。
 *   2. `js_pal_os_sleep_ms__async: true` 在 emcc 6.x 下**不触发** Asyncify
 *      自动包装——`src/jsifier.mjs:482` 只识别 `'auto'`，`true` 只是元数据
 *      标记。之前 sleep 是"立即返回"的空操作，Asyncify 从未真正生效。修正
 *      为 `'auto'` 后 emcc 才用 `Asyncify.handleAsync` 包装 Promise 返回值。
 *
 * ========================================================================
 * Asyncify 契约（ADR-0019 §落地规则 4，面向 host 覆盖 sleep/busy_wait）
 * ========================================================================
 * 宿主覆盖 `js_pal_os_sleep_ms` / `js_pal_os_busy_wait_us` 时**必须返回 Promise**：
 *
 *   // ✅ 正确
 *   Module.js_pal_os_sleep_ms = (ms) =>
 *       new Promise(resolve => scheduleWakeAtVirtualUs(nowUs + ms*1000, resolve));
 *
 *   // ❌ 错误——sync 返回触发 Asyncify unwind→rewind 死循环，main 剩余
 *   //         代码被反复执行，无编译期或运行时诊断
 *   Module.js_pal_os_sleep_ms = (ms) => { clock.advance(ms); };
 *
 * 唯一防线是 TS 侧 `WasmImports.js_pal_os_sleep_ms: Promise<void>` 类型标注
 * （见 ../../../../wink-ai/packages/unisim/src/unisim/types/wasm/imports.ts，Phase B B1 落地）。
 *
 * ========================================================================
 * 宿主分工提醒
 * ========================================================================
 * 本桩只保证 wasm 能"跑动"，不保证宿主 event loop 里其它 timer/interval 能被
 * 公平调度。在 Node 主线程直接 require 本胶水会让 Asyncify unwind→rewind 循环
 * starve 掉外部 setTimeout（长跑还会 OOM）。因此 node stub 把 wasm 关进
 * worker_thread 隔离；Workbench 前端同理，应把 wasm runtime 放进 Web Worker，
 * 主 UI 线程只做消息驱动。
 *
 * ========================================================================
 * 时间 SSOT（P2-1 清理后）
 * ========================================================================
 * C 侧 pal_os_get_us/ms() 直接读 s_virtual_us 内存（零 JS 调用），虚拟时钟
 * 唯一推进入口是 pal_wasm_advance_virtual_clock(bigint)（C→JS 导出）。
 * js_pal_os_get_ms / js_pal_os_get_us 已在 Phase C P2-1 清理中删除（死桩，
 * 从未被 wasm 实际导入）。需要读时钟的宿主代码请自行持有 VirtualClock 实例。
 *
 * ========================================================================
 * 契约与 SSOT
 * ========================================================================
 * 符号集合与签名以 targets/wasm/wasm_bridge.h 为 SSOT；漂移即 node stub smoke
 * 失败（wink_sim_stub.js 会静态解析 wasm imports 与已知集合比对）。
 */

addToLibrary({
    /* ---- Asyncify 让出点 ----
     * `__async: 'auto'` 是 emcc 6.x 的正确语法（`true` 无效）。emcc 会用
     * `Asyncify.handleAsync` 包装函数体的 Promise 返回值，与
     * `-sASYNCIFY_IMPORTS=['js_pal_os_sleep_ms', ...]` 声明匹配。
     *
     * 默认桩的时钟推进契约（ADR-0013 / pal_osal_wasm.c header 注释）：
     *   当宿主未覆盖 js_pal_os_sleep_ms/js_pal_os_busy_wait_us 时，本桩用
     *   wall-clock setTimeout 让出，并在 Promise resolve 前**先**推进 C 侧
     *   s_virtual_us（_pal_wasm_advance_virtual_clock），保证
     *   pal_os_get_us() 看到的虚拟时间随 sleep 单调前进——否则协作调度器
     *   pal_sim_scheduler_run() 会永远把 wakeup_us 当成"未来"陷入无限 sleep
     *   循环（t1_us/t1_ms 永远 == 0）。
     *
     *   覆盖钩子的宿主（jest / Workbench）自己负责虚拟时钟推进——通常通过
     *   一个 VirtualClock 单步 + _pal_wasm_advance_virtual_clock 双锁步
     *   （见 ../../../../wink-ai/packages/unisim/src/unisim/bridge/createUnisimImports.ts +
     *   nodeSmoke.test.ts 锁步循环）。此时本桩的 setTimeout+advance 路径不
     *   会执行，不会发生双重推进。
     *
     *   _pal_wasm_advance_virtual_clock 由 pal_osal_wasm.c 以
     *   EMSCRIPTEN_KEEPALIVE 导出，签名为 (uint64_t us) → void，WASM_BIGINT=1
     *   下以 BigInt 传参。 */
    js_pal_os_sleep_ms__postset: "Module.setSimMode = function (mode) { var v = (mode === 'HEADLESS') ? 1 : 0; if (typeof Module._pal_wasm_set_sim_mode === 'function') { Module._pal_wasm_set_sim_mode(v); } };",
    js_pal_os_sleep_ms__async: 'auto',
    js_pal_os_sleep_ms: function (ms) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_os_sleep_ms === 'function') {
            return Module.js_pal_os_sleep_ms(ms);
        }
        var advanceUs = BigInt(ms) * 1000n;
        return new Promise(function (resolve) {
            setTimeout(function () {
                if (typeof Module !== 'undefined' && typeof Module._pal_wasm_advance_virtual_clock === 'function') {
                    Module._pal_wasm_advance_virtual_clock(advanceUs);
                }
                resolve();
            }, ms);
        });
    },

    js_pal_os_busy_wait_us__async: 'auto',
    js_pal_os_busy_wait_us: function (us) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_os_busy_wait_us === 'function') {
            return Module.js_pal_os_busy_wait_us(us);
        }
        /* 桩：把微秒转 ms 后 setTimeout 让出；真实宿主可切换到 spin-wait。
         * 注意：推进值按"请求的微秒数"（非 setTimeout 实际等待），与 C 侧
         * busy_wait 语义一致——调用者期望至少 us 微秒已过去。 */
        var waitMs = Math.max(1, Math.floor(us / 1000));
        var advanceUs = BigInt(us);
        return new Promise(function (resolve) {
            setTimeout(function () {
                if (typeof Module !== 'undefined' && typeof Module._pal_wasm_advance_virtual_clock === 'function') {
                    Module._pal_wasm_advance_virtual_clock(advanceUs);
                }
                resolve();
            }, waitMs);
        });
    },

    /* ---- 分级日志桥接（P1-L1）----
     * UTF8ToString 由 emscripten 内置；level 对应 pal_log_level_t
     * (ERROR=1, WARN=2, INFO=3, DEBUG=4)。宿主可覆盖 Module.js_pal_log
     * 把日志转发到 UI 面板。*/
    js_pal_log: function (level, msgPtr) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_log === 'function') {
            return Module.js_pal_log(level, msgPtr);
        }
        var msg = UTF8ToString(msgPtr);
        switch (level) {
            case 1: (console.error || console.log).call(console, '[wink E] ' + msg); break;
            case 2: (console.warn  || console.log).call(console, '[wink W] ' + msg); break;
            case 3: (console.info  || console.log).call(console, '[wink I] ' + msg); break;
            case 4: /* debug: 默认桩不输出以保持 smoke 输出干净；宿主打开 verbose 时覆盖即可 */ break;
            default: console.log('[wink ?] ' + msg); break;
        }
    },

    /* ---- PAL HAL 默认 no-op（Workbench 侧要接入真实 UI 反馈）---- */
    js_pal_gpio_write: function (pin, level) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_gpio_write === 'function') {
            return Module.js_pal_gpio_write(pin, level);
        }
    },
    js_pal_gpio_read: function (pin) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_gpio_read === 'function') {
            return Module.js_pal_gpio_read(pin);
        }
        return 0;
    },
    /* P1 electrical SSOT — PinArbiter-backed; Host overrides via Module.* */
    js_pal_gpio_read_state: function (pin) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_gpio_read_state === 'function') {
            return Module.js_pal_gpio_read_state(pin);
        }
        return 2; /* HiZ default */
    },
    js_pal_gpio_drive_ideal: function (pin, level) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_gpio_drive_ideal === 'function') {
            return Module.js_pal_gpio_drive_ideal(pin, level);
        }
    },
    js_pal_gpio_release_ideal: function (pin) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_gpio_release_ideal === 'function') {
            return Module.js_pal_gpio_release_ideal(pin);
        }
    },
    js_pal_gpio_release_mcu: function (pin) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_gpio_release_mcu === 'function') {
            return Module.js_pal_gpio_release_mcu(pin);
        }
    },
    js_pal_pwm_set_duty: function (channel, duty) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_pwm_set_duty === 'function') {
            return Module.js_pal_pwm_set_duty(channel, duty);
        }
    },
    js_pal_i2c_transfer: function (port, addr, wbuf, wlen, rbuf, rlen) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_i2c_transfer === 'function') {
            return Module.js_pal_i2c_transfer(port, addr, wbuf, wlen, rbuf, rlen);
        }
        /* 桩：立即返回成功；真实实现须模拟设备响应写入 rbuf */
        return 1;
    },
    js_pal_spi_transfer: function (port, deviceId, txbuf, len, rxbuf, mode, sckHz) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_spi_transfer === 'function') {
            return Module.js_pal_spi_transfer(port, deviceId, txbuf, len, rxbuf, mode, sckHz);
        }
        /* 桩：无设备模型时 NACK（false）；Host 覆盖后走 SPIBus */
        return 0;
    },
    js_pal_uart_write: function (port, buf, len) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_uart_write === 'function') {
            return Module.js_pal_uart_write(port, buf, len);
        }
    },

    /* ---- 中断桥 Poll 模型（方案 C）默认无 pending ---- */
    js_pal_register_interrupt: function (pin, cbIdx, argPtr) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_register_interrupt === 'function') {
            return Module.js_pal_register_interrupt(pin, cbIdx, argPtr);
        }
    },
    js_pal_deregister_interrupt: function (pin) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_deregister_interrupt === 'function') {
            return Module.js_pal_deregister_interrupt(pin);
        }
    },
    js_pal_poll_interrupt: function (outCbPtr, outArgPtr) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_poll_interrupt === 'function') {
            return Module.js_pal_poll_interrupt(outCbPtr, outArgPtr);
        }
        return 0;
    },

    /* ---- GPIO notify bridge ---- */
    js_pal_gpio_on_write: function (pin, level) {
        if (typeof Module !== 'undefined' && typeof Module.js_pal_gpio_on_write === 'function') {
            return Module.js_pal_gpio_on_write(pin, level);
        }
    },

    /* ---- Phase3 Plugin Channel API (2026-07-21) ----
     * 标准插件通道读取：从 JS 侧插件实例读取状态值。
     * ADR-0019 规约：原样传递 C 指针（由 unisim/worker createUnisimImports 的 readCString 处理）。
     * 默认桩返回 -1（表示插件未初始化，C 侧使用旧路径或默认值）。
     */
    js_sim_get_plugin_channel: function (instanceIdPtr, channelNamePtr) {
        if (typeof Module !== 'undefined' && typeof Module.js_sim_get_plugin_channel === 'function') {
            return Module.js_sim_get_plugin_channel(instanceIdPtr, channelNamePtr);
        }
        return -1.0; // 未初始化哨兵
    },
});
