/**
 * wink_sim_stub.js — Wasm 仿真 runtime 烟测桩（Node.js 侧）
 *
 * 目的：
 *   1. 验证 wasm 与 glue 能被 Node 实例化；
 *   2. 验证 --js-library 注入的 js_* 桩集合与 wasm imports 契约一致（无 stray symbol）；
 *   3. 验证 Asyncify 真的挂起 wasm（wall-clock 证明 sleep 让出了执行）；
 *   4. 验证默认桩推进了 s_virtual_us（app_init 末尾 LOG_I 的 t1>0）——避免
 *      "setTimeout 触发但虚拟时钟冻结" 的退化（见 wink_sim_js.js §时钟推进契约）。
 *
 * 架构：主线程做超时/判定；Worker 线程加载 wasm（防 Asyncify starve 主 event loop）。
 *
 * 用法：
 *   node targets/wasm/wink_sim_stub.js [--build-dir=<path>] [path]
 *   （Node ≥ 16）
 *
 * 注：js_pal_log 覆盖里**不能**使用 UTF8ToString——它是 emcc 闭包内部符号，
 * 未加入 EXPORTED_RUNTIME_METHODS。我们用 HEAPU8 手动读取 null-terminated UTF-8。
 */

'use strict';

const fs = require('fs');
const path = require('path');
const { Worker, isMainThread, parentPort, workerData } = require('worker_threads');

const HERE = __dirname;
function resolveBuildDir(argv, env) {
    const flag = argv.find((a) => a.startsWith('--build-dir='));
    if (flag) return path.resolve(flag.slice('--build-dir='.length));
    const positional = argv.find((a) => !a.startsWith('-'));
    if (positional) return path.resolve(positional);
    if (env.WINK_BUILD_DIR) return path.resolve(env.WINK_BUILD_DIR);
    return path.resolve(HERE, '..', '..', 'build-wasm');
}

/**
 * Read a null-terminated UTF-8 string from Module.HEAPU8 at byte offset `ptr`.
 * Replacement for emcc's UTF8ToString which is not in EXPORTED_RUNTIME_METHODS.
 */
function readCString(heap, ptr) {
    if (!heap || !ptr) return '';
    let end = ptr;
    while (end < heap.length && heap[end] !== 0) end++;
    const bytes = heap.subarray(ptr, end);
    try {
        return Buffer.from(bytes).toString('utf8');
    } catch (_e) {
        return '';
    }
}

if (isMainThread) {
    const BUILD_DIR = resolveBuildDir(process.argv.slice(2), process.env);
    const GLUE_PATH = path.join(BUILD_DIR, 'wink_simulator.js');
    const WASM_PATH = path.join(BUILD_DIR, 'wink_simulator.wasm');

    if (!fs.existsSync(GLUE_PATH) || !fs.existsSync(WASM_PATH)) {
        console.error(`[stub] missing wasm build artifacts under ${BUILD_DIR}`);
        console.error('       run: emcmake cmake -S . -B build-wasm -DTARGET_PLATFORM=wasm && cmake --build build-wasm');
        process.exit(2);
    }

    /* wasm imports 契约校验 */
    const bytes = fs.readFileSync(WASM_PATH);
    const mod = new WebAssembly.Module(bytes);
    const imports = WebAssembly.Module.imports(mod)
        .filter((i) => i.module === 'env' && i.name.startsWith('js_'))
        .map((i) => i.name)
        .sort();
    console.log(`[stub] wasm imports env.js_* (${imports.length}):`);
    for (const name of imports) console.log(`  - ${name}`);

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
        'js_pal_log',
        'js_sim_trigger_ultrasonic',
        'js_sim_measure_echo_pulse_us',
    ];
    const stray = imports.filter((n) => !knownBridgeSymbols.includes(n));
    if (stray.length > 0) {
        console.error(`[stub] FAIL: wasm imports symbols not declared in wasm_bridge.h: ${stray.join(', ')}`);
        process.exit(1);
    }
    const treeShaken = knownBridgeSymbols.filter((n) => !imports.includes(n));
    if (treeShaken.length > 0) {
        console.log(`[stub] tree-shaken (unused by current App variant): ${treeShaken.length} symbols — ${treeShaken.join(',')}`);
    }

    /* 启动 worker */
    const worker = new Worker(__filename, { workerData: { buildDir: BUILD_DIR } });
    let ready = false;
    let asyncifyProven = false;
    let smokeProven = false;
    const TIMEOUT_MS = 5000;

    worker.on('message', (msg) => {
        if (!msg || !msg.type) return;
        switch (msg.type) {
            case 'ready':
                ready = true;
                console.log(`[stub] worker signalled ready; imports negotiated OK`);
                break;
            case 'asyncify_ok':
                asyncifyProven = true;
                console.log(`[stub] Asyncify timing PASS: observed ${msg.observed} sleep call(s), max wall-delta=${msg.maxDeltaMs.toFixed(1)}ms (req ${msg.reqMs}ms)`);
                break;
            case 'init_observed':
                smokeProven = true;
                console.log(`[stub] init complete observed: t0_us=${msg.t0_us} t0_ms=${msg.t0_ms} t1_us=${msg.t1_us} t1_ms=${msg.t1_ms}`);
                console.log('[stub] wasm runtime + Asyncify + scheduler + virtual clock verified → smoke PASS');
                worker.terminate().then(() => process.exit(0));
                break;
            case 'asyncify_fail':
                console.error(`[stub] FAIL: Asyncify not effective — ${msg.reason}`);
                worker.terminate().then(() => process.exit(1));
                break;
            case 'clock_fail':
                console.error(`[stub] FAIL: virtual clock frozen — ${msg.reason}`);
                worker.terminate().then(() => process.exit(1));
                break;
            case 'error':
                console.error('[stub] FAIL: worker reported error:', msg.err);
                if (msg.stack) console.error(msg.stack);
                worker.terminate().then(() => process.exit(1));
                break;
            case 'log':
                /* 透传 worker 侧日志（默认桩 js_pal_log 被我们覆盖，我们自己打） */
                process.stdout.write(msg.line + '\n');
                break;
            default:
                break;
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
            console.error(`[stub] FAIL: Asyncify did not prove effective within ${TIMEOUT_MS}ms`);
            worker.terminate().then(() => process.exit(1));
        } else if (!smokeProven) {
            console.error(`[stub] FAIL: did not observe 'init complete' with t1>0 within ${TIMEOUT_MS}ms`);
            worker.terminate().then(() => process.exit(1));
        }
    }, TIMEOUT_MS);
} else {
    /* ---------- Worker 线程 ---------- */
    try {
        const BUILD_DIR = (workerData && workerData.buildDir)
            ? path.resolve(workerData.buildDir)
            : resolveBuildDir(process.argv.slice(2), process.env);
        const GLUE_PATH = path.join(BUILD_DIR, 'wink_simulator.js');
        const WasmSandbox = require(GLUE_PATH);

        let lastEnterMs = null;
        let lastReqMs = 0;
        let maxDeltaMs = 0;
        let observed = 0;
        let asyncifyProven = false;

        let moduleRef = null;
        let initObserved = false;
        const INIT_RE = /init complete\s+t0_us=(\d+)\s+t0_ms=(\d+)\s+t1_us=(\d+)\s+t1_ms=(\d+)/;
        const OBSERVE_MS = 3000;

        WasmSandbox({
            preRun: [(mod) => {
                moduleRef = mod;
            }],
            js_pal_os_sleep_ms: (ms) => {
                const now = Date.now();
                if (lastEnterMs !== null) {
                    const delta = now - lastEnterMs;
                    if (delta > maxDeltaMs) maxDeltaMs = delta;
                    if (!asyncifyProven && lastReqMs >= 2 && delta >= lastReqMs * 0.5) {
                        asyncifyProven = true;
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
                const advanceUs = BigInt(ms) * 1000n;
                return new Promise(function (resolve) {
                    setTimeout(function () {
                        if (moduleRef && typeof moduleRef._pal_wasm_advance_virtual_clock === 'function') {
                            moduleRef._pal_wasm_advance_virtual_clock(advanceUs);
                        }
                        resolve();
                    }, ms);
                });
            },
            /* 不覆盖 js_pal_os_busy_wait_us——走默认桩，默认桩会推进时钟（我们刚修过）。
             * 但是！默认桩的 js_pal_os_busy_wait_us 实现也依赖闭包内的 UTF8ToString
             * 吗？不会——busy_wait 不调日志，它只 return setTimeout。默认桩里对
             * Module._pal_wasm_advance_virtual_clock 的访问是通过 Module 全局（闭包
             * 内的 Module 是 emcc 内部变量，可用），所以默认桩在被我们覆盖了
             * sleep_ms 的情况下仍能正常推进 busy_wait 的时钟。 */
            js_pal_log: function (level, msgPtr) {
                var msg = '';
                try {
                    if (moduleRef && moduleRef.HEAPU8) {
                        msg = readCString(moduleRef.HEAPU8, msgPtr);
                    }
                } catch (_e) { msg = '<decode-err>'; }
                var line;
                switch (level) {
                    case 1: line = '[wink E] ' + msg; break;
                    case 2: line = '[wink W] ' + msg; break;
                    case 3: line = '[wink I] ' + msg; break;
                    case 4: line = null; break; /* debug 默认静默 */
                    default: line = '[wink ?] ' + msg; break;
                }
                if (line) parentPort.postMessage({ type: 'log', line });
                if (!initObserved) {
                    const m = msg.match(INIT_RE);
                    if (m) {
                        const t0_us = Number(m[1]);
                        const t0_ms = Number(m[2]);
                        const t1_us = Number(m[3]);
                        const t1_ms = Number(m[4]);
                        initObserved = true;
                        if (t1_us > 0 || t1_ms > 0) {
                            parentPort.postMessage({
                                type: 'init_observed',
                                t0_us, t0_ms, t1_us, t1_ms,
                            });
                        } else {
                            parentPort.postMessage({
                                type: 'clock_fail',
                                reason: `init complete 但 t1_us=${t1_us} t1_ms=${t1_ms} 均为 0（虚拟时钟未前进）`,
                            });
                        }
                    }
                }
            },
            onRuntimeInitialized: () => {
                parentPort.postMessage({ type: 'ready' });
                setTimeout(() => {
                    if (!initObserved) {
                        parentPort.postMessage({
                            type: 'clock_fail',
                            reason: `${OBSERVE_MS}ms 内未观察到 'init complete t0=... t1=...' 日志（app_init 是否走到末尾？）`,
                        });
                    }
                }, OBSERVE_MS);
            },
            print: (s) => parentPort.postMessage({ type: 'log', line: '[wasm] ' + s }),
            printErr: (s) => parentPort.postMessage({ type: 'log', line: '[wasm-err] ' + s }),
        }).catch((err) => {
            parentPort.postMessage({ type: 'error', err: String(err), stack: err && err.stack });
        });
    } catch (err) {
        parentPort.postMessage({ type: 'error', err: String(err), stack: err && err.stack });
    }
}
