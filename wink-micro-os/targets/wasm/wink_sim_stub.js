/**
 * wink_sim_stub.js — Wasm 仿真 runtime 烟测桩（Node.js 侧）
 *
 * 目的：
 *   1. 验证 `build-wasm/wink_simulator.wasm` 与 `.js` 胶水能被 Node 的 WebAssembly 引擎实例化；
 *   2. 验证 wink_sim_js.js（`--js-library`）注入的默认 `js_*` 桩集合与 wasm imports 契约一致；
 *   3. 验证 Asyncify 让出 → rewind 循环在 `js_pal_os_sleep_ms` 返回 Promise 的场景下不死锁；
 *   4. **验证 Asyncify 真的挂起 wasm 执行**（ADR-0019 Task 0）——通过 `Module.js_pal_os_sleep_ms`
 *      wrapper 覆盖钩子测量 wasm 连续 sleep 调用间的 wall-clock 间隔，判定 `__async: 'auto'` 修复生效。
 *      老 stub 只验 `onRuntimeInitialized` 存活，`__async: true` 失效（Asyncify 空转）也能通过。
 *
 * 架构：主线程负责计时 + 判定；wasm runtime 跑在 Worker Thread 里。理由：
 *   Emscripten 6.x Asyncify 在 Node 下与 `setTimeout(resolve, 10)` 配合会让主
 *   event loop 完全 starve —— 我们的 setTimeout / setInterval 永远排不上号，长
 *   时间跑还会 OOM。把 wasm 关进 Worker 隔离，主线程只等它发第一条 "ready"
 *   消息作为 smoke 通过依据，然后 `worker.terminate()` 强制回收。
 *
 * 不做：
 *   - Workbench 前端胶水（本 stub 只用默认 `wink_sim_js.js` 桩，不覆盖它）；
 *   - Device Registry 语义模拟；
 *   - 长时行为验证（1000ms 后强制退出）。
 *
 * 用法：
 *   cd wink-micro-os
 *   node targets/wasm/wink_sim_stub.js
 *   （Node ≥ 16 内置 BigInt / worker_threads / WebAssembly）
 *
 * 依赖：先执行
 *   emcmake cmake -S . -B build-wasm -DTARGET_PLATFORM=wasm
 *   cmake --build build-wasm
 * 生成 `build-wasm/wink_simulator.js` + `.wasm`。
 *
 * 契约：本 stub 通过 CommonJS 加载 emscripten 胶水，让胶水侧走完整 Asyncify
 *   流程，不做任何 imports 覆盖。若 wink_sim_js.js 未导出某个 wasm 引用的
 *   `js_*` 符号，emscripten 会在编译期直接 abort。`ready` 消息到达 =
 *   onRuntimeInitialized 触发 = wasm 表达式 + Asyncify runtime 存活。
 */

'use strict';

const fs = require('fs');
const path = require('path');
const { Worker, isMainThread, parentPort } = require('worker_threads');

const HERE = __dirname;
/*
 * 构建目录可通过 --build-dir=<path> / WINK_BUILD_DIR 环境变量 / 位置参数覆盖，
 * 默认 wink-micro-os/build-wasm/（即 avoidance_car 默认变体）。这样切换
 * WINK_APP_DIR 出的另一 build 目录（例如 build-wasm-oled）不需要改 stub。
 */
function resolveBuildDir() {
    const argv = process.argv.slice(2);
    const flag = argv.find((a) => a.startsWith('--build-dir='));
    if (flag) return path.resolve(flag.slice('--build-dir='.length));
    const positional = argv.find((a) => !a.startsWith('-'));
    if (positional) return path.resolve(positional);
    if (process.env.WINK_BUILD_DIR) return path.resolve(process.env.WINK_BUILD_DIR);
    return path.resolve(HERE, '..', '..', 'build-wasm');
}

const BUILD_DIR = resolveBuildDir();
const GLUE_PATH = path.join(BUILD_DIR, 'wink_simulator.js');
const WASM_PATH = path.join(BUILD_DIR, 'wink_simulator.wasm');

if (isMainThread) {
    /* ---------- 主线程：预检 + 计时 + 判定 ---------- */
    if (!fs.existsSync(GLUE_PATH) || !fs.existsSync(WASM_PATH)) {
        console.error(`[stub] missing wasm build artifacts under ${BUILD_DIR}`);
        console.error('       run: emcmake cmake -S . -B build-wasm -DTARGET_PLATFORM=wasm && cmake --build build-wasm');
        process.exit(2);
    }

    /* 打印 wasm imports 快照（漂移可视化） */
    const bytes = fs.readFileSync(WASM_PATH);
    const mod = new WebAssembly.Module(bytes);
    const imports = WebAssembly.Module.imports(mod)
        .filter((i) => i.module === 'env' && i.name.startsWith('js_'))
        .map((i) => i.name)
        .sort();
    console.log(`[stub] wasm imports env.js_* (${imports.length}):`);
    for (const name of imports) console.log(`  - ${name}`);

    /* 期望集合：wasm_bridge.h 声明的完整 js_* 全集。任何实际 import 若不在
     * 此集合内，说明有人在别的 header/源文件里偷偷加了新 extern —— 违反
     * "wasm_bridge.h 是 JS 看到的 C 符号 SSOT" 契约。反之 DCE 掉某些没被
     * App 引用的符号是正常的（avoidance_car → 5 个；oled_dashboard → 5 个
     * 但组合不同）—— 只 warn 不 fail。 */
    // P2-1 (Phase C): js_pal_os_get_ms / js_pal_os_get_us removed — C-side
    // pal_os_get_us/ms() reads s_virtual_us directly (zero JS call); JS host
    // drives virtual time forward via the C→JS export pal_wasm_advance_virtual_clock.
    const knownBridgeSymbols = [
        'js_pal_gpio_write',
        'js_pal_gpio_read',
        'js_pal_pwm_set_duty',
        'js_pal_i2c_transfer',
        'js_pal_register_interrupt',
        'js_pal_deregister_interrupt',
        'js_pal_poll_interrupt',
        'js_pal_os_sleep_ms',
        'js_pal_os_busy_wait_us',
        'js_sim_trigger_ultrasonic',
        'js_sim_measure_echo_pulse_us',
    ];
    const stray = imports.filter((n) => !knownBridgeSymbols.includes(n));
    if (stray.length > 0) {
        console.error(`[stub] FAIL: wasm imports symbols not declared in wasm_bridge.h: ${stray.join(', ')}`);
        console.error('       Add them there (SSOT) or remove the extern from wherever it leaked in.');
        process.exit(1);
    }
    const treeShaken = knownBridgeSymbols.filter((n) => !imports.includes(n));
    if (treeShaken.length > 0) {
        console.log(`[stub] tree-shaken (unused by current App variant): ${treeShaken.length} symbols`);
    }

    /* 启动 worker */
    const worker = new Worker(__filename);
    let ready = false;
    let asyncifyProven = false;
    const TIMEOUT_MS = 3000;
    const OBSERVE_MS = 500;   /* 观察 500ms 内是否至少一次 sleep 真的挂起 */

    worker.on('message', (msg) => {
        if (msg && msg.type === 'ready') {
            ready = true;
            console.log(`[stub] worker signalled ready; imports negotiated OK`);
            /* ADR-0019 Task 0 补：等 worker 观察 sleep 时序后再判定。
             * asyncify_ok 消息由 worker 在观测到 wasm sleep 真的耗时了才发出；
             * 否则超时视为 Asyncify 失效（回归到 __async: true 的既存 bug）。 */
        } else if (msg && msg.type === 'asyncify_ok') {
            asyncifyProven = true;
            console.log(`[stub] Asyncify timing PASS: observed ${msg.observed} sleep call(s), max wall-delta=${msg.maxDeltaMs.toFixed(1)}ms (req ${msg.reqMs}ms)`);
            console.log('[stub] wasm runtime + Asyncify verified → smoke PASS');
            worker.terminate().then(() => process.exit(0));
        } else if (msg && msg.type === 'asyncify_fail') {
            console.error(`[stub] FAIL: Asyncify not effective — ${msg.reason}`);
            console.error('[stub]        (ADR-0019 §背景：既存 __async: true bug 未修复？运行 diff wink_sim_js.js 检查是否为 \'auto\')');
            worker.terminate().then(() => process.exit(1));
        } else if (msg && msg.type === 'error') {
            console.error('[stub] FAIL: worker reported error:', msg.err);
            worker.terminate().then(() => process.exit(1));
        }
    });

    worker.on('error', (err) => {
        console.error('[stub] FAIL: worker threw:', err);
        process.exit(1);
    });

    worker.on('exit', (code) => {
        if (!ready) {
            console.error(`[stub] FAIL: worker exited early with code=${code} before onRuntimeInitialized`);
            process.exit(1);
        }
    });

    setTimeout(() => {
        if (!ready) {
            console.error(`[stub] FAIL: worker did not reach onRuntimeInitialized within ${TIMEOUT_MS}ms`);
            worker.terminate().then(() => process.exit(1));
        } else if (!asyncifyProven) {
            console.error(`[stub] FAIL: Asyncify did not prove effective within ${TIMEOUT_MS}ms — no sleep call took real wall-clock time`);
            console.error('[stub]        (ADR-0019: __async: \'auto\' 修复未生效？wrapper 未正确 return Promise？)');
            worker.terminate().then(() => process.exit(1));
        }
    }, TIMEOUT_MS);
} else {
    /* ---------- Worker 线程：加载 emscripten 胶水 ---------- */
    try {
        const WasmSandbox = require(GLUE_PATH);

        /* ADR-0019 Task 0：Asyncify 时序验证。
         * 现有 stub 只验 onRuntimeInitialized 存活，无法暴露 __async: true 失效
         * 导致 Asyncify 空转的既存 bug（ADR-0019 §背景 spike #5-#6）。
         *
         * 判据：通过 Module.js_pal_os_sleep_ms 覆盖钩子拦截 wasm 侧调用，
         * 记录每次 sleep 请求进入 wrapper 的 wall-clock 时刻；连续两次 sleep
         * 之间的实际间隔 >= 上一次 req_ms * 0.5 视为 Asyncify 真正让 wasm 挂起。
         * 若 Asyncify 失效则 wasm 会跑直连、间隔 ≈ 0，超时失败。
         *
         * 注意：这里我们仍然返回默认 setTimeout Promise，不改变行为语义——
         * 只是插入观察点。观察到一次即上报 asyncify_ok，多余的调用照常处理。 */
        const OBSERVE_MS = 500;
        let lastEnterMs = null;
        let lastReqMs = 0;
        let maxDeltaMs = 0;
        let observed = 0;
        let proven = false;

        WasmSandbox({
            js_pal_os_sleep_ms: (ms) => {
                const now = Date.now();
                if (lastEnterMs !== null) {
                    const delta = now - lastEnterMs;
                    if (delta > maxDeltaMs) maxDeltaMs = delta;
                    /* 判据：wall-delta 大于 req 的 50%，说明上一次 sleep 真的挂起过。
                     * 用 50% 阈值容忍 setTimeout 抖动与 event loop 调度延迟。
                     * lastReqMs>=2 避免 ms=0 / ms=1 边界（Node timer 精度不足）。 */
                    if (!proven && lastReqMs >= 2 && delta >= lastReqMs * 0.5) {
                        proven = true;
                        parentPort.postMessage({
                            type: 'asyncify_ok',
                            observed: observed + 1,
                            reqMs: lastReqMs,
                            maxDeltaMs,
                        });
                    }
                }
                lastEnterMs = now;
                lastReqMs = ms;
                observed++;
                /* 返回真 Promise——沿用默认桩语义，不改变行为 */
                return new Promise(function (resolve) { setTimeout(resolve, ms); });
            },
            onRuntimeInitialized: () => {
                parentPort.postMessage({ type: 'ready' });
                /* main() 会通过 Asyncify 进入无限 sleep 循环；worker 线程被主
                 * 线程 terminate() 强杀。这是本 stub 唯一"预期不 return"的路径。 */

                /* Fallback：若 OBSERVE_MS 内没观察到有效 sleep 序列，报错。
                 * 场景：某些 sample（如无阻塞主循环）根本不调用 sleep_ms，
                 * 此时 Asyncify 是否生效无法从时序判定——但至少要提示。 */
                setTimeout(() => {
                    if (!proven) {
                        if (observed === 0) {
                            parentPort.postMessage({
                                type: 'asyncify_fail',
                                reason: `wasm 侧在 ${OBSERVE_MS}ms 内未调用 js_pal_os_sleep_ms，无法验证 Asyncify（此 sample 可能无阻塞主循环，请换用调 sleep 的 sample）`,
                            });
                        } else {
                            parentPort.postMessage({
                                type: 'asyncify_fail',
                                reason: `wasm 侧调用 ${observed} 次 sleep 但连续间隔最大仅 ${maxDeltaMs}ms（上次 req ${lastReqMs}ms）——Asyncify 未挂起 wasm 执行`,
                            });
                        }
                    }
                }, OBSERVE_MS);
            },
            print: (msg) => process.stdout.write(`[wasm] ${msg}\n`),
            printErr: (msg) => process.stderr.write(`[wasm-err] ${msg}\n`),
        }).catch((err) => {
            parentPort.postMessage({ type: 'error', err: String(err) });
        });
    } catch (err) {
        parentPort.postMessage({ type: 'error', err: String(err) });
    }
}
