// SPDX-License-Identifier: GPL-3.0-only
'use strict';
// Candidate observer at the production UART/log, virtual-clock and reset ABI.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const hash = p => crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
const clean = s => s.replace(/\x1b\[[0-9;]*m/g, '');
async function main() {
    const [assetsArg, contractArg, outputArg, runId] = process.argv.slice(2);
    const assets = path.resolve(assetsArg), contractPath = path.resolve(contractArg);
    const contract = JSON.parse(fs.readFileSync(contractPath, 'utf8'));
    if (contract.kind !== 'twdt_timeout_contract' || contract.format_version !== 1 ||
        !['control', 'task', 'func_a', 'func_b'].includes(contract.case) ||
        contract.timeout_us !== 3000000 || contract.window_us !== 16000000 || contract.tolerance_us !== 50000) {
        throw new Error('Unsupported TWDT contract');
    }
    const report = {format_version: 1, kind: 'twdt_timeout_report', run_id: runId,
        backend: 'wasm_node_abi_harness', case: contract.case, same_instance: true,
        contract_sha256: hash(contractPath), harness_sha256: hash(__filename),
        firmware_sha256: hash(path.join(assets, 'wink_simulator.wasm')),
        glue_sha256: hash(path.join(assets, 'wink_simulator.js')),
        device_tree_sha256: hash(path.join(assets, 'device-tree.json')),
        runtime_errors: [], diagnostics: [], frames: [], reset_requested: false};
    let mod;
    const now = () => Number(mod._pal_wasm_get_virtual_clock_us());
    const frame = (channel, value) => report.frames.push({channel, time_us: now(), text: clean(value)});
    try {
        mod = await require(path.join(assets, 'wink_simulator.js'))({noInitialRun: true,
            wasmBinary: fs.readFileSync(path.join(assets, 'wink_simulator.wasm')),
            preRun: [m => {
                mod = m;
                m.js_pal_uart_write = (port, ptr, len) => {
                    if (port === 0) frame('uart', Buffer.from(m.HEAPU8.slice(ptr, ptr + len)).toString('utf8'));
                    return 0;
                };
                m.js_pal_log = (level, ptr) => frame('log', m.UTF8ToString(ptr));
            }], print: () => {}, printErr: s => report.diagnostics.push(String(s))});
        mod._pal_wasm_set_sim_mode(1);
        report.start_us = now();
        if (await mod.ccall('pal_wasm_app_init', 'number', [], [], {async: true}) !== 0) throw new Error('App init failed');
        for (let ticks = 0; now() < report.start_us + contract.window_us; ticks++) {
            if (ticks >= 30000) throw new Error('Virtual clock made insufficient progress');
            mod._pal_wasm_advance_virtual_clock(1000n);
            if (await mod.ccall('pal_wasm_app_tick', 'number', [], [], {async: true}) !== 0 || mod._pal_wasm_is_faulted()) {
                throw new Error('App tick faulted');
            }
            if (mod._pal_wasm_has_pending_reset()) { report.reset_requested = true; break; }
        }
        report.end_us = now();
        const subscribed = report.frames.find(f => f.text.includes('Subscribed to TWDT'));
        const done = report.frames.find(f => f.text.includes('Example complete'));
        const alarms = report.frames.filter(f => f.text.includes('Task watchdog got triggered.'));
        const failures = report.frames.filter(f => f.text.includes('did not reset:'));
        const expected = contract.case === 'control' ? [] : [`${contract.case === 'task' ? 'task' : 'user'} '${contract.case}'`];
        const correctAlarm = contract.case === 'control' ? alarms.length === 0 && failures.length === 0 :
            subscribed && alarms.length > 0 && alarms[0].time_us >= subscribed.time_us + contract.timeout_us &&
            alarms[0].time_us <= subscribed.time_us + contract.timeout_us + contract.tolerance_us &&
            failures.length === alarms.length && failures.every(f => f.text.includes(`did not reset: ${expected[0]}`));
        const lifecycle = done && subscribed && !report.reset_requested && report.end_us >= report.start_us + contract.window_us &&
            report.frames.some(f => f.text.includes('Unsubscribed from TWDT')) &&
            report.frames.some(f => f.text.includes('TWDT deinitialized'));
        const quiet = done && alarms.every(f => f.time_us < done.time_us) && report.end_us >= done.time_us + contract.timeout_us;
        report.step_results = [
            {index: 0, type: 'automatic_timeout', status: correctAlarm ? 'passed' : 'failed'},
            {index: 1, type: 'unsubscribe_recovery', status: lifecycle && quiet ? 'passed' : 'failed'}];
        process.exitCode = report.step_results.every(s => s.status === 'passed') ? 0 : 1;
    } catch (error) { report.runtime_errors.push(String(error.stack || error)); process.exitCode = 2; }
    report.exit_code = process.exitCode;
    fs.writeFileSync(outputArg, JSON.stringify(report, null, 2) + '\n');
    console.log(`${contract.case}: exit ${report.exit_code}; ${JSON.stringify(report.step_results)}`);
}
main().catch(e => { console.error(e.stack || e); process.exitCode = 2; });
