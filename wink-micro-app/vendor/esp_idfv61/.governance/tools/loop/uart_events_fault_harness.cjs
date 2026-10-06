// SPDX-License-Identifier: GPL-3.0-only
'use strict';

// Independent candidate harness using the production Wasm module's public ABI.
// This report is deliberately distinct from a native UniSim scenario report.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');

const ERROR_TEXT = {frame: 'uart frame error', parity: 'uart parity error', fifo: 'hw fifo overflow'};
const FLAGS = {control: 0, frame: 1, parity: 2, fifo: 4};
const sha256 = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const stripAnsi = text => text.replace(/\x1b\[[0-9;]*m/g, '');

async function main() {
    const args = process.argv.slice(2);
    if (args.length !== 8 || args[0] !== '--assets' || args[2] !== '--contract' ||
        args[4] !== '--report' || args[6] !== '--run-id') {
        throw new Error('Usage: node harness.cjs --assets DIR --contract FILE --report FILE --run-id ID');
    }
    const assets = path.resolve(args[1]);
    const contractPath = path.resolve(args[3]);
    const reportPath = path.resolve(args[5]);
    const contract = JSON.parse(fs.readFileSync(contractPath, 'utf8'));
    if (contract.format_version !== 1 || contract.kind !== 'uart_events_fault_contract' ||
        !Object.hasOwn(FLAGS, contract.case) || contract.port !== 0 ||
        contract.flags !== FLAGS[contract.case] || contract.window_us !== 500000 ||
        contract.before_payload !== `PRE_${contract.case.toUpperCase()}_WINK` ||
        contract.recovery_payload !== `POST_${contract.case.toUpperCase()}_WINK`) {
        throw new Error('Unsupported pinned UART Events fault contract');
    }
    const report = {
        format_version: 1, kind: 'uart_events_fault_report', run_id: args[7],
        backend: 'wasm_node_abi_harness', case: contract.case,
        contract_sha256: sha256(contractPath), harness_sha256: sha256(__filename),
        firmware_sha256: sha256(path.join(assets, 'wink_simulator.wasm')),
        glue_sha256: sha256(path.join(assets, 'wink_simulator.js')),
        device_tree_sha256: sha256(path.join(assets, 'device-tree.json')),
        same_instance: true, runtime_errors: [], diagnostics: [], injections: [], step_results: [],
    };
    let moduleRef;
    let activeFrames = null;
    const now = () => Number(moduleRef._pal_wasm_get_virtual_clock_us());
    const faultLines = frames => frames.map(frame => stripAnsi(Buffer.from(frame.hex, 'hex').toString('utf8')))
        .filter(line => Object.values(ERROR_TEXT).some(text => line.includes(`uart_events: ${text}`)));
    const echo = frames => frames.filter(frame => frame.port === 0 && frame.hex.length === 2)
        .map(frame => frame.hex).join('');
    try {
        const factory = require(path.join(assets, 'wink_simulator.js'));
        moduleRef = await factory({
            noInitialRun: true,
            wasmBinary: fs.readFileSync(path.join(assets, 'wink_simulator.wasm')),
            preRun: [mod => {
                moduleRef = mod;
                mod.js_pal_uart_write = (port, ptr, len) => {
                    if (activeFrames) activeFrames.push({port, time_us: now(),
                        hex: Buffer.from(mod.HEAPU8.slice(ptr, ptr + len)).toString('hex')});
                    return 0;
                };
                mod.js_pal_log = () => {};
            }],
            print: () => {},
            printErr: text => report.diagnostics.push(String(text)),
        });
        for (const name of ['_pal_wasm_set_sim_mode', '_pal_wasm_app_init', '_pal_wasm_app_tick',
            '_pal_wasm_advance_virtual_clock', '_pal_wasm_get_virtual_clock_us',
            '_pal_wasm_push_uart_rx_byte', '_pal_wasm_push_uart_rx_error', '_pal_wasm_is_faulted']) {
            if (typeof moduleRef[name] !== 'function') throw new Error(`Missing public ABI export: ${name}`);
        }
        moduleRef._pal_wasm_set_sim_mode(1);
        if (await moduleRef.ccall('pal_wasm_app_init', 'number', [], [], {async: true}) !== 0) {
            throw new Error('Firmware initialization failed');
        }
        async function tick() {
            moduleRef._pal_wasm_advance_virtual_clock(1000n);
            const rc = await moduleRef.ccall('pal_wasm_app_tick', 'number', [], [], {async: true});
            if (rc !== 0 || moduleRef._pal_wasm_is_faulted()) throw new Error(`Firmware tick failed: ${rc}`);
        }
        async function payloadStep(index, payload) {
            const start = now();
            activeFrames = [];
            const bytes = Buffer.from(payload, 'utf8');
            const expectedHex = bytes.toString('hex');
            for (const byte of bytes) {
                if (!moduleRef._pal_wasm_push_uart_rx_byte(0, byte)) throw new Error('UART RX rejected input byte');
                await tick();
            }
            while (echo(activeFrames) !== expectedHex && now() < start + contract.window_us) await tick();
            const passed = echo(activeFrames) === expectedHex && faultLines(activeFrames).length === 0 &&
                now() <= start + contract.window_us;
            report.step_results.push({index, type: index === 0 ? 'normal_echo' : 'same_instance_recovery',
                status: passed ? 'passed' : 'failed', start_us: start, end_us: now(),
                expected: {echo_hex: expectedHex, fault_log_count: 0}, tx_frames: activeFrames});
        }
        await payloadStep(0, contract.before_payload);
        const start = now();
        activeFrames = [];
        moduleRef._pal_wasm_push_uart_rx_error(0, contract.flags);
        report.injections.push({function: 'pal_wasm_push_uart_rx_error', port: 0,
            flags: contract.flags, time_us: start});
        const text = ERROR_TEXT[contract.case] || null;
        while (now() < start + contract.window_us && (text === null || faultLines(activeFrames).length === 0)) await tick();
        const lines = faultLines(activeFrames);
        const passed = (text === null ? lines.length === 0 : lines.length === 1 &&
            lines[0].includes(`uart_events: ${text}`)) && echo(activeFrames) === '' &&
            now() <= start + contract.window_us + 11000;
        report.step_results.push({index: 1, type: 'fault_handling', status: passed ? 'passed' : 'failed',
            start_us: start, end_us: now(), expected: {fault_log: text, fault_log_count: text === null ? 0 : 1},
            tx_frames: activeFrames});
        await payloadStep(2, contract.recovery_payload);
        process.exitCode = report.step_results.every(step => step.status === 'passed') ? 0 : 1;
    } catch (error) {
        report.runtime_errors.push(String(error.stack || error));
        process.exitCode = 2;
    }
    report.exit_code = process.exitCode;
    fs.writeFileSync(reportPath, JSON.stringify(report, null, 2) + '\n');
    console.log(`${contract.case}: exit ${report.exit_code}; ${report.step_results.map(s => `${s.type}=${s.status}`).join(', ')}`);
}

main().catch(error => { console.error(error.stack || error); process.exitCode = 2; });
