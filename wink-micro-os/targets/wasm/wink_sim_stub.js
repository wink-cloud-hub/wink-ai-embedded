/**
 * wink_sim_stub.js — Wasm simulation runtime smoke test stub (Node.js side).
 *
 * Purpose:
 *   1. Verify that wasm and glue code can be instantiated by Node;
 *   2. Verify that js_* stubs injected via --js-library match wasm imports contract (no stray symbols);
 *   3. Verify that Asyncify actually suspends wasm execution (wall-clock proof);
 *   4. Verify that default stubs advance s_virtual_us (app_init end LOG_I shows t1>0).
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

    /* Wasm imports contract verification */
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
        'js_pal_adc_read_norm',
        'js_pal_gpio_read_state',
        'js_pal_gpio_drive_ideal',
        'js_pal_gpio_release_ideal',
        'js_pal_gpio_release_mcu',
        'js_pal_gpio_on_write',
        'js_pal_pwm_set_duty',
        'js_pal_i2c_transfer',
        'js_pal_spi_transfer',
        'js_pal_uart_write',
        'js_pal_register_interrupt',
        'js_pal_deregister_interrupt',
        'js_pal_poll_interrupt',
        'js_pal_os_sleep_ms',
        'js_pal_os_busy_wait_us',
        'js_pal_log',
        'js_sim_trigger_ultrasonic',
        'js_sim_measure_echo_pulse_us',
        'js_sim_get_plugin_channel',
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

    /* Start worker thread */
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
    /* ---------- Worker Thread ---------- */
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
                const start = Date.now();
                observed++;
                const advanceUs = BigInt(ms) * 1000n;
                return new Promise(function (resolve) {
                    setTimeout(function () {
                        const delta = Date.now() - start;
                        if (!asyncifyProven && delta >= ms * 0.5) {
                            asyncifyProven = true;
                            parentPort.postMessage({
                                type: 'asyncify_ok',
                                observed,
                                reqMs: ms,
                                maxDeltaMs: delta,
                            });
                        }
                        const m = moduleRef || (typeof Module !== 'undefined' ? Module : null);
                        if (m && typeof m._pal_wasm_advance_virtual_clock === 'function') {
                            try { m._pal_wasm_advance_virtual_clock(advanceUs); } catch (_e) {}
                        } else if (m && typeof m.ccall === 'function') {
                            try { m.ccall('pal_wasm_advance_virtual_clock', null, ['bigint'], [advanceUs]); } catch (_e) {}
                        }
                        resolve();
                    }, ms);
                });
            },
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
                    case 4: line = null; break;
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
                                reason: `init complete but t1_us=${t1_us} t1_ms=${t1_ms} both 0 (virtual clock not advanced)`,
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
                            reason: `${OBSERVE_MS}ms elapsed without observing 'init complete t0=... t1=...' log`,
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
