// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const hash = p => crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
async function main() {
    const [assetArg, outputArg, caseName = 'panic'] = process.argv.slice(2);
    const assets = path.resolve(assetArg);
    const report = {format_version: 1, kind: 'twdt_sdk_report', case: caseName,
        backend: 'wasm_node_abi_harness', harness_sha256: hash(__filename),
        firmware_sha256: hash(path.join(assets, 'wink_simulator.wasm')),
        glue_sha256: hash(path.join(assets, 'wink_simulator.js')),
        device_tree_sha256: hash(path.join(assets, 'device-tree.json')),
        checks: [], frames: [], runtime_errors: [], diagnostics: []};
    let m;
    const now = () => Number(m._pal_wasm_get_virtual_clock_us());
    const check = (name, actual, expected) => {
        const passed = actual === expected;
        report.checks.push({name, time_us: now(), actual, expected, passed});
        return passed;
    };
    const api = (name, ...args) => {
        const value = m[`_twdt_probe_${name}`](...args);
        report.checks.push({name: `SDK ${name}`, time_us: now(), args, actual: value, expected: 0, passed: value === 0});
        return value;
    };
    try {
        m = await require(path.join(assets, 'wink_simulator.js'))({noInitialRun: true,
            wasmBinary: fs.readFileSync(path.join(assets, 'wink_simulator.wasm')),
            preRun: [mod => {
                m = mod;
                mod.js_pal_uart_write = (port, ptr, len) => {
                    report.frames.push({channel: 'uart', time_us: now(), text: Buffer.from(mod.HEAPU8.slice(ptr, ptr + len)).toString('utf8')});
                    return 0;
                };
                mod.js_pal_log = (level, ptr) => report.frames.push({channel: 'log', time_us: now(), text: mod.UTF8ToString(ptr)});
            }], print: () => {}, printErr: s => report.diagnostics.push(String(s))});
        m._pal_wasm_set_sim_mode(1);
        check('app init', await m.ccall('pal_wasm_app_init', 'number', [], [], {async: true}), 0);
        async function until(target) {
            for (let n = 0; now() < target; n++) {
                if (n > 3000) throw new Error('Virtual clock stalled');
                m._pal_wasm_advance_virtual_clock(1000n);
                if (await m.ccall('pal_wasm_app_tick', 'number', [], [], {async: true}) !== 0 || m._pal_wasm_is_faulted()) throw new Error('Runtime fault');
                if (m._pal_wasm_has_pending_reset()) break;
            }
        }
        if (caseName === 'panic') {
            api('init', 100, 1); api('add', 0);
            const start = now();
            api('diagnose');
            check('diagnostic CPU mask', m._twdt_probe_cpus(), 1);
            check('diagnostic has no reset side effect', m._pal_wasm_has_pending_reset(), 0);
            await until(start + 95000);
            check('no early timeout', m._twdt_probe_isr_count(), 0);
            await until(start + 110000);
            check('automatic ISR hook', m._twdt_probe_isr_count(), 1);
            check('hook has ISR restrictions', m._twdt_probe_hook_in_isr(), 1);
            if (check('panic requests reset', m._pal_wasm_has_pending_reset(), 1)) {
                check('public ABI watchdog reason', m._pal_wasm_get_reset_reason(), 2);
                m._pal_wasm_clear_pending_reset();
                m._pal_wasm_reset_app_state();
                check('reset cleared', m._pal_wasm_has_pending_reset(), 0);
                check('SDK last reset is TASK_WDT', m._twdt_probe_reset_reason(), 6);
                check('reinitialize application', await m.ccall('pal_wasm_app_init', 'number', [], [], {async: true}), 0);
                await until(now() + 220000);
                check('old deadline cancelled after reset', m._twdt_probe_isr_count(), 1);
                api('init', 100, 0); api('add', 0);
                check('old handle rejected after reset and reuse', m._twdt_probe_feed_old(), 0x105);
                api('feed', 0); api('delete', 0); api('deinit');
            }
        } else if (caseName === 'shared') {
            api('init', 100, 0); api('add', 0); api('add', 1);
            const start = now();
            api('feed', 0);
            await until(start + 80000);
            api('feed', 1);
            const refreshed = now();
            await until(refreshed + 80000);
            check('all feeders refresh one shared deadline', m._twdt_probe_isr_count(), 0);
            api('feed', 0);
            await until(refreshed + 120000);
            check('missed user triggers once', m._twdt_probe_isr_count(), 1);
            const fired = Number(m._twdt_probe_last_isr_us());
            check('shared deadline timing', fired >= refreshed + 100000 && fired <= refreshed + 120000, true);
            check('non-panic continues without reset', m._pal_wasm_has_pending_reset(), 0);
            api('diagnose');
            check('CPU bitmap is not subscriber count', m._twdt_probe_cpus(), 1);
            const message = m.UTF8ToString(m._twdt_probe_message());
            check('only missed user is diagnosed', message.includes("user 'probe_b'") && !message.includes("user 'probe_a'"), true);
            api('feed', 1);
            for (let n = 0; n < 4; n++) {
                await until(now() + 40000);
                api('feed', 0); api('feed', 1);
            }
            check('feeding resumes without further alarms', m._twdt_probe_isr_count(), 1);
            api('delete', 0); api('delete', 1); api('deinit');
            await until(now() + 220000);
            check('unsubscribe cancels shared deadline', m._twdt_probe_isr_count(), 1);
        } else if (caseName === 'handles') {
            api('init', 100, 0); api('add', 0); api('delete', 0); api('deinit');
            api('init', 100, 0); api('add', 0);
            if (check('deleted handle remains invalid after pool reuse', m._twdt_probe_feed_old(), 0x105)) {
                check('foreign handle is rejected without dereference', m._twdt_probe_feed_foreign(), 0x105);
            }
            api('feed', 0); api('delete', 0); api('deinit');
            await until(now() + 220000);
            check('deinit cancels pending deadline', m._twdt_probe_isr_count(), 0);
        } else { throw new Error(`Unknown SDK case ${caseName}`); }
        process.exitCode = report.checks.every(c => c.passed) ? 0 : 1;
    } catch (error) { report.runtime_errors.push(String(error.stack || error)); process.exitCode = 2; }
    report.exit_code = process.exitCode;
    fs.writeFileSync(outputArg, JSON.stringify(report, null, 2) + '\n');
    console.log(`${caseName}: exit ${report.exit_code}; ${report.checks.filter(c => !c.passed).map(c => c.name).join(', ')}`);
}
main().catch(e => { console.error(e.stack || e); process.exitCode = 2; });
