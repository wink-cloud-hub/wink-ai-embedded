/**
 * wink_sim_js.js — Wasm 侧 `extern js_*` 符号的默认 JS-library 实现
 *
 * 编译期通过 `--js-library=<this>` 由 emcc 注入到 wink_simulator.js 胶水层，
 * 提供 wasm_bridge.h 里所有 `js_pal_*` / `js_sim_*` extern 符号的默认桩实现，
 * 让本仓的 wink_simulator.wasm/.js 无须外挂 JS 侧胶水即可在 Node / 浏览器
 * 里跑起来（node stub 只用来 smoke，Workbench 前端会**覆盖**这里的实现）。
 *
 * 为什么需要它：Emscripten 6.x 下 wasm `extern` symbol 只能靠
 *   ①`--js-library` 编译期注入，或 ②`--pre-js` 在初始化前覆盖 wasmImports。
 * 通过 `Module.js_*` 顶层挂 property 不会被 wasm-loader wire —— 生成的 glue
 * 会直接 abort('missing function: js_pal_os_sleep_ms')。本文件采用方案 ①，
 * 让 wasm 侧无论谁做宿主都有一个「跑得起来」的下限。
 *
 * 覆盖机制：Emscripten JS-library 定义的每个符号都是 default，被 Module 顶层
 * 属性同名覆盖前生效。所以 Workbench 前端未来只需要 `Module.js_pal_gpio_write
 * = customImpl` 即可替换本文件的默认实现（前提是 Emscripten 5.x+，本仓 6.0.1
 * 版本适用）；不需要重新编译 wasm。
 *
 * 契约：符号集合与签名以 targets/wasm/wasm_bridge.h 为 SSOT；漂移即 node
 * stub smoke 失败。
 */

addToLibrary({
    /* ---- Asyncify 让出点 ----
     * `js_pal_os_sleep_ms__async: true` 告诉 emscripten 这个 import 返回 Promise，
     * emcc 会自动用 `Asyncify.handleAsync` 包装它 —— 由 wasm 侧 `pal_os_sleep_ms`
     * 触发 Asyncify unwind，Promise resolve 时 rewind 回 wasm。
     * 必须与 -sASYNCIFY_IMPORTS=['js_pal_os_sleep_ms', ...] 一致。
     *
     * 宿主分工提醒：本桩只保证 wasm 能"跑动"，不保证宿主 event loop 里其它
     * timer/interval 能被公平调度。在 Node 主线程直接 require 本胶水会让
     * Asyncify unwind→rewind 循环 starve 掉外部 setTimeout（长跑还会 OOM）。
     * 因此 node stub 把 wasm 关进 worker_thread 隔离；Workbench 前端同理，
     * 应把 wasm runtime 放进 Web Worker，主 UI 线程只做消息驱动。
     */
    js_pal_os_sleep_ms__async: true,
    js_pal_os_sleep_ms: function (ms) {
        return new Promise(function (resolve) { setTimeout(resolve, ms); });
    },

    js_pal_os_busy_wait_us__async: true,
    js_pal_os_busy_wait_us: function (us) {
        /* 桩：把微秒转 ms 后 setTimeout 让出；真实宿主可切换到 spin-wait */
        return new Promise(function (resolve) {
            setTimeout(resolve, Math.max(1, Math.floor(us / 1000)));
        });
    },

    /* ---- 同步 getters（Asyncify 不介入）---- */
    js_pal_os_get_ms: function () {
        return BigInt(Date.now());
    },
    js_pal_os_get_us: function () {
        return BigInt(Date.now()) * 1000n;
    },

    /* ---- PAL HAL 默认 no-op（Workbench 侧要接入真实 UI 反馈） ---- */
    js_pal_gpio_write: function (pin, level) {},
    js_pal_gpio_read: function (pin) { return 0; },
    js_pal_pwm_set_duty: function (channel, duty) {},
    js_pal_i2c_transfer: function (port, addr, wbuf, wlen, rbuf, rlen) {
        /* 桩：立即返回成功；真实实现须模拟设备响应写入 rbuf */
        return 1;
    },

    /* ---- 中断桥 Poll 模型（方案 C）默认无 pending ---- */
    js_pal_register_interrupt: function (pin, cbIdx, argPtr) {},
    js_pal_deregister_interrupt: function (pin) {},
    js_pal_poll_interrupt: function (outCbPtr, outArgPtr) { return 0; },

    /* ---- DAL bypass 默认桩：ultrasonic 返 ~17cm 让 avoidance_car 跑得动 ---- */
    js_sim_trigger_ultrasonic: function (trigPin) {},
    js_sim_measure_echo_pulse_us: function (trigPin) { return 1000; },
});
